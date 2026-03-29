#include "amjXYPlotTest.H"
#include "ui_amjXYPlotTest.h"

amjXYPlotTest::amjXYPlotTest(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::amjXYPlotTest) {
  ui->setupUi(this);
}

amjXYPlotTest::~amjXYPlotTest() { delete ui; }
