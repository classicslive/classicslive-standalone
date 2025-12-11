#include "vita3k.h"

#include <string.h>

bool ClsHookVita3k::getIdentification(cl_game_identifier_t *identifier)
{
  /** @todo */
  identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
  strncpy(identifier->product, "1", sizeof(identifier->product));
  strncpy(identifier->filename, "1", sizeof(identifier->filename));
  strncpy(identifier->version, "1", sizeof(identifier->version));

  return true;
}

bool ClsHookVita3k::init(void)
{
  /**
   * @todo Vita3K prefers a mapping of a 32-bit address space (4GB) at this
   * exact address, but the OS can use something else. A proper search would
   * be more appropriate.
   *
   * Base class needs to be edited to read across multiple host regions for
   * emus that implement memory safety better, like this one. Lazybones
   * solution here is to ignore a chunk at the start thats always private (1).
   *
   * Do we want to use anything else? Cheats seem to live here
   */
  memset(&m_MemoryRegions[0], 0, sizeof(cl_memory_region_t));
  m_MemoryRegions[0].base_alloc = reinterpret_cast<void*>(0x481000000);
  m_MemoryRegions[0].base_host = reinterpret_cast<void*>(0x481000000);
  m_MemoryRegions[0].base_guest = 0x81000000;
  m_MemoryRegions[0].endianness = CL_ENDIAN_LITTLE;
  m_MemoryRegions[0].pointer_length = 4;
  m_MemoryRegions[0].size = 512 * 1024 * 1024 - 0x1000000;
  m_MemoryRegionCount = 1;

  return ClsHook::init();
}

bool ClsHookVita3k::run(void)
{
  return true;
}
