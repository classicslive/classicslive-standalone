#ifndef CLS_HOOK_HORIZON_H
#define CLS_HOOK_HORIZON_H

#include <stdint.h>

#define CLS_HORIZON_MAIN_BASE 0x8000000000ULL

/* Module header, at the offset given by the second word of every module */
typedef struct
{
  char magic[4];

  /* Offsets from the start of this header */
  int32_t dynamic;
  int32_t bss_start;
  int32_t bss_end;
  int32_t eh_frame_hdr_start;
  int32_t eh_frame_hdr_end;
  int32_t module_object;
} cls_horizon_mod0_t;

#endif
