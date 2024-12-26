#pragma once

#include "GStreamerSink.h"
#include "ScreenSinks.h"

class GStreamerSinkScreen : public GStreamerSink
{
public:
	GStreamerSinkScreen() { GStreamerSinkScreen(ScreenSinks::AUTOVIDEOSINK); };
	GStreamerSinkScreen(ScreenSinks);
private:
};