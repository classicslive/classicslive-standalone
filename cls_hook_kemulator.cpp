#include "cls_hook_kemulator.h"

#include <string.h>

bool ClsHookKemulator::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
    strncpy(identifier->product, "j2me_test", sizeof(identifier->product));
    strncpy(identifier->version, "1", sizeof(identifier->version));
    strncpy(identifier->filename, "j2me_test", sizeof(identifier->product));

    return true;
  }
  else
    return false;
}

bool ClsHookKemulator::init(void)
{
  /**
   * The Java objects we need for memory access are always statically mapped
   * here. There's usually another region at 0xf4e00000 but I don't know its
   * purpose.
   */
  cl_memory_region_t region;

  region.base_alloc = (void*)0xe0000000;
  region.base_guest = 0xe0000000;
  region.base_host = (void*)0xe0000000;
  region.endianness = CL_ENDIAN_LITTLE;
  region.flags.bits.read = 1;
  region.flags.bits.write = 1;
  region.pointer_length = 4;
  region.size = 0x2000000;
  strncpy(region.title, "Java Objects", sizeof(region.title));

  m_MemoryRegions[0] = region;
  m_MemoryRegionCount = 1;

  return ClsHook::init();
}

bool ClsHookKemulator::run(void)
{
  return true;
}
