#pragma once

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
};
