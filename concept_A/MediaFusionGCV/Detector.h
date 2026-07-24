#pragma once

#include "Algorithm.h"
#include "DetectorConfig.h"
#include "InferenceBackend.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

// The object-detection stage, registered in the algorithm factory as "detect".
// The actual forward pass lives behind IInferenceBackend (cv::dnn on CPU,
// ncnn+Vulkan on GPU, CUDA placeholder) so this class only orchestrates:
// scheduling, latency/telemetry, and drawing.
//
// Inference does NOT run on the streaming thread. apply() hands a copy of the
// frame to a worker thread when that worker is idle and immediately overlays the
// most recent completed result, so a 40 ms inference never becomes 40 ms of
// pipeline backpressure — the stream keeps the camera's frame rate and boxes lag
// by a frame or two. framesSkipped in InferenceStats counts frames drawn from a
// previous result, which is what makes the console's inference-latency figure
// meaningful separately from FPS.
//
// The backend is chosen by the resolved acceleration selection (setAccel); a GPU
// engine that cannot load a model falls back to cv::dnn so the stage never goes
// dark just because the GPU weights are missing.
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

    // Records which acceleration backend the next model load should target.
    void setAccel(AccelBackend backend) override;

    // Loads `cfg.model` on the selected backend and applies the thresholds.
    // Returns false if the graph cannot be read; the reason is left in the stats'
    // lastError. An empty model name unloads the net and leaves the stage a no-op.
    bool configure(const DetectorConfig& cfg);

    DetectorConfig config() const;

private:
    void inferenceLoop();
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& dets) const;
    void publish(std::vector<Detection> dets, double elapsedMs);

    // The backend is touched only by the worker thread and by configure(), which
    // holds m_netMutex to swap it — never concurrently with a forward pass.
    // apply() must never take this mutex: it would stall the streaming thread for
    // a whole forward pass, which is exactly what the worker exists to avoid, so
    // readiness is published separately as an atomic.
    mutable std::mutex                 m_netMutex;
    std::unique_ptr<IInferenceBackend> m_backend;
    std::atomic<bool>                  m_netReady { false };
    std::atomic<AccelBackend>          m_accelBackend { AccelBackend::CPU };

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
    std::string            m_activeBackend; // engine that actually loaded it
    std::vector<Detection> m_detections;
    double                 m_inferenceMs    = 0.0;
    double                 m_avgInferenceMs = 0.0;
    uint64_t               m_framesInferred = 0;
    uint64_t               m_framesSkipped  = 0;
    std::string            m_lastError;

    std::thread m_worker;
};
