#include "pch.h"
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

int32_t GStreamerSinkScreen::setSinkElement(std::string deviceName)
{
	if (sinkElement != nullptr && !deviceName.empty())
	{
		g_object_set(sinkElement, "device-name", deviceName.c_str(), NULL);
		return (int32_t)errorState::NO_ERR;
	}
	return (int32_t)errorState::NULLPTR_ERR;
}
