#pragma once
#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SinkType.h"
#include "errorState.h"

#include <string>
#include <sstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <iostream>


class GStreamerSink{
public:
    GStreamerSink();    
    struct deviceProperties
    {
        std::string deviceName = "";
        std::string longName = "";
        GstCaps* deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
    };
    std::vector<deviceProperties*> devicesContainer;
    //virtual std::list<std::pair<std::string, std::string>> getDeviceInfoReadable();

    virtual errorState getSinkDevices() ;
    virtual errorState setSinkElement(std::string) { return errorState::NO_ERR; };
    //virtual int32_t setConvertElement(std::string) ;
    virtual errorState setCapsFilterElement(int32_t, int32_t) { return errorState::NO_ERR; };
    virtual void addDevicePropertie(std::string, std::string, GstCaps*) ;

    //std::string getCapsStringAtIndex(int32_t deviceID, guint index);


    GstElement* sinkElement = nullptr;
    GstElement* capsFilter = nullptr;
    GstElement* converter = nullptr;


private:
};