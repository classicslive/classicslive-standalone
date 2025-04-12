#ifndef CLS_HOOK_H
#define CLS_HOOK_H

extern "C"
{
  #include <cl_common.h>
  #include <cl_identify.h>
  #include <cl_memory.h>
  #include <cl_search.h>
}

#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#endif

enum cls_hook_method_t
{
  HOOK_METHOD_BASIC = 0,

  HOOK_METHOD_CEMU,

  HOOK_METHOD_SIZE
};

struct cls_window_preset_t
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  const char *window_class;
  const char *window_title;
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  const char *process_title;
#endif
  const char *title;
};

typedef struct
{
  /* The size of the target memory region */
  uint64_t size;

  /* The amount to offset the base after finding region of specified size */
  uint64_t offset;
} cls_find_memory_region_t;

const cls_window_preset_t cls_window_presets[] =
{
  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "^wxWindowNR$",
    "^Cemu [1-9].*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^cemu$",
#endif
    "Cemu"
  },

  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "gdkWindowToplevel", "",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "todo",
#endif
    "Ryujinx"
  },

  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "SDL_app", "",
#elif defined (__linux__)
    "todo",
#endif
    "touchHLE"
  },

  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "GLFW30", "^Infuse .*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^Infuse$",
#endif
    "Infuse"
  },

  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    nullptr, nullptr,
#else
    nullptr,
#endif
    nullptr
  }
};

class ClsHook
{
public:
  ClsHook(unsigned pid = 0, const cls_window_preset_t *preset = nullptr,
    void *window = nullptr);

  virtual ~ClsHook();

  virtual bool init(void);

  virtual bool run(void);

  cl_memory_t* memory(void) { return m_Memory; }

  virtual uintptr_t memoryData(void) { return m_MemoryData; }

  virtual uint64_t memorySize(void) { return m_MemorySize; }

  /**
   * Extracts a number of bytes from an address in external process memory
   * into a provided buffer.
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
   */
  virtual bool deepCopy(cl_search_t *search);

  /**
   * Returns a buffer of data to be hashed to be provided as game
   * identification information.
   * @param data A pointer to identification data to be hashed
   * @param size The size of the data
   * @return Whether or not identification info could be retrieved
   */
  virtual bool getIdentification(uint8_t **data, unsigned *size)
  {
    *data = nullptr;
    *size = 0;

    return false;
  }

  /**
   * @return The library name of the guest program
   */
  virtual const char *getLibrary(void) { return "unknown"; }

  /**
   * Attempts to pause the guest program. Triggered when debugging scripts or
   * optionally by a deep copy.
   * @return Whether the pause succeeded
   */
  virtual bool pause(void);

  /**
   * Attempts to unpause the guest program after pausing it.
   * @return Whether the unpause succeeded
   */
  virtual bool unpause(void);

  /**
   * Reads the window title of the hooked program into a buffer.
   * @return Whether the window title was read
   */
  virtual bool getWindowTitle(char *buffer, unsigned buffer_len);

  cl_memory_region_t findMemoryRegion(const cls_find_memory_region_t fvmr);

  bool initViaMemoryRegions(const cls_find_memory_region_t fvmr);

protected:
  char m_ContentHash[32 + 1];
  cl_memory_t *m_Memory = nullptr;
  uintptr_t m_MemoryData = 0;
  uint64_t m_MemorySize = 0;
  const cls_window_preset_t *m_Preset = nullptr;

#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  HWND m_Window = nullptr;
  HANDLE m_Handle = nullptr;
  DWORD m_ProcessId = 0;
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  pid_t m_ProcessId;
#endif
};

#endif
