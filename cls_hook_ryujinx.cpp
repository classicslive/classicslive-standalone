#include <QString>
#include <QtEndian>

#include "cls_hook_ryujinx.h"

ClsHookRyujinx::ClsHookRyujinx(unsigned pid, const cls_window_preset_t *preset) : ClsHook(pid, preset) {}

static bool get_title_id(cl_identify_nx_t *ident, const QString &str)
{
  int count = 0;
  int position = -1;

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

  /* Cast the title ID to a 64-bit integer */
  QString title_id_string = str.mid(openPosition + 1,
                                    position - openPosition - 1);
  bool ok;
  auto title_id = title_id_string.toULongLong(&ok, 16);
  if (!ok || !title_id)
    return false;
  ident->title_id = title_id;

  /* Get the version string by reading between the parentheses and the 'v' */
  int v_position = str.lastIndexOf('v', openPosition);
  QString version_string = str.mid(v_position + 1,
                                   openPosition - v_position - 1).trimmed();
  if (version_string.isEmpty() ||
      static_cast<unsigned long long>(version_string.length()) >= sizeof(ident->version))
    return false;

  /* Copy it into the identification struct and set the remainder to 00 */
  memset(ident->version, 0, sizeof(ident->version));
  snprintf(ident->version, sizeof(ident->version), "%s",
           version_string.toUtf8().constData());

  return true;
}

bool ClsHookRyujinx::getIdentification(uint8_t **data, unsigned int *size)
{
  char window_title[256];

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

bool ClsHookRyujinx::init(void)
{
  if (!ClsHook::init())
    return false;
  else
    return initViaMemoryRegions({ memorySize(), 0 });
}

bool ClsHookRyujinx::run(void)
{
  return true;
}
