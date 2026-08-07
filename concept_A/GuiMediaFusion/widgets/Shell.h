#pragma once

// The VISION_OS window chrome: TopBar (logo, page tabs, action icons, operator
// chip) and SideRail (SOURCES header, device-category list, ADD SOURCE, footer
// links). Pure presentation — pages/services connect to the signals.

#include "../core/DeviceParser.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVector>

class QComboBox;

namespace vos { class LedDot; }

// ── NavTabBar ── "DASHBOARD  PIPELINE  MULTI-GRID …" underlined tabs ─────────
class NavTabBar : public QWidget
{
    Q_OBJECT
public:
    explicit NavTabBar(const QStringList& tabs, QWidget* parent = nullptr);
    void setCurrent(int index);
    int  current() const { return m_current; }
signals:
    void currentChanged(int index);
private:
    QVector<QPushButton*> m_buttons;
    int m_current = 0;
};

// ── TopBar ───────────────────────────────────────────────────────────────────
class TopBar : public QFrame
{
    Q_OBJECT
public:
    explicit TopBar(const QStringList& pages, QWidget* parent = nullptr);
    NavTabBar* nav() const { return m_nav; }
signals:
    void settingsRequested();
    void snapshotRequested();
    void recordRequested();          // placeholder (PLANNED)
private:
    NavTabBar* m_nav;
};

// ── SideRail ─────────────────────────────────────────────────────────────────
class SideRail : public QFrame
{
    Q_OBJECT
public:
    explicit SideRail(QWidget* parent = nullptr);

    void setLinkOnline(bool online);              // SOURCES LED + caption
    void setUsbCount(int devices);                // real V4L2 count

    // The rail is where a source is chosen, console-wide. It already claimed the
    // SOURCES identity and already listed the devices; the selection used to sit
    // in two page panels instead, which is how the same two combo boxes came to
    // exist on the Dashboard and the Pipeline page at once.
    void setDevices(const QVector<DeviceInfo>& devices);
    void setSource(int deviceIndex, int capIndex);   // reflect the working config

signals:
    void addSourceRequested();                    // opens DeviceManagerDialog
    void categorySelected(const QString& name);
    void helpRequested();
    void systemRequested();
    void sourceChanged(int deviceIndex, int capIndex);

private:
    QPushButton* addCategory(const QString& icon, const QString& name, bool enabled);
    void         emitSource();

    vos::LedDot* m_led   = nullptr;
    QLabel*      m_liveCaption = nullptr;
    QPushButton* m_usbButton   = nullptr;
    QComboBox*   m_deviceBox   = nullptr;
    QComboBox*   m_capsBox     = nullptr;
    QVector<DeviceInfo> m_devices;
};
