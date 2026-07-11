#pragma once

// VISION_OS main window: TopBar + SideRail + stacked pages + status footer.
// Owns the services (BackendService, SystemMonitor) and wires them to pages.

#include <QMainWindow>

class BackendService;
class SystemMonitor;
class TopBar;
class SideRail;
class QStackedWidget;
class QLabel;
class DashboardPage;
class PipelinePage;
class MultiGridPage;
class AnalyticsPage;
class ComparePage;
class SettingsPage;
class LogsPage;
namespace vos { class LedDot; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

    // --selftest-screenshot support: grab every page into <base>_N.png
    void screenshotTour(const QString& basePath);
    // --selftest-stream: deploy the first camera end-to-end and exit 0 once
    // frames actually arrive in the viewport (1 = timeout without frames).
    void streamSelfTest(int timeoutSeconds = 15);

private slots:
    void onDaemonState(int state);
    void applyTheme();
    void openDeviceManager();
    void snapshotViewport();

private:
    BackendService* m_service = nullptr;
    SystemMonitor*  m_monitor = nullptr;

    TopBar*         m_topBar = nullptr;
    SideRail*       m_rail   = nullptr;
    QStackedWidget* m_pages  = nullptr;

    DashboardPage* m_dashboard = nullptr;
    PipelinePage*  m_pipeline  = nullptr;
    MultiGridPage* m_grid      = nullptr;
    ComparePage*   m_compare   = nullptr;
    AnalyticsPage* m_analytics = nullptr;
    LogsPage*      m_logs      = nullptr;
    SettingsPage*  m_settings  = nullptr;

    vos::LedDot* m_statusLed   = nullptr;
    QLabel*      m_statusText  = nullptr;
    QLabel*      m_statusRight = nullptr;
};
