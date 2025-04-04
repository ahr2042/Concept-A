#include "pch.h"
#include "GStreamerSourceCamera.h"

GStreamerSourceCamera::GStreamerSourceCamera()
{    
#ifdef  _WIN32
    sourceElement = gst_element_factory_make("mfvideosrc", "camera-source");
#elif __linux__ 
    sourceElement = gst_element_factory_make("v4l2src", "camera-source");
#elif __APPLE__
    sourceElement = gst_element_factory_make("avfvideosrc", "camera-source");
#endif     
    getSourceDevices();       
}

errorState GStreamerSourceCamera::getSourceDevices()
{
    GstDeviceMonitor* device_monitor = gst_device_monitor_new();
    if (!device_monitor) {
        return errorState::CREATE_DEVICE_MONITOR_ERR;
    }

    // Add a filter for video sources
    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(device_monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    // Start the device monitor
    if (!gst_device_monitor_start(device_monitor)) {
        g_object_unref(device_monitor);
        return errorState::START_DEVICE_MONITOR_ERR;
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
        return errorState::NO_VIDEO_DEVICE_FOUND_ERR;
    }

    // Stop and clean up
    gst_device_monitor_stop(device_monitor);
    g_object_unref(device_monitor);
    return errorState::NO_ERR;
}



void GStreamerSourceCamera::addDevicePropertie(std::string deviceName, GstCaps* deviceCaps)
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



gboolean process_structure_field(GQuark field_id, const GValue* value, gpointer deviceContainer) {
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

std::list<std::pair<std::string, std::string>> GStreamerSourceCamera::getDeviceInfoReadable()
{    
    std::list<std::pair<std::string, std::string>> devicesList;
    size_t totalDevicesNumebr = devicesContainer.size();
    for (int deviceId = 0; deviceId < totalDevicesNumebr; deviceId++)
    {
        if (devicesContainer[deviceId] == nullptr)
        {                        
            continue;
        }
        for (guint i = 0; i < gst_caps_get_size(devicesContainer[deviceId]->deviceCapabilities); ++i) {
            const GstStructure* structure = gst_caps_get_structure(devicesContainer[deviceId]->deviceCapabilities, i);
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
        devicesList.push_back(std::make_pair(devicesContainer[deviceId]->deviceName, devicesContainer[deviceId]->formattedDeviceCapabilities.str()));
    }
    return devicesList;
}



errorState GStreamerSourceCamera::setSourceElement(std::string deviceName)
{
    if (sourceElement != nullptr && !deviceName.empty())
    {
        g_object_set(sourceElement, "device-name", deviceName.c_str(), NULL);
        return errorState::NO_ERR;
    }
    return errorState::NULLPTR_ERR;
}

//int32_t GStreamerSourceCamera::setConvertElement(std::string deviceName)
//{
//    if (converter != nullptr && !deviceName.empty())
//    {
//        g_object_set(converter, "device-name", deviceName.c_str(), NULL);
//        return (int32_t)errorState::NO_ERR;
//    }
//    return (int32_t)errorState::NULLPTR_ERR;
//}


errorState GStreamerSourceCamera::setCapsFilterElement(int32_t deviceId, int32_t capIndex)
{
    if (capsFilter != nullptr)
    {
        std::string caps = getCapsStringAtIndex(deviceId, capIndex);
        if (caps.empty())
        {
            return errorState::EMPTY_STRING_ERR;
        }
        GstCaps* capsPtr = gst_caps_from_string(caps.c_str());
        if (!capsPtr)
        {
            return errorState::OBJECT_CREATION_ERR;
        }
        g_object_set(capsFilter, "caps", capsPtr, NULL);
        gst_caps_unref(capsPtr);
        return errorState::NO_ERR;
    }
    return errorState::NULLPTR_ERR;
}
