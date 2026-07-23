#!/usr/bin/env bash
#
# Build and install the OpenCV the inference stage needs.
#
# Ubuntu 24.04 ships OpenCV 4.6, whose ONNX importer cannot read current YOLO
# exports: yolov5n from the v7.0 release fails first on FP16 initializers
# ("Unsupported data type: FLOAT16") and then, once converted to FP32, on the
# Split node in its detect head. 4.8+ reads them. Nothing else in the project
# needs the newer version — only MediaFusionGCV's detector links OpenCV.
#
# Installs to a user-writable prefix, so no root is required. The engine's
# CMakeLists looks in $HOME/.local and /opt/opencv before the system copy, so a
# plain `cmake -S concept_A/MediaFusionGCV -B .../build` picks this up with no
# extra flags. The apt 4.6 stays installed and untouched — different sonames,
# so the two coexist.
#
# Usage:  scripts/build-opencv.sh [prefix]        (default: $HOME/.local)
set -euo pipefail

OPENCV_VERSION="${OPENCV_VERSION:-4.12.0}"
PREFIX="${1:-$HOME/.local}"
WORKDIR="${OPENCV_BUILD_DIR:-${TMPDIR:-/tmp}/opencv-build-$OPENCV_VERSION}"
JOBS="${JOBS:-$(nproc)}"

echo "==> OpenCV $OPENCV_VERSION -> $PREFIX (build in $WORKDIR, -j$JOBS)"

mkdir -p "$WORKDIR"
if [ ! -d "$WORKDIR/opencv/.git" ]; then
    git clone --depth 1 --branch "$OPENCV_VERSION" \
        https://github.com/opencv/opencv.git "$WORKDIR/opencv"
fi

# Only the modules the detector needs. Trimming the module list (no gapi, no
# python/java bindings, no videoio/highgui — GStreamer is the engine's job, not
# OpenCV's) takes the build from ~40 minutes to under ten.
cmake -S "$WORKDIR/opencv" -B "$WORKDIR/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_LIST=core,imgproc,imgcodecs,dnn \
    -DOPENCV_GENERATE_PKGCONFIG=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_opencv_apps=OFF \
    -DBUILD_JAVA=OFF \
    -DBUILD_opencv_python2=OFF \
    -DBUILD_opencv_python3=OFF \
    -DWITH_GSTREAMER=OFF \
    -DWITH_FFMPEG=OFF \
    -DWITH_GTK=OFF \
    -DWITH_QT=OFF

cmake --build "$WORKDIR/build" -j"$JOBS"
cmake --install "$WORKDIR/build"

echo
echo "==> installed:"
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
    pkg-config --modversion opencv4
echo "==> reconfigure the engine so it picks this up:"
echo "    rm -rf concept_A/MediaFusionGCV/build"
echo "    cmake -S concept_A/MediaFusionGCV -B concept_A/MediaFusionGCV/build"
