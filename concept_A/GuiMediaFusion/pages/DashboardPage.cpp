#include "DashboardPage.h"

#include "../theme/Theme.h"
#include "../widgets/Components.h"
#include "../widgets/VideoTile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QWidget* telemetryBlock(const QString& title, QLabel*& valueOut, vos::MiniBars*& barsOut,
                        const QColor& barColor, QWidget* parent)
{
    auto* card = vos::makeCard("sunken", parent);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(4);
    auto* head = new QHBoxLayout;
    head->setSpacing(6);
    head->addWidget(vos::capsLabel(title, 8, card));
    head->addStretch(1);
    valueOut = vos::dataLabel(QStringLiteral("—"), 10, card);
    head->addWidget(valueOut);
    lay->addLayout(head);
    barsOut = new vos::MiniBars(24, card);
    barsOut->setBarColor(barColor);
    lay->addWidget(barsOut, 1);
    return card;
}

} // namespace

DashboardPage::DashboardPage(BackendService* service, SystemMonitor* monitor, QWidget* parent)
    : QWidget(parent), m_service(service), m_monitor(monitor)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(14);
    lay->addWidget(buildCenterColumn(), 1);
    lay->addWidget(buildConfigPanel());

    connect(m_service, &BackendService::devicesChanged,    this, &DashboardPage::onDevices);
    connect(m_service, &BackendService::modelsChanged,     this, &DashboardPage::onModels);
    connect(m_service, &BackendService::configChanged,     this, &DashboardPage::onConfigChanged);
    connect(m_service, &BackendService::inferenceStatsChanged,
            this, &DashboardPage::onInferenceStats);
    connect(m_service, &BackendService::sessionStarted,    this, &DashboardPage::onSessionStarted);
    connect(m_service, &BackendService::sessionStopped,    this, &DashboardPage::onSessionStopped);
    connect(m_service, &BackendService::sessionFailed,     this, &DashboardPage::onSessionFailed);
    connect(&AppLog::instance(), &AppLog::entryAdded,      this, &DashboardPage::onLogEntry);

    connect(m_monitor, &SystemMonitor::sampled, this, [this](const SystemMonitor::Sample& s) {
        if (s.gpuBusyPct >= 0) {
            m_gpuBars->push(s.gpuBusyPct);
            m_gpuValue->setText(QStringLiteral("%1%").arg(s.gpuBusyPct));
        }
    });

    connect(m_tile, &VideoTile::statsTick, this,
            [this](double fps, double mbps, QSize, quint64) {
        m_fpsBars->push(fps);
        m_fpsValue->setText(QString::number(fps, 'f', 1));
        m_throughput->setValue(QStringLiteral("%1 MB/s").arg(mbps, 0, 'f', 1));
        emit fpsSample(fps);
    });
    // Show the config as it stands rather than waiting for a change: the first
    // source selection matches the defaults, so configChanged never fires for it
    // and the panel would sit on placeholder dashes until something moved.
    onConfigChanged(m_service->config());

    connect(m_tile, &VideoTile::streamError, this, [this](const QString& e) {
        logErr("STREAM", QStringLiteral("receiver error: %1").arg(e));
        if (m_sessionId >= 0)
            m_service->stop(m_sessionId);
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
    });
}

QWidget* DashboardPage::buildCenterColumn()
{
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(14);

    m_tile = new VideoTile(QStringLiteral("MAIN_VIEWPORT"), host);
    lay->addWidget(m_tile, 1);

    // SYSTEM_TELEMETRY strip
    auto* card = vos::makeCard("true", host);
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(14, 10, 14, 12);
    cardLay->setSpacing(10);

    auto* head = new QHBoxLayout;
    head->addWidget(new vos::SectionHeader(QStringLiteral("SYSTEM_TELEMETRY"), card));
    head->addStretch(1);
    m_throughput = new vos::KeyValueRow(QStringLiteral("THROUGHPUT"), QStringLiteral("0.0 MB/s"),
                                        theme::palette().accent, card);
    m_frameDrop  = new vos::KeyValueRow(QStringLiteral("FRAME_DROP"), QStringLiteral("N/A"),
                                        QColor(theme::kError), card);
    m_frameDrop->setToolTip(QStringLiteral("PLANNED — backend does not report drop counters yet"));
    head->addWidget(m_throughput);
    head->addSpacing(18);
    head->addWidget(m_frameDrop);
    cardLay->addLayout(head);

    auto* blocks = new QHBoxLayout;
    blocks->setSpacing(10);
    blocks->addWidget(telemetryBlock(QStringLiteral("FPS_STABILITY"), m_fpsValue, m_fpsBars,
                                     theme::palette().accentDim, card), 1);
    auto* latBlock = telemetryBlock(QStringLiteral("INFERENCE_LATENCY (MS)"), m_latValue, m_latBars,
                                    QColor(theme::kTertiary), card);
    m_latValue->setText(QStringLiteral("OFFLINE"));
    latBlock->setToolTip(QStringLiteral("Time for one detector forward pass. Inference runs off the "
                                        "streaming thread, so this is independent of FPS."));
    blocks->addWidget(latBlock, 1);
    blocks->addWidget(telemetryBlock(QStringLiteral("GPU_UTILIZATION"), m_gpuValue, m_gpuBars,
                                     theme::palette().accentDim, card), 1);
    cardLay->addLayout(blocks);

    lay->addWidget(card);
    return host;
}

QWidget* DashboardPage::buildConfigPanel()
{
    auto* panel = vos::makeCard("true", this);
    panel->setFixedWidth(310);
    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    // ── STREAM_CONTROL ──
    //
    // This page watches a pipeline run; it does not build one. The chain is
    // shown, not edited — the editor lives on the Pipeline page, which is the
    // console's one configuration surface. Both used to carry a full copy of it.
    outer->addWidget(new vos::SectionHeader(QStringLiteral("STREAM_CONTROL"), panel));

    auto* chainHead = new QHBoxLayout;
    chainHead->addWidget(vos::capsLabel(QStringLiteral("ACTIVE CHAIN"), 8, panel));
    chainHead->addStretch(1);
    auto* configure = new QPushButton(QStringLiteral("CONFIGURE →"), panel);
    configure->setProperty("vosRole", QStringLiteral("outline"));
    configure->setCursor(Qt::PointingHandCursor);
    configure->setFixedHeight(24);
    configure->setToolTip(QStringLiteral("Edit the processing chain on the Pipeline page"));
    connect(configure, &QPushButton::clicked, this, &DashboardPage::configureRequested);
    chainHead->addWidget(configure);
    outer->addLayout(chainHead);

    m_chainSummary = vos::dataLabel(QStringLiteral("—"), 10, panel);
    m_chainSummary->setWordWrap(true);
    outer->addWidget(m_chainSummary);

    auto* btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(QStringLiteral("START_STREAM"), panel);
    m_startBtn->setProperty("vosRole", QStringLiteral("primary"));
    m_stopBtn = new QPushButton(QStringLiteral("STOP_STREAM"), panel);
    m_stopBtn->setEnabled(false);
    connect(m_startBtn, &QPushButton::clicked, this, &DashboardPage::onStart);
    connect(m_stopBtn,  &QPushButton::clicked, this, &DashboardPage::onStop);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    outer->addLayout(btnRow);

    outer->addWidget(vos::makeHSeparator(panel));

    // ── AI_INFERENCE (real: OpenCV DNN detector in the processing chain) ──
    auto* aiHead = new QHBoxLayout;
    aiHead->addWidget(new vos::SectionHeader(QStringLiteral("AI_INFERENCE"), panel));
    m_aiBadge = new vos::Badge(QStringLiteral("OFFLINE"), vos::Badge::Planned, panel);
    aiHead->addWidget(m_aiBadge);
    aiHead->addStretch(1);
    outer->addLayout(aiHead);

    // Read-only: the model and its thresholds are set where the chain is set.
    m_modelLabel = vos::dataLabel(QStringLiteral("—"), 10, panel);
    m_modelLabel->setWordWrap(true);
    m_modelLabel->setToolTip(QStringLiteral("Chosen on the Pipeline page, with the rest of the chain"));
    outer->addWidget(m_modelLabel);

    m_sumBadge = new vos::Badge(QStringLiteral("OFFLINE"), vos::Badge::Planned, panel);
    m_sumBadge->setVisible(false);         // the AI badge above already says this

    auto* tiles = new QHBoxLayout;
    tiles->setSpacing(8);
    m_objectsTile = new vos::StatTile(QStringLiteral("TOTAL_OBJECTS"), QStringLiteral("0"),
                                      theme::palette().accent, panel);
    m_confTile    = new vos::StatTile(QStringLiteral("AVG_CONFIDENCE"), QStringLiteral("—"),
                                      QColor(theme::kSecondaryLight), panel);
    tiles->addWidget(m_objectsTile);
    tiles->addWidget(m_confTile);
    outer->addLayout(tiles);

    outer->addWidget(vos::makeHSeparator(panel));

    // ── RECENT EVENTS (real feed) ──
    outer->addWidget(new vos::SectionHeader(QStringLiteral("RECENT_EVENTS"), panel));
    auto* feedCard = vos::makeCard("sunken", panel);
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* feedHost = new QWidget(feedCard);
    m_feedLay = new QVBoxLayout(feedHost);
    m_feedLay->setContentsMargins(10, 8, 10, 8);
    m_feedLay->setSpacing(3);
    m_feedLay->addStretch(1);
    scroll->setWidget(feedHost);
    auto* feedLayOuter = new QVBoxLayout(feedCard);
    feedLayOuter->setContentsMargins(1, 1, 1, 1);
    feedLayOuter->addWidget(scroll);
    outer->addWidget(feedCard, 1);

    // ── daemon controls ──
    auto* daemonRow = new QHBoxLayout;
    auto* reboot = new QPushButton(QStringLiteral("REBOOT_CORE"), panel);
    reboot->setToolTip(QStringLiteral("Restart the MediaFusionGCV daemon"));
    auto* term = new QPushButton(QStringLiteral("TERMINATE_PID"), panel);
    term->setProperty("vosRole", QStringLiteral("danger"));
    term->setToolTip(QStringLiteral("Shut the MediaFusionGCV daemon down"));
    connect(reboot, &QPushButton::clicked, m_service, &BackendService::restartDaemon);
    connect(term,   &QPushButton::clicked, m_service, &BackendService::shutdownDaemon);
    daemonRow->addWidget(reboot);
    daemonRow->addWidget(term);
    outer->addLayout(daemonRow);

    return panel;
}

void DashboardPage::selfTestStart()
{
    if (!m_devices.isEmpty()) {
        onStart();
        return;
    }
    connect(m_service, &BackendService::devicesChanged, this,
            [this](const QVector<DeviceInfo>& devs) {
        if (!devs.isEmpty() && m_sessionId < 0)
            onStart();
    }, Qt::SingleShotConnection);
}

void DashboardPage::onDevices(const QVector<DeviceInfo>& devices)
{
    m_devices = devices;
    // Choosing a source is the rail's job now; what this page needs to know is
    // simply whether there is one to start.
    m_startBtn->setEnabled(!devices.isEmpty() && m_sessionId < 0);
}

// The name of the source the working config points at, for the viewport chip.
QString DashboardPage::sourceName() const
{
    for (const DeviceInfo& d : m_devices)
        if (d.index == m_service->config().deviceIndex)
            return d.name.toUpper();
    return QStringLiteral("SOURCE");
}

void DashboardPage::onModels(const QVector<DetectorModel>& models)
{
    // Choosing a model belongs with the chain, on the Pipeline page. All this
    // page needs from the list is whether there is anything to run at all.
    if (models.isEmpty())
        m_aiBadge->setToolTip(QStringLiteral("No weights in models/ — run scripts/fetch-models.sh"));
}

// The working config changed somewhere — on the Pipeline page, or in the rail.
// This page only reports it.
void DashboardPage::onConfigChanged(const BackendService::DeploySpec& cfg)
{
    const QStringList active = cfg.algosCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);
    m_chainSummary->setText(active.isEmpty()
        ? QStringLiteral("PASSTHROUGH — NO PROCESSING")
        : active.join(QStringLiteral("  →  ")).toUpper());

    m_modelLabel->setText(cfg.detectorModel.isEmpty()
        ? QStringLiteral("NO MODEL SELECTED")
        : QStringLiteral("%1 · CONF %2").arg(cfg.detectorModel.toUpper())
                                        .arg(cfg.confidence, 0, 'f', 2));
}

void DashboardPage::onInferenceStats(int sessionId, const InferenceSnapshot& s)
{
    if (sessionId != m_sessionId)
        return;

    if (!s.error.isEmpty()) {
        m_aiBadge->setText(QStringLiteral("ERROR"));
        m_aiBadge->setKind(vos::Badge::Error);
        m_aiBadge->setToolTip(s.error);
        return;
    }
    if (!s.active()) {
        m_aiBadge->setText(s.loaded ? QStringLiteral("LOADED") : QStringLiteral("OFFLINE"));
        m_aiBadge->setKind(s.loaded ? vos::Badge::Secondary : vos::Badge::Planned);
        m_aiBadge->setToolTip(s.loaded
            ? QStringLiteral("Model resident; enable DETECT in the processing chain to run it")
            : QStringLiteral("No detector in this pipeline's chain"));
        return;
    }

    m_aiBadge->setText(QStringLiteral("ACTIVE"));
    m_aiBadge->setKind(vos::Badge::Ok);
    m_aiBadge->setToolTip(QStringLiteral("%1 · %2 inferences, %3 frames drawn from a previous result")
                              .arg(s.modelName).arg(s.framesInferred).arg(s.framesSkipped));
    m_latValue->setText(QString::number(s.avgInferenceMs, 'f', 1));
    m_latBars->push(s.avgInferenceMs);

    m_sumBadge->setText(QStringLiteral("LIVE"));
    m_sumBadge->setKind(vos::Badge::Ok);
    m_objectsTile->setValue(QString::number(s.objectCount));
    m_confTile->setValue(s.objectCount > 0 ? QString::number(s.avgConfidence, 'f', 2)
                                           : QStringLiteral("—"));
}

void DashboardPage::onStart()
{
    if (m_devices.isEmpty())
        return;
    // Deploy what the working config says, not what this page's widgets say.
    // The two deploy paths used to read their own widgets and so could disagree
    // — this one never sent the acceleration choice at all.
    BackendService::DeploySpec spec = m_service->config();
    spec.name = QStringLiteral("dashboard");
    m_sessionId = m_service->deploy(spec);
    m_startBtn->setEnabled(false);
}

void DashboardPage::onStop()
{
    if (m_sessionId >= 0)
        m_service->stop(m_sessionId);
    m_stopBtn->setEnabled(false);
}

void DashboardPage::onSessionStarted(int sessionId, const QString& socket, const QString& desc)
{
    if (sessionId != m_sessionId)
        return;
    if (socket.isEmpty() || !m_tile->bind(socket.toStdString(), sourceName())) {
        logErr("STREAM", QStringLiteral("cannot bind viewport to %1").arg(socket));
        m_service->stop(sessionId);
        m_sessionId = -1;
        m_startBtn->setEnabled(true);
        return;
    }
    Q_UNUSED(desc);
    m_stopBtn->setEnabled(true);
}

void DashboardPage::onSessionStopped(int sessionId)
{
    if (sessionId != m_sessionId)
        return;
    m_sessionId = -1;
    m_tile->unbind();
    m_startBtn->setEnabled(!m_devices.isEmpty());
    m_stopBtn->setEnabled(false);

    // The inference stage went away with the pipeline — stop showing its last
    // numbers as if they were live.
    m_aiBadge->setText(QStringLiteral("OFFLINE"));
    m_aiBadge->setKind(vos::Badge::Planned);
    m_sumBadge->setText(QStringLiteral("OFFLINE"));
    m_sumBadge->setKind(vos::Badge::Planned);
    m_latValue->setText(QStringLiteral("OFFLINE"));
    m_objectsTile->setValue(QStringLiteral("0"));
    m_confTile->setValue(QStringLiteral("—"));
}

void DashboardPage::onSessionFailed(int sessionId, const QString& error)
{
    if (sessionId != m_sessionId)
        return;
    Q_UNUSED(error);
    m_sessionId = -1;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void DashboardPage::onLogEntry(const AppLog::Entry& e)
{
    if (int(e.level) < m_minLogLevel)
        return;
    QString color = theme::kOnSurfaceVariant;
    if (e.level == AppLog::Warn) color = theme::kWarn;
    if (e.level == AppLog::Err)  color = theme::kError;
    auto* row = new QLabel(QStringLiteral("<span style='color:%1;'>%2</span> "
                                          "<span style='color:%3;'>%4</span>")
                               .arg(theme::palette().accent.name(),
                                    e.ts.toString(QStringLiteral("HH:mm:ss")),
                                    color, e.message.toHtmlEscaped()));
    row->setFont(theme::monoFont(8, QFont::Medium));
    row->setWordWrap(true);
    // insert above the stretch; cap the visible feed at 40 rows
    m_feedLay->insertWidget(m_feedLay->count() - 1, row);
    while (m_feedLay->count() > 41) {
        QLayoutItem* it = m_feedLay->takeAt(0);
        delete it->widget();
        delete it;
    }
}
