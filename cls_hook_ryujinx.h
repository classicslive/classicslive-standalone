#ifndef CLS_HOOK_RYUJINX_H
#define CLS_HOOK_RYUJINX_H

#include "cls_hook.h"

class ClsHookRyujinx : public ClsHook
{
public:
  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "ryujinx"; }

private:
  uint32_t m_CycleCount = 0;
};

#endif
