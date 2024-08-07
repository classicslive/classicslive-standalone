#include <QString>
#include <QtEndian>

#include "cls_hook_cemu.h"

ClsHookCemu::ClsHookCemu(unsigned pid, const cls_window_preset_t *preset) :
  ClsHook(pid, preset) {}

static bool get_title_id(cl_identify_cafe_t *ident, const QString &str)
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

  /* Cast the version to a big endian 16-bit integer */
  QString version_string = str.mid(v_pos + 1,
                                   position - v_pos - 1);
  bool ok;
  uint64_t version = version_string.toUShort(&ok, 10);
  if (!ok)
    return false;
  cl_read(&ident->version, (uint8_t*)&version, 0, sizeof(version),
          CL_ENDIAN_BIG);

  /* Get the title ID by reading between the next brackets */
  int closed_pos = str.lastIndexOf(']', v_pos);
  int open_pos = str.lastIndexOf(' ', closed_pos);
  QString title_id_string = str.mid(open_pos + 1,
                                    closed_pos - open_pos - 1).remove('-');
  if (title_id_string.isEmpty())
    return false;

  /* Copy it into the identification struct and set the remainder to 00 */
  uint64_t title_id = title_id_string.toULong(&ok, 16);
  if (!ok)
    return false;
  cl_read(&ident->title_id, (uint8_t*)&title_id, 0, sizeof(title_id),
          CL_ENDIAN_BIG);

  return true;
}

bool ClsHookCemu::getIdentification(uint8_t **data, unsigned int *size)
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

bool ClsHookCemu::init()
{
  return initViaMemoryRegions({0x4E000000, 0x0E000000});
}

bool ClsHookCemu::run()
{
  return true;
}
