#include "chartmaster.h"

#include "comm/datacontrol.h"
#include "comm/session.h"
#include "startupdialog.h"

#include "ui_chartmaster.h"

#include "chartdata.h"
#include "chartwidget.h"

#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

// special includes
#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

///
/// \brief Look up a list of global ids and convert them to their UUIDs
///
/// This only exists for when we want to force some specific plots for testing.
///
static QStringList vids_to_uuids(ExperimentDefinition const& def,
                                 std::vector<size_t> const&  ids) {

    QStringList ret;

    for (auto id : ids) {
        auto const& v = def.global_to_var_mapping[id];

        ret << uuid_to_qstring(v->uuid);
    }
    return ret;
}

///
/// \brief The ReadResult struct is a result tuple for reading the experiment
/// definition.
///
struct ReadResult {
    ExperimentPtr experiment;
    QString       error;

    ReadResult() = default;

    // Note, the pointer T is not const!
    ReadResult(std::shared_ptr<ExperimentDefinition> const& d)
        : experiment(d) {}
    ReadResult(QString const& error_message) : error(error_message) {}
};

///
/// \brief Read the experiment definition and report any errors
///
/// Also overrides the chart requirements for testing if needed.
///
static ReadResult read_experiment(QString const& path, bool debug_mode) {
    QFile file(path);

    if (!file.open(QFile::ReadOnly)) {
        return QString("Unable to open file");
    }

    QJsonParseError error{};
    auto            doc = QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError) {
        return QString("Unable to parse JSON: %1").arg(error.errorString());
    }

    auto ptr = std::make_shared<ExperimentDefinition>(doc.object());

    if (debug_mode) {
        auto& charts = ptr->charts;
        charts.clear();
        {
            // add testing alert panel

            // find all vars that are alerts
            std::vector<size_t> vids;

            auto iter = ptr->global_to_var_mapping.begin();
            auto last = ptr->global_to_var_mapping.end();

            for (; iter != last; iter++) {
                if (iter.value()->data_type != "float") {
                    vids.push_back(iter.key());
                }
            }


            std::iota(vids.begin(), vids.end(), 3);

            Chart c;
            c.title          = "Alarm";
            c.type           = chart_type_to_string(ChartType::ALERT);
            c.chart_row      = 0;
            c.chart_col      = 0;
            c.chart_col_span = 2;

            c.variables = vids_to_uuids(*ptr, vids);

            charts.push_back(c);
        }

        {
            // add testing line chart

            std::vector<size_t> vids(3);

            std::iota(vids.begin(), vids.end(), 3);

            Chart c;
            c.title     = "Test Chart 1";
            c.type      = "line";
            c.chart_row = 1;
            c.chart_col = 0;

            c.variables = vids_to_uuids(*ptr, vids);

            charts.push_back(c);
        }

        {
            // add testing line chart 2

            std::vector<size_t> vids(100);

            std::iota(vids.begin(), vids.end(), 255);

            Chart c;
            c.title     = "Test Chart 2";
            c.type      = "line";
            c.chart_row = 1;
            c.chart_col = 1;

            c.variables = vids_to_uuids(*ptr, vids);

            charts.push_back(c);
        }


        {
            // add testing stack chart

            std::vector<size_t> vids(5);

            std::iota(vids.begin(), vids.end(), 3);

            Chart c;
            c.title          = "Test Chart 1";
            c.type           = "stack";
            c.chart_row      = 2;
            c.chart_col      = 0;
            c.chart_col_span = 2;

            c.variables = vids_to_uuids(*ptr, vids);

            charts.push_back(c);
        }

        {
            Chart c;
            c.title          = "Scope";
            c.type           = "scope";
            c.chart_row      = 3;
            c.chart_col      = 0;
            c.chart_col_span = 2;

            c.variables << QString("6beebdce-63b0-402d-b207-d1c29ada278d");

            charts.push_back(c);
        }
    }

    return ptr;
}


void ChartMaster::load_source(QString experiment_source,
                              QUrl    server,
                              int     resample_hz,
                              bool    time_override) {
    qDebug() << Q_FUNC_INFO << experiment_source << server;

    // QFileInfo source_file(experiment_source);

    auto read_result = read_experiment(experiment_source, false);

    if (!read_result.error.isEmpty()) {

        QMessageBox::critical(
            this, "Open File Error", read_result.error, QMessageBox::Close);

        exit(EXIT_FAILURE);
    }

    // figure out the rate we should be resampling at in MS.

    int msec = (1000.0f / static_cast<float>(resample_hz));

    qDebug() << "Data sample rate" << msec << "ms";

    m_server_ms_delay = msec;

    // TODO: remove the port hardcode here, doesn't appear to be used.
    m_session = new Session(read_result.experiment,
                            server.toString(),
                            50012,
                            msec,
                            time_override ? TimeOption::USE_LOCAL
                                          : TimeOption::USE_SOURCE,
                            this);

    qDebug() << "Session started";

    m_required_charts = read_result.experiment->charts;

    connect(
        m_session, &Session::new_data_ready, this, &ChartMaster::new_timestep);
}

///
/// \brief Find a suitable empty row and column for a given chart.
///
/// \todo Improve
///
static std::pair<int, int> get_row_col(QGridLayout* layout, Chart const& c) {
    int row = c.chart_row;
    int col = c.chart_col;

    while (layout->itemAtPosition(row, col) != nullptr) {
        row++;
    }

    return { row, col };
}


void ChartMaster::build_charts(int history_seconds) {
    qDebug() << "Loading specified plots";

    auto mapping =
        m_session->experiment_definition().uuid_to_global_varid_mapping;

    for (auto const& c : m_required_charts) {
        std::vector<size_t> vids;

        for (auto const& uuid : c.variables) {

            auto iter = mapping.find(uuid);

            if (iter == mapping.end()) {
                qCritical() << "Unknown UUID" << uuid;
                continue;
            }

            vids.push_back(*iter);
        }

        ChartWidgetOptions options(c, std::move(vids));
        options.experiment_info = m_session->experiment_definition_ptr();
        options.history_ms      = history_seconds * 1000;

        int row, col;

        std::tie(row, col) = get_row_col(ui->gridLayout, c);

        auto* chart = make_panel(options, m_session);

        ui->gridLayout->addWidget(
            chart, row, col, c.chart_row_span, c.chart_col_span);

        m_charts.push_back(chart);
    }


    update_stretch();
}

void ChartMaster::update_stretch() {
    auto num_cols = ui->gridLayout->columnCount();
    auto num_rows = ui->gridLayout->rowCount();

    // All charts are to use the size the layout gives them
    for (auto* p : m_charts) {
        p->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    // All charts colums shall be the same size
    for (int i = 0; i < num_cols; i++) {
        ui->gridLayout->setColumnStretch(i, 1);
    }

    // for all the charts in this row, are they the same chart?
    auto is_all_same = [num_cols, this](int row) -> QWidget* {
        QWidget* p = nullptr;

        for (int i = 0; i < num_cols; i++) {
            auto* item = ui->gridLayout->itemAtPosition(row, i);
            if (!item) continue;

            auto* widget = item->widget();

            if (!widget) continue;

            if (!p) p = widget;

            if (p != widget) return nullptr;
        }

        return p;
    };

    // for each row, see if it is occupied by just an alarm panel.
    // if so, crunch the alarm panel down. else, distribute all rows evenly.
    for (int i = 0; i < num_rows; i++) {

        auto* p = is_all_same(i);

        auto* alarm = dynamic_cast<AlertChartWidget*>(p);

        int stretch = 1;

        if (p and alarm) stretch = 0;

        ui->gridLayout->setRowStretch(i, stretch);
    }
}

// TODO: improve error handling

static void handle_runtime_error(QWidget* parent, std::runtime_error const& e) {
    QMessageBox::critical(
        parent,
        "Exception",
        QString("An internal error occured: %1").arg(e.what()));
    exit(EXIT_FAILURE);
}

static void handle_unk_error(QWidget* parent) {
    QMessageBox::critical(parent,
                          "Exception",
                          QString("An internal error occured. Please contact "
                                  "the developer for a resolution."));
    exit(EXIT_FAILURE);
}


enum ScreenSaverStates { ENABLE_SCREENSAVER, DISABLE_SCREENSAVER };

void set_inhibit_screensaver(ScreenSaverStates state, unsigned& assertion_id) {
#ifdef __APPLE__
    switch (state) {
    case DISABLE_SCREENSAVER: {
        CFStringRef reason_for_activity = CFSTR("ADMS Chart Running");
        if (IOPMAssertionCreateWithName(
                kIOPMAssertionTypePreventUserIdleDisplaySleep,
                kIOPMAssertionLevelOn,
                reason_for_activity,
                &assertion_id) != kIOReturnSuccess) {
            assertion_id = kIOPMNullAssertionID;
        }

    } break;
    case ENABLE_SCREENSAVER:
        if (assertion_id != kIOPMNullAssertionID) {
            IOPMAssertionRelease(assertion_id);
            assertion_id = kIOPMNullAssertionID;
        }
        break;
    }

#endif
}

ChartMaster::ChartMaster(QString experiment_override, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::ChartMaster) {
    ui->setupUi(this);

    // m_dialog = new ToolDialog(this);

    try {
        QString experiment_path;
        QUrl    server_url;
        bool    time_override = false;

        int resample_hz     = 30;
        int history_seconds = 10;

        // get info from our user
        if (experiment_override.isEmpty()) {

            StartupDialog dialog(this);

            auto result = dialog.exec();

            if (result == QDialog::Rejected) {
                exit(EXIT_SUCCESS);
            }

            experiment_path = dialog.path_to_experiment();
            server_url      = dialog.server_url();
            time_override   = dialog.time_override();
            resample_hz     = dialog.resample_hz();
            history_seconds = dialog.history_seconds();
        }

        if (resample_hz < 1) {
            throw std::runtime_error("Unable to sample at this rate");
        }

        if (history_seconds < 1) {
            throw std::runtime_error("Please specify a higher history");
        }

        load_source(experiment_path, server_url, resample_hz, time_override);

        qDebug() << "Source loaded";

        build_charts(history_seconds);

        qDebug() << "Charts built";

        setWindowTitle(
            QString("%1 @ %2 Hz").arg(server_url.toString()).arg(resample_hz));

    } catch (std::runtime_error const& e) {
        handle_runtime_error(this, e);
    } catch (...) {
        handle_unk_error(this);
    }

    set_inhibit_screensaver(DISABLE_SCREENSAVER, m_power_assertion_id);
}

ChartMaster::~ChartMaster() {
    delete ui;
    set_inhibit_screensaver(ENABLE_SCREENSAVER, m_power_assertion_id);
}


void ChartMaster::new_timestep(double house_time, QVector<float> array) try {
    // qDebug() << Q_FUNC_INFO << house_time;

    DataRef ref;

    ref.server_ms_delay = m_server_ms_delay;

    ref.server_time = house_time;

    auto data  = array;
    ref.source = data.data();
    ref.count  = data.size();

    for (auto* p : m_charts) {
        p->add(ref);
    }

    update_all();
} catch (std::runtime_error const& e) {
    handle_runtime_error(this, e);
} catch (...) {
    handle_unk_error(this);
}

void ChartMaster::update_all() try {
    for (auto* p : m_charts) {
        p->update();
    }
} catch (std::runtime_error const& e) {
    handle_runtime_error(this, e);
} catch (...) {
    handle_unk_error(this);
}

void ChartMaster::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        // m_dialog->show();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}
