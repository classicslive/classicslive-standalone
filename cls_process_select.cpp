#include "cls_hook.h"
#include "cls_process_select.h"

#include <regex>

#include <QHeaderView>
#include <QTimer>

ClsProcessSelect::ClsProcessSelect(QWidget *parent) : QWidget(parent)
{
  m_Table = new QTableWidget(this);

  m_Table->setColumnCount(4);
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
    QTableWidgetItem *item = m_Table->item(row, CLS_COLUMN_PID);
    bool ok = false;
    unsigned pid = item->text().toUInt(&ok, 10);

    if (ok && pid > 0)
      emit selected(pid);
  }
}

int ClsProcessSelect::refresh(void)
{
#ifdef WIN32
#error "todo"
#elif defined(__linux__)
  /**
   * Enumerate all processes Compactly, with no Header, and only for current
   * User. Sort by CPU usage.
   */
  const char *cmd = "ps -e chux --sort=-%cpu";
  FILE *cmd_file = nullptr;
  char line[256];
  int i = 0;

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

    /* Highlight known processes */
    for (unsigned j = 0; j < sizeof(cls_window_presets) / sizeof(cls_window_preset_t) - 1; j++)
      if (std::regex_match(std::string(m_Processes[i].title),
                           std::regex(cls_window_presets[j].process_title)))
        m_Table->itemAt(i, CLS_COLUMN_TITLE)->setBackground(QBrush(QColor(200, 200, 255)));
  }

  return i;
#endif
}
