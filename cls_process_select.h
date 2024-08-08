#ifndef CLS_PROCESS_SELECT_H
#define CLS_PROCESS_SELECT_H

#include <QMainWindow>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

extern "C"
{
  #include <cl_config.h>
};

#define CLS_PROCESS_MAX 256

#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
#error "todo"
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
#define CLS_COLUMN_TITLE 0
#define CLS_COLUMN_PID 1
#define CLS_COLUMN_CPU 2
#define CLS_COLUMN_MEMORY 3
#endif

typedef struct
{
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
#error "todo"
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  char title[256];
  pid_t pid;
  float cpu;
  unsigned long memory;
#endif
} cls_process_t;

class ClsProcessSelect : public QWidget
{
  Q_OBJECT

public:
  ClsProcessSelect(QWidget *parent = nullptr);

public slots:
  int refresh(void);
  void onHookButtonClicked(void);

signals:
  void selected(unsigned pid);

private:
  cls_process_t m_Processes[CLS_PROCESS_MAX];
  int m_ProcessCount = 0;

  QPushButton *m_HookButton = nullptr;
  QTableWidget *m_Table = nullptr;
};

#endif
