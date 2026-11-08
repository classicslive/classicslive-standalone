#include "kemulator.h"

#include <string.h>

#include <vector>

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
#include <elf.h>
#include <stdio.h>
#include <sys/uio.h>
#endif

/* The emulator class whose static fields hold the MIDlet and the current
 * Displayable, among other things */
#define CLS_KEMULATOR_MAIN_CLASS "emulator/Emulator"

bool ClsHookKemulator::getIdentification(cl_game_identifier_t *identifier)
{
  if (identifier)
  {
    identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
    strncpy(identifier->product, "j2me_test", sizeof(identifier->product));
    strncpy(identifier->version, "1", sizeof(identifier->version));
    strncpy(identifier->filename, "j2me_test", sizeof(identifier->product));

    return true;
  }
  else
    return false;
}

bool ClsHookKemulator::readHost(void *dest, uintptr_t address, size_t size)
{
  if (!address)
    return false;
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  SIZE_T bytes_read = 0;

  return ReadProcessMemory(m_Handle, reinterpret_cast<LPCVOID>(address), dest,
                           size, &bytes_read) && bytes_read == size;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  struct iovec local_iov = { dest, size };
  struct iovec remote_iov = { reinterpret_cast<void*>(address), size };

  return process_vm_readv(m_ProcessId, &local_iov, 1, &remote_iov, 1, 0) ==
         static_cast<ssize_t>(size);
#else
  return false;
#endif
}

uintptr_t ClsHookKemulator::readHostPointer(uintptr_t address)
{
  uint64_t value = 0;

  return readHost(&value, address, sizeof(value)) ?
    static_cast<uintptr_t>(value) : 0;
}

std::string ClsHookKemulator::readHostString(uintptr_t address)
{
  std::string result;
  char buffer[64];

  while (address)
  {
    /* Don't read across a page boundary, the next page might be unmapped */
    size_t size = 0x1000 - (address & 0xfff);

    if (size > sizeof(buffer))
      size = sizeof(buffer);
    if (!readHost(buffer, address, size))
      break;

    size_t length = strnlen(buffer, size);

    result.append(buffer, length);
    if (length < size)
      break;
    address += size;
  }

  return result;
}

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
bool ClsHookKemulator::findJvmSymbols(const char **names, uintptr_t *addresses,
  unsigned count)
{
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_ProcessId);
  MODULEENTRY32 module;
  uintptr_t base = 0;

  if (snapshot == INVALID_HANDLE_VALUE)
    return false;
  module.dwSize = sizeof(module);
  if (Module32First(snapshot, &module))
  {
    do
    {
      if (!_tcsicmp(module.szModule, TEXT("jvm.dll")))
      {
        base = reinterpret_cast<uintptr_t>(module.modBaseAddr);
        break;
      }
    } while (Module32Next(snapshot, &module));
  }
  CloseHandle(snapshot);
  if (!base)
    return false;

  /* Walk the export table of the module in the JVM's memory */
  IMAGE_DOS_HEADER dos;
  IMAGE_NT_HEADERS64 nt;
  IMAGE_EXPORT_DIRECTORY exports;

  if (!readHost(&dos, base, sizeof(dos)) ||
      !readHost(&nt, base + dos.e_lfanew, sizeof(nt)))
    return false;

  const IMAGE_DATA_DIRECTORY &dir =
    nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

  if (!dir.VirtualAddress ||
      !readHost(&exports, base + dir.VirtualAddress, sizeof(exports)))
    return false;

  std::vector<DWORD> name_rvas(exports.NumberOfNames);
  std::vector<WORD> ordinals(exports.NumberOfNames);
  std::vector<DWORD> functions(exports.NumberOfFunctions);

  if (!readHost(name_rvas.data(), base + exports.AddressOfNames,
                name_rvas.size() * sizeof(DWORD)) ||
      !readHost(ordinals.data(), base + exports.AddressOfNameOrdinals,
                ordinals.size() * sizeof(WORD)) ||
      !readHost(functions.data(), base + exports.AddressOfFunctions,
                functions.size() * sizeof(DWORD)))
    return false;

  for (unsigned i = 0; i < count; i++)
    addresses[i] = 0;
  for (size_t i = 0; i < name_rvas.size(); i++)
  {
    std::string name = readHostString(base + name_rvas[i]);

    for (unsigned j = 0; j < count; j++)
      if (name == names[j] && ordinals[i] < functions.size())
        addresses[j] = base + functions[ordinals[i]];
  }
  for (unsigned i = 0; i < count; i++)
    if (!addresses[i])
      return false;

  return true;
}
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
bool ClsHookKemulator::findJvmSymbols(const char **names, uintptr_t *addresses,
  unsigned count)
{
  char line[1024];
  char path[1100];
  char lib_path[1024] = { 0 };
  uintptr_t base = 0;
  FILE *file;

  /* Find where the start of libjvm.so is mapped */
  snprintf(path, sizeof(path), "/proc/%u/maps", m_ProcessId);
  file = fopen(path, "r");
  if (!file)
    return false;
  while (fgets(line, sizeof(line), file))
  {
    uintptr_t start, offset;
    int path_pos = 0;

    if (sscanf(line, "%lx-%*x %*4s %lx %*s %*s %n", &start, &offset,
               &path_pos) == 2 && path_pos && offset == 0 &&
        strstr(line + path_pos, "/libjvm.so"))
    {
      base = start;
      snprintf(lib_path, sizeof(lib_path), "%s", line + path_pos);
      lib_path[strcspn(lib_path, "\n")] = '\0';
      break;
    }
  }
  fclose(file);
  if (!base)
    return false;

  /* Read the library's dynamic symbol table from disk, as the JVM sees it */
  snprintf(path, sizeof(path), "/proc/%u/root%s", m_ProcessId, lib_path);
  file = fopen(path, "rb");
  if (!file)
    return false;

  std::vector<char> elf;
  char buffer[0x10000];
  size_t size;

  while ((size = fread(buffer, 1, sizeof(buffer), file)) > 0)
    elf.insert(elf.end(), buffer, buffer + size);
  fclose(file);
  if (elf.size() < sizeof(Elf64_Ehdr))
    return false;

  const auto *ehdr = reinterpret_cast<const Elf64_Ehdr*>(elf.data());

  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
      ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > elf.size() ||
      ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr) > elf.size())
    return false;

  const auto *phdrs = reinterpret_cast<const Elf64_Phdr*>(&elf[ehdr->e_phoff]);
  const auto *shdrs = reinterpret_cast<const Elf64_Shdr*>(&elf[ehdr->e_shoff]);

  /* Symbols are relative to the lowest loaded segment */
  for (unsigned i = 0; i < ehdr->e_phnum; i++)
  {
    if (phdrs[i].p_type == PT_LOAD)
    {
      base -= phdrs[i].p_vaddr & ~static_cast<Elf64_Addr>(0xfff);
      break;
    }
  }

  for (unsigned i = 0; i < count; i++)
    addresses[i] = 0;
  for (unsigned i = 0; i < ehdr->e_shnum; i++)
  {
    const Elf64_Shdr &symtab = shdrs[i];

    if (symtab.sh_type != SHT_DYNSYM || symtab.sh_link >= ehdr->e_shnum)
      continue;

    const Elf64_Shdr &strtab = shdrs[symtab.sh_link];

    if (symtab.sh_offset + symtab.sh_size > elf.size() ||
        strtab.sh_offset + strtab.sh_size > elf.size())
      return false;

    const auto *syms = reinterpret_cast<const Elf64_Sym*>(&elf[symtab.sh_offset]);

    size_t sym_count = symtab.sh_size / sizeof(Elf64_Sym);

    for (size_t j = 0; j < sym_count; j++)
    {
      if (syms[j].st_name >= strtab.sh_size || syms[j].st_shndx == SHN_UNDEF)
        continue;

      const char *sym_name = &elf[strtab.sh_offset + syms[j].st_name];

      for (unsigned k = 0; k < count; k++)
        if (!strncmp(sym_name, names[k], strtab.sh_size - syms[j].st_name))
          addresses[k] = base + syms[j].st_value;
    }
  }
  for (unsigned i = 0; i < count; i++)
    if (!addresses[i])
      return false;

  return true;
}
#else
bool ClsHookKemulator::findJvmSymbols(const char **names, uintptr_t *addresses,
  unsigned count)
{
  CL_UNUSED(names);
  CL_UNUSED(addresses);
  CL_UNUSED(count);
  return false;
}
#endif

bool ClsHookKemulator::loadVmStructs(void)
{
  const char *symbols[] =
  {
    "gHotSpotVMStructs",
    "gHotSpotVMStructEntryArrayStride",
    "gHotSpotVMStructEntryTypeNameOffset",
    "gHotSpotVMStructEntryFieldNameOffset",
    "gHotSpotVMStructEntryIsStaticOffset",
    "gHotSpotVMStructEntryOffsetOffset",
    "gHotSpotVMStructEntryAddressOffset"
  };
  uintptr_t values[7];

  if (!findJvmSymbols(symbols, values, 7))
    return false;
  for (unsigned i = 0; i < 7; i++)
    values[i] = readHostPointer(values[i]);

  uintptr_t entry = values[0];
  uintptr_t stride = values[1];

  if (!entry || !stride)
    return false;

  /* The table ends with an entry that has no type name */
  for (;; entry += stride)
  {
    uintptr_t type_name = readHostPointer(entry + values[2]);
    uintptr_t field_name = readHostPointer(entry + values[3]);
    int32_t is_static = 0;

    if (!type_name)
      break;
    if (!field_name ||
        !readHost(&is_static, entry + values[4], sizeof(is_static)))
      continue;
    m_VmFields[readHostString(type_name) + "::" +
               readHostString(field_name)] =
      readHostPointer(entry + (is_static ? values[6] : values[5]));
  }

  return !m_VmFields.empty();
}

bool ClsHookKemulator::vmField(const char *type, const char *field,
  uintptr_t *value)
{
  auto it = m_VmFields.find(std::string(type) + "::" + field);

  if (it == m_VmFields.end())
    return false;
  *value = it->second;

  return true;
}

bool ClsHookKemulator::findMirrorSlot(const char *class_name,
  uintptr_t *klass_out, uintptr_t *slot)
{
  uintptr_t head, cld_klasses, cld_next, klass_name, klass_next, klass_mirror,
    handle_obj, symbol_length, symbol_body;

  if (!vmField("ClassLoaderDataGraph", "_head", &head) ||
      !vmField("ClassLoaderData", "_klasses", &cld_klasses) ||
      !vmField("ClassLoaderData", "_next", &cld_next) ||
      !vmField("Klass", "_name", &klass_name) ||
      !vmField("Klass", "_next_link", &klass_next) ||
      !vmField("Klass", "_java_mirror", &klass_mirror) ||
      !vmField("OopHandle", "_obj", &handle_obj) ||
      !vmField("Symbol", "_length", &symbol_length) ||
      !vmField("Symbol", "_body", &symbol_body))
    return false;

  size_t name_length = strlen(class_name);
  std::vector<char> name(name_length);

  /* Every class loader keeps a linked list of the classes it loaded */
  for (uintptr_t cld = readHostPointer(head); cld;
       cld = readHostPointer(cld + cld_next))
  {
    for (uintptr_t klass = readHostPointer(cld + cld_klasses); klass;
         klass = readHostPointer(klass + klass_next))
    {
      uintptr_t symbol = readHostPointer(klass + klass_name);
      uint16_t length = 0;

      if (!readHost(&length, symbol + symbol_length, sizeof(length)) ||
          length != name_length ||
          !readHost(name.data(), symbol + symbol_body, name_length) ||
          memcmp(name.data(), class_name, name_length))
        continue;

      /* The handle points to an off-heap slot the GC keeps up to date */
      *slot = readHostPointer(klass + klass_mirror + handle_obj);
      if (klass_out)
        *klass_out = klass;

      return *slot != 0;
    }
  }

  return false;
}

bool ClsHookKemulator::init(void)
{
  cl_memory_region_t region;
  uintptr_t heap_start = 0xe0000000;
  uintptr_t heap_size = 0x20000000;
  uintptr_t statics_size = 0x100;

  if (!ClsHook::init())
    return false;

  memset(&region, 0, sizeof(region));
  region.endianness = CL_ENDIAN_LITTLE;
  region.flags.bits.read = 1;
  region.flags.bits.write = 1;
  region.pointer_length = 4;

  if (loadVmStructs())
  {
    uintptr_t address, offset, base = 0, klass, class_klass, layout_helper;
    int32_t shift = 0, instance_size, static_size;

    /**
     * Java references are stored as 32-bit addresses as long as the heap fits
     * below 4GB, which it does with KEmulator's default 512MB max heap.
     * Otherwise they are compressed and pointer chains can't follow them.
     */
    if (vmField("CompressedOops", "_narrow_oop._base", &address))
      base = readHostPointer(address);
    if (vmField("CompressedOops", "_narrow_oop._shift", &address))
      readHost(&shift, address, sizeof(shift));
    if (base || shift)
    {
      cl_message(CL_MSG_ERROR, "KEmulator's Java heap is too large to be read "
        "by Classics Live. Use a maximum heap size (-Xmx) of 3GB or less.");
      return false;
    }

    /* Get the bounds of the Java heap */
    if (vmField("Universe", "_collectedHeap", &address) &&
        vmField("CollectedHeap", "_reserved", &offset))
    {
      uintptr_t heap = readHostPointer(address) + offset;
      uintptr_t start_offset, size_offset;

      if (vmField("MemRegion", "_start", &start_offset) &&
          vmField("MemRegion", "_word_size", &size_offset))
      {
        uintptr_t start = readHostPointer(heap + start_offset);
        uintptr_t words = readHostPointer(heap + size_offset);

        if (start && words)
        {
          heap_start = start;
          heap_size = words * 8;
        }
      }
    }

    /**
     * Static fields are stored after the instance fields of the class mirror,
     * so they start at the instance size of java.lang.Class.
     */
    if (vmField("Klass", "_layout_helper", &layout_helper) &&
        findMirrorSlot("java/lang/Class", &class_klass, &address) &&
        readHost(&instance_size, class_klass + layout_helper,
                 sizeof(instance_size)) && instance_size > 0)
      m_StaticsOffset = static_cast<uintptr_t>(instance_size) & ~7u;

    if (m_StaticsOffset &&
        findMirrorSlot(CLS_KEMULATOR_MAIN_CLASS, &klass, &m_MirrorSlot) &&
        vmField("InstanceKlass", "_static_field_size", &offset) &&
        readHost(&static_size, klass + offset, sizeof(static_size)) &&
        static_size > 0)
      statics_size = static_cast<uintptr_t>(static_size) * 8;
  }

  region.base_alloc = reinterpret_cast<void*>(heap_start);
  region.base_guest = heap_start;
  region.base_host = reinterpret_cast<void*>(heap_start);
  region.size = heap_size;
  snprintf(region.title, sizeof(region.title), "%s", "Java Heap");
  m_MemoryRegions[0] = region;
  m_MemoryRegionCount = 1;

  if (m_MirrorSlot)
  {
    region.base_alloc = nullptr;
    region.base_guest = CLS_KEMULATOR_STATICS_BASE;
    region.base_host = nullptr;
    region.size = statics_size;
    snprintf(region.title, sizeof(region.title), "%s", "Emulator Statics");
    m_MemoryRegions[1] = region;
    m_MemoryRegionCount = 2;
  }
  else
    cl_message(CL_MSG_WARN, "Could not locate KEmulator's static fields. "
      "Pointers into the Java heap may change whenever memory is cleaned up.");

  return run();
}

bool ClsHookKemulator::run(void)
{
  if (m_MirrorSlot)
  {
    /* The garbage collector can move the mirror, so follow it every frame */
    uintptr_t mirror = readHostPointer(m_MirrorSlot);

    if (mirror)
      m_MemoryRegions[1].base_host =
        reinterpret_cast<void*>(mirror + m_StaticsOffset);
  }

  return ClsHook::run();
}
