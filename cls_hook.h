#ifndef CLS_HOOK_H
#define CLS_HOOK_H

#ifdef WIN32
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#else
#error "This program currently only supports Windows."
#endif

extern "C"
{
  #include <cl_common.h>
  #include <cl_identify.h>
  #include <cl_memory.h>
  #include <cl_search.h>
}

enum cls_hook_method_t
{
  HOOK_METHOD_BASIC = 0,

  HOOK_METHOD_CEMU,

  HOOK_METHOD_SIZE
};

struct cls_window_preset_t
{
  const char *window_class;
  const char *window_title;
  const char *library_name;
  uintptr_t memory_data;
  uint64_t memory_size;
};

typedef struct
{
  /* The size of the target memory region */
  uint64_t size;

  /* The amount to offset the base after finding region of specified size */
  uint64_t offset;
} cls_find_memory_region;

const cls_window_preset_t cls_window_presets[] =
{
  { "wxWindowNR", /*"Cemu 1.26.2f"*/ "", "cemu", 0x7FF6A6FE6438, 0x40000000 },
  { "Main", "Kero Blaster", "KeroBlaster", 0, 0 },
  { "gdkWindowToplevel", "", "ryujinx", 0, 0 },
  { "SDL_app", "", "touchHLE", 0, 0 },
  { "GLFW30", /*"^Infuse .*"*/ "", "Infuse", 0, 0 },

  { nullptr, nullptr, nullptr, 0, 0 }
};

class ClsHook
{
public:
  ClsHook(const cls_window_preset_t *preset = nullptr);
  virtual ~ClsHook();

  virtual bool init();
  virtual bool run();

  cl_memory_t* memory(void) { return m_Memory; }
  virtual uintptr_t memoryData(void) { return m_MemoryData; }
  virtual uint64_t memorySize(void) { return m_MemorySize; }

  /**
   * Extracts a number of bytes from an address in external process memory into
   *   a provided buffer.
   * @param dest Pointer to a buffer to write to.
   * @param address Address to extract process memory from.
   * @param size Number of bytes to read into the buffer.
   * @return The number of bytes successfully read.
   * @see cl_frontend.h / cl_fe_memory_read
   */
  virtual size_t read(void* dest, cl_addr_t address, size_t size);
  virtual size_t write(const void* src, cl_addr_t address, size_t size);

  /**
   * Extracts as much memory as needed from the host for a search step.
   * @return Whether or not the copy succeeded.
   **/
  virtual bool deepCopy(cl_search_t *search);

  virtual bool getIdentification(uint8_t **data, unsigned *size) {}

  virtual bool pause(void);
  virtual bool unpause(void);

  bool initViaMemoryRegions(const cls_find_memory_region fvmr);

protected:
  char m_ContentHash[32 + 1];
  cl_memory_t *m_Memory = nullptr;
  uintptr_t m_MemoryData = 0;
  uint64_t m_MemorySize = 0;
  const cls_window_preset_t *m_Preset = nullptr;

#ifdef WIN32
  HWND m_Window;
  HANDLE m_Handle;
  DWORD m_ProcessId;
#elif __linux__
  pid_t m_ProcessId;
#endif
};

#endif
