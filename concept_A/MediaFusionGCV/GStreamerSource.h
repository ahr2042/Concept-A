#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include <string>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <iostream>



#include "errorState.h"

class GStreamerSource {
public:
    struct deviceProperties
    {
        std::string deviceName = "";
        GstCaps* deviceCapabilities = nullptr;
        std::stringstream formattedDeviceCapabilities;
    };


    // Enum for source types
    enum class SourceType {
        File,
        Camera,
        Network,
        Screen,
        Test,
        Custom
    };
    
    GStreamerSource(SourceType type, const std::string& config = "");
    ~GStreamerSource();

    // Public method to get the configured GStreamer source element
    GstElement* getSourceElement();

private:
    // Private helper methods for each source type
    GstElement* createFileSource(const std::string& config);
    GstElement* createCameraSource(const std::string& config);
    GstElement* createNetworkSource(const std::string& config);
    GstElement* createScreenSource(const std::string& config);
    GstElement* createTestSource(const std::string& config);
    GstElement* createCustomSource(const std::string& config);

    static int getSourceDevices();
    static GStreamerSource* createElement(std::string deviceName);
    static void addDevicePropertie(std::string, GstCaps*);
    static std::string getDeviceInfoReadable(int deviceId, deviceProperties*);
    static gboolean process_structure_field(GQuark, const GValue*, gpointer);

    

    // GStreamer source element
    GstElement* sourceElement;

    // Source configuration
    SourceType sourceType;
    std::string sourceConfig;

    // Helper to parse configurations (e.g., JSON, strings, or key-value pairs)
    std::map<std::string, std::string> parseConfig(const std::string& config);


};