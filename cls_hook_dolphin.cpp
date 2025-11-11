#include "cls_hook_dolphin.h"

bool ClsHookDolphin::getIdentification(cl_game_identifier_t *identifier)
{
  int temp = 0;

  if (identifier)
  {
    identifier->data = reinterpret_cast<uint8_t*>(&temp);
    identifier->size = sizeof(temp);
    m_MemorySize = 24 * 1024 * 1024;

    return true;
  }
  else
    return false;
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
