#include "XYPlot.H"
#include <qcustomplot.h>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>

using namespace amjWidgets;

XYPlot::XYPlot(QWidget* parent)
    : QWidget(parent), m_settings("amjWidgets","XYPlot")
{
    setupUI();
    connect(&m_timer, &QTimer::timeout, this, &XYPlot::replotCoalesced);
    m_timer.start(33);
}

void XYPlot::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QPushButton* cfgBtn = new QPushButton("Config", this);
    layout->addWidget(cfgBtn);
    connect(cfgBtn, &QPushButton::clicked, this, &XYPlot::openConfigDialog);

    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    rebuildPanels();

    m_crosshairH = new QCPItemLine(m_plot);
    m_crosshairV = new QCPItemLine(m_plot);
    m_crosshairText = new QCPItemText(m_plot);

    m_crosshairH->setVisible(false);
    m_crosshairV->setVisible(false);
    m_crosshairText->setVisible(false);
}

void XYPlot::openConfigDialog()
{
    QDialog dlg(this);
    QFormLayout layout(&dlg);

    QSpinBox panelSpin;
    panelSpin.setValue(m_panelCount);

    QCheckBox sharedX;
    sharedX.setChecked(m_sharedXAxis);

    layout.addRow("Panels", &panelSpin);
    layout.addRow("Shared X", &sharedX);

    if(dlg.exec())
    {
        setPanelCount(panelSpin.value());
        setSharedXAxis(sharedX.isChecked());
    }
}

void XYPlot::rebuildPanels()
{
    m_plot->plotLayout()->clear();
    m_panels.clear();

    QCPLayoutGrid* grid = new QCPLayoutGrid;
    m_plot->plotLayout()->addElement(0,0,grid);

    for(int i=0;i<m_panelCount;i++)
    {
        QCPAxisRect* rect = new QCPAxisRect(m_plot);
        grid->addElement(i,0,rect);
        m_panels.append(rect);
    }

    reassignGraphs();
}

void XYPlot::reassignGraphs()
{
    for(auto it=m_curves.begin(); it!=m_curves.end(); ++it)
    {
        int p = it.value().panel;
        if(p >= m_panels.size()) p = 0;

        auto* rect = m_panels[p];
        it.value().graph->setKeyAxis(rect->axis(QCPAxis::atBottom));
        it.value().graph->setValueAxis(rect->axis(QCPAxis::atLeft));
    }
}

void XYPlot::setPanelCount(int n)
{
    m_panelCount = n;
    rebuildPanels();
}

void XYPlot::setCurvePanel(const QString& name, int panel)
{
    if(!m_curves.contains(name)) return;
    m_curves[name].panel = panel;
    reassignGraphs();
}

void XYPlot::setSharedXAxis(bool shared)
{
    m_sharedXAxis = shared;
}

void XYPlot::addCurve(const QString& name)
{
    if(m_curves.contains(name)) return;
    QCPGraph* g = m_plot->addGraph();
    m_curves[name] = {g,0};
    reassignGraphs();
}

void XYPlot::removeCurve(const QString& name)
{
    if(!m_curves.contains(name)) return;
    m_plot->removeGraph(m_curves[name].graph);
    m_curves.remove(name);
}

void XYPlot::setFrozen(bool frozen)
{
    m_frozen = frozen;
    m_crosshairH->setVisible(frozen);
    m_crosshairV->setVisible(frozen);
    m_crosshairText->setVisible(frozen);

    if(!frozen) applyPending();
}

void XYPlot::applyPending()
{
    for(auto it=m_pendingWhileFrozen.begin(); it!=m_pendingWhileFrozen.end(); ++it)
    {
        if(!m_curves.contains(it.key())) continue;
        m_curves[it.key()].graph->setData(it.value().x, it.value().y);
    }
    m_pendingWhileFrozen.clear();
    m_replotPending = true;
}

void XYPlot::replotCoalesced()
{
    if(!m_replotPending) return;
    m_plot->replot();
    m_replotPending = false;
}

void XYPlot::mouseMoveEvent(QMouseEvent* event)
{
    if(!m_frozen) return;
    updateCrosshair(event->pos());
}

void XYPlot::updateCrosshair(const QPoint& pos)
{
    double x = m_plot->xAxis->pixelToCoord(pos.x());

    QString text;
    double yRef = 0;
    bool first = true;

    for(auto it=m_curves.begin(); it!=m_curves.end(); ++it)
    {
        double y = findNearestY(it.value().graph, x);
        text += QString("%1: %2\n").arg(it.key()).arg(y);
        if(first){ yRef=y; first=false; }
    }

    m_crosshairH->start->setCoords(m_plot->xAxis->range().lower, yRef);
    m_crosshairH->end->setCoords(m_plot->xAxis->range().upper, yRef);

    m_crosshairV->start->setCoords(x, m_plot->yAxis->range().lower);
    m_crosshairV->end->setCoords(x, m_plot->yAxis->range().upper);

    m_crosshairText->position->setCoords(x, yRef);
    m_crosshairText->setText(text);

    m_plot->replot();
}


double XYPlot::findNearestY(QCPGraph* graph, double x)
{
    auto data = graph->data();
    double bestDist = std::numeric_limits<double>::max();
    double bestY = 0;

    for(auto d=data->constBegin(); d!=data->constEnd(); ++d)
    {
        double dist = std::abs(d->key - x);
        if(dist < bestDist)
        {
            bestDist = dist;
            bestY = d->value;
        }
    }
    return bestY;
}
