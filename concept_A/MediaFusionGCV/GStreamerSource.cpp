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

int32_t GStreamerSource::getSourceDevices()
{
	return 0;
}
