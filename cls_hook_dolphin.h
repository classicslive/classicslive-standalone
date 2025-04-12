#ifndef CLS_HOOK_DOLPHIN_H
#define CLS_HOOK_DOLPHIN_H

#include "cls_hook.h"

class ClsHookDolphin : public ClsHook
{
public:
  bool init(void) override;

  bool run(void) override;

  bool getIdentification(uint8_t **data, unsigned *size) override;

  const char *getLibrary(void) override { return "dolphin"; }

private:
  typedef enum
  {
    CLS_DOLPHIN_TYPE_UNKNOWN = 0,

    CLS_DOLPHIN_TYPE_GAMECUBE,
    CLS_DOLPHIN_TYPE_WII,
    CLS_DOLPHIN_TYPE_TRIFORCE,

    CLS_DOLPHIN_TYPE_SIZE
  } cls_dolphin_type;

  cls_dolphin_type type;
};

#endif
