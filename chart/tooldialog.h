#ifndef TOOLDIALOG_H
#define TOOLDIALOG_H

#include <QDialog>

namespace Ui {
class ToolDialog;
}

class ToolDialog : public QDialog {
    /// TODO: remove this class, as it is deprecated
    Q_OBJECT

public:
    explicit ToolDialog(QWidget* parent = nullptr);
    ~ToolDialog();

private:
    Ui::ToolDialog* ui;
};

#endif // TOOLDIALOG_H
