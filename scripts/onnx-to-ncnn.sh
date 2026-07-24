#!/usr/bin/env bash
#
# Convert a (possibly FP16) YOLO ONNX into an ncnn model that runs on ncnn's
# VULKAN backend — not just its CPU one.
#
# Why this is more than `onnx2ncnn`:
#   * The published yolov5n.onnx is FP16. onnx2ncnn mishandles the FP16 weights,
#     leaving Convolution weight_data null → SIGSEGV in ncnn's Vulkan
#     Convolution_vulkan::create_pipeline (the same model loads fine on ncnn CPU).
#   * yolov5's Detect head becomes raw BinaryOp layers under onnx2ncnn that ncnn's
#     Vulkan BinaryOp path can't run (null pipeline in forward).
#   pnnx, fed an FP32 graph, folds/optimises both away and emits a Vulkan-safe
#   ncnn model. So: onnx --(fp16->fp32)--> pnnx --> <stem>.param/.bin.
#
# Needs python3 with onnx + pnnx. If they are not importable, a throwaway venv is
# created (set MFGCV_VENV to reuse one). Best-effort: the CPU detector (cv::dnn on
# the .onnx) does not need any of this.
#
# Usage: onnx-to-ncnn.sh <model.onnx> [out_dir] [WxH]     (default out_dir = onnx's dir, size 640x640)
set -euo pipefail

ONNX="$1"
OUTDIR="${2:-$(cd "$(dirname "$ONNX")" && pwd)}"
SIZE="${3:-640}"
STEM="$(basename "$ONNX" .onnx)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="${MFGCV_VENV:-${TMPDIR:-/tmp}/mfgcv-convert-venv}"

PY=python3
if ! python3 -c 'import onnx, pnnx' >/dev/null 2>&1; then
    if [ ! -x "$VENV/bin/pnnx" ]; then
        echo "==> setting up conversion venv at $VENV (onnx + pnnx; downloads torch, a few hundred MB)"
        python3 -m venv "$VENV"
        "$VENV/bin/pip" install --quiet --upgrade pip
        "$VENV/bin/pip" install --quiet onnx pnnx
    fi
    PY="$VENV/bin/python"
fi
PNNX="$(dirname "$PY")/pnnx"

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT

echo "==> $STEM: FP16 -> FP32"
"$PY" "$HERE/fp16to32.py" "$ONNX" "$work/${STEM}_fp32.onnx"

echo "==> $STEM: pnnx (FP32 ONNX -> ncnn, inputshape [1,3,$SIZE,$SIZE])"
( cd "$work" && "$PNNX" "${STEM}_fp32.onnx" "inputshape=[1,3,$SIZE,$SIZE]" >/dev/null 2>&1 )

p="$work/${STEM}_fp32.ncnn.param"
b="$work/${STEM}_fp32.ncnn.bin"
if [ -s "$p" ] && [ -s "$b" ]; then
    cp "$p" "$OUTDIR/$STEM.param"
    cp "$b" "$OUTDIR/$STEM.bin"
    echo "==> wrote $OUTDIR/$STEM.param and $OUTDIR/$STEM.bin (Vulkan-ready)"
else
    echo "!! pnnx produced no ncnn output for $STEM" >&2
    exit 1
fi
