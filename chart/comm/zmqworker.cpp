#include "zmqworker.h"

#include "ext/zmq.hpp"

#include <chrono>

#include <QDebug>
#include <QThread>
#include <QUrl>

namespace {
// register the message vector type
struct StaticInit {
    StaticInit() {
        qRegisterMetaType<QVector<QByteArray>>("QVector<QByteArray>");
    }
};


StaticInit statics;

} // namespace

//==============================================================================

// TODO: do we need this?
class ZMQContext {
    // this handles thread stuff on its own and does not require sync
    zmq::context_t m_context;

public:
    ZMQContext(int num_background_threads);

    zmq::context_t& context();
};

ZMQContext::ZMQContext(int num_background_threads)
    : m_context(num_background_threads) {}

zmq::context_t& ZMQContext::context() { return m_context; }

//==============================================================================

ZMQWorker::ZMQWorker(QString                            target,
                     QStringList const&                 subs,
                     std::shared_ptr<ZMQContext> const& context,
                     QObject*                           parent)
    : QObject(parent), m_url(target), m_subs(subs), m_context(context) {


    m_run_flag = true;
}

ZMQWorker::~ZMQWorker() { Q_ASSERT(!m_run_flag); }

void ZMQWorker::demand_stop() {
    qDebug() << Q_FUNC_INFO;
    m_run_flag = false;
    m_context.reset();
}

///
/// \brief Consume a message from a socket.
///
static QByteArray get_message(zmq::socket_t& socket) try {
    zmq::message_t message;

    bool ok = socket.recv(&message);

    if (!ok) {
        qWarning() << "Socket recv badness!";
        return QByteArray();
    }

    char* first = reinterpret_cast<char*>(message.data());

    size_t size = message.size();

    // Qt uses ints for sizes.
    if (size > std::numeric_limits<int>::max()) {
        qWarning() << "Message too large! Poke the dev!";

        // In this case, however, the dev is probably just going to cry.
        return QByteArray();
    }

    return QByteArray(first, static_cast<int>(size));
} catch (zmq::error_t const& err) {
    // during shutdown, this library tends to do some weird things, like throw
    // errors.
    // so, catch weirdness and return empty
    if (err.num() != ETERM) {
        throw;
    }
    return QByteArray();
}

void ZMQWorker::run() {
    // create a socket
    zmq::socket_t socket(m_context->context(), ZMQ_SUB);

    // for each sub, set the socket to listen for it
    for (auto const& sub : m_subs) {
        auto local_string = sub.toStdString();
        socket.setsockopt(
            ZMQ_SUBSCRIBE, local_string.data(), local_string.size());
    }

    socket.connect(m_url.toStdString());

    Q_ASSERT(socket.connected());

    // rate limiting. not the smart way
    auto last_time = std::chrono::high_resolution_clock::now();

    // TODO: improve rate limiting code

    while (m_run_flag) {

        // handle a message

        QVector<QByteArray> all_messages;

        // get the first one, blocking

        all_messages.push_back(get_message(socket));

        // get any other parts, if multipart. but only if we are running.
        // when we shut down, we can get here and dont want to touch the socket
        // anymore
        while (m_run_flag) {
            auto rc = socket.getsockopt<size_t>(ZMQ_RCVMORE);

            if (rc == 0) break;

            all_messages.push_back(get_message(socket));
        }

        auto message_time = std::chrono::high_resolution_clock::now();

        double since_last =
            std::chrono::duration<double>(message_time - last_time).count();

        // just the worst
        if (since_last < .0005) {
            qDebug() << "Rate Limit!";
            continue;
        }

        // emit the new message
        if (!all_messages.empty()) {
            emit message_acquired(all_messages);
        }
    }

    socket.close();
}

//==============================================================================
ZMQThreadController::ZMQThreadController(
    QString                            url,
    QStringList const&                 subs,
    std::shared_ptr<ZMQContext> const& context,
    QObject*                           parent)
    : QObject(parent) {

    m_worker = new ZMQWorker(url, subs, context);

    m_worker->moveToThread(&m_worker_thread);
    connect(
        &m_worker_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(&m_worker_thread, &QThread::started, m_worker, &ZMQWorker::run);

    connect(m_worker,
            &ZMQWorker::message_acquired,
            this,
            &ZMQThreadController::message_acquired);
}
ZMQThreadController::~ZMQThreadController() {
    m_worker_thread.quit();
    m_worker_thread.wait();
}


void ZMQThreadController::start() { m_worker_thread.start(); }

void ZMQThreadController::stop() { m_worker->demand_stop(); }

//==============================================================================


ZMQCenter::ZMQCenter(QObject* parent) : QObject(parent) {}
ZMQCenter::~ZMQCenter() {}

void ZMQCenter::stop_all() { emit issue_stop(); }

ZMQThreadController* ZMQCenter::open_channel(QString            url,
                                             QStringList const& subs) {
    ZMQThreadController* state = new ZMQThreadController(
        url, subs, std::make_shared<ZMQContext>(2), this);

    connect(this, &ZMQCenter::issue_stop, state, &ZMQThreadController::stop);

    m_connections.insert(state);

    return state;
}
