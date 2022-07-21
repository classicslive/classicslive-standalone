#ifndef CLS_HOOK_CEMU_H
#define CLS_HOOK_CEMU_H

#include "cls_hook.h"

class ClsHookCemu : public ClsHook
{
public:
  ClsHookCemu(const cls_window_preset_t *preset = nullptr);
  bool init() override;
  bool run() override;

  bool read(void *dest, cl_addr_t address, unsigned long long size) override;
  bool write(void *src, cl_addr_t address, unsigned long long size) override;
  bool deepCopy(cl_search_t *search) override;

  uintptr_t memoryData(void) override { return m_AddressForegroundApp; }
  uint64_t memorySize(void) override { return 0x40000000; }

private:
  uintptr_t m_AddressCemuModule = 0;
  uintptr_t m_AddressForegroundApp = 0;
  uint32_t m_CycleCount = 0;
};

#endif
