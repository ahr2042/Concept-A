#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera();
    ~GStreamerSourceCamera() override;

    std::vector<std::pair<std::string, std::string>> getDeviceInfoReadable() override;

private:
    errorState getSourceDevices() override;
    void       addDevicePropertie(const std::string&, GstCaps*, GstDevice*);

    errorState setSourceElement(const std::string&) override;
    errorState setCapsFilterElement(int32_t, int32_t) override;
};
