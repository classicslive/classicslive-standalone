#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>

#include "cls_hook.h"
#include "cls_main.h"
#include "cls_network_manager.h"
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
    cl_membank_t* bank;

    if (hooks.size() != 1)
      return false;

    memory.banks = (cl_membank_t*)malloc(sizeof(cl_membank_t));
    bank = &memory.banks[0];
    bank->data = (uint8_t*)malloc(hooks[0]->memorySize());
    bank->start = 0x10000000;
    bank->size = hooks[0]->memorySize();
    snprintf(bank->title, 256, "%s", cl_fe_library_name());
    memory.bank_count = 1;

    return true;
  }
}

const char* cl_fe_library_name(void)
{
  return "cemu";
}

bool cl_fe_memory_read(cl_memory_t *mem, void *dest, cl_addr_t address, unsigned size)
{
  if (hooks.size() != 1)
    return false;
  else
  {
    if (hooks[0]->read(dest, address, size))
    {
      if (size <= 8)
        return cl_read(dest, reinterpret_cast<uint8_t*>(dest), 0, size, mem->endianness);
    }
    else
      return false;
  }
}

bool cl_fe_memory_write(cl_memory_t *mem, void *src, cl_addr_t address, unsigned size)
{
  if (hooks.size() != 1)
    return false;
  else
  {
    if (size <= 8)
      cl_read(src, reinterpret_cast<uint8_t*>(src), 0, size, mem->endianness);
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
  user->username = "jacory";
  user->password = "jacory";
  user->language = "en_US";

  return true;
}

#include <QApplication>
#include <QFileDialog>

#include <cls_hook_cemu.h>

ClsMain::ClsMain()
{
  timer = new QTimer(this);
  timer->setInterval(16);
  connect(timer, SIGNAL(timeout()), this, SLOT(run()));
  timer->start();
}

void ClsMain::run()
{
  if (hooks.size() == 1 && hooks[0]->run())
    cl_run();
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  ClsHookCemu b(&cls_window_presets[0]);
  b.init();

  QFileDialog dialog;

  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setNameFilter("Wii U executables (*.rpx *.elf)");
  dialog.exec();
  auto filename = dialog.selectedFiles()[0].toStdString();

  session.ready = false;
  hooks.push_back(&b);

  cl_init(nullptr, 0, filename.c_str());

  ClsMain clsmain;
  clsmain.show();

  return a.exec();
}
