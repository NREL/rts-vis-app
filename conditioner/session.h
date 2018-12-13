#ifndef SESSION_H
#define SESSION_H

#include "datacontrol.h"

#include <QObject>
#include <QString>
#include <QWebSocketServer>

#include <chrono>
#include <vector>

class ZMQCenter;

///
/// \brief The Session class models a user session, and sets up the ZMQ->socket
/// flow
///
class Session : public QObject {
    Q_OBJECT

    ExperimentDefinition m_experiment_def;

    ZMQCenter* m_message_center;

    QWebSocketServer* m_server;

    // cached string for use with websockets
    QString m_welcome_preamble;

    std::chrono::high_resolution_clock::time_point m_startup_time;

public:
    explicit Session(QString  var_json_path,
                     QString  host,
                     uint16_t port,
                     int      msec_sample_rate);
    ~Session();

signals:
    void new_data_ready(QByteArray);

private slots:
    void on_client_connect();
    void on_new_state_vector(QVector<float>);
};

#endif // SESSION_H
