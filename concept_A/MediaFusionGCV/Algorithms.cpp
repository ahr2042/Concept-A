#include "Algorithms.h"
#include "Detector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>

namespace {

// Both filters keep the BGR/CV_8UC3 layout the pipeline expects (convert back
// after a single-channel step) so they can be chained in any order.
//
// GPU path: when a GPU backend is selected these run through OpenCV's T-API
// (cv::UMat), which dispatches cvtColor/Canny to OpenCL. That is a different
// runtime from the detector's Vulkan (ncnn) — the filters are cheap enough that
// a second GPU stack is not worth it — but it shares the "GPU vs CPU" switch and
// falls back to the CPU path when no OpenCL device/ICD is present, so it is safe
// on any machine. The pad probe hands us a cv::Mat over the pipeline buffer, so
// the GPU path uploads a copy and downloads the result back into `frame` to keep
// the in-place contract (see FrameProcessor).

// Enable OpenCL globally when a GPU backend is chosen; useOpenCL() then reports
// whether a device actually exists, which is what each apply() checks.
bool wantOpenCL(AccelBackend b)
{
    if (b == AccelBackend::CPU)
        return false;
    cv::ocl::setUseOpenCL(true);
    return cv::ocl::useOpenCL();
}

// Read one key out of a partial parameter set, keeping the current value when
// the caller did not send it.
void readParam(const AlgorithmParams& in, const char* key, double& out)
{
    const auto it = in.find(key);
    if (it != in.end())
        out = it->second;
}

class GrayscaleAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "grayscale"; }

    void setAccel(AccelBackend b) override { m_gpu = wantOpenCL(b); }

    void apply(cv::Mat& frame) override
    {
        if (m_gpu && cv::ocl::useOpenCL()) {
            cv::UMat uframe, ugray;
            frame.copyTo(uframe);
            cv::cvtColor(uframe, ugray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(ugray, uframe, cv::COLOR_GRAY2BGR);
            uframe.copyTo(frame);
            return;
        }
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, frame, cv::COLOR_GRAY2BGR);
    }

private:
    bool m_gpu = false;
};

class CannyEdgesAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "canny"; }

    void setAccel(AccelBackend b) override { m_gpu = wantOpenCL(b); }

    void setParams(const AlgorithmParams& p) override
    {
        readParam(p, "low", m_low);
        readParam(p, "high", m_high);
    }

    void apply(cv::Mat& frame) override
    {
        if (m_gpu && cv::ocl::useOpenCL()) {
            cv::UMat uframe, ugray, uedges;
            frame.copyTo(uframe);
            cv::cvtColor(uframe, ugray, cv::COLOR_BGR2GRAY);
            cv::Canny(ugray, uedges, m_low, m_high);
            cv::cvtColor(uedges, uframe, cv::COLOR_GRAY2BGR);
            uframe.copyTo(frame);
            return;
        }
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, m_low, m_high);
        cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGR);
    }

private:
    bool   m_gpu  = false;
    double m_low  = 80.0;
    double m_high = 160.0;
};

std::vector<AlgorithmParam> noParams()
{
    return {};
}

std::vector<AlgorithmParam> cannyParams()
{
    // The hysteresis pair. Scene-dependent enough that the old hardcoded 80/160
    // was only ever right for one room.
    return {
        { "low", "LOW THRESHOLD", AlgorithmParam::Type::Int, 0.0, 255.0, 1.0, 80.0, {} },
        { "high", "HIGH THRESHOLD", AlgorithmParam::Type::Int, 0.0, 255.0, 1.0, 160.0, {} },
    };
}

} // namespace

const std::vector<AlgorithmInfo>& algorithmRegistry()
{
    // Menu order. Cheap pixel ops first, inference last, which is also the order
    // they make sense in a chain.
    static const std::vector<AlgorithmInfo> registry = {
        { "grayscale", "Desaturate to luma, kept in BGR so it chains anywhere",
          &noParams,
          [] { return std::unique_ptr<Algorithm>(new GrayscaleAlgorithm()); } },

        { "canny", "Canny edge map with tunable hysteresis thresholds",
          &cannyParams,
          [] { return std::unique_ptr<Algorithm>(new CannyEdgesAlgorithm()); } },

        // Starts idle; FrameProcessor hands it the selected model right after.
        // Its model and thresholds travel on the dedicated `model` /
        // `detect-params` verbs rather than here, because a model is a name and
        // this schema carries numbers.
        { "detect", "YOLO object detection, inference on its own worker thread",
          &noParams,
          [] { return std::unique_ptr<Algorithm>(new DetectorAlgorithm()); } },
    };
    return registry;
}

const AlgorithmInfo* findAlgorithm(const std::string& name)
{
    for (const auto& info : algorithmRegistry())
        if (name == info.name)
            return &info;
    return nullptr;
}

std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name)
{
    const AlgorithmInfo* info = findAlgorithm(name);
    return info ? info->make() : nullptr;
}

std::vector<std::string> availableAlgorithms()
{
    std::vector<std::string> names;
    names.reserve(algorithmRegistry().size());
    for (const auto& info : algorithmRegistry())
        names.emplace_back(info.name);
    return names;
}

std::vector<AlgorithmParam> algorithmParams(const std::string& name)
{
    const AlgorithmInfo* info = findAlgorithm(name);
    return info ? info->schema() : std::vector<AlgorithmParam>{};
}
