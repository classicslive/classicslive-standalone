#include "citra.h"

bool ClsHookCitra::getIdentification(cl_game_identifier_t *identifier)
{
  /** @todo Find the NCCH header in memory and run the custom hasher in cl_identify.c */
  return false;
}

bool ClsHookCitra::init(void)
{
  static const cls_find_memory_region_t fmr =
  {
    /** @todo
     * Is this offset correct?
     * Test on Windows -- is the extra data size the same?
     * This assumes New 3DS; add Old 3DS mode?
     */
    .host_offset=0x2d000,
    .host_size=0x1002d000,
    .guest_base=0,
    .guest_size=CL_MB(256),
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4,
    .title="Nintendo 3DS RAM",
    .shared=false,
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookCitra::run(void)
{
  return true;
}
