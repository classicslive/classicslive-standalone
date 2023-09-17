#include "cls_hook_ryujinx.h"

ClsHookRyujinx::ClsHookRyujinx(const cls_window_preset_t *preset) : ClsHook(preset) {}

bool ClsHookRyujinx::init(void)
{
  if (!ClsHook::init())
    return false;
  else
  {
    MEMORY_BASIC_INFORMATION memory;
    char *addr = nullptr;

    while (VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(addr), &memory, sizeof(MEMORY_BASIC_INFORMATION)))
    {
      if (memory.RegionSize == memorySize() && memory.Protect == PAGE_READWRITE)
      {
        m_AddressForegroundApp = reinterpret_cast<uintptr_t>(memory.BaseAddress);
        return true;
      }
      addr += memory.RegionSize;
    }


    return false;
  }
}

bool ClsHookRyujinx::run(void)
{
  return true;
}

unsigned ClsHookRyujinx::read(void *dest, cl_addr_t address, unsigned long long size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::read(dest, address + m_AddressForegroundApp - 0x10000, size);
}

unsigned ClsHookRyujinx::write(const void *src, cl_addr_t address, unsigned long long size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::write(src, address + m_AddressForegroundApp - 0x10000, size);
}

bool ClsHookRyujinx::deepCopy(cl_search_t *search)
{
  if (!search || search->searchbank_count != 1 || !search->searchbanks->bank[0].data)
    return false;

  return read(
    search->searchbanks[0].bank->data + search->searchbanks[0].first_valid,
    search->searchbanks[0].first_valid,
    search->searchbanks[0].last_valid - search->searchbanks[0].first_valid + search->params.size);
}

