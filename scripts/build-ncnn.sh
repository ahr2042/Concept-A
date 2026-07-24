#!/usr/bin/env bash
#
# Build and install ncnn (with Vulkan) for the detector's GPU inference path.
#
# The dev box is AMD (RDNA2) with no CUDA, so the GPU forward pass runs on ncnn's
# Vulkan backend through Mesa RADV — the most reliable GPU route on a consumer
# Radeon. Only MediaFusionGCV's detector links ncnn, and only when the engine is
# configured with -DWITH_GPU=ON; without ncnn the engine still builds CPU-only
# (cv::dnn), exactly as it degrades to "no models available".
#
# Installs to a user-writable prefix (no root). The engine's CMakeLists appends
# $HOME/.local and /opt/ncnn to CMAKE_PREFIX_PATH, so find_package(ncnn) picks
# this up and flips WITH_GPU ON automatically — no extra flags. Building the
# tools also yields onnx2ncnn, which scripts/fetch-models.sh uses to convert the
# YOLO weights into ncnn's .param/.bin format.
#
# Usage:  scripts/build-ncnn.sh [prefix]        (default: $HOME/.local)
set -euo pipefail

NCNN_VERSION="${NCNN_VERSION:-20260526}"
PREFIX="${1:-$HOME/.local}"
WORKDIR="${NCNN_BUILD_DIR:-${TMPDIR:-/tmp}/ncnn-build-$NCNN_VERSION}"
JOBS="${JOBS:-$(nproc)}"

echo "==> ncnn $NCNN_VERSION (Vulkan) -> $PREFIX (build in $WORKDIR, -j$JOBS)"

# Vulkan headers/loader are required for NCNN_VULKAN. Warn early if absent so the
# failure is legible rather than a mid-build missing-header error.
if ! pkg-config --exists vulkan; then
    echo "!! Vulkan dev files not found (apt: libvulkan-dev mesa-vulkan-drivers)." >&2
    echo "!! ncnn vendors glslang as a submodule, so the shader compiler is handled;" >&2
    echo "!! only the Vulkan loader/headers must be present." >&2
fi

mkdir -p "$WORKDIR"
if [ ! -d "$WORKDIR/ncnn/.git" ]; then
    # --recursive: ncnn vendors glslang (the Vulkan shader compiler) as a submodule.
    git clone --depth 1 --branch "$NCNN_VERSION" --recursive \
        https://github.com/Tencent/ncnn.git "$WORKDIR/ncnn"
fi

# Static lib keeps deployment simple (one .a folded into the engine, like the
# rest of MediaFusionGCV_Lib). Tools ON so we get onnx2ncnn; examples/tests/bench
# OFF to keep the build short.
cmake -S "$WORKDIR/ncnn" -B "$WORKDIR/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DNCNN_VULKAN=ON \
    -DNCNN_BUILD_TOOLS=ON \
    -DNCNN_BUILD_EXAMPLES=OFF \
    -DNCNN_BUILD_BENCHMARK=OFF \
    -DNCNN_BUILD_TESTS=OFF \
    -DNCNN_SHARED_LIB=OFF

cmake --build "$WORKDIR/build" -j"$JOBS"
cmake --install "$WORKDIR/build"

# onnx2ncnn is a build tool, not part of the library install; expose it on PATH
# for scripts/fetch-models.sh (which looks in $PREFIX/bin).
if [ -x "$WORKDIR/build/tools/onnx/onnx2ncnn" ]; then
    install -Dm755 "$WORKDIR/build/tools/onnx/onnx2ncnn" "$PREFIX/bin/onnx2ncnn"
    echo "==> installed onnx2ncnn -> $PREFIX/bin/onnx2ncnn"
fi

echo
if [ -f "$PREFIX/lib/cmake/ncnn/ncnnConfig.cmake" ]; then
    echo "==> installed: find_package(ncnn) will succeed; WITH_GPU auto-enables."
fi
echo "==> rebuild the engine so it picks this up:"
echo "    rm -rf concept_A/MediaFusionGCV/build"
echo "    cmake -S concept_A/MediaFusionGCV -B concept_A/MediaFusionGCV/build"
echo "    (expect the configure line: 'GPU inference: ncnn <ver> + Vulkan ENABLED')"
echo "==> then convert the weights for the GPU path:  scripts/fetch-models.sh"
