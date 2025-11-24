#include "touchhle.h"

#include <string.h>

/** @todo this is no longer needed but string lengths should be documented */
typedef struct
{
  /**
   * The bundle identifier of the software; ie, "net.classicslive.cls". Make
   * sure to fill any unused data with 00s.
   * https://developer.apple.com/documentation/bundleresources/information_property_list/cfbundleidentifier
   * https://stackoverflow.com/questions/49001779/what-is-maximum-allowed-length-of-ios-bundleid
   */
  char identifier[155 + 1];

  /**
   * The version of the software as a string; ie, "1.3.0". Make sure to fill
   * any unused data with 00s.
   * https://stackoverflow.com/questions/38330781/the-value-for-key-cfbundleversion-in-the-info-plist-file-must-be-no-longer-than
   */
  char version[18 + 1];
} cl_identify_bundle_t;

static cl_identify_bundle_t test_bundle = { "jp.co.capcom.res4", "1.00.00" };

bool ClsHookTouchhle::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
    strncpy(identifier->product, test_bundle.identifier, sizeof(identifier->product));
    strncpy(identifier->version, test_bundle.version, sizeof(identifier->version));
    strncpy(identifier->filename, test_bundle.identifier, sizeof(identifier->product));

    return true;
  }
  else
    return false;
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
    .title="iOS Current Process Address Space"
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookTouchhle::run(void)
{
  return true;
}
