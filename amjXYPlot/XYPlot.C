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
  : QWidget(parent), m_settings("amjWidgets","XYPlot"){
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
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom|QCP::iSelectLegend);
    connect(m_plot, &QCustomPlot::legendClick,this, &XYPlot::onLegendClick);
    
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

void XYPlot::onLegendClick(QCPLegend* /*legend*/,
                          QCPAbstractLegendItem* item,
                          QMouseEvent* /*event*/)
{
    // We only care about plottable legend items
    auto* plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    if (!plItem) return;

    QCPGraph* graph = qobject_cast<QCPGraph*>(plItem->plottable());
    if (!graph) return;

    // Find matching curve
    for (auto it = m_curves.begin(); it != m_curves.end(); ++it)
    {
        if (it.value().graph == graph)
        {
            bool newVisible = !it.value().visible;
            setCurveVisible(it.key(), newVisible);

            // Optional: fade legend text when hidden
            plItem->setTextColor(newVisible ? Qt::black : Qt::gray);

            break;
        }
    }
}

void XYPlot::rebuildPanels()
{
    // 🔥 CLEAN UP old legend FIRST
    if (m_legend)
    {
        m_plot->plotLayout()->remove(m_legend);
        delete m_legend;
        m_legend = nullptr;
    }

    // Now safe to clear layout
    m_plot->plotLayout()->clear();
    m_panels.clear();

    // Create main grid
    QCPLayoutGrid* grid = new QCPLayoutGrid;
    m_plot->plotLayout()->addElement(0, 0, grid);

    // Create panels
    for (int i = 0; i < m_panelCount; i++)
    {
        QCPAxisRect* rect = new QCPAxisRect(m_plot);
        grid->addElement(i, 0, rect);
        m_panels.append(rect);
    }

    // Create new legend
    m_legend = new QCPLegend;
    m_plot->plotLayout()->addElement(1, 0, m_legend);

    m_legend->setVisible(true);
    m_legend->setBrush(QBrush(QColor(255,255,255,200)));
    m_legend->setFillOrder(QCPLegend::foColumnsFirst);
    m_legend->setWrap(5);
    m_legend->setMargins(QMargins(5,5,5,5));
    m_legend->setMaximumSize(QSize(QWIDGETSIZE_MAX,50));
    
    m_plot->plotLayout()->setRowStretchFactor(0, 10);
    m_plot->plotLayout()->setRowStretchFactor(1, 1);

    
    reassignGraphs();
}


void XYPlot::applyLegendStyle(){
  m_plot->legend->setFillOrder(QCPLegend::foColumnsFirst);
  m_plot->legend->setWrap(5);
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

void XYPlot::setXAxisRange(double lower, double upper, int panel)
{
    if (m_panels.isEmpty()) return;

    if (panel < 0 || m_sharedXAxis)
    {
        // Apply to all panels
        for (auto* rect : m_panels)
        {
            rect->axis(QCPAxis::atBottom)->setRange(lower, upper);
        }
    }
    else
    {
        if (panel >= m_panels.size()) return;

        m_panels[panel]->axis(QCPAxis::atBottom)->setRange(lower, upper);
    }

    m_replotPending = true;
}

void XYPlot::setYAxisRange(double lower, double upper, int panel)
{
    if (m_panels.isEmpty()) return;

    if (panel < 0)
    {
        // Apply to all panels
        for (auto* rect : m_panels)
        {
            rect->axis(QCPAxis::atLeft)->setRange(lower, upper);
        }
    }
    else
    {
        if (panel >= m_panels.size()) return;
        m_panels[panel]->axis(QCPAxis::atLeft)->setRange(lower, upper);
    }

    m_replotPending = true;
}

void XYPlot::addCurve(const QString& name)
{
    if (m_curves.contains(name)) return;

    // Create graph
    QCPGraph* g = m_plot->addGraph();
    g->removeFromLegend();
    
    // Determine color index based on current number of curves
    int idx = m_curves.size();

    QColor col = m_defaultColors[idx % m_defaultColors.size()];

    QPen pen(col);

    // If we exceed palette, reuse colors but dashed
    if (idx >= m_defaultColors.size())
        pen.setStyle(Qt::DashLine);

    g->setPen(pen);

    // Important for future legend support
    g->setName(name);
    m_legend->addItem(new QCPPlottableLegendItem(m_legend, g));
    g->setSelectable(QCP::stWhole);
    g->setVisible(true);
    
    // Store curve (including color)
    Curve c;
    c.graph = g;
    c.panel = 0;
    c.color = col;
    
    m_curves.insert(name, c);//    m_curves.insert(name, Curve{g, 0, col});
    //m_curves[name] = { g, 0, col };

    // Attach to correct panel axes
    reassignGraphs();
}


void XYPlot::removeCurve(const QString& name)
{
    if(!m_curves.contains(name)) return;
    m_plot->removeGraph(m_curves[name].graph);
    m_curves.remove(name);
}

void XYPlot::setCurveColor(const QString& name, const QColor& color)
{
    if (!m_curves.contains(name)) return;

    auto& c = m_curves[name];
    c.color = color;

    if (c.graph)
    {
        QPen pen = c.graph->pen();
        pen.setColor(color);
        c.graph->setPen(pen);
    }

    m_replotPending = true;
}

// void XYPlot::setCurveVisible(const QString& name, bool visible)
// {
//     if (!m_curves.contains(name)) return;

//     auto& c = m_curves[name];
//     c.visible = visible;

//     if (c.graph)
//     {
//         c.graph->setVisible(visible);

//         // 🔥 ADD THIS BLOCK HERE
//         QPen pen = c.graph->pen();
//         pen.setColor(visible ? c.color : Qt::gray);
//         pen.setStyle(visible ? Qt::SolidLine : Qt::DotLine);
//         c.graph->setPen(pen);
//     }

//     m_replotPending = true;
// }
void XYPlot::setCurveVisible(const QString& name, bool visible)
{
    if (!m_curves.contains(name)) return;

    auto& c = m_curves[name];
    c.visible = visible;

    if (c.graph){
      c.graph->setVisible(visible);
      
      QPen pen = c.graph->pen();
      pen.setStyle(visible ? Qt::SolidLine : Qt::DotLine);
      c.graph->setPen(pen);
    }
    
    m_replotPending = true;
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
