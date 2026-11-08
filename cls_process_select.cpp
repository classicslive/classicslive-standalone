#include "cls_hook.h"
#include "cls_process_select.h"

#include <algorithm>
#include <thread>

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <QHeaderView>
#include <QTimer>

ClsProcessSelect::ClsProcessSelect(QWidget *parent) : QWidget(parent)
{
  m_Table = new QTableWidget(this);

  m_Table->setColumnCount(CLS_COLUMN_SIZE);
  m_Table->setHorizontalHeaderLabels(QStringList() <<
    "Title" << "PID" << "CPU" << "RAM");
  m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_Table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_Table->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
  refresh();
}

void ClsProcessSelect::onHookButtonClicked(void)
{
  int row = m_Table->currentRow();

  if (row >= 0 && row < m_ProcessCount && m_Processes[row].pid > 0)
  {
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
    emit selected(m_Processes[row].pid, m_Processes[row].window);
#else
    emit selected(m_Processes[row].pid, nullptr);
#endif
  }
}

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
typedef struct
{
  cls_process_t *processes;
  int *process_count;
} cls_windows_cb_t;

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
  auto processes = reinterpret_cast<cls_windows_cb_t*>(lParam);
  auto process = &processes->processes[*processes->process_count];

  if (*processes->process_count >= CLS_PROCESS_MAX)
    return FALSE;

  /* Filter out invisible windows */
  if (IsWindowVisible(hWnd))
  {
    HANDLE hProcess = nullptr;

    GetWindowThreadProcessId(hWnd, &process->pid);
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process->pid);
    if (hProcess)
    {
      /* Get window title; filter ourself */
      GetWindowTextA(hWnd, process->title, sizeof(process->title));
      process->window = hWnd;
      if (!strncmp(process->title, "classicslive", strlen("classicslive")))
        goto cleanup;

      /* Get memory usage; filter processes with no memory commit */
      PROCESS_MEMORY_COUNTERS pmc;
      if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)) && pmc.WorkingSetSize)
        process->memory = pmc.WorkingSetSize;
      else
        goto cleanup;

      /* Get CPU usage */
      FILETIME creationTime, exitTime, kernelTime, userTime;
      process->cpu_time = 0;
      if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
      {
        ULARGE_INTEGER ktime, utime;
        ktime.LowPart = kernelTime.dwLowDateTime;
        ktime.HighPart = kernelTime.dwHighDateTime;
        utime.LowPart = userTime.dwLowDateTime;
        utime.HighPart = userTime.dwHighDateTime;

        /* FILETIME counts 100ns intervals */
        process->cpu_time = static_cast<double>(ktime.QuadPart + utime.QuadPart) / 1e7;
      }
      *processes->process_count += 1;
    }
    cleanup:
    CloseHandle(hProcess);
  }

  return TRUE;
}
#endif

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
/* Reads a process's title, CPU time and resident memory from /proc */
static bool cls_read_process(pid_t pid, cls_process_t *process)
{
  static const double ticks = sysconf(_SC_CLK_TCK);
  static const unsigned long long page_size = sysconf(_SC_PAGESIZE);
  unsigned long long utime, stime;
  long rss;
  char path[64];
  char buffer[1024];
  char *open, *close;
  size_t length;
  FILE *file;

  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  file = fopen(path, "r");
  if (!file)
    return false;
  length = fread(buffer, 1, sizeof(buffer) - 1, file);
  fclose(file);
  buffer[length] = '\0';

  /* The title is in parentheses and may itself contain spaces or parentheses */
  open = strchr(buffer, '(');
  close = strrchr(buffer, ')');
  if (!open || !close || close < open)
    return false;
  snprintf(process->title, sizeof(process->title), "%.*s",
           static_cast<int>(close - open - 1), open + 1);

  /* Fields 14, 15 and 24 of proc_pid_stat(5) */
  if (sscanf(close + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu "
             "%*d %*d %*d %*d %*d %*d %*u %*u %ld", &utime, &stime, &rss) != 3)
    return false;

  process->pid = pid;
  process->cpu_time = (utime + stime) / ticks;
  process->memory = rss > 0 ? rss * page_size : 0;
  process->cpu = 0;

  return true;
}
#endif

static QString cls_format_memory(unsigned long long bytes)
{
  if (bytes >= 1000000000ULL)
    return QString("%1 GB").arg(bytes / 1e9, 0, 'f', 2);
  else if (bytes >= 1000000ULL)
    return QString("%1 MB").arg(bytes / 1e6, 0, 'f', 1);
  else
    return QString("%1 KB").arg(bytes / 1e3, 0, 'f', 0);
}

int ClsProcessSelect::refresh(void)
{
  int current_row = m_Table->currentRow();
  cls_process_t current;
  bool has_current = current_row >= 0 && current_row < m_ProcessCount;
  int i;

  /* Remember the highlighted process so it can be found again after sorting */
  if (has_current)
    current = m_Processes[current_row];

#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
  cls_windows_cb_t processes = { m_Processes, &m_ProcessCount };

  m_ProcessCount = 0;
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&processes));
  for (i = 0; i < m_ProcessCount; i++)
    m_Processes[i].preset = cls_find_preset(m_Processes[i].pid, m_Processes[i].window);
#elif CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  DIR *proc = opendir("/proc");
  struct dirent *entry;
  uid_t uid = getuid();
  pid_t self = getpid();

  if (!proc)
    return 0;

  m_ProcessCount = 0;
  while (m_ProcessCount < CLS_PROCESS_MAX && (entry = readdir(proc)))
  {
    cls_process_t process;
    struct stat info;
    char path[64];
    char *end;
    pid_t pid = static_cast<pid_t>(strtol(entry->d_name, &end, 10));

    /* Only numeric entries are processes; only ours can be hooked */
    if (*end || pid <= 0 || pid == self)
      continue;
    snprintf(path, sizeof(path), "/proc/%d", pid);
    if (stat(path, &info) || info.st_uid != uid)
      continue;
    if (!cls_read_process(pid, &process))
      continue;

    process.preset = cls_find_preset(process.pid, nullptr);

    /* Ignore ourself, and kernel threads or other processes with no memory */
    if (strncmp(process.title, "classicslive", strlen("classicslive")) &&
        process.memory && (process.cpu_time > 0 || process.preset))
      m_Processes[m_ProcessCount++] = process;
  }
  closedir(proc);
#endif

  /* Measure CPU usage as the CPU time spent since the last refresh */
  {
    static const double cores = std::max(1u, std::thread::hardware_concurrency());
    double elapsed = m_CpuTimer.isValid() ? m_CpuTimer.restart() / 1000.0 : 0;
    QHash<qint64, double> cpu_times;

    if (!m_CpuTimer.isValid())
      m_CpuTimer.start();
    for (i = 0; i < m_ProcessCount; i++)
    {
      cls_process_t &process = m_Processes[i];
      auto last = m_LastCpuTime.constFind(process.pid);

      process.cpu = 0;
      if (elapsed > 0 && last != m_LastCpuTime.constEnd() && process.cpu_time >= *last)
        process.cpu = (process.cpu_time - *last) / elapsed / cores * 100.0;
      cpu_times.insert(process.pid, process.cpu_time);
    }
    m_LastCpuTime.swap(cpu_times);
  }

#if CL_HOST_PLATFORM == _CL_PLATFORM_LINUX
  /* Busiest first, falling back to lifetime CPU time before usage is known */
  std::stable_sort(m_Processes, m_Processes + m_ProcessCount,
    [](const cls_process_t &a, const cls_process_t &b)
    {
      return a.cpu != b.cpu ? a.cpu > b.cpu : a.cpu_time > b.cpu_time;
    });
#endif

  /* Supported programs go first, otherwise keep the enumeration order */
  std::stable_partition(m_Processes, m_Processes + m_ProcessCount,
    [](const cls_process_t &process) { return process.preset != nullptr; });

  /* Refresh the UI table */
  QColor supported = palette().color(QPalette::Highlight);
  QFont supported_font = m_Table->font();
  int new_row = -1;

  supported.setAlpha(64);
  supported_font.setBold(true);
  m_Table->setRowCount(m_ProcessCount);
  for (i = 0; i < m_ProcessCount; i++)
  {
    const cls_process_t &process = m_Processes[i];
    QTableWidgetItem *items[CLS_COLUMN_SIZE];

    items[CLS_COLUMN_TITLE] = new QTableWidgetItem(process.title);
    items[CLS_COLUMN_PID] = new QTableWidgetItem(QString::number(process.pid));
    items[CLS_COLUMN_CPU] = new QTableWidgetItem(QString("%1%").arg(process.cpu, 0, 'f', 1));
    items[CLS_COLUMN_MEMORY] = new QTableWidgetItem(cls_format_memory(process.memory));
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
    items[CLS_COLUMN_WINDOW] = new QTableWidgetItem(
      QString::asprintf("%p", reinterpret_cast<void*>(process.window)));
#endif

    for (int column = 0; column < CLS_COLUMN_SIZE; column++)
    {
      if (process.preset)
      {
        items[column]->setBackground(supported);
        items[column]->setFont(supported_font);
        items[column]->setToolTip(QString("Supported: %1").arg(process.preset->title));
      }
      m_Table->setItem(i, column, items[column]);
    }

    if (has_current && process.pid == current.pid
#if CL_HOST_PLATFORM == _CL_PLATFORM_WINDOWS
        && process.window == current.window
#endif
        )
      new_row = i;
  }

  if (new_row >= 0)
    m_Table->setCurrentCell(new_row, m_Table->currentColumn() >= 0 ? m_Table->currentColumn() : 0);
  else
    m_Table->clearSelection();

  return m_ProcessCount;
}
