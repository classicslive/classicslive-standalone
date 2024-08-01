#ifdef WIN32

#include <QString>
#include <QtEndian>

#include "cls_hook_cemu.h"

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
  uint16_t version = version_string.toUShort(&ok, 10);
  if (!ok)
    return false;
  qToBigEndian(version, &ident->version);

  /* Get the title ID by reading between the next brackets */
  int closed_pos = str.lastIndexOf(']', v_pos);
  int open_pos = str.lastIndexOf(' ', closed_pos);
  QString title_id_string = str.mid(open_pos + 1,
                                    closed_pos - open_pos - 1).remove('-');
  if (title_id_string.isEmpty())
    return false;

  /* Copy it into the identification struct and set the remainder to 00 */
  uint64_t title_id = title_id_string.toULongLong(&ok, 16);
  if (!ok)
    return false;
  qToBigEndian(title_id, &ident->title_id);

  return true;
}

uintptr_t GetModuleBaseAddress(unsigned process_id, const wchar_t* module_name)
{
  HANDLE th = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);

  if (th != INVALID_HANDLE_VALUE)
  {
    MODULEENTRY32 module;

    module.dwSize = sizeof(module);
    if (Module32First(th, &module))
    {
      do
      {
        if (!wcsicmp(module.szModule, module_name))
        {
          CloseHandle(th);
          return reinterpret_cast<uintptr_t>(module.modBaseAddr);
        }
      } while (Module32Next(th, &module));
    }
  }
  CloseHandle(th);

  return 0;
}

ClsHookCemu::ClsHookCemu(const cls_window_preset_t *preset)
{
  m_Preset = preset;
}

bool ClsHookCemu::getIdentification(uint8_t **data, unsigned int *size)
{
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
}

bool ClsHookCemu::init()
{
  if (!ClsHook::init())
    return false;

  m_AddressCemuModule = GetModuleBaseAddress(m_ProcessId, L"Cemu.exe");
  if (!m_AddressCemuModule)
    return false;

  if (!ClsHook::read(&m_AddressForegroundApp,
            m_AddressCemuModule + 0x1306438,
            sizeof(m_AddressForegroundApp)) || !m_AddressForegroundApp)
    return false;

  return true;
}

bool ClsHookCemu::run()
{
  if (!m_AddressCemuModule)
    return false;
  ClsHook::read(&m_CycleCount, m_AddressCemuModule + 0x13064EC, sizeof(m_CycleCount));

  return m_CycleCount;
}

size_t ClsHookCemu::read(void *dest, cl_addr_t address, size_t size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::read(dest, address + m_AddressForegroundApp, size);
}

size_t ClsHookCemu::write(const void *src, cl_addr_t address, size_t size)
{
  if (!m_AddressForegroundApp)
    return false;
  else
    return ClsHook::write(src, address + m_AddressForegroundApp, size);
}

bool ClsHookCemu::deepCopy(cl_search_t *search)
{
  if (!search || search->searchbank_count != 1 || !search->searchbanks->bank[0].data)
    return false;

  return read(
    search->searchbanks[0].bank->data + search->searchbanks[0].first_valid,
    search->searchbanks[0].first_valid + 0x10000000,
    search->searchbanks[0].last_valid - search->searchbanks[0].first_valid + search->params.size);
}

#endif
