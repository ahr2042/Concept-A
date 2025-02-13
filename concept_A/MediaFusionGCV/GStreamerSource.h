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
    std::vector<deviceProperties*> devicesContainer;
    virtual std::list<std::pair<std::string, std::string>> getDeviceInfoReadable() { return std::list<std::pair<std::string, std::string>>(); };

    virtual errorState getSourceDevices() { return errorState::NO_ERR; };
    virtual errorState setSourceElement(std::string) { return errorState::NO_ERR; };
    virtual errorState setConvertElement(std::string) { return errorState::NO_ERR; };
    virtual errorState setCapsFilterElement(int32_t, int32_t) { return errorState::NO_ERR; };
    virtual void addDevicePropertie(std::string, GstCaps*) {};
      
    std::string getCapsStringAtIndex(int32_t deviceID, guint index);
    

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