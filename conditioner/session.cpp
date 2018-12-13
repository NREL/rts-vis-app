#include "session.h"

#include "samplebuffer.h"
#include "zmqworker.h"

#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWebSocket>

Session::Session(QString  var_json_path,
                 QString  host,
                 uint16_t port,
                 int      msec_sample_rate) {
    qDebug() << var_json_path << port << msec_sample_rate;

    QFile file(var_json_path);

    if (!file.open(QFile::ReadOnly)) {
        qFatal("Unable to open json description!");
    }

    auto obj = QJsonDocument::fromJson(file.readAll()).object();

    m_experiment_def = ExperimentDefinition(obj);

    m_message_center = new ZMQCenter(this);

    m_server = new QWebSocketServer(
        "Chart Server", QWebSocketServer::SslMode::NonSecureMode, this);

    auto* collector = new SampleCollector(m_experiment_def, this);

    connect(collector,
            &SampleCollector::new_state_vector,
            this,
            &Session::on_new_state_vector);

    collector->start();

    QTimer* timer = new QTimer(this);

    connect(timer,
            &QTimer::timeout,
            collector,
            &SampleCollector::on_sample_request);

    for (auto const& v : m_experiment_def) {
        auto const& def = *v;
        qDebug() << def.frame_id << def.variables.size();

        auto* socket = m_message_center->listen(host, { def.frame_id });

        auto* buffer = new SampleBuffer(v, this);

        connect(buffer,
                &SampleBuffer::new_state_vector,
                collector,
                &SampleCollector::on_new_state_subvector);

        connect(socket,
                &ZMQWorker::message_acquired,
                buffer,
                &SampleBuffer::on_new_data);

        connect(
            timer, &QTimer::timeout, buffer, &SampleBuffer::on_sample_request);

        buffer->start();
        socket->start();
    }

    {
        // pre build text sent to connecting clients, as this doesn't change
        QJsonObject lobj;
        lobj["mapping"]          = m_experiment_def.mapping_object;
        lobj["sample_rate_msec"] = static_cast<int>(msec_sample_rate);

        m_welcome_preamble = QString(QJsonDocument(lobj).toJson());
    }


    connect(m_server,
            &QWebSocketServer::newConnection,
            this,
            &Session::on_client_connect);
    bool ok = m_server->listen(QHostAddress::Any, port);

    if (!ok) {
        qFatal("Unable to listen on port %i!\n", port);
    }


    timer->start(msec_sample_rate);

    m_startup_time = std::chrono::high_resolution_clock::now();

    qInfo() << "Server is up.";
}

Session::~Session() {}

void Session::on_client_connect() {
    QWebSocket* conn = m_server->nextPendingConnection();

    connect(
        this, &Session::new_data_ready, conn, &QWebSocket::sendBinaryMessage);

    conn->sendTextMessage(m_welcome_preamble);
}

void Session::on_new_state_vector(QVector<float> state) {
    // pack it up

    QByteArray array;

    QDataStream stream(&array, QIODevice::WriteOnly);

    double sample_time =
        std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - m_startup_time)
            .count();

    write_primitive(stream, sample_time);

    // now write data
    write_primitive_array(stream, state.data(), state.size());
    emit new_data_ready(array);
}
