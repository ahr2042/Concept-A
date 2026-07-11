#include "BackendService.h"

#include "../ControlClient.h"
#include "AppLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

// ═════════════════════════════ BackendWorker ═══════════════════════════════

BackendWorker::~BackendWorker()
{
    delete m_client;
}

bool BackendWorker::cmd(const QString& line, QString& reply)
{
    if (!m_client || !m_client->connected())
        return false;
    emit wire(AppLog::Debug, QStringLiteral("→ %1").arg(line));
    std::string resp;
    if (!m_client->command(line.toStdString(), resp)) {
        emit wire(AppLog::Err, QStringLiteral("control link lost while sending '%1'").arg(line));
        m_client->disconnect();
        emit connectedChanged(false, QString());
        return false;
    }
    reply = QString::fromStdString(resp).trimmed();
    emit wire(AppLog::Debug, QStringLiteral("← %1").arg(reply.left(160)));
    return true;
}

void BackendWorker::connectTo(const QString& socketPath)
{
    if (!m_client)
        m_client = new ControlClient;
    m_client->disconnect();
    const bool ok = m_client->connect(socketPath.toStdString());
    emit connectedChanged(ok, socketPath);
}

void BackendWorker::disconnectCtl()
{
    if (m_client)
        m_client->disconnect();
    m_sessions.clear();
    m_sessionSockets.clear();
}

long BackendWorker::createPipeline(const QString& src, const QString& snk, const QString& name)
{
    QString reply;
    if (!cmd(QStringLiteral("create %1 %2 %3").arg(src, snk, name), reply))
        return -1;
    const int pos = reply.indexOf(QStringLiteral("id="));
    if (!reply.startsWith(QStringLiteral("OK")) || pos < 0)
        return -1;
    bool numOk = false;
    const long id = reply.mid(pos + 3).toLong(&numOk);
    return numOk ? id : -1;
}

void BackendWorker::deletePipeline(long daemonId)
{
    QString reply;
    cmd(QStringLiteral("delete %1").arg(daemonId), reply);
    // The daemon erases the vector slot: every id above the deleted one shifts
    // down by one. Re-base our session table accordingly.
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
        if (it.value() > daemonId)
            it.value() -= 1;
}

void BackendWorker::queryDevices()
{
    // `devices` needs a pipeline to ask; use a short-lived probe pipeline
    // (camera → app) that is deleted straight after. Commands are serialized on
    // this thread, so the probe id cannot interleave with session commands.
    const long probe = createPipeline(QStringLiteral("camera"), QStringLiteral("app"),
                                      QStringLiteral("__probe__"));
    if (probe < 0) {
        emit devicesReady(false, {});
        return;
    }
    QString reply;
    const bool ok = cmd(QStringLiteral("devices %1").arg(probe), reply);
    m_sessions.insert(-1, probe);              // temporary entry so re-basing math stays uniform
    QVector<DeviceInfo> devices;
    if (ok)
        deviceparser::parse(reply, devices);
    m_sessions.remove(-1);
    deletePipeline(probe);
    emit devicesReady(ok && !devices.isEmpty(), devices);
}

void BackendWorker::queryAlgorithms()
{
    QString reply;
    QStringList algos;
    if (cmd(QStringLiteral("algos-list"), reply) && reply.startsWith(QStringLiteral("OK"))) {
        const QString csv = reply.mid(2).trimmed();
        for (const QString& a : csv.split(',', Qt::SkipEmptyParts))
            algos << a.trimmed();
    }
    emit algorithmsReady(algos);
}

void BackendWorker::deploy(int sessionId, int deviceIndex, int capIndex,
                           const QString& algosCsv, bool screenSink, const QString& name)
{
    const long id = createPipeline(QStringLiteral("camera"),
                                   screenSink ? QStringLiteral("screen") : QStringLiteral("app"),
                                   name.isEmpty() ? QStringLiteral("gui") : name);
    if (id < 0) {
        emit sessionFailed(sessionId, QStringLiteral("create failed (control link down?)"));
        return;
    }
    m_sessions.insert(sessionId, id);

    QString reply;
    if (!cmd(QStringLiteral("set-device %1 %2 %3").arg(id).arg(deviceIndex).arg(capIndex), reply)
        || !reply.startsWith(QStringLiteral("OK"))) {
        m_sessions.remove(sessionId);
        deletePipeline(id);
        emit sessionFailed(sessionId, QStringLiteral("set-device %1/%2 rejected: %3")
                                          .arg(deviceIndex).arg(capIndex).arg(reply));
        return;
    }

    // Always send algos — an empty csv explicitly disables processing.
    if (!cmd(QStringLiteral("algos %1 %2").arg(id).arg(algosCsv), reply)
        || !reply.startsWith(QStringLiteral("OK"))) {
        emit wire(AppLog::Warn, QStringLiteral("algos '%1' rejected: %2").arg(algosCsv, reply));
    }

    if (!cmd(QStringLiteral("start %1").arg(id), reply) || !reply.startsWith(QStringLiteral("OK"))) {
        m_sessions.remove(sessionId);
        deletePipeline(id);
        emit sessionFailed(sessionId, QStringLiteral("start rejected: %1").arg(reply));
        return;
    }

    const QString videoSocket = reply.section(' ', 1, 1);   // "OK <path>" for app sink
    m_sessionSockets.insert(sessionId, videoSocket);

    QString desc = QStringLiteral("DEV_%1/CAP_%2").arg(deviceIndex).arg(capIndex);
    if (!algosCsv.trimmed().isEmpty())
        desc += QStringLiteral(" · %1").arg(algosCsv.toUpper());
    emit sessionStarted(sessionId, videoSocket, desc);
}

void BackendWorker::stopSession(int sessionId)
{
    if (!m_sessions.contains(sessionId)) {
        emit sessionStopped(sessionId);
        return;
    }
    const long id = m_sessions.take(sessionId);
    m_sessionSockets.remove(sessionId);
    QString reply;
    cmd(QStringLiteral("stop %1").arg(id), reply);
    deletePipeline(id);
    emit sessionStopped(sessionId);
}

void BackendWorker::stopAllSessions()
{
    const QList<int> ids = m_sessions.keys();
    for (int sessionId : ids)
        stopSession(sessionId);
}

void BackendWorker::requestShutdown()
{
    QString reply;
    cmd(QStringLiteral("shutdown"), reply);
    if (m_client)
        m_client->disconnect();
    m_sessions.clear();
    m_sessionSockets.clear();
    emit connectedChanged(false, QString());
}

// ═════════════════════════════ BackendService ══════════════════════════════

QString BackendService::defaultSocketPath()
{
    const QByteArray env = qgetenv("MEDIAFUSION_CTL");
    return env.isEmpty() ? QStringLiteral("/tmp/mediafusiongcv-control.sock")
                         : QString::fromLocal8Bit(env);
}

QString BackendService::defaultBinaryPath()
{
    // The backend executable lands next to the GUI in concept_A/x64_debug/.
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString sibling = appDir.filePath(QStringLiteral("MediaFusionGCV"));
    if (QFileInfo::exists(sibling))
        return sibling;
    return QStringLiteral("MediaFusionGCV");   // rely on PATH
}

BackendService::BackendService(QObject* parent)
    : QObject(parent)
    , m_socketPath(defaultSocketPath())
    , m_binary(defaultBinaryPath())
{
    qRegisterMetaType<QVector<DeviceInfo>>("QVector<DeviceInfo>");

    m_worker = new BackendWorker;
    m_worker->moveToThread(&m_thread);

    connect(m_worker, &BackendWorker::connectedChanged, this, &BackendService::onWorkerConnected);
    connect(m_worker, &BackendWorker::devicesReady,     this, &BackendService::onDevicesReady);
    connect(m_worker, &BackendWorker::algorithmsReady,  this, [this](const QStringList& a) {
        m_algorithms = a;
        logInfo("CTL", QStringLiteral("processing algorithms: %1")
                           .arg(a.isEmpty() ? QStringLiteral("(none)") : a.join(", ")));
        emit algorithmsChanged(a);
    });
    connect(m_worker, &BackendWorker::sessionStarted, this,
            [this](int id, const QString& sock, const QString& desc) {
        logInfo("STREAM", QStringLiteral("session %1 started [%2] socket=%3").arg(id).arg(desc, sock));
        emit sessionStarted(id, sock, desc);
    });
    connect(m_worker, &BackendWorker::sessionStopped, this, [this](int id) {
        logInfo("STREAM", QStringLiteral("session %1 stopped").arg(id));
        emit sessionStopped(id);
    });
    connect(m_worker, &BackendWorker::sessionFailed, this, [this](int id, const QString& err) {
        logErr("STREAM", QStringLiteral("session %1 failed: %2").arg(id).arg(err));
        emit sessionFailed(id, err);
    });
    connect(m_worker, &BackendWorker::wire, this, &BackendService::onWire);

    m_thread.setObjectName(QStringLiteral("backend-ctl"));
    m_thread.start();
}

BackendService::~BackendService()
{
    // Teardown: no signal may reach the (possibly half-destroyed) GUI from here
    // on — sever every route back before doing the blocking shutdown work.
    m_worker->disconnect(this);
    if (m_process)
        m_process->disconnect(this);

    // Best effort: leave no orphaned pipelines behind; take the daemon down
    // only if this session spawned it.
    QMetaObject::invokeMethod(m_worker, &BackendWorker::stopAllSessions, Qt::BlockingQueuedConnection);
    if (m_process && m_process->state() == QProcess::Running) {
        QMetaObject::invokeMethod(m_worker, &BackendWorker::requestShutdown, Qt::BlockingQueuedConnection);
        m_process->waitForFinished(1500);
    }
    m_thread.quit();
    m_thread.wait(2000);
    delete m_worker;          // its thread's loop is stopped; deleteLater can't run
    m_worker = nullptr;
}

qint64 BackendService::daemonPid() const
{
    return (m_process && m_process->state() == QProcess::Running) ? m_process->processId() : -1;
}

void BackendService::setState(DaemonState st)
{
    if (m_state == st) return;
    m_state = st;
    emit daemonStateChanged(st);
}

void BackendService::ensureOnline()
{
    if (m_state == DaemonState::Online)
        return;
    setState(DaemonState::Starting);
    m_connectAttempts = 0;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, p = m_socketPath] { w->connectTo(p); },
                              Qt::QueuedConnection);
}

void BackendService::spawnDaemon()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        return;
    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &BackendService::onProcessOutput);
        connect(m_process, &QProcess::stateChanged, this, &BackendService::onProcessStateChanged);
    }
    logInfo("DAEMON", QStringLiteral("spawning %1 --serve %2").arg(m_binary, m_socketPath));
    m_process->start(m_binary, { QStringLiteral("--serve"), m_socketPath });
}

void BackendService::onProcessStateChanged(QProcess::ProcessState st)
{
    if (st == QProcess::NotRunning) {
        const int code = m_process ? m_process->exitCode() : 0;
        logWarn("DAEMON", QStringLiteral("backend process exited (code %1)").arg(code));
        if (m_state != DaemonState::Starting)
            setState(DaemonState::Offline);
        if (m_restartPending) {
            m_restartPending = false;
            QTimer::singleShot(300, this, &BackendService::ensureOnline);
        }
    }
}

void BackendService::onProcessOutput()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty())
            logDebug("DAEMON", line);
    }
}

void BackendService::onWorkerConnected(bool ok, const QString& socketPath)
{
    if (ok) {
        m_connectAttempts = 0;
        logInfo("CTL", QStringLiteral("control link online at %1").arg(socketPath));
        setState(DaemonState::Online);
        refreshAlgorithms();
        refreshDevices();
        return;
    }

    if (m_state == DaemonState::Online) {       // lost an established link
        logErr("CTL", QStringLiteral("control link lost"));
        setState(DaemonState::Offline);
        return;
    }

    // Still trying to come online.
    ++m_connectAttempts;
    if (m_connectAttempts == 1 && m_autostart) {
        spawnDaemon();
    }
    if (m_connectAttempts <= 10) {
        QTimer::singleShot(400, this, [this] {
            if (m_state == DaemonState::Starting)
                QMetaObject::invokeMethod(m_worker,
                    [w = m_worker, p = m_socketPath] { w->connectTo(p); }, Qt::QueuedConnection);
        });
    } else {
        logErr("CTL", QStringLiteral("cannot reach control daemon at %1 (autostart %2)")
                          .arg(m_socketPath, m_autostart ? "on" : "off"));
        setState(DaemonState::Offline);
    }
}

void BackendService::onDevicesReady(bool ok, const QVector<DeviceInfo>& devices)
{
    m_devices = devices;
    if (!ok && devices.isEmpty())
        logWarn("CTL", QStringLiteral("device scan returned nothing (no camera detected?)"));
    else
        logInfo("CTL", QStringLiteral("device scan: %1 source(s)").arg(devices.size()));
    emit devicesChanged(devices);
}

void BackendService::onWire(int level, const QString& text)
{
    AppLog::instance().log(static_cast<AppLog::Level>(level), QStringLiteral("CTL"), text);
}

void BackendService::restartDaemon()
{
    logWarn("DAEMON", QStringLiteral("REBOOT_CORE requested"));
    stopAll();
    if (m_process && m_process->state() == QProcess::Running) {
        m_restartPending = true;
        QMetaObject::invokeMethod(m_worker, &BackendWorker::requestShutdown, Qt::QueuedConnection);
    } else {
        // Daemon not ours (or not running): ask it to quit, then reconnect.
        QMetaObject::invokeMethod(m_worker, &BackendWorker::requestShutdown, Qt::QueuedConnection);
        QTimer::singleShot(500, this, &BackendService::ensureOnline);
    }
    setState(DaemonState::Starting);
}

void BackendService::shutdownDaemon()
{
    logWarn("DAEMON", QStringLiteral("TERMINATE_PID requested"));
    stopAll();
    QMetaObject::invokeMethod(m_worker, &BackendWorker::requestShutdown, Qt::QueuedConnection);
    setState(DaemonState::Offline);
}

void BackendService::refreshDevices()
{
    QMetaObject::invokeMethod(m_worker, &BackendWorker::queryDevices, Qt::QueuedConnection);
}

void BackendService::refreshAlgorithms()
{
    QMetaObject::invokeMethod(m_worker, &BackendWorker::queryAlgorithms, Qt::QueuedConnection);
}

int BackendService::deploy(const DeploySpec& spec)
{
    const int sessionId = m_nextSession++;
    QMetaObject::invokeMethod(m_worker, [w = m_worker, sessionId, spec] {
        w->deploy(sessionId, spec.deviceIndex, spec.capIndex, spec.algosCsv,
                  spec.screenSink, spec.name);
    }, Qt::QueuedConnection);
    return sessionId;
}

void BackendService::stop(int sessionId)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, sessionId] { w->stopSession(sessionId); },
                              Qt::QueuedConnection);
}

void BackendService::stopAll()
{
    QMetaObject::invokeMethod(m_worker, &BackendWorker::stopAllSessions, Qt::QueuedConnection);
}
