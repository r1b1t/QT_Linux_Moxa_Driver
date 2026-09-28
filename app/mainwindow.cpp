#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "moxadrivermanager.h"
#include "moxaportmanager.h"
#include "processrunner.h"
#include "systeminfo.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

#ifndef MOXA_APP_NAME
#define MOXA_APP_NAME "Moxa UPort Manager"
#endif
#ifndef MOXA_APP_VERSION
#define MOXA_APP_VERSION "1.0.0"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral(MOXA_APP_NAME " " MOXA_APP_VERSION));
    ui->verticalLayout->setStretch(0, 2);
    ui->verticalLayout->setStretch(1, 1);

    ui->portTable->horizontalHeader()->setStretchLastSection(true);
    ui->portTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->portTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->portTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->modeCombo->addItems(moxaModeNames());

    m_runner = new ProcessRunner(this);
    connect(m_runner, &ProcessRunner::outputLine, this, &MainWindow::onRunnerOutputLine);
    connect(m_runner, &ProcessRunner::finished, this, &MainWindow::onRunnerFinished);
    connect(m_runner, &ProcessRunner::failedToStart, this, &MainWindow::onRunnerFailedToStart);

    m_driverManager = new MoxaDriverManager(m_runner, this);
    m_portManager = new MoxaPortManager(m_runner, this);

    connect(ui->btnQuery, &QPushButton::clicked, this, &MainWindow::onQueryDriverClicked);
    connect(ui->btnInstall, &QPushButton::clicked, this, &MainWindow::onInstallOrUpdateClicked);
    connect(ui->btnRemove, &QPushButton::clicked, this, &MainWindow::onRemoveDriverClicked);
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    connect(ui->btnApplyMode, &QPushButton::clicked, this, &MainWindow::onApplyModeClicked);
    connect(ui->chkPersist, &QCheckBox::toggled, this, &MainWindow::onPersistCheckboxToggled);
    connect(ui->portTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onPortSelectionChanged);

    statusBar()->showMessage(QStringLiteral("Hazır"));

    refreshDriverStatus();
    refreshPorts();
    refreshUdevRuleDisplay();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::refreshDriverStatus()
{
    const OsInfo os = SystemInfo::detect();
    ui->lblDistro->setText(os.prettyName);
    ui->lblKernel->setText(os.kernelRelease
        + (os.kernelHeadersInstalled ? QStringLiteral(" (başlık dosyaları kurulu)")
                                      : QStringLiteral(" (başlık dosyaları eksik — kurulumda otomatik kurulacak)")));
    ui->lblCompat->setText(os.versionInSupportedRange
        ? QStringLiteral("Desteklenen sürüm aralığında (Ubuntu 22.04–24.04)")
        : QStringLiteral("Bu Ubuntu sürümü (%1) test aralığının dışında — kurulum yine de denenebilir").arg(os.versionId));

    const DriverStatus st = m_driverManager->queryStatus();

    if (!st.resourcesFound) {
        ui->lblBundledVersion->setText(QStringLiteral("BULUNAMADI"));
        ui->lblInstalledVersion->setText(QStringLiteral("-"));
        ui->lblModuleLoaded->setText(QStringLiteral("-"));
        ui->lblKernelMatch->setText(QStringLiteral("-"));
        ui->btnInstall->setEnabled(false);
        ui->btnRemove->setEnabled(false);
        statusBar()->showMessage(QStringLiteral("Gömülü sürücü kaynakları bulunamadı!"), 8000);
        return;
    }

    ui->lblBundledVersion->setText(st.bundledVersion.isEmpty() ? QStringLiteral("bilinmiyor") : st.bundledVersion);

    if (st.isInstalled()) {
        ui->lblInstalledVersion->setText(st.installedVersion);
        ui->lblModuleLoaded->setText(st.moduleLoaded
            ? QStringLiteral("Yüklü (aktif)")
            : QStringLiteral("Kurulu ama şu an yüklenmemiş (modprobe mxu11x0 ile yüklenebilir)"));
        ui->lblKernelMatch->setText(st.kernelMatches
            ? QStringLiteral("Çalışan çekirdekle uyumlu")
            : QStringLiteral("UYUMSUZ — çekirdek güncellenmiş, yeniden derleme gerekiyor"));
        ui->btnInstall->setText(!st.kernelMatches || st.installedVersion != st.bundledVersion
            ? QStringLiteral("Güncelle") : QStringLiteral("Yeniden Kur"));
    } else {
        ui->lblInstalledVersion->setText(QStringLiteral("Kurulu değil"));
        ui->lblModuleLoaded->setText(QStringLiteral("-"));
        ui->lblKernelMatch->setText(QStringLiteral("-"));
        ui->btnInstall->setText(QStringLiteral("Kur"));
    }

    ui->btnInstall->setEnabled(true);
    ui->btnRemove->setEnabled(st.isInstalled());
}

void MainWindow::refreshPorts()
{
    m_currentPorts = m_portManager->listPorts();
    ui->portTable->setRowCount(m_currentPorts.size());

    for (int i = 0; i < m_currentPorts.size(); ++i) {
        const MoxaPortInfo &port = m_currentPorts.at(i);
        ui->portTable->setItem(i, 0, new QTableWidgetItem(port.devicePath));
        ui->portTable->setItem(i, 1, new QTableWidgetItem(port.driverName.isEmpty() ? QStringLiteral("-") : port.driverName));
        ui->portTable->setItem(i, 2, new QTableWidgetItem(
            port.idVendor.isEmpty() ? QStringLiteral("-") : (port.idVendor + ":" + port.idProduct)));

        QString modeText;
        if (!port.isMoxaManaged())
            modeText = QStringLiteral("Desteklenmiyor (genel sürücü kullanılıyor)");
        else if (port.modeValue < 0)
            modeText = QStringLiteral("Bilinmiyor");
        else
            modeText = moxaModeName(port.modeValue);
        ui->portTable->setItem(i, 3, new QTableWidgetItem(modeText));
    }

    ui->portTable->resizeColumnsToContents();

    if (m_currentPorts.isEmpty())
        statusBar()->showMessage(QStringLiteral("Bağlı Moxa USB seri portu bulunamadı."), 5000);

    onPortSelectionChanged();
}

void MainWindow::refreshUdevRuleDisplay()
{
    const UdevRuleInfo info = m_portManager->queryUdevRule();

    m_updatingPersistCheckbox = true;
    ui->chkPersist->setChecked(info.present);
    m_updatingPersistCheckbox = false;

    ui->lblUdevStatus->setText(info.present
        ? QStringLiteral("Kalıcı kural aktif: %1:%2 → %3").arg(info.idVendor, info.idProduct, moxaModeName(info.modeValue))
        : QStringLiteral("Kalıcı kural yok — aygıt her takıldığında/açılışta RS-232'ye döner"));
}

void MainWindow::setBusy(bool busy)
{
    ui->btnInstall->setDisabled(busy);
    ui->btnRemove->setDisabled(busy);
    ui->btnApplyMode->setDisabled(busy);
    ui->chkPersist->setDisabled(busy);
}

void MainWindow::beginOperation(PendingOp op, const QString &statusMessage)
{
    m_pendingOp = op;
    ui->logView->appendPlainText(QStringLiteral("=== %1 ===").arg(statusMessage));
    statusBar()->showMessage(statusMessage);
    setBusy(true);
}

void MainWindow::abortPendingOperation(const QString &error)
{
    m_pendingOp = PendingOp::None;
    setBusy(false);
    statusBar()->showMessage(QStringLiteral("Hazır"));
    QMessageBox::warning(this, QStringLiteral("İşlem başlatılamadı"), error);
}

void MainWindow::onQueryDriverClicked()
{
    refreshDriverStatus();
    statusBar()->showMessage(QStringLiteral("Durum güncellendi."), 3000);
}

void MainWindow::onInstallOrUpdateClicked()
{
    beginOperation(PendingOp::DriverInstall,
                   QStringLiteral("Sürücü kuruluyor/güncelleniyor (yönetici izni gerekebilir)..."));
    if (!m_driverManager->installOrUpdate())
        abortPendingOperation(m_driverManager->lastError());
}

void MainWindow::onRemoveDriverClicked()
{
    const auto answer = QMessageBox::question(this, QStringLiteral("Sürücüyü Kaldır"),
        QStringLiteral("Moxa mxu11x0 sürücüsünü sistemden kaldırmak istediğinize emin misiniz?\n\n"
                        "Cihaz bir sonraki takılışında Ubuntu'nun genel sürücüsünü kullanacak ve "
                        "RS-485/422 mod değişimi çalışmayacaktır."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    beginOperation(PendingOp::DriverRemove, QStringLiteral("Sürücü kaldırılıyor (yönetici izni gerekebilir)..."));
    if (!m_driverManager->removeDriver())
        abortPendingOperation(m_driverManager->lastError());
}

void MainWindow::onRefreshPortsClicked()
{
    refreshPorts();
    refreshUdevRuleDisplay();
    statusBar()->showMessage(QStringLiteral("Port listesi güncellendi."), 3000);
}

void MainWindow::onApplyModeClicked()
{
    const int row = ui->portTable->currentRow();
    if (row < 0 || row >= m_currentPorts.size()) {
        QMessageBox::information(this, QStringLiteral("Port seçilmedi"), QStringLiteral("Önce tablodan bir port seçin."));
        return;
    }

    const MoxaPortInfo &port = m_currentPorts.at(row);
    if (!port.isMoxaManaged()) {
        QMessageBox::warning(this, QStringLiteral("Desteklenmiyor"),
            QStringLiteral("Bu port Moxa mxu11x0 sürücüsüne bağlı değil. Mod değiştirmeden önce "
                            "'Sürücü Yönetimi' sekmesinden Moxa sürücüsünü kurun."));
        return;
    }

    const int mode = ui->modeCombo->currentIndex();
    beginOperation(PendingOp::PortSetMode,
        QStringLiteral("%1 için mod uygulanıyor: %2 (yönetici izni gerekebilir)...").arg(port.devicePath, moxaModeName(mode)));
    if (!m_portManager->applyMode(port, mode))
        abortPendingOperation(m_portManager->lastError());
}

void MainWindow::onPersistCheckboxToggled(bool checked)
{
    if (m_updatingPersistCheckbox)
        return;

    if (checked) {
        const int row = ui->portTable->currentRow();
        if (row < 0 || row >= m_currentPorts.size() || !m_currentPorts.at(row).isMoxaManaged()) {
            QMessageBox::warning(this, QStringLiteral("Port seçilmedi"),
                QStringLiteral("Kalıcı kural oluşturmak için önce listeden Moxa sürücüsüne bağlı bir port seçin."));
            m_updatingPersistCheckbox = true;
            ui->chkPersist->setChecked(false);
            m_updatingPersistCheckbox = false;
            return;
        }

        const MoxaPortInfo &port = m_currentPorts.at(row);
        const int mode = ui->modeCombo->currentIndex();
        beginOperation(PendingOp::UdevInstall,
            QStringLiteral("Kalıcı udev kuralı yazılıyor (%1:%2 → %3)...")
                .arg(port.idVendor, port.idProduct, moxaModeName(mode)));
        if (!m_portManager->installUdevRule(port.idVendor, port.idProduct, mode)) {
            abortPendingOperation(m_portManager->lastError());
            m_updatingPersistCheckbox = true;
            ui->chkPersist->setChecked(false);
            m_updatingPersistCheckbox = false;
        }
    } else {
        beginOperation(PendingOp::UdevRemove, QStringLiteral("Kalıcı udev kuralı kaldırılıyor..."));
        if (!m_portManager->removeUdevRule())
            abortPendingOperation(m_portManager->lastError());
    }
}

void MainWindow::onPortSelectionChanged()
{
    const int row = ui->portTable->currentRow();
    const bool moxa = row >= 0 && row < m_currentPorts.size() && m_currentPorts.at(row).isMoxaManaged();
    const bool busy = m_runner->isRunning();
    ui->btnApplyMode->setEnabled(moxa && !busy);
    ui->chkPersist->setEnabled(moxa && !busy);
}

void MainWindow::onRunnerOutputLine(const QString &line)
{
    ui->logView->appendPlainText(line);
}

void MainWindow::onRunnerFinished(int exitCode, QProcess::ExitStatus status)
{
    const PendingOp op = m_pendingOp;
    m_pendingOp = PendingOp::None;
    setBusy(false);

    const bool success = (status == QProcess::NormalExit && exitCode == 0);
    ui->logView->appendPlainText(success
        ? QStringLiteral("=== İşlem tamamlandı ===")
        : QStringLiteral("=== İşlem başarısız (çıkış kodu %1) ===").arg(exitCode));

    refreshDriverStatus();
    refreshPorts();
    refreshUdevRuleDisplay();

    if (success) {
        statusBar()->showMessage(QStringLiteral("İşlem tamamlandı."), 5000);
        return;
    }

    QString opName;
    switch (op) {
    case PendingOp::DriverInstall: opName = QStringLiteral("Sürücü kurulumu/güncellemesi"); break;
    case PendingOp::DriverRemove:  opName = QStringLiteral("Sürücü kaldırma"); break;
    case PendingOp::PortSetMode:   opName = QStringLiteral("Mod uygulama"); break;
    case PendingOp::UdevInstall:   opName = QStringLiteral("Kalıcı kural yazma"); break;
    case PendingOp::UdevRemove:    opName = QStringLiteral("Kalıcı kural kaldırma"); break;
    default:                       opName = QStringLiteral("İşlem"); break;
    }

    statusBar()->showMessage(opName + QStringLiteral(" başarısız oldu."), 8000);
    QMessageBox::warning(this, opName + QStringLiteral(" Başarısız"),
        opName + QStringLiteral(" başarısız oldu (çıkış kodu %1).\n\n"
                                 "Yönetici kimlik doğrulama penceresini iptal etmiş olabilirsiniz. "
                                 "Ayrıntılar için aşağıdaki İşlem Günlüğü'ne bakın.").arg(exitCode));
}

void MainWindow::onRunnerFailedToStart(const QString &errorString)
{
    m_pendingOp = PendingOp::None;
    setBusy(false);
    ui->logView->appendPlainText(QStringLiteral("=== Başlatılamadı: %1 ===").arg(errorString));
    statusBar()->showMessage(QStringLiteral("Hazır"));
    QMessageBox::critical(this, QStringLiteral("Başlatılamadı"),
        QStringLiteral("İşlem başlatılamadı: %1\n(pkexec kurulu mu?)").arg(errorString));
}
