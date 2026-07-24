#pragma once

#include <string>

// Runtime knobs for the inference stage, set from the control protocol. Lives in
// its own header so both the detector and the inference backends can see it
// without a circular include (Detector.h -> InferenceBackend.h -> here).
struct DetectorConfig
{
    std::string model;                 // model stem or .onnx path; "" = idle
    float       confidence = 0.25f;    // keep detections at/above this score
    float       nms        = 0.45f;    // IoU threshold for non-maximum suppression
    bool        drawBoxes  = true;     // overlay boxes+labels on the frame
};
