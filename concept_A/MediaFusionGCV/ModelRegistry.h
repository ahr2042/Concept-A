#pragma once

#include <string>
#include <vector>

// Discovery of detector models on disk.
//
// Model weights are deliberately NOT in git (they are tens of megabytes and
// have their own licences) — `scripts/fetch-models.sh` downloads them into
// `models/` at the repository root. Everything here degrades to "no models
// available", which is the normal state on a fresh clone and on CI.

// A detector model: the ONNX graph plus its class-label sidecar.
struct ModelInfo
{
    std::string name;             // file stem ("yolov5n") — what clients select by
    std::string path;             // absolute path to the .onnx graph
    std::string labelsPath;       // absolute path to the labels file, "" if none
    int         inputSize  = 640; // square network input (YOLO-family default)
    size_t      classCount = 0;   // labels found, 0 when there is no sidecar
    std::string ncnnParam;        // sibling <stem>.param for the ncnn/Vulkan backend, "" if none
    std::string ncnnBin;          // sibling <stem>.bin, "" if none
};

// Directories searched, in priority order:
//   1. $MEDIAFUSION_MODEL_DIR
//   2. <dir of this executable>/models
//   3. <repository root>/models          (…/concept_A/x64_debug/../../models)
//   4. ./models
//   5. /usr/share/mediafusiongcv/models
// Only existing directories are returned.
std::vector<std::string> modelSearchPath();

// Every readable *.onnx in the search path, sorted by name; earlier directories
// win when the same stem appears twice.
std::vector<ModelInfo> availableModels();

// Look one up by stem (or by an explicit path to an .onnx file, which lets the
// console point at a model outside the search path). False if not found.
bool findModel(const std::string& nameOrPath, ModelInfo& out);

// One label per line, blank lines skipped. Empty vector if the file is missing.
std::vector<std::string> loadLabels(const std::string& path);
