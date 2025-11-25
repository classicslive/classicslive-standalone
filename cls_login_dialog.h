#ifndef CLS_LOGIN_DIALOG_H
#define CLS_LOGIN_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>

class ClsLoginDialog : public QDialog
{
  Q_OBJECT

public:
  ClsLoginDialog(QWidget *parent = nullptr);

  QString username(void) const { return m_UsernameLine ? m_UsernameLine->text() : ""; }
  QString password(void) const { return m_PasswordLine ? m_PasswordLine->text() : ""; }

private:
  QLineEdit *m_UsernameLine = nullptr;
  QLineEdit *m_PasswordLine = nullptr;
};

#endif
