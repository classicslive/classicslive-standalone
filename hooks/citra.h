#ifndef CLS_HOOK_CITRA_H
#define CLS_HOOK_CITRA_H

#include "cls_hook.h"

class ClsHookCitra : public ClsHook
{
public:
  ClsHookCitra(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "citra"; }
};

#endif
