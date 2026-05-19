#include "GStreamerSourceCamera.h"

GStreamerSourceCamera::GStreamerSourceCamera()
{
#ifdef  _WIN32
    sourceElement = gst_element_factory_make("mfvideosrc",  "camera-source");
#elif __linux__
    sourceElement = gst_element_factory_make("v4l2src",     "camera-source");
#elif __APPLE__
    sourceElement = gst_element_factory_make("avfvideosrc", "camera-source");
#endif
    getSourceDevices();
}

GStreamerSourceCamera::~GStreamerSourceCamera()
{
    for (auto* dev : devicesContainer) {
        if (!dev) continue;
        if (dev->gstDevice)          g_object_unref(dev->gstDevice);
        if (dev->deviceCapabilities) gst_caps_unref(dev->deviceCapabilities);
        delete dev;
    }
    devicesContainer.clear();
}

errorState GStreamerSourceCamera::getSourceDevices()
{
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor)
        return errorState::CREATE_DEVICE_MONITOR_ERR;

    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    if (!gst_device_monitor_start(monitor)) {
        g_object_unref(monitor);
        return errorState::START_DEVICE_MONITOR_ERR;
    }

    GList* devices = gst_device_monitor_get_devices(monitor);
    if (!devices) {
        gst_device_monitor_stop(monitor);
        g_object_unref(monitor);
        return errorState::NO_VIDEO_DEVICE_FOUND_ERR;
    }

    for (GList* l = devices; l; l = l->next) {
        GstDevice* device = GST_DEVICE(l->data);
        const gchar* name = gst_device_get_display_name(device);
        GstCaps* devCaps = gst_device_get_caps(device);
        addDevicePropertie(name ? name : "Unknown Device", devCaps, device);
        if (devCaps) gst_caps_unref(devCaps);
        g_object_unref(device);
    }
    g_list_free(devices);

    gst_device_monitor_stop(monitor);
    g_object_unref(monitor);
    return errorState::NO_ERR;
}

static gboolean format_structure_field(GQuark field_id, const GValue* value, gpointer user_data)
{
    auto* dev = static_cast<GStreamerSource::deviceProperties*>(user_data);
    gchar* value_str = value ? g_strdup_value_contents(value) : nullptr;
    if (value_str) {
        dev->formattedDeviceCapabilities
            << "        " << g_quark_to_string(field_id) << ": " << value_str << "\n";
        g_free(value_str);
    }
    return TRUE;
}

void GStreamerSourceCamera::addDevicePropertie(const std::string& deviceName,
                                               GstCaps* deviceCaps, GstDevice* device)
{
    if (deviceName == "Unknown Device" && (!deviceCaps || gst_caps_is_empty(deviceCaps)))
        return;

    deviceProperties* dev = new deviceProperties;
    dev->deviceName  = deviceName;
    dev->gstDevice   = device ? GST_DEVICE(g_object_ref(device)) : nullptr;
    dev->deviceCapabilities = gst_caps_copy(deviceCaps);

    guint n = gst_caps_get_size(dev->deviceCapabilities);
    for (guint i = 0; i < n; ++i) {
        const GstStructure* structure = gst_caps_get_structure(dev->deviceCapabilities, i);
        if (!structure) continue;
        if (i > 0) dev->formattedDeviceCapabilities << ";";
        const gchar* mediaType = gst_structure_get_name(structure);
        dev->formattedDeviceCapabilities
            << "    Capability " << i + 1 << ":\n"
            << "      Media Type: " << (mediaType ? mediaType : "Unknown") << "\n";
        gst_structure_foreach(structure, format_structure_field, dev);
    }

    devicesContainer.push_back(dev);
}

std::vector<std::pair<std::string, std::string>> GStreamerSourceCamera::getDeviceInfoReadable()
{
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(devicesContainer.size());
    for (const auto* dev : devicesContainer) {
        if (dev)
            result.emplace_back(dev->deviceName, dev->formattedDeviceCapabilities.str());
    }
    return result;
}

errorState GStreamerSourceCamera::setSourceElement(const std::string& /*deviceName*/)
{
    // Element is already created in the constructor; device selection is done via setCapsFilterElement.
    return sourceElement ? errorState::NO_ERR : errorState::NULLPTR_ERR;
}

errorState GStreamerSourceCamera::setCapsFilterElement(int32_t deviceId, int32_t capIndex)
{
    if (!capsFilter)
        return errorState::NULLPTR_ERR;

    if (deviceId < 0 || capIndex < 0
        || static_cast<size_t>(deviceId) >= devicesContainer.size()
        || !devicesContainer[deviceId])
        return errorState::NULLPTR_ERR;

    if (devicesContainer[deviceId]->gstDevice && sourceElement)
        gst_device_reconfigure_element(devicesContainer[deviceId]->gstDevice, sourceElement);

    std::string caps = getCapsStringAtIndex(deviceId, static_cast<guint>(capIndex));
    if (caps.empty())
        return errorState::EMPTY_STRING_ERR;

    GstCaps* capsPtr = gst_caps_from_string(caps.c_str());
    if (!capsPtr)
        return errorState::OBJECT_CREATION_ERR;

    g_object_set(capsFilter, "caps", capsPtr, NULL);
    gst_caps_unref(capsPtr);
    return errorState::NO_ERR;
}
