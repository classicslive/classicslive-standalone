#include <QRegularExpression>
#include <QString>

#include "cemu.h"

static bool get_title_id(cl_game_identifier_t *identifier, const QString &str)
{
  bool ok = false;

  //
  // 1. Extract the Title ID     [TitleId: 00000-00000]
  //
  int titleStart = str.indexOf("[TitleId:");
  if (titleStart < 0)
    return false;

  int titleEnd = str.indexOf("]", titleStart);
  if (titleEnd < 0)
    return false;

  // Extract "00000-00000"
  QString titleSegment = str.mid(titleStart, titleEnd - titleStart + 1);
  QRegularExpression reTitle(R"(TitleId:\s*([0-9A-Fa-f]+)-([0-9A-Fa-f]+))");
  QRegularExpressionMatch m = reTitle.match(titleSegment);
  if (!m.hasMatch())
    return false;

  QString title_id_string = m.captured(1) + m.captured(2); // remove dash
  uint64_t title_id = title_id_string.toULongLong(&ok, 16);
  if (!ok || !title_id)
    return false;

  snprintf(identifier->product, sizeof(identifier->product),
           "%s", title_id_string.toUtf8().constData());

  //
  // 2. Extract filename (between last "] " and last " [")
  //
  int fnameStart = str.lastIndexOf("] ");
  int fnameEnd   = str.lastIndexOf(" [");

  if (fnameStart < 0 || fnameEnd < 0 || fnameEnd <= fnameStart)
    return false;

  QString filename = str.mid(fnameStart + 2,   // skip "] "
                             fnameEnd - (fnameStart + 2));

  filename = filename.trimmed();

  if (filename.isEmpty())
    return false;

  snprintf(identifier->filename, sizeof(identifier->filename),
           "%s", filename.toUtf8().constData());


  //
  // 3. Extract version string from something like:  [US v16]
  //
  // Look for the final "[...vNN]"
  //
  int versionOpen = str.lastIndexOf("[");
  int versionClose = str.lastIndexOf("]");
  if (versionOpen < 0 || versionClose <= versionOpen)
    return false;

  QString versionPart = str.mid(versionOpen + 1,
                                versionClose - versionOpen - 1);

  // Match "v16" or region + v16
  QRegularExpression reVer(R"(v(\d+))");
  QRegularExpressionMatch mv = reVer.match(versionPart);
  if (!mv.hasMatch())
    return false;

  QString version_number = mv.captured(1); // just the digits

  // Validate it's an integer
  version_number.toUShort(&ok);
  if (!ok)
    return false;

  snprintf(identifier->version, sizeof(identifier->version),
           "%s", version_number.toUtf8().constData());

  return true;
}

bool ClsHookCemu::getIdentification(cl_game_identifier_t *identifier)
{
  char window_title[256];

  if (!getWindowTitle(window_title, sizeof(window_title)))
    return false;
  else if (!get_title_id(identifier, QString(window_title)))
    return false;
  else
  {
    identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
    return true;
  }
}

bool ClsHookCemu::init()
{
  /* One gigabyte of contiguous RAM for the CafeOS foreground app */
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0x0E000000,
    .host_size=0x4E000000,
    .guest_base=0x10000000,
    .guest_size=0x40000000,
    .endianness=CL_ENDIAN_BIG,
    .pointer_size=4,
    .title = "CafeOS Foreground App"
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookCemu::run()
{
  return true;
}
