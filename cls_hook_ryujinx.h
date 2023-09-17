#ifndef CLS_HOOK_RYUJINX_H
#define CLS_HOOK_RYUJINX_H

#include "cls_hook.h"

class ClsHookRyujinx : public ClsHook
{
public:
  ClsHookRyujinx(const cls_window_preset_t *preset);
  bool init() override;
  bool run() override;

  unsigned read(void *dest, cl_addr_t address, unsigned long long size) override;
  unsigned write(const void *src, cl_addr_t address, unsigned long long size) override;
  bool deepCopy(cl_search_t *search) override;

  uintptr_t memoryData(void) override { return m_AddressForegroundApp; }

  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  uint64_t memorySize(void) override { return 0xC0000000; }

private:
  uintptr_t m_AddressForegroundApp = 0;
  uint32_t m_CycleCount = 0;
};

#endif
