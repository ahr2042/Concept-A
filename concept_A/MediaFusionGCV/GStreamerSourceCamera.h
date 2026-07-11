#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera();
    ~GStreamerSourceCamera() override;

    std::vector<std::pair<std::string, std::string>> getDeviceInfoReadable() override;

    // Public like the base-class variant: fed by getSourceDevices() and
    // directly by the device-filtering regression tests.
    void addDevicePropertie(const std::string&, GstCaps*, GstDevice*);

private:
    errorState getSourceDevices() override;

    errorState setSourceElement(const std::string&) override;
    errorState setCapsFilterElement(int32_t, int32_t) override;
};
