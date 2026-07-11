#pragma once

// Performance Analytics — design screen "VISION_OS / Performance Analytics".
// Real data everywhere the host can provide it: receive-side FPS chart (the
// design's latency chart slot, honestly relabelled until the AI stage exists),
// thermal clusters + fan from hwmon, VRAM from amdgpu, uptime, and the live
// system event log with CSV export. Frame-integrity and memory-bandwidth
// widgets are placeholders with PLANNED badges.

#include "../core/AppLog.h"
#include "../core/SystemMonitor.h"

#include <QElapsedTimer>
#include <QWidget>

class QLabel;
class BackendService;

namespace vos { class LineChart; class StatTile; class KeyValueRow; }

class AnalyticsPage : public QWidget
{
    Q_OBJECT
public:
    AnalyticsPage(BackendService* service, SystemMonitor* monitor, QWidget* parent = nullptr);

public slots:
    void pushFps(double fps);               // fed from the dashboard viewport

private slots:
    void onSample(const SystemMonitor::Sample& s);
    void onLogEntry(const AppLog::Entry& e);

protected:
    void timerEvent(QTimerEvent*) override;

private:
    QWidget* buildHeader();
    QWidget* buildEventLog();

    BackendService* m_service;
    SystemMonitor*  m_monitor;

    vos::LineChart* m_fpsChart  = nullptr;
    vos::StatTile*  m_gpuTemp   = nullptr;
    vos::StatTile*  m_cpuTemp   = nullptr;
    vos::StatTile*  m_vramTemp  = nullptr;
    QLabel*         m_fanLabel  = nullptr;
    QLabel*         m_vramLabel = nullptr;
    QLabel*         m_uptime    = nullptr;
    QLabel*         m_framesTotal = nullptr;
    class QTextEdit* m_eventLog = nullptr;

    QElapsedTimer m_started;
    quint64       m_frames = 0;
};
