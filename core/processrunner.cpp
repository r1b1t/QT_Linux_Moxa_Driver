#include "processrunner.h"

#include <QEventLoop>
#include <QTimer>

CapturedOutput runCapture(const QString &program, const QStringList &args, int timeoutMs)
{
    CapturedOutput result;

    QProcess process;
    process.start(program, args);

    if (!process.waitForStarted(timeoutMs)) {
        result.startFailed = true;
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
    }

    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    return result;
}

ProcessRunner::ProcessRunner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessRunner::handleReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process->state() == QProcess::NotRunning) {
            emit failedToStart(m_process->errorString());
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
                flushPendingLine();
                emit finished(exitCode, status);
            });
}

bool ProcessRunner::start(const QString &program, const QStringList &args)
{
    if (isRunning())
        return false;

    m_pendingLine.clear();
    m_process->start(program, args);
    return true;
}

bool ProcessRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void ProcessRunner::handleReadyRead()
{
    m_pendingLine += QString::fromUtf8(m_process->readAllStandardOutput());

    int newlineIndex;
    while ((newlineIndex = m_pendingLine.indexOf('\n')) != -1) {
        QString line = m_pendingLine.left(newlineIndex);
        if (line.endsWith('\r'))
            line.chop(1);
        emit outputLine(line);
        m_pendingLine.remove(0, newlineIndex + 1);
    }
}

void ProcessRunner::flushPendingLine()
{
    if (!m_pendingLine.isEmpty()) {
        emit outputLine(m_pendingLine);
        m_pendingLine.clear();
    }
}
