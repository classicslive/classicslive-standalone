#ifndef CLS_HOOK_INFUSE_H
#define CLS_HOOK_INFUSE_H

#include "cls_hook.h"

typedef struct
{
  uint64_t test;
} cl_identify_zeebo_t;

class ClsHookInfuse : public ClsHook
{
public:
  ClsHookInfuse(const cls_window_preset_t *preset);

  bool init() override;
  bool run() override;
  bool getIdentification(uint8_t **data, unsigned *size) override;

  uint64_t memorySize(void) override { return 0x0A000000; }

private:
  cl_identify_zeebo_t m_Identification;
};

#endif