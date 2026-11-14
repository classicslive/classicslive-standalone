#ifndef CLS_PERMISSIONS_H
#define CLS_PERMISSIONS_H

#include <QDialog>

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
 * and on success shows the confirmation.
 * @param pid The process that was about to be hooked
 * @return Whether hooking can proceed now
 */
bool cls_permission_resume(unsigned pid);

/**
 * Asks for approval before anything privileged happens.
 */
class ClsPermissionRequestDialog : public QDialog
{
  Q_OBJECT

public:
  ClsPermissionRequestDialog(QWidget *parent = nullptr);
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
};

#endif
