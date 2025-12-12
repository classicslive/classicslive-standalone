#include "touchhle.h"

bool ClsHookTouchhle::getIdentification(cl_game_identifier_t *identifier)
{
  return getIdentificationViaFile(identifier);
}

bool ClsHookTouchhle::init(void)
{
  /**
   * The emulator allocates the entire 32-bit address space + some extra, but
   * currently it seems anything relevant is always in the top 1GB. Should the
   * emulator progress to supporting operating system versions for devices with
   * more memory, this should be updated.
   */
  static const cls_find_memory_region_t fmr =
  {
  #if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    .host_offset=0x40,
  #elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    .host_offset=0x10,
  #endif
    .host_size=0x100001000,
    .guest_base=0,
    .guest_size=0x40000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4,
    .title="iOS Current Process Address Space",
    .shared=false
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookTouchhle::run(void)
{
  return true;
}
