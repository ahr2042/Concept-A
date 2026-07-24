// CUDA inference backend — a compiled PLACEHOLDER for a future NVIDIA GPU.
//
// The dev machine is AMD today; this file exists so that adding an NVIDIA card is
// a drop-in, not a rewrite: the enum value (AccelBackend::CUDA), the detection
// row (AcceleratorRegistry), the factory branch, and the GUI option are all in
// place. It is compiled only with WITH_CUDA (OFF by default) and links no NVIDIA
// libraries yet. load() fails cleanly, so even if something selected CUDA the
// detector falls back to cv::dnn — and detection reports CUDA unavailable, so
// AUTO never picks it. When the hardware lands, fill this in with the chosen
// stack (cv::dnn CUDA target / ONNX Runtime CUDA EP / TensorRT) behind this same
// IInferenceBackend seam.
#ifdef MEDIAFUSION_WITH_CUDA

#include "InferenceBackend.h"

namespace {

class CudaBackend : public IInferenceBackend
{
public:
    const char* backendName() const override { return "cuda"; }

    bool load(const ModelInfo&, std::string& err) override
    {
        err = "CUDA backend not implemented yet (placeholder for a future NVIDIA GPU)";
        return false;
    }

    bool infer(const cv::Mat&, const DetectorConfig&,
               std::vector<Detection>&, double&, std::string& err) override
    {
        err = "CUDA backend not implemented yet";
        return false;
    }
};

} // namespace

std::unique_ptr<IInferenceBackend> makeCudaBackend()
{
    return std::make_unique<CudaBackend>();
}

#endif // MEDIAFUSION_WITH_CUDA
