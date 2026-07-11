#pragma once

// BackendService — the GUI's single gateway to the MediaFusionGCV daemon.
//
// Responsibilities:
//   * daemon lifecycle: connect to MEDIAFUSION_CTL socket; if unreachable and
//     autostart is on, spawn `MediaFusionGCV --serve <socket>` (QProcess) and
//     retry with backoff. REBOOT_CORE / TERMINATE_PID map to restart()/shutdown().
//   * control protocol: a BackendWorker on a dedicated QThread owns the blocking
//     ControlClient; every command is serialized there, results come back as
//     queued signals — the GUI thread never blocks on a socket.
//   * session table: a GUI "stream session" = one fresh daemon pipeline
//     (create → set-device → algos → start). Stopping = stop + delete. The
//     daemon erases deleted pipelines from its vector so ids SHIFT; the worker
//     re-bases every mapped id after each delete, callers only see sessionIds.
//
// Threading rule: construct/use BackendService from the GUI thread only.

#include "DeviceParser.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QThread>

class ControlClient;

// Runs on the worker thread; owns the blocking socket client.
class BackendWorker : public QObject
{
    Q_OBJECT
public:
    ~BackendWorker() override;

public slots:
    void connectTo(const QString& socketPath);
    void disconnectCtl();
    void queryDevices();
    void queryAlgorithms();
    void deploy(int sessionId, int deviceIndex, int capIndex,
                const QString& algosCsv, bool screenSink, const QString& name);
    void stopSession(int sessionId);
    void stopAllSessions();
    void requestShutdown();                    // daemon 'shutdown'

signals:
    void connectedChanged(bool ok, const QString& socketPath);
    void devicesReady(bool ok, const QVector<DeviceInfo>& devices);
    void algorithmsReady(const QStringList& algorithms);
    void sessionStarted(int sessionId, const QString& videoSocket, const QString& description);
    void sessionStopped(int sessionId);
    void sessionFailed(int sessionId, const QString& error);
    void wire(int level, const QString& text); // protocol/log lines (AppLog::Level)

private:
    bool cmd(const QString& line, QString& reply);      // logs both directions
    long createPipeline(const QString& src, const QString& snk, const QString& name);
    void deletePipeline(long daemonId);

    ControlClient*        m_client = nullptr;           // created lazily on this thread
    QHash<int, long>      m_sessions;                   // sessionId → daemon pipeline id
    QHash<int, QString>   m_sessionSockets;             // sessionId → video socket path
};

class BackendService : public QObject
{
    Q_OBJECT
public:
    enum class DaemonState { Offline, Starting, Online };
    Q_ENUM(DaemonState)

    struct DeploySpec {
        int     deviceIndex = 0;
        int     capIndex    = 0;
        QString algosCsv;                       // "" = no processing
        bool    screenSink  = false;            // true: backend-side autovideosink
        QString name        = QStringLiteral("gui");
    };

    explicit BackendService(QObject* parent = nullptr);
    ~BackendService() override;

    // configuration (persisted by SettingsPage via QSettings)
    QString controlSocketPath() const { return m_socketPath; }
    QString backendBinary() const     { return m_binary; }
    bool    autostart() const         { return m_autostart; }
    void    setControlSocketPath(const QString& p) { m_socketPath = p; }
    void    setBackendBinary(const QString& b)     { m_binary = b; }
    void    setAutostart(bool on)                  { m_autostart = on; }

    DaemonState state() const { return m_state; }
    qint64      daemonPid() const;

    const QVector<DeviceInfo>& devices() const   { return m_devices; }
    const QStringList& algorithms() const        { return m_algorithms; }

    // async operations (results via signals)
    void ensureOnline();                        // connect, spawning if allowed
    void restartDaemon();                       // REBOOT_CORE
    void shutdownDaemon();                      // TERMINATE_PID
    void refreshDevices();
    void refreshAlgorithms();
    int  deploy(const DeploySpec& spec);        // returns sessionId
    void stop(int sessionId);
    void stopAll();

    static QString defaultSocketPath();
    static QString defaultBinaryPath();

signals:
    void daemonStateChanged(BackendService::DaemonState state);
    void devicesChanged(const QVector<DeviceInfo>& devices);
    void algorithmsChanged(const QStringList& algorithms);
    void sessionStarted(int sessionId, const QString& videoSocket, const QString& description);
    void sessionStopped(int sessionId);
    void sessionFailed(int sessionId, const QString& error);

private slots:
    void onWorkerConnected(bool ok, const QString& socketPath);
    void onDevicesReady(bool ok, const QVector<DeviceInfo>& devices);
    void onWire(int level, const QString& text);
    void onProcessStateChanged(QProcess::ProcessState st);
    void onProcessOutput();

private:
    void setState(DaemonState st);
    void spawnDaemon();

    QThread        m_thread;
    BackendWorker* m_worker = nullptr;
    QProcess*      m_process = nullptr;

    QString     m_socketPath;
    QString     m_binary;
    bool        m_autostart   = true;
    DaemonState m_state       = DaemonState::Offline;
    int         m_nextSession = 1;
    int         m_connectAttempts = 0;
    bool        m_restartPending  = false;

    QVector<DeviceInfo> m_devices;
    QStringList         m_algorithms;
};
