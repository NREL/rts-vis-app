#ifndef STARTUPDIALOG_H
#define STARTUPDIALOG_H

#include <QDialog>

namespace Ui {
class StartupDialog;
}

class StartupDialog : public QDialog {
    Q_OBJECT
    Ui::StartupDialog* ui;

public:
    explicit StartupDialog(QWidget* parent = nullptr);
    ~StartupDialog();

    QString path_to_experiment() const;
    QUrl    server_url() const;
    bool    time_override() const;
    int     resample_hz() const;
    int     history_seconds() const;

private slots:
    void on_setPathButton_clicked();
    void on_buttonBox_accepted();
    void on_sampleRateSpinBox_valueChanged(int);
    void on_historyTimeEdit_userTimeChanged(QTime const& time);
    void on_openLogButton_clicked();
};

#endif // STARTUPDIALOG_H
