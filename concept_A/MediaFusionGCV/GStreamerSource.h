#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SourceType.h"
#include "errorState.h"

#include <string>
#include <sstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <iostream>



class GStreamerSource {
public:
    GStreamerSource() {};
    //// Public method to get the configured GStreamer source element
    //GstElement* getSourceElement();

    struct deviceProperties
    {
        std::string deviceName = "";     
        GstCaps* deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
    };

    virtual std::list<std::pair<std::string, std::string>> getDeviceInfoReadable();

    virtual int32_t getSourceDevices();   
    virtual int32_t setSourceElement(std::string) {};
    virtual void addDevicePropertie(std::string, GstCaps*) {};
      

    

    // GStreamer source element
    GstElement* sourceElement = nullptr;
    GstElement* capsFilter = nullptr;
    GstElement* converter = nullptr;

    //// Source configuration
    //SourceType sourceType;
    //std::string sourceConfig;

    // Helper to parse configurations (e.g., JSON, strings, or key-value pairs)
    //std::map<std::string, std::string> parseConfig(const std::string& config);


};