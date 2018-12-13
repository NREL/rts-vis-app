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
class ZMQWorker : public QObject {
    Q_OBJECT

    QString                     m_url;
    QStringList                 m_subs;
    std::shared_ptr<ZMQContext> m_context;

    std::atomic<bool> m_run_flag;

public:
    explicit ZMQWorker(QString                            url,
                       QStringList const&                 subs,
                       std::shared_ptr<ZMQContext> const& context,
                       QObject*                           parent = nullptr);

    ~ZMQWorker() override;

public slots:
    void run();
    void demand_stop();

signals:
    void message_acquired(QVector<QByteArray>);
};

//==============================================================================

///
/// \brief The ZMQThreadController class manages a threaded ZMQ subscription.
///
class ZMQThreadController : public QObject {
    Q_OBJECT

    QThread    m_worker_thread;
    ZMQWorker* m_worker;

public:
    ZMQThreadController(QString                            url,
                        QStringList const&                 subs,
                        std::shared_ptr<ZMQContext> const& context,
                        QObject*                           parent = nullptr);
    ~ZMQThreadController();

public slots:
    void start();
    void stop();


signals:
    void message_acquired(QVector<QByteArray>);
};

//==============================================================================

///
/// \brief The ZMQCenter class helps in subscribing and starting ZMQ threads.
///
class ZMQCenter : public QObject {
    Q_OBJECT

    std::unordered_set<ZMQThreadController*> m_connections;

public:
    ZMQCenter(QObject* parent = nullptr);

    ~ZMQCenter();

    ZMQThreadController* open_channel(QString url, QStringList const& subs);

    // this should be called before the destructor
    void stop_all();

signals:
    void issue_stop();
};


#endif // ZMQWORKER_H
