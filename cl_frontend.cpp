#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>

#include "cls_hook.h"
#include "cls_hook_cemu.h"
#include "cls_hook_ryujinx.h"
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

static std::vector<ClsHook*> hooks;
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
    cl_memory_region_t* region;

    memory.regions = (cl_memory_region_t*)calloc(1, sizeof(cl_memory_region_t));
    region = &memory.regions[0];
    region->base_host = (uint8_t*)malloc(hooks[0]->memorySize());
    region->base_guest = 0;
    region->size = hooks[0]->memorySize();
    snprintf(region->title, sizeof(region->title), "%s", cl_fe_library_name());
    memory.region_count = 1;

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

void cl_fe_network_post(const char *url, char *data, void(*callback)(cl_network_response_t))
{
  cls_net_cb cb = { callback };
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
  connect(m_ProcessSelect, SIGNAL(selected(uint, void*)),
          this, SLOT(selected(uint, void*)));
  m_ProcessSelect->show();
}

void ClsMain::run(void)
{
  if (hooks.size() == 1 && hooks[0]->run())
    cl_run();
}

void ClsMain::selected(uint pid, void *window)
{
  ClsHookRyujinx *hook = new ClsHookRyujinx(pid, nullptr, window);
  uint8_t *data;
  unsigned size;

  if (hook->init() && hook->getIdentification(&data, &size))
  {
    hooks.push_back(hook);
    cl_init(data, size, "");
  }
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  ClsMain clsmain;
  clsmain.show();

  return a.exec();
}
