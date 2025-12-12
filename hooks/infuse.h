#ifndef CLS_HOOK_INFUSE_H
#define CLS_HOOK_INFUSE_H

#include "cls_hook.h"

class ClsHookInfuse : public ClsHook
{
public:
  ClsHookInfuse(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "infuse"; }
};

#endif
