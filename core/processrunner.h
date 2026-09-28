#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

struct CapturedOutput
{
    int exitCode = -1;
    bool startFailed = false;
    QString stdOut;
    QString stdErr;
};

CapturedOutput runCapture(const QString &program, const QStringList &args, int timeoutMs = 5000);

class ProcessRunner : public QObject
{
    Q_OBJECT
public:
    explicit ProcessRunner(QObject *parent = nullptr);

    bool start(const QString &program, const QStringList &args);
    bool isRunning() const;

signals:
    void outputLine(const QString &line);
    void finished(int exitCode, QProcess::ExitStatus status);
    void failedToStart(const QString &errorString);

private:
    QProcess *m_process;
    QString m_pendingLine;

    void handleReadyRead();
    void flushPendingLine();
};

#endif
