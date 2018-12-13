#include "common.h"

#include <QDir>

// Todo: Better handling for windows
QString get_log_directory() {
    QString path;
#ifdef __APPLE__
    path = QString("%1/Library/Logs/RTSVis/").arg(QDir::homePath());
#else
    path = QString("%1/RTSVisLogs/").arg(QDir::homePath());
#endif
    return path;
}
