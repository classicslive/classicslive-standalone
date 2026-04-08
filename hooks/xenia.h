#ifndef CLS_HOOK_XENIA_H
#define CLS_HOOK_XENIA_H

#include "cls_hook.h"

class ClsHookXenia : public ClsHook
{
public:
  ClsHookXenia(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "xenia"; }
};

#endif
