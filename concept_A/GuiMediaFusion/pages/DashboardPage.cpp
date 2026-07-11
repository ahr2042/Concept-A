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
    connect(m_service, &BackendService::algorithmsChanged, this, &DashboardPage::onAlgorithms);
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
    QLabel* latValue = nullptr;
    auto* latBlock = telemetryBlock(QStringLiteral("INFERENCE_LATENCY (MS)"), latValue, m_latBars,
                                    QColor(theme::kTertiary), card);
    latValue->setText(QStringLiteral("OFFLINE"));
    latBlock->setToolTip(QStringLiteral("PLANNED — AI inference stage not yet integrated (ONNX Runtime)"));
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

    // ── PIPELINE_CONFIGURATION (real) ──
    outer->addWidget(new vos::SectionHeader(QStringLiteral("PIPELINE_CONFIGURATION"), panel));

    outer->addWidget(vos::capsLabel(QStringLiteral("VIDEO SOURCE"), 8, panel));
    m_deviceBox = new QComboBox(panel);
    m_deviceBox->addItem(QStringLiteral("SCANNING…"));
    outer->addWidget(m_deviceBox);

    outer->addWidget(vos::capsLabel(QStringLiteral("CAPTURE MODE"), 8, panel));
    m_capsBox = new QComboBox(panel);
    outer->addWidget(m_capsBox);

    connect(m_deviceBox, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_capsBox->clear();
        if (idx >= 0 && idx < m_devices.size())
            for (const CapInfo& c : m_devices[idx].caps)
                m_capsBox->addItem(c.label, c.index);
    });

    outer->addWidget(vos::capsLabel(QStringLiteral("PROCESSING CHAIN (OPENCV)"), 8, panel));
    auto* algoHost = new QWidget(panel);
    auto* algoLay = new QVBoxLayout(algoHost);
    algoLay->setContentsMargins(0, 0, 0, 0);
    algoLay->setSpacing(6);
    m_algoBoxes.clear();
    algoLay->addWidget(new QLabel(QStringLiteral("querying daemon…"), algoHost));
    outer->addWidget(algoHost);
    algoHost->setObjectName(QStringLiteral("algoHost"));

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

    // ── AI_INFERENCE (placeholder, adapted to the AMD/ONNX plan) ──
    auto* aiHead = new QHBoxLayout;
    aiHead->addWidget(new vos::SectionHeader(QStringLiteral("AI_INFERENCE"), panel));
    aiHead->addWidget(new vos::Badge(QStringLiteral("PLANNED"), vos::Badge::Planned, panel));
    aiHead->addStretch(1);
    outer->addLayout(aiHead);

    outer->addWidget(vos::capsLabel(QStringLiteral("DETECTION MODEL"), 8, panel));
    auto* modelBox = new QComboBox(panel);
    modelBox->addItems({ QStringLiteral("YOLOv8_NANO (ONNX)"), QStringLiteral("SSD_MOBILENET_V3"),
                         QStringLiteral("POSE_NET_INDUSTRIAL") });
    vos::markPlanned(modelBox, QStringLiteral("PLANNED — ONNX Runtime stage not yet in backend"));
    outer->addWidget(modelBox);

    auto* accelCard = vos::makeCard("raised", panel);
    auto* accelLay = new QHBoxLayout(accelCard);
    accelLay->setContentsMargins(10, 8, 10, 8);
    auto* accelText = new QVBoxLayout;
    accelText->setSpacing(2);
    auto* accelTitle = vos::dataLabel(QStringLiteral("GPU_ACCELERATION"), 10, accelCard);
    m_hwLabel = vos::capsLabel(m_monitor->gpuName(), 8, accelCard);
    accelText->addWidget(accelTitle);
    accelText->addWidget(m_hwLabel);
    accelLay->addLayout(accelText);
    accelLay->addStretch(1);
    auto* accelToggle = new vos::ToggleSwitch(accelCard);
    vos::markPlanned(accelToggle, QStringLiteral("PLANNED — inference acceleration lands with the ONNX stage"));
    accelLay->addWidget(accelToggle);
    outer->addWidget(accelCard);

    outer->addWidget(vos::makeHSeparator(panel));

    // ── DETECTION_SUMMARY (placeholder) ──
    auto* sumHead = new QHBoxLayout;
    sumHead->addWidget(new vos::SectionHeader(QStringLiteral("DETECTION_SUMMARY"), panel));
    sumHead->addWidget(new vos::Badge(QStringLiteral("OFFLINE"), vos::Badge::Planned, panel));
    sumHead->addStretch(1);
    outer->addLayout(sumHead);

    auto* tiles = new QHBoxLayout;
    tiles->setSpacing(8);
    tiles->addWidget(new vos::StatTile(QStringLiteral("TOTAL_OBJECTS"), QStringLiteral("0"),
                                       theme::palette().accent, panel));
    tiles->addWidget(new vos::StatTile(QStringLiteral("AVG_CONFIDENCE"), QStringLiteral("—"),
                                       QColor(theme::kSecondaryLight), panel));
    outer->addLayout(tiles);

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
    m_deviceBox->clear();
    if (devices.isEmpty()) {
        m_deviceBox->addItem(QStringLiteral("NO_VIDEO_DEVICE_FOUND"));
        m_startBtn->setEnabled(false);
        return;
    }
    for (const DeviceInfo& d : devices)
        m_deviceBox->addItem(d.name.toUpper(), d.index);
    m_startBtn->setEnabled(m_sessionId < 0);
}

void DashboardPage::onAlgorithms(const QStringList& algos)
{
    auto* host = findChild<QWidget*>(QStringLiteral("algoHost"));
    if (!host) return;
    QLayoutItem* item;
    while ((item = host->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_algoBoxes.clear();
    if (algos.isEmpty()) {
        host->layout()->addWidget(new QLabel(QStringLiteral("no algorithms reported"), host));
        return;
    }
    for (const QString& a : algos) {
        auto* cb = new QCheckBox(a.toUpper(), host);
        m_algoBoxes.append(cb);
        host->layout()->addWidget(cb);
    }
}

QString DashboardPage::algosCsv() const
{
    QStringList active;
    for (QCheckBox* cb : m_algoBoxes)
        if (cb->isChecked())
            active << cb->text().toLower();
    return active.join(',');
}

void DashboardPage::onStart()
{
    if (m_deviceBox->count() == 0 || !m_deviceBox->currentData().isValid())
        return;
    BackendService::DeploySpec spec;
    spec.deviceIndex = m_deviceBox->currentData().toInt();
    spec.capIndex    = m_capsBox->currentData().isValid() ? m_capsBox->currentData().toInt() : 0;
    spec.algosCsv    = algosCsv();
    spec.name        = QStringLiteral("dashboard");
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
    if (socket.isEmpty() || !m_tile->bind(socket.toStdString(), m_deviceBox->currentText())) {
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
    m_startBtn->setEnabled(m_deviceBox->count() > 0 && m_deviceBox->currentData().isValid());
    m_stopBtn->setEnabled(false);
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
