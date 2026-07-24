#pragma once

#include "AccelBackend.h"
#include "DetectorConfig.h"
#include "InferenceStats.h"   // Detection
#include "ModelRegistry.h"    // ModelInfo

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

// The object detector's inference engine, behind one seam so the streaming code
// in DetectorAlgorithm (worker thread, latency stats, box overlay) is fully
// backend-agnostic. Concrete backends:
//   - cv::dnn      — CPU, always built (OpenCV is already a dependency)
//   - ncnn+Vulkan  — GPU on AMD/RDNA2 and any Vulkan device, WITH_GPU
//   - CUDA         — NVIDIA placeholder, WITH_CUDA
// One instance is owned by one DetectorAlgorithm and touched only on its worker
// thread; load() and infer() never run concurrently.
class IInferenceBackend
{
public:
    virtual ~IInferenceBackend() = default;

    // Protocol/log spelling of the engine ("cv::dnn", "ncnn-vulkan", "cuda").
    virtual const char* backendName() const = 0;

    // Load a model. Returns false and fills `err` on failure (missing file,
    // unreadable graph, unsupported op, or — for a GPU backend — a missing
    // converted model). Called once per configure().
    virtual bool load(const ModelInfo& model, std::string& err) = 0;

    // Run one forward pass over a BGR CV_8UC3 frame and return NMS'd detections
    // in that frame's pixel coordinates. Sets `elapsedMs` to the forward-pass
    // time. Returns false and fills `err` on a run failure.
    virtual bool infer(const cv::Mat& frame, const DetectorConfig& cfg,
                       std::vector<Detection>& out, double& elapsedMs,
                       std::string& err) = 0;
};

// Backend factories. makeInferenceBackend() maps a RESOLVED backend (never AUTO)
// to a concrete engine and falls back to cv::dnn for anything not compiled in,
// so a caller can request VULKAN/CUDA unconditionally and still get a usable
// engine on a CPU-only build.
std::unique_ptr<IInferenceBackend> makeCvDnnBackend();
#ifdef MEDIAFUSION_WITH_GPU
std::unique_ptr<IInferenceBackend> makeNcnnVulkanBackend();
#endif
#ifdef MEDIAFUSION_WITH_CUDA
std::unique_ptr<IInferenceBackend> makeCudaBackend();
#endif
std::unique_ptr<IInferenceBackend> makeInferenceBackend(AccelBackend backend);

// ── Shared, backend-agnostic helpers ──────────────────────────────────────────

// Letterbox to a square (image at top-left, pad bottom/right) so detections map
// back to frame coordinates with a single scale factor and no centring offset.
cv::Mat squarePadded(const cv::Mat& src);

// Decode a YOLO output tensor into detections. Handles both export layouts:
//   v5: [1, anchors, 5+classes]  (box, objectness, then class scores)
//   v8: [1, 4+classes, anchors]  (transposed, no objectness)
// which one is inferred from which dimension is larger. `raw` is the network's
// first output as a 3-D CV_32F cv::Mat; `labels` names classes; `frameW/H` and
// `inputSize` undo the letterbox scale. Applies cfg.confidence and cfg.nms.
std::vector<Detection> parseYoloOutput(const cv::Mat& raw, const DetectorConfig& cfg,
                                       const std::vector<std::string>& labels,
                                       int frameW, int frameH, int inputSize);
