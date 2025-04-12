#include "cls_hook.h"
#include "cls_process_select.h"

#include <regex>

#include <QHeaderView>
#include <QTimer>

ClsProcessSelect::ClsProcessSelect(QWidget *parent) : QWidget(parent)
{
  m_Table = new QTableWidget(this);

  m_Table->setColumnCount(CLS_COLUMN_SIZE);
  m_Table->setHorizontalHeaderLabels(QStringList() <<
    "Title" << "PID" << "CPU" << "RAM");
  m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_Table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_Table->verticalHeader()->setVisible(false);

  m_HookButton = new QPushButton("Hook", this);
  connect(m_HookButton, &QPushButton::clicked, this, &ClsProcessSelect::onHookButtonClicked);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(m_Table);
  layout->addWidget(m_HookButton);

  QTimer *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &ClsProcessSelect::refresh);
  timer->start(2000);

  setLayout(layout);
}

void ClsProcessSelect::onHookButtonClicked(void)
{
  int row = m_Table->currentRow();

  if (row >= 0)
  {
    QTableWidgetItem *item_pid = m_Table->item(row, CLS_COLUMN_PID);
    bool ok = false;
    unsigned pid = item_pid->text().toUInt(&ok, 10);

    /* Retrieve the window handle (Windows only) */
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    QTableWidgetItem *item_win = m_Table->item(row, CLS_COLUMN_WINDOW);
    HWND window = nullptr;
    if (item_win)
    {
      QString hwndStr = item_win->text();
      window = reinterpret_cast<HWND>(hwndStr.toULongLong(&ok, 16));
    }
#else
    void *window = nullptr;
#endif

    if (ok && pid > 0)
      emit selected(pid, window);
  }
}

#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
typedef struct
{
  cls_process_t *processes;
  int *process_count;
} cls_windows_cb_t;

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
  auto processes = reinterpret_cast<cls_windows_cb_t*>(lParam);
  auto process = &processes->processes[*processes->process_count];

  /* Filter out invisible windows */
  if (IsWindowVisible(hWnd))
  {
    HANDLE hProcess = nullptr;

    GetWindowThreadProcessId(hWnd, &process->pid);
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process->pid);
    if (hProcess)
    {
      /* Get window title */
      GetWindowTextA(hWnd, process->title, sizeof(process->title));
      process->window = hWnd;

      /* Get memory usage */
      PROCESS_MEMORY_COUNTERS pmc;
      if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        process->memory = pmc.WorkingSetSize;

      /* Get CPU usage */
      FILETIME creationTime, exitTime, kernelTime, userTime;
      if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
      {
        ULARGE_INTEGER ktime, utime;
        ktime.LowPart = kernelTime.dwLowDateTime;
        ktime.HighPart = kernelTime.dwHighDateTime;
        utime.LowPart = userTime.dwLowDateTime;
        utime.HighPart = userTime.dwHighDateTime;

        process->cpu = static_cast<double>(ktime.QuadPart + utime.QuadPart) / 20000.0;
      }
      *processes->process_count += 1;
    }
    CloseHandle(hProcess);
  }

  return TRUE;
}
#endif

int ClsProcessSelect::refresh(void)
{
  int i = 0;
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
  cls_windows_cb_t processes = { m_Processes, &m_ProcessCount };

  m_ProcessCount = 0;
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&processes));
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
  /**
   * Enumerate all processes Compactly, with no Header, and only for current
   * User. Sort by CPU usage.
   */
  const char *cmd = "ps -e chux --sort=-%cpu";
  FILE *cmd_file = nullptr;
  char line[256];

  /* Retain highlighted item */
  if (m_Table->selectedItems().count())
  {
    int row = m_Table->selectedItems()[0]->row();

    snprintf(m_Processes[0].title, sizeof(m_Processes[0].title), "%s",
      m_Table->item(row, CLS_COLUMN_TITLE)->text().toStdString().c_str());
    m_Processes[0].pid =
      m_Table->item(row, CLS_COLUMN_PID)->text().toULong();
    m_Processes[0].cpu =
      m_Table->item(row, CLS_COLUMN_CPU)->text().toFloat();
    m_Processes[0].memory =
      m_Table->item(row, CLS_COLUMN_MEMORY)->text().toULong();
    i++;
  }

  /* Refresh the processes list */
  cmd_file = popen(cmd, "r");
  while (i < CLS_PROCESS_MAX && fgets(line, sizeof(line), cmd_file))
  {
    cls_process_t process;

    if (sscanf(line, "%*s %u %f %*f %lu %*u %*s %*s %*s %*s %s",
               &process.pid,
               &process.cpu,
               &process.memory,
               process.title) == 4)
    {
      /* Ignore ourself, and processes with no memory commit */
      if (strncmp(process.title, "ps", sizeof(process.title)) &&
          strncmp(process.title, "classicslive", strlen("classicslive")) &&
          process.memory && process.cpu)
      {
        m_Processes[i] = process;
        i++;
      }
    }
  }
  m_ProcessCount = i;
#endif

  /* Refresh the UI table */
  m_Table->setRowCount(m_ProcessCount);
  for (i = 0; i < m_ProcessCount; i++)
  {
    m_Table->setItem(i, CLS_COLUMN_TITLE,
      new QTableWidgetItem(m_Processes[i].title));
    m_Table->setItem(i, CLS_COLUMN_PID,
      new QTableWidgetItem(QString::number(m_Processes[i].pid)));
    m_Table->setItem(i, CLS_COLUMN_CPU,
      new QTableWidgetItem(QString::number(m_Processes[i].cpu, 'f', 1)));
    m_Table->setItem(i, CLS_COLUMN_MEMORY,
      new QTableWidgetItem(QString::number(m_Processes[i].memory)));
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    m_Table->setItem(i, CLS_COLUMN_WINDOW,
      new QTableWidgetItem(QString::asprintf("%p", reinterpret_cast<void*>(m_Processes[i].window))));
#endif

    /* Highlight known processes */
    //for (unsigned j = 0; j < sizeof(cls_window_presets) / sizeof(cls_window_preset_t) - 1; j++)
    //  if (std::regex_match(std::string(m_Processes[i].title),
    //                       std::regex(cls_window_presets[j].process_title)))
    //    m_Table->itemAt(i, CLS_COLUMN_TITLE)->setBackground(QBrush(QColor(200, 200, 255)));
  }

  return i;
}
