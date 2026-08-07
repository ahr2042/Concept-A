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
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QThread>
#include <QVariantMap>

class ControlClient;
class QTimer;

// Tuning for a whole chain: algorithm name → its parameter values. Carried in a
// DeploySpec so the operator's settings are applied with the chain rather than
// after it, and re-sent when a control moves on a live session.
using AlgorithmSettings = QMap<QString, QVariantMap>;

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
                const QString& accelSelection, const AlgorithmSettings& algoParams);
    void applyAlgorithmParams(int sessionId, const QString& algo, const QVariantMap& values);
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
    void algorithmParamsReady(const QString& algo, const QString& summary,
                              const QVector<AlgorithmParamSpec>& params);
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

        // Per-algorithm tuning, applied before `algos` so a stage starts on the
        // operator's values rather than its defaults. Algorithms absent from the
        // map simply keep their defaults.
        AlgorithmSettings algoParams;

        // Acceleration backend for this session: "auto"/"cpu"/"vulkan"/"cuda".
        // Sent as `accel <id> <sel>` before start; the daemon resolves AUTO and
        // falls back to CPU for anything unavailable, so a stale pick never fails.
        QString accelSelection = QStringLiteral("auto");
    };

    explicit BackendService(QObject* parent = nullptr);
    ~BackendService() override;

    // ── The working configuration ─────────────────────────────────────────
    //
    // The pipeline the operator is currently assembling. This used to live in
    // each page's widget tree, which is why the same controls existed on two
    // pages at once — and why the two deploy paths could quietly disagree: the
    // Dashboard never sent the acceleration choice and the Pipeline page never
    // sent the confidence.
    //
    // One document now, with each surface editing its own slice: the SideRail
    // owns the source, the Pipeline page owns the chain and the inference
    // settings, and the Dashboard only reads it (to summarise the chain, and to
    // START what Pipeline configured). `name` and `screenSink` are not part of
    // it — they belong to an individual deploy, and MultiGrid still builds its
    // own spec per tile because each tile is its own pipeline.
    const DeploySpec& config() const { return m_config; }

    void setSource(int deviceIndex, int capIndex);
    void setChain(const QString& algosCsv, const AlgorithmSettings& params);
    void setDetectorSettings(const QString& model, double confidence,
                             double nms, bool drawBoxes);

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
        // Also the config's, so there is one source of truth for what the next
        // deploy will ask for. Both signals fire: the Settings radios listen for
        // the specific one, the pages for the general one.
        m_config.accelSelection = s;
        emit accelSelectionChanged(s);             // keep the Settings radios + Dashboard toggle in sync
        emit configChanged(m_config);
    }

    DaemonState state() const { return m_state; }
    qint64      daemonPid() const;

    const QVector<DeviceInfo>& devices() const      { return m_devices; }
    const QStringList& algorithms() const           { return m_algorithms; }

    // Schema for one algorithm, empty until the daemon has answered (the
    // schemas are fetched once, right after `algos-list`) or if it has no knobs.
    QVector<AlgorithmParamSpec> algorithmParams(const QString& algo) const
    { return m_algorithmParams.value(algo); }

    // One-line description of what the stage does, for a control's tooltip.
    QString algorithmSummary(const QString& algo) const
    { return m_algorithmSummaries.value(algo); }

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

    // Retune an algorithm on a running session. Values are clamped by the
    // engine, so a control can send whatever its widget produces.
    void setAlgorithmParams(int sessionId, const QString& algo, const QVariantMap& values);

    static QString defaultSocketPath();
    static QString defaultBinaryPath();

signals:
    void daemonStateChanged(BackendService::DaemonState state);
    void devicesChanged(const QVector<DeviceInfo>& devices);
    void algorithmsChanged(const QStringList& algorithms);
    // Emitted once per algorithm as its schema arrives; pages build controls here.
    void algorithmParamsChanged(const QString& algo, const QVector<AlgorithmParamSpec>& params);
    void sessionStarted(int sessionId, const QString& videoSocket, const QString& description);
    void sessionStopped(int sessionId);
    void sessionFailed(int sessionId, const QString& error);
    void modelsChanged(const QVector<DetectorModel>& models);
    void acceleratorsChanged(const QVector<AcceleratorOption>& accelerators);
    void accelSelectionChanged(const QString& selection);
    // The working config changed, whoever changed it. Surfaces that show part of
    // it (the rail's combos, the Dashboard's chain summary) refresh from here.
    void configChanged(const BackendService::DeploySpec& config);
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
    QHash<QString, QVector<AlgorithmParamSpec>> m_algorithmParams;
    QHash<QString, QString>                     m_algorithmSummaries;
    QVector<DetectorModel> m_models;
    QVector<AcceleratorOption> m_accelerators;
    QString                m_accelSelection = QStringLiteral("auto");
    DeploySpec             m_config;                // the working configuration
    QHash<int, QString>    m_lastDetections;   // sessionId → last logged label set
};
