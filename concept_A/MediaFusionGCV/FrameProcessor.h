#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>

#include "Algorithm.h"
#include "Detector.h"

#include <map>
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
    // while streaming). Rejects the whole request and leaves the current chain
    // alone if any name is unknown — a typo used to yield a silently empty
    // chain that still reported success.
    bool                     setAlgorithms(const std::vector<std::string>& names);
    std::vector<std::string> activeAlgorithms() const;

    // Tuning for one stage, keyed by AlgorithmParam::key. Like the detector
    // config below these live here rather than in the stage, because
    // setAlgorithms() rebuilds the chain from names and the operator's settings
    // have to survive that. Values are clamped to the algorithm's schema;
    // false means an unknown algorithm or an unknown key.
    bool            setAlgorithmParams(const std::string& algo, const AlgorithmParams& values);
    AlgorithmParams algorithmParams(const std::string& algo) const;

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

    // What every reporting stage in the chain last produced, in chain order;
    // empty when no stage reports. Deliberately a list rather than "the stats":
    // more than one stage can report (a detector and a motion stage), and
    // returning only the first hid the others with no error and no clue.
    //
    // This does NOT take m_mutex — see m_statsStages below.
    std::vector<InferenceStats> stageStats() const;

private:
    static GstPadProbeReturn onBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer user);
    GstPadProbeReturn        processBuffer(GstPad* pad, GstPadProbeInfo* info);

    // Overrides only, not a full value set: a rebuilt stage starts at its own
    // defaults and these are replayed on top, so a later change to a schema
    // default is picked up instead of being pinned by a stale copy.
    std::map<std::string, AlgorithmParams>  m_algoParams;

    // shared_ptr rather than unique_ptr so stageStats() can hold a stage alive
    // without holding a lock — see m_statsStages.
    std::vector<std::shared_ptr<Algorithm>> m_algos;
    DetectorConfig                          m_detectorConfig;
    AccelBackend                            m_accel = AccelBackend::CPU;
    mutable std::mutex                      m_mutex;

    // The same stages again, behind their own lock, so a telemetry poll never
    // queues behind a frame: m_mutex is held for the WHOLE algorithm chain on
    // every buffer, so reading stats through it made the 1 Hz poll wait out a
    // frame's OpenCV work and report a sample time it had not actually taken
    // (docs/PERFORMANCE.md, P15).
    //
    // stageStats() copies these pointers, releases m_statsMutex, and only then
    // calls snapshotStats(). Shared ownership is what makes letting go of the
    // lock safe: setAlgorithms() may drop the chain meanwhile, and
    // ~DetectorAlgorithm joins a worker thread that can be mid-forward-pass, so
    // a raw pointer would be either a crash or a ~57 ms wait.
    //
    // Lock order is m_mutex -> m_statsMutex, in setAlgorithms() and nowhere
    // else; stageStats() takes only the second, so there is no cycle.
    std::vector<std::shared_ptr<Algorithm>> m_statsStages;
    mutable std::mutex                      m_statsMutex;
    GstVideoInfo                            m_info;
    bool                                    m_haveInfo = false;
    gulong                                  m_probeId  = 0;
};
