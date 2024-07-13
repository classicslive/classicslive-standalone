#include "cls_hook_infuse.h"

ClsHookInfuse::ClsHookInfuse(const cls_window_preset_t *preset) : ClsHook(preset) {}

static cl_identify_zeebo_t test_bundle = { 0x233b0 };

bool ClsHookInfuse::getIdentification(uint8_t **data, unsigned int *size)
{
  *data = reinterpret_cast<uint8_t*>(&test_bundle);
  *size = sizeof(test_bundle);
  return true;
}

bool ClsHookInfuse::init(void)
{
  if (!ClsHook::init())
    return false;
  else
    return ClsHook::initViaMemoryRegions({0xA001000, 0x40});
}

bool ClsHookInfuse::run(void)
{
  return true;
}
