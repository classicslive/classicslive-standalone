#ifndef CLS_MAIN_H
#define CLS_MAIN_H

#include <QMainWindow>
#include <QTimer>

class ClsMain : public QMainWindow
{
  Q_OBJECT

public:
  ClsMain();

public slots:
  void run();

private:
  QTimer *timer;
};

#endif
