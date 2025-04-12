#include "cls_hook_touchhle.h"

static cl_identify_bundle_t test_bundle = { "jp.co.capcom.res4", "1.00.00" };

bool ClsHookTouchhle::getIdentification(uint8_t **data, unsigned int *size)
{
  *data = reinterpret_cast<uint8_t*>(&test_bundle);
  *size = sizeof(test_bundle);
  return true;
}

bool ClsHookTouchhle::init(void)
{
  /**
   * Query all memory regions looking for one of size 4GB + some extra. This
   * emulator allocates the entire possible 32-bit address space into one
   * bucket, but for reasonable purposes, CL is only accessing the top 1GB.
   */
  if (!ClsHook::init())
    return false;
  else
    return ClsHook::initViaMemoryRegions({0x100001000, 0x40});
}

bool ClsHookTouchhle::run(void)
{
  return true;
}
