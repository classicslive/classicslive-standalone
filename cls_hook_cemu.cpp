#include "cls_hook_cemu.h"

uintptr_t GetModuleBaseAddress(unsigned process_id, const wchar_t* module_name)
{
  HANDLE th = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);

  if (th != INVALID_HANDLE_VALUE)
  {
    MODULEENTRY32 module;

    module.dwSize = sizeof(module);
    if (Module32First(th, &module))
    {
      do
      {
        if (!wcsicmp(module.szModule, module_name))
        {
          CloseHandle(th);
          return reinterpret_cast<uintptr_t>(module.modBaseAddr);
        }
      } while (Module32Next(th, &module));
    }
  }
  CloseHandle(th);

  return 0;
}

ClsHookCemu::ClsHookCemu(const cls_window_preset_t *preset)
{
  m_Preset = preset;
}

bool ClsHookCemu::init()
{
  if (!ClsHook::init())
    return false;

  m_AddressCemuModule = GetModuleBaseAddress(m_ProcessId, L"Cemu.exe");
  if (!m_AddressCemuModule)
    return false;

  if (!ClsHook::read(&m_AddressForegroundApp,
            m_AddressCemuModule + 0x1306438,
            sizeof(m_AddressForegroundApp)) || !m_AddressForegroundApp)
    return false;

  return true;
}

bool ClsHookCemu::run()
{
  if (!m_AddressCemuModule)
    return false;
  ClsHook::read(&m_CycleCount, m_AddressCemuModule + 0x13064EC, sizeof(m_CycleCount));

  return m_CycleCount;
}

bool ClsHookCemu::read(void *dest, cl_addr_t address, unsigned long long size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::read(dest, address + m_AddressForegroundApp, size);
}

bool ClsHookCemu::write(void *src, cl_addr_t address, unsigned long long size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::write(src, address + m_AddressForegroundApp, size);
}

bool ClsHookCemu::deepCopy(cl_search_t *search)
{
  if (!search || search->searchbank_count != 1 || !search->searchbanks->bank[0].data)
    return false;

  return read(
    search->searchbanks[0].bank->data + search->searchbanks[0].first_valid,
    search->searchbanks[0].first_valid + 0x10000000,
    search->searchbanks[0].last_valid - search->searchbanks[0].first_valid + search->params.size);
}
