// ncnn + Vulkan inference backend — the GPU forward pass on AMD/RDNA2 (via Mesa
// RADV) and any other Vulkan device. Compiled only with WITH_GPU (see
// CMakeLists); the whole file is guarded so a CPU-only build never sees ncnn.
//
// It targets the standard onnx2ncnn conversion of the YOLOv5 ONNX export, whose
// single output blob carries the same [anchors, 5+classes] rows as the ONNX
// graph — so the frame prep (letterbox to a square, resize to inputSize, BGR->RGB,
// 1/255 normalise) and the output decode are shared with the cv::dnn backend via
// parseYoloOutput(). Validated on a machine where scripts/build-ncnn.sh has run;
// exotic exports with three raw detection heads would need per-anchor decoding
// here instead.
#ifdef MEDIAFUSION_WITH_GPU

#include "InferenceBackend.h"

#include <net.h>   // ncnn: the target's include dir is <prefix>/include/ncnn
#include <gpu.h>

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstring>

namespace {

class NcnnVulkanBackend : public IInferenceBackend
{
public:
    NcnnVulkanBackend()
    {
        // Minimal, canonical Vulkan setup — the same as ncnn's own benchncnn:
        // enable Vulkan and let load_param create the GPU instance and auto-select
        // the default device. Every other option (fp16, winograd, sgemm,
        // lightmode) stays at ncnn's defaults, which run correctly on RADV. The
        // earlier load/forward crashes were NOT an ncnn/option problem but a bad
        // FP16->ncnn model conversion (null weights) — fixed at conversion time
        // (scripts/fetch-models.sh converts the FP16 ONNX to FP32 before onnx2ncnn).
        m_net.opt.use_vulkan_compute = true;
    }

    ~NcnnVulkanBackend() override
    {
        m_net.clear();
    }

    const char* backendName() const override { return "ncnn-vulkan"; }

    bool load(const ModelInfo& model, std::string& err) override
    {
        if (model.ncnnParam.empty() || model.ncnnBin.empty()) {
            err = "no ncnn model for '" + model.name
                + "' (.param/.bin missing) -- run scripts/fetch-models.sh after build-ncnn.sh";
            return false;
        }
        if (ncnn::get_gpu_count() <= 0) {
            err = "no Vulkan device available";
            return false;
        }

        m_net.clear();
        m_net.opt.use_vulkan_compute = true;    // reassert after clear()

        if (m_net.load_param(model.ncnnParam.c_str()) != 0) {
            err = "cannot read " + model.ncnnParam;
            return false;
        }
        if (m_net.load_model(model.ncnnBin.c_str()) != 0) {
            err = "cannot read " + model.ncnnBin;
            return false;
        }

        // First declared input/output blob — avoids hardcoding "images"/"output"
        // across export variants.
        const auto ins  = m_net.input_names();
        const auto outs = m_net.output_names();
        if (ins.empty() || outs.empty()) {
            err = "ncnn graph has no input/output blobs: " + model.ncnnParam;
            return false;
        }
        m_inputName  = ins.front();
        m_outputName = outs.front();

        m_labels    = loadLabels(model.labelsPath);
        m_inputSize = model.inputSize > 0 ? model.inputSize : 640;
        return true;
    }

    bool infer(const cv::Mat& frame, const DetectorConfig& cfg,
               std::vector<Detection>& out, double& elapsedMs, std::string& err) override
    {
        const auto start = std::chrono::steady_clock::now();

        // Same letterbox as the CPU path, then resize to the square input.
        const cv::Mat sq = squarePadded(frame);
        ncnn::Mat in = ncnn::Mat::from_pixels_resize(
            sq.data, ncnn::Mat::PIXEL_BGR2RGB, sq.cols, sq.rows, m_inputSize, m_inputSize);

        const float normVals[3] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
        in.substract_mean_normalize(nullptr, normVals);

        ncnn::Mat outBlob;
        try {
            ncnn::Extractor ex = m_net.create_extractor();
            ex.input(m_inputName.c_str(), in);
            if (ex.extract(m_outputName.c_str(), outBlob) != 0) {
                err = "ncnn extract failed for blob '" + m_outputName + "'";
                return false;
            }
        } catch (const std::exception& e) {
            err = e.what();
            return false;
        }

        elapsedMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();

        // ncnn 2-D output is [w = 5+classes, h = anchors]; wrap it as the 3-D
        // [1, anchors, dims] CV_32F tensor parseYoloOutput expects (v5 layout).
        const int anchors = outBlob.h;
        const int dims    = outBlob.w;
        if (anchors <= 0 || dims <= 0) {
            out.clear();
            return true;
        }
        const int sizes[3] = { 1, anchors, dims };
        cv::Mat raw(3, sizes, CV_32F);
        for (int i = 0; i < anchors; ++i)
            std::memcpy(raw.ptr<float>(0, i), outBlob.row(i), dims * sizeof(float));

        out = parseYoloOutput(raw, cfg, m_labels, frame.cols, frame.rows, m_inputSize);
        return true;
    }

private:
    ncnn::Net                m_net;
    std::string              m_inputName;
    std::string              m_outputName;
    std::vector<std::string> m_labels;
    int                      m_inputSize = 640;
};

} // namespace

std::unique_ptr<IInferenceBackend> makeNcnnVulkanBackend()
{
    return std::make_unique<NcnnVulkanBackend>();
}

#endif // MEDIAFUSION_WITH_GPU
