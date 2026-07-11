#pragma once

// Parses the daemon's `devices <id>` reply (formatted by
// GStreamerSourceCamera::addDevicePropertie) into structured data the GUI can
// put in combo boxes / device cards. Reply shape:
//
//   OK 2 device(s)
//   Device [0]: HD Webcam C270
//     Cap [0]: video/x-raw
//       format: "YUY2"
//       width: 640
//       height: 480
//       framerate: { (fraction)30/1, (fraction)25/1 }
//     Cap [1]: ...
//     -> use: set-device 0 0 <cap>
//   (blank line between devices)

#include <QString>
#include <QVector>

struct CapInfo {
    int     index = 0;
    QString label;      // friendly: "640x480 @ 30fps YUY2"
    QString raw;        // the indented field block, for detail views
};

struct DeviceInfo {
    int              index = 0;
    QString          name;
    QVector<CapInfo> caps;
};

namespace deviceparser {

// Returns true when the reply started with "OK"; devices out param filled.
bool parse(const QString& reply, QVector<DeviceInfo>& devices);

} // namespace deviceparser
