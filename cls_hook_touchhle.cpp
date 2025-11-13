#include "cls_hook_touchhle.h"

#include <string.h>

static cl_identify_bundle_t test_bundle = { "jp.co.capcom.res4", "1.00.00" };

bool ClsHookTouchhle::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
    strncpy(identifier->product, test_bundle.identifier, sizeof(identifier->product));
    strncpy(identifier->version, test_bundle.version, sizeof(identifier->version));

    return true;
  }
  else
    return false;
}

bool ClsHookTouchhle::init(void)
{
  /**
   * Query all memory regions looking for one of size 4GB + some extra. This
   * emulator allocates the entire possible 32-bit address space into one
   * bucket, but for reasonable purposes, CL is only accessing the top 1GB.
   */
  cls_find_memory_region_t fmr =
  {
    .host_offset=0x40,
    .host_size=0x100001000,
    .guest_base=0,
    .guest_size=0x40000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookTouchhle::run(void)
{
  return true;
}
