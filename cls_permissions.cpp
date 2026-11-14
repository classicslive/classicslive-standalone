#include "cls_permissions.h"

extern "C"
{
  #include <cl_config.h>
}

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStorageInfo>
#include <QVBoxLayout>

#include <cerrno>
#include <cstring>

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
#include <endian.h>
#include <linux/capability.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/xattr.h>
#include <unistd.h>
#endif

#define CLS_HELPER_NAME "classicslive-ptrace-helper"
#define CLS_SOURCE_URL \
  "https://github.com/classicslive/classicslive-standalone/blob/master/tools/" \
  CLS_HELPER_NAME

/* pkexec's own exit codes, see pkexec(1) */
#define CLS_PKEXEC_DISMISSED 126
#define CLS_PKEXEC_FAILED 127

int cls_permission_probe(unsigned pid)
{
#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  char byte;
  struct iovec local_iov = { &byte, 1 };
  struct iovec remote_iov = { nullptr, 1 };

  /* Access is checked before the address, so EFAULT means access is allowed */
  if (process_vm_readv(static_cast<pid_t>(pid), &local_iov, 1, &remote_iov, 1, 0) >= 0)
    return 0;

  return errno == EFAULT ? 0 : errno;
#else
  Q_UNUSED(pid);
  return 0;
#endif
}

static void cls_permission_warn(const QString &title, const QString &text,
                                const QString &details)
{
  QMessageBox box;

  box.setWindowTitle(title);
  box.setIcon(QMessageBox::Warning);
  box.setText(text);
  box.setInformativeText(details);
  box.exec();
}

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX

static QString cls_executable_path(void)
{
  return QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
}

/* Prefers the system-installed helper, which the polkit policy refers to */
static QString cls_helper_path(void)
{
  QString beside = QCoreApplication::applicationDirPath() + "/" CLS_HELPER_NAME;

#ifdef CLS_HELPER_PATH
  if (QFileInfo(CLS_HELPER_PATH).isExecutable())
    return CLS_HELPER_PATH;
#endif
#ifdef CLS_HELPER_SOURCE_PATH
  if (QFileInfo(CLS_HELPER_SOURCE_PATH).isExecutable())
    return CLS_HELPER_SOURCE_PATH;
#endif

  return QFileInfo(beside).isExecutable() ? beside : QString();
}

static int cls_ptrace_scope(void)
{
  QFile file("/proc/sys/kernel/yama/ptrace_scope");
  bool ok = false;
  int scope;

  if (!file.open(QIODevice::ReadOnly))
    return -1;
  scope = file.readAll().trimmed().toInt(&ok);

  return ok ? scope : -1;
}

/* Linux ignores file capabilities on filesystems mounted nosuid */
static bool cls_executable_on_nosuid(void)
{
  QByteArray path = QFile::encodeName(cls_executable_path());
  struct statvfs info;

  return statvfs(path.constData(), &info) == 0 && (info.f_flag & ST_NOSUID);
}

/* Reads, and optionally clears, CAP_SYS_PTRACE of this process */
static bool cls_process_ptrace_cap(bool drop)
{
  struct __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
  struct __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3];
  unsigned index = CAP_TO_INDEX(CAP_SYS_PTRACE);
  unsigned mask = CAP_TO_MASK(CAP_SYS_PTRACE);
  bool has;

  memset(data, 0, sizeof(data));
  if (syscall(SYS_capget, &header, data))
    return false;
  has = data[index].effective & mask;

  /* Lowering our own capabilities never needs privileges */
  if (drop && has)
  {
    data[index].effective &= ~mask;
    data[index].permitted &= ~mask;
    data[index].inheritable &= ~mask;
    syscall(SYS_capset, &header, data);
  }

  return has;
}

/* Reads the executable's file capabilities without depending on libcap */
static bool cls_executable_has_ptrace_cap(void)
{
  QByteArray path = QFile::encodeName(cls_executable_path());
  struct vfs_ns_cap_data caps;
  ssize_t size;

  size = getxattr(path.constData(), "security.capability", &caps, sizeof(caps));
  if (size < static_cast<ssize_t>(sizeof(caps.magic_etc) + sizeof(caps.data[0])))
    return false;

  return le32toh(caps.data[CAP_TO_INDEX(CAP_SYS_PTRACE)].permitted) &
         CAP_TO_MASK(CAP_SYS_PTRACE);
}

/**
 * Runs the helper as root through pkexec, waiting for it to finish.
 * @param action "grant" or "revoke"
 * @return An error message, or an empty string on success
 */
static QString cls_helper_run(const char *action)
{
  QProcess process;
  QString output;

  process.start("pkexec", QStringList() << cls_helper_path() << action <<
                cls_executable_path());
  if (!process.waitForStarted(-1))
    return QObject::tr("pkexec could not be started. Make sure polkit is installed.");
  process.waitForFinished(-1);
  if (process.exitStatus() != QProcess::NormalExit)
    return QObject::tr("The helper stopped unexpectedly.");

  output = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
  if (process.exitCode() == 0)
    return QString();
  else if (process.exitCode() == CLS_PKEXEC_DISMISSED)
    return QObject::tr("Authentication was cancelled.");
  else if (process.exitCode() == CLS_PKEXEC_FAILED)
    return QObject::tr("Authentication failed, or no authentication agent is "
                       "running.\n\n%1").arg(output);
  else if (output.isEmpty())
    return QObject::tr("The helper failed with exit code %1.").arg(process.exitCode());

  return output;
}

ClsPermissionRequestDialog::ClsPermissionRequestDialog(QWidget *parent) : QDialog(parent)
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  QLabel *text = new QLabel(this);
  QLabel *links = new QLabel(this);
  QDialogButtonBox *buttons = new QDialogButtonBox(this);

  setWindowTitle(tr("Permission required"));

  text->setWordWrap(true);
  text->setText(tr(
    CLS_PROGRAM_NAME " requires a one-time approval to be able to read external "
    "game memory. Without it, " CLS_PROGRAM_NAME " will not function on your "
    "system."));

  links->setTextFormat(Qt::RichText);
  links->setOpenExternalLinks(true);
  links->setText(QString("<a href=\"" CLS_SOURCE_URL "\">%1</a>")
    .arg(tr("View source code")));

  buttons->addButton(tr("Accept"), QDialogButtonBox::AcceptRole)->setDefault(true);
  buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(text);
  layout->addSpacing(8);
  layout->addWidget(links);
  layout->addSpacing(8);
  layout->addWidget(buttons);
  setMinimumWidth(440);
}

bool cls_permission_ensure(unsigned pid)
{
  ClsPermissionRequestDialog request;
  QByteArray path;
  QByteArray pid_arg;
  QString error;
  char *argv[4];

  /* Other errors, like the process having exited, are left to the hook */
  if (cls_permission_probe(pid) != EPERM)
    return true;
  else if (cls_process_ptrace_cap(false))
    error = QObject::tr("Access was refused even though the permission is "
      "already granted. The program may be running as another user, or be "
      "protected by your system's security policy.");
  else if (cls_executable_on_nosuid())
    error = QObject::tr("%1 is on a filesystem mounted with the \"nosuid\" "
      "option (%2), where Linux ignores the permission " CLS_PROGRAM_NAME
      " needs. This includes AppImages and many external drives.\n\n"
      "Install " CLS_PROGRAM_NAME " on another filesystem, such as your home "
      "folder or /usr/local, and try again.")
      .arg(cls_executable_path(), QStorageInfo(cls_executable_path()).rootPath());
  else if (cls_helper_path().isEmpty())
    error = QObject::tr("The " CLS_HELPER_NAME " helper that grants the "
      "permission is not installed. Reinstall " CLS_PROGRAM_NAME " and try again.");
  else if (request.exec() != QDialog::Accepted)
    return false;
  else
    error = cls_helper_run("grant");

  if (!error.isEmpty())
  {
    cls_permission_warn(QObject::tr("Cannot read game memory"),
      QObject::tr(CLS_PROGRAM_NAME " is not allowed to read the memory of "
                  "this program."), error);
    return false;
  }

  /* Capabilities apply on exec, so restart to pick it up and retry the read */
  path = QFile::encodeName(cls_executable_path());
  pid_arg = QByteArray::number(pid);
  argv[0] = path.data();
  argv[1] = const_cast<char*>(CLS_RESUME_HOOK_ARG);
  argv[2] = pid_arg.data();
  argv[3] = nullptr;
  execv(path.constData(), argv);

  cls_permission_warn(QObject::tr("Restart required"),
    QObject::tr("The permission was granted."),
    QObject::tr("Restart " CLS_PROGRAM_NAME " to finish (%1).").arg(strerror(errno)));

  return false;
}

bool cls_permission_resume(unsigned pid)
{
  QMessageBox box;
  QString error;
  int result = cls_permission_probe(pid);

  if (result == ESRCH)
    error = QObject::tr("The program you were hooking has exited. Start it "
                        "again and hook it.");
  else if (result && !cls_process_ptrace_cap(false))
    error = QObject::tr("The permission did not take effect. The filesystem "
                        "%1 is on may not support it.").arg(cls_executable_path());
  else if (result)
    error = QObject::tr("Reading game memory still failed: %1.").arg(strerror(result));

  if (!error.isEmpty())
  {
    cls_permission_warn(QObject::tr("Cannot read game memory"),
                        QObject::tr("The permission was granted, but hooking "
                                    "could not continue."), error);
    return false;
  }

  box.setWindowTitle(QObject::tr("Additional permission given"));
  box.setIcon(QMessageBox::Information);
  box.setText(QObject::tr("Additional permission given"));
  box.setInformativeText(QObject::tr(
    "The permission clearance succeeded. It can be revoked at any time in "
    "Settings > Permissions.\n\nEnjoy using " CLS_PROGRAM_NAME "!"));
  box.exec();

  return true;
}

ClsPermissionsDialog::ClsPermissionsDialog(QWidget *parent) : QDialog(parent)
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  QLabel *heading = new QLabel(QString("<b>%1</b>").arg(tr("Read external game memory")), this);

  setWindowTitle(tr("Permissions"));

  m_Status = new QLabel(this);
  m_Status->setWordWrap(true);
  m_Status->setTextFormat(Qt::RichText);
  m_Status->setTextInteractionFlags(Qt::TextSelectableByMouse);

  m_RevokeButton = buttons->addButton(tr("Revoke"), QDialogButtonBox::ActionRole);
  connect(m_RevokeButton, &QPushButton::clicked, this, &ClsPermissionsDialog::onRevokeClicked);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(heading);
  layout->addWidget(m_Status);
  layout->addStretch();
  layout->addWidget(buttons);
  setMinimumWidth(460);
  refresh();
}

void ClsPermissionsDialog::refresh(void)
{
  bool file_cap = cls_executable_has_ptrace_cap();
  bool process_cap = cls_process_ptrace_cap(false);
  int scope = cls_ptrace_scope();
  QString status;

  if (file_cap || process_cap)
    status = tr("<p>Granted. " CLS_PROGRAM_NAME " has the CAP_SYS_PTRACE "
                "capability, which lets it read the memory of games and "
                "emulators.</p>");
  else if (scope <= 0)
    status = tr("<p>Not needed. Your system already allows " CLS_PROGRAM_NAME
                " to read game memory.</p>");
  else
    status = tr("<p>Not granted. You will be asked for approval the next time "
                "you hook a program.</p>");

  status += tr("<p><small>Executable: %1<br>kernel.yama.ptrace_scope: %2<br>"
               "Active in this session: %3</small></p>")
    .arg(cls_executable_path().toHtmlEscaped(),
         scope >= 0 ? QString::number(scope) : tr("not present"),
         process_cap ? tr("yes") : tr("no"));

  m_Status->setText(status);
  m_RevokeButton->setEnabled(file_cap || process_cap);
}

void ClsPermissionsDialog::onRevokeClicked(void)
{
  QString error;

  /* With nothing on disk to remove, just drop it from this session */
  if (cls_executable_has_ptrace_cap())
  {
    if (cls_helper_path().isEmpty())
      error = tr("The " CLS_HELPER_NAME " helper is not installed. Run this "
                 "command instead:\n\nsudo setcap -r %1").arg(cls_executable_path());
    else
      error = cls_helper_run("revoke");
  }

  if (error.isEmpty())
  {
    /* Takes effect immediately, not just on the next launch */
    cls_process_ptrace_cap(true);
    QMessageBox::information(this, tr("Permission revoked"),
      tr(CLS_PROGRAM_NAME " can no longer read external game memory. You "
         "will be asked again the next time it is needed."));
  }
  else
    cls_permission_warn(tr("Permission not revoked"),
                        tr("The permission could not be revoked."), error);
  refresh();
}

#else /* Other platforms need no additional permissions */

bool cls_permission_ensure(unsigned pid)
{
  Q_UNUSED(pid);
  return true;
}

bool cls_permission_resume(unsigned pid)
{
  Q_UNUSED(pid);
  return true;
}

ClsPermissionRequestDialog::ClsPermissionRequestDialog(QWidget *parent) : QDialog(parent)
{
}

ClsPermissionsDialog::ClsPermissionsDialog(QWidget *parent) : QDialog(parent)
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

  setWindowTitle(tr("Permissions"));
  layout->addWidget(new QLabel(tr(CLS_PROGRAM_NAME " needs no additional "
                                  "permissions on this system."), this));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ClsPermissionsDialog::refresh(void)
{
}

void ClsPermissionsDialog::onRevokeClicked(void)
{
}

#endif
