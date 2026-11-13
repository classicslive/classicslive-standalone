#ifndef CLS_HOOK_YUZU_H
#define CLS_HOOK_YUZU_H

#include "cls_hook.h"

#include <QElapsedTimer>

#include <vector>

class ClsHookYuzu : public ClsHook
{
public:
  ClsHookYuzu(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  size_t read(void *dest, cl_addr_t address, size_t size) override;
  size_t write(const void *src, cl_addr_t address, size_t size) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "yuzu"; }

private:
  /* One mapping of the emulated DRAM into the fastmem arena */
  struct ArenaMapping
  {
    uintptr_t host;
    uintptr_t size;

    /* Offset into the DRAM backing, ie. the guest physical address */
    uint64_t physical;

    /* Mapped r--s; the arena maps code and read-only data this way */
    bool readonly;
  };

  struct Module
  {
    cl_addr_t base;
    cl_addr_t size;

    /* Guest address of the module object rtld keeps for this module */
    uint64_t object;

    /* The first two pointers of the module object: links of rtld's module list */
    uint64_t links[2];
  };

  /* Orders arena mappings by host address, for qsort */
  static int compareMappings(const void *a, const void *b);

  /**
   * Identifies the game from the emulator's log. Only used for the title ID;
   * modules are found in memory.
   */
  bool parseLog(QString &error);

  /**
   * Reads the arena's mappings from the process's memory map.
   * @return Whether the DRAM backing was found
   */
  bool readMappings(void);

  /**
   * Finds the game's modules by scanning read-only arena mappings for module
   * headers, and with them the arena base and the main module.
   * @return Whether the main module was found unambiguously
   */
  bool findModules(void);

  bool initViaArena(void);

  /**
   * Adds a memory region at a guest virtual address in the arena.
   * @return The region, or nullptr if there is no room for another
   */
  cl_memory_region_t *addRegion(cl_addr_t guest, cl_addr_t size, const char *title);

  /**
   * Reads from a host address in the arena, wherever it is held in the DRAM
   * backing. A module's pieces may be in separate mappings.
   * @return Whether all of the bytes were read
   */
  bool readArena(void *dest, uintptr_t host, size_t size);

  /**
   * Reads or writes guest virtual memory, which may be scattered in the DRAM
   * backing even where it is contiguous.
   * @return The number of bytes transferred
   */
  size_t transfer(void *buffer, cl_addr_t address, size_t size, bool write);

  /**
   * Finds where a host address in the arena is held in the DRAM backing.
   * @param host The host address in the arena
   * @param available Receives how many bytes can be accessed from there
   * @return The host address in the backing, or 0 if it is not mapped
   */
  uintptr_t arenaBackingAddress(uintptr_t host, size_t *available);

  /**
   * Finds where a guest virtual address is held in the DRAM backing.
   * @param address The guest virtual address, in the canonical layout
   * @param available Receives how many bytes can be accessed from there
   * @return The host address in the backing, or 0 if it is not mapped
   */
  uintptr_t backingAddress(cl_addr_t address, size_t *available);

  /* Guest virtual memory: emulated DRAM mapped at fastmem arena + address */
  std::vector<ArenaMapping> m_Mappings;
  uintptr_t m_ArenaBase = 0;
  uintptr_t m_Backing = 0;
  QElapsedTimer m_MappingsAge;

  /* Modules of the running game, in address order, and main among them */
  std::vector<Module> m_Modules;
  Module m_Main = {};
  unsigned m_PointerSize = 8;

  cl_game_identifier_t m_Identifier = {};
  QString m_LogError;
  bool m_LogParsed = false;
};

#endif
