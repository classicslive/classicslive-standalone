#include "cls_hook_dolphin.h"

ClsHookDolphin::ClsHookDolphin(unsigned pid, const cls_window_preset_t *preset) : ClsHook(pid, preset) {}

bool ClsHookDolphin::getIdentification(uint8_t **data, unsigned int *size)
{
  int temp = 0;

  *data = reinterpret_cast<uint8_t*>(&temp);
  *size = sizeof(temp);
  m_MemorySize = 24 * 1024 * 1024;

  return true;
}

bool ClsHookDolphin::init(void)
{
  if (!ClsHook::init())
    return false;
  else
    return ClsHook::initViaMemoryRegions({0x2000000, 0});
}

bool ClsHookDolphin::run(void)
{
  return true;
}
