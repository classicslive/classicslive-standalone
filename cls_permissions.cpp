#include "cls_permissions.h"

extern "C"
{
  #include <cl_config.h>
}

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStorageInfo>
#include <QTimer>
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
  /**
   * The kernel checks access to the process before touching the address, so
   * reading address 0 fails with EPERM when access is refused and with EFAULT
   * when it is allowed.
   */
  char byte;
  struct iovec local_iov = { &byte, 1 };
  struct iovec remote_iov = { nullptr, 1 };

  if (process_vm_readv(static_cast<pid_t>(pid), &local_iov, 1, &remote_iov, 1, 0) >= 0)
    return 0;

  return errno == EFAULT ? 0 : errno;
#else
  Q_UNUSED(pid);
  return 0;
#endif
}

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX

static QString cls_executable_path(void)
{
  return QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
}

/* Prefers the system-installed helper, which the polkit policy refers to */
static QString cls_helper_path(void)
{
#ifdef CLS_HELPER_PATH
  if (QFileInfo(CLS_HELPER_PATH).isExecutable())
    return CLS_HELPER_PATH;
#endif
#ifdef CLS_HELPER_SOURCE_PATH
  if (QFileInfo(CLS_HELPER_SOURCE_PATH).isExecutable())
    return CLS_HELPER_SOURCE_PATH;
#endif
  QString beside = QCoreApplication::applicationDirPath() + "/" CLS_HELPER_NAME;

  return QFileInfo(beside).isExecutable() ? beside : QString();
}

static int cls_ptrace_scope(void)
{
  QFile file("/proc/sys/kernel/yama/ptrace_scope");

  if (!file.open(QIODevice::ReadOnly))
    return -1;

  bool ok;
  int scope = file.readAll().trimmed().toInt(&ok);

  return ok ? scope : -1;
}

/* Linux ignores file capabilities on filesystems mounted nosuid */
static bool cls_executable_on_nosuid(void)
{
  struct statvfs info;
  QByteArray path = QFile::encodeName(cls_executable_path());

  return statvfs(path.constData(), &info) == 0 && (info.f_flag & ST_NOSUID);
}

static bool cls_process_has_ptrace_cap(void)
{
  struct __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
  struct __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};

  if (syscall(SYS_capget, &header, data))
    return false;

  return data[CAP_TO_INDEX(CAP_SYS_PTRACE)].effective & CAP_TO_MASK(CAP_SYS_PTRACE);
}

/* Lowering our own capabilities never needs privileges */
static void cls_process_drop_ptrace_cap(void)
{
  struct __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
  struct __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3] = {};
  unsigned index = CAP_TO_INDEX(CAP_SYS_PTRACE);
  unsigned mask = CAP_TO_MASK(CAP_SYS_PTRACE);

  if (syscall(SYS_capget, &header, data))
    return;
  data[index].effective &= ~mask;
  data[index].permitted &= ~mask;
  data[index].inheritable &= ~mask;
  syscall(SYS_capset, &header, data);
}

/* Reads the executable's file capabilities without depending on libcap */
static bool cls_executable_has_ptrace_cap(void)
{
  struct vfs_ns_cap_data caps;
  QByteArray path = QFile::encodeName(cls_executable_path());
  ssize_t size = getxattr(path.constData(), "security.capability", &caps, sizeof(caps));

  /* Every revision starts with magic_etc and the first permitted word */
  if (size < static_cast<ssize_t>(sizeof(caps.magic_etc) + sizeof(caps.data[0])))
    return false;

  return le32toh(caps.data[CAP_TO_INDEX(CAP_SYS_PTRACE)].permitted) &
         CAP_TO_MASK(CAP_SYS_PTRACE);
}

/* Starts the helper as root; the result arrives through the QProcess signals */
static void cls_helper_start(QProcess *process, const QString &action)
{
  process->start("pkexec",
                 QStringList() << cls_helper_path() << action << cls_executable_path());
}

static QString cls_helper_error(QProcess *process, int exit_code)
{
  QString output = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();

  switch (exit_code)
  {
  case CLS_PKEXEC_DISMISSED:
    return QObject::tr("Authentication was cancelled.");
  case CLS_PKEXEC_FAILED:
    return QObject::tr("Authentication failed, or no authentication agent is "
                       "running.%1").arg(output.isEmpty() ? "" : "\n\n" + output);
  default:
    return output.isEmpty() ?
      QObject::tr("The helper failed with exit code %1.").arg(exit_code) : output;
  }
}

static QString cls_process_error(QProcess::ProcessError error)
{
  return error == QProcess::FailedToStart ?
    QObject::tr("pkexec could not be started. Make sure polkit is installed.") :
    QObject::tr("The helper stopped unexpectedly.");
}

static void cls_show_technical_explanation(QWidget *parent)
{
  QMessageBox box(parent);
  int scope = cls_ptrace_scope();

  box.setWindowTitle(QObject::tr("Technical explanation"));
  box.setIcon(QMessageBox::Information);
  box.setTextFormat(Qt::RichText);
  box.setText(QObject::tr(
    "<p>Linux's Yama security module (<code>kernel.yama.ptrace_scope</code>, "
    "currently <b>%1</b> on this system) only lets a program read the memory "
    "of processes it started itself. Games and emulators you launch "
    "separately are not started by " CLS_PROGRAM_NAME ", so the kernel refuses "
    "its reads through <code>process_vm_readv()</code> with <code>EPERM</code>.</p>"

    "<p>The <code>CAP_SYS_PTRACE</code> capability lifts this restriction for a "
    "single program. When you accept, " CLS_PROGRAM_NAME " runs a small helper "
    "as administrator through <code>pkexec</code>, which shows your system's "
    "password prompt. The helper runs one command:</p>"
    "<p><code>setcap cap_sys_ptrace+ep %2</code></p>"

    "<p>Nothing else on your system is changed, and nothing stays running in "
    "the background. " CLS_PROGRAM_NAME " then restarts to pick up the "
    "capability and retries the read.</p>"

    "<p><code>CAP_SYS_PTRACE</code> allows reading the memory of other "
    "processes; " CLS_PROGRAM_NAME " only reads the process you choose to "
    "hook. Updating or reinstalling " CLS_PROGRAM_NAME " replaces the "
    "executable and removes the capability, so you may be asked again.</p>"

    "<p>You can revoke it at any time in Settings &gt; Permissions, or by "
    "running <code>sudo setcap -r %2</code>.</p>")
    .arg(scope >= 0 ? QString::number(scope) : QObject::tr("unknown"))
    .arg(cls_executable_path().toHtmlEscaped()));
  box.exec();
}

/* Shows the exact helper that will be run as root */
static void cls_show_source_code(QWidget *parent)
{
  QDialog dialog(parent);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  QPlainTextEdit *source = new QPlainTextEdit(&dialog);
  QString path = cls_helper_path();
  QFile file(path);
  QLabel *header = new QLabel(&dialog);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

  dialog.setWindowTitle(QObject::tr("Helper source code"));
  header->setTextFormat(Qt::RichText);
  header->setOpenExternalLinks(true);
  header->setWordWrap(true);
  header->setText(QObject::tr("This is the helper that will run as "
    "administrator:<br><code>%1</code><br><a href=\"%2\">View it online</a>")
    .arg(path.isEmpty() ? QObject::tr("(not installed)") : path.toHtmlEscaped(),
         CLS_SOURCE_URL));

  source->setReadOnly(true);
  source->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  source->setLineWrapMode(QPlainTextEdit::NoWrap);
  if (!path.isEmpty() && file.open(QIODevice::ReadOnly))
    source->setPlainText(QString::fromUtf8(file.readAll()));
  else
    source->setPlainText(QObject::tr("The helper could not be found."));

  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(header);
  layout->addWidget(source);
  layout->addWidget(buttons);
  dialog.resize(720, 560);
  dialog.exec();
}

/* The first dialog: asks for approval before anything privileged happens */
static bool cls_request_approval(void)
{
  QDialog dialog;
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  QLabel *text = new QLabel(&dialog);
  QLabel *links = new QLabel(&dialog);
  QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);

  dialog.setWindowTitle(QObject::tr("Permission required"));

  text->setWordWrap(true);
  text->setText(QObject::tr(
    CLS_PROGRAM_NAME " requires a one-time approval to be able to read external "
    "game memory. Without it, " CLS_PROGRAM_NAME " will not function on your "
    "system."));

  links->setTextFormat(Qt::RichText);
  links->setText(QString("<a href=\"explain\">%1</a><br><a href=\"source\">%2</a>")
    .arg(QObject::tr("View technical explanation"), QObject::tr("View source code")));
  QObject::connect(links, &QLabel::linkActivated, &dialog, [&dialog](const QString &link)
  {
    if (link == "explain")
      cls_show_technical_explanation(&dialog);
    else
      cls_show_source_code(&dialog);
  });

  buttons->addButton(QObject::tr("Accept"), QDialogButtonBox::AcceptRole)->setDefault(true);
  buttons->addButton(QObject::tr("Cancel"), QDialogButtonBox::RejectRole);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  layout->addWidget(text);
  layout->addSpacing(8);
  layout->addWidget(links);
  layout->addSpacing(8);
  layout->addWidget(buttons);
  dialog.setMinimumWidth(440);

  return dialog.exec() == QDialog::Accepted;
}

static void cls_show_unfixable(const QString &reason)
{
  QMessageBox box;

  box.setWindowTitle(QObject::tr("Cannot read game memory"));
  box.setIcon(QMessageBox::Warning);
  box.setText(QObject::tr(CLS_PROGRAM_NAME " is not allowed to read the "
                          "memory of this program."));
  box.setInformativeText(reason);
  box.exec();
}

bool cls_permission_ensure(unsigned pid)
{
  int error = cls_permission_probe(pid);

  /* Other errors, like the process having exited, are left to the hook */
  if (error != EPERM)
    return true;
  else if (cls_process_has_ptrace_cap())
  {
    /* Already granted, so something other than Yama is refusing access */
    cls_show_unfixable(QObject::tr(
      "Access was refused even though the permission is already granted. The "
      "program may be running as another user, or be protected by your "
      "system's security policy."));
    return false;
  }
  else if (cls_executable_on_nosuid())
  {
    cls_show_unfixable(QObject::tr(
      "%1 is on a filesystem mounted with the \"nosuid\" option (%2), where "
      "Linux ignores the permission " CLS_PROGRAM_NAME " needs. This includes "
      "AppImages and many external drives.\n\n"
      "Install " CLS_PROGRAM_NAME " on another filesystem, such as your home "
      "folder or /usr/local, and try again.")
      .arg(cls_executable_path(), QStorageInfo(cls_executable_path()).rootPath()));
    return false;
  }
  else if (cls_helper_path().isEmpty())
  {
    cls_show_unfixable(QObject::tr(
      "The " CLS_HELPER_NAME " helper that grants the permission is not "
      "installed. Reinstall " CLS_PROGRAM_NAME " and try again."));
    return false;
  }
  else if (!cls_request_approval())
    return false;
  else
  {
    /* On success this restarts the program and never returns */
    ClsPermissionProgressDialog progress(pid);

    progress.startGrant();
    progress.exec();

    return false;
  }
}

void cls_permission_resume(unsigned pid, const std::function<void()> &proceed)
{
  ClsPermissionProgressDialog progress(pid);

  progress.startRetry();
  progress.exec();
  if (progress.succeeded())
  {
    QMessageBox box;

    box.setWindowTitle(QObject::tr("Additional permission given"));
    box.setIcon(QMessageBox::Information);
    box.setText(QObject::tr("Additional permission given"));
    box.setInformativeText(QObject::tr(
      "The permission clearance succeeded. It can be revoked at any time in "
      "Settings > Permissions.\n\nEnjoy using " CLS_PROGRAM_NAME "!"));
    box.exec();
    proceed();
  }
}

ClsPermissionProgressDialog::ClsPermissionProgressDialog(unsigned pid, QWidget *parent)
  : QDialog(parent), m_Pid(pid)
{
  static const char *titles[StepCount] =
  {
    QT_TR_NOOP("Request authentication"),
    QT_TR_NOOP("Grant capability"),
    QT_TR_NOOP("Retry")
  };
  static const char *descriptions[StepCount] =
  {
    QT_TR_NOOP("Launch your installed privileged helper using pkexec.\n"
               "PolicyKit displays the administrator authentication prompt."),
    QT_TR_NOOP("The helper applies CAP_SYS_PTRACE to the installed "
               "memory-reader executable."),
    QT_TR_NOOP("Retry process_vm_readv() and resume normal operation.")
  };
  QVBoxLayout *layout = new QVBoxLayout(this);
  QGridLayout *steps = new QGridLayout();
  QDialogButtonBox *buttons = new QDialogButtonBox(this);

  setWindowTitle(tr("Granting permission"));
  steps->setColumnStretch(1, 1);
  steps->setHorizontalSpacing(12);
  steps->setVerticalSpacing(14);
  for (int i = 0; i < StepCount; i++)
  {
    QLabel *title = new QLabel(QString("<b>%1</b>").arg(tr(titles[i])), this);
    QLabel *description = new QLabel(tr(descriptions[i]), this);
    QWidget *text = new QWidget(this);
    QVBoxLayout *text_layout = new QVBoxLayout(text);
    QFont icon_font = font();

    description->setWordWrap(true);
    description->setForegroundRole(QPalette::PlaceholderText);
    text_layout->setContentsMargins(0, 0, 0, 0);
    text_layout->setSpacing(2);
    text_layout->addWidget(title);
    text_layout->addWidget(description);

    icon_font.setPointSizeF(icon_font.pointSizeF() * 1.4);
    m_StepIcons[i] = new QLabel(this);
    m_StepIcons[i]->setFont(icon_font);
    m_StepIcons[i]->setFixedWidth(m_StepIcons[i]->fontMetrics().horizontalAdvance("MM"));
    m_StepIcons[i]->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    steps->addWidget(m_StepIcons[i], i, 0);
    steps->addWidget(text, i, 1);
    setStep(static_cast<Step>(i), StatePending);
  }

  m_Error = new QLabel(this);
  m_Error->setWordWrap(true);
  m_Error->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_Error->hide();

  m_CloseButton = buttons->addButton(QDialogButtonBox::Close);
  m_CloseButton->setEnabled(false);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addLayout(steps);
  layout->addWidget(m_Error);
  layout->addStretch();
  layout->addWidget(buttons);
  setMinimumWidth(460);

  m_Helper = new QProcess(this);
  connect(m_Helper, &QProcess::finished, this, &ClsPermissionProgressDialog::onHelperFinished);
  connect(m_Helper, &QProcess::errorOccurred, this, &ClsPermissionProgressDialog::onHelperError);
}

void ClsPermissionProgressDialog::setStep(Step step, State state)
{
  static const char *icons[] = { "○", "●", "✔", "✘" };
  QLabel *icon = m_StepIcons[step];
  QPalette palette = icon->palette();

  icon->setText(icons[state]);
  switch (state)
  {
  case StateRunning:
    palette.setColor(QPalette::WindowText, this->palette().color(QPalette::Highlight));
    break;
  case StateDone:
    palette.setColor(QPalette::WindowText, QColor(0x2e, 0xa0, 0x43));
    break;
  case StateFailed:
    palette.setColor(QPalette::WindowText, QColor(0xd0, 0x3b, 0x3b));
    break;
  default:
    palette.setColor(QPalette::WindowText, this->palette().color(QPalette::PlaceholderText));
  }
  icon->setPalette(palette);
}

void ClsPermissionProgressDialog::fail(Step step, const QString &message)
{
  setStep(step, StateFailed);
  m_Error->setText(message);
  m_Error->show();
  m_CloseButton->setEnabled(true);
  m_CloseButton->setFocus();
}

void ClsPermissionProgressDialog::startGrant(void)
{
  setStep(StepAuthenticate, StateRunning);
  cls_helper_start(m_Helper, "grant");
}

void ClsPermissionProgressDialog::onHelperError(QProcess::ProcessError error)
{
  /* Crashes also emit finished(), which reports them */
  if (error == QProcess::FailedToStart)
    fail(StepAuthenticate, cls_process_error(error));
}

void ClsPermissionProgressDialog::onHelperFinished(int exit_code,
                                                   QProcess::ExitStatus status)
{
  if (status != QProcess::NormalExit)
    fail(StepGrant, cls_process_error(QProcess::Crashed));
  else if (exit_code == CLS_PKEXEC_DISMISSED || exit_code == CLS_PKEXEC_FAILED)
    fail(StepAuthenticate, cls_helper_error(m_Helper, exit_code));
  else
  {
    /* Any other exit code comes from the helper, after authenticating */
    setStep(StepAuthenticate, StateDone);
    if (exit_code != 0)
      fail(StepGrant, cls_helper_error(m_Helper, exit_code));
    else
    {
      setStep(StepGrant, StateDone);
      setStep(StepRetry, StateRunning);
      restart();
    }
  }
}

/**
 * Linux applies file capabilities only when a program is executed, so the
 * program replaces itself with a fresh copy that retries the read.
 */
void ClsPermissionProgressDialog::restart(void)
{
  QByteArray path = QFile::encodeName(cls_executable_path());
  QByteArray pid = QByteArray::number(m_Pid);
  char *argv[] = { path.data(), const_cast<char*>(CLS_RESUME_HOOK_ARG),
                   pid.data(), nullptr };

  QApplication::processEvents();
  execv(path.constData(), argv);

  /* Only reached if the restart failed */
  fail(StepRetry, tr("%1 could not restart itself (%2). Restart it manually "
                     "to finish.").arg(CLS_PROGRAM_NAME, strerror(errno)));
}

void ClsPermissionProgressDialog::startRetry(void)
{
  int error;

  setStep(StepAuthenticate, StateDone);
  setStep(StepGrant, StateDone);
  setStep(StepRetry, StateRunning);

  error = cls_permission_probe(m_Pid);
  if (error == 0)
  {
    setStep(StepRetry, StateDone);
    m_Succeeded = true;

    /* Leave the finished steps up briefly before moving on */
    QTimer::singleShot(600, this, &QDialog::accept);
  }
  else if (error == ESRCH)
    fail(StepRetry, tr("The permission was granted, but the program you were "
                       "hooking has exited. Start it again and hook it."));
  else if (!cls_process_has_ptrace_cap())
    fail(StepRetry, tr("The permission was granted, but did not take effect. "
                       "The filesystem %1 is on may not support it.")
                    .arg(cls_executable_path()));
  else
    fail(StepRetry, tr("Reading game memory still failed: %1.")
                    .arg(strerror(error)));
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

  m_Helper = new QProcess(this);
  connect(m_Helper, &QProcess::finished, this, [this](int exit_code, QProcess::ExitStatus status)
  {
    if (status == QProcess::NormalExit && exit_code == 0)
    {
      /* Takes effect immediately, not just on the next launch */
      cls_process_drop_ptrace_cap();
      QMessageBox::information(this, tr("Permission revoked"),
        tr(CLS_PROGRAM_NAME " can no longer read external game memory. You "
           "will be asked again the next time it is needed."));
    }
    else
      QMessageBox::warning(this, tr("Permission not revoked"),
        status == QProcess::NormalExit ? cls_helper_error(m_Helper, exit_code) :
                                         cls_process_error(QProcess::Crashed));
    refresh();
  });
  connect(m_Helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
  {
    if (error == QProcess::FailedToStart)
    {
      QMessageBox::warning(this, tr("Permission not revoked"), cls_process_error(error));
      refresh();
    }
  });

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
  bool process_cap = cls_process_has_ptrace_cap();
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
  m_RevokeButton->setEnabled((file_cap || process_cap) &&
                             m_Helper->state() == QProcess::NotRunning);
}

void ClsPermissionsDialog::onRevokeClicked(void)
{
  /* Nothing on disk to remove; just drop it from this session */
  if (!cls_executable_has_ptrace_cap())
  {
    cls_process_drop_ptrace_cap();
    refresh();
  }
  else if (cls_helper_path().isEmpty())
    QMessageBox::warning(this, tr("Permission not revoked"),
      tr("The " CLS_HELPER_NAME " helper is not installed. Run this "
         "command instead:\n\nsudo setcap -r %1").arg(cls_executable_path()));
  else
  {
    m_RevokeButton->setEnabled(false);
    cls_helper_start(m_Helper, "revoke");
  }
}

#else /* Other platforms need no additional permissions */

bool cls_permission_ensure(unsigned pid)
{
  Q_UNUSED(pid);
  return true;
}

void cls_permission_resume(unsigned pid, const std::function<void()> &proceed)
{
  Q_UNUSED(pid);
  proceed();
}

ClsPermissionProgressDialog::ClsPermissionProgressDialog(unsigned pid, QWidget *parent)
  : QDialog(parent), m_Pid(pid) {}
void ClsPermissionProgressDialog::startGrant(void) {}
void ClsPermissionProgressDialog::startRetry(void) {}
void ClsPermissionProgressDialog::onHelperFinished(int, QProcess::ExitStatus) {}
void ClsPermissionProgressDialog::onHelperError(QProcess::ProcessError) {}

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

void ClsPermissionsDialog::refresh(void) {}
void ClsPermissionsDialog::onRevokeClicked(void) {}

#endif
