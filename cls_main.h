#ifndef CLS_MAIN_H
#define CLS_MAIN_H

#include <QMainWindow>
#include <QTimer>

#include "cls_process_select.h"

class ClsMain : public QMainWindow
{
  Q_OBJECT

public:
  ClsMain(void);

public slots:
  void run(void);
  void selected(uint pid, void *window);

  /**
   * Finishes hooking a process after restarting with newly granted memory
   * access permission.
   */
  void resumeHook(uint pid);

private:
  void closeEvent(QCloseEvent *event) override;

  QTimer *m_Timer = nullptr;
  ClsProcessSelect *m_ProcessSelect = nullptr;
};

#endif
