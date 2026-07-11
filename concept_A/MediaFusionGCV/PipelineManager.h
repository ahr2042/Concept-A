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

class FrameProcessor;   // OpenCV processing stage (see FrameProcessor.h)

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

    // The endpoint a peer connects to for this pipeline's frames (app/IPC sink),
    // or "" for non-IPC sinks. Valid once the sink exists (after construction).
    std::string getStreamEndpoint() const;

    // OpenCV processing stage. Call before startStreaming(); setAlgorithms()
    // implies enabling. Algorithm names come from availableAlgorithms().
    void       setProcessingEnabled(bool enabled);
    errorState setAlgorithms(const std::vector<std::string>& names);

private:
    GstElement*           pipeline       = nullptr;
    GThread*              pipelineThread = nullptr;
    GStreamerSource*       source         = nullptr;
    GStreamerSink*         sink           = nullptr;
    FrameProcessor*        processor      = nullptr;
    std::atomic<bool>     stopRequested  { false };

    errorState      buildPipeline();
    static gpointer startLoop(gpointer data);
};
