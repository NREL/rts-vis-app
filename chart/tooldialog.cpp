#include "tooldialog.h"
#include "ui_tooldialog.h"

#include <QTextDocument>

static QTextDocument application_log;
static QTextCursor   log_cursor(&application_log);
static QTextEdit*    current_edit = nullptr;

QtMessageHandler old_handler = nullptr;

static void message_handler(QtMsgType                 type,
                            QMessageLogContext const& context,
                            QString const&            message) {
    QTextBlockFormat format;

    switch (type) {
    case QtDebugMsg: format.setBackground(QColor(0, 100, 0)); break;
    case QtInfoMsg: break;
    case QtWarningMsg: format.setBackground(QColor(100, 100, 0)); break;
    case QtCriticalMsg: format.setBackground(QColor(100, 0, 0)); break;
    case QtFatalMsg: format.setBackground(QColor(200, 0, 0)); break;
    }

    log_cursor.movePosition(QTextCursor::End);
    log_cursor.insertBlock(format);
    log_cursor.insertText(message.toUtf8());

    if (current_edit and current_edit->isVisible()) {
        current_edit->ensureCursorVisible();
    }

    if (old_handler) {
        old_handler(type, context, message);
    }
}

static bool registered = []() {
    old_handler = qInstallMessageHandler(message_handler);
    return true;
}();

ToolDialog::ToolDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ToolDialog) {
    ui->setupUi(this);

    // since app log was static inited, it has bogus font info.
    // this line cleans things up nicely.
    QFont font;
    font.setPointSize(10);
    application_log.setDefaultFont(font);

    ui->logEdit->setDocument(&application_log);

    current_edit = ui->logEdit;
}

ToolDialog::~ToolDialog() {
    current_edit = nullptr;
    delete ui;
}
