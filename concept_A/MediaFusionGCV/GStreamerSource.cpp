#include "pch.h"
#include "GStreamerSource.h"

static std::vector<GStreamerSource::deviceProperties*> devicesContainer;


// Constructor
GStreamerSource::GStreamerSource(SourceType type, const std::string& config)
    : sourceType(type), sourceConfig(config), sourceElement(nullptr) {
    switch (sourceType) {
    case SourceType::File:
        sourceElement = createFileSource(config);
        break;
    case SourceType::Camera:
        sourceElement = createCameraSource(config);
        break;
    case SourceType::Network:
        sourceElement = createNetworkSource(config);
        break;
    case SourceType::Screen:
        sourceElement = createScreenSource(config);
        break;
    case SourceType::Test:
        sourceElement = createTestSource(config);
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

// Private helper to create a file source
GstElement* GStreamerSource::createFileSource(const std::string& config) {
    std::map<std::string, std::string> options = parseConfig(config);
    std::string location = options["location"];

    GstElement* fileSrc = gst_element_factory_make("filesrc", "file-source");
    if (!fileSrc) return nullptr;

    g_object_set(G_OBJECT(fileSrc), "location", location.c_str(), nullptr);
    return fileSrc;
}

// Private helper to create a camera source
GstElement* GStreamerSource::createCameraSource(const std::string& config)
{
    std::map<std::string, std::string> options = parseConfig(config);

#ifdef _WIN32
    GstElement* cameraSrc = gst_element_factory_make("ksvideosrc", "camera-source");
#else
    GstElement* cameraSrc = gst_element_factory_make("v4l2src", "camera-source");
#endif
    if (!cameraSrc) return nullptr;

    if (options.count("device")) {
        g_object_set(G_OBJECT(cameraSrc), "device", options["device"].c_str(), nullptr);
    }
    return cameraSrc;
}

// Private helper to create a network source
GstElement* GStreamerSource::createNetworkSource(const std::string& config) {
    std::map<std::string, std::string> options = parseConfig(config);
    std::string location = options["location"];

    GstElement* netSrc = gst_element_factory_make("rtspsrc", "network-source");
    if (!netSrc) return nullptr;

    g_object_set(G_OBJECT(netSrc), "location", location.c_str(), nullptr);
    return netSrc;
}

// Private helper to create a screen source
GstElement* GStreamerSource::createScreenSource(const std::string& config) {
#ifdef _WIN32
    GstElement* screenSrc = gst_element_factory_make("dx9screencapsrc", "screen-source");
#else
    GstElement* screenSrc = gst_element_factory_make("ximagesrc", "screen-source");
#endif
    return screenSrc ? screenSrc : nullptr;
}

// Private helper to create a test source
GstElement* GStreamerSource::createTestSource(const std::string& config) {
    std::map<std::string, std::string> options = parseConfig(config);

    GstElement* testSrc = gst_element_factory_make("videotestsrc", "test-source");
    if (!testSrc) return nullptr;

    if (options.count("pattern")) {
        g_object_set(G_OBJECT(testSrc), "pattern", std::stoi(options["pattern"]), nullptr);
    }
    return testSrc;
}

// Private helper to create a custom source
GstElement* GStreamerSource::createCustomSource(const std::string& config) {
    std::map<std::string, std::string> options = parseConfig(config);

    GstElement* customSrc = gst_element_factory_make("appsrc", "custom-source");
    return customSrc ? customSrc : nullptr;
}

// Helper to parse configurations
std::map<std::string, std::string> GStreamerSource::parseConfig(const std::string& config) {
    std::map<std::string, std::string> options;

    // Very simple key=value;key=value parsing (extend as needed)
    std::istringstream stream(config);
    std::string keyValue;
    while (std::getline(stream, keyValue, ';')) {
        rsize_t delimiterPos = keyValue.find('=');
        std::string key = keyValue.substr(0, delimiterPos);
        std::string value = keyValue.substr(delimiterPos + 1);
        options[key] = value;
    }

    return options;
}

int GStreamerSource::getDevices()
{
    GstDeviceMonitor* device_monitor = gst_device_monitor_new();
    if (!device_monitor) {
        return CREATE_DEVICE_MONITOR_ERR;
    }

    // Add a filter for video sources
    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(device_monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    // Start the device monitor
    if (!gst_device_monitor_start(device_monitor)) {
        g_object_unref(device_monitor);
        return START_DEVICE_MONITOR_ERR;
    }

    // Get a list of devices
    GList* devices = gst_device_monitor_get_devices(device_monitor);
    if (devices) {
        for (GList* l = devices; l != nullptr; l = l->next) {
            GstDevice* device = GST_DEVICE(l->data);
            const gchar* name = gst_device_get_display_name(device);
            GstCaps* caps = gst_device_get_caps(device);
            addDevicePropertie((name ? name : "Unknown Device"), caps);
            if (caps)
            {
                gst_caps_unref(caps);
            }
            g_object_unref(device); // Free the device when done
        }
        g_list_free(devices); // Free the list itself
    }
    else {
        return NO_VIDEO_DEVICE_FOUND_ERR;
    }

    // Stop and clean up
    gst_device_monitor_stop(device_monitor);
    g_object_unref(device_monitor);

}

GStreamerSource* GStreamerSource::createElement(std::string deviceName)
{
    
    return nullptr;
}



void GStreamerSource::addDevicePropertie(std::string deviceName, GstCaps* deviceCaps)
{
    if (deviceName == "Unknown Device" && (deviceCaps == nullptr || gst_caps_is_empty(deviceCaps)))
    {
        return;
    }

    deviceProperties* currentDevice = new deviceProperties;
    currentDevice->deviceName = deviceName;
    currentDevice->deviceCapabilities = gst_caps_new_empty();
    for (guint i = 0; i < gst_caps_get_size(deviceCaps); ++i) {
        GstStructure* structure = gst_structure_copy(gst_caps_get_structure(deviceCaps, i));
        gst_caps_append_structure(currentDevice->deviceCapabilities, structure);
    }
    devicesContainer.push_back(currentDevice);
}


std::string GStreamerSource::getDeviceInfoReadable(int deviceId, deviceProperties* vidDevice)
{
    if (vidDevice == nullptr)
    {
        return std::string();
    }    
    for (guint i = 0; i < gst_caps_get_size(vidDevice->deviceCapabilities); ++i) {
        const GstStructure* structure = gst_caps_get_structure(vidDevice->deviceCapabilities, i);
        if (!structure) {
            std::cerr << "    Failed to retrieve structure from caps." << std::endl;
            continue;
        }
        if (i > 0)
        {
            devicesContainer[deviceId]->formattedDeviceCapabilities << ";";
        }
        devicesContainer[deviceId]->formattedDeviceCapabilities << "    Capability " << i + 1 << ":" << std::endl;

        // Print the media type (name of the structure)
        const gchar* name = gst_structure_get_name(structure);
        if (name) {
            devicesContainer[deviceId]->formattedDeviceCapabilities << "      Media Type: " << name << std::endl;
        }
        else {
            devicesContainer[deviceId]->formattedDeviceCapabilities << "      Media Type: Unknown" << std::endl;
        }        
        // Process and print all fields in the structure
        gst_structure_foreach(structure, process_structure_field, devicesContainer[deviceId]);
    }
    return devicesContainer[deviceId]->formattedDeviceCapabilities.str();
}

gboolean GStreamerSource::process_structure_field(GQuark field_id, const GValue* value, gpointer deviceContainer) {
    if (!value) {
        std::cerr << "  Null value encountered in structure field: " << field_id << std::endl;
        ((GStreamerSource::deviceProperties*)deviceContainer)->formattedDeviceCapabilities << "  Null value encountered in structure field: " << field_id << std::endl;
        return TRUE;  // Continue gracefully
    }
    gchar* value_str = g_strdup_value_contents(value);
    const gchar* fieldName = g_quark_to_string(field_id);
    if (value_str) {
        ((GStreamerSource::deviceProperties*)deviceContainer)->formattedDeviceCapabilities << "        " << fieldName << ": " << value_str << std::endl;
        g_free(value_str);
    }
    else {
        std::cerr << "  Failed to get value contents for field: " << fieldName << std::endl;
        ((GStreamerSource::deviceProperties*)deviceContainer)->formattedDeviceCapabilities << "  Failed to get value contents for field: " << fieldName << std::endl;
    }
    return TRUE;  // Continue iteration

}
