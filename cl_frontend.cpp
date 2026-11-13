#include "cls_hook.h"
#include "hooks/cemu.h"
#include "hooks/citra.h"
#include "hooks/dolphin.h"
#include "hooks/infuse.h"
#include "hooks/kemulator.h"
#include "hooks/ryujinx.h"
#include "hooks/shockwave.h"
#include "hooks/touchhle.h"
#include "hooks/vita3k.h"
#include "hooks/xemu.h"
#include "hooks/xenia.h"
#include "hooks/yuzu.h"
#include "cls_login_dialog.h"
#include "cls_main.h"
#include "cls_network_manager.h"
#include "cls_permissions.h"
#include "cls_process_select.h"
#include "cls_thread.h"

extern "C"
{
  #include <cl_abi.h>
  #include <cl_common.h>
  #include <cl_main.h>
  #include <cl_memory.h>
}

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>

static std::vector<std::unique_ptr<ClsHook>> hooks;
static ClsNetworkManager network_manager;

static cl_error cls_abi_display_message(unsigned level, const char *msg)
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

  return CL_OK;
}

static cl_error cls_abi_install_memory_regions(cl_memory_region_t **regions,
  unsigned *region_count)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    *region_count = hooks[0]->regionCount();
    *regions = reinterpret_cast<cl_memory_region_t*>(
      calloc(*region_count, sizeof(cl_memory_region_t)));
    memcpy(*regions, hooks[0]->regions(),
           sizeof(cl_memory_region_t) * *region_count);

    return CL_OK;
  }
}

static cl_error cls_abi_library_name(const char **name)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    *name = hooks[0]->getLibrary();
    return *name ? CL_OK : CL_ERR_PARAMETER_NULL;
  }
}

static cl_error cls_abi_read_buffer(void *dest, cl_addr_t address,
  unsigned size, unsigned *read)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    unsigned mread = hooks[0]->read(dest, address, size);

    if (read)
      *read = mread;

    return CL_OK;
  }
}

static cl_error cls_abi_read_value(void *dest, cl_addr_t address,
  cl_value_type type)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    hooks[0]->read(dest, address, cl_sizeof_memtype(type));
    return cl_read_value(dest, dest, 0, type, hooks[0]->regions()[0].endianness);
  }
}

static cl_error cls_abi_write_buffer(const void *src, cl_addr_t address,
  unsigned size, unsigned *written)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    unsigned mwritten = hooks[0]->write(src, address, size);

    if (written)
      *written = mwritten;

    return CL_OK;
  }
}

static cl_error cls_abi_write_value(const void *src, cl_addr_t address,
  cl_value_type type)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    cl_read_value(const_cast<void*>(src), src, 0, type, hooks[0]->regions()[0].endianness);
    hooks[0]->write(src, address, cl_sizeof_memtype(type));
    return CL_OK;
  }
}

static cl_error cls_abi_network_post(const char *url, char *data,
  cl_network_cb_t callback, void *userdata)
{
  if (!url || !data)
    return CL_ERR_PARAMETER_NULL;
  else
  {
    cls_net_cb cb = { callback, userdata };
    emit network_manager.request(url, data, cb);
  }

  return CL_OK;
}

static cl_error cls_abi_set_pause(unsigned mode)
{
  if (hooks.size() != 1)
    return CL_ERR_PARAMETER_INVALID;
  else
  {
    if (mode)
      return hooks[0]->pause() ? CL_OK : CL_ERR_PARAMETER_INVALID;
    else
      return hooks[0]->unpause() ? CL_OK : CL_ERR_PARAMETER_INVALID;
  }
}

static cl_error cls_abi_thread(cl_task_t *task)
{
  if (!task)
    return CL_ERR_PARAMETER_NULL;
  else
  {
    auto *thread = new ClsThread(task);
    thread->start();
  }

  return CL_OK;
}

static cl_error cls_abi_user_data(cl_user_t *user, unsigned index)
{
  if (!user)
    return CL_ERR_PARAMETER_NULL;
  else
  {
    ClsLoginDialog login(nullptr);
    QString username, password;
    CL_UNUSED(index);

    memset(user, 0, sizeof(*user));
    login.exec();
    username = login.username();
    password = login.password();

    if (username.isEmpty() || password.isEmpty())
      return CL_ERR_USER_CONFIG;
    else
    {
      snprintf(user->username, sizeof(user->username), "%s",
               username.toStdString().c_str());
      snprintf(user->password, sizeof(user->password), "%s",
               password.toStdString().c_str());
      snprintf(user->language, sizeof(user->language), "%s", "en_US");

      return CL_OK;
    }
  }
}

const cl_abi_t cls_abi
{
  CL_ABI_VERSION,
  {
    {
      cls_abi_display_message,
      cls_abi_install_memory_regions,
      cls_abi_library_name,
      cls_abi_network_post,
      cls_abi_set_pause,
      cls_abi_thread,
      cls_abi_user_data
    },
    {
      cls_abi_read_buffer,
      cls_abi_read_value,
      cls_abi_write_buffer,
      cls_abi_write_value
    }
  }
};

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

  QMenu *settings = menuBar()->addMenu(tr("&Settings"));
  connect(settings->addAction(tr("&Permissions...")), &QAction::triggered, this, [this]()
  {
    ClsPermissionsDialog dialog(this);
    dialog.exec();
  });
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

static std::unique_ptr<ClsHook> createHook(uint pid, void *window)
{
  const cls_window_preset_t *preset = cls_find_preset(pid, window);
  cls_hook_type type = preset ? preset->type : CLS_HOOK_GENERIC;

  switch (type)
  {
  case CLS_HOOK_CEMU:
    return std::make_unique<ClsHookCemu>(pid, nullptr, window);
  case CLS_HOOK_CITRA:
    return std::make_unique<ClsHookCitra>(pid, nullptr, window);
  case CLS_HOOK_DOLPHIN:
    return std::make_unique<ClsHookDolphin>(pid, nullptr, window);
  case CLS_HOOK_INFUSE:
    return std::make_unique<ClsHookInfuse>(pid, nullptr, window);
  case CLS_HOOK_KEMULATOR:
    return std::make_unique<ClsHookKemulator>(pid, nullptr, window);
  case CLS_HOOK_RYUJINX:
    return std::make_unique<ClsHookRyujinx>(pid, nullptr, window);
  case CLS_HOOK_SHOCKWAVE:
    return std::make_unique<ClsHookShockwave>(pid, nullptr, window);
  case CLS_HOOK_TOUCHHLE:
    return std::make_unique<ClsHookTouchhle>(pid, nullptr, window);
  case CLS_HOOK_VITA3K:
    return std::make_unique<ClsHookVita3k>(pid, nullptr, window);
  case CLS_HOOK_XEMU:
    return std::make_unique<ClsHookXemu>(pid, nullptr, window);
  case CLS_HOOK_XENIA:
    return std::make_unique<ClsHookXenia>(pid, nullptr, window);
  case CLS_HOOK_YUZU:
    return std::make_unique<ClsHookYuzu>(pid, nullptr, window);
  default:
    return nullptr;
  }
}

static void startHook(std::unique_ptr<ClsHook> hook)
{
  cl_game_identifier_t identifier;

  memset(&identifier, 0, sizeof(identifier));
  if (hook && hook->init() && hook->getIdentification(&identifier))
  {
    hooks.push_back(std::move(hook));
    cl_login_and_start(identifier);
  }
}

void ClsMain::selected(uint pid, void *window)
{
  /* Make sure the process memory can be read before hooking it */
  if (cls_permission_ensure(pid))
    startHook(createHook(pid, window));
}

void ClsMain::resumeHook(uint pid)
{
  cls_permission_resume(pid, [pid]()
  {
    startHook(createHook(pid, nullptr));
  });
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  ClsMain clsmain;
  clsmain.show();

  cl_abi_register(&cls_abi);

  /* Restarted after being granted memory access; finish hooking */
  QStringList args = a.arguments();
  int resume = args.indexOf(CLS_RESUME_HOOK_ARG);

  if (resume >= 0 && resume + 1 < args.size())
  {
    bool ok;
    uint pid = args[resume + 1].toUInt(&ok);

    if (ok && pid)
      QTimer::singleShot(0, &clsmain, [&clsmain, pid]() { clsmain.resumeHook(pid); });
  }

  return a.exec();
}
