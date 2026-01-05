#include <QString>
#include <QStringList>

#include "yuzu.h"

static bool get_title_id(cl_game_identifier_t *identifier, const QString &str)
{
  if (!identifier)
    return false;

  QStringList split = str.split('|');
  if (split.size() < 5)
    return false;

  QString title_string = split[3].trimmed();
  int openParen = title_string.lastIndexOf('(');
  if (openParen != -1)
    title_string = title_string.left(openParen).trimmed();

  if (title_string.isEmpty())
    return false;

  snprintf(identifier->product, sizeof(identifier->product), "%s", title_string.toUtf8().constData());
  snprintf(identifier->filename, sizeof(identifier->filename), "%s", title_string.toUtf8().constData());

  QString version_string = split[4].trimmed();
  if (version_string.isEmpty() || static_cast<size_t>(version_string.length()) >= sizeof(identifier->version))
    return false;

  snprintf(identifier->version, sizeof(identifier->version), "%s", version_string.toUtf8().constData());

  return true;
}

bool ClsHookYuzu::getIdentification(cl_game_identifier_t *identifier)
{
  char window_title[256];

  /** @todo what was i doing here? cycle count?
  cl_memory_region_t region = findMemoryRegion({ 0x1A600, 0 });
  */

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

bool ClsHookYuzu::init(void)
{
  /**
   * @todo This may be more in the future.
   * See https://switchbrew.org/wiki/SMC#MemoryMode
   */
  static const cls_find_memory_region_t fmr =
  {
    .host_offset=0,
    .host_size=0x100000000,
    .guest_base=0,
    .guest_size=CL_GB(3), /* Save some space. Usage hovers at ~3GB */
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4,
    .title="Horizon OS Foreground App",
    .shared=true
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookYuzu::run(void)
{
  return true;
}
