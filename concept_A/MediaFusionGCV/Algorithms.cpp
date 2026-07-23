#include "Algorithms.h"
#include "Detector.h"

#include <opencv2/imgproc.hpp>

namespace {

// Both keep the BGR/CV_8UC3 layout the pipeline expects (convert back after a
// single-channel step) so they can be chained in any order.

class GrayscaleAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "grayscale"; }
    void apply(cv::Mat& frame) override
    {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, frame, cv::COLOR_GRAY2BGR);
    }
};

class CannyEdgesAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "canny"; }
    void apply(cv::Mat& frame) override
    {
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 80.0, 160.0);
        cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGR);
    }
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
