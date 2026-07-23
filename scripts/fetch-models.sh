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

echo
echo "==> models in $DEST:"
ls -lh "$DEST"
echo
echo "Try it:  MediaFusionGCV then 'models', 'create camera app c', 'model 0 yolov5n',"
echo "         'set-device 0 0 0', 'algos 0 detect', 'start 0', 'stats 0'"
