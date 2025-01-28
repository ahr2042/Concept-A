#pragma once

#include "GStreamerSink.h"
#include "ScreenSinks.h"

class GStreamerSinkScreen : public GStreamerSink
{
public:
	GStreamerSinkScreen(ScreenSinks);
private:
	errorState setSinkElement(std::string);

};