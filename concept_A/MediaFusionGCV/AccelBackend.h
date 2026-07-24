#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The concrete acceleration engine a pipeline actually runs on. Resolved from an
// AccelSelection (below) against what the host really has — see
// AcceleratorRegistry. CPU is always available; the others depend on hardware,
// drivers, and build flags, so a resolved backend is never assumed, only chosen.
enum struct AccelBackend : int32_t
{
    CPU    = 0,   // OpenCV cv::dnn + host cv::Mat — always available
    VULKAN = 1,   // ncnn + Vulkan (AMD/RDNA2 today; any Vulkan device), WITH_GPU
    CUDA   = 2    // NVIDIA — placeholder, compiled in behind WITH_CUDA
};

// What the operator asked for. AUTO resolves to the best AVAILABLE backend
// (CUDA > VULKAN > CPU); a specific pick forces that engine but still falls back
// to CPU when it is unavailable at deploy time, so a stale selection never fails
// a stream — it just degrades.
enum struct AccelSelection : int32_t
{
    AUTO   = 0,
    CPU    = 1,
    VULKAN = 2,
    CUDA   = 3
};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

// One row of the init-time capability probe (AcceleratorRegistry).
struct AcceleratorInfo
{
    AccelBackend backend   = AccelBackend::CPU;
    std::string  device    = "";      // human-readable device name, or "" if none
    bool         available = false;   // usable on this host with this build
};

// Protocol/log spellings. Backend: "cpu"/"vulkan"/"cuda"; selection adds "auto".
const char* accelBackendName(AccelBackend b);
const char* accelSelectionName(AccelSelection s);

// Parses "auto|cpu|vulkan|cuda" (case-insensitive; "gpu" is an alias for vulkan).
bool parseAccelSelection(const std::string& token, AccelSelection& out);
#endif
