#include "cls_login_dialog.h"

extern "C"
{
  #include "../cl_types.h"
}

ClsLoginDialog::ClsLoginDialog(QWidget *parent) : QDialog(parent)
{
  cl_user_t dummy;

  setWindowTitle(tr("Login to Classics Live"));

  m_UsernameLine = new QLineEdit(this);
  m_UsernameLine->setMaxLength(sizeof(dummy.username));

  m_PasswordLine = new QLineEdit(this);
  m_PasswordLine->setMaxLength(sizeof(dummy.password));
  m_PasswordLine->setEchoMode(QLineEdit::Password);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->addRow(tr("Username:"), m_UsernameLine);
  formLayout->addRow(tr("Password:"), m_PasswordLine);

  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->addLayout(formLayout);
  mainLayout->addWidget(buttons);
}
