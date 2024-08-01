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
  snprintf(ident->version, sizeof(ident->version),
           version_string.toUtf8().constData());

  return true;
}

bool ClsHookRyujinx::getIdentification(uint8_t **data, unsigned int *size)
{
#if WIN32
  char window_title[256];

  GetWindowTextA(m_Window, window_title, sizeof(window_title));
  if (!get_title_id(&m_Identification, QString(window_title)))
    return false;
  else
  {
    *data = reinterpret_cast<uint8_t*>(&m_Identification);
    *size = sizeof(m_Identification);
    return true;
  }
#else
  return false;
#endif
}

/**
 * @todo Re-confirm this
 */
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

size_t ClsHookRyujinx::read(void *dest, cl_addr_t address, size_t size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::read(dest, address + m_AddressForegroundApp /* - 0x10000*/, size);
}

size_t ClsHookRyujinx::write(const void *src, cl_addr_t address, size_t size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::write(src, address + m_AddressForegroundApp /* - 0x10000*/, size);
}

bool ClsHookRyujinx::deepCopy(cl_search_t *search)
{
  if (!search || search->searchbank_count != 1 || !search->searchbanks->region->base_host)
    return false;

  return read(
    (uint8_t*)search->searchbanks[0].region->base_host + search->searchbanks[0].first_valid,
    search->searchbanks[0].first_valid,
    search->searchbanks[0].last_valid - search->searchbanks[0].first_valid + search->params.size);
}
