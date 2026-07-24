#include "AcceleratorRegistry.h"

#ifdef MEDIAFUSION_WITH_GPU
#include <gpu.h>   // ncnn: the target's include dir is <prefix>/include/ncnn

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

std::vector<AcceleratorInfo> probe()
{
    std::vector<AcceleratorInfo> out;

    // CPU is the floor: OpenCV cv::dnn / host cv::Mat always run.
    out.push_back({ AccelBackend::CPU, "CPU (OpenCV)", true });

    // VULKAN is offered only when ncnn+Vulkan is compiled in AND the driver
    // enumerates at least one device. Without WITH_GPU there is no engine to run
    // it, so it stays unavailable even on a box that has a Vulkan-capable GPU —
    // the honest thing to report, since selecting it could not do inference.
    AcceleratorInfo vk{ AccelBackend::VULKAN, "", false };
#ifdef MEDIAFUSION_WITH_GPU
    // ncnn prints a block of adapter capabilities to stderr the first time it
    // creates the Vulkan instance (which get_gpu_count() does). That is noise in
    // the daemon log, so silence fd 2 just around the probe; our own diagnostics
    // go through separate calls and are unaffected. The instance is left alive on
    // purpose — the ncnn detector backend reuses it, and detection is cached, so
    // this whole block runs once per process.
    fflush(stderr);
    const int savedErr = dup(STDERR_FILENO);
    const int devnull  = open("/dev/null", O_WRONLY);
    if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }

    if (ncnn::get_gpu_count() > 0) {
        vk.available = true;
        const ncnn::GpuInfo& g = ncnn::get_gpu_info(0);
        vk.device = g.device_name();
    }

    fflush(stderr);
    if (savedErr >= 0) { dup2(savedErr, STDERR_FILENO); close(savedErr); }
#endif
    out.push_back(vk);

    // CUDA is the NVIDIA placeholder. The row always exists so the GUI can show
    // it the moment the hardware/build lands; today it never reports available.
    AcceleratorInfo cu{ AccelBackend::CUDA, "", false };
#ifdef MEDIAFUSION_WITH_CUDA
    // TODO(nvidia): probe cudaGetDeviceCount()/cv::cuda::getCudaEnabledDeviceCount
    // and fill cu.device/cu.available once a CUDA inference backend exists.
#endif
    out.push_back(cu);

    return out;
}

} // namespace

const std::vector<AcceleratorInfo>& detectAccelerators()
{
    // Function-local static: thread-safe one-time init, cached for the process.
    static const std::vector<AcceleratorInfo> cached = probe();
    return cached;
}

AccelBackend resolveBackend(AccelSelection selection)
{
    const auto& acc = detectAccelerators();
    auto available = [&](AccelBackend b) {
        for (const auto& a : acc)
            if (a.backend == b) return a.available;
        return false;
    };

    switch (selection) {
        case AccelSelection::CPU:
            return AccelBackend::CPU;
        case AccelSelection::VULKAN:
            return available(AccelBackend::VULKAN) ? AccelBackend::VULKAN : AccelBackend::CPU;
        case AccelSelection::CUDA:
            return available(AccelBackend::CUDA) ? AccelBackend::CUDA : AccelBackend::CPU;
        case AccelSelection::AUTO:
        default:
            if (available(AccelBackend::CUDA))   return AccelBackend::CUDA;
            if (available(AccelBackend::VULKAN)) return AccelBackend::VULKAN;
            return AccelBackend::CPU;
    }
}
