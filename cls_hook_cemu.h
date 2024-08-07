#ifndef CLS_HOOK_CEMU_H
#define CLS_HOOK_CEMU_H

#include "cls_hook.h"

typedef struct
{
  /* Big endian */
  uint64_t title_id;

  /* Big endian */
  uint64_t version;
} cl_identify_cafe_t;

class ClsHookCemu : public ClsHook
{
public:
  ClsHookCemu(unsigned pid = 0, const cls_window_preset_t *preset = nullptr);
  bool init() override;
  bool run() override;

  bool getIdentification(uint8_t **data, unsigned int *size) override;
  const char *getLibrary(void) override { return "cemu"; }

  uint64_t memorySize(void) override { return 0x40000000; }

private:
  cl_identify_cafe_t m_Identification;
  uintptr_t m_AddressCemuModule = 0;
  uintptr_t m_AddressForegroundApp = 0;
  uint32_t m_CycleCount = 0;
};

#endif
