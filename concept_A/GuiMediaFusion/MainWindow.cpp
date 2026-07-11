#include "MainWindow.h"

#include "core/AppLog.h"
#include "core/BackendService.h"
#include "core/SystemMonitor.h"
#include "dialogs/DeviceManagerDialog.h"
#include "pages/AnalyticsPage.h"
#include "pages/ComparePage.h"
#include "pages/DashboardPage.h"
#include "pages/LogsPage.h"
#include "pages/MultiGridPage.h"
#include "pages/PipelinePage.h"
#include "pages/SettingsPage.h"
#include "theme/Theme.h"
#include "widgets/Components.h"
#include "widgets/Shell.h"
#include "widgets/VideoTile.h"

#include <gst/gst.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("VISION_OS / MediaFusionGCV"));
    resize(1440, 920);

    // ── services ──
    m_service = new BackendService(this);
    QSettings s;
    if (s.contains(QStringLiteral("backend/socket")))
        m_service->setControlSocketPath(s.value(QStringLiteral("backend/socket")).toString());
    if (s.contains(QStringLiteral("backend/binary")))
        m_service->setBackendBinary(s.value(QStringLiteral("backend/binary")).toString());
    m_service->setAutostart(s.value(QStringLiteral("backend/autostart"), true).toBool());

    m_monitor = new SystemMonitor(this);
    m_monitor->start();
    vos::LedDot::setPulseEnabled(s.value(QStringLiteral("ui/ledPulse"), true).toBool());

    // ── chrome ──
    auto* central = new QWidget(this);
    auto* rootLay = new QVBoxLayout(central);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    const QStringList pageNames = { QStringLiteral("DASHBOARD"), QStringLiteral("PIPELINE"),
                                    QStringLiteral("MULTI-GRID"), QStringLiteral("COMPARE"),
                                    QStringLiteral("ANALYTICS"), QStringLiteral("LOGS") };
    m_topBar = new TopBar(pageNames, central);
    rootLay->addWidget(m_topBar);

    auto* body = new QWidget(central);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    m_rail = new SideRail(body);
    bodyLay->addWidget(m_rail);

    m_pages = new QStackedWidget(body);
    m_dashboard = new DashboardPage(m_service, m_monitor, m_pages);
    m_pipeline  = new PipelinePage(m_service, m_pages);
    m_grid      = new MultiGridPage(m_service, m_pages);
    m_compare   = new ComparePage(m_service, m_pages);
    m_analytics = new AnalyticsPage(m_service, m_monitor, m_pages);
    m_logs      = new LogsPage(m_pages);
    m_settings  = new SettingsPage(m_service, m_pages);
    for (QWidget* p : { static_cast<QWidget*>(m_dashboard), static_cast<QWidget*>(m_pipeline),
                        static_cast<QWidget*>(m_grid),      static_cast<QWidget*>(m_compare),
                        static_cast<QWidget*>(m_analytics), static_cast<QWidget*>(m_logs),
                        static_cast<QWidget*>(m_settings) })
        m_pages->addWidget(p);
    bodyLay->addWidget(m_pages, 1);
    rootLay->addWidget(body, 1);
    setCentralWidget(central);

    // ── status footer ──
    m_statusLed = new vos::LedDot(QColor(theme::kError), this);
    m_statusText = new QLabel(QStringLiteral("CONTROL_LINK: OFFLINE"), this);
    m_statusText->setFont(theme::monoFont(8, QFont::Bold, 110.0));
    statusBar()->addWidget(m_statusLed);
    statusBar()->addWidget(m_statusText);
    m_statusRight = new QLabel(this);
    m_statusRight->setFont(theme::monoFont(8, QFont::Medium, 106.0));
    m_statusRight->setText(QStringLiteral("GST %1.%2 · QT %3 · %4")
                               .arg(GST_VERSION_MAJOR).arg(GST_VERSION_MINOR)
                               .arg(QStringLiteral(QT_VERSION_STR),
                                    m_service->controlSocketPath()));
    statusBar()->addPermanentWidget(m_statusRight);

    // ── wiring ──
    connect(m_topBar->nav(), &NavTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_topBar, &TopBar::settingsRequested, this, [this] {
        m_pages->setCurrentWidget(m_settings);
        m_topBar->nav()->setCurrent(-1);
    });
    connect(m_topBar, &TopBar::snapshotRequested, this, &MainWindow::snapshotViewport);

    connect(m_rail, &SideRail::addSourceRequested, this, &MainWindow::openDeviceManager);
    connect(m_rail, &SideRail::systemRequested, this, [this] { m_pages->setCurrentWidget(m_settings); });
    connect(m_rail, &SideRail::categorySelected, this, [this](const QString&) { openDeviceManager(); });
    connect(m_rail, &SideRail::helpRequested, this, [this] {
        QMessageBox::about(this, QStringLiteral("VISION_OS / MFGCV"), QStringLiteral(
            "<b>VISION_OS — MediaFusionGCV operator console</b><br><br>"
            "Two-process architecture: this GUI drives the MediaFusionGCV daemon over a "
            "Unix control socket; video streams over per-pipeline unixfd sockets "
            "(zero-copy memfd). Camera → OpenCV chain → viewport is live today; "
            "AI inference (ONNX Runtime), RTSP/GigE ingest, recording and stream "
            "comparison are PLANNED and appear as disabled placeholders.<br><br>"
            "Design: Stitch project «VisionStream AI Dashboard»."));
    });

    connect(m_service, &BackendService::daemonStateChanged, this,
            [this](BackendService::DaemonState st) { onDaemonState(int(st)); });
    connect(m_service, &BackendService::devicesChanged, this,
            [this](const QVector<DeviceInfo>& devs) { m_rail->setUsbCount(devs.size()); });

    connect(m_dashboard, &DashboardPage::fpsSample, m_analytics, &AnalyticsPage::pushFps);
    connect(m_settings, &SettingsPage::accentChanged, this, &MainWindow::applyTheme);
    connect(m_settings, &SettingsPage::verbosityChanged, this, [this](int lvl) {
        m_logs->setMinLevel(lvl);
        m_dashboard->setMinLogLevel(lvl);
    });
    m_logs->setMinLevel(SettingsPage::savedVerbosity());
    m_dashboard->setMinLogLevel(SettingsPage::savedVerbosity());

    logInfo("GUI", QStringLiteral("VISION_OS console up — bringing control link online"));
    m_service->ensureOnline();
}

MainWindow::~MainWindow()
{
    // Stop log fan-out into widgets that are about to die, then shut the
    // backend down while the page tree is still fully intact.
    AppLog::instance().disconnect();
    m_monitor->stop();
    delete m_service;
    m_service = nullptr;
}

void MainWindow::onDaemonState(int state)
{
    const auto st = static_cast<BackendService::DaemonState>(state);
    switch (st) {
        case BackendService::DaemonState::Online:
            m_statusLed->setColor(QColor(theme::kOk));
            m_statusLed->setPulsing(true);
            m_statusText->setText(QStringLiteral("CONTROL_LINK: ONLINE · PID %1 · %2")
                                      .arg(m_service->daemonPid())
                                      .arg(m_service->controlSocketPath()));
            m_rail->setLinkOnline(true);
            break;
        case BackendService::DaemonState::Starting:
            m_statusLed->setColor(QColor(theme::kWarn));
            m_statusLed->setPulsing(true);
            m_statusText->setText(QStringLiteral("CONTROL_LINK: NEGOTIATING…"));
            m_rail->setLinkOnline(false);
            break;
        case BackendService::DaemonState::Offline:
        default:
            m_statusLed->setColor(QColor(theme::kError));
            m_statusLed->setPulsing(false);
            m_statusText->setText(QStringLiteral("CONTROL_LINK: OFFLINE"));
            m_rail->setLinkOnline(false);
            break;
    }
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(theme::buildStyleSheet());
}

void MainWindow::openDeviceManager()
{
    auto* dlg = new DeviceManagerDialog(m_service, this);
    connect(dlg, &DeviceManagerDialog::deviceChosen, this, [this](int, int) {
        // Selection lands in the Dashboard's source combos via devicesChanged;
        // jump the operator there to press START.
        m_pages->setCurrentWidget(m_dashboard);
        m_topBar->nav()->setCurrent(0);
    });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void MainWindow::snapshotViewport()
{
    const QString dir = QDir::homePath() + QStringLiteral("/Pictures");
    QDir().mkpath(dir);
    const QString path = QStringLiteral("%1/visionos_%2.png")
        .arg(dir, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QPixmap shot = m_dashboard->tile()->grab();
    if (shot.save(path))
        logInfo("GUI", QStringLiteral("snapshot saved: %1").arg(path));
    else
        logErr("GUI", QStringLiteral("snapshot failed: %1").arg(path));
}

void MainWindow::streamSelfTest(int timeoutSeconds)
{
    auto ticks = std::make_shared<int>(0);
    connect(m_dashboard->tile(), &VideoTile::statsTick, this,
            [ticks](double fps, double mbps, QSize res, quint64 total) {
        if (fps <= 0)
            return;
        if (++(*ticks) >= 3) {
            qInfo("SELFTEST_STREAM: PASS fps=%.1f mbps=%.1f res=%dx%d frames=%llu",
                  fps, mbps, res.width(), res.height(),
                  static_cast<unsigned long long>(total));
            QApplication::exit(0);
        }
    });
    QTimer::singleShot(timeoutSeconds * 1000, this, [] {
        qCritical("SELFTEST_STREAM: FAIL — no frames reached the viewport in time");
        QApplication::exit(1);
    });
    m_dashboard->selfTestStart();
}

void MainWindow::screenshotTour(const QString& basePath)
{
    // Walk every page, grabbing each into <base>_<n>_<name>.png, then quit.
    const QStringList names = { QStringLiteral("dashboard"), QStringLiteral("pipeline"),
                                QStringLiteral("multigrid"), QStringLiteral("compare"),
                                QStringLiteral("analytics"), QStringLiteral("logs"),
                                QStringLiteral("settings") };
    auto* timer = new QTimer(this);
    timer->setInterval(700);
    auto step = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, this, [this, basePath, names, step, timer] {
        const int i = *step;
        if (i >= names.size()) {
            timer->stop();
            QApplication::quit();
            return;
        }
        m_topBar->nav()->setCurrent(i < 6 ? i : -1);   // keep nav highlight honest
        m_pages->setCurrentIndex(i);
        // let the page lay out before grabbing on the next tick
        QTimer::singleShot(350, this, [this, basePath, names, i] {
            grab().save(QStringLiteral("%1_%2_%3.png").arg(basePath).arg(i).arg(names[i]));
        });
        ++(*step);
    });
    timer->start();
}
