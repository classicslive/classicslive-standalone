#ifndef CLS_HOOK_CEMU_H
#define CLS_HOOK_CEMU_H

#include "cls_hook.h"

class ClsHookCemu : public ClsHook
{
public:
  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "cemu"; }
};

#endif
