#include "xenia.h"

bool ClsHookXenia::getIdentification(cl_game_identifier_t *identifier)
{
  return getIdentificationViaFile(identifier);
}

// loaded xex - purely private, unusable
// 0000000082b10000-000000009fff0000     1d4e0000   469MB ---p

// 0x00000000 - 0x6FFFFFFF
// (1024mb) - virtual 4k pages + (1024mb) - virtual 64k pages (cont) -- 0x10000 reserved
// 0000000200010000-0000000270000000     6fff0000  1792MB rw-s

// 0x70000000 - 0x7F000000
// (1024mb) - virtual 64k pages (cont) -- Stack spaces
// 00000002712f0000-000000027f000000      dd10000   221MB rw-s

// GPU shared memory here. Don't care.

// 0x80000000 - 0x8FFFFFFF
// (256mb) - xex 64k pages - host memory
// xex loaded somewhere after 82000000
// 0000000282b10000-0000000290000000      d4f0000   213MB rw-s

// 0x90000000 - 0x9FFFFFFF
// (256mb) - xex 4k pages - host memory
// 0000000290000000-00000002a0000000     10000000   256MB rw-s

bool ClsHookXenia::init(void)
{
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0,
    .host_size=0x6fff0000,
    .guest_base=0x10000,
    .guest_size=0x6fff0000,
    .endianness=CL_ENDIAN_BIG,
    .pointer_size=4,
    .title="Xbox 360 RAM",
    .shared=true,
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookXenia::run(void)
{
  return true;
}
