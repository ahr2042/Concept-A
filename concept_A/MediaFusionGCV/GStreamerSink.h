#pragma once
#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SinkType.h"
#include "errorState.h"

#include <string>
#include <sstream>
#include <vector>
#include <iostream>

class GStreamerSink {
public:
    GStreamerSink() = default;
    virtual ~GStreamerSink();

    struct deviceProperties
    {
        std::string deviceName;
        std::string longName;
        GstCaps*    deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
    };

    std::vector<deviceProperties*> devicesContainer;

    virtual errorState getSinkDevices();
    virtual errorState setSinkElement(const std::string&)     { return errorState::NO_ERR; }
    virtual errorState setCapsFilterElement(int32_t, int32_t) { return errorState::NO_ERR; }
    virtual void       addDevicePropertie(const std::string&, const std::string&, GstCaps*);

    GstElement* sinkElement = nullptr;
    GstElement* capsFilter  = nullptr;
    GstElement* converter   = nullptr;
};
