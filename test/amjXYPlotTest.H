#ifndef AMJXYPLOTTEST_H
#define AMJXYPLOTTEST_H

#include <QMainWindow>

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
};
#endif// AMJXYPLOTTEST_H
