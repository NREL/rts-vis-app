#include "zmqworker.h"

#include "ext/zmq.hpp"

#include <QDebug>
#include <QThread>
#include <QUrl>

namespace {

struct StaticInit {
    StaticInit() {
        qRegisterMetaType<QVector<QByteArray>>("QVector<QByteArray>");
    }
};


StaticInit statics;

} // namespace

//==============================================================================

class ZMQContext {
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
    : QThread(parent), m_url(target), m_subs(subs), m_context(context) {


    m_run_flag = true;
}

ZMQWorker::~ZMQWorker() {
    issue_stop();

    wait();
}

void ZMQWorker::issue_stop() { m_run_flag = false; }


QByteArray get_message(zmq::socket_t& socket) {
    zmq::message_t message;

    bool ok = socket.recv(&message);

    if (!ok) {
        qWarning() << "Socket recv badness!";
        return QByteArray();
    }

    char* first = reinterpret_cast<char*>(message.data());

    size_t size = message.size();

    if (size > std::numeric_limits<int>::max()) {
        qWarning() << "Message too large!";
        return QByteArray();
    }

    return QByteArray(first, static_cast<int>(size));
}

void ZMQWorker::run() {

    zmq::socket_t socket(m_context->context(), ZMQ_SUB);

    for (auto const& sub : m_subs) {
        auto local_string = sub.toStdString();
        socket.setsockopt(
            ZMQ_SUBSCRIBE, local_string.data(), local_string.size());
    }

    socket.connect(m_url.toStdString());

    Q_ASSERT(socket.connected());

    while (m_run_flag) {

        QVector<QByteArray> all_messages;

        all_messages.push_back(get_message(socket));

        while (true) {
            auto rc = socket.getsockopt<size_t>(ZMQ_RCVMORE);

            if (rc == 0) break;

            all_messages.push_back(get_message(socket));
        }

        emit message_acquired(all_messages);
    }
}

//==============================================================================


ZMQCenter::ZMQCenter(QObject* parent) : QObject(parent) {}

ZMQWorker* ZMQCenter::listen(QString url, QStringList const& subs) {
    qInfo() << Q_FUNC_INFO << url << subs;
    ZMQWorker* state =
        new ZMQWorker(url, subs, std::make_shared<ZMQContext>(2), this);

    // m_connections.insert(state);

    return state;
}
