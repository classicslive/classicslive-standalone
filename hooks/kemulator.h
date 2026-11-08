#ifndef CLS_HOOK_KEMULATOR_H
#define CLS_HOOK_KEMULATOR_H

#include "cls_hook.h"

#include <string>
#include <unordered_map>

/**
 * The guest address the static fields of the emulator's main class are
 * exposed at. Java objects move whenever the garbage collector compacts the
 * heap, so this is the only stable base for pointer chains.
 */
#define CLS_KEMULATOR_STATICS_BASE 0x10000000

class ClsHookKemulator : public ClsHook
{
public:
  ClsHookKemulator(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool run(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "kemulator"; }

private:
  /* Reads memory at an address in the JVM's own address space */
  bool readHost(void *dest, uintptr_t address, size_t size);

  uintptr_t readHostPointer(uintptr_t address);

  std::string readHostString(uintptr_t address);

  /* Finds the addresses of exported symbols in the JVM library */
  bool findJvmSymbols(const char **names, uintptr_t *addresses, unsigned count);

  /* Loads the field offsets HotSpot exports for its serviceability agent */
  bool loadVmStructs(void);

  /* Looks up an exported field offset, or a static field's address */
  bool vmField(const char *type, const char *field, uintptr_t *value);

  /* Finds the address of the OopHandle slot holding a class's mirror */
  bool findMirrorSlot(const char *class_name, uintptr_t *klass_out,
    uintptr_t *slot);

  std::unordered_map<std::string, uintptr_t> m_VmFields;

  /* Native address holding the current address of the emulator class mirror */
  uintptr_t m_MirrorSlot = 0;

  /* Offset of the first static field from the start of a class mirror */
  uintptr_t m_StaticsOffset = 0;
};

#endif
