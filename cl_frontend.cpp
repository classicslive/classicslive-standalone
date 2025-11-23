#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "cls_hook.h"
#include "cls_hook_cemu.h"
#include "cls_hook_dolphin.h"
#include "cls_hook_infuse.h"
#include "cls_hook_kemulator.h"
#include "cls_hook_ryujinx.h"
#include "cls_hook_touchhle.h"
#include "cls_hook_xemu.h"
#include "cls_hook_yuzu.h"
#include "cls_main.h"
#include "cls_network_manager.h"
#include "cls_process_select.h"
#include "cls_thread.h"

extern "C"
{
  #include <cl_common.h>
  #include <cl_main.h>
  #include <cl_memory.h>
}
#include <cl_frontend.h>

static std::vector<std::unique_ptr<ClsHook>> hooks;
static ClsNetworkManager network_manager;

void cl_fe_display_message(unsigned level, const char *msg)
{
  QMessageBox msg_box;

  msg_box.setText(msg);
  switch (level)
  {
  case CL_MSG_DEBUG:
  case CL_MSG_INFO:
    msg_box.setIcon(QMessageBox::Information);
    break;
  case CL_MSG_WARN:
    msg_box.setIcon(QMessageBox::Warning);
    break;
  case CL_MSG_ERROR:
    msg_box.setIcon(QMessageBox::Critical);
    break;
  default:
    msg_box.setIcon(QMessageBox::Question);
  }
  msg_box.exec();
}

bool cl_fe_install_membanks(void)
{
  if (hooks.size() != 1)
    return false;
  else
  {
    memory.region_count = hooks[0]->regionCount();
    memory.regions = reinterpret_cast<cl_memory_region_t*>(malloc(sizeof(cl_memory_region_t) * memory.region_count));
    memcpy(memory.regions, hooks[0]->regions(), sizeof(cl_memory_region_t) * memory.region_count);

    return true;
  }
}

const char* cl_fe_library_name(void)
{
  if (hooks.size() == 1)
    return hooks[0]->getLibrary();
  else
    return nullptr;
}

unsigned cl_fe_memory_read(cl_memory_t *mem, void *dest, cl_addr_t address,
  unsigned size)
{
  CL_UNUSED(mem);
  if (hooks.size() != 1)
    return false;
  else
  {
    unsigned read = hooks[0]->read(dest, address, size);

    if (read && size <= 8)
      return cl_read(dest,
                     reinterpret_cast<uint8_t*>(dest),
                     0,
                     size,
                     memory.regions[0].endianness);
    else
      return read;
  }
}

unsigned cl_fe_memory_write(cl_memory_t *mem, const void *src, cl_addr_t address,
  unsigned size)
{
  CL_UNUSED(mem);
  if (hooks.size() != 1)
    return false;
  else
  {
    if (size <= 8)
    {
      int64_t temp = 0;
      cl_read(&temp, reinterpret_cast<const uint8_t*>(src), 0, size, memory.regions[0].endianness);

      return hooks[0]->write(&temp, address, size);
    }
    else
      return hooks[0]->write(src, address, size);
  }
}

bool cl_fe_search_deep_copy(cl_search_t *search)
{
  if (hooks.size() != 1)
    return false;
  return hooks[0]->deepCopy(search);
}

void cl_fe_pause(void)
{
  if (hooks.size() == 1)
    hooks[0]->pause();
}

void cl_fe_unpause(void)
{
  if (hooks.size() == 1)
    hooks[0]->unpause();
}

void cl_fe_network_post(const char *url, char *data, cl_network_cb_t callback,
                        void *userdata)
{
  cls_net_cb cb = { callback, userdata };
  emit network_manager.request(url, data, cb);
}

void cl_fe_thread(cl_task_t *cl_task)
{
  auto *thread = new ClsThread(cl_task);
  thread->start();
}

bool cl_fe_user_data(cl_user_t *user, unsigned index)
{
  CL_UNUSED(index);
  snprintf(user->username, sizeof(user->username), "%s", "jacory");
  snprintf(user->password, sizeof(user->password), "%s", "jacory");
  snprintf(user->language, sizeof(user->language), "%s", "en_US");

  return true;
}

ClsMain::ClsMain(void)
{
  m_Timer = new QTimer(this);
  m_Timer->setInterval(16);
  connect(m_Timer, SIGNAL(timeout()), this, SLOT(run()));
  m_Timer->start();

  m_ProcessSelect = new ClsProcessSelect();
  connect(m_ProcessSelect, SIGNAL(selected(uint,void*)),
          this, SLOT(selected(uint,void*)));
  m_ProcessSelect->show();
}

void ClsMain::closeEvent(QCloseEvent *event)
{
  cl_free();
  qApp->quit();
  event->accept();
}

void ClsMain::run(void)
{
  if (hooks.size() == 1 && hooks[0]->run())
    cl_run();
}

std::string getProcessTitle(uint32_t pid)
{
  std::ostringstream path;
  path << "/proc/" << pid << "/comm";

  std::ifstream file(path.str());
  if (!file.is_open())
  {
    return "";
  }

  std::string title;
  std::getline(file, title);
  return title;
}

static std::unique_ptr<ClsHook> createHook(uint pid, void *window)
{
  cls_hook_type type = CLS_HOOK_GENERIC;

  for (const auto& preset : cls_window_presets)
  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    if (preset.window_class &&
        std::regex_match(getWindowClassName(window), std::regex(preset.window_class)) &&
        std::regex_match(getWindowTitle(window), std::regex(preset.window_title)))
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    if (preset.process_title &&
        std::regex_match(getProcessTitle(pid), std::regex(preset.process_title)))
#endif
    {
      type = preset.type;
      break;
    }
  }

  switch (type)
  {
  case CLS_HOOK_CEMU:
    return std::make_unique<ClsHookCemu>(pid, nullptr, window);
  case CLS_HOOK_DOLPHIN:
    return std::make_unique<ClsHookDolphin>(pid, nullptr, window);
  case CLS_HOOK_INFUSE:
    return std::make_unique<ClsHookInfuse>(pid, nullptr, window);
  case CLS_HOOK_KEMULATOR:
    return std::make_unique<ClsHookKemulator>(pid, nullptr, window);
  case CLS_HOOK_RYUJINX:
    return std::make_unique<ClsHookRyujinx>(pid, nullptr, window);
  case CLS_HOOK_TOUCHHLE:
    return std::make_unique<ClsHookTouchhle>(pid, nullptr, window);
  case CLS_HOOK_XEMU:
    return std::make_unique<ClsHookXemu>(pid, nullptr, window);
  case CLS_HOOK_YUZU:
    return std::make_unique<ClsHookYuzu>(pid, nullptr, window);
  default:
    return nullptr;
  }
}

void ClsMain::selected(uint pid, void *window)
{
  auto hook = createHook(pid, window);
  cl_game_identifier_t identifier;

  memset(&identifier, 0, sizeof(identifier));
  if (hook && hook->init() && hook->getIdentification(&identifier))
  {
    hooks.push_back(std::move(hook));
    cl_login_and_start(identifier);
  }
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  ClsMain clsmain;
  clsmain.show();

  return a.exec();
}
