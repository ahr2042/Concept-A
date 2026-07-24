#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>

#include "Algorithm.h"
#include "Detector.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Runs the (runtime-swappable) OpenCV algorithm chain over each frame, IN PLACE,
// via a pad probe on a BGR capsfilter. PipelineManager splices the capsfilter
// into the chain:
//
//     ... videoconvert ! [filterElement = capsfilter(BGR)] ! queue ! sink ...
//                                    ^ in-place buffer probe runs here
//
// In-place (vs an appsink/appsrc bridge) is deliberate: it keeps the buffer
// inside one pipeline segment, so downstream allocation negotiation is intact —
// e.g. unixfdsink's memfd buffers are modified and forwarded, not replaced with
// incompatible system-memory ones. The capsfilter forces BGR so the buffer maps
// straight to a cv::Mat CV_8UC3. This is the one CPU touch in the unixfd path.
class FrameProcessor
{
public:
    FrameProcessor();
    ~FrameProcessor();

    bool valid() const { return filterElement != nullptr; }

    // Spliced into the pipeline by PipelineManager (owned by the bin once added;
    // this class unrefs it on destruction). The probe is attached to its src pad.
    GstElement* filterElement = nullptr;   // capsfilter, "fp-bgr"

    // Replace the active algorithm chain by name (thread-safe; may be called
    // while streaming). Unknown names are skipped.
    void                     setAlgorithms(const std::vector<std::string>& names);
    std::vector<std::string> activeAlgorithms() const;

    // Resolved acceleration backend for the chain (CPU/Vulkan/CUDA). Stored and
    // pushed to every current and future stage, so a detector added later still
    // runs on the selected engine. Thread-safe.
    void                     setAccel(AccelBackend backend);

    // Inference-stage settings. They live here rather than inside the detector
    // because setAlgorithms() rebuilds the chain from names — the selected
    // model has to survive that, and has to be applied to a detector that
    // joins the chain later. Returns false if the model could not be loaded.
    bool           setDetectorConfig(const DetectorConfig& cfg);
    DetectorConfig detectorConfig() const;

    // Stats from the inference stage; false when no detector is in the chain.
    bool inferenceStats(InferenceStats& out) const;

private:
    static GstPadProbeReturn onBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer user);
    GstPadProbeReturn        processBuffer(GstPad* pad, GstPadProbeInfo* info);

    std::vector<std::unique_ptr<Algorithm>> m_algos;
    DetectorConfig                          m_detectorConfig;
    AccelBackend                            m_accel = AccelBackend::CPU;
    mutable std::mutex                      m_mutex;
    GstVideoInfo                            m_info;
    bool                                    m_haveInfo = false;
    gulong                                  m_probeId  = 0;
};
