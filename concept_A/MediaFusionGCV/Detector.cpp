#include "Detector.h"
#include "ModelRegistry.h"

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdio>
#include <iostream>

namespace {

// A distinct, readable colour per class id (golden-angle walk over hue).
cv::Scalar classColor(int classId)
{
    const int hue = static_cast<int>(std::fmod(classId * 137.508, 180.0));
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, 200, 255)), bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const cv::Vec3b c = bgr.at<cv::Vec3b>(0, 0);
    return cv::Scalar(c[0], c[1], c[2]);
}

} // namespace

DetectorAlgorithm::DetectorAlgorithm()
{
    m_worker = std::thread(&DetectorAlgorithm::inferenceLoop, this);
}

DetectorAlgorithm::~DetectorAlgorithm()
{
    {
        std::lock_guard<std::mutex> lk(m_jobMutex);
        m_quit = true;
    }
    m_jobCv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

DetectorConfig DetectorAlgorithm::config() const
{
    std::lock_guard<std::mutex> lk(m_configMutex);
    return m_config;
}

void DetectorAlgorithm::setAccel(AccelBackend backend)
{
    // Just record it; configure() reads it when (re)building the net and the
    // factory maps it to a concrete engine. The backend is already resolved to
    // something runnable upstream (AcceleratorRegistry), and a GPU engine that
    // still cannot load a given model falls back to cv::dnn in configure().
    m_accelBackend.store(backend, std::memory_order_relaxed);
}

bool DetectorAlgorithm::configure(const DetectorConfig& cfg)
{
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        m_config = cfg;
    }

    // No model selected: unload and go quiet. The stage stays in the chain as a
    // no-op rather than erroring, so "detect" is always a valid algorithm name
    // even on a machine with no weights fetched.
    if (cfg.model.empty()) {
        m_netReady.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(m_netMutex);
            m_backend.reset();
        }
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_loadedModel.clear();
        m_activeBackend.clear();
        m_lastError.clear();
        m_detections.clear();
        return true;
    }

    ModelInfo info;
    if (!findModel(cfg.model, info)) {
        m_netReady.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_lastError   = "model not found: " + cfg.model;
        m_loadedModel.clear();
        m_detections.clear();
        return false;
    }

    const AccelBackend want = m_accelBackend.load(std::memory_order_relaxed);
    std::string        err;
    auto               backend = makeInferenceBackend(want);

    if (!backend || !backend->load(info, err)) {
        // Graceful fallback: a GPU engine that cannot load this model (e.g. the
        // .param/.bin have not been generated yet) drops to cv::dnn rather than
        // leaving the stage dark. CPU is the floor and is always present.
        if (want != AccelBackend::CPU) {
            std::cerr << "detector: " << (backend ? backend->backendName() : "gpu")
                      << " load failed (" << err << "); falling back to cv::dnn\n";
            backend = makeCvDnnBackend();
            err.clear();
        }
        if (!backend || !backend->load(info, err)) {
            m_netReady.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lk(m_resultMutex);
            m_lastError = "cannot load " + info.name + ": " + err;
            m_loadedModel.clear();
            m_detections.clear();
            return false;
        }
    }

    const std::string engine = backend->backendName();
    {
        std::lock_guard<std::mutex> lk(m_netMutex);
        m_backend = std::move(backend);
    }
    {
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_loadedModel    = info.name;
        m_activeBackend  = engine;
        m_lastError.clear();
        m_detections.clear();
        m_inferenceMs    = 0.0;
        m_avgInferenceMs = 0.0;
        m_framesInferred = 0;
        m_framesSkipped  = 0;
    }
    std::cerr << "detector: loaded " << info.name << " on " << engine << "\n";
    m_netReady.store(true, std::memory_order_release);
    return true;
}

void DetectorAlgorithm::apply(cv::Mat& frame)
{
    if (!m_netReady.load(std::memory_order_acquire))
        return;

    // Submit this frame only if the worker is idle; otherwise it is drawn from
    // the previous result and counted as skipped.
    bool skipped = false;
    {
        std::lock_guard<std::mutex> lk(m_jobMutex);
        if (m_jobPending) {
            skipped = true;
        } else {
            frame.copyTo(m_job);          // m_job's buffer is ours alone (see swap below)
            m_jobPending = true;
        }
    }
    if (skipped) {
        std::lock_guard<std::mutex> lk(m_resultMutex);
        ++m_framesSkipped;
    } else {
        m_jobCv.notify_one();
    }

    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        if (!m_config.drawBoxes)
            return;
    }

    std::vector<Detection> dets;
    {
        std::lock_guard<std::mutex> lk(m_resultMutex);
        dets = m_detections;
    }
    drawDetections(frame, dets);
}

bool DetectorAlgorithm::snapshotStats(InferenceStats& out) const
{
    out = InferenceStats{};
    out.modelLoaded = m_netReady.load(std::memory_order_acquire);

    std::lock_guard<std::mutex> lk(m_resultMutex);
    out.modelName      = m_loadedModel;
    out.lastError      = m_lastError;
    out.inferenceMs    = m_inferenceMs;
    out.avgInferenceMs = m_avgInferenceMs;
    out.framesInferred = m_framesInferred;
    out.framesSkipped  = m_framesSkipped;
    out.detections     = m_detections;
    out.objectCount    = static_cast<int>(m_detections.size());

    float sum = 0.0f;
    for (const auto& d : m_detections) sum += d.confidence;
    out.avgConfidence = m_detections.empty()
                      ? 0.0f
                      : sum / static_cast<float>(m_detections.size());
    return true;
}

void DetectorAlgorithm::inferenceLoop()
{
    cv::Mat job;
    while (true) {
        {
            std::unique_lock<std::mutex> lk(m_jobMutex);
            m_jobCv.wait(lk, [this] { return m_jobPending || m_quit; });
            if (m_quit)
                return;
            // Swap rather than copy: the worker walks away with the submitted
            // buffer and hands its previous one back for apply() to refill, so
            // neither side ever writes a buffer the other is reading.
            std::swap(job, m_job);
            m_jobPending = false;
        }

        if (job.empty())
            continue;

        const DetectorConfig   cfg = config();
        double                 elapsedMs = 0.0;
        std::vector<Detection> dets;
        std::string            error;
        {
            std::lock_guard<std::mutex> lk(m_netMutex);
            if (m_backend)
                m_backend->infer(job, cfg, dets, elapsedMs, error);
        }

        if (!error.empty()) {
            // A graph the backend accepted but cannot run: stop trying, and let
            // the console show why instead of flooding the log.
            m_netReady.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lk(m_resultMutex);
            m_lastError = "inference failed: " + error;
            m_detections.clear();
            continue;
        }
        publish(std::move(dets), elapsedMs);
    }
}

void DetectorAlgorithm::publish(std::vector<Detection> dets, double elapsedMs)
{
    std::lock_guard<std::mutex> lk(m_resultMutex);
    m_detections  = std::move(dets);
    m_inferenceMs = elapsedMs;
    // EMA over roughly the last ten inferences — steady enough to display, quick
    // enough to show a model swap.
    m_avgInferenceMs = (m_framesInferred == 0)
                     ? elapsedMs
                     : m_avgInferenceMs * 0.9 + elapsedMs * 0.1;
    ++m_framesInferred;
}

void DetectorAlgorithm::drawDetections(cv::Mat& frame, const std::vector<Detection>& dets) const
{
    for (const auto& d : dets) {
        const cv::Scalar color = classColor(d.classId);
        const cv::Rect   box(d.x, d.y, d.width, d.height);
        cv::rectangle(frame, box, color, 2);

        char text[160];
        std::snprintf(text, sizeof(text), "%s %.0f%%",
                      d.label.c_str(), static_cast<double>(d.confidence) * 100.0);

        int            baseline = 0;
        const cv::Size ts = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.45, 1, &baseline);
        // Caption above the box, or tucked inside when the box hugs the top edge.
        const int labelTop = (box.y - ts.height - 6 >= 0) ? box.y - ts.height - 6 : box.y;

        cv::rectangle(frame, cv::Rect(box.x, labelTop, ts.width + 8, ts.height + 6),
                      color, cv::FILLED);
        cv::putText(frame, text, cv::Point(box.x + 4, labelTop + ts.height + 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(16, 16, 16), 1, cv::LINE_AA);
    }
}
