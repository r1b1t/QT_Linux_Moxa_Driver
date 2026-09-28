#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "moxaportmanager.h"

#include <QMainWindow>
#include <QProcess>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ProcessRunner;
class MoxaDriverManager;
class MoxaPortManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onQueryDriverClicked();
    void onInstallOrUpdateClicked();
    void onRemoveDriverClicked();

    void onRefreshPortsClicked();
    void onApplyModeClicked();
    void onPersistCheckboxToggled(bool checked);
    void onPortSelectionChanged();

    void onRunnerOutputLine(const QString &line);
    void onRunnerFinished(int exitCode, QProcess::ExitStatus status);
    void onRunnerFailedToStart(const QString &errorString);

private:
    enum class PendingOp {
        None,
        DriverInstall,
        DriverRemove,
        PortSetMode,
        UdevInstall,
        UdevRemove,
    };

    void refreshDriverStatus();
    void refreshPorts();
    void refreshUdevRuleDisplay();

    void setBusy(bool busy);
    void beginOperation(PendingOp op, const QString &statusMessage);
    void abortPendingOperation(const QString &error);

    Ui::MainWindow *ui;

    ProcessRunner *m_runner = nullptr;
    MoxaDriverManager *m_driverManager = nullptr;
    MoxaPortManager *m_portManager = nullptr;

    PendingOp m_pendingOp = PendingOp::None;
    QVector<MoxaPortInfo> m_currentPorts;
    bool m_updatingPersistCheckbox = false;
};
#endif
