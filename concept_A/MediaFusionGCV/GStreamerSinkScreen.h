#pragma once

#include "GStreamerSink.h"
#include "ScreenSinks.h"

class GStreamerSinkScreen : public GStreamerSink
{
public:
    GStreamerSinkScreen(ScreenSinks);
private:
    errorState setSinkElement(const std::string&) override;
    errorState setCapsFilterElement(int32_t, int32_t) override;
};