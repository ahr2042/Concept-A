#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera();    
    std::list<std::pair<std::string, std::string>> getDeviceInfoReadable();

private:
    
    int32_t getSourceDevices();
    void addDevicePropertie(std::string, GstCaps*);
    std::vector<deviceProperties*> devicesContainer;
    

      
};