#include "Algorithms.h"
#include "Detector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/video/background_segm.hpp>

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

// Motion by background subtraction: a per-pixel statistical model of what the
// scene looks like when nothing is happening, and everything that does not fit
// it is foreground.
//
// This is the step up from framediff. A running average holds one number per
// pixel, so anything that oscillates — foliage, a flickering monitor, sensor
// noise in a dim room — reads as permanent motion. MOG2 models each pixel as a
// mixture of Gaussians and KNN as a set of recent samples, so both can hold
// "this pixel is usually one of these few values" and stay quiet. The cost is
// real work per pixel, which is what the downscale below pays for.
//
// The analysis runs on a downscaled copy and the mask is scaled back up to draw.
// A motion mask does not need 1080p, and the whole chain runs on the streaming
// thread holding FrameProcessor::m_mutex — see docs/PERFORMANCE.md. Everything
// here is CPU: the frame is already in host memory, and one stage does not
// justify a second GPU runtime alongside the detector's Vulkan.
class MotionAlgorithm : public Algorithm
{
public:
    const char* name() const override { return "motion"; }

    void setParams(const AlgorithmParams& p) override
    {
        const double wasMethod = m_method;
        readParam(p, "method", m_method);
        readParam(p, "history", m_history);
        readParam(p, "threshold", m_threshold);
        readParam(p, "shadows", m_shadows);
        readParam(p, "learn-rate", m_learnRate);
        readParam(p, "mode", m_mode);

        // Switching subtractor needs a different object, so the learned scene is
        // necessarily lost. The rest are live setters — retuning a threshold
        // mid-stream must not throw away a background that took seconds to
        // learn, which a rebuild-on-every-change would do.
        if (m_method != wasMethod)
            m_subtractor.release();
        else
            retune();
    }

    void apply(cv::Mat& frame) override
    {
        if (m_subtractor.empty()) {
            m_subtractor = static_cast<int>(m_method) == 1
                ? cv::Ptr<cv::BackgroundSubtractor>(cv::createBackgroundSubtractorKNN())
                : cv::Ptr<cv::BackgroundSubtractor>(cv::createBackgroundSubtractorMOG2());
            retune();
        }

        // INTER_AREA rather than a cheaper filter: it averages the pixels it
        // discards, where nearest-neighbour would alias fine texture into
        // shimmer that the subtractor then reports as motion.
        const bool downscale = frame.rows > kAnalysisRows;
        if (downscale) {
            const double s = static_cast<double>(kAnalysisRows) / frame.rows;
            cv::resize(frame, m_small, cv::Size(), s, s, cv::INTER_AREA);
        }
        const cv::Mat& input = downscale ? m_small : frame;

        // Both subtractors reinitialize themselves if the geometry changes, so a
        // caps change costs a relearn rather than an exception.
        m_subtractor->apply(input, m_raw, learningRate());

        // Shadows come back as 127 rather than 255. Cutting above that is what
        // makes the flag useful — a shadow sweeping across the floor is not an
        // object — and is a no-op when shadow detection is off, since the mask
        // is then already binary.
        cv::threshold(m_raw, m_fg, 200.0, 255.0, cv::THRESH_BINARY);
        // An empty kernel is a 3x3 rect. Opening erases the isolated speckle
        // every subtractor produces without eating into a real blob.
        cv::morphologyEx(m_fg, m_fg, cv::MORPH_OPEN, cv::Mat());

        if (static_cast<int>(m_mode) == 2) {
            renderBackground(frame);
            return;
        }

        // Nearest-neighbour on the way back up, deliberately: the mask is binary
        // and interpolation would produce in-between values that setTo() then
        // treats as foreground anyway. Blocky edges are honest about the
        // resolution the decision was actually made at.
        const cv::Mat& mask = upscaledMask(frame.size());

        if (static_cast<int>(m_mode) == 1) {
            cv::cvtColor(mask, frame, cv::COLOR_GRAY2BGR);
            return;
        }

        // overlay — blend rather than paint, so the operator can still see what
        // is moving and not just that something is. copyTo into a kept buffer
        // rather than clone(): this is the default mode and the copy is
        // full-resolution, so it is the one allocation worth not repeating.
        frame.copyTo(m_tinted);
        m_tinted.setTo(cv::Scalar(0, 0, 255), mask);
        cv::addWeighted(m_tinted, 0.45, frame, 0.55, 0.0, frame);
    }

private:
    // 360 rows is enough to separate a person from the scene at any capture size
    // this engine offers, and it caps the subtractor's cost at a constant no
    // matter what the camera delivers.
    static constexpr int kAnalysisRows = 360;

    // 0 would mean "freeze the model" to OpenCV, which is not what an operator
    // dragging a slider to the bottom expects; -1 is its automatic rate, derived
    // from `history`.
    double learningRate() const { return m_learnRate <= 0.0 ? -1.0 : m_learnRate; }

    // The two subtractors do not share a tuning interface, and their thresholds
    // are not even the same quantity: MOG2's is a squared Mahalanobis distance
    // (default 16), KNN's a squared pixel distance (default 400). The x25 keeps
    // one knob honest — both defaults land on the same slider position, and the
    // ends of the range mean the same thing to both.
    void retune()
    {
        if (m_subtractor.empty())
            return;
        const int  history = static_cast<int>(m_history);
        const bool shadows = m_shadows >= 0.5;

        if (auto mog2 = m_subtractor.dynamicCast<cv::BackgroundSubtractorMOG2>()) {
            mog2->setHistory(history);
            mog2->setVarThreshold(m_threshold);
            mog2->setDetectShadows(shadows);
        } else if (auto knn = m_subtractor.dynamicCast<cv::BackgroundSubtractorKNN>()) {
            knn->setHistory(history);
            knn->setDist2Threshold(m_threshold * 25.0);
            knn->setDetectShadows(shadows);
        }
    }

    const cv::Mat& upscaledMask(cv::Size full)
    {
        if (m_fg.size() == full)
            return m_fg;
        cv::resize(m_fg, m_mask, full, 0, 0, cv::INTER_NEAREST);
        return m_mask;
    }

    // What the model currently believes the empty scene looks like. The most
    // useful diagnostic the family has: "why is it flagging everything" is
    // almost always visible here as a background that never settled.
    void renderBackground(cv::Mat& frame)
    {
        m_subtractor->getBackgroundImage(m_background);
        if (m_background.empty())
            return;   // nothing learned yet; leave the scene alone
        if (m_background.size() == frame.size())
            m_background.copyTo(frame);
        else
            cv::resize(m_background, frame, frame.size(), 0, 0, cv::INTER_LINEAR);
    }

    cv::Ptr<cv::BackgroundSubtractor> m_subtractor;

    // Held across frames rather than declared in apply(): cv::Mat reuses its
    // allocation when the geometry matches, so these are buffers allocated once
    // instead of a malloc/free pair each per frame on the streaming thread.
    cv::Mat m_small;        // downscaled analysis copy
    cv::Mat m_raw;          // subtractor output, shadows still at 127
    cv::Mat m_fg;           // cleaned binary mask, analysis size
    cv::Mat m_mask;         // ... scaled back up to the frame
    cv::Mat m_tinted;       // overlay scratch, full resolution
    cv::Mat m_background;

    double m_method    = 0.0;     // 0 = MOG2, 1 = KNN
    double m_history   = 500.0;
    double m_threshold = 16.0;
    double m_shadows   = 1.0;
    double m_learnRate = 0.0;     // 0 = automatic
    double m_mode      = 0.0;
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

std::vector<AlgorithmParam> motionParams()
{
    return {
        { "method", "SUBTRACTOR", AlgorithmParam::Type::Enum, 0.0, 1.0, 1.0, 0.0,
          { "mog2", "knn" } },
        // How many frames the model remembers. At 20 fps the 500 default is
        // about 25 seconds of scene, which is also the automatic learning rate.
        { "history", "HISTORY (FRAMES)", AlgorithmParam::Type::Int, 50.0, 2000.0, 10.0, 500.0, {} },
        // Distance a pixel has to sit from the model to count as foreground.
        // Lower finds more and flags more noise.
        { "threshold", "FOREGROUND THRESHOLD", AlgorithmParam::Type::Int, 4.0, 200.0, 1.0, 16.0, {} },
        { "shadows", "DETECT SHADOWS", AlgorithmParam::Type::Bool, 0.0, 1.0, 1.0, 1.0, {} },
        { "learn-rate", "LEARN RATE (0 = AUTO)", AlgorithmParam::Type::Float, 0.0, 0.5, 0.001, 0.0, {} },
        { "mode", "RENDER MODE", AlgorithmParam::Type::Enum, 0.0, 2.0, 1.0, 0.0,
          { "overlay", "mask", "background" } },
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

        { "motion", "Background subtraction that learns the scene, MOG2 or KNN",
          &motionParams,
          [] { return std::unique_ptr<Algorithm>(new MotionAlgorithm()); } },

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
