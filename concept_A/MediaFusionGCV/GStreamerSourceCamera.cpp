#include "pch.h"
#include "GStreamerSourceCamera.h"

int32_t GStreamerSourceCamera::getSourceDevices()
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
    return (int32_t)errorState::NO_ERR;
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

std::string GStreamerSourceCamera::getDeviceInfoReadable(int deviceId, deviceProperties* vidDevice)
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


gboolean GStreamerSourceCamera::process_structure_field(GQuark field_id, const GValue* value, gpointer deviceContainer) {
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
