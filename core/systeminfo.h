#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QString>

struct OsInfo
{
    QString prettyName;
    QString versionId;
    QString kernelRelease;
    bool versionInSupportedRange = false;
    bool kernelHeadersInstalled = false;
};

namespace SystemInfo {

OsInfo detect();

QString resolveResourceRoot();

QString driverSourceDir();
QString helperScriptPath();

}

#endif
