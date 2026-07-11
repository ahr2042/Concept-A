#include "AnalyticsPage.h"

#include "../core/BackendService.h"
#include "../theme/Theme.h"
#include "../widgets/Components.h"

#include <gst/gst.h>

#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

AnalyticsPage::AnalyticsPage(BackendService* service, SystemMonitor* monitor, QWidget* parent)
    : QWidget(parent), m_service(service), m_monitor(monitor)
{
    m_started.start();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(14);

    lay->addWidget(buildHeader());

    // row 1: fps chart + frame integrity
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(14);

    auto* chartCard = vos::makeCard("true", this);
    auto* chartLay = new QVBoxLayout(chartCard);
    chartLay->setContentsMargins(14, 10, 14, 12);
    chartLay->setSpacing(8);
    auto* chartHead = new QHBoxLayout;
    chartHead->addWidget(new vos::SectionHeader(QStringLiteral("RECEIVE_RATE (FPS)"), chartCard));
    auto* infNote = new vos::Badge(QStringLiteral("INFERENCE_LATENCY: PLANNED"), vos::Badge::Planned, chartCard);
    infNote->setToolTip(QStringLiteral("The design's inference-latency chart activates with the ONNX stage"));
    chartHead->addStretch(1);
    chartHead->addWidget(infNote);
    chartLay->addLayout(chartHead);
    m_fpsChart = new vos::LineChart(120, chartCard);
    m_fpsChart->setUnit(QStringLiteral(" fps"));
    chartLay->addWidget(m_fpsChart, 1);
    row1->addWidget(chartCard, 2);

    auto* integCard = vos::makeCard("true", this);
    auto* integLay = new QVBoxLayout(integCard);
    integLay->setContentsMargins(14, 10, 14, 12);
    integLay->setSpacing(8);
    auto* integHead = new QHBoxLayout;
    integHead->addWidget(new vos::SectionHeader(QStringLiteral("FRAME_INTEGRITY"), integCard));
    integHead->addStretch(1);
    integHead->addWidget(new vos::Badge(QStringLiteral("PARTIAL"), vos::Badge::Warn, integCard));
    integLay->addLayout(integHead);
    m_framesTotal = new QLabel(QStringLiteral("0"), integCard);
    m_framesTotal->setFont(theme::displayFont(30, QFont::Bold));
    m_framesTotal->setAlignment(Qt::AlignCenter);
    m_framesTotal->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    integLay->addWidget(m_framesTotal);
    auto* integCaption = vos::capsLabel(QStringLiteral("FRAMES RECEIVED / SESSION"), 8, integCard);
    integCaption->setAlignment(Qt::AlignCenter);
    integLay->addWidget(integCaption);
    integLay->addSpacing(6);
    auto* dropRow = new QHBoxLayout;
    dropRow->addWidget(vos::capsLabel(QStringLiteral("BUFFER OVERFLOW"), 8, integCard));
    dropRow->addStretch(1);
    auto* dropVal = vos::dataLabel(QStringLiteral("N/A — PLANNED"), 9, integCard);
    dropVal->setToolTip(QStringLiteral("Backend drop counters not exported yet"));
    dropRow->addWidget(dropVal);
    integLay->addLayout(dropRow);
    integLay->addStretch(1);
    row1->addWidget(integCard, 1);
    lay->addLayout(row1);

    // row 2: thermals + memory
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(14);

    auto* thermCard = vos::makeCard("true", this);
    auto* thermLay = new QVBoxLayout(thermCard);
    thermLay->setContentsMargins(14, 10, 14, 12);
    thermLay->setSpacing(10);
    thermLay->addWidget(new vos::SectionHeader(QStringLiteral("THERMAL_CLUSTERS"), thermCard));
    auto* tiles = new QHBoxLayout;
    tiles->setSpacing(10);
    m_gpuTemp  = new vos::StatTile(QStringLiteral("GPU CORE"),  QStringLiteral("—"), theme::palette().accent, thermCard);
    m_cpuTemp  = new vos::StatTile(QStringLiteral("CPU (SOC)"), QStringLiteral("—"), QColor(theme::kSecondaryLight), thermCard);
    m_vramTemp = new vos::StatTile(QStringLiteral("VRAM PKG"),  QStringLiteral("—"), theme::palette().accent, thermCard);
    tiles->addWidget(m_gpuTemp);
    tiles->addWidget(m_cpuTemp);
    tiles->addWidget(m_vramTemp);
    thermLay->addLayout(tiles);
    auto* fanRow = new QHBoxLayout;
    fanRow->addWidget(vos::capsLabel(QStringLiteral("FAN SPEED:"), 8, thermCard));
    m_fanLabel = vos::dataLabel(QStringLiteral("—"), 9, thermCard);
    fanRow->addWidget(m_fanLabel);
    fanRow->addStretch(1);
    thermLay->addLayout(fanRow);
    row2->addWidget(thermCard, 3);

    auto* memCard = vos::makeCard("true", this);
    auto* memLay = new QVBoxLayout(memCard);
    memLay->setContentsMargins(14, 10, 14, 12);
    memLay->setSpacing(8);
    memLay->addWidget(new vos::SectionHeader(QStringLiteral("VIDEO_MEMORY"), memCard));
    m_vramLabel = new QLabel(QStringLiteral("—"), memCard);
    m_vramLabel->setFont(theme::displayFont(24, QFont::Bold));
    m_vramLabel->setAlignment(Qt::AlignCenter);
    m_vramLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::palette().accent.name()));
    memLay->addWidget(m_vramLabel, 1);
    auto* memCaption = vos::capsLabel(QStringLiteral("VRAM USED / TOTAL (AMDGPU)"), 8, memCard);
    memCaption->setAlignment(Qt::AlignCenter);
    memLay->addWidget(memCaption);
    auto* bwRow = new QHBoxLayout;
    bwRow->addWidget(vos::capsLabel(QStringLiteral("MEMORY BANDWIDTH"), 8, memCard));
    bwRow->addStretch(1);
    bwRow->addWidget(new vos::Badge(QStringLiteral("PLANNED"), vos::Badge::Planned, memCard));
    memLay->addLayout(bwRow);
    row2->addWidget(memCard, 2);
    lay->addLayout(row2);

    lay->addWidget(buildEventLog(), 1);

    connect(m_monitor, &SystemMonitor::sampled, this, &AnalyticsPage::onSample);
    connect(&AppLog::instance(), &AppLog::entryAdded, this, &AnalyticsPage::onLogEntry);
    startTimer(1000);
}

QWidget* AnalyticsPage::buildHeader()
{
    auto* host = new QWidget(this);
    auto* lay = new QHBoxLayout(host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(16);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(6);
    auto* title = new QLabel(QStringLiteral("PERFORMANCE ANALYTICS"), host);
    title->setFont(theme::displayFont(26, QFont::Bold));
    titleBox->addWidget(title);
    auto* meta = new QHBoxLayout;
    meta->setSpacing(12);
    auto* engine = new vos::Badge(QStringLiteral("● LIVE ENGINE GST-%1.%2")
                                      .arg(GST_VERSION_MAJOR).arg(GST_VERSION_MINOR),
                                  vos::Badge::Accent, host);
    meta->addWidget(engine);
    meta->addWidget(vos::capsLabel(QStringLiteral("UPTIME:"), 8, host));
    m_uptime = vos::dataLabel(QStringLiteral("00:00:00"), 9, host);
    meta->addWidget(m_uptime);
    meta->addStretch(1);
    titleBox->addLayout(meta);
    lay->addLayout(titleBox, 1);

    auto* score = vos::makeCard("sunken", host);
    auto* scoreLay = new QVBoxLayout(score);
    scoreLay->setContentsMargins(16, 8, 16, 8);
    scoreLay->setSpacing(2);
    auto* scoreCaption = vos::capsLabel(QStringLiteral("AGGREGATE SCORE"), 8, score);
    scoreCaption->setAlignment(Qt::AlignRight);
    scoreLay->addWidget(scoreCaption);
    auto* scoreVal = new QLabel(QStringLiteral("N/A"), score);
    scoreVal->setFont(theme::displayFont(22, QFont::Bold));
    scoreVal->setAlignment(Qt::AlignRight);
    scoreVal->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kOnSurfaceVariant));
    scoreVal->setToolTip(QStringLiteral("PLANNED — composite health score needs backend QoS counters"));
    scoreLay->addWidget(scoreVal);
    lay->addWidget(score);
    return host;
}

QWidget* AnalyticsPage::buildEventLog()
{
    auto* card = vos::makeCard("true", this);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(14, 10, 14, 12);
    lay->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->addWidget(new vos::SectionHeader(QStringLiteral("SYSTEM EVENT LOG [REAL-TIME]"), card));
    head->addStretch(1);
    auto* exportBtn = new QPushButton(QStringLiteral("EXPORT .CSV"), card);
    exportBtn->setProperty("vosRole", QStringLiteral("ghost"));
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export event log"),
            QDir::homePath() + QStringLiteral("/visionos_events.csv"),
            QStringLiteral("CSV (*.csv)"));
        if (!path.isEmpty() && AppLog::instance().exportCsv(path))
            logInfo("GUI", QStringLiteral("event log exported to %1").arg(path));
    });
    head->addWidget(exportBtn);
    lay->addLayout(head);

    m_eventLog = new QTextEdit(card);
    m_eventLog->setReadOnly(true);
    m_eventLog->setMinimumHeight(140);
    lay->addWidget(m_eventLog, 1);

    // seed with what already happened
    for (const AppLog::Entry& e : AppLog::instance().entries())
        onLogEntry(e);
    return card;
}

void AnalyticsPage::pushFps(double fps)
{
    m_fpsChart->push(fps);
    m_frames += quint64(fps);
    m_framesTotal->setText(QString::number(m_frames));
}

void AnalyticsPage::onSample(const SystemMonitor::Sample& s)
{
    auto fmtTemp = [](double t) {
        return t < 0 ? QStringLiteral("N/A") : QStringLiteral("%1 °C").arg(t, 0, 'f', 0);
    };
    m_gpuTemp->setValue(fmtTemp(s.gpuTempC));
    m_cpuTemp->setValue(fmtTemp(s.cpuTempC));
    m_vramTemp->setValue(fmtTemp(s.vramTempC));
    m_fanLabel->setText(s.fanRpm < 0 ? QStringLiteral("N/A")
                                     : QStringLiteral("%1 RPM").arg(s.fanRpm));
    if (s.vramUsedMB >= 0 && s.vramTotMB > 0)
        m_vramLabel->setText(QStringLiteral("%1 / %2 MB")
                                 .arg(s.vramUsedMB, 0, 'f', 0).arg(s.vramTotMB, 0, 'f', 0));
}

void AnalyticsPage::onLogEntry(const AppLog::Entry& e)
{
    QString color = theme::kOnSurfaceVariant;
    if (e.level == AppLog::Warn) color = theme::kWarn;
    if (e.level == AppLog::Err)  color = theme::kError;
    if (e.level == AppLog::Debug) color = QStringLiteral("#6a6a6e");
    m_eventLog->append(QStringLiteral(
        "<span style='color:%1;'>[%2]</span> <span style='color:%3;'>[%4]</span> "
        "<span style='color:%5;'>%6</span>")
        .arg(theme::palette().accent.name(),
             e.ts.toString(QStringLiteral("HH:mm:ss.zzz")),
             color, AppLog::levelName(e.level),
             color, e.message.toHtmlEscaped()));
    m_eventLog->verticalScrollBar()->setValue(m_eventLog->verticalScrollBar()->maximum());
}

void AnalyticsPage::timerEvent(QTimerEvent*)
{
    const qint64 secs = m_started.elapsed() / 1000;
    m_uptime->setText(QStringLiteral("%1:%2:%3")
                          .arg(secs / 3600, 2, 10, QLatin1Char('0'))
                          .arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
                          .arg(secs % 60, 2, 10, QLatin1Char('0')));
}
