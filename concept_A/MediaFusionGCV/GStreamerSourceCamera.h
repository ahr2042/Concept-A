#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera();
    std::vector<deviceProperties*> devicesContainer;
    std::list<std::pair<std::string, std::string>> getDeviceInfoReadable();

private:
    
    int32_t getSourceDevices();
    void addDevicePropertie(std::string, GstCaps*);
    
     gboolean process_structure_field(GQuark, const GValue*, gpointer);

      
};