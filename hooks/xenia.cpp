#include "xenia.h"

#include <string.h>

#define CLS_XENIA_INCLUDE_4K_HEAP 0
#define CLS_XENIA_INCLUDE_STACK   0
#define CLS_XENIA_INCLUDE_XEX     0

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
  static const cl_addr_t heap4k_base = 0x00010000;
  static const cl_addr_t heap64k_base = 0x40000000;
  static const cl_addr_t heap4k_size = heap64k_base - heap4k_base;
  static const cl_addr_t heap64k_size = 0x70000000 - heap64k_base;

  static const cls_find_memory_region_t virtual_pages =
  {
    .host_offset=0,
    .host_size=0x6fff0000,
    .guest_base=0x10000,
    .guest_size=0x6fff0000,
    .endianness=CL_ENDIAN_BIG,
    .pointer_size=4,
    .title="Heaps",
    .shared=true,
  };
  cl_memory_region_t combined;
  cl_memory_region_t heap4k;
  cl_memory_region_t heap64k;
  unsigned region_index = 0;

  if (!ClsHook::init() || !findRegions(&combined, 1, virtual_pages))
    return false;

#if CLS_XENIA_INCLUDE_4K_HEAP
  heap4k = combined;
  heap4k.base_guest = heap4k_base;
  heap4k.size = heap4k_size;
  strncpy(heap4k.title, "4K Heap", sizeof(heap4k.title));
  heap4k.title[sizeof(heap4k.title) - 1] = '\0';
  m_MemoryRegions[region_index++] = heap4k;
#endif

  heap64k = combined;
  heap64k.base_host = reinterpret_cast<uint8_t*>(combined.base_host) + heap4k_size;
  heap64k.base_guest = heap64k_base;
  heap64k.size = heap64k_size;
  strncpy(heap64k.title, "64K Heap", sizeof(heap64k.title));
  heap64k.title[sizeof(heap64k.title) - 1] = '\0';
  m_MemoryRegions[region_index++] = heap64k;
  m_MemoryRegionCount = region_index;

  auto add_anchor_region = [&](uintptr_t host_anchor, cl_addr_t guest_end,
    const char *title)
  {
    cl_memory_region_t region;

    if (m_MemoryRegionCount >= 16)
      return;

    if (findRegionByHostAddress(&region, host_anchor))
    {
      region.base_guest = guest_end - region.size;
      region.endianness = CL_ENDIAN_BIG;
      region.pointer_length = 4;
      strncpy(region.title, title, sizeof(region.title));
      region.title[sizeof(region.title) - 1] = '\0';

      m_MemoryRegions[m_MemoryRegionCount++] = region;
    }
  };

#if CLS_XENIA_INCLUDE_STACK
  add_anchor_region(0x000000027effffffULL, 0x80000000, "Stacks");
#endif

#if CLS_XENIA_INCLUDE_XEX
  add_anchor_region(0x000000028fffffffULL, 0x90000000, "64K XEX");
  add_anchor_region(0x000000029fffffffULL, 0xA0000000, "4K XEX");
#endif

  return true;
}

bool ClsHookXenia::run(void)
{
  return true;
}
