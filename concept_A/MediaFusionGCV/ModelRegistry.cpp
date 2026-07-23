#include "ModelRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// Directory holding the running executable, or "" if it cannot be determined.
fs::path executableDir()
{
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return {};
    return exe.parent_path();
}

bool isDirectory(const fs::path& p)
{
    std::error_code ec;
    return !p.empty() && fs::is_directory(p, ec);
}

// The labels that go with <dir>/<stem>.onnx: a same-named sidecar first, then
// the shared COCO list the YOLO family uses.
std::string findLabelsFor(const fs::path& dir, const std::string& stem)
{
    const std::string candidates[] = {
        stem + ".names", stem + ".txt", "coco.names", "labels.txt"
    };
    std::error_code ec;
    for (const auto& c : candidates) {
        fs::path p = dir / c;
        if (fs::is_regular_file(p, ec))
            return p.string();
    }
    return {};
}

} // namespace

std::vector<std::string> modelSearchPath()
{
    std::vector<fs::path> candidates;

    if (const char* env = std::getenv("MEDIAFUSION_MODEL_DIR"); env && *env)
        candidates.emplace_back(env);

    if (fs::path exeDir = executableDir(); !exeDir.empty()) {
        candidates.push_back(exeDir / "models");
        // The binary lives in <repo>/concept_A/x64_debug/, so the repository
        // root — where fetch-models.sh writes — is two levels up.
        candidates.push_back(exeDir.parent_path().parent_path() / "models");
    }

    candidates.emplace_back("models");
    candidates.emplace_back("/usr/share/mediafusiongcv/models");

    std::vector<std::string> dirs;
    for (const auto& c : candidates) {
        if (!isDirectory(c))
            continue;
        std::error_code ec;
        fs::path canonical = fs::weakly_canonical(c, ec);
        std::string dir = ec ? c.string() : canonical.string();
        if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end())
            dirs.push_back(dir);
    }
    return dirs;
}

std::vector<ModelInfo> availableModels()
{
    std::vector<ModelInfo> models;

    for (const auto& dir : modelSearchPath()) {
        std::error_code ec;
        fs::directory_iterator it(dir, ec), end;
        if (ec) continue;

        for (; it != end; it.increment(ec)) {
            if (ec) break;
            const fs::path& p = it->path();
            if (!it->is_regular_file(ec) || p.extension() != ".onnx")
                continue;

            const std::string stem = p.stem().string();
            // An earlier search directory already provided this stem.
            if (std::any_of(models.begin(), models.end(),
                            [&](const ModelInfo& m) { return m.name == stem; }))
                continue;

            ModelInfo info;
            info.name       = stem;
            info.path       = p.string();
            info.labelsPath = findLabelsFor(p.parent_path(), stem);
            info.classCount = loadLabels(info.labelsPath).size();
            models.push_back(std::move(info));
        }
    }

    std::sort(models.begin(), models.end(),
              [](const ModelInfo& a, const ModelInfo& b) { return a.name < b.name; });
    return models;
}

bool findModel(const std::string& nameOrPath, ModelInfo& out)
{
    if (nameOrPath.empty())
        return false;

    for (const auto& m : availableModels()) {
        if (m.name == nameOrPath) { out = m; return true; }
    }

    // Not a known stem — accept an explicit path to an .onnx file.
    std::error_code ec;
    fs::path p(nameOrPath);
    if (p.extension() == ".onnx" && fs::is_regular_file(p, ec)) {
        out            = ModelInfo{};
        out.name       = p.stem().string();
        out.path       = p.string();
        out.labelsPath = findLabelsFor(p.parent_path(), out.name);
        out.classCount = loadLabels(out.labelsPath).size();
        return true;
    }
    return false;
}

std::vector<std::string> loadLabels(const std::string& path)
{
    std::vector<std::string> labels;
    if (path.empty())
        return labels;

    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = line.find_last_not_of(" \t");
        labels.push_back(line.substr(a, b - a + 1));
    }
    return labels;
}
