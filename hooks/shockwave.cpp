#include "shockwave.h"

#include <string.h>

#include <QFile>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <vector>

/* Director movie formats: protected, compressed (Shockwave), and casts */
static const QRegularExpression cls_shockwave_movie_pattern(
  "\\.(dcr|dxr|dir|cct|cxt|cst)$",
  QRegularExpression::CaseInsensitiveOption);

QString ClsHookShockwave::normalizeUrl(const QString &url)
{
  QUrl parsed(url.trimmed(), QUrl::TolerantMode);
  QString host, path;

  if (!parsed.isValid() || parsed.host().isEmpty() ||
      (parsed.scheme() != "http" && parsed.scheme() != "https"))
    return {};

  host = parsed.host().toLower();
  if (host.startsWith("www."))
    host = host.mid(4);
  path = parsed.path(QUrl::FullyDecoded).toLower().replace('\\', '/');
  if (!path.startsWith('/'))
    path.prepend('/');

  return host + path;
}

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
/* ProcessCommandLineInformation, available since Windows 8.1 */
#define CLS_PROCESS_COMMAND_LINE_INFORMATION 60

typedef NTSTATUS (NTAPI *cls_nt_query_information_process_t)(HANDLE,
  ULONG, PVOID, ULONG, PULONG);

QStringList ClsHookShockwave::arguments(void)
{
  static const auto query = reinterpret_cast<cls_nt_query_information_process_t>(
    GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"));
  QStringList result;
  HANDLE process;
  ULONG size = 0;

  if (!query)
    return {};
  process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, m_ProcessId);
  if (!process)
    return {};
  query(process, CLS_PROCESS_COMMAND_LINE_INFORMATION, nullptr, 0, &size);
  if (size > sizeof(UNICODE_STRING) && size < 0x100000)
  {
    std::vector<uint8_t> buffer(size);
    const auto *line = reinterpret_cast<const UNICODE_STRING*>(buffer.data());

    if (query(process, CLS_PROCESS_COMMAND_LINE_INFORMATION, buffer.data(),
              size, &size) >= 0 && line->Buffer)
    {
      int count = 0;
      std::wstring text(line->Buffer, line->Length / sizeof(wchar_t));
      LPWSTR *args = CommandLineToArgvW(text.c_str(), &count);

      if (args)
      {
        for (int i = 0; i < count; i++)
          result.append(QString::fromWCharArray(args[i]));
        LocalFree(args);
      }
    }
  }
  CloseHandle(process);

  return result;
}
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
QStringList ClsHookShockwave::arguments(void)
{
  QFile file(QString("/proc/%1/cmdline").arg(m_ProcessId));
  QStringList result;

  /* Wine passes the Windows command line through as separate arguments */
  if (file.open(QIODevice::ReadOnly))
    for (const QByteArray &arg : file.readAll().split('\0'))
      if (!arg.isEmpty())
        result.append(QString::fromUtf8(arg));

  return result;
}
#else
QStringList ClsHookShockwave::arguments(void)
{
  return {};
}
#endif

bool ClsHookShockwave::getIdentification(cl_game_identifier_t *identifier)
{
  QStringList args = arguments();

  if (!identifier)
    return false;

  /* Skip the program itself, and the values of the player's own options */
  for (int i = 1; i < args.size(); i++)
  {
    const QString &arg = args[i];
    QString url;

    if (arg.startsWith("--"))
    {
      i++;
      continue;
    }

    url = normalizeUrl(arg);
    if (!url.isEmpty())
    {
      QByteArray utf8 = url.toUtf8();

      if (static_cast<size_t>(utf8.size()) >= sizeof(identifier->product))
      {
        cl_message(CL_MSG_ERROR, "Shockwave movie URL is too long.");
        return false;
      }
      memset(identifier, 0, sizeof(*identifier));
      identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
      snprintf(identifier->product, sizeof(identifier->product), "%s",
               utf8.constData());
      snprintf(identifier->version, sizeof(identifier->version), "%s", "0");
      snprintf(identifier->filename, sizeof(identifier->filename), "%s",
               utf8.constData());
      cl_log("Shockwave movie URL: %s\n", identifier->product);

      return true;
    }
    else if (cls_shockwave_movie_pattern.match(arg).hasMatch())
    {
      QString path = arg;

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
      /* Wine maps the host filesystem root to Z: by default */
      if (path.size() > 2 && path[1] == ':')
        path = path[0].toUpper() == 'Z' ?
          path.mid(2).replace('\\', '/') : QString();
#endif
      if (!path.isEmpty() && QFile::exists(path))
      {
        memset(identifier, 0, sizeof(*identifier));
        identifier->type = CL_GAMEIDENTIFIER_FILE_HASH;
        snprintf(identifier->filename, sizeof(identifier->filename), "%s",
                 path.toUtf8().constData());

        return true;
      }
    }
  }

  /* Projectors embed their movie, or it could be anywhere; ask the user */
  return getIdentificationViaOpenFile(identifier, cls_shockwave_movie_pattern);
}

/* The player is 32-bit, so nothing it uses is above 4GB */
#define CLS_SHOCKWAVE_ADDRESS_START 0x10000ull
#define CLS_SHOCKWAVE_ADDRESS_END 0xFFFF0000ull

std::vector<std::pair<uintptr_t, uintptr_t>> ClsHookShockwave::writableSpans(void)
{
  std::vector<std::pair<uintptr_t, uintptr_t>> spans;

  /* Merges a mapping into the previous span if they touch */
  auto add = [&spans](uintptr_t start, uintptr_t end)
  {
    if (start < CLS_SHOCKWAVE_ADDRESS_START || end > CLS_SHOCKWAVE_ADDRESS_END ||
        start >= end)
      return;
    if (!spans.empty() && spans.back().second == start)
      spans.back().second = end;
    else
      spans.emplace_back(start, end);
  };

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  MEMORY_BASIC_INFORMATION memory;
  uintptr_t address = CLS_SHOCKWAVE_ADDRESS_START;

  while (address < CLS_SHOCKWAVE_ADDRESS_END &&
         VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(address), &memory,
                        sizeof(memory)))
  {
    uintptr_t start = reinterpret_cast<uintptr_t>(memory.BaseAddress);
    DWORD protect = memory.Protect & 0xFF;

    if (memory.State == MEM_COMMIT && !(memory.Protect & PAGE_GUARD) &&
        (protect == PAGE_READWRITE || protect == PAGE_EXECUTE_READWRITE ||
         protect == PAGE_WRITECOPY || protect == PAGE_EXECUTE_WRITECOPY))
      add(start, start + memory.RegionSize);
    address = start + memory.RegionSize;
  }
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char path[64];
  char line[512];
  FILE *file;

  snprintf(path, sizeof(path), "/proc/%u/maps", m_ProcessId);
  file = fopen(path, "r");
  if (!file)
    return spans;
  while (fgets(line, sizeof(line), file))
  {
    unsigned long start, end;
    char flags[5];

    if (sscanf(line, "%lx-%lx %4s", &start, &end, flags) == 3 &&
        flags[0] == 'r' && flags[1] == 'w')
      add(start, end);
  }
  fclose(file);
#endif

  return spans;
}

bool ClsHookShockwave::init(void)
{
  cl_memory_region_t region;

  if (!ClsHook::init())
    return false;

  memset(&region, 0, sizeof(region));
  region.endianness = CL_ENDIAN_LITTLE;
  region.pointer_length = 4;
  region.flags.bits.read = 1;
  region.flags.bits.write = 1;

  /**
   * The player's whole address space, at the same addresses, so any pointer
   * chain resolves. It's mostly unmapped, and searches skip any bucket with
   * a hole in it, so searches ignore it in favor of the regions below.
   */
  region.base_guest = CLS_SHOCKWAVE_ADDRESS_START;
  region.base_host = reinterpret_cast<void*>(
    static_cast<uintptr_t>(CLS_SHOCKWAVE_ADDRESS_START));
  region.size = CLS_SHOCKWAVE_ADDRESS_END - CLS_SHOCKWAVE_ADDRESS_START;
  region.ignore = CL_TRUE;
  snprintf(region.title, sizeof(region.title), "%s",
           "Shockwave Player Address Space");
  m_MemoryRegions[0] = region;
  m_MemoryRegionCount = 1;

  /**
   * The writable memory the player had mapped when it was hooked, for
   * searching. Only the largest spans fit, which cover nearly all of it since
   * the rest are small allocations scattered between modules.
   */
  auto spans = writableSpans();
  const size_t max_spans =
    sizeof(m_MemoryRegions) / sizeof(m_MemoryRegions[0]) - 1;

  if (spans.size() > max_spans)
  {
    std::sort(spans.begin(), spans.end(), [](const auto &a, const auto &b)
      { return a.second - a.first > b.second - b.first; });
    spans.resize(max_spans);
  }
  std::sort(spans.begin(), spans.end());
  region.ignore = CL_FALSE;
  for (const auto &span : spans)
  {
    region.base_guest = span.first;
    region.base_host = reinterpret_cast<void*>(span.first);
    region.size = span.second - span.first;
    snprintf(region.title, sizeof(region.title), "Writable %08lX",
             static_cast<unsigned long>(span.first));
    m_MemoryRegions[m_MemoryRegionCount++] = region;
  }

  return true;
}
