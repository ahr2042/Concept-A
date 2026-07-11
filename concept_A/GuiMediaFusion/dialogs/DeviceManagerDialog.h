#pragma once

// DEVICE MANAGEMENT modal — design screen "VISION_OS / Device Manager".
// Left: scan status + protocol filter (USB/V4L2 is the real backend path; the
// other protocols are PLANNED chips). Right: one card per device from the
// daemon's `devices` reply, with a capture-mode combo and CONNECT. CONNECT (or
// PROCEED with a selection) hands the chosen device/cap back to the caller.

#include "../core/DeviceParser.h"

#include <QDialog>

class BackendService;
class QLabel;
class QVBoxLayout;

class DeviceManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceManagerDialog(BackendService* service, QWidget* parent = nullptr);

signals:
    void deviceChosen(int deviceIndex, int capIndex);

private slots:
    void onDevices(const QVector<DeviceInfo>& devices);

private:
    void rebuildCards();

    BackendService* m_service;
    QVector<DeviceInfo> m_devices;
    QLabel*      m_countLabel = nullptr;
    QVBoxLayout* m_cardsLay   = nullptr;
    int          m_chosenDevice = -1;
    int          m_chosenCap    = 0;
};
