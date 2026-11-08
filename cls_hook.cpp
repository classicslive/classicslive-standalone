#include "cls_hook.h"

#include <cstring>
#include <wchar.h>

#ifdef __linux__
#include <signal.h>
#include <stdio.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

#include <memory>
#include <regex>
#include <vector>

ClsHook::ClsHook(unsigned pid, const cls_window_preset_t *preset, void *window)
{
  m_ProcessId = pid;
  m_Preset = preset;
  /* Hooks fill in only the region fields they know about */
  memset(m_MemoryRegions, 0, sizeof(m_MemoryRegions));
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

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
typedef struct
{
  PVOID Object;
  ULONG_PTR UniqueProcessId;
  ULONG_PTR HandleValue;
  ULONG GrantedAccess;
  USHORT CreatorBackTraceIndex;
  USHORT ObjectTypeIndex;
  ULONG HandleAttributes;
  ULONG Reserved;
} cls_handle_entry_t;

typedef struct
{
  ULONG_PTR NumberOfHandles;
  ULONG_PTR Reserved;
  cls_handle_entry_t Handles[1];
} cls_handle_info_t;

typedef NTSTATUS (NTAPI *cls_nt_query_system_information_t)(ULONG, PVOID, ULONG, PULONG);

#define CLS_SYSTEM_EXTENDED_HANDLE_INFORMATION 64
#define CLS_STATUS_INFO_LENGTH_MISMATCH static_cast<NTSTATUS>(0xC0000004)

/* Gets a snapshot of every open handle on the system */
static std::vector<uint8_t> get_system_handles(void)
{
  static const auto query = reinterpret_cast<cls_nt_query_system_information_t>(
    GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation"));
  std::vector<uint8_t> buffer(0x100000);
  NTSTATUS status;

  if (!query)
    return {};
  while ((status = query(CLS_SYSTEM_EXTENDED_HANDLE_INFORMATION, buffer.data(),
                         static_cast<ULONG>(buffer.size()), nullptr)) ==
         CLS_STATUS_INFO_LENGTH_MISMATCH && buffer.size() < 0x10000000)
    buffer.resize(buffer.size() * 2);

  return status >= 0 ? buffer : std::vector<uint8_t>();
}

/* Finds a file the process has open whose path matches the pattern */
QString ClsHook::findOpenFile(const QRegularExpression &pattern)
{
  std::vector<uint8_t> buffer;
  HANDLE process, own_file;
  wchar_t own_path[MAX_PATH];
  USHORT file_type = 0;
  QString result;

  /**
   * Object type indices change between Windows versions, so find the one for
   * files from a file handle of our own, which the snapshot will include.
   */
  if (!GetModuleFileNameW(nullptr, own_path, MAX_PATH))
    return {};
  own_file = CreateFileW(own_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (own_file == INVALID_HANDLE_VALUE)
    return {};
  buffer = get_system_handles();
  CloseHandle(own_file);
  if (buffer.empty())
    return {};

  const auto *info = reinterpret_cast<const cls_handle_info_t*>(buffer.data());

  for (ULONG_PTR i = 0; i < info->NumberOfHandles; i++)
  {
    if (info->Handles[i].UniqueProcessId == GetCurrentProcessId() &&
        info->Handles[i].HandleValue == reinterpret_cast<ULONG_PTR>(own_file))
    {
      file_type = info->Handles[i].ObjectTypeIndex;
      break;
    }
  }
  if (!file_type)
    return {};

  process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, m_ProcessId);
  if (!process)
    return {};

  for (ULONG_PTR i = 0; i < info->NumberOfHandles && result.isEmpty(); i++)
  {
    const cls_handle_entry_t &entry = info->Handles[i];
    HANDLE handle;
    wchar_t path[1024];
    DWORD length;

    /* Synchronous pipes can block path queries forever; skip their masks */
    if (entry.UniqueProcessId != m_ProcessId || entry.ObjectTypeIndex != file_type ||
        entry.GrantedAccess == 0x0012019F || entry.GrantedAccess == 0x001A019F ||
        entry.GrantedAccess == 0x00120189 || entry.GrantedAccess == 0x00100000)
      continue;
    if (!DuplicateHandle(process, reinterpret_cast<HANDLE>(entry.HandleValue),
                         GetCurrentProcess(), &handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS))
      continue;
    if (GetFileType(handle) == FILE_TYPE_DISK)
    {
      length = GetFinalPathNameByHandleW(handle, path, 1024,
                                         FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
      if (length && length < 1024)
      {
        QString file = QString::fromWCharArray(path, static_cast<int>(length));

        if (file.startsWith("\\\\?\\"))
          file = file.mid(4);
        file = QDir::fromNativeSeparators(file);
        if (pattern.match(file).hasMatch())
          result = file;
      }
    }
    CloseHandle(handle);
  }
  CloseHandle(process);

  return result;
}
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
QString ClsHook::findOpenFile(const QRegularExpression &pattern)
{
  QDir fd_dir(QString("/proc/%1/fd").arg(m_ProcessId));

  for (const QFileInfo &fd : fd_dir.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot))
  {
    QString file = fd.symLinkTarget();

    if (pattern.match(file).hasMatch())
      return file;
  }

  return {};
}
#else
QString ClsHook::findOpenFile(const QRegularExpression &pattern)
{
  CL_UNUSED(pattern);
  return {};
}
#endif

bool ClsHook::getIdentificationViaOpenFile(cl_game_identifier_t *identifier,
  const QRegularExpression &pattern)
{
  QString file;

  if (!identifier)
    return false;

  file = findOpenFile(pattern);
  if (file.isEmpty())
    return getIdentificationViaFile(identifier);

  identifier->type = CL_GAMEIDENTIFIER_FILE_HASH;
  snprintf(identifier->filename, sizeof(identifier->filename), "%s",
           file.toUtf8().constData());

  return true;
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

bool ClsHook::findRegionByHostAddress(cl_memory_region_t *region,
  uintptr_t host_address)
{
  if (!region || !host_address)
    return false;

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  MEMORY_BASIC_INFORMATION memory;

  if (!VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(host_address),
                      &memory, sizeof(memory)))
    return false;

  memset(region, 0, sizeof(*region));
  region->base_alloc = memory.AllocationBase;
  region->base_host = reinterpret_cast<void*>(memory.BaseAddress);
  region->size = memory.RegionSize;
  region->flags.bits.read = (memory.Protect != PAGE_NOACCESS);
  region->flags.bits.write = (memory.Protect == PAGE_READWRITE ||
                              memory.Protect == PAGE_EXECUTE_READWRITE);
  region->endianness = CL_ENDIAN_NATIVE;
  region->pointer_length = sizeof(void*);
  snprintf(region->title, sizeof(region->title), "%s", "Mapped region");

  return true;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char line[256];
  char map_path[256];
  FILE *map_file = nullptr;
  uintptr_t addr_start, addr_end;
  char flags[5];

  if (!m_ProcessId)
    return false;

  snprintf(map_path, sizeof(map_path), "/proc/%d/maps", m_ProcessId);
  map_file = fopen(map_path, "r");
  if (!map_file)
    return false;

  while (fgets(line, sizeof(line), map_file))
  {
    if (sscanf(line, "%lx-%lx %4s", &addr_start, &addr_end, flags) == 3)
    {
      if (host_address >= addr_start && host_address < addr_end)
      {
        memset(region, 0, sizeof(*region));
        region->base_host = reinterpret_cast<void*>(addr_start);
        region->base_alloc = reinterpret_cast<void*>(addr_start);
        region->size = addr_end - addr_start;
        region->flags.bits.read = (flags[0] == 'r');
        region->flags.bits.write = (flags[1] == 'w');
        region->endianness = CL_ENDIAN_NATIVE;
        region->pointer_length = sizeof(void*);
        snprintf(region->title, sizeof(region->title), "%s", "Mapped region");

        fclose(map_file);
        return true;
      }
    }
  }

  fclose(map_file);
  return false;
#else
  CL_UNUSED(region);
  CL_UNUSED(host_address);
  return false;
#endif
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
  uintptr_t host = translate(address);

  return host ? readHost(dest, host, size) : 0;
}

size_t ClsHook::readHost(void *dest, uintptr_t address, size_t size)
{
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
  uintptr_t host = translate(address);

  return host ? writeHost(src, host, size) : 0;
}

size_t ClsHook::writeHost(const void *src, uintptr_t address, size_t size)
{
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

/* An empty pattern matches anything */
static bool cls_preset_match(const std::regex *pattern, const char *text)
{
  return !pattern || std::regex_match(text, *pattern);
}

const cls_window_preset_t *cls_find_preset(unsigned pid, void *window)
{
  typedef struct
  {
    const cls_window_preset_t *preset;
    std::unique_ptr<std::regex> first;
    std::unique_ptr<std::regex> second;
  } cls_compiled_preset_t;

  static std::vector<cls_compiled_preset_t> compiled = []()
  {
    std::vector<cls_compiled_preset_t> list;
    auto compile = [](const char *pattern)
    {
      return pattern && pattern[0] ?
        std::make_unique<std::regex>(pattern) : nullptr;
    };

    for (const auto &preset : cls_window_presets)
    {
      if (preset.type == CLS_HOOK_GENERIC)
        continue;
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
      list.push_back({ &preset, compile(preset.window_class),
                       compile(preset.window_title) });
#else
      list.push_back({ &preset, compile(preset.process_title), nullptr });
#endif
    }

    return list;
  }();
  char first[256] = "";
  char second[256] = "";

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  CL_UNUSED(pid);
  GetClassNameA(reinterpret_cast<HWND>(window), first, sizeof(first));
  GetWindowTextA(reinterpret_cast<HWND>(window), second, sizeof(second));
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char path[64];
  FILE *file;

  CL_UNUSED(window);
  snprintf(path, sizeof(path), "/proc/%u/comm", pid);
  file = fopen(path, "r");
  if (!file)
    return nullptr;
  if (fgets(first, sizeof(first), file))
    first[strcspn(first, "\n")] = '\0';
  fclose(file);
#endif

  for (const auto &entry : compiled)
    if (cls_preset_match(entry.first.get(), first) &&
        cls_preset_match(entry.second.get(), second))
      return entry.preset;

  return nullptr;
}
