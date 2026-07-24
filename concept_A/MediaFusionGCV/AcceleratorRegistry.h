#pragma once

#include "AccelBackend.h"

#include <vector>

// Probes the host ONCE for the acceleration backends it can run and caches the
// result. Detection has real side-effecting cost (creating a Vulkan instance to
// enumerate devices), so it runs at init — never per-frame or per-deploy — and
// the cached list is what the control protocol reports to the GUI, which renders
// only the backends whose `available` is true.
const std::vector<AcceleratorInfo>& detectAccelerators();

// Resolves an operator selection against the detected capabilities:
//   AUTO   -> best available (CUDA > VULKAN > CPU)
//   CPU    -> CPU
//   VULKAN -> VULKAN if available, else CPU
//   CUDA   -> CUDA   if available, else CPU
// The result is always a backend the host can actually run, so callers never
// have to re-check availability.
AccelBackend resolveBackend(AccelSelection selection);
