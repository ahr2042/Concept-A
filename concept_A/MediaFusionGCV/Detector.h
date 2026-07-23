#pragma once

#include "Algorithm.h"
#include "ModelRegistry.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

// Runtime knobs for the inference stage, set from the control protocol.
struct DetectorConfig
{
    std::string model;                 // model stem or .onnx path; "" = idle
    float       confidence = 0.25f;    // keep detections at/above this score
    float       nms        = 0.45f;    // IoU threshold for non-maximum suppression
    bool        drawBoxes  = true;     // overlay boxes+labels on the frame
};

// The object-detection stage: a YOLO-family ONNX graph run through OpenCV's DNN
// module, registered in the algorithm factory as "detect".
//
// Inference does NOT run on the streaming thread. apply() hands a copy of the
// frame to a worker thread when that worker is idle and immediately overlays
// the most recent completed result, so a 40 ms inference never becomes 40 ms of
// pipeline backpressure — the stream keeps the camera's frame rate and boxes
// lag by a frame or two. framesSkipped in InferenceStats counts frames drawn
// from a previous result, which is what makes the console's inference-latency
// figure meaningful separately from FPS.
//
// Why OpenCV DNN and not ONNX Runtime: OpenCV is already a dependency of this
// library and its ONNX importer reads the same graphs, so the inference stage
// adds no new build dependency. The seam is this class — swapping in another
// runtime means reimplementing runInference() only.
class DetectorAlgorithm : public Algorithm
{
public:
    DetectorAlgorithm();
    ~DetectorAlgorithm() override;

    const char* name() const override { return "detect"; }

    // Overlays the newest available result and (if the worker is free) submits
    // this frame for inference. Cheap and non-blocking.
    void apply(cv::Mat& frame) override;

    bool snapshotStats(InferenceStats& out) const override;

    // Loads `cfg.model` and applies the thresholds. Returns false if the graph
    // cannot be read; the reason is left in the stats' lastError. An empty
    // model name unloads the net and leaves the stage a no-op.
    bool configure(const DetectorConfig& cfg);

    DetectorConfig config() const;

private:
    void                   inferenceLoop();
    std::vector<Detection> runInference(const cv::Mat& frame, double& elapsedMs);
    void                   drawDetections(cv::Mat& frame,
                                          const std::vector<Detection>& dets) const;
    void                   publish(std::vector<Detection> dets, double elapsedMs);

    // The net is touched only by the worker thread and by configure(), which
    // holds m_netMutex to swap it — never concurrently with a forward pass.
    // apply() must never take this mutex: it would stall the streaming thread
    // for a whole forward pass, which is exactly what the worker exists to
    // avoid, so readiness is published separately as an atomic.
    mutable std::mutex       m_netMutex;
    cv::dnn::Net             m_net;
    std::vector<std::string> m_labels;
    ModelInfo                m_model;
    std::atomic<bool>        m_netReady { false };

    mutable std::mutex m_configMutex;
    DetectorConfig     m_config;

    // Frame handoff to the worker: one slot, overwritten only when free.
    std::mutex              m_jobMutex;
    std::condition_variable m_jobCv;
    cv::Mat                 m_job;
    bool                    m_jobPending = false;
    bool                    m_quit       = false;

    mutable std::mutex     m_resultMutex;
    std::string            m_loadedModel;   // resolved stem of the resident net
    std::vector<Detection> m_detections;
    double                 m_inferenceMs    = 0.0;
    double                 m_avgInferenceMs = 0.0;
    uint64_t               m_framesInferred = 0;
    uint64_t               m_framesSkipped  = 0;
    std::string            m_lastError;

    std::thread m_worker;
};
