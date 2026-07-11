#include "GStreamerSinkScreen.h"

GStreamerSinkScreen::GStreamerSinkScreen(ScreenSinks screenSink)
{
	switch (screenSink)
	{
	case AUTOVIDEOSINK:
		sinkElement = gst_element_factory_make("autovideosink", "sink");
		break;
	case XVIMAGESINK:		
		break;
	case GLIMAGESINK:
		break;
	case WAYLANDSINK:
		break;
	case CACASINK:
		break;
	case QT5GLSINK:
		break;
	default:
		break;
	}
}

errorState GStreamerSinkScreen::setSinkElement(const std::string& deviceName)
{
    if (sinkElement && !deviceName.empty()) {
        g_object_set(sinkElement, "device-name", deviceName.c_str(), NULL);
        return errorState::NO_ERR;
    }
    return errorState::NULLPTR_ERR;
}

errorState GStreamerSinkScreen::setCapsFilterElement(int32_t deviceID, int32_t capsIndex)
{
    if (!capsFilter)
        return errorState::NULLPTR_ERR;

    if (devicesContainer.empty())
        getSinkDevices();

    if (deviceID < 0 || capsIndex < 0
        || static_cast<size_t>(deviceID) >= devicesContainer.size()
        || !devicesContainer[deviceID])
        return errorState::NULLPTR_ERR;

    const deviceProperties* dev = devicesContainer[deviceID];
    if (!dev->deviceCapabilities
        || static_cast<guint>(capsIndex) >= gst_caps_get_size(dev->deviceCapabilities))
        return errorState::NULLPTR_ERR;

    GstCaps* caps = gst_caps_new_full(
        gst_structure_copy(gst_caps_get_structure(dev->deviceCapabilities, static_cast<guint>(capsIndex))),
        NULL);
    g_object_set(capsFilter, "caps", caps, NULL);
    gst_caps_unref(caps);
    return errorState::NO_ERR;
}
