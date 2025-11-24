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

typedef enum
{
  CLS_HOOK_GENERIC = 0,

  CLS_HOOK_CEMU,
  CLS_HOOK_DOLPHIN,
  CLS_HOOK_INFUSE,
  CLS_HOOK_KEMULATOR,
  CLS_HOOK_RYUJINX,
  CLS_HOOK_TOUCHHLE,
  CLS_HOOK_XEMU,
  CLS_HOOK_YUZU,

  CLS_HOOK_SIZE
} cls_hook_type;

struct cls_window_preset_t
{
  cls_hook_type type;
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
  /**
   * The amount to offset the base in host memory after finding region of
   * specified size
   */
  cl_addr_t host_offset;

  /* The size of the target memory region in host memory */
  cl_addr_t host_size;

  /* The base virtual address of this region in guest memory */
  cl_addr_t guest_base;

  /* The size of the target memory region in guest memory */
  cl_addr_t guest_size;

  cl_endianness endianness;

  unsigned pointer_size;

  const char *title;

  /* Whether or not the region uses shared mapping. If false, it's private */
  bool shared;
} cls_find_memory_region_t;

const cls_window_preset_t cls_window_presets[] =
{
  {
    CLS_HOOK_CEMU,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "^wxWindowNR$",
    "^Cemu [1-9].*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^cemu$",
#endif
    "Cemu"
  },

  {
    CLS_HOOK_RYUJINX,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "gdkWindowToplevel", "",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "todo",
#endif
    "Ryujinx"
  },

  {
    CLS_HOOK_TOUCHHLE,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "SDL_app", "",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^touchHLE$",
#endif
    "touchHLE"
  },

  {
    CLS_HOOK_INFUSE,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "GLFW30", "^Infuse .*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^Infuse$",
#endif
    "Infuse"
  },

  {
    CLS_HOOK_KEMULATOR,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "java", "^KEm.*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^java$",
#endif
    "KEmulator"
  },

  {
    CLS_HOOK_XEMU,
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    "", "^xemu.*",
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    "^AppRun$",
#endif
    "xemu"
  },

  {
    CLS_HOOK_GENERIC,
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
  virtual size_t read(void *dest, const cl_memory_region_t *region, cl_addr_t offset, size_t size);
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
  virtual bool getIdentification(cl_game_identifier_t *identifier)
  {
    if (identifier)
      identifier->type = CL_GAMEIDENTIFIER_INVALID;
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

  cl_memory_region_t *regions(void) { return m_MemoryRegions; }

  unsigned regionCount(void) { return m_MemoryRegionCount; }

protected:
  /**
   * Finds memory regions matching the specified parameters.
   * @param buffer A buffer of regions to write into
   * @param buffer_count The maximum number of regions the buffer can hold
   * @param fvmr The parameters by which to find regions
   * @return The number of regions found
   */
  unsigned findRegions(cl_memory_region_t *buffer, const unsigned buffer_count,
                       const cls_find_memory_region_t fvmr);

  /**
   * Launches a file picker dialogue asking the user to choose which content
   * file they are using.
   * @param identifier The identifier struct to enter a filename into
   * @return Whether the identifier info was entered
   */
  bool getIdentificationViaFile(cl_game_identifier_t *identifier);

  bool initViaMemoryRegions(const cls_find_memory_region_t fvmr);

  /**
   * Translates a guest virtual address to a host virtual address by stepping
   * through each registered memory region.
   * @param address The guest virtual address
   * @return The host virtual address
   */
  uintptr_t translate(cl_addr_t address);

  char m_ContentHash[32 + 1];
  cl_memory_t *m_Memory = nullptr;
  cl_memory_region_t m_MemoryRegions[16];
  unsigned m_MemoryRegionCount = 0;
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
