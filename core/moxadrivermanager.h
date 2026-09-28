#ifndef MOXADRIVERMANAGER_H
#define MOXADRIVERMANAGER_H

#include <QObject>
#include <QString>

class ProcessRunner;

struct DriverStatus
{
    bool resourcesFound = false;
    bool headersAvailable = false;

    QString bundledVersion;
    QString installedVersion;
    bool moduleLoaded = false;
    bool kernelMatches = false;

    bool isInstalled() const { return !installedVersion.isEmpty(); }
};

class MoxaDriverManager : public QObject
{
    Q_OBJECT
public:
    explicit MoxaDriverManager(ProcessRunner *privilegedRunner, QObject *parent = nullptr);

    DriverStatus queryStatus() const;

    bool installOrUpdate();
    bool removeDriver();

    QString lastError() const { return m_lastError; }

private:
    ProcessRunner *m_runner;
    mutable QString m_lastError;

    bool startHelper(const QString &subCommand, const QStringList &extraArgs);
};

#endif
