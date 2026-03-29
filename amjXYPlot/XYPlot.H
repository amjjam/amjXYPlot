#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QSettings>

#include <qcustomplot.h>

class QCustomPlot;
class QCPGraph;
class QCPItemLine;
class QCPItemText;
class QCPAxisRect;
class QDialog;

namespace amjWidgets {

class XYPlot : public QWidget
{
    Q_OBJECT

public:
    explicit XYPlot(QWidget* parent = nullptr);

    void addCurve(const QString& name);
    void removeCurve(const QString& name);

    template <typename TX, typename TY>
    void setCurveData(const QString& name, const QVector<TX>& x, const QVector<TY>& y);
  void setCurveColor(const QString& name, const QColor& color);
  void setCurveVisible(const QString& name, bool visible);
  
    void setCurvePanel(const QString& name, int panel);
    void setPanelCount(int n);
    void setSharedXAxis(bool shared);
    void setXAxisRange(double lower, double upper, int panel= -1);
    void setYAxisRange(double lower, double upper, int panel = -1);

    void setFrozen(bool frozen);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void replotCoalesced();
    void openConfigDialog();
    void onLegendClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);

private:
  struct Curve
  {
    QVector<double> x;
    QVector<double> y;
    QCPGraph* graph{nullptr};
    int panel{0};
    
    QColor color;
    bool visible{true};
  };
  
    struct PendingData
    {
        QVector<double> x;
        QVector<double> y;
    };
  
  QVector<QColor> m_defaultColors {
    Qt::red, Qt::green, Qt::blue, Qt::magenta, Qt::cyan,
    Qt::darkYellow, Qt::black, Qt::darkBlue, Qt::darkRed, Qt::darkGreen
  };
    void setupUI();
    void rebuildPanels();
  void applyLegendStyle();
  void reassignGraphs();
    void applyPending();
    void updateCrosshair(const QPoint& pos);
    double findNearestY(QCPGraph* graph, double x);

  QCustomPlot* m_plot{nullptr};
  QCPLegend* m_legend{nullptr};
  QMap<QString, Curve> m_curves;
  QMap<QString, PendingData> m_pendingWhileFrozen;

  // Last range set with set X/Y AxisRange, so it can be restored with click
  QVector<QCPRange> m_defaultXRanges;
  QVector<QCPRange> m_defaultYRanges;

  // Axis reset range buttons
  QVector<QCPItemText*> m_xResetButtons;
  QVector<QCPItemText*> m_yResetButtons;
  
    QVector<QCPAxisRect*> m_panels;
    int m_panelCount{1};
    bool m_sharedXAxis{true};

    QTimer m_timer;
    bool m_replotPending{false};

    bool m_frozen{false};

    QCPItemLine* m_crosshairH{nullptr};
    QCPItemLine* m_crosshairV{nullptr};
    QCPItemText* m_crosshairText{nullptr};

    QSettings m_settings;
};

// Template

template <typename TX, typename TY>
void XYPlot::setCurveData(const QString& name, const QVector<TX>& x, const QVector<TY>& y)
{
    QVector<double> xd(x.size()), yd(y.size());
    for (int i = 0; i < x.size(); ++i)
    {
        xd[i] = static_cast<double>(x[i]);
        yd[i] = static_cast<double>(y[i]);
    }

    QMetaObject::invokeMethod(this, [=]() {
        if (!m_curves.contains(name)) return;

        if (m_frozen)
        {
            m_pendingWhileFrozen[name] = {xd, yd};
            return;
        }

        m_curves[name].graph->setData(xd, yd);
        m_replotPending = true;
    }, Qt::QueuedConnection);
}

} // namespace amjWidgets
