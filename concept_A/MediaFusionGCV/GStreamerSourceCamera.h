#pragma once
#include "GStreamerSource.h"

class GStreamerSourceCamera : public GStreamerSource
{
public:
    GStreamerSourceCamera();
    ~GStreamerSourceCamera();
    std::list<std::pair<std::string, std::string>> getDeviceInfoReadable();

private:

    errorState getSourceDevices();
    void addDevicePropertie(std::string, GstCaps*, GstDevice*);
    
    
    errorState setSourceElement(std::string);
    //int32_t setConvertElement(std::string);
    errorState setCapsFilterElement(int32_t,int32_t);

      
};