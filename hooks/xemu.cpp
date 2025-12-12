#include "xemu.h"

bool ClsHookXemu::getIdentification(cl_game_identifier_t *identifier)
{
  return getIdentificationViaFile(identifier);
}

bool ClsHookXemu::init(void)
{
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0,
    .host_size=0x04000000,
    .guest_base=0,
    .guest_size=0x04000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4,
    .title="Xbox SDRAM",
    .shared = false,
  };
  cl_memory_region_t regions[16];
  unsigned found = ClsHook::findRegions(regions, 16, fmr);
  unsigned deadbeef, i;

  /**
   * There will be a few regions of the expected size. The correct one begins
   * RAM with 0xDEADBEEF.
   * @todo 128MB mode?
   */
  m_MemoryRegionCount = 1;
  for (i = 0; i < found; i++)
  {
    m_MemoryRegions[0] = regions[i];
    if (read(&deadbeef, 0, sizeof(deadbeef)) == sizeof(deadbeef) &&
        deadbeef == 0xdeadbeef)
      return ClsHook::init();
  }
  m_MemoryRegionCount = 0;

  return false;
}

bool ClsHookXemu::run(void)
{
  return true;
}
