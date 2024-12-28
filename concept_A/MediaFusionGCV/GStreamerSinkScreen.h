#pragma once

#include "GStreamerSink.h"
#include "ScreenSinks.h"

class GStreamerSinkScreen : public GStreamerSink
{
public:
	GStreamerSinkScreen(ScreenSinks);
private:
	int32_t setSinkElement(std::string);

};