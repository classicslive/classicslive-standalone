#ifndef CLS_THREAD_H
#define CLS_THREAD_H

#include <QThread>

extern "C"
{
  #include <cl_frontend.h>
}

class ClsThread : public QThread
{
public:
  ClsThread(cl_task_t* task);
  ~ClsThread() override;

protected:
  void run(void) override;

private:
  bool error(void);
  cl_task_t *m_Task;
};

#endif
