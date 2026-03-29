#ifndef AMJXYPLOTTEST_H
#define AMJXYPLOTTEST_H

#include <QMainWindow>
#include <QTimer>

#include <amjWidgets.H>

QT_BEGIN_NAMESPACE
namespace Ui {
  class amjXYPlotTest;
}
QT_END_NAMESPACE

class amjXYPlotTest: public QMainWindow {
  Q_OBJECT

public:
  amjXYPlotTest(QWidget *parent= nullptr);
  ~amjXYPlotTest();

private:
  Ui::amjXYPlotTest *ui;
  QTimer timer;
  int counter;
  amjWidgets::RateCounter *rateCounter;
  void update_graphs();
};
#endif// AMJXYPLOTTEST_H
