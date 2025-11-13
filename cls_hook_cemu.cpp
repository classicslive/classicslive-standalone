#include <QRegExp>
#include <QString>

#include "cls_hook_cemu.h"

static bool get_title_id(cl_game_identifier_t *identifier, const QString &str)
{
  int position = -1;

  /* Find the first closed parentheses in the string */
  for (int i = str.length() - 1; i >= 0; i--)
  {
    QChar ch = str[i];

    if (ch == ']')
    {
      position = i;
      break;
    }
  }
  if (position == -1)
    return false;

  /* Find its matching 'v', use to get the version */
  int v_pos = str.lastIndexOf('v', position);
  if (v_pos == -1)
    return false;

  /* Cast the version to an integer to verify valid */
  QString version_string = str.mid(v_pos + 1,
                                   position - v_pos - 1);
  bool ok;
  version_string.toUShort(&ok, 10);
  if (!ok)
    return false;
  snprintf(identifier->version, sizeof(identifier->version), "%s",
           version_string.toUtf8().constData());

  /* Get the title ID by reading between the next brackets */
  int closed_pos = str.lastIndexOf(']', v_pos);
  int open_pos = str.lastIndexOf(' ', closed_pos);
  QString title_id_string = str.mid(open_pos + 1,
                                    closed_pos - open_pos - 1).remove('-');
  if (title_id_string.isEmpty())
    return false;
  uint64_t title_id = title_id_string.toULongLong(&ok, 16);
  if (!ok || !title_id)
    return false;
  snprintf(identifier->product, sizeof(identifier->product), "%s",
           title_id_string.toUtf8().constData());

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
    .pointer_size=4
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookCemu::run()
{
  return true;
}
