#pragma once

// Dashboard — design screen "VISION_OS / Dashboard".
// Center: live viewport (VideoTile) + SYSTEM_TELEMETRY strip (real fps &
// throughput from the receiver probe, real GPU utilization from sysfs, real
// inference latency from the detector). Right: PIPELINE_CONFIGURATION (real
// device/caps/processing selection + start/stop), AI_INFERENCE (real model
// picker and confidence, applied live), DETECTION_SUMMARY fed by the daemon's
// stats poll, live event feed, and the REBOOT_CORE / TERMINATE_PID controls.

#include "../core/AppLog.h"
#include "../core/BackendService.h"
#include "../core/SystemMonitor.h"

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
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
    void onModels(const QVector<DetectorModel>& models);
    void onInferenceStats(int sessionId, const InferenceSnapshot& snapshot);
    void onDetectorSettingChanged();     // model / confidence moved by the operator
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

    // AI_INFERENCE / DETECTION_SUMMARY
    QComboBox*      m_modelBox   = nullptr;
    QSlider*        m_confSlider = nullptr;
    QLabel*         m_confLabel  = nullptr;
    vos::Badge*     m_aiBadge    = nullptr;
    vos::Badge*     m_sumBadge   = nullptr;
    vos::StatTile*  m_objectsTile = nullptr;
    vos::StatTile*  m_confTile    = nullptr;

    // telemetry strip
    vos::KeyValueRow* m_throughput = nullptr;
    vos::KeyValueRow* m_frameDrop  = nullptr;
    vos::MiniBars*    m_fpsBars    = nullptr;
    vos::MiniBars*    m_latBars    = nullptr;
    vos::MiniBars*    m_gpuBars    = nullptr;
    QLabel*           m_fpsValue   = nullptr;
    QLabel*           m_gpuValue   = nullptr;
    QLabel*           m_latValue   = nullptr;

    // event feed
    QVBoxLayout* m_feedLay = nullptr;
    int          m_minLogLevel = 1;      // Info
    QVector<DeviceInfo> m_devices;
};
