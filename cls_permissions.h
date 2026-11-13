#ifndef CLS_PERMISSIONS_H
#define CLS_PERMISSIONS_H

#include <QDialog>
#include <QProcess>
#include <QString>

#include <functional>

class QLabel;
class QPushButton;

#define CLS_PROGRAM_NAME "Classics Live"

/**
 * The command line switch the program re-executes itself with after being
 * granted memory access, followed by the PID to resume hooking.
 */
#define CLS_RESUME_HOOK_ARG "--resume-hook"

/**
 * Tests whether the external memory of a process can be read, with a dummy
 * one-byte read.
 * @param pid The process to test
 * @return 0 if memory can be read, otherwise an errno value (EPERM when the
 * kernel refuses access)
 */
int cls_permission_probe(unsigned pid);

/**
 * Makes sure the memory of a process can be read before hooking it, asking
 * for and applying a one-time permission grant if the kernel refuses access.
 * When the grant succeeds, the program re-executes itself to pick up the new
 * capability and does not return.
 * @param pid The process about to be hooked
 * @return Whether hooking can proceed now
 */
bool cls_permission_ensure(unsigned pid);

/**
 * Finishes a grant after the program re-executed itself: retries the read,
 * and on success shows the confirmation and calls the provided function.
 * @param pid The process that was about to be hooked
 * @param proceed Called to resume hooking once the read succeeds
 */
void cls_permission_resume(unsigned pid, const std::function<void()> &proceed);

/**
 * The step-by-step progress of a permission grant.
 */
class ClsPermissionProgressDialog : public QDialog
{
  Q_OBJECT

public:
  enum Step
  {
    StepAuthenticate = 0,
    StepGrant,
    StepRetry,

    StepCount
  };

  ClsPermissionProgressDialog(unsigned pid, QWidget *parent = nullptr);

  /** Requests authentication and grants the capability, then restarts. */
  void startGrant(void);

  /** Retries the read after restarting with the capability. */
  void startRetry(void);

  bool succeeded(void) const { return m_Succeeded; }

private slots:
  void onHelperFinished(int exit_code, QProcess::ExitStatus status);
  void onHelperError(QProcess::ProcessError error);

private:
  enum State
  {
    StatePending = 0,
    StateRunning,
    StateDone,
    StateFailed
  };

  void setStep(Step step, State state);
  void fail(Step step, const QString &message);
  void restart(void);

  unsigned m_Pid;
  bool m_Succeeded = false;
  QProcess *m_Helper = nullptr;
  QLabel *m_StepIcons[StepCount];
  QLabel *m_Error = nullptr;
  QPushButton *m_CloseButton = nullptr;
};

/**
 * Settings > Permissions: shows whether memory access was granted, and lets it
 * be revoked.
 */
class ClsPermissionsDialog : public QDialog
{
  Q_OBJECT

public:
  ClsPermissionsDialog(QWidget *parent = nullptr);

private slots:
  void refresh(void);
  void onRevokeClicked(void);

private:
  QLabel *m_Status = nullptr;
  QPushButton *m_RevokeButton = nullptr;
  QProcess *m_Helper = nullptr;
};

#endif
