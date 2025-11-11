#include "cls_hook_infuse.h"

static cl_identify_zeebo_t test_bundle = { 0x233b0 };

ClsHookInfuse::ClsHookInfuse(unsigned pid, const cls_window_preset_t *preset,
  void *window) : ClsHook(pid, preset, window) {}

bool ClsHookInfuse::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_FILE_HASH;
    identifier->data = reinterpret_cast<uint8_t*>(&test_bundle);
    identifier->size = sizeof(test_bundle);
    m_MemorySize = 0xA000000;

    return true;
  }
  else
    return false;
}

bool ClsHookInfuse::init(void)
{
  if (!ClsHook::init())
    return false;
  else
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    return ClsHook::initViaMemoryRegions({0xA001000, 0x40});
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    return ClsHook::initViaMemoryRegions({0xA001000, 0x10});
#else
    return false;
#endif
}

bool ClsHookInfuse::run(void)
{
  return true;
}
