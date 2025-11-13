#include "cls_hook_infuse.h"

static unsigned test_bundle = 0x233b0;

bool ClsHookInfuse::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_FILE_HASH;
    identifier->data = reinterpret_cast<uint8_t*>(&test_bundle);
    identifier->size = sizeof(test_bundle);

    return true;
  }
  else
    return false;
}

bool ClsHookInfuse::init(void)
{
  static const cls_find_memory_region_t fmr =
  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    .host_offset=0x40,
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    .host_offset=0x10,
#endif
    .host_size=0xA001000,
    .guest_base=0,
    .guest_size=0xA000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookInfuse::run(void)
{
  return true;
}
