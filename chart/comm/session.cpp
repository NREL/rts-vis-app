#include "session.h"

#include "chart.h"
#include "samplebuffer.h"
#include "zmqworker.h"

#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

Session::Session(ExperimentPtr const& definition,
                 QString              host,
                 uint16_t             port,
                 int                  msec_sample_rate,
                 TimeOption           time_option,
                 QObject*             parent)
    : QObject(parent), m_experiment_def(definition) {
    qDebug() << host << port << msec_sample_rate;

    m_message_center = new ZMQCenter(this);

    m_collector = new SampleCollector(m_experiment_def, this);

    connect(m_collector,
            &SampleCollector::new_state_vector,
            this,
            &Session::on_new_state_vector);

    m_collector->start();

    QTimer* timer = new QTimer(this);

    connect(timer,
            &QTimer::timeout,
            m_collector,
            &SampleCollector::on_sample_request);

    QHash<QString, ZMQThreadController*> socket_map;

    // connect sampled sources
    for (auto const& v : *m_experiment_def) {
        auto const& def = *v;
        qDebug() << "Creating buffer for" << def.frame_id
                 << def.variables.size();

        auto* socket = m_message_center->open_channel(host, { def.frame_id });

        SampleBufferOptions options;
        options.override_time = (time_option == TimeOption::USE_LOCAL);

        auto* buffer = new SampleBuffer(v, options, this);

        socket_map[def.frame_id] = socket;

        m_buffers.push_back(buffer);

        connect(buffer,
                &SampleBuffer::new_state_vector,
                m_collector,
                &SampleCollector::on_new_state_subvector);

        connect(socket,
                &ZMQThreadController::message_acquired,
                buffer,
                &SampleBuffer::on_new_data);

        connect(
            timer, &QTimer::timeout, buffer, &SampleBuffer::on_sample_request);

        buffer->start();
        socket->start();
    }

    // now, for each chart that demands a high quality signal
    // create a buffer for that

    for (auto const& chart : m_experiment_def->charts) {
        auto type = string_to_chart_type(chart.type);

        if (type != ChartType::SCOPE) continue;

        qDebug() << "building high res source for scope";

        // at this point we will only support one socket for one scope

        // for all vars in the chart, get the frame its coming from

        auto frame_ptr = get_common_frame(m_experiment_def, chart.variables);

        if (!frame_ptr) {
            // TODO: remove this limitation
            throw std::runtime_error("No common frame! Scopes need all "
                                     "variables from the same frame!");
        }

        if (m_frame_to_line_delay_map.contains(frame_ptr->frame_id)) {
            // all set for this one
            continue;
        }

        Q_ASSERT(socket_map.contains(frame_ptr->frame_id));

        auto* ptr = socket_map[frame_ptr->frame_id];

        Q_ASSERT(ptr != nullptr);

        LineDelayBuffer* buffer = new LineDelayBuffer(frame_ptr, this);

        m_buffers.push_back(buffer);

        Q_ASSERT(!m_frame_to_line_delay_map.contains(frame_ptr->frame_id));

        m_frame_to_line_delay_map[frame_ptr->frame_id] = buffer;

        qDebug() << ptr << buffer;

        connect(ptr,
                &ZMQThreadController::message_acquired,
                buffer,
                &LineDelayBuffer::on_new_data);

        buffer->start();
    }

    timer->start(msec_sample_rate);

    m_startup_time = std::chrono::high_resolution_clock::now();

    qInfo() << "Session is up and listening @" << msec_sample_rate;
}

Session::~Session() {
    m_message_center->stop_all();

    m_collector->quit();
    m_collector->wait();

    for (auto* p : m_buffers) {
        p->quit();
        p->wait();
    }
}

ExperimentDefinition const& Session::experiment_definition() const {
    return *m_experiment_def;
}

void Session::on_new_state_vector(QVector<float> state, double timestamp) {
    if (m_last_timestamp > 0) {
        if (timestamp <= m_last_timestamp) {
            // source has stalled. skip updates
            return;
        }
    }
    m_last_timestamp = timestamp;
    //    double hb_time =
    //        std::chrono::duration<double>(
    //            std::chrono::high_resolution_clock::now() - m_startup_time)
    //            .count();
    emit new_data_ready(timestamp, state);
}

LineDelayBuffer* Session::buffer_for_frame(QString const& s) const {
    return m_frame_to_line_delay_map[s];
}
