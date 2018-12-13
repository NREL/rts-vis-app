#ifndef CHARTMASTER_H
#define CHARTMASTER_H

#include "chart.h"
#include "tooldialog.h"

#include <QMainWindow>
#include <QUuid>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// forward decls
namespace Ui {
class ChartMaster;
}

class Panel;
class Session;

class ChartMaster : public QMainWindow {
    Q_OBJECT
    Ui::ChartMaster* ui; // Qt gui root

    Session* m_session = nullptr;

    std::vector<Chart>  m_required_charts;
    std::vector<Panel*> m_charts;

    size_t m_server_ms_delay;

    unsigned m_power_assertion_id = 0;

    ///
    /// \brief Load the given experiment and start up the session
    ///
    void load_source(QString, QUrl server, int resample_hz, bool time_override);

    ///
    /// \brief Construct the charts as given in m_required_charts
    ///
    void build_charts(int history_seconds);

    ///
    /// \brief Finalize the chart sizes
    ///
    void update_stretch();

public:
    explicit ChartMaster(QString  experiment_override,
                         QWidget* parent = nullptr);
    ~ChartMaster() override;

private slots:
    ///
    /// \brief Issue an update for each chart
    ///
    void update_all();

    ///
    /// \brief Handle a new frame of data with a time
    ///
    void new_timestep(double, QVector<float>);

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent* event) override;
};

#endif // CHARTMASTER_H
