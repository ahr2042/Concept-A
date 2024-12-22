#include "pch.h"
#include "GStreamerSource.h"




// Destructor
//GStreamerSource::~GStreamerSource() {
//    if (sourceElement) {
//        gst_object_unref(sourceElement);
//    }
//}
//
//// Public method to get the source element
//GstElement* GStreamerSource::getSourceElement() {
//    return sourceElement;
//}

std::list<std::pair<std::string, std::string>> GStreamerSource::getDeviceInfoReadable()
{
	return std::list<std::pair<std::string, std::string>>();
}

std::string GStreamerSource::getCapsStringAtIndex(int32_t deviceID, guint index)
{
	if (devicesContainer[deviceID] != nullptr)
	{
		const GstStructure* structure = gst_caps_get_structure(devicesContainer[deviceID]->deviceCapabilities, index);
		// Convert the structure to a string
		gchar* capsString = gst_structure_to_string(structure);
		if (!capsString)
		{
			return std::string();
		}
		std::string result(capsString);
		g_free(capsString);

		return result;
	}
	return std::string();
}


