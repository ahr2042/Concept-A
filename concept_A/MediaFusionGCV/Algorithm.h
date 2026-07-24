#pragma once

#include "InferenceStats.h"
#include "AccelBackend.h"

#include <opencv2/core.hpp>

// One image-processing / AI step applied to a frame, in place.
//
// This is the extension point for the real-time, user-selectable processing:
// concrete subclasses wrap OpenCV ops now, and later an ONNX Runtime / ncnn
// detector is just another Algorithm whose apply() runs inference and draws the
// results onto the frame. FrameProcessor owns a runtime-swappable list of these
// and applies them in order, so "multiple algorithms at once" is list order.
//
// Contract: apply() receives a BGR cv::Mat (CV_8UC3) and must NOT change its
// size or type (the pipeline caps are fixed for the buffer's lifetime). One
// Algorithm instance is used by one FrameProcessor on one thread.
class Algorithm
{
public:
    virtual ~Algorithm() = default;
    virtual const char* name() const = 0;
    virtual void        apply(cv::Mat& frame) = 0;

    // Optional: choose the acceleration backend this stage runs on. Called
    // before the stage processes frames and again whenever the resolved backend
    // changes. Plain CPU OpenCV ops ignore it; an inference stage uses it to pick
    // between its cv::dnn (CPU) and ncnn-Vulkan / CUDA engines.
    virtual void        setAccel(AccelBackend) {}

    // Optional: an inference stage reports what it last produced here so the
    // control protocol can serve DETECTION_SUMMARY / latency telemetry without
    // knowing which concrete Algorithm is in the chain. Plain image ops leave
    // this alone and return false. Must be safe to call from another thread.
    virtual bool snapshotStats(InferenceStats& out) const { (void)out; return false; }
};
