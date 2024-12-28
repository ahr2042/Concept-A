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
    
    
    int32_t setSourceElement(std::string);
    //int32_t setConvertElement(std::string);
    int32_t setCapsFilterElement(int32_t,int32_t);

      
};