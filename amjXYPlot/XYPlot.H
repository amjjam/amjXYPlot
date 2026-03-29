#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QSettings>

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

    void setCurvePanel(const QString& name, int panel);
    void setPanelCount(int n);
    void setSharedXAxis(bool shared);

    void setFrozen(bool frozen);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void replotCoalesced();
    void openConfigDialog();

private:
    struct Curve
    {
        QCPGraph* graph{nullptr};
        int panel{0};
    };

    struct PendingData
    {
        QVector<double> x;
        QVector<double> y;
    };

    void setupUI();
    void rebuildPanels();
    void reassignGraphs();
    void applyPending();
    void updateCrosshair(const QPoint& pos);
    double findNearestY(QCPGraph* graph, double x);

    QCustomPlot* m_plot{nullptr};
    QMap<QString, Curve> m_curves;
    QMap<QString, PendingData> m_pendingWhileFrozen;

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
