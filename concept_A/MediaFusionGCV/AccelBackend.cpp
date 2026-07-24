#include "AccelBackend.h"

#include <cctype>

const char* accelBackendName(AccelBackend b)
{
    switch (b) {
        case AccelBackend::CPU:    return "cpu";
        case AccelBackend::VULKAN: return "vulkan";
        case AccelBackend::CUDA:   return "cuda";
    }
    return "cpu";
}

const char* accelSelectionName(AccelSelection s)
{
    switch (s) {
        case AccelSelection::AUTO:   return "auto";
        case AccelSelection::CPU:    return "cpu";
        case AccelSelection::VULKAN: return "vulkan";
        case AccelSelection::CUDA:   return "cuda";
    }
    return "auto";
}

bool parseAccelSelection(const std::string& token, AccelSelection& out)
{
    std::string t;
    t.reserve(token.size());
    for (char c : token)
        t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (t == "auto")                 { out = AccelSelection::AUTO;   return true; }
    if (t == "cpu")                  { out = AccelSelection::CPU;    return true; }
    if (t == "vulkan" || t == "gpu") { out = AccelSelection::VULKAN; return true; }
    if (t == "cuda")                 { out = AccelSelection::CUDA;   return true; }
    return false;
}
