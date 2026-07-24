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
#include "InferenceTypes.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QThread>

class ControlClient;
class QTimer;

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
    void queryModels();
    void queryAccelerators();
    void deploy(int sessionId, int deviceIndex, int capIndex,
                const QString& algosCsv, bool screenSink, const QString& name,
                const QString& detectorModel, double confidence, double nms, bool drawBoxes,
                const QString& accelSelection);
    void applyDetector(int sessionId, const QString& model,
                       double confidence, double nms, bool drawBoxes);
    void pollStats();                          // one `stats` per live session
    void stopSession(int sessionId);
    void stopAllSessions();
    void requestShutdown();                    // daemon 'shutdown'

signals:
    void connectedChanged(bool ok, const QString& socketPath);
    void devicesReady(bool ok, const QVector<DeviceInfo>& devices);
    void algorithmsReady(const QStringList& algorithms);
    void modelsReady(const QVector<DetectorModel>& models);
    void acceleratorsReady(const QVector<AcceleratorOption>& accelerators);
    void detectorApplied(int sessionId, bool ok, const QString& detail);
    void inferenceStats(int sessionId, const InferenceSnapshot& snapshot);
    void sessionStarted(int sessionId, const QString& videoSocket, const QString& description);
    void sessionStopped(int sessionId);
    void sessionFailed(int sessionId, const QString& error);
    void wire(int level, const QString& text); // protocol/log lines (AppLog::Level)

private:
    // quiet=true keeps the 1 Hz stats poll out of the event log, which would
    // otherwise bury every other entry.
    bool cmd(const QString& line, QString& reply, bool quiet = false);
    long createPipeline(const QString& src, const QString& snk, const QString& name);
    void deletePipeline(long daemonId);
    bool sendDetector(long daemonId, const QString& model,
                      double confidence, double nms, bool drawBoxes, QString& detail);

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

        // Inference stage. The model is loaded before `algos` runs, so a
        // chain containing "detect" starts detecting on its first frame.
        QString detectorModel;                  // "" = stage stays idle
        double  confidence  = 0.25;             // YOLOv5's own default
        double  nms         = 0.45;
        bool    drawBoxes   = true;

        // Acceleration backend for this session: "auto"/"cpu"/"vulkan"/"cuda".
        // Sent as `accel <id> <sel>` before start; the daemon resolves AUTO and
        // falls back to CPU for anything unavailable, so a stale pick never fails.
        QString accelSelection = QStringLiteral("auto");
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
    QString accelSelection() const                 { return m_accelSelection; }
    void    setAccelSelection(const QString& s) {
        if (m_accelSelection == s) return;
        m_accelSelection = s;
        emit accelSelectionChanged(s);             // keep the Settings radios + Dashboard toggle in sync
    }

    DaemonState state() const { return m_state; }
    qint64      daemonPid() const;

    const QVector<DeviceInfo>& devices() const      { return m_devices; }
    const QStringList& algorithms() const           { return m_algorithms; }
    const QVector<DetectorModel>& models() const    { return m_models; }
    const QVector<AcceleratorOption>& accelerators() const { return m_accelerators; }

    // async operations (results via signals)
    void ensureOnline();                        // connect, spawning if allowed
    void restartDaemon();                       // REBOOT_CORE
    void shutdownDaemon();                      // TERMINATE_PID
    void refreshDevices();
    void refreshAlgorithms();
    void refreshModels();
    void refreshAccelerators();
    int  deploy(const DeploySpec& spec);        // returns sessionId
    void stop(int sessionId);
    void stopAll();

    // Change model or thresholds on a running session (takes effect within a
    // frame or two — the detector reloads off the streaming thread).
    void setDetector(int sessionId, const QString& model,
                     double confidence, double nms, bool drawBoxes);

    static QString defaultSocketPath();
    static QString defaultBinaryPath();

signals:
    void daemonStateChanged(BackendService::DaemonState state);
    void devicesChanged(const QVector<DeviceInfo>& devices);
    void algorithmsChanged(const QStringList& algorithms);
    void sessionStarted(int sessionId, const QString& videoSocket, const QString& description);
    void sessionStopped(int sessionId);
    void sessionFailed(int sessionId, const QString& error);
    void modelsChanged(const QVector<DetectorModel>& models);
    void acceleratorsChanged(const QVector<AcceleratorOption>& accelerators);
    void accelSelectionChanged(const QString& selection);
    void detectorChanged(int sessionId, bool ok, const QString& detail);
    void inferenceStatsChanged(int sessionId, const InferenceSnapshot& snapshot);

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
    QTimer*        m_statsTimer = nullptr;

    QString     m_socketPath;
    QString     m_binary;
    bool        m_autostart   = true;
    DaemonState m_state       = DaemonState::Offline;
    int         m_nextSession = 1;
    int         m_connectAttempts = 0;
    bool        m_restartPending  = false;

    QVector<DeviceInfo>    m_devices;
    QStringList            m_algorithms;
    QVector<DetectorModel> m_models;
    QVector<AcceleratorOption> m_accelerators;
    QString                m_accelSelection = QStringLiteral("auto");
    QHash<int, QString>    m_lastDetections;   // sessionId → last logged label set
};
