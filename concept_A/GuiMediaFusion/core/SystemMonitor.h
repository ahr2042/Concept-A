#pragma once

// Real host telemetry for the Analytics / Dashboard panels, read from Linux
// sysfs at 1 Hz. AMD-first (this rig: Radeon via amdgpu hwmon) with graceful
// degradation: every field is optional and reads -1 / NaN when unavailable, so
// the widgets can show an OFFLINE state instead of fake numbers.

#include <QObject>
#include <QString>
#include <QTimer>

class SystemMonitor : public QObject
{
    Q_OBJECT
public:
    struct Sample {
        double gpuTempC   = -1;   // amdgpu edge
        double cpuTempC   = -1;   // coretemp/k10temp package
        double vramTempC  = -1;   // amdgpu mem temp (if exposed)
        int    gpuBusyPct = -1;   // amdgpu gpu_busy_percent
        int    fanRpm     = -1;   // amdgpu fan1_input
        double vramUsedMB = -1;   // mem_info_vram_used
        double vramTotMB  = -1;   // mem_info_vram_total
    };

    explicit SystemMonitor(QObject* parent = nullptr);

    void start(int intervalMs = 1000);
    void stop();
    const Sample& last() const { return m_last; }
    QString gpuName() const    { return m_gpuName; }

signals:
    void sampled(const SystemMonitor::Sample& s);

private:
    void probePaths();
    void tick();
    static double readNum(const QString& path, double scale = 1.0);

    QTimer  m_timer;
    Sample  m_last;
    QString m_gpuName;

    QString m_gpuHwmon;       // /sys/class/hwmon/hwmonN (amdgpu)
    QString m_cpuHwmon;       // /sys/class/hwmon/hwmonN (coretemp / k10temp)
    QString m_drmDevice;      // /sys/class/drm/cardN/device
};
