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

// Motion by temporal difference against a running average of the scene.
//
// The cheapest useful motion stage, and the reference the heavier subtractors
// are judged against: one grayscale conversion, one absdiff, one threshold and
// one accumulate per frame, all single-channel. The running average is what
// makes it more than a naive frame-to-frame diff — a parked object fades into
// the background over a few seconds instead of flickering forever, and slow
// lighting drift is absorbed rather than reported as motion.
//
// This is the first stage that keeps state across frames. The state is per
// instance and FrameProcessor gives every chain its own objects, so two cameras
// never share a background model.
class FrameDiffAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "framediff"; }

    void setParams(const AlgorithmParams& p) override
    {
        readParam(p, "sensitivity", m_sensitivity);
        readParam(p, "decay", m_decay);
        readParam(p, "mode", m_mode);
    }

    void apply(cv::Mat& frame) override
    {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        // First frame, or a geometry change: seed the model and report nothing.
        // There is no "previous" to difference against, and claiming the whole
        // frame moved would blind the operator for a frame.
        if (m_background.empty() || m_background.size() != gray.size()) {
            gray.convertTo(m_background, CV_32FC1);
            return;
        }

        cv::Mat reference, diff;
        m_background.convertTo(reference, CV_8UC1);
        cv::absdiff(gray, reference, diff);

        // Update before rendering, so the cost is paid whichever mode is on and
        // the model keeps adapting even while the operator is looking at a mask.
        cv::accumulateWeighted(gray, m_background, m_decay);

        switch (static_cast<int>(m_mode)) {
            case 1: {   // mask — what the later motion stages will threshold
                cv::Mat mask;
                cv::threshold(diff, mask, pixelThreshold(), 255.0, cv::THRESH_BINARY);
                cv::cvtColor(mask, frame, cv::COLOR_GRAY2BGR);
                break;
            }
            case 2:     // heat — raw difference magnitude, no threshold at all
                cv::applyColorMap(diff, frame, cv::COLORMAP_INFERNO);
                break;
            default: {  // overlay — tint the moving pixels, keep the scene
                cv::Mat mask;
                cv::threshold(diff, mask, pixelThreshold(), 255.0, cv::THRESH_BINARY);
                // Blend rather than paint: a solid fill hides what moved, which
                // is the thing the operator is trying to look at.
                cv::Mat tinted = frame.clone();
                tinted.setTo(cv::Scalar(0, 0, 255), mask);
                cv::addWeighted(tinted, 0.45, frame, 0.55, 0.0, frame);
                break;
            }
        }
    }

private:
    // Sensitivity reads the operator's way round — higher finds smaller changes
    // — so it is the inverse of the pixel threshold it drives.
    double pixelThreshold() const { return 101.0 - m_sensitivity; }

    cv::Mat m_background;               // CV_32FC1 running average
    double  m_sensitivity = 75.0;
    double  m_decay       = 0.05;
    double  m_mode        = 0.0;
};

std::vector<AlgorithmParam> noParams()
{
    return {};
}

std::vector<AlgorithmParam> frameDiffParams()
{
    return {
        { "sensitivity", "SENSITIVITY", AlgorithmParam::Type::Int, 1.0, 100.0, 1.0, 75.0, {} },
        // How fast the background forgets. 0.05 at 20 fps means a still object
        // is absorbed in roughly a second; lower holds the scene longer.
        { "decay", "BACKGROUND DECAY", AlgorithmParam::Type::Float, 0.001, 0.5, 0.001, 0.05, {} },
        { "mode", "RENDER MODE", AlgorithmParam::Type::Enum, 0.0, 2.0, 1.0, 0.0,
          { "overlay", "mask", "heat" } },
    };
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

        { "framediff", "Motion by difference against a running average of the scene",
          &frameDiffParams,
          [] { return std::unique_ptr<Algorithm>(new FrameDiffAlgorithm()); } },

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
