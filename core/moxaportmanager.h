#ifndef MOXAPORTMANAGER_H
#define MOXAPORTMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

class ProcessRunner;

QStringList moxaModeNames();
QString moxaModeName(int modeValue);

struct MoxaPortInfo
{
    QString devicePath;
    QString driverName;
    QString idVendor;
    QString idProduct;
    int modeValue = -1;

    bool isMoxaManaged() const { return driverName.startsWith("mxu"); }
};

struct UdevRuleInfo
{
    bool present = false;
    QString idVendor;
    QString idProduct;
    int modeValue = -1;
};

class MoxaPortManager : public QObject
{
    Q_OBJECT
public:
    explicit MoxaPortManager(ProcessRunner *privilegedRunner, QObject *parent = nullptr);

    QVector<MoxaPortInfo> listPorts() const;
    UdevRuleInfo queryUdevRule() const;

    bool applyMode(const MoxaPortInfo &port, int modeValue);
    bool installUdevRule(const QString &idVendor, const QString &idProduct, int modeValue);
    bool removeUdevRule();

    QString lastError() const { return m_lastError; }

private:
    ProcessRunner *m_runner;
    mutable QString m_lastError;

    bool startHelper(const QString &subCommand, const QStringList &extraArgs);
};

#endif
