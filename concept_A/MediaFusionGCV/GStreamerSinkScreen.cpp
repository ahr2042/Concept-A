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

errorState GStreamerSinkScreen::setSinkElement(std::string deviceName)
{
	if (sinkElement != nullptr && !deviceName.empty())
	{
		g_object_set(sinkElement, "device-name", deviceName.c_str(), NULL);
		return errorState::NO_ERR;
	}
	return errorState::NULLPTR_ERR;
}
