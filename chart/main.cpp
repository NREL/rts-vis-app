#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSurfaceFormat>
#include <QTextStream>

#include "chartmaster.h"
#include "common.h"

static QFile            logging_file;
static QTextStream      logging_stream;
static QtMessageHandler old_handler = nullptr;

static void message_handler(QtMsgType                 type,
                            QMessageLogContext const& context,
                            QString const&            message) {
    switch (type) {
    case QtDebugMsg: logging_stream << "DEBUG"; break;
    case QtInfoMsg: logging_stream << "INFO"; break;
    case QtWarningMsg: logging_stream << "WARN"; break;
    case QtCriticalMsg: logging_stream << "CRIT"; break;
    case QtFatalMsg: logging_stream << "FATAL"; break;
    }

    logging_stream << ": " << message << "\n";
    logging_stream.flush();

    if (old_handler) {
        old_handler(type, context, message);
    }
}

static void set_up_logging() {
    // some operating systems dont like ':' in their file names.
    QString current_date =
        QDateTime::currentDateTime().toString().replace(' ', '_').replace(':',
                                                                          '-');

    auto log_dir = get_log_directory();
    QDir().mkpath(log_dir);

    QString filename =
        QString("%1/%2.txt").arg(get_log_directory()).arg(current_date);

    logging_file.setFileName(filename);
    if (!logging_file.open(QFile::WriteOnly)) {
        qCritical() << "Unable to open log file!!";
    } else {
        logging_stream.setDevice(&logging_file);
        old_handler = qInstallMessageHandler(message_handler);
    }
}

int main(int argc, char* argv[]) {
    set_up_logging();

    QCoreApplication::setApplicationName("RTSVis");
    QCoreApplication::setApplicationVersion("0.10");

    qInfo() << QCoreApplication::applicationName()
            << QCoreApplication::applicationVersion()
            << QDateTime::currentDateTime().toString();

    QSurfaceFormat format;
    format.setSamples(2);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    ChartMaster chart(app.arguments().value(1));
    chart.show();

    return app.exec();
}
