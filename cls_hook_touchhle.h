#ifndef CLS_HOOK_TOUCHHLE_H
#define CLS_HOOK_TOUCHHLE_H

#include "cls_hook.h"

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

class ClsHookTouchhle : public ClsHook
{
public:
  ClsHookTouchhle(unsigned pid = 0, const cls_window_preset_t *preset = nullptr);

  bool init(void) override;
  bool run(void) override;
  bool getIdentification(uint8_t **data, unsigned *size) override;
  const char *getLibrary(void) override { return "touchhle"; }

  /**
   * The emulator allocates the entire 32-bit address space, but currently it
   * seems anything relevant is always in the top 1GB. Should the emulator
   * progress to supporting operating system versions for devices with more
   * memory, this should be updated.
   */
  uint64_t memorySize(void) override { return 0x40000000; }

private:
  cl_identify_bundle_t m_Identification;
};

#endif
