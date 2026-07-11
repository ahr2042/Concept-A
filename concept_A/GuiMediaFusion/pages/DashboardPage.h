#pragma once

// Dashboard — design screen "VISION_OS / Dashboard".
// Center: live viewport (VideoTile) + SYSTEM_TELEMETRY strip (real fps &
// throughput from the receiver probe, real GPU utilization from sysfs;
// inference latency is a labelled placeholder). Right: PIPELINE_CONFIGURATION
// (real device/caps/processing selection + start/stop), an AI_INFERENCE
// placeholder section, DETECTION_SUMMARY placeholders, live event feed, and the
// REBOOT_CORE / TERMINATE_PID daemon controls.

#include "../core/AppLog.h"
#include "../core/BackendService.h"
#include "../core/SystemMonitor.h"

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class VideoTile;

namespace vos { class MiniBars; class KeyValueRow; class StatTile; class Badge; }

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    DashboardPage(BackendService* service, SystemMonitor* monitor, QWidget* parent = nullptr);

    VideoTile* tile() const { return m_tile; }
    void setMinLogLevel(int lvl) { m_minLogLevel = lvl; }

    // Self-test hook: press START on the first device as soon as one is known.
    void selfTestStart();

signals:
    void fpsSample(double fps);          // forwarded to AnalyticsPage

private slots:
    void onDevices(const QVector<DeviceInfo>& devices);
    void onAlgorithms(const QStringList& algos);
    void onStart();
    void onStop();
    void onSessionStarted(int sessionId, const QString& socket, const QString& desc);
    void onSessionStopped(int sessionId);
    void onSessionFailed(int sessionId, const QString& error);
    void onLogEntry(const AppLog::Entry& e);

private:
    QWidget* buildCenterColumn();
    QWidget* buildConfigPanel();
    QString  algosCsv() const;

    BackendService* m_service;
    SystemMonitor*  m_monitor;

    VideoTile*   m_tile = nullptr;
    int          m_sessionId = -1;

    // config panel
    QComboBox*   m_deviceBox = nullptr;
    QComboBox*   m_capsBox   = nullptr;
    QList<QCheckBox*> m_algoBoxes;
    QPushButton* m_startBtn  = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QLabel*      m_hwLabel   = nullptr;

    // telemetry strip
    vos::KeyValueRow* m_throughput = nullptr;
    vos::KeyValueRow* m_frameDrop  = nullptr;
    vos::MiniBars*    m_fpsBars    = nullptr;
    vos::MiniBars*    m_latBars    = nullptr;
    vos::MiniBars*    m_gpuBars    = nullptr;
    QLabel*           m_fpsValue   = nullptr;
    QLabel*           m_gpuValue   = nullptr;

    // event feed
    QVBoxLayout* m_feedLay = nullptr;
    int          m_minLogLevel = 1;      // Info
    QVector<DeviceInfo> m_devices;
};
