#include "pch.h"
#include "GStreamerSource.h"



// Constructor
GStreamerSource::GStreamerSource(SourceType type, const std::string& config)
    : sourceType(type), sourceConfig(config), sourceElement(nullptr) {
    switch (sourceType) {
    case SourceType::File:
        
        break;
    case SourceType::Camera:
        
        break;
    case SourceType::Network:
        
        break;
    case SourceType::Screen:
        
        break;
    case SourceType::Test:
        
        break;
    case SourceType::Custom:
        sourceElement = createCustomSource(config);
        break;
    default:
        throw std::invalid_argument("Unsupported source type");
    }

    if (!sourceElement) {
        throw std::runtime_error("Failed to create GStreamer source element");
    }
}

// Destructor
GStreamerSource::~GStreamerSource() {
    if (sourceElement) {
        gst_object_unref(sourceElement);
    }
}

// Public method to get the source element
GstElement* GStreamerSource::getSourceElement() {
    return sourceElement;
}

//// Private helper to create a file source
//GstElement* GStreamerSource::createFileSource(const std::string& config) {
//    std::map<std::string, std::string> options = parseConfig(config);
//    std::string location = options["location"];
//
//    GstElement* fileSrc = gst_element_factory_make("filesrc", "file-source");
//    if (!fileSrc) return nullptr;
//
//    g_object_set(G_OBJECT(fileSrc), "location", location.c_str(), nullptr);
//    return fileSrc;
//}
//
//// Private helper to create a camera source
//GstElement* GStreamerSource::createCameraSource(const std::string& config)
//{
//    std::map<std::string, std::string> options = parseConfig(config);
//
//#ifdef _WIN32
//    GstElement* cameraSrc = gst_element_factory_make("ksvideosrc", "camera-source");
//#else
//    GstElement* cameraSrc = gst_element_factory_make("v4l2src", "camera-source");
//#endif
//    if (!cameraSrc) return nullptr;
//
//    if (options.count("device")) {
//        g_object_set(G_OBJECT(cameraSrc), "device", options["device"].c_str(), nullptr);
//    }
//    return cameraSrc;
//}
//
//// Private helper to create a network source
//GstElement* GStreamerSource::createNetworkSource(const std::string& config) {
//    std::map<std::string, std::string> options = parseConfig(config);
//    std::string location = options["location"];
//
//    GstElement* netSrc = gst_element_factory_make("rtspsrc", "network-source");
//    if (!netSrc) return nullptr;
//
//    g_object_set(G_OBJECT(netSrc), "location", location.c_str(), nullptr);
//    return netSrc;
//}
//
//// Private helper to create a screen source
//GstElement* GStreamerSource::createScreenSource(const std::string& config) {
//#ifdef _WIN32
//    GstElement* screenSrc = gst_element_factory_make("dx9screencapsrc", "screen-source");
//#else
//    GstElement* screenSrc = gst_element_factory_make("ximagesrc", "screen-source");
//#endif
//    return screenSrc ? screenSrc : nullptr;
//}
//
//// Private helper to create a test source
//GstElement* GStreamerSource::createTestSource(const std::string& config) {
//    std::map<std::string, std::string> options = parseConfig(config);
//
//    GstElement* testSrc = gst_element_factory_make("videotestsrc", "test-source");
//    if (!testSrc) return nullptr;
//
//    if (options.count("pattern")) {
//        g_object_set(G_OBJECT(testSrc), "pattern", std::stoi(options["pattern"]), nullptr);
//    }
//    return testSrc;
//}
//
//// Private helper to create a custom source
//GstElement* GStreamerSource::createCustomSource(const std::string& config) {
//    std::map<std::string, std::string> options = parseConfig(config);
//
//    GstElement* customSrc = gst_element_factory_make("appsrc", "custom-source");
//    return customSrc ? customSrc : nullptr;
//}
//
//// Helper to parse configurations
//std::map<std::string, std::string> GStreamerSource::parseConfig(const std::string& config) {
//    std::map<std::string, std::string> options;
//
//    // Very simple key=value;key=value parsing (extend as needed)
//    std::istringstream stream(config);
//    std::string keyValue;
//    while (std::getline(stream, keyValue, ';')) {
//        rsize_t delimiterPos = keyValue.find('=');
//        std::string key = keyValue.substr(0, delimiterPos);
//        std::string value = keyValue.substr(delimiterPos + 1);
//        options[key] = value;
//    }
//
//    return options;
//}


GStreamerSource* GStreamerSource::createElement(std::string deviceName)
{
    
    return nullptr;
}
