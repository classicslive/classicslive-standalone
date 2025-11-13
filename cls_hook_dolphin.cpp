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
  /** @todo Wii, Triforce */
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0,
    .host_size=0x2000000,
    .guest_base=0,
    .guest_size=0,
    .endianness=CL_ENDIAN_BIG,
    .pointer_size=4
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookDolphin::run(void)
{
  return true;
}
