#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera() { Sleep(100); };
	int32_t getSourceDevices();
private:
    void addDevicePropertie(std::string, GstCaps*);
    std::string getDeviceInfoReadable(int deviceId, deviceProperties*);
     gboolean process_structure_field(GQuark, const GValue*, gpointer);
};