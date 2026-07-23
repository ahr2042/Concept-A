#pragma once

#include <cstdint>
#include <string>
#include <vector>

// What the inference stage did on the most recently completed frame.
//
// The detector runs asynchronously (see Detector.h), so these numbers describe
// the last finished inference, not necessarily the frame currently on screen.
// That separation is deliberate: it lets the console report a real inference
// latency independent of the stream's frame rate.

// One detected object, in coordinates of the frame it was found in.
struct Detection
{
    std::string label;                 // class name, or "class_<n>" without labels
    int         classId    = 0;
    float       confidence = 0.0f;
    int         x = 0, y = 0, width = 0, height = 0;
};

struct InferenceStats
{
    bool        modelLoaded = false;   // a net is resident and usable
    std::string modelName;             // "" when no model is selected
    std::string lastError;             // load/run failure, "" when healthy

    double   inferenceMs    = 0.0;     // last completed inference
    double   avgInferenceMs = 0.0;     // exponential moving average
    uint64_t framesInferred = 0;       // inferences completed since load
    uint64_t framesSkipped  = 0;       // frames drawn while the worker was busy

    int    objectCount   = 0;          // objects in the last result
    float  avgConfidence = 0.0f;       // mean confidence of those objects

    std::vector<Detection> detections; // the last result itself
};
