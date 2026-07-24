#include "InferenceBackend.h"

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>

// ── Shared helpers ────────────────────────────────────────────────────────────

cv::Mat squarePadded(const cv::Mat& src)
{
    const int side = std::max(src.cols, src.rows);
    if (side == src.cols && side == src.rows)
        return src;

    cv::Mat square = cv::Mat::zeros(side, side, src.type());
    src.copyTo(square(cv::Rect(0, 0, src.cols, src.rows)));
    return square;
}

std::vector<Detection> parseYoloOutput(const cv::Mat& raw, const DetectorConfig& cfg,
                                       const std::vector<std::string>& labels,
                                       int frameW, int frameH, int inputSize)
{
    if (raw.dims != 3)
        return {};

    // Two layouts are in the wild for YOLO ONNX exports:
    //   v5: [1, anchors, 5 + classes] — box, objectness, then class scores
    //   v8: [1, 4 + classes, anchors] — transposed, and with no objectness
    // Which one this is follows from which dimension is larger.
    cv::Mat out           = raw;
    int     rows          = out.size[1];
    int     dimensions    = out.size[2];
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
    const float scale = static_cast<float>(std::max(frameW, frameH))
                      / static_cast<float>(inputSize);

    std::vector<int>      classIds;
    std::vector<float>    scores;
    std::vector<cv::Rect> boxes;
    const float*          data = reinterpret_cast<const float*>(out.data);

    for (int i = 0; i < rows; ++i, data += dimensions) {
        const float* classScores = data + (hasObjectness ? 5 : 4);

        if (hasObjectness && data[4] < cfg.confidence)
            continue;                                   // cheap reject before the class scan

        cv::Mat   scoreRow(1, classCount, CV_32F, const_cast<float*>(classScores));
        cv::Point best;
        double    bestScore = 0.0;
        cv::minMaxLoc(scoreRow, nullptr, &bestScore, nullptr, &best);

        const float score = hasObjectness ? data[4] * static_cast<float>(bestScore)
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

    const cv::Rect         bounds(0, 0, frameW, frameH);
    std::vector<Detection> dets;
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
        d.label      = (d.classId >= 0 && d.classId < static_cast<int>(labels.size()))
                     ? labels[d.classId]
                     : "class_" + std::to_string(d.classId);
        dets.push_back(std::move(d));
    }
    return dets;
}

// ── cv::dnn backend (CPU) ─────────────────────────────────────────────────────

namespace {

// The original inference path: a YOLO ONNX graph run through OpenCV's DNN module
// on the CPU. This rig is AMD, so there is no CUDA path, and OpenCL through
// DNN_TARGET_OPENCL needs an ICD that is not guaranteed present — the GPU forward
// pass is ncnn+Vulkan's job (see InferenceBackendNcnn.cpp), not this backend's.
class CvDnnBackend : public IInferenceBackend
{
public:
    const char* backendName() const override { return "cv::dnn"; }

    bool load(const ModelInfo& model, std::string& err) override
    {
        cv::dnn::Net net;
        try {
            net = cv::dnn::readNetFromONNX(model.path);
        } catch (const cv::Exception& e) {
            err = std::string("cannot read ") + model.path + ": " + e.what();
            return false;
        }
        if (net.empty()) {
            err = "empty network: " + model.path;
            return false;
        }
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        m_net       = std::move(net);
        m_labels    = loadLabels(model.labelsPath);
        m_inputSize = model.inputSize > 0 ? model.inputSize : 640;
        return true;
    }

    bool infer(const cv::Mat& frame, const DetectorConfig& cfg,
               std::vector<Detection>& out, double& elapsedMs, std::string& err) override
    {
        const auto start = std::chrono::steady_clock::now();

        cv::Mat blob;
        cv::dnn::blobFromImage(squarePadded(frame), blob, 1.0 / 255.0,
                               cv::Size(m_inputSize, m_inputSize), cv::Scalar(),
                               /*swapRB=*/true, /*crop=*/false);

        std::vector<cv::Mat> outputs;
        try {
            m_net.setInput(blob);
            m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
        } catch (const cv::Exception& e) {
            err = e.what();
            return false;
        }

        elapsedMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();

        out = outputs.empty()
            ? std::vector<Detection>{}
            : parseYoloOutput(outputs[0], cfg, m_labels, frame.cols, frame.rows, m_inputSize);
        return true;
    }

private:
    cv::dnn::Net             m_net;
    std::vector<std::string> m_labels;
    int                      m_inputSize = 640;
};

} // namespace

std::unique_ptr<IInferenceBackend> makeCvDnnBackend()
{
    return std::make_unique<CvDnnBackend>();
}

// ── Factory ───────────────────────────────────────────────────────────────────

std::unique_ptr<IInferenceBackend> makeInferenceBackend(AccelBackend backend)
{
    switch (backend) {
#ifdef MEDIAFUSION_WITH_GPU
        case AccelBackend::VULKAN: return makeNcnnVulkanBackend();
#endif
#ifdef MEDIAFUSION_WITH_CUDA
        case AccelBackend::CUDA:   return makeCudaBackend();
#endif
        case AccelBackend::CPU:
        default:                   return makeCvDnnBackend();
    }
}
