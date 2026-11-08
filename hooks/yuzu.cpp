#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <cstring>

#include "horizon.h"
#include "yuzu.h"

/* Size of the fastmem arena, which mirrors the whole guest address space */
#define CLS_YUZU_ARENA_SIZE (1ULL << 39)

/* How often the arena's mappings can be reread when an address is missing */
#define CLS_YUZU_MAPPINGS_MS 1000

bool ClsHookYuzu::parseLog(QString &error)
{
  /* "Booting game: 010051F0207B2000 | Tomodachi Life (64-bit) | 1.0.0" */
  static const QRegularExpression reBoot(
    R"(Booting game: ([0-9A-Fa-f]{16}) \| (.*?)(?: \((32|64)-bit\))? \|\s*(.*)$)");
  /* "LoadModule: Module main loaded at 0x80713000 (size 0x559000)" */
  static const QRegularExpression reModule(
    R"(Module (\S+) loaded at 0x([0-9A-Fa-f]+) \(size 0x([0-9A-Fa-f]+)\))");
  static const QRegularExpression reLog("/log/[^/]*_log\\.txt$",
    QRegularExpression::CaseInsensitiveOption);
  QString log_path = findOpenFile(reLog);
  QRegularExpressionMatch boot;
  bool booted = false;

  if (log_path.isEmpty())
  {
    error = "The emulator has no log file open.";
    return false;
  }

  QFile log(log_path);

  if (!log.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    error = QString("Could not open %1: %2").arg(log_path, log.errorString());
    return false;
  }

  /* Modules are logged as they load, just before their game's boot line */
  while (!log.atEnd())
  {
    QByteArray line = log.readLine();

    if (line.contains("Booting game: "))
    {
      QRegularExpressionMatch match = reBoot.match(QString::fromUtf8(line).trimmed());

      if (match.hasMatch())
      {
        boot = match;
        booted = true;
      }
    }
    else if (line.contains("LoadModule: "))
    {
      QRegularExpressionMatch match = reModule.match(QString::fromUtf8(line));

      if (booted)
      {
        m_Modules.clear();
        m_Main = {};
        booted = false;
      }
      if (match.hasMatch())
      {
        Module module = { match.captured(2).toULongLong(nullptr, 16),
                          match.captured(3).toULongLong(nullptr, 16) };

        m_Modules.push_back(module);
        if (match.captured(1) == "main")
          m_Main = module;
      }
    }
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

  /* Untagged games are almost always 64-bit */
  m_PointerSize = boot.captured(3) == "32" ? 4 : 8;

  return true;
}

bool ClsHookYuzu::readMappings(void)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char line[512];
  char map_path[64];
  FILE *map_file;
  uintptr_t backing_size = 0;

  snprintf(map_path, sizeof(map_path), "/proc/%d/maps", m_ProcessId);
  map_file = fopen(map_path, "r");
  if (!map_file)
    return false;

  m_Mappings.clear();
  m_Backing = 0;
  while (fgets(line, sizeof(line), map_file))
  {
    ArenaMapping mapping;
    uintptr_t start, end;
    char flags[5];

    if (!strstr(line, "/memfd:HostMemory") ||
        sscanf(line, "%lx-%lx %4s %lx", &start, &end, flags,
               &mapping.physical) != 4)
      continue;
    mapping.host = start;
    mapping.size = end - start;

    /* The DRAM backing is the one mapping of the whole memfd */
    if (mapping.physical == 0 && mapping.size > backing_size)
    {
      if (m_Backing)
        m_Mappings.push_back({ m_Backing, backing_size, 0 });
      m_Backing = start;
      backing_size = mapping.size;
    }
    else
      m_Mappings.push_back(mapping);
  }
  fclose(map_file);

  std::sort(m_Mappings.begin(), m_Mappings.end(),
    [](const ArenaMapping &a, const ArenaMapping &b) { return a.host < b.host; });
  m_MappingsAge.start();

  return m_Backing != 0;
#else
  return false;
#endif
}

uintptr_t ClsHookYuzu::backingAddress(cl_addr_t address, size_t *available)
{
  if (address >= CLS_HORIZON_MAIN_BASE &&
      address < CLS_HORIZON_MAIN_BASE + m_Main.size)
    address = address - CLS_HORIZON_MAIN_BASE + m_Main.base;
  if (address >= CLS_YUZU_ARENA_SIZE)
    return 0;

  for (int attempt = 0; attempt < 2; attempt++)
  {
    uintptr_t host = m_ArenaBase + address;
    auto it = std::upper_bound(m_Mappings.begin(), m_Mappings.end(), host,
      [](uintptr_t h, const ArenaMapping &m) { return h < m.host; });

    if (it != m_Mappings.begin() && host < (it - 1)->host + (it - 1)->size)
    {
      --it;
      *available = it->host + it->size - host;
      return m_Backing + it->physical + (host - it->host);
    }

    /* The game may have mapped more memory since the mappings were read */
    if (m_MappingsAge.elapsed() < CLS_YUZU_MAPPINGS_MS || !readMappings())
      break;
  }

  return 0;
}

bool ClsHookYuzu::validateArenaBase(uintptr_t base)
{
  m_ArenaBase = base;

  /* Every module has a header, so check them all to rule out shifted bases */
  for (const auto &module : m_Modules)
  {
    uint32_t mod0_offset = 0;
    char magic[4];

    if (read(&mod0_offset, module.base + 4, sizeof(mod0_offset)) !=
          sizeof(mod0_offset) ||
        mod0_offset < 8 || mod0_offset >= module.size ||
        read(magic, module.base + mod0_offset, sizeof(magic)) !=
          sizeof(magic) ||
        memcmp(magic, "MOD0", sizeof(magic)))
    {
      m_ArenaBase = 0;
      return false;
    }
  }

  return true;
}

/**
 * With fastmem, the emulator mirrors the guest address space in a large host
 * reservation, mapping pieces of its DRAM backing in where the game has
 * memory. The pages there get protected while the GPU caches them, so this
 * only uses the arena to translate guest addresses, and accesses the DRAM
 * backing, which is always readable.
 */
bool ClsHookYuzu::initViaArena(void)
{
  if (!m_Main.size || !readMappings())
    return false;

  /* The main module starts a mapping, as its code follows the RW data of rtld */
  std::vector<ArenaMapping> mappings = m_Mappings;

  for (const auto &mapping : mappings)
    if (mapping.host > m_Main.base && validateArenaBase(mapping.host - m_Main.base))
      break;
  if (!m_ArenaBase)
    return false;

  auto add_region = [this](cl_addr_t guest, cl_addr_t size, const char *title)
  {
    if (m_MemoryRegionCount >= sizeof(m_MemoryRegions) / sizeof(m_MemoryRegions[0]))
      return false;

    cl_memory_region_t &region = m_MemoryRegions[m_MemoryRegionCount++];

    memset(&region, 0, sizeof(region));
    region.base_host = reinterpret_cast<void*>(m_ArenaBase + guest);
    region.base_alloc = region.base_host;
    region.base_guest = guest;
    region.size = size;
    region.endianness = CL_ENDIAN_LITTLE;
    region.pointer_length = m_PointerSize;
    region.flags.bits.read = 1;
    region.flags.bits.write = 1;
    snprintf(region.title, sizeof(region.title), "%s", title);

    return true;
  };

  m_MemoryRegionCount = 0;
  add_region(CLS_HORIZON_MAIN_BASE, m_Main.size, "main");
  m_MemoryRegions[0].flags.bits.image = 1;
  m_MemoryRegions[0].flags.bits.pointer_root = 1;
  m_MemoryRegions[0].base_runtime = m_Main.base;

  /**
   * Like on hardware, the only other region is the heap, at its real address.
   * The emulator doesn't say where its heap region is, but the heap is mapped
   * as one block and is almost always the largest run of contiguous mappings.
   */
  cl_addr_t main_end = m_Main.base + m_Main.size;
  cl_addr_t run_start = 0, run_end = 0;
  cl_addr_t heap_start = 0, heap_end = 0;

  auto end_run = [&](void)
  {
    if (run_end - run_start > heap_end - heap_start &&
        (run_end <= m_Main.base || run_start >= main_end))
    {
      heap_start = run_start;
      heap_end = run_end;
    }
  };

  for (const auto &mapping : m_Mappings)
  {
    if (mapping.host < m_ArenaBase ||
        mapping.host + mapping.size > m_ArenaBase + CLS_YUZU_ARENA_SIZE)
      continue;

    cl_addr_t guest = mapping.host - m_ArenaBase;

    if (guest != run_end)
    {
      end_run();
      run_start = guest;
    }
    run_end = guest + mapping.size;
  }
  end_run();

  if (heap_end)
    add_region(heap_start, heap_end - heap_start, "Heap");

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
  if (!ClsHook::init())
    return false;

  m_LogParsed = parseLog(m_LogError);
  if (m_LogParsed && initViaArena())
    return true;

  cl_message(CL_MSG_WARN, "Could not find the emulator's fastmem arena, so "
    "guest physical memory is shown instead. Pointers in it cannot be "
    "followed. Is fastmem enabled in the CPU settings?");

  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  cls_find_memory_region_t fmr_s1 =
  {
    .host_offset=0,
    .host_size=0x100000000, /* 4GB backing */
    .guest_base=0,
    .guest_size=CL_GB(3), /* Save some space. Usage hovers at ~3GB */
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=m_PointerSize,
    .title="Horizon OS Foreground App",
    .shared=true
  };

  cls_find_memory_region_t fmr_s2 =
  {
    .host_offset=0x280000000,
    .host_size=0x500000000, /* 20GB backing */
    .guest_base=0x280000000, /* Offset ahead to where game memory seemed to be appearing */
    .guest_size=CL_GB(8), /* Again, cover ~8GB */
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=m_PointerSize,
    .title="Horizon OS Foreground App (Switch 2)",
    .shared=true
  };

  /* Look for S1 first, then S2 on failure */
  return initViaMemoryRegions(fmr_s1) || initViaMemoryRegions(fmr_s2);
}

bool ClsHookYuzu::run(void)
{
  return true;
}

size_t ClsHookYuzu::read(void *dest, cl_addr_t address, size_t size)
{
  uint8_t *out = reinterpret_cast<uint8_t*>(dest);
  size_t done = 0;

  if (!m_ArenaBase)
    return ClsHook::read(dest, address, size);

  /* Guest memory that is contiguous may be scattered in the backing */
  while (done < size)
  {
    size_t available;
    uintptr_t host = backingAddress(address + done, &available);
    size_t transferred;

    if (!host)
      break;
    available = std::min(available, size - done);
    transferred = readHost(out + done, host, available);
    done += transferred;
    if (transferred != available)
      break;
  }

  return done;
}

size_t ClsHookYuzu::write(const void *src, cl_addr_t address, size_t size)
{
  const uint8_t *in = reinterpret_cast<const uint8_t*>(src);
  size_t done = 0;

  if (!m_ArenaBase)
    return ClsHook::write(src, address, size);

  while (done < size)
  {
    size_t available;
    uintptr_t host = backingAddress(address + done, &available);
    size_t transferred;

    if (!host)
      break;
    available = std::min(available, size - done);
    transferred = writeHost(in + done, host, available);
    done += transferred;
    if (transferred != available)
      break;
  }

  return done;
}
