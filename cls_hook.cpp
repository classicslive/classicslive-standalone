#include <wchar.h>

#ifdef __linux__
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "cls_hook.h"

ClsHook::ClsHook(const cls_window_preset_t *preset)
{
  m_Preset = preset;
}

ClsHook::~ClsHook()
{
}

bool ClsHook::init()
{
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

  return true;
}

bool ClsHook::initViaMemoryRegions(const cls_find_memory_region fvmr)
{
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

  return false;
}

bool ClsHook::run()
{
#ifdef WIN32
  return IsWindow(m_Window);
#else
  return false;
#endif
}

size_t ClsHook::read(void* dest, cl_addr_t address, size_t size)
{
#ifdef WIN32
  size_t read = 0;

  ReadProcessMemory(m_Handle,
                    reinterpret_cast<LPCVOID>(m_MemoryData + address),
                    dest, size, &read);

  return read;
#elif __linux__
  struct iovec local_iov;
  struct iovec remote_iov;

  local_iov.iov_base = dest;
  local_iov.iov_len = size;

  remote_iov.iov_base = reinterpret_cast<void*>(address);
  remote_iov.iov_len = size;

  ssize_t bytes_read = process_vm_readv(m_ProcessId, &local_iov, 1,
                                        &remote_iov, 1, 0);

  if (bytes_read == -1)
    return 0;
  else
    return static_cast<size_t>(bytes_read);
#else
  return 0;
#endif
}

size_t ClsHook::write(const void* src, cl_addr_t address, size_t size)
{
#ifdef WIN32
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

    if (!sbank->bank->data)
    {
      success = false;
      continue;
    }
    else
      success &= read
      (
        sbank->bank->data + sbank->first_valid,
        sbank->first_valid,
        sbank->last_valid - sbank->first_valid + search->params.size
      ) != 0;
  }

  return success;
}

bool ClsHook::pause(void)
{
#ifdef WIN32
  return DebugActiveProcess(m_ProcessId);
#else
  return kill(m_ProcessId, SIGSTOP) == 0;
#endif
}

bool ClsHook::unpause(void)
{
#ifdef WIN32
  return DebugActiveProcessStop(m_ProcessId);
#else
  return kill(m_ProcessId, SIGCONT) == 0;
#endif
}
