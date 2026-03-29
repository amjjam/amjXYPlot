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
    connect(m_plot, &QCustomPlot::mousePress, this,[this](QMouseEvent* event){
      const double tol = 12.0;
      
      for (int i = 0; i < m_panels.size(); ++i)
	{
	  double dx = m_xResetButtons[i]->selectTest(event->pos(), false);
	  double dy = m_yResetButtons[i]->selectTest(event->pos(), false);
	  
	  bool xHit = dx >= 0 && dx < tol;
	  bool yHit = dy >= 0 && dy < tol;
	  
	  // If both hit → choose closest
	  if (xHit && yHit)
        {
	  if (dx < dy)
            {
	      m_panels[i]->axis(QCPAxis::atBottom)
		->setRange(m_defaultXRanges[i]);
            }
	  else
            {
	      m_panels[i]->axis(QCPAxis::atLeft)
		->setRange(m_defaultYRanges[i]);
            }
	  
	  m_replotPending = true;
	  return;
        }
	  
	  if (xHit)
	    {
	      m_panels[i]->axis(QCPAxis::atBottom)
                ->setRange(m_defaultXRanges[i]);
	      
	      m_replotPending = true;
	      return;
	    }
	  
	  if (yHit)
	    {
	      m_panels[i]->axis(QCPAxis::atLeft)
                ->setRange(m_defaultYRanges[i]);
	      
	      m_replotPending = true;
	      return;
	    }
	}
    });
 // for (int i = 0; i < m_panels.size(); ++i){
 // 	// X reset
 //        if (m_xResetButtons[i]->selectTest(event->pos(), false) >= 0){
 // 	  if (i < m_defaultXRanges.size())
 // 	    m_panels[i]->axis(QCPAxis::atBottom)->setRange(m_defaultXRanges[i]);
 // 	  m_replotPending = true;
 // 	  return;
 //        }
	
 //        // Y reset
 //        if (m_yResetButtons[i]->selectTest(event->pos(), false) >= 0){
 // 	  if (i < m_defaultYRanges.size())
 // 	    m_panels[i]->axis(QCPAxis::atLeft)->setRange(m_defaultYRanges[i]);
 // 	  m_replotPending = true;
 // 	  return;
 //        }
 //      }
 //    });
    
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
                          QMouseEvent* event)
{
    auto* plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    if (!plItem) return;

    QCPGraph* graph = qobject_cast<QCPGraph*>(plItem->plottable());
    if (!graph) return;

    QString clickedName;

    // Find which curve was clicked
    for (auto it = m_curves.begin(); it != m_curves.end(); ++it)
    {
        if (it.value().graph == graph)
        {
            clickedName = it.key();
            break;
        }
    }

    if (clickedName.isEmpty()) return;

    // ---- CTRL CLICK → SOLO / RESTORE TOGGLE ----
    if (event->modifiers() & Qt::ControlModifier)
    {
        bool alreadySolo = true;

        for (auto it = m_curves.begin(); it != m_curves.end(); ++it)
        {
            if (it.key() == clickedName)
            {
                if (!it.value().visible)
                    alreadySolo = false;
            }
            else
            {
                if (it.value().visible)
                    alreadySolo = false;
            }
        }

        if (alreadySolo)
        {
            // Restore all curves
            for (auto it = m_curves.begin(); it != m_curves.end(); ++it)
                setCurveVisible(it.key(), true);
        }
        else
        {
            // Solo the clicked curve
            for (auto it = m_curves.begin(); it != m_curves.end(); ++it)
                setCurveVisible(it.key(), it.key() == clickedName);
        }
    }
    else
    {
        // ---- NORMAL CLICK → TOGGLE ----
        bool newVisible = !m_curves[clickedName].visible;
        setCurveVisible(clickedName, newVisible);
    }
}


void XYPlot::rebuildPanels()
{
    if (!m_plot) return;

    // --------------------------------------------------
    // 1. Clean up old reset buttons
    // --------------------------------------------------
    for (auto* b : m_xResetButtons) delete b;
    for (auto* b : m_yResetButtons) delete b;
    m_xResetButtons.clear();
    m_yResetButtons.clear();

    // --------------------------------------------------
    // 2. Remove old panels
    // --------------------------------------------------
    m_plot->plotLayout()->clear();
    m_panels.clear();
    m_plot->clearGraphs();

    int nPanels = std::max(1, m_panelCount);

    // Ensure default range storage matches panel count
    m_defaultXRanges.resize(nPanels);
    m_defaultYRanges.resize(nPanels);

    // --------------------------------------------------
    // 3. Create panels (QCPAxisRect)
    // --------------------------------------------------
    for (int i = 0; i < nPanels; ++i)
    {
        QCPAxisRect* rect = new QCPAxisRect(m_plot);

        rect->setupFullAxesBox(true);

        // Only bottom panel shows x-axis labels if shared
        if (m_sharedXAxis && i < nPanels - 1)
        {
            rect->axis(QCPAxis::atBottom)->setTickLabels(false);
            rect->axis(QCPAxis::atBottom)->setLabel("");
        }

        m_plot->plotLayout()->addElement(i, 0, rect);
        m_panels.push_back(rect);
    }

    // --------------------------------------------------
    // 4. Link X axes if shared
    // --------------------------------------------------
    if (m_sharedXAxis && m_panels.size() > 1)
    {
        for (int i = 1; i < m_panels.size(); ++i)
        {
            connect(m_panels[i]->axis(QCPAxis::atBottom),
                    SIGNAL(rangeChanged(QCPRange)),
                    m_panels[0]->axis(QCPAxis::atBottom),
                    SLOT(setRange(QCPRange)));
        }
    }

    // --------------------------------------------------
    // 5. Apply stored/default ranges (if valid)
    // --------------------------------------------------
    for (int i = 0; i < m_panels.size(); ++i)
    {
        if (m_defaultXRanges[i].size() > 0)
            m_panels[i]->axis(QCPAxis::atBottom)->setRange(m_defaultXRanges[i]);

        if (m_defaultYRanges[i].size() > 0)
            m_panels[i]->axis(QCPAxis::atLeft)->setRange(m_defaultYRanges[i]);
    }

    // --------------------------------------------------
    // 6. Create reset buttons (AFTER panels exist)
    // --------------------------------------------------
    for (int i = 0; i < m_panels.size(); ++i)
    {
        auto* rect = m_panels[i];

        // --- X reset button ---
        QCPItemText* xBtn = new QCPItemText(m_plot);
        xBtn->setText("⟲");
        xBtn->setColor(Qt::darkGray);
        xBtn->setSelectable(true);

        xBtn->position->setType(QCPItemPosition::ptAxisRectRatio);
        xBtn->position->setAxisRect(rect);
        xBtn->position->setCoords(0.98, 0.98);
        xBtn->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);

        m_xResetButtons.push_back(xBtn);

        // --- Y reset button ---
        QCPItemText* yBtn = new QCPItemText(m_plot);
        yBtn->setText("⟲");
        yBtn->setColor(Qt::darkGray);
        yBtn->setSelectable(true);

        yBtn->position->setType(QCPItemPosition::ptAxisRectRatio);
        yBtn->position->setAxisRect(rect);
        yBtn->position->setCoords(0.02, 0.02);
        yBtn->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);

        m_yResetButtons.push_back(yBtn);
    }

    // --------------------------------------------------
    // 7. Rebuild legend (if you are doing custom legend)
    // --------------------------------------------------
    if (m_legend)
    {
        m_plot->plotLayout()->remove(m_legend);
        delete m_legend;
        m_legend = nullptr;
    }

    m_legend = new QCPLegend;
    m_legend->setFillOrder(QCPLegend::foColumnsFirst);
    m_legend->setWrap(5);

    m_plot->plotLayout()->addElement(m_panels.size(), 0, m_legend);
    // Panels get full weight
for (int i = 0; i < m_panels.size(); ++i)
{
    m_plot->plotLayout()->setRowStretchFactor(i, 1);
}

// Legend gets tiny weight
m_plot->plotLayout()->setRowStretchFactor(m_panels.size(), 0.01);

    
    // --------------------------------------------------
    // 8. IMPORTANT: Reattach graphs to panels (your logic)
    // --------------------------------------------------
    for (auto& c : m_curves)
    {
        int panel = c.panel;
        if (panel >= 0 && panel < m_panels.size())
        {
            m_plot->addGraph(
                m_panels[panel]->axis(QCPAxis::atBottom),
                m_panels[panel]->axis(QCPAxis::atLeft)
            );

            c.graph = m_plot->graph(m_plot->graphCount() - 1);
        }
    }

    // --------------------------------------------------
    // 9. Done
    // --------------------------------------------------
    m_replotPending = true;
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
        for (int i = 0; i < m_panels.size(); ++i)
        {
            m_panels[i]->axis(QCPAxis::atBottom)->setRange(lower, upper);

            if (i >= m_defaultXRanges.size())
                m_defaultXRanges.resize(m_panels.size());

            m_defaultXRanges[i] = QCPRange(lower, upper);
        }
    }
    else
    {
        if (panel >= m_panels.size()) return;

        m_panels[panel]->axis(QCPAxis::atBottom)->setRange(lower, upper);

        if (panel >= m_defaultXRanges.size())
            m_defaultXRanges.resize(m_panels.size());

        m_defaultXRanges[panel] = QCPRange(lower, upper);
    }

    m_replotPending = true;
}

void XYPlot::setYAxisRange(double lower, double upper, int panel)
{
    if (m_panels.isEmpty()) return;

    if (panel < 0)
    {
        for (int i = 0; i < m_panels.size(); ++i)
        {
            m_panels[i]->axis(QCPAxis::atLeft)->setRange(lower, upper);

            if (i >= m_defaultYRanges.size())
                m_defaultYRanges.resize(m_panels.size());

            m_defaultYRanges[i] = QCPRange(lower, upper);
        }
    }
    else
    {
        if (panel >= m_panels.size()) return;

        m_panels[panel]->axis(QCPAxis::atLeft)->setRange(lower, upper);

        if (panel >= m_defaultYRanges.size())
            m_defaultYRanges.resize(m_panels.size());

        m_defaultYRanges[panel] = QCPRange(lower, upper);
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
