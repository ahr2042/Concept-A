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
