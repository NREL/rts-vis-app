#include "startupdialog.h"
#include "ui_startupdialog.h"

#include "common.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QSettings>
#include <QUrl>

static const QString last_source_dir_key = QStringLiteral("last_source_dir");
static const QString last_source_key     = QStringLiteral("last_source");
static const QString last_server_key     = QStringLiteral("last_server");
static const QString last_server_port    = QStringLiteral("last_server_port");

StartupDialog::StartupDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::StartupDialog) {
    ui->setupUi(this);

    QSettings settings;

    auto path = settings.value(last_source_key).toString();

    if (!path.isEmpty()) {
        ui->pathEdit->setText(path);
    }

    ui->historyTimeEdit->setMinimumTime(QTime(0, 0, 1, 0));

    auto tmax = QTime(0, 0).addSecs(30000 / resample_hz());

    ui->historyTimeEdit->setMaximumTime(tmax);
    ui->historyTimeEdit->setTime(QTime(0, 0, 10));
}

StartupDialog::~StartupDialog() { delete ui; }

QString StartupDialog::path_to_experiment() const {
    return ui->pathEdit->text();
}
QUrl StartupDialog::server_url() const {
    QUrl url = QUrl::fromUserInput(ui->serverBox->currentText());

    url.setPort(ui->portBox->value());

    return url;
}
bool StartupDialog::time_override() const {
    return ui->overrideTimeCheckBox->isChecked();
}

int StartupDialog::resample_hz() const {
    return ui->sampleRateSpinBox->value();
}

int StartupDialog::history_seconds() const {
    return ui->historyTimeEdit->time().msecsSinceStartOfDay() / 1000;
}

void StartupDialog::on_setPathButton_clicked() {
    QSettings settings;

    QString experiment_source = QFileDialog::getOpenFileName(
        this,
        "Experiment Source",
        settings.value(last_source_dir_key, QDir::homePath()).toString(),
        tr("Experiment File (*.json)"));

    if (!experiment_source.isEmpty()) {
        ui->pathEdit->setText(experiment_source);

        settings.setValue(last_source_dir_key,
                          QDir(experiment_source).absolutePath());

        settings.setValue(last_source_key, experiment_source);
    }
}

void StartupDialog::on_buttonBox_accepted() {
    QSettings settings;

    settings.setValue(last_server_key, ui->serverBox->currentText());
}

void StartupDialog::on_sampleRateSpinBox_valueChanged(int value) {
    auto tmax = QTime(0, 0).addSecs(30000 / value);

    ui->historyTimeEdit->setMaximumTime(tmax);
}

void StartupDialog::on_historyTimeEdit_userTimeChanged(QTime const& /*time*/) {}

void StartupDialog::on_openLogButton_clicked() {
    QUrl url(QString("file:///%1").arg(get_log_directory()));
    QDesktopServices::openUrl(url);
}
