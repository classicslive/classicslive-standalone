#ifndef CLS_HOOK_RYUJINX_H
#define CLS_HOOK_RYUJINX_H

#include "cls_hook.h"

typedef struct
{
  /**
   * The title ID of the game, a 64-bit integer.
   * https://switchbrew.org/wiki/Title_list/Games
   */
  uint64_t title_id;

  /**
   * The version of the game as a string; ie, "1.3.0". The "v" is not actually
   * part of the string. Make sure to fill any unused data with 00s.
   * @todo The length of the string is taken from system version definition and
   * might not be accurate for game update strings.
   */
  char version[0x18];

  /**
   * A 32-bit (?) identifier for the software version.
   * Appears to just be the revision number shifted left by two bytes, not
   * representing the "version number" string.
   * uint64_t version = 0x40000;
   * @todo Currently we are using the string instead. Would there be a reason
   * to need this?
   */
} cl_identify_nx_t;

class ClsHookRyujinx : public ClsHook
{
public:
  ClsHookRyujinx(unsigned pid = 0, const cls_window_preset_t *preset = nullptr);

  bool init() override;
  bool run() override;
  size_t read(void *dest, cl_addr_t address, size_t size) override;
  size_t write(const void *src, cl_addr_t address, size_t size) override;
  bool deepCopy(cl_search_t *search) override;
  bool getIdentification(uint8_t **data, unsigned *size) override;
  const char *getLibrary(void) override { return "ryujinx"; }

  uintptr_t memoryData(void) override { return m_AddressForegroundApp; }

  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  uint64_t memorySize(void) override { return 0xC0000000; }

private:
  uintptr_t m_AddressForegroundApp = 0;
  uint32_t m_CycleCount = 0;
  cl_identify_nx_t m_Identification;
};

#endif
