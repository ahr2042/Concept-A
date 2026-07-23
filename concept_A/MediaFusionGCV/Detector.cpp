#include "Detector.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace {

// Pads to a square with the image at the top-left corner. Keeping the padding
// on the bottom/right means detections map back to frame coordinates with one
// uniform scale factor and no centring offset to undo.
cv::Mat squarePadded(const cv::Mat& src)
{
    const int side = std::max(src.cols, src.rows);
    if (side == src.cols && side == src.rows)
        return src;

    cv::Mat square = cv::Mat::zeros(side, side, src.type());
    src.copyTo(square(cv::Rect(0, 0, src.cols, src.rows)));
    return square;
}

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

bool DetectorAlgorithm::configure(const DetectorConfig& cfg)
{
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        m_config = cfg;
    }

    // No model selected: unload and go quiet. The stage stays in the chain as
    // a no-op rather than erroring, so "detect" is always a valid algorithm
    // name even on a machine with no weights fetched.
    if (cfg.model.empty()) {
        m_netReady.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(m_netMutex);
            m_net = cv::dnn::Net();
            m_labels.clear();
            m_model = ModelInfo{};
        }
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_loadedModel.clear();
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

    cv::dnn::Net net;
    try {
        net = cv::dnn::readNetFromONNX(info.path);
    } catch (const cv::Exception& e) {
        m_netReady.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_lastError = std::string("cannot read ") + info.path + ": " + e.what();
        m_loadedModel.clear();
        m_detections.clear();
        return false;
    }

    if (net.empty()) {
        m_netReady.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_lastError = "empty network: " + info.path;
        m_loadedModel.clear();
        m_detections.clear();
        return false;
    }

    // CPU inference: this rig is AMD, so there is no CUDA path, and OpenCL
    // through DNN_TARGET_OPENCL needs an ICD that is not guaranteed present.
    // The console's GPU_ACCELERATION control stays PLANNED for that reason.
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    std::vector<std::string> labels = loadLabels(info.labelsPath);

    {
        std::lock_guard<std::mutex> lk(m_netMutex);
        m_net    = std::move(net);
        m_labels = std::move(labels);
        m_model  = info;
    }
    {
        std::lock_guard<std::mutex> lk(m_resultMutex);
        m_loadedModel    = info.name;
        m_lastError.clear();
        m_detections.clear();
        m_inferenceMs    = 0.0;
        m_avgInferenceMs = 0.0;
        m_framesInferred = 0;
        m_framesSkipped  = 0;
    }
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

        double                 elapsedMs = 0.0;
        std::vector<Detection> dets;
        std::string            error;
        {
            std::lock_guard<std::mutex> lk(m_netMutex);
            if (!m_net.empty()) {
                try {
                    dets = runInference(job, elapsedMs);
                } catch (const cv::Exception& e) {
                    error = e.what();
                }
            }
        }

        if (!error.empty()) {
            // A graph the importer accepted but cannot run: stop trying, and
            // let the console show why instead of flooding the log.
            m_netReady.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lk(m_resultMutex);
            m_lastError = "inference failed: " + error;
            m_detections.clear();
            continue;
        }
        publish(std::move(dets), elapsedMs);
    }
}

// Called with m_netMutex held.
std::vector<Detection> DetectorAlgorithm::runInference(const cv::Mat& frame, double& elapsedMs)
{
    const auto start = std::chrono::steady_clock::now();

    const DetectorConfig cfg       = config();
    const int            inputSize = m_model.inputSize > 0 ? m_model.inputSize : 640;

    cv::Mat blob;
    cv::dnn::blobFromImage(squarePadded(frame), blob, 1.0 / 255.0,
                           cv::Size(inputSize, inputSize), cv::Scalar(),
                           /*swapRB=*/true, /*crop=*/false);

    std::vector<cv::Mat> outputs;
    m_net.setInput(blob);
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());

    elapsedMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();

    if (outputs.empty() || outputs[0].dims != 3)
        return {};

    // Two layouts are in the wild for YOLO ONNX exports:
    //   v5: [1, anchors, 5 + classes] — box, objectness, then class scores
    //   v8: [1, 4 + classes, anchors] — transposed, and with no objectness
    // Which one this is follows from which dimension is larger.
    cv::Mat out         = outputs[0];
    int     rows        = out.size[1];
    int     dimensions  = out.size[2];
    bool    hasObjectness = true;

    if (dimensions > rows) {
        hasObjectness = false;
        std::swap(rows, dimensions);
        out = out.reshape(1, dimensions);
        cv::transpose(out, out);
    }
    if (!out.isContinuous())
        out = out.clone();

    const int classCount = dimensions - (hasObjectness ? 5 : 4);
    if (classCount <= 0)
        return {};

    // squarePadded() scaled the longer edge up to inputSize; undo that.
    const float scale = static_cast<float>(std::max(frame.cols, frame.rows))
                      / static_cast<float>(inputSize);

    std::vector<int>      classIds;
    std::vector<float>    scores;
    std::vector<cv::Rect> boxes;
    const float*          data = reinterpret_cast<const float*>(out.data);

    for (int i = 0; i < rows; ++i, data += dimensions) {
        const float* classScores = data + (hasObjectness ? 5 : 4);
        float        score       = 0.0f;

        if (hasObjectness && data[4] < cfg.confidence)
            continue;                                   // cheap reject before the class scan

        cv::Mat   scoreRow(1, classCount, CV_32F, const_cast<float*>(classScores));
        cv::Point best;
        double    bestScore = 0.0;
        cv::minMaxLoc(scoreRow, nullptr, &bestScore, nullptr, &best);

        score = hasObjectness ? data[4] * static_cast<float>(bestScore)
                              : static_cast<float>(bestScore);
        if (score < cfg.confidence)
            continue;

        const float cx = data[0], cy = data[1], w = data[2], h = data[3];
        boxes.emplace_back(cvRound((cx - w * 0.5f) * scale),
                           cvRound((cy - h * 0.5f) * scale),
                           cvRound(w * scale), cvRound(h * scale));
        scores.push_back(score);
        classIds.push_back(best.x);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, scores, cfg.confidence, cfg.nms, keep);

    const cv::Rect          bounds(0, 0, frame.cols, frame.rows);
    std::vector<Detection>  dets;
    dets.reserve(keep.size());
    for (int idx : keep) {
        const cv::Rect box = boxes[idx] & bounds;
        if (box.width <= 0 || box.height <= 0)
            continue;

        Detection d;
        d.classId    = classIds[idx];
        d.confidence = scores[idx];
        d.x          = box.x;
        d.y          = box.y;
        d.width      = box.width;
        d.height     = box.height;
        d.label      = (d.classId >= 0 && d.classId < static_cast<int>(m_labels.size()))
                     ? m_labels[d.classId]
                     : "class_" + std::to_string(d.classId);
        dets.push_back(std::move(d));
    }
    return dets;
}

void DetectorAlgorithm::publish(std::vector<Detection> dets, double elapsedMs)
{
    std::lock_guard<std::mutex> lk(m_resultMutex);
    m_detections  = std::move(dets);
    m_inferenceMs = elapsedMs;
    // EMA over roughly the last ten inferences — steady enough to display,
    // quick enough to show a model swap.
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
