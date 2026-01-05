#include "cls_hook.h"

#include <cstring>
#include <wchar.h>

#ifdef __linux__
#include <signal.h>
#include <stdio.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <QFileDialog>
#include <QString>

ClsHook::ClsHook(unsigned pid, const cls_window_preset_t *preset, void *window)
{
  m_ProcessId = pid;
  m_Preset = preset;
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  m_Window = reinterpret_cast<HWND>(window);
#else
  CL_UNUSED(window);
#endif
}

ClsHook::~ClsHook()
{
}

bool ClsHook::init(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  /* Get process handle with read and write permissions */
  m_Handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE |
                         PROCESS_VM_OPERATION, FALSE, m_ProcessId);
  if (!m_Handle)
    return false;
  else if (!m_Window) /* Find an appropriate window if one wasn't given */
  {
    struct WindowInfo
    {
      HWND hWnd;
      DWORD processId;
      int padding;
    };

    WindowInfo windowInfo = { nullptr, m_ProcessId, 0 };

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
    {
      WindowInfo* pInfo = reinterpret_cast<WindowInfo*>(lParam);
      DWORD wndProcessId;
      GetWindowThreadProcessId(hWnd, &wndProcessId);
      char title[256];

      // Check if this window belongs to the process we're looking for
      if (wndProcessId == pInfo->processId && GetWindowTextA(hWnd, title, 256))
      {
        pInfo->hWnd = hWnd;
        return FALSE; // Stop enumeration, we found the window
      }

      return TRUE; // Continue enumeration
    }, reinterpret_cast<LPARAM>(&windowInfo));

    // If we found a window handle, store it in m_Window
    if (windowInfo.hWnd)
      m_Window = windowInfo.hWnd;
  }
#endif

  return true;
}

unsigned ClsHook::findRegions(cl_memory_region_t *buffer, const unsigned buffer_count,
                              const cls_find_memory_region_t fvmr)
{
  unsigned found = 0;

  if (!buffer || buffer_count == 0)
    return 0;

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  MEMORY_BASIC_INFORMATION memory;
  char *addr = nullptr;

  while (VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(addr), &memory,
                        sizeof(MEMORY_BASIC_INFORMATION)))
  {
    if (memory.RegionSize == fvmr.size && memory.Protect == PAGE_READWRITE)
    {
      if (found < buffer_count)
      {
        cl_memory_region_t &region = buffer[found++];

        region.base_alloc = memory.AllocationBase;
        region.base_host = reinterpret_cast<unsigned char*>(memory.BaseAddress) + fvmr.offset;
        region.size = memory.RegionSize;

        region.flags.bits.read = 1;
        region.flags.bits.write = 1;
        region.endianness = fvmr.endianness ? fvmr.endianness : CL_ENDIAN_NATIVE;
        region.pointer_length = fvmr.pointer_size ? fvmr.pointer_size : 4;
        snprintf(region.title, sizeof(region.title), "%s", fvmr.title ? fvmr.title : "Memory region");
      }
    }
    addr += memory.RegionSize;
  }
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char line[256];
  char map_path[256];
  FILE *map_file = nullptr;
  uintptr_t addr_start, addr_end, size;
  char flags[5];

  if (!m_ProcessId)
    return 0;

  snprintf(map_path, sizeof(map_path), "/proc/%d/maps", m_ProcessId);
  map_file = fopen(map_path, "r");
  if (!map_file)
    return 0;

  while (fgets(line, sizeof(line), map_file))
  {
    if (sscanf(line, "%lx-%lx %4s", &addr_start, &addr_end, flags) == 3)
    {
      size = addr_end - addr_start;

      if (size == fvmr.host_size && ((flags[3] == 's') == fvmr.shared))
      {
        if (found < buffer_count)
        {
          cl_memory_region_t &region = buffer[found++];

          /* host fields */
          region.base_host = reinterpret_cast<uint8_t*>(addr_start) + fvmr.host_offset;
          region.base_alloc = reinterpret_cast<void*>(addr_start);

          /* guest fields */
          region.base_guest = fvmr.guest_base;
          region.size = fvmr.guest_size;

          /* status fields */
          region.flags.bits.read = 1;
          region.flags.bits.write = 1;

          /* misc fields */
          region.endianness = fvmr.endianness ? fvmr.endianness : CL_ENDIAN_NATIVE;
          region.pointer_length = fvmr.pointer_size ? fvmr.pointer_size : 4;
          snprintf(region.title, sizeof(region.title), "%s", fvmr.title ? fvmr.title : "Memory region");
        }
      }
    }
  }
  fclose(map_file);
#endif

  return found;
}

bool ClsHook::getIdentificationViaFile(cl_game_identifier_t *identifier)
{
  if (!identifier)
    return false;
  else
  {
    QString file = QFileDialog::getOpenFileName(
      nullptr,
      QString("%1 - Select content file").arg(getLibrary()),
      QString(),
      "All Files (*.*)"
    );

    if (file.isEmpty())
      return false;
    else
    {
      QByteArray utf8 = file.toUtf8();

      identifier->type = CL_GAMEIDENTIFIER_FILE_HASH;
      snprintf(identifier->filename, sizeof(identifier->filename), "%s",
               utf8.constData());

      return true;
    }
  }
}

bool ClsHook::initViaMemoryRegions(const cls_find_memory_region_t fvmr)
{
  unsigned found = findRegions(m_MemoryRegions, 1, fvmr);

  if (found)
  {
    m_MemoryRegionCount = 1;
    return true;
  }
  else
    return false;
}

bool ClsHook::run(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  return IsWindow(m_Window);
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char proc[32];
  snprintf(proc, sizeof(proc), "/proc/%u", m_ProcessId);
  return (access(proc, F_OK) == 0);
#endif
}

uintptr_t ClsHook::translate(cl_addr_t address)
{
  const cl_memory_region_t *region = nullptr;

  for (unsigned i = 0; i < m_MemoryRegionCount; ++i)
  {
    const cl_memory_region_t &r = m_MemoryRegions[i];
    if (address >= r.base_guest &&
      address <  r.base_guest + r.size)
    {
      region = &r;
      break;
    }
  }

  if (!region)
    return 0;
  else
    return reinterpret_cast<uintptr_t>(region->base_host) +
      (address - region->base_guest);
}

size_t ClsHook::read(void *dest, cl_addr_t address, size_t size)
{
  address = translate(address);
  if (!address)
    return 0;
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  size_t bytes_read = 0;

  ReadProcessMemory(
    m_Handle,
    reinterpret_cast<LPCVOID>(address),
    dest,
    size,
    &bytes_read
  );

  return bytes_read;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  struct iovec local_iov;
  struct iovec remote_iov;

  local_iov.iov_base = dest;
  local_iov.iov_len  = size;

  remote_iov.iov_base = reinterpret_cast<void*>(address);
  remote_iov.iov_len  = size;

  ssize_t bytes_read = process_vm_readv(
    m_ProcessId,
    &local_iov, 1,
    &remote_iov, 1,
    0
  );

  return (bytes_read > 0) ? static_cast<size_t>(bytes_read) : 0;
#else
  return 0;
#endif
}

size_t ClsHook::read(void *dest, const cl_memory_region_t *region, cl_addr_t offset, size_t size)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  size_t bytes_read = 0;

  ReadProcessMemory(
    m_Handle,
    reinterpret_cast<LPCVOID>(region->base_host + address),
    dest,
    size,
    &bytes_read
  );

  return bytes_read;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  struct iovec local_iov;
  struct iovec remote_iov;

  local_iov.iov_base = dest;
  local_iov.iov_len  = size;

  remote_iov.iov_base = reinterpret_cast<void*>(
    reinterpret_cast<char*>(region->base_host) + offset);
  remote_iov.iov_len  = size;

  ssize_t bytes_read = process_vm_readv(
    m_ProcessId,
    &local_iov, 1,
    &remote_iov, 1,
    0
  );

  return (bytes_read > 0) ? static_cast<size_t>(bytes_read) : 0;
#else
  return 0;
#endif
}

size_t ClsHook::write(const void *src, cl_addr_t address, size_t size)
{
  address = translate(address);
  if (!address)
    return 0;
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  size_t bytes_written = 0;

  WriteProcessMemory(
    m_Handle,
    reinterpret_cast<LPVOID>(address),
    src,
    size,
    &bytes_written
  );

  return bytes_written;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  struct iovec local_iov;
  struct iovec remote_iov;

  local_iov.iov_base = const_cast<void*>(src);
  local_iov.iov_len  = size;

  remote_iov.iov_base = reinterpret_cast<void*>(address);
  remote_iov.iov_len  = size;

  ssize_t bytes_written = process_vm_writev(
    m_ProcessId,
    &local_iov, 1,
    &remote_iov, 1,
    0
  );

  return (bytes_written > 0) ? static_cast<size_t>(bytes_written) : 0;
#else
#error "Unhandled platform"
#endif
}

bool ClsHook::pause(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  return DebugActiveProcess(m_ProcessId);
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  return kill(m_ProcessId, SIGSTOP) == 0;
#else
#error "Unhandled platform"
#endif
}

bool ClsHook::unpause(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  return DebugActiveProcessStop(m_ProcessId);
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  return kill(m_ProcessId, SIGCONT) == 0;
#else
#error "Unhandled platform"
#endif
}

bool ClsHook::getWindowTitle(char *buffer, unsigned buffer_len)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  return GetWindowTextA(m_Window, buffer, static_cast<int>(buffer_len));
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  if (!buffer || buffer_len == 0)
    return false;

  buffer[0] = '\0';

  char cmd[256];
  char line[1024];

  snprintf(cmd, sizeof(cmd),
           "wmctrl -lp | awk -v pid=%u '$3 == pid'", m_ProcessId);

  FILE *cmd_file = popen(cmd, "r");
  if (!cmd_file)
    return false;

  bool found = false;

  while (fgets(line, sizeof(line), cmd_file))
  {
    char title[1024];

    if (sscanf(line, "%*s %*d %*d %*s %[^\n]", title) == 1)
    {
      strncpy(buffer, title, buffer_len - 1);
      buffer[buffer_len - 1] = '\0';
      found = true;
      break;
    }
  }

  pclose(cmd_file);
  return found;
#else
  return false;
#endif
}
