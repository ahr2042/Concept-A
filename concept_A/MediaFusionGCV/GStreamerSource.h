#pragma once

#include "PipelineManager.h"



class GStreamerSource {
public:
    GStreamerSource() {};
    //GStreamerSource(SourceType type, const std::string& config = "");
    ~GStreamerSource();



    // Public method to get the configured GStreamer source element
    GstElement* getSourceElement();

    struct deviceProperties
    {
        std::string deviceName = "";     
        GstCaps* deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
    };


    
    virtual int32_t getSourceDevices() {};   
    virtual GStreamerSource* createElement(std::string deviceName) {};
    virtual void addDevicePropertie(std::string, GstCaps*) {};
    virtual std::list<std::pair<std::string, std::string>> getDeviceInfoReadable() {};    

    

    // GStreamer source element
    GstElement* sourceElement;

    // Source configuration
    SourceType sourceType;
    std::string sourceConfig;

    // Helper to parse configurations (e.g., JSON, strings, or key-value pairs)
    std::map<std::string, std::string> parseConfig(const std::string& config);


};