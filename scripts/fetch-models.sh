#!/usr/bin/env bash
#
# Download the detector models the inference stage runs.
#
# Weights are not in git: they are megabytes of binary with their own licences.
# The engine finds whatever lands in models/ (see ModelRegistry.cpp) — drop any
# other YOLO-family ONNX export in there and it shows up in `models` and in the
# console's DETECTION MODEL picker.
#
# Needs OpenCV 4.8+ (scripts/build-opencv.sh): the 4.6 in Ubuntu 24.04 cannot
# import these graphs.
#
# Usage:  scripts/fetch-models.sh [dest]        (default: <repo>/models)
set -euo pipefail

DEST="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/models}"
mkdir -p "$DEST"

# yolov5n: the nano YOLOv5, 80 COCO classes, 640x640 input, ~4 MB. Published as
# an FP16 graph, which OpenCV reads directly from 4.8 on.
YOLOV5N_URL="https://github.com/ultralytics/yolov5/releases/download/v7.0/yolov5n.onnx"
LABELS_URL="https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names"

fetch() {
    local url="$1" out="$2"
    if [ -s "$out" ]; then
        echo "==> $(basename "$out") already present, skipping"
        return
    fi
    echo "==> fetching $(basename "$out")"
    curl -fL --progress-bar -o "$out.part" "$url"
    mv "$out.part" "$out"
}

fetch "$YOLOV5N_URL" "$DEST/yolov5n.onnx"
fetch "$LABELS_URL"  "$DEST/coco.names"

# --- ncnn conversion (GPU / Vulkan detector) ------------------------------------
# The Vulkan detector loads ncnn .param/.bin, not ONNX. This is NOT a plain
# onnx2ncnn run: yolov5n.onnx is FP16 and its Detect head both break ncnn's Vulkan
# backend (null weight_data in Convolution_vulkan::create_pipeline; null pipeline
# in BinaryOp_vulkan). scripts/onnx-to-ncnn.sh does the working pipeline —
# FP16->FP32 then pnnx — producing a Vulkan-safe model. Best-effort: it needs
# python3 with onnx + pnnx (the helper sets up a venv if missing). The CPU detector
# uses the .onnx directly and needs none of this, so a failure here is non-fatal.
convert_ncnn() {
    local stem="$1"
    local onnx="$DEST/$stem.onnx"
    [ -s "$onnx" ] || return 0
    if [ -s "$DEST/$stem.param" ] && [ -s "$DEST/$stem.bin" ]; then
        echo "==> $stem.param/.bin already present, skipping"
        return 0
    fi

    local here; here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    echo "==> converting $stem -> ncnn (Vulkan-ready) via scripts/onnx-to-ncnn.sh"
    if ! bash "$here/onnx-to-ncnn.sh" "$onnx" "$DEST"; then
        echo "!! ncnn conversion failed/skipped for $stem -- the GPU (Vulkan) detector" >&2
        echo "   stays unavailable until $stem.param/.bin exist; the CPU detector still works." >&2
    fi
}

convert_ncnn "yolov5n"

echo
echo "==> models in $DEST:"
ls -lh "$DEST"
echo
echo "Try it:  MediaFusionGCV then 'models', 'create camera app c', 'model 0 yolov5n',"
echo "         'set-device 0 0 0', 'algos 0 detect', 'start 0', 'stats 0'"
echo "GPU:     'accelerators' lists what was detected; 'accel 0 auto' (or vulkan)"
echo "         before 'start' runs the forward pass on the Radeon when available."
