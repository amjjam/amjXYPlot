#include "amjXYPlotTest.H"
#include "ui_amjXYPlotTest.h"

#include <iostream>

amjXYPlotTest::amjXYPlotTest(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::amjXYPlotTest), counter(0),
    rateCounter(new amjWidgets::RateCounter("rate (/s): ", 1000, this)) {
  ui->setupUi(this);
  ui->statusbar->addPermanentWidget(rateCounter);
  ui->plot->addCurve("curve 1");
  ui->plot->addCurve("curve 2");
  ui->plot->addCurve("curve 3");
  ui->plot->addCurve("curve 4");
  ui->plot->addCurve("curve 5");
  ui->plot->addCurve("curve 6");
  ui->plot->setXAxisRange(-100, 100);
  ui->plot->setYAxisRange(0, 1);
  timer.callOnTimeout(this, &amjXYPlotTest::update_graphs);
  timer.start(100);
}

amjXYPlotTest::~amjXYPlotTest() { delete ui; }

void amjXYPlotTest::update_graphs() {
  rateCounter->count();
  std::cout << "update_graphs: " << counter << ", "
            << 50 * cos(counter / 100 * 2 * M_PI) + 2 << std::endl;
  counter++;
  QVector<float> x1, y1, x2, y2, x3, y3, x4, y4, x5, y5, x6, y6;
  x1.resize(201);
  y1.resize(201);
  for(int i= 0; i < 201; i++) {
    x1[i]= i - 100;
    y1[i]= cos((x1[i] + counter) / 100 * 2 * M_PI);
    y1[i]*= y1[i];
  }
  x2.resize(201);
  y2.resize(201);
  for(int i= 0; i < 201; i++) {
    x2[i]= i - 100;
    y2[i]= exp(-(x2[i] + 25) * (x2[i] + 25) / 2
               / (1000 * (cos((double)counter / 100 * 2 * M_PI) + 1.1)));
  }
  x3.resize(201);
  y3.resize(201);
  for(int i= 0; i < 201; i++) {
    x3[i]= i - 100;
    y3[i]= cos((x3[i] + counter) / 50 * 2 * M_PI);
    y3[i]*= y3[i];
  }
  x4.resize(201);
  y4.resize(201);
  for(int i= 0; i < 201; i++) {
    x4[i]= i - 100;
    y4[i]= exp(-(x4[i] - 25) * (x4[i] - 25) / 2
               / (1000 * (cos((double)counter / 100 * 2 * M_PI) + 1.1)));
  }
  x5.resize(201);
  y5.resize(201);
  for(int i= 0; i < 201; i++) {
    x5[i]= i - 100;
    y5[i]= sin((x5[i] + counter) / 50 * 2 * M_PI);
    y5[i]*= y5[i];
  }
  x6.resize(201);
  y6.resize(201);
  for(int i= 0; i < 201; i++) {
    x6[i]= i - 100;
    y6[i]= exp(-(x6[i]) * (x6[i]) / 2
               / (2000 * (cos((double)counter / 100 * 2 * M_PI) + 1.1)));
  }

  ui->plot->setCurveData("curve 1", x1, y1);
  ui->plot->setCurveData("curve 2", x2, y2);
  ui->plot->setCurveData("curve 3", x3, y3);
  ui->plot->setCurveData("curve 4", x4, y4);
  ui->plot->setCurveData("curve 5", x5, y5);
  ui->plot->setCurveData("curve 6", x6, y6);
}
