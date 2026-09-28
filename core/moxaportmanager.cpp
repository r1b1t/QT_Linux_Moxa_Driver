#include "moxaportmanager.h"

#include "processrunner.h"
#include "systeminfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace {

const char *const kUdevRulePath = "/etc/udev/rules.d/99-moxa-uport-rs485.rules";

QString symlinkTargetBaseName(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists())
        return {};
    const QString target = info.symLinkTarget();
    return target.isEmpty() ? QString() : QFileInfo(target).fileName();
}

bool findUsbIds(const QString &ttyDeviceLink, QString *idVendor, QString *idProduct)
{
    QFileInfo linkInfo(ttyDeviceLink);
    if (!linkInfo.exists())
        return false;

    QDir dir(linkInfo.canonicalFilePath());
    for (int i = 0; i < 6 && dir.exists(); ++i) {
        QFile vendorFile(dir.filePath("idVendor"));
        QFile productFile(dir.filePath("idProduct"));
        if (vendorFile.open(QIODevice::ReadOnly) && productFile.open(QIODevice::ReadOnly)) {
            *idVendor = QString::fromUtf8(vendorFile.readAll()).trimmed().toLower();
            *idProduct = QString::fromUtf8(productFile.readAll()).trimmed().toLower();
            return true;
        }
        if (!dir.cdUp())
            break;
    }
    return false;
}

int parseSetserialModeValue(const QString &output)
{
    static const QRegularExpression re(QStringLiteral("port:?\\s+0x([0-9a-fA-F]+)"),
                                        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(output);
    if (!match.hasMatch())
        return -1;
    bool ok = false;
    const int value = match.captured(1).toInt(&ok, 16);
    return ok ? value : -1;
}

}

QStringList moxaModeNames()
{
    return {QStringLiteral("RS-232"), QStringLiteral("RS-485 2 telli (2W)"),
            QStringLiteral("RS-422"), QStringLiteral("RS-485 4 telli (4W)")};
}

QString moxaModeName(int modeValue)
{
    const QStringList names = moxaModeNames();
    if (modeValue >= 0 && modeValue < names.size())
        return names.at(modeValue);
    return QStringLiteral("Bilinmiyor");
}

MoxaPortManager::MoxaPortManager(ProcessRunner *privilegedRunner, QObject *parent)
    : QObject(parent)
    , m_runner(privilegedRunner)
{
}

QVector<MoxaPortInfo> MoxaPortManager::listPorts() const
{
    QVector<MoxaPortInfo> ports;

    QDir devDir("/dev");
    const QStringList names = devDir.entryList({"ttyUSB*"}, QDir::System, QDir::Name);

    for (const QString &name : names) {
        MoxaPortInfo port;
        port.devicePath = "/dev/" + name;
        port.driverName = symlinkTargetBaseName("/sys/class/tty/" + name + "/device/driver");
        findUsbIds("/sys/class/tty/" + name + "/device", &port.idVendor, &port.idProduct);

        if (port.isMoxaManaged()) {
            const CapturedOutput out = runCapture("setserial", {"-G", port.devicePath});
            if (out.exitCode == 0)
                port.modeValue = parseSetserialModeValue(out.stdOut);
        }

        ports.append(port);
    }

    return ports;
}

UdevRuleInfo MoxaPortManager::queryUdevRule() const
{
    UdevRuleInfo info;

    QFile file(kUdevRulePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    const QString contents = QString::fromUtf8(file.readAll());
    static const QRegularExpression re(
        "idVendor\\}==\"([0-9a-fA-F]{4})\", ATTRS\\{idProduct\\}==\"([0-9a-fA-F]{4})\".*port (\\d)");
    const QRegularExpressionMatch match = re.match(contents);
    if (!match.hasMatch())
        return info;

    info.present = true;
    info.idVendor = match.captured(1).toLower();
    info.idProduct = match.captured(2).toLower();
    info.modeValue = match.captured(3).toInt();
    return info;
}

bool MoxaPortManager::startHelper(const QString &subCommand, const QStringList &extraArgs)
{
    if (m_runner->isRunning()) {
        m_lastError = QStringLiteral("Başka bir işlem sürüyor, lütfen bekleyin.");
        return false;
    }

    const QString script = SystemInfo::helperScriptPath();
    if (script.isEmpty()) {
        m_lastError = QStringLiteral("Gömülü sürücü kaynakları bulunamadı.");
        return false;
    }

    QStringList args{script, subCommand};
    args += extraArgs;
    return m_runner->start(QStringLiteral("pkexec"), args);
}

bool MoxaPortManager::applyMode(const MoxaPortInfo &port, int modeValue)
{
    if (modeValue < 0 || modeValue > 3) {
        m_lastError = QStringLiteral("Geçersiz mod değeri.");
        return false;
    }
    return startHelper(QStringLiteral("set-mode"), {port.devicePath, QString::number(modeValue)});
}

bool MoxaPortManager::installUdevRule(const QString &idVendor, const QString &idProduct, int modeValue)
{
    if (modeValue < 0 || modeValue > 3) {
        m_lastError = QStringLiteral("Geçersiz mod değeri.");
        return false;
    }
    return startHelper(QStringLiteral("install-udev"), {idVendor, idProduct, QString::number(modeValue)});
}

bool MoxaPortManager::removeUdevRule()
{
    return startHelper(QStringLiteral("remove-udev"), {});
}
