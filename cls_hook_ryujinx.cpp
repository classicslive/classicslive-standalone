#include <QString>

#include "cls_hook_ryujinx.h"

static bool get_title_id(cl_game_identifier_t *identifier, const QString &str)
{
  int count = 0;
  int position = -1;

  if (!identifier)
    return false;

  /* Find the second closed parentheses in the string */
  for (int i = str.length() - 1; i >= 0; i--)
  {
    QChar ch = str[i];

    if (ch == ')')
    {
      count++;
      if (count == 2)
      {
        position = i;
        break;
      }
    }
  }
  if (position == -1)
    return false;

  /* Find its matching open parentheses, use to get the title ID */
  int openPosition = str.lastIndexOf('(', position);
  if (openPosition == -1)
    return false;

  /* Cast the title ID to a 64-bit integer to make sure it's valid and non-zero */
  QString title_id_string = str.mid(openPosition + 1,
                                    position - openPosition - 1);
  bool ok;
  auto title_id = title_id_string.toULongLong(&ok, 16);
  if (!ok || !title_id)
    return false;
  snprintf(identifier->product, sizeof(identifier->product), "%s",
           title_id_string.toUtf8().constData());

  /* Get the version string by reading between the parentheses and the 'v' */
  int v_position = str.lastIndexOf('v', openPosition);
  QString version_string = str.mid(v_position + 1,
                                   openPosition - v_position - 1).trimmed();
  if (version_string.isEmpty() ||
      static_cast<unsigned long long>(version_string.length()) >= sizeof(identifier->version))
    return false;

  /* Copy it into the identification struct and set the remainder to 00 */
  memset(identifier->version, 0, sizeof(identifier->version));
  snprintf(identifier->version, sizeof(identifier->version), "%s",
           version_string.toUtf8().constData());

  return true;
}

bool ClsHookRyujinx::getIdentification(cl_game_identifier_t *identifier)
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

bool ClsHookRyujinx::init(void)
{
  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0,
    .host_size=0xC0000000,
    .guest_base=0,
    .guest_size=0xC0000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookRyujinx::run(void)
{
  return true;
}
