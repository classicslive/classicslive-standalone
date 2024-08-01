#include "cls_process_select.h"

#include <QHeaderView>
#include <QTimer>

ClsProcessSelect::ClsProcessSelect(QWidget *parent) : QWidget(parent)
{
  m_Table = new QTableWidget(this);

  m_Table->setColumnCount(4);
  m_Table->setHorizontalHeaderLabels(QStringList() <<
    "Title" << "PID" << "CPU" << "Memory");
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

void ClsProcessSelect::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  int width = m_Table->width();
  m_Table->horizontalHeader()->resizeSection(CLS_COLUMN_TITLE, width * 0.50);
  m_Table->horizontalHeader()->resizeSection(CLS_COLUMN_PID, width * 0.1667);
  m_Table->horizontalHeader()->resizeSection(CLS_COLUMN_CPU, width * 0.1667);
  m_Table->horizontalHeader()->resizeSection(CLS_COLUMN_MEMORY, width * 0.1667);
}

int ClsProcessSelect::refresh(void)
{
#ifdef WIN32
#error "todo"
#elif defined(__linux__)
  /* Compactly, with no Header, Running, only for current User. */
  const char *cmd = "ps chrux --sort=-%cpu";
  FILE *cmd_file = nullptr;
  char line[256];
  int i = 0;

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
      /* Ignore ourself */
      if (strncmp(process.title, "ps", sizeof(process.title)))
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
      new QTableWidgetItem(QString::number(m_Processes[i].cpu)));
    m_Table->setItem(i, CLS_COLUMN_MEMORY,
      new QTableWidgetItem(QString::number(m_Processes[i].memory)));
  }

  return i;
#endif
}
