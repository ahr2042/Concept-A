#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SourceType.h"
#include "SinkType.h"
#include "errorState.h"

#include "GStreamerSource.h"
#include "GStreamerSink.h"

#include <atomic>
#include <string>
#include <vector>
#include <iostream>

class PipelineManager {
public:
    PipelineManager(SourceType, SinkType, const char*);
    ~PipelineManager();

    errorState getSourceInformation(std::vector<std::pair<std::string, std::string>>&);
    errorState setSourceElement(const std::string&);
    errorState setSourceCaps(int32_t, int32_t);
    errorState setSinkElement(const std::string&);
    errorState setSinkCaps(int32_t, int32_t);
    errorState startStreaming();
    errorState stopStreaming();

private:
    GstElement*           pipeline       = nullptr;
    GThread*              pipelineThread = nullptr;
    GStreamerSource*       source         = nullptr;
    GStreamerSink*         sink           = nullptr;
    std::atomic<bool>     stopRequested  { false };

    errorState      buildPipeline();
    static gpointer startLoop(gpointer data);
};
