#include "pch.h"
#include "GStreamerSink.h"

GStreamerSink::GStreamerSink()
{
    getSinkDevices();
}

int32_t GStreamerSink::getSinkDevices()
{
    // Get the registry
    GstRegistry* registry = gst_registry_get();
    GList* plugin_list = gst_registry_get_plugin_list(registry);

    // Iterate over all plugins
    for (GList* plugin_node = plugin_list; plugin_node != nullptr; plugin_node = plugin_node->next) {
        GstPlugin* plugin = GST_PLUGIN(plugin_node->data);
        if (!plugin) continue;

        // Get the list of features (elements) for this plugin
        GList* feature_list = gst_registry_get_feature_list_by_plugin(registry, gst_plugin_get_name(plugin));

        for (GList* feature_node = feature_list; feature_node != nullptr; feature_node = feature_node->next) {
            if (!GST_IS_ELEMENT_FACTORY(feature_node->data)) continue;

            GstElementFactory* factory = GST_ELEMENT_FACTORY(feature_node->data);
            if (!factory) continue;

            // Check if the element is a sink             
            if (g_strrstr(gst_element_factory_get_klass(factory), "Sink")) {
                const gchar* name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
                const gchar* longname = gst_element_factory_get_longname(factory);
                bool supports_video = false;
                std::vector<std::string> caps_info;                

                // Get static pad templates to extract caps
                const GList* pad_templates = gst_element_factory_get_static_pad_templates(factory);
                for (const GList* pad_node = pad_templates; pad_node != nullptr; pad_node = pad_node->next) {
                    GstStaticPadTemplate* pad_template = (GstStaticPadTemplate*)pad_node->data;
                    if (pad_template->direction == GST_PAD_SINK) {
                        GstCaps* caps = gst_static_pad_template_get_caps(pad_template);
                        gchar* caps_str = gst_caps_to_string(caps);
                        if (g_strrstr(caps_str, "video/x-raw")) {
                            addDevicePropertie(name, longname, caps);
                        }
                        g_free(caps_str);
                        gst_caps_unref(caps);
                    }
                }
            }
        }

        // Free feature list
        gst_plugin_feature_list_free(feature_list);
    }

    // Free plugin list
    gst_plugin_list_free(plugin_list);
}

void GStreamerSink::addDevicePropertie(std::string deviceName, std::string longName, GstCaps* deviceCaps)
{
    if (deviceName == "Unknown Device" && (deviceCaps == nullptr || gst_caps_is_empty(deviceCaps)))
    {
        return;
    }

    deviceProperties* currentDevice = new deviceProperties;
    currentDevice->deviceName = deviceName;
    currentDevice->longName = longName;
    currentDevice->deviceCapabilities = gst_caps_new_empty();
    for (guint i = 0; i < gst_caps_get_size(deviceCaps); ++i) {
        GstStructure* structure = gst_structure_copy(gst_caps_get_structure(deviceCaps, i));
        gst_caps_append_structure(currentDevice->deviceCapabilities, structure);
    }
    if (deviceCaps || !gst_caps_is_empty(deviceCaps)) {
        for (guint i = 0; i < gst_caps_get_size(deviceCaps); ++i) {
            GstStructure* structure = gst_caps_get_structure(deviceCaps, i);
            currentDevice->formattedDeviceCapabilities << "  - Format: " << gst_structure_get_name(structure) << std::endl;

            // Print fields within the structure
            const GQuark* field_names;
            gint n_fields = gst_structure_n_fields(structure);
            for (int j = 0; j < n_fields; ++j) {
                const gchar* field_name = gst_structure_nth_field_name(structure, j);
                const GValue* value = gst_structure_get_value(structure, field_name);

                gchar* value_str = g_strdup_value_contents(value);
                currentDevice->formattedDeviceCapabilities << "    " << field_name << ": " << value_str << std::endl;
                g_free(value_str);
            }
            currentDevice->formattedDeviceCapabilities << ";" << std::endl;
        }

    }

    devicesContainer.push_back(currentDevice);
}
