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

    void apply(cv::Mat& frame) override
    {
        if (m_gpu && cv::ocl::useOpenCL()) {
            cv::UMat uframe, ugray, uedges;
            frame.copyTo(uframe);
            cv::cvtColor(uframe, ugray, cv::COLOR_BGR2GRAY);
            cv::Canny(ugray, uedges, 80.0, 160.0);
            cv::cvtColor(uedges, uframe, cv::COLOR_GRAY2BGR);
            uframe.copyTo(frame);
            return;
        }
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 80.0, 160.0);
        cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGR);
    }

private:
    bool m_gpu = false;
};

} // namespace

std::unique_ptr<Algorithm> makeAlgorithm(const std::string& name)
{
    if (name == "grayscale") return std::make_unique<GrayscaleAlgorithm>();
    if (name == "canny")     return std::make_unique<CannyEdgesAlgorithm>();
    // Starts idle; FrameProcessor hands it the selected model right after.
    if (name == "detect")    return std::make_unique<DetectorAlgorithm>();
    return nullptr;
}

std::vector<std::string> availableAlgorithms()
{
    return { "grayscale", "canny", "detect" };
}
