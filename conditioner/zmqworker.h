#ifndef ZMQWORKER_H
#define ZMQWORKER_H

#include <QMetaType>
#include <QObject>
#include <QThread>

#include <memory>
#include <unordered_set>

class ZMQContext;

///
/// \brief The ZMQWorker class handles a single ZMQ subscription
///
class ZMQWorker : public QThread {
    Q_OBJECT

    QString                     m_url;
    QStringList                 m_subs;
    std::shared_ptr<ZMQContext> m_context;

    std::atomic<bool> m_run_flag;

    void run() override;

public:
    explicit ZMQWorker(QString                            url,
                       QStringList const&                 subs,
                       std::shared_ptr<ZMQContext> const& context,
                       QObject*                           parent = nullptr);

    ~ZMQWorker() override;

    void issue_stop();


signals:
    void message_acquired(QVector<QByteArray>);
};

//==============================================================================

///
/// \brief The ZMQCenter class helps in subscribing and starting ZMQ threads.
///

class ZMQCenter : public QObject {
    Q_OBJECT

public:
    ZMQCenter(QObject* parent = nullptr);

    ///
    /// \brief Begin a subscription
    /// \param url The host+port
    /// \param subs The list of topics you wish to subscribe to
    /// \return A worker to connect to messages
    ///
    ZMQWorker* listen(QString url, QStringList const& subs);
};


#endif // ZMQWORKER_H
