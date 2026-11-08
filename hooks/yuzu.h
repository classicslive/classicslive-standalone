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
  };

  struct Module
  {
    cl_addr_t base;
    cl_addr_t size;
  };

  bool parseLog(QString &error);

  /**
   * Reads the arena's mappings from the process's memory map.
   * @return Whether the DRAM backing was found
   */
  bool readMappings(void);

  /**
   * @return Whether the given arena base puts a module header at every
   *   module's logged address
   */
  bool validateArenaBase(uintptr_t base);

  bool initViaArena(void);

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

  /* Modules of the running game, as logged, and main among them */
  std::vector<Module> m_Modules;
  Module m_Main = {};
  unsigned m_PointerSize = 8;

  cl_game_identifier_t m_Identifier = {};
  QString m_LogError;
  bool m_LogParsed = false;
};

#endif
