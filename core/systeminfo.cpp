#include "systeminfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QTextStream>

#ifndef MOXA_APP_ID
#define MOXA_APP_ID "moxauportmanager"
#endif

#ifndef MOXA_DEV_RESOURCE_DIR
#define MOXA_DEV_RESOURCE_DIR ""
#endif

namespace {

QString readOsReleaseField(const QString &key)
{
    QFile file("/etc/os-release");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.startsWith(key + "="))
            continue;
        QString value = line.mid(key.size() + 1).trimmed();
        if (value.startsWith('"') && value.endsWith('"'))
            value = value.mid(1, value.size() - 2);
        return value;
    }
    return {};
}

bool versionInRange(const QString &versionId, double minVersion, double maxVersion)
{
    bool ok = false;
    const double value = versionId.toDouble(&ok);
    if (!ok)
        return false;
    return value >= minVersion - 0.001 && value <= maxVersion + 0.001;
}

bool looksLikeResourceRoot(const QString &candidate)
{
    if (candidate.isEmpty())
        return false;
    return QFileInfo::exists(candidate + "/driver/mxu11x0/driver/Makefile")
        && QFileInfo::exists(candidate + "/scripts/moxa-helper.sh");
}

}

OsInfo SystemInfo::detect()
{
    OsInfo info;
    info.prettyName = readOsReleaseField("PRETTY_NAME");
    if (info.prettyName.isEmpty())
        info.prettyName = QStringLiteral("Bilinmeyen Linux dağıtımı");

    info.versionId = readOsReleaseField("VERSION_ID");
    info.versionInSupportedRange = versionInRange(info.versionId, 22.04, 24.04);

    info.kernelRelease = QSysInfo::kernelVersion();
    info.kernelHeadersInstalled = QDir("/lib/modules/" + info.kernelRelease + "/build").exists();

    return info;
}

QString SystemInfo::resolveResourceRoot()
{
    const QString envOverride = qEnvironmentVariable("MOXA_GUI_RESOURCE_DIR");
    if (looksLikeResourceRoot(envOverride))
        return envOverride;

    const QString installedRoot = QStringLiteral("/opt/%1").arg(MOXA_APP_ID);
    if (looksLikeResourceRoot(installedRoot))
        return installedRoot;

    const QString devRoot = QStringLiteral(MOXA_DEV_RESOURCE_DIR);
    if (looksLikeResourceRoot(devRoot))
        return devRoot;

    return {};
}

QString SystemInfo::driverSourceDir()
{
    const QString root = resolveResourceRoot();
    if (root.isEmpty())
        return {};
    return root + "/driver";
}

QString SystemInfo::helperScriptPath()
{
    const QString root = resolveResourceRoot();
    if (root.isEmpty())
        return {};
    return root + "/scripts/moxa-helper.sh";
}
