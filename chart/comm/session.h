#ifndef SESSION_H
#define SESSION_H

#include "datacontrol.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <vector>

class ZMQCenter;
class SampleCollector;
class SampleBuffer;
class LineDelayBuffer;

///
/// \brief The TimeOption enum lets the user select between using the source
/// data clock, or the local plotting tool clock for data timestamps
///
enum class TimeOption {
    USE_SOURCE,
    USE_LOCAL,
};

///
/// \brief The Session class models a user session, handling setup and teardown
/// of the sampling system
///
class Session : public QObject {
    Q_OBJECT

    std::shared_ptr<ExperimentDefinition const> m_experiment_def;

    ZMQCenter*            m_message_center;
    SampleCollector*      m_collector;
    std::vector<QThread*> m_buffers;
    double                m_last_timestamp = 0;

    QHash<QString, LineDelayBuffer*> m_frame_to_line_delay_map;

    std::chrono::high_resolution_clock::time_point m_startup_time;

public:
    explicit Session(ExperimentPtr const& definition,
                     QString              host, ///< ZMQ Server
                     uint16_t             port, ///< ZMQ Server Port
                     int                  msec_sample_rate,
                     TimeOption           time_option,
                     QObject*             parent);
    ~Session();

    ExperimentDefinition const& experiment_definition() const;
    auto experiment_definition_ptr() const { return m_experiment_def; }

    LineDelayBuffer* buffer_for_frame(QString const&) const;

signals:
    ///
    /// \brief new_data_ready is emitted when a new state vector is ready
    ///
    void new_data_ready(double, QVector<float>);

private slots:
    void on_new_state_vector(QVector<float>, double);
};

#endif // SESSION_H
