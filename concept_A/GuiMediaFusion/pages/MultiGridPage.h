#pragma once

// Multi-Grid Analysis — design screen "VISION_OS / Multi-Grid Analysis".
// Global bar (master clock real, sync/rec-all/freeze PLANNED) over a 2×2 grid.
// Each slot runs its own backend session (fresh pipeline per slot). With a
// single physical camera only one slot can be live at a time — the design's
// four simultaneous feeds light up as more cameras are attached.

#include "../core/BackendService.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class VideoTile;

class MultiGridPage : public QWidget
{
    Q_OBJECT
public:
    explicit MultiGridPage(BackendService* service, QWidget* parent = nullptr);

private slots:
    void onDevices(const QVector<DeviceInfo>& devices);
    void onSessionStarted(int sessionId, const QString& socket, const QString& desc);
    void onSessionStopped(int sessionId);
    void onSessionFailed(int sessionId, const QString& error);

private:
    struct Slot {
        VideoTile*   tile      = nullptr;
        QComboBox*   deviceBox = nullptr;
        QComboBox*   algoBox   = nullptr;
        QPushButton* button    = nullptr;
        int          sessionId = -1;
    };
    QWidget* buildSlot(int index);
    void     toggleSlot(int index);

    BackendService* m_service;
    QVector<Slot>   m_slots;
    QLabel*         m_clock = nullptr;
    QVector<DeviceInfo> m_devices;

protected:
    void timerEvent(QTimerEvent*) override;
};
