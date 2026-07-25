#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SourceType.h"
#include "errorState.h"

#include <string>
#include <sstream>
#include <vector>
#include <iostream>

class GStreamerSource {
public:
    GStreamerSource() = default;
    virtual ~GStreamerSource();

    struct deviceProperties
    {
        std::string   deviceName;
        GstCaps*      deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
        GstDevice*    gstDevice = nullptr;
        std::string   nodePath;             // underlying kernel node (/dev/videoN)
        bool          directV4l2 = false;   // raw v4l2 provider vs PipeWire wrapper
    };

    std::vector<deviceProperties*> devicesContainer;

    virtual std::vector<std::pair<std::string, std::string>> getDeviceInfoReadable() { return {}; }
    virtual errorState getSourceDevices()                          { return errorState::NO_ERR; }
    virtual errorState setSourceElement(const std::string&)        { return errorState::NO_ERR; }
    virtual errorState setCapsFilterElement(int32_t, int32_t)      { return errorState::NO_ERR; }
    virtual void       addDevicePropertie(const std::string&, GstCaps*) {}

    std::string getCapsStringAtIndex(int32_t deviceID, guint index);

    GstElement* sourceElement = nullptr;
    // Decouples capture from everything downstream: the source's streaming task
    // only dequeues from the driver and pushes here, so a slow colourspace
    // convert or OpenCV stage can no longer stall the v4l2 buffer queue (which
    // shows up as driver-level frame drops). Leaky downstream — under load the
    // oldest frame is dropped rather than latency accumulating.
    GstElement* queue         = nullptr;
    GstElement* capsFilter    = nullptr;
    // Only present when the selected capture format is encoded (MJPEG); nullptr
    // for a raw format, where the source already delivers video/x-raw.
    GstElement* decoder       = nullptr;
    GstElement* converter     = nullptr;
};
