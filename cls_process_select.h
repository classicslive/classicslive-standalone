#ifndef CLS_PROCESS_SELECT_H
#define CLS_PROCESS_SELECT_H

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

extern "C"
{
  #include <cl_config.h>
};

#define CLS_PROCESS_MAX 256

struct cls_window_preset_t;

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
#define CLS_COLUMN_TITLE 0
#define CLS_COLUMN_PID 1
#define CLS_COLUMN_CPU 2
#define CLS_COLUMN_MEMORY 3
#define CLS_COLUMN_WINDOW 4
#define CLS_COLUMN_SIZE 5
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
#define CLS_COLUMN_TITLE 0
#define CLS_COLUMN_PID 1
#define CLS_COLUMN_CPU 2
#define CLS_COLUMN_MEMORY 3
#define CLS_COLUMN_SIZE 4
#endif

typedef struct
{
  char title[256];
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  unsigned long pid;
  HWND window;
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  pid_t pid;
#endif

  /* Total CPU time used over the process's lifetime, in seconds */
  double cpu_time;

  /* Share of all cores used since the last refresh, as a percentage */
  double cpu;

  /* Resident memory, in bytes */
  unsigned long long memory;

  /* The preset of a supported program, or nullptr */
  const cls_window_preset_t *preset;
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
  void selected(unsigned pid, void *window);

private:
  cls_process_t m_Processes[CLS_PROCESS_MAX];
  int m_ProcessCount = 0;

  /* CPU time of each process at the last refresh, to measure usage between */
  QHash<qint64, double> m_LastCpuTime;
  QElapsedTimer m_CpuTimer;

  QPushButton *m_HookButton = nullptr;
  QTableWidget *m_Table = nullptr;
};

#endif
