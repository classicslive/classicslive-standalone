#ifndef CLS_HOOK_YUZU_H
#define CLS_HOOK_YUZU_H

#include "cls_hook.h"

class ClsHookYuzu : public ClsHook
{
public:
  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "yuzu"; }

private:
  uint32_t m_CycleCount = 0;
};

#endif
