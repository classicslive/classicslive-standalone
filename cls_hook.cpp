#include <wchar.h>

#ifdef __linux__
#include <signal.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "cls_hook.h"

ClsHook::ClsHook(unsigned pid, const cls_window_preset_t *preset)
{
  m_ProcessId = pid;
  m_Preset = preset;
}

ClsHook::~ClsHook()
{
}

bool ClsHook::init()
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  const int wsize = 256;
  wchar_t wide_class[wsize];
  wchar_t wide_title[wsize];

  /* Cast window title and class to wide char string */
  mbstowcs(wide_class, m_Preset->window_class, wsize);
  mbstowcs(wide_title, m_Preset->window_title, wsize);

  /* Find window, discard class and title if they are blank */
  m_Window = FindWindow(wcslen(wide_class) == 0 ? nullptr : wide_class,
                        wcslen(wide_title) == 0 ? nullptr : wide_title);
  if (!m_Window)
    return false;

  /* Get process ID */
  GetWindowThreadProcessId(m_Window, &m_ProcessId);
  if (!m_ProcessId)
    return false;

  /* Get process handle with read and write permissions */
  m_Handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, m_ProcessId);
  if (!m_Handle)
    return false;

  /*HANDLE th = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_ProcessId);
  if (th == INVALID_HANDLE_VALUE)
    return false;
  else
  {
    MODULEENTRY32 module;

    module.dwSize = sizeof(module);
    if (Module32First(th, &module))
    {
      m_MemoryData = reinterpret_cast<uintptr_t>(module.modBaseAddr);
      m_MemorySize = module.modBaseSize;
      do
      {
        wprintf(L"%s\n", module.szModule);
      } while (Module32Next(th, &module));
    }
  }
  CloseHandle(th);*/

  /*MEMORY_BASIC_INFORMATION mbi;
  auto step = reinterpret_cast<void*>(m_MemoryData);
  unsigned long long total = 0;
  while (VirtualQueryEx(m_Handle, step, &mbi, sizeof(mbi)) && total < 0x40000000)
  {
    cl_log("%p %u\n", mbi.BaseAddress, mbi.RegionSize);
    step = reinterpret_cast<uint8_t*>(step) + mbi.RegionSize;
    if (mbi.RegionSize > 0x40000000)
      break;
    total += mbi.RegionSize;
  }*/

  m_MemoryData = 0;
  m_MemorySize = 0x02f752D4;
#endif

  return true;
}

bool ClsHook::initViaMemoryRegions(const cls_find_memory_region_t fvmr)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  MEMORY_BASIC_INFORMATION memory;
  char *addr = nullptr;

  while (VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(addr), &memory,
                        sizeof(MEMORY_BASIC_INFORMATION)))
  {
    if (memory.RegionSize == fvmr.size && memory.Protect == PAGE_READWRITE)
    {
      m_MemoryData = reinterpret_cast<uintptr_t>(memory.BaseAddress) + fvmr.offset;
      return true;
    }
    addr += memory.RegionSize;
  }
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  char line[256];
  char map_path[256];
  FILE *map_file = nullptr;
  uintptr_t addr_start, addr_end, size;

  if (!m_ProcessId)
    return false;
  snprintf(map_path, sizeof(map_path), "/proc/%d/maps", m_ProcessId);
  map_file = fopen(map_path, "r");
  if (!map_file)
    return false;

  while (fgets(line, sizeof(line), map_file))
  {
    if (sscanf(line, "%lx-%lx", &addr_start, &addr_end) == 2)
    {
      size = addr_end - addr_start;
      if (size == fvmr.size)
      {
        m_MemoryData = reinterpret_cast<uintptr_t>(addr_start) + fvmr.offset;
        fclose(map_file);

        return true;
      }
    }
  }
  fclose(map_file);
#endif
  return false;
}

bool ClsHook::run()
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  return IsWindow(m_Window);
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  char proc[32];
  snprintf(proc, sizeof(proc), "/proc/%u", m_ProcessId);
  return (access(proc, F_OK) == 0);
#endif
}

size_t ClsHook::read(void* dest, cl_addr_t address, size_t size)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  size_t read = 0;

  ReadProcessMemory(m_Handle,
                    reinterpret_cast<LPCVOID>(m_MemoryData + address),
                    dest, size, &read);

  return read;
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  struct iovec local_iov;
  struct iovec remote_iov;
  ssize_t read = 0;

  local_iov.iov_base = dest;
  local_iov.iov_len = size;

  remote_iov.iov_base = reinterpret_cast<void*>(m_MemoryData + address);
  remote_iov.iov_len = size;

  read = process_vm_readv(m_ProcessId, &local_iov, 1,
                          &remote_iov, 1, 0);

  if (read < 0)
    return 0;
  else
    return static_cast<size_t>(read);
#else
  return 0;
#endif
}

size_t ClsHook::write(const void* src, cl_addr_t address, size_t size)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  size_t written = 0;

  WriteProcessMemory
  (
    m_Handle,
    reinterpret_cast<void*>(m_MemoryData + address),
    src,
    size,
    &written
  );

  return written;
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  struct iovec local_iov;
  struct iovec remote_iov;
  ssize_t written = 0;

  local_iov.iov_base = const_cast<void*>(src);
  local_iov.iov_len = size;

  remote_iov.iov_base = reinterpret_cast<void*>(m_MemoryData + address);
  remote_iov.iov_len = size;

  written = process_vm_writev(m_ProcessId, &local_iov, 1,
                              &remote_iov, 1, 0);

  if (written < 0)
    return 0;
  else
    return static_cast<size_t>(written);
#else
  return false;
#endif
}

bool ClsHook::deepCopy(cl_search_t *search)
{
  bool success = true;

  if (!search)
    return false;
  for (unsigned i = 0; i < search->searchbank_count; i++)
  {
    auto sbank = &search->searchbanks[i];

    if (!sbank->region->base_host)
    {
      success = false;
      continue;
    }
    else
      success &= read
      (
        (uint8_t*)sbank->region->base_host + sbank->first_valid,
        sbank->first_valid,
        sbank->last_valid - sbank->first_valid + search->params.size
      ) != 0;
  }

  return success;
}

bool ClsHook::pause(void)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  return DebugActiveProcess(m_ProcessId);
#else
  return kill(m_ProcessId, SIGSTOP) == 0;
#endif
}

bool ClsHook::unpause(void)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  return DebugActiveProcessStop(m_ProcessId);
#else
  return kill(m_ProcessId, SIGCONT) == 0;
#endif
}

bool ClsHook::getWindowTitle(char *buffer, unsigned buffer_len)
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  return GetWindowTextA(m_Window, buffer, buffer_len);
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  char cmd[256];
  char line[1024];

  snprintf(cmd, sizeof(cmd), "wmctrl -lp | awk -v pid=%u '$3 == pid'",
           m_ProcessId);
  FILE *cmd_file = popen(cmd, "r");

  if (cmd_file == nullptr)
    return false;

  bool found = false;
  while (fgets(line, sizeof(line), cmd_file))
  {
    if (sscanf(line, "%*s %*d %*d %*s %[^\n]", buffer) == 1)
    {
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
