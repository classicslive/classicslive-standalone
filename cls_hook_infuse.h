#ifndef CLS_HOOK_INFUSE_H
#define CLS_HOOK_INFUSE_H

#include "cls_hook.h"

class ClsHookInfuse : public ClsHook
{
public:
  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "infuse"; }
};

#endif
