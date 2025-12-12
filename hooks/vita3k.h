#ifndef CLS_HOOK_VITA3K_H
#define CLS_HOOK_VITA3K_H

#include "cls_hook.h"

class ClsHookVita3k : public ClsHook
{
public:
  ClsHookVita3k(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "vita3k"; }
};

#endif
