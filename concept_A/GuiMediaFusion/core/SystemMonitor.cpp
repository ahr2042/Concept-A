#include "SystemMonitor.h"

#include <QDir>
#include <QFile>

namespace {

QString readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromLatin1(f.readAll()).trimmed();
}

} // namespace

SystemMonitor::SystemMonitor(QObject* parent) : QObject(parent)
{
    probePaths();
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::tick);
}

void SystemMonitor::probePaths()
{
    const QDir hwmons(QStringLiteral("/sys/class/hwmon"));
    const QStringList entries = hwmons.entryList({ QStringLiteral("hwmon*") }, QDir::Dirs);
    for (const QString& e : entries) {
        const QString base = hwmons.filePath(e);
        const QString name = readAll(base + QStringLiteral("/name"));
        if (name == QLatin1String("amdgpu") && m_gpuHwmon.isEmpty())
            m_gpuHwmon = base;
        else if ((name == QLatin1String("coretemp") || name == QLatin1String("k10temp"))
                 && m_cpuHwmon.isEmpty())
            m_cpuHwmon = base;
    }

    // amdgpu's hwmon lives under /sys/class/drm/cardN/device/hwmon; walk up for
    // the drm device dir (gpu_busy_percent, VRAM counters live there).
    const QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList cards = drm.entryList({ QStringLiteral("card?") }, QDir::Dirs);
    for (const QString& c : cards) {
        const QString dev = drm.filePath(c) + QStringLiteral("/device");
        if (QFile::exists(dev + QStringLiteral("/gpu_busy_percent"))) {
            m_drmDevice = dev;
            break;
        }
    }

    m_gpuName = m_gpuHwmon.isEmpty() ? QStringLiteral("GPU_NOT_DETECTED")
                                     : QStringLiteral("AMDGPU (RADEON)");
}

double SystemMonitor::readNum(const QString& path, double scale)
{
    const QString s = readAll(path);
    if (s.isEmpty())
        return -1;
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v * scale : -1;
}

void SystemMonitor::start(int intervalMs)
{
    m_timer.start(intervalMs);
    tick();
}

void SystemMonitor::stop() { m_timer.stop(); }

void SystemMonitor::tick()
{
    Sample s;
    if (!m_gpuHwmon.isEmpty()) {
        s.gpuTempC  = readNum(m_gpuHwmon + QStringLiteral("/temp1_input"), 1e-3);   // edge
        s.vramTempC = readNum(m_gpuHwmon + QStringLiteral("/temp3_input"), 1e-3);   // mem (optional)
        s.fanRpm    = int(readNum(m_gpuHwmon + QStringLiteral("/fan1_input")));
    }
    if (!m_cpuHwmon.isEmpty())
        s.cpuTempC = readNum(m_cpuHwmon + QStringLiteral("/temp1_input"), 1e-3);    // package
    if (!m_drmDevice.isEmpty()) {
        s.gpuBusyPct = int(readNum(m_drmDevice + QStringLiteral("/gpu_busy_percent")));
        s.vramUsedMB = readNum(m_drmDevice + QStringLiteral("/mem_info_vram_used"),  1.0 / (1024 * 1024));
        s.vramTotMB  = readNum(m_drmDevice + QStringLiteral("/mem_info_vram_total"), 1.0 / (1024 * 1024));
    }
    m_last = s;
    emit sampled(s);
}
