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

  return true;
}

bool ClsHook::run()
{
  return true;
}

bool ClsHook::read(void* dest, cl_addr_t address, unsigned long long size)
{
#ifdef WIN32
  size_t read;

  return ReadProcessMemory
  (
    m_Handle,
    reinterpret_cast<LPCVOID>(m_MemoryData + address),
    dest,
    size,
    &read
  ) && read == size;
#else
  return false;
#endif
}

bool ClsHook::write(void* src, cl_addr_t address, unsigned long long size)
{
#ifdef WIN32
  size_t written;

  return WriteProcessMemory
  (
    m_Handle,
    reinterpret_cast<void*>(m_MemoryData + address),
    src,
    size,
    &written
  ) && written == size;
#else
  return false;
#endif
}

bool ClsHook::deepCopy(cl_search_t *search)
{
  bool success = true;

  if (!search)
    return false;
  for (int i = 0; i < search->searchbank_count; i++)
  {
    auto sbank = &search->searchbanks[i];

    if (!sbank->bank->data)
    {
      success = false;
      continue;
    }
    else
      success &= read(sbank->bank->data + sbank->first_valid,
                      sbank->first_valid,
                      sbank->last_valid - sbank->first_valid + search->params.size);
  }

  return success;
}

bool ClsHook::pause(void)
{
#ifdef WIN32
  return DebugActiveProcess(m_ProcessId);
#else
  return false;
#endif
}

bool ClsHook::unpause(void)
{
#ifdef WIN32
  return DebugActiveProcessStop(m_ProcessId);
#else
  return false;
#endif
}
