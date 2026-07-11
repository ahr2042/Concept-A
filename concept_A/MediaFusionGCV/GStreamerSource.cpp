#include "GStreamerSource.h"

GStreamerSource::~GStreamerSource()
{
    if (sourceElement) { gst_object_unref(sourceElement); sourceElement = nullptr; }
    if (capsFilter)    { gst_object_unref(capsFilter);    capsFilter    = nullptr; }
    if (converter)     { gst_object_unref(converter);     converter     = nullptr; }
}



std::string GStreamerSource::getCapsStringAtIndex(int32_t deviceID, guint index)
{
	if (deviceID < 0 || static_cast<size_t>(deviceID) >= devicesContainer.size())
		return std::string();

	const deviceProperties* dev = devicesContainer[deviceID];
	if (!dev || !dev->deviceCapabilities)
		return std::string();

	if (index >= gst_caps_get_size(dev->deviceCapabilities))
		return std::string();

	const GstStructure* structure = gst_caps_get_structure(dev->deviceCapabilities, index);
	if (!structure)
		return std::string();

	gchar* capsString = gst_structure_to_string(structure);
	if (!capsString)
		return std::string();

	std::string result(capsString);
	g_free(capsString);
	return result;
}


