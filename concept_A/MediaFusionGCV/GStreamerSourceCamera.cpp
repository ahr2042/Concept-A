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
            << "    " << g_quark_to_string(field_id) << ": " << value_str << "\n";
        g_free(value_str);
    }
    return TRUE;
}

void GStreamerSourceCamera::addDevicePropertie(const std::string& deviceName,
                                               GstCaps* deviceCaps, GstDevice* device)
{
    if (deviceName == "Unknown Device" && (!deviceCaps || gst_caps_is_empty(deviceCaps)))
        return;

    GstCaps* rawCaps = gst_caps_new_empty();
    guint total = deviceCaps ? gst_caps_get_size(deviceCaps) : 0;
    for (guint i = 0; i < total; ++i) {
        const GstStructure* s = gst_caps_get_structure(deviceCaps, i);
        if (!s || g_strcmp0(gst_structure_get_name(s), "video/x-raw") != 0)
            continue;
        // Skip modes the CPU pipeline cannot negotiate: structures carrying a
        // special memory feature (PipeWire's memory:DMABuf with format=DMA_DRM)
        // lose that annotation on the way to the capsfilter and then match no
        // software element — start would fail with BUILD_PIPELINE_FAILED.
        // GPU DMABuf import is a planned feature.
        GstCapsFeatures* feat = gst_caps_get_features(deviceCaps, i);
        if (feat && !gst_caps_features_is_any(feat)
                 && !gst_caps_features_is_equal(feat, GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
            continue;
        if (g_strcmp0(gst_structure_get_string(s, "format"), "DMA_DRM") == 0)
            continue;
        gst_caps_append_structure(rawCaps, gst_structure_copy(s));
    }
    if (gst_caps_is_empty(rawCaps)) {
        gst_caps_unref(rawCaps);
        return;
    }

    // One physical camera can be reported twice: by the raw v4l2 provider
    // ("device.path") and again wrapped by PipeWire ("api.v4l2.path").
    // Identify the kernel node so duplicates collapse into a single entry.
    std::string nodePath;
    bool directV4l2 = false;
    if (device) {
        if (GstStructure* props = gst_device_get_properties(device)) {
            if (const gchar* p = gst_structure_get_string(props, "device.path")) {
                nodePath    = p;
                directV4l2  = true;
            } else if (const gchar* pw = gst_structure_get_string(props, "api.v4l2.path")) {
                nodePath = pw;
            }
            gst_structure_free(props);
        }
    }

    deviceProperties* dev = new deviceProperties;
    dev->deviceName         = deviceName;
    dev->gstDevice          = device ? GST_DEVICE(g_object_ref(device)) : nullptr;
    dev->deviceCapabilities = rawCaps;
    dev->nodePath           = nodePath;
    dev->directV4l2         = directV4l2;

    guint n = gst_caps_get_size(dev->deviceCapabilities);
    for (guint i = 0; i < n; ++i) {
        const GstStructure* structure = gst_caps_get_structure(dev->deviceCapabilities, i);
        if (!structure) continue;
        if (i > 0) dev->formattedDeviceCapabilities << "\n";
        const gchar* mediaType = gst_structure_get_name(structure);
        dev->formattedDeviceCapabilities
            << "  Cap [" << i << "]: " << (mediaType ? mediaType : "Unknown") << "\n";
        gst_structure_foreach(structure, format_structure_field, dev);
    }

    if (!nodePath.empty()) {
        for (auto& existing : devicesContainer) {
            if (!existing || existing->nodePath != nodePath)
                continue;
            // Same kernel node listed twice — keep the raw v4l2 entry
            // (direct kernel access, no session dependency).
            if (existing->directV4l2 || !directV4l2) {
                if (dev->gstDevice)          g_object_unref(dev->gstDevice);
                gst_caps_unref(dev->deviceCapabilities);
                delete dev;
            } else {
                if (existing->gstDevice)          g_object_unref(existing->gstDevice);
                if (existing->deviceCapabilities) gst_caps_unref(existing->deviceCapabilities);
                delete existing;
                existing = dev;
            }
            return;
        }
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

    // The enumerated device may come from a different provider than the
    // default source element (e.g. a PipeWire node while the element is
    // v4l2src) — reconfiguring across providers is a silent no-op. Let the
    // device build its own matching element and swap it in; this only works
    // while the element is not yet inside the pipeline bin.
    if (GstDevice* gstDev = devicesContainer[deviceId]->gstDevice) {
        if (sourceElement && !GST_OBJECT_PARENT(sourceElement)) {
            if (GstElement* fromDev = gst_device_create_element(gstDev, "camera-source")) {
                gst_object_unref(sourceElement);
                sourceElement = fromDev;
            } else if (sourceElement) {
                gst_device_reconfigure_element(gstDev, sourceElement);
            }
        } else if (sourceElement) {
            gst_device_reconfigure_element(gstDev, sourceElement);
        }
        if (sourceElement)
            if (GstElementFactory* f = gst_element_get_factory(sourceElement))
                std::cerr << "camera-source element: " << GST_OBJECT_NAME(f)
                          << " for device '" << devicesContainer[deviceId]->deviceName << "'\n";
    }

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
