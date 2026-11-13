#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QString>

#include <cstdlib>
#include <cstring>

#include "horizon.h"
#include "yuzu.h"

/* Size of the fastmem arena, which mirrors the whole guest address space */
#define CLS_YUZU_ARENA_SIZE (1ULL << 39)

/* How often the arena's mappings can be reread when an address is missing */
#define CLS_YUZU_MAPPINGS_MS 1000

/* Pointer widths a module object may use, for 64-bit and 32-bit games */
#define CLS_YUZU_POINTER_WIDTHS 2
static const unsigned cls_yuzu_pointer_widths[CLS_YUZU_POINTER_WIDTHS] = { 8, 4 };

int ClsHookYuzu::compareMappings(const void *a, const void *b)
{
  uintptr_t host_a = static_cast<const ArenaMapping*>(a)->host;
  uintptr_t host_b = static_cast<const ArenaMapping*>(b)->host;

  return host_a < host_b ? -1 : host_a > host_b;
}

bool ClsHookYuzu::parseLog(QString &error)
{
  /* "Booting game: 010051F0207B2000 | Tomodachi Life (64-bit) | 1.0.0" */
  static const QRegularExpression reBoot(
    R"(Booting game: ([0-9A-Fa-f]{16}) \| (.*?)(?: \((32|64)-bit\))? \|\s*(.*)$)");
  static const QRegularExpression reLog("/log/[^/]*_log\\.txt$",
    QRegularExpression::CaseInsensitiveOption);
  QString log_path = findOpenFile(reLog);
  QRegularExpressionMatch boot;
  QRegularExpressionMatch match;
  QByteArray line;
  QFile log;

  if (log_path.isEmpty())
  {
    error = "The emulator has no log file open.";
    return false;
  }

  log.setFileName(log_path);
  if (!log.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    error = QString("Could not open %1: %2").arg(log_path, log.errorString());
    return false;
  }

  /* The last boot line is the running game */
  while (!log.atEnd())
  {
    line = log.readLine();
    if (!line.contains("Booting game: "))
      continue;
    match = reBoot.match(QString::fromUtf8(line).trimmed());
    if (match.hasMatch())
      boot = match;
  }
  if (!boot.hasMatch())
  {
    error = QString("%1 has no \"Booting game:\" line. The emulator may be "
                    "filtering out Info-level log messages.").arg(log_path);
    return false;
  }

  m_Identifier.type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
  snprintf(m_Identifier.product, sizeof(m_Identifier.product), "%s",
           boot.captured(1).toUpper().toUtf8().constData());
  snprintf(m_Identifier.filename, sizeof(m_Identifier.filename), "%s",
           boot.captured(2).toUtf8().constData());
  snprintf(m_Identifier.version, sizeof(m_Identifier.version), "%s",
           boot.captured(4).trimmed().toUtf8().constData());

  return true;
}

bool ClsHookYuzu::readMappings(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char line[512];
  char map_path[64];
  char flags[5];
  FILE *map_file;
  ArenaMapping mapping;
  ArenaMapping old_backing;
  uintptr_t start, end;
  uintptr_t backing_size = 0;

  snprintf(map_path, sizeof(map_path), "/proc/%d/maps", m_ProcessId);
  map_file = fopen(map_path, "r");
  if (!map_file)
    return false;

  m_Mappings.clear();
  m_Backing = 0;
  while (fgets(line, sizeof(line), map_file))
  {
    if (!strstr(line, "/memfd:HostMemory") ||
        sscanf(line, "%lx-%lx %4s %lx", &start, &end, flags,
               &mapping.physical) != 4)
      continue;
    mapping.host = start;
    mapping.size = end - start;
    mapping.readonly = !strcmp(flags, "r--s");

    /* The DRAM backing is the one mapping of the whole memfd */
    if (mapping.physical == 0 && mapping.size > backing_size)
    {
      if (m_Backing)
      {
        old_backing.host = m_Backing;
        old_backing.size = backing_size;
        old_backing.physical = 0;
        old_backing.readonly = false;
        m_Mappings.push_back(old_backing);
      }
      m_Backing = start;
      backing_size = mapping.size;
    }
    else
      m_Mappings.push_back(mapping);
  }
  fclose(map_file);

  qsort(m_Mappings.data(), m_Mappings.size(), sizeof(ArenaMapping),
        compareMappings);
  m_MappingsAge.start();

  return m_Backing != 0;
#else
  return false;
#endif
}

uintptr_t ClsHookYuzu::backingAddress(cl_addr_t address, size_t *available)
{
  uintptr_t host;

  if (address >= CLS_HORIZON_MAIN_BASE &&
      address < CLS_HORIZON_MAIN_BASE + m_Main.size)
    address = address - CLS_HORIZON_MAIN_BASE + m_Main.base;
  if (address >= CLS_YUZU_ARENA_SIZE)
    return 0;

  host = arenaBackingAddress(m_ArenaBase + address, available);

  /* The game may have mapped more memory since the mappings were read */
  if (!host && m_MappingsAge.elapsed() >= CLS_YUZU_MAPPINGS_MS && readMappings())
    host = arenaBackingAddress(m_ArenaBase + address, available);

  return host;
}

uintptr_t ClsHookYuzu::arenaBackingAddress(uintptr_t host, size_t *available)
{
  const ArenaMapping *mapping;
  size_t low = 0;
  size_t high = m_Mappings.size();
  size_t middle;

  /* Find the last mapping starting at or before the address */
  while (low < high)
  {
    middle = (low + high) / 2;
    if (host < m_Mappings[middle].host)
      high = middle;
    else
      low = middle + 1;
  }
  if (low == 0)
    return 0;

  mapping = &m_Mappings[low - 1];
  if (host >= mapping->host + mapping->size)
    return 0;
  *available = mapping->host + mapping->size - host;

  return m_Backing + mapping->physical + (host - mapping->host);
}

bool ClsHookYuzu::readArena(void *dest, uintptr_t host, size_t size)
{
  size_t available;
  uintptr_t backing = arenaBackingAddress(host, &available);

  return backing && available >= size && readHost(dest, backing, size) == size;
}

bool ClsHookYuzu::findModules(void)
{
  const ArenaMapping *mapping;
  const Module *rtld = nullptr;
  cls_horizon_mod0_t mod0;
  uint8_t object[0x80];
  uint64_t candidate, dynamic;
  uintptr_t mod0_host;
  uintptr_t arena_base = 0;
  uint32_t mod0_offset;
  unsigned pointer_size = 0;
  unsigned width = 0;
  unsigned i, j, k;
  Module module;

  m_Modules.clear();
  m_Main = {};
  m_ArenaBase = 0;

  /* Every module starts with code, which the arena maps read-only */
  for (i = 0; i < m_Mappings.size(); i++)
  {
    mapping = &m_Mappings[i];

    /* The header may be in read-only data, past the mapping of the code */
    if (!mapping->readonly ||
        !readArena(&mod0_offset, mapping->host + 4, sizeof(mod0_offset)) ||
        mod0_offset < 8)
      continue;
    mod0_host = mapping->host + mod0_offset;
    if (!readArena(&mod0, mod0_host, sizeof(mod0)) ||
        memcmp(mod0.magic, "MOD0", sizeof(mod0.magic)) || mod0.bss_end <= 0 ||
        !readArena(object, mod0_host + mod0.module_object, sizeof(object)))
      continue;

    /* Find the module object's base address, followed by its dynamic pointer */
    module.base = 0;
    for (j = 0; !module.base && j < CLS_YUZU_POINTER_WIDTHS; j++)
    {
      width = cls_yuzu_pointer_widths[j];
      for (k = 0; !module.base && (k + 2) * width <= sizeof(object); k++)
      {
        candidate = 0;
        dynamic = 0;
        memcpy(&candidate, object + k * width, width);
        memcpy(&dynamic, object + (k + 1) * width, width);
        if (candidate && !(candidate & 0xFFF) &&
            candidate < CLS_YUZU_ARENA_SIZE && candidate <= mapping->host &&
            dynamic == candidate + mod0_offset + mod0.dynamic)
          module.base = candidate;
      }
    }
    if (!module.base)
      continue;

    /* Every module must agree on where the arena is */
    if (arena_base && arena_base != mapping->host - module.base)
      return false;
    arena_base = mapping->host - module.base;
    pointer_size = width;

    /* The image ends with its bss, rounded up to a page */
    module.size = (mod0_offset + mod0.bss_end + 0xFFFULL) & ~0xFFFULL;
    module.object = module.base + mod0_offset + mod0.module_object;
    module.links[0] = 0;
    module.links[1] = 0;
    memcpy(&module.links[0], object, width);
    memcpy(&module.links[1], object + width, width);
    m_Modules.push_back(module);
  }

  /* rtld's module list head is in its own image, and rtld links to main */
  for (i = 0; i < m_Modules.size(); i++)
  {
    if (m_Modules[i].links[0] < m_Modules[i].base ||
        m_Modules[i].links[0] >= m_Modules[i].base + m_Modules[i].size ||
        m_Modules[i].links[0] == m_Modules[i].object)
      continue;
    else if (rtld)
      return false;
    rtld = &m_Modules[i];
  }
  if (!rtld)
    return false;
  for (i = 0; i < m_Modules.size() && !m_Main.size; i++)
    if (&m_Modules[i] != rtld && m_Modules[i].object == rtld->links[1])
      m_Main = m_Modules[i];
  if (!m_Main.size)
    return false;

  m_ArenaBase = arena_base;
  m_PointerSize = pointer_size;

  return true;
}

cl_memory_region_t *ClsHookYuzu::addRegion(cl_addr_t guest, cl_addr_t size,
                                           const char *title)
{
  cl_memory_region_t *region;

  if (m_MemoryRegionCount >= sizeof(m_MemoryRegions) / sizeof(m_MemoryRegions[0]))
    return nullptr;

  region = &m_MemoryRegions[m_MemoryRegionCount++];
  memset(region, 0, sizeof(*region));
  region->base_host = reinterpret_cast<void*>(m_ArenaBase + guest);
  region->base_alloc = region->base_host;
  region->base_guest = guest;
  region->size = size;
  region->endianness = CL_ENDIAN_LITTLE;
  region->pointer_length = m_PointerSize;
  region->flags.bits.read = 1;
  region->flags.bits.write = 1;
  snprintf(region->title, sizeof(region->title), "%s", title);

  return region;
}

bool ClsHookYuzu::initViaArena(void)
{
  const ArenaMapping *mapping = nullptr;
  cl_memory_region_t *main_region;
  cl_addr_t main_end;
  cl_addr_t guest = 0;
  cl_addr_t run_start = 0, run_end = 0;
  cl_addr_t heap_start = 0, heap_end = 0;
  size_t i;

  if (!findModules())
    return false;

  m_MemoryRegionCount = 0;
  main_region = addRegion(CLS_HORIZON_MAIN_BASE, m_Main.size, "main");
  main_region->flags.bits.image = 1;
  main_region->flags.bits.pointer_root = 1;
  main_region->base_runtime = m_Main.base;

  /* The heap is the largest run of contiguous mappings */
  main_end = m_Main.base + m_Main.size;
  for (i = 0; i <= m_Mappings.size(); i++)
  {
    if (i < m_Mappings.size())
    {
      mapping = &m_Mappings[i];
      if (mapping->host < m_ArenaBase ||
          mapping->host + mapping->size > m_ArenaBase + CLS_YUZU_ARENA_SIZE)
        continue;
      guest = mapping->host - m_ArenaBase;
      if (guest == run_end)
      {
        run_end = guest + mapping->size;
        continue;
      }
    }

    /* A run ended; keep it if it is the largest outside of main */
    if (run_end - run_start > heap_end - heap_start &&
        (run_end <= m_Main.base || run_start >= main_end))
    {
      heap_start = run_start;
      heap_end = run_end;
    }
    if (i < m_Mappings.size())
    {
      run_start = guest;
      run_end = guest + mapping->size;
    }
  }

  if (heap_end)
    addRegion(heap_start, heap_end - heap_start, "Heap");

  return true;
}

bool ClsHookYuzu::getIdentification(cl_game_identifier_t *identifier)
{
  if (!identifier)
    return false;
  else if (m_LogParsed)
  {
    *identifier = m_Identifier;
    return true;
  }

  QMessageBox::critical(nullptr, "Could not identify game",
    QString("Could not identify the game running in the emulator.\n\n%1")
      .arg(m_LogError));

  return false;
}

bool ClsHookYuzu::init(void)
{
  const char *error = nullptr;

  if (!ClsHook::init())
    return false;

  m_LogParsed = parseLog(m_LogError);
  if (!readMappings())
    error = "Could not find the emulator's guest memory. Is fastmem enabled "
            "in the CPU settings?";
  else if (!initViaArena())
    error = "Could not find the game's main module. Hook the emulator again "
            "once the game has finished booting.";

  if (error)
    QMessageBox::critical(nullptr, "Could not read game memory", error);

  return !error;
}

bool ClsHookYuzu::run(void)
{
  return true;
}

size_t ClsHookYuzu::transfer(void *buffer, cl_addr_t address, size_t size,
                             bool write)
{
  uint8_t *bytes = static_cast<uint8_t*>(buffer);
  uintptr_t host;
  size_t available;
  size_t transferred;
  size_t done = 0;

  if (!m_ArenaBase)
    return 0;

  /* Guest memory that is contiguous may be scattered in the backing */
  while (done < size)
  {
    host = backingAddress(address + done, &available);
    if (!host)
      break;
    if (available > size - done)
      available = size - done;
    transferred = write ? writeHost(bytes + done, host, available) :
                          readHost(bytes + done, host, available);
    done += transferred;
    if (transferred != available)
      break;
  }

  return done;
}

size_t ClsHookYuzu::read(void *dest, cl_addr_t address, size_t size)
{
  return transfer(dest, address, size, false);
}

size_t ClsHookYuzu::write(const void *src, cl_addr_t address, size_t size)
{
  return transfer(const_cast<void*>(src), address, size, true);
}
