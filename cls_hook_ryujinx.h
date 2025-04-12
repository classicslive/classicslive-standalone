#ifndef CLS_HOOK_RYUJINX_H
#define CLS_HOOK_RYUJINX_H

#include "cls_hook.h"
#include "cls_identification_methods.h"

class ClsHookRyujinx : public ClsHook
{
public:
  ClsHookRyujinx(unsigned pid = 0, const cls_window_preset_t *preset = nullptr,
    void *window = nullptr);

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(uint8_t **data, unsigned *size) override;

  const char *getLibrary(void) override { return "ryujinx"; }

  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  uint64_t memorySize(void) override { return 0xC0000000; }

private:
  uint32_t m_CycleCount = 0;
  cl_identify_nx_t m_Identification;
};

#endif
