#include "moxadrivermanager.h"

#include "processrunner.h"
#include "systeminfo.h"

#include <QFile>
#include <QTextStream>

namespace {

const char *const kModuleName = "mxu11x0";

QString parseBundledVersion(const QString &driverDir)
{
    QFile file(driverDir + "/mxu11x0/version.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString prefix = QStringLiteral("Version Number:");
        if (line.trimmed().startsWith(prefix))
            return line.trimmed().mid(prefix.size()).trimmed();
    }
    return {};
}

bool isModuleLoaded()
{
    const CapturedOutput out = runCapture("lsmod", {});
    for (const QString &line : out.stdOut.split('\n')) {
        if (line.startsWith(QString(kModuleName) + " "))
            return true;
    }
    return false;
}

}

MoxaDriverManager::MoxaDriverManager(ProcessRunner *privilegedRunner, QObject *parent)
    : QObject(parent)
    , m_runner(privilegedRunner)
{
}

DriverStatus MoxaDriverManager::queryStatus() const
{
    DriverStatus status;

    const OsInfo os = SystemInfo::detect();
    status.headersAvailable = os.kernelHeadersInstalled;

    const QString driverDir = SystemInfo::driverSourceDir();
    status.resourcesFound = !driverDir.isEmpty();
    if (status.resourcesFound)
        status.bundledVersion = parseBundledVersion(driverDir);

    const CapturedOutput versionOut = runCapture("modinfo", {"-F", "version", kModuleName});
    if (versionOut.exitCode == 0)
        status.installedVersion = versionOut.stdOut.trimmed();

    if (status.isInstalled()) {
        const CapturedOutput vermagicOut = runCapture("modinfo", {"-F", "vermagic", kModuleName});
        const QString vermagic = vermagicOut.stdOut.trimmed();
        const QString installedForKernel = vermagic.section(' ', 0, 0);
        status.kernelMatches = !installedForKernel.isEmpty() && installedForKernel == os.kernelRelease;
        status.moduleLoaded = isModuleLoaded();
    }

    return status;
}

bool MoxaDriverManager::startHelper(const QString &subCommand, const QStringList &extraArgs)
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

bool MoxaDriverManager::installOrUpdate()
{
    const QString driverDir = SystemInfo::driverSourceDir();
    if (driverDir.isEmpty()) {
        m_lastError = QStringLiteral("Gömülü sürücü kaynakları bulunamadı.");
        return false;
    }
    return startHelper(QStringLiteral("install"), {driverDir});
}

bool MoxaDriverManager::removeDriver()
{
    const QString driverDir = SystemInfo::driverSourceDir();
    if (driverDir.isEmpty()) {
        m_lastError = QStringLiteral("Gömülü sürücü kaynakları bulunamadı.");
        return false;
    }
    return startHelper(QStringLiteral("remove"), {driverDir});
}
