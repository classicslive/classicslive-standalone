#include <QString>
#include <QStringList>
#include <QtEndian>

#include "cls_hook_yuzu.h"

static bool get_title_id(cl_identify_nx_t *ident, const QString &str)
{
  int position = -1;

  QStringList split = str.split('|');

  if (split.count() != 4)
    return false;

  /* Find its matching open parentheses, use to get the title ID */
  int openPosition = split[1].lastIndexOf('(', position);
  if (openPosition == -1)
    return false;
  QString title_string = str.mid(0, openPosition - 1).trimmed();
  if (title_string.isEmpty() ||
      static_cast<size_t>(title_string.length()) >= sizeof(ident->version))
    return false;

  /* Get the version string */
  QString version_string = split[2].trimmed();
  if (version_string.isEmpty() ||
      static_cast<size_t>(version_string.length()) >= sizeof(ident->version))
    return false;

  /* Copy it into the identification struct and set the remainder to 00 */
  memset(ident->version, 0, sizeof(ident->version));
  snprintf(ident->version, sizeof(ident->version), "%s",
           version_string.toUtf8().constData());

  return true;
}

bool ClsHookYuzu::getIdentification(uint8_t **data, unsigned int *size)
{
  char window_title[256];

  cl_memory_region_t region = findMemoryRegion({ 0x1A600, 0 });
  /** @todo */

  if (!getWindowTitle(window_title, sizeof(window_title)))
    return false;
  else if (!get_title_id(&m_Identification, QString(window_title)))
    return false;
  else
  {
    *data = reinterpret_cast<uint8_t*>(&m_Identification);
    *size = sizeof(m_Identification);

    return true;
  }
}

bool ClsHookYuzu::init(void)
{
  if (!ClsHook::init())
    return false;
  else
    return initViaMemoryRegions({ memorySize(), 0 });
}

bool ClsHookYuzu::run(void)
{
  return true;
}
