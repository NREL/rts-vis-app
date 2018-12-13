#include "session.h"

#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("conditioner");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("ADMS data conditioning server");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        "experiment",
        QCoreApplication::translate(
            "main", "Json file describing the experimental data setup."));

    QString host = "tcp://my_relay:5020";
    int     port = 50012;
    int     rate = 10;

    QCommandLineOption host_option(
        "s",
        QString("ZMQ host to query. Default is %1").arg(host),
        "host",
        host);
    parser.addOption(host_option);

    QCommandLineOption port_option(
        "p",
        QString("Websocket port to publish on. Default is %1").arg(port),
        "port",
        QString::number(port));
    parser.addOption(port_option);

    QCommandLineOption rate_option(
        "r",
        QString("Message rate, given as messages per second. Default is %1 "
                "messages/sec")
            .arg(rate),
        "rate",
        QString::number(rate));
    parser.addOption(rate_option);

    parser.process(app);

    if (parser.isSet(host_option)) {
        host = parser.value(host_option);
    }

    if (parser.isSet(port_option)) {
        port = parser.value(port_option).toInt();
    }

    if (parser.isSet(rate_option)) {
        rate = qBound(1, parser.value(rate_option).toInt(), 1000);
    }


    int msec = static_cast<int>((1.0 / rate) * 1000);
    assert(msec > 0);

    if (parser.positionalArguments().isEmpty()) {
        parser.showHelp();
    }

    Session session(parser.positionalArguments().value(0),
                    host,
                    static_cast<uint16_t>(port),
                    msec);

    return app.exec();
}
