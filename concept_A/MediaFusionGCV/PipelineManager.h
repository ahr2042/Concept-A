#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SourceType.h"
#include "SinkType.h"
#include "errorState.h"
#include "InferenceStats.h"
#include "AccelBackend.h"

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

    // Inference stage — the "detect" algorithm. The model is loaded here and
    // now (not lazily at start) so a bad name or an unreadable graph is
    // reported to the caller instead of failing silently mid-stream. An empty
    // name unloads the model and leaves the stage idle.
    errorState setDetectorModel(const std::string& modelNameOrPath);
    errorState setDetectorParams(float confidence, float nms, bool drawBoxes);

    // False when this pipeline has no inference stage in its chain.
    bool inferenceStats(InferenceStats& out) const;

    // Acceleration backend selection (auto/cpu/vulkan/cuda). Resolved against the
    // detected hardware at the next startStreaming(), since the choice shapes the
    // decode/convert topology built there and cannot change while PLAYING.
    errorState setAccel(AccelSelection selection);

private:
    GstElement*           pipeline       = nullptr;
    GThread*              pipelineThread = nullptr;
    GStreamerSource*       source         = nullptr;
    GStreamerSink*         sink           = nullptr;
    FrameProcessor*        processor      = nullptr;
    std::atomic<bool>     stopRequested  { false };
    AccelSelection        m_accelSel     = AccelSelection::AUTO;
    AccelBackend          m_backend      = AccelBackend::CPU;   // resolved at build
    std::vector<GstElement*> m_gpuElements;                     // optional GPU convert segment

    // Lazily creates the FrameProcessor, having it adopt the currently-selected
    // backend (m_backend) at once — so a detector configured next runs on the
    // right engine, not just from the next start().
    void            ensureProcessor();
    errorState      buildPipeline();
    // Builds the GPU colorspace-convert element chain for m_backend (e.g.
    // glupload!glcolorconvert!gldownload). Returns false — leaving out empty — if
    // the backend is CPU or any element is unavailable, so the caller stays on
    // the CPU topology. The returned elements carry a floating ref (not yet in a
    // bin); buildPipeline() takes ownership.
    bool            makeAccelSegment(std::vector<GstElement*>& out) const;
    static gpointer startLoop(gpointer data);
};
