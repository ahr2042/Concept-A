cmake_minimum_required(VERSION 3.28)
project(MediaFusionGCV VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_search_module(gstreamer       REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app   REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
pkg_check_modules(gstreamer-check REQUIRED IMPORTED_TARGET gstreamer-check-1.0)

# The detector needs OpenCV 4.8+: the 4.6 that Ubuntu 24.04 ships cannot import
# current YOLO ONNX exports (FP16 initializers, then an unsupported Split node).
# scripts/build-opencv.sh installs a suitable build into a user-writable prefix;
# look there first so no configure-time flags are needed.
foreach(_cvPrefix "$ENV{HOME}/.local" "/opt/opencv")
    if(EXISTS "${_cvPrefix}/lib/pkgconfig/opencv4.pc")
        set(ENV{PKG_CONFIG_PATH} "${_cvPrefix}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endforeach()

pkg_check_modules(opencv REQUIRED IMPORTED_TARGET opencv4>=4.8)
message(STATUS "OpenCV ${opencv_VERSION} (${opencv_PREFIX})")

# ── Optional GPU acceleration ─────────────────────────────────────────────────
# WITH_GPU pulls in ncnn (built with Vulkan) for the detector's GPU path. It is
# auto-detected: a machine without ncnn (CI, a fresh clone) still builds, CPU
# only, exactly like it degrades to "no models available". scripts/build-ncnn.sh
# installs a suitable ncnn into ~/.local, discovered via CMAKE_PREFIX_PATH below.
# WITH_CUDA is the NVIDIA placeholder — OFF until that hardware exists; its code
# paths are #ifdef'd out, not stubbed at runtime, so nothing NVIDIA links today.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local" "/opt/ncnn")
find_package(ncnn QUIET)
option(WITH_GPU  "GPU inference via ncnn + Vulkan"          ${ncnn_FOUND})
option(WITH_CUDA "NVIDIA/CUDA inference backend (placeholder)" OFF)

# ── Shared library (everything except main.cpp) ───────────────────────────────
set(LIB_SOURCES
    GStreamerSink.cpp
    GStreamerSinkScreen.cpp
    GStreamerSinkApplication.cpp
    GStreamerSource.cpp
    GStreamerSourceCamera.cpp
    PipelineManager.cpp
    MediaFusionGCV_API.cpp
    FrameProcessor.cpp
    Algorithms.cpp
    Detector.cpp
    InferenceBackend.cpp
    ModelRegistry.cpp
    AccelBackend.cpp
    AcceleratorRegistry.cpp
)

add_library(MediaFusionGCV_Lib STATIC ${LIB_SOURCES})

target_include_directories(MediaFusionGCV_Lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MediaFusionGCV_Lib PUBLIC
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
    PkgConfig::opencv
)

if(WITH_GPU)
    find_package(Vulkan REQUIRED)
    target_sources(MediaFusionGCV_Lib PRIVATE InferenceBackendNcnn.cpp)
    target_link_libraries(MediaFusionGCV_Lib PUBLIC ncnn Vulkan::Vulkan)
    target_compile_definitions(MediaFusionGCV_Lib PUBLIC MEDIAFUSION_WITH_GPU)
    message(STATUS "GPU inference: ncnn ${ncnn_VERSION} + Vulkan ENABLED")
else()
    message(STATUS "GPU inference: DISABLED (ncnn not found) -- CPU-only build")
endif()

if(WITH_CUDA)
    # Placeholder: no CUDA libs are linked yet; the backend is a compiled stub.
    target_sources(MediaFusionGCV_Lib PRIVATE InferenceBackendCuda.cpp)
    target_compile_definitions(MediaFusionGCV_Lib PUBLIC MEDIAFUSION_WITH_CUDA)
    message(STATUS "CUDA backend: placeholder compiled in (no NVIDIA libs linked)")
endif()

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE MediaFusionGCV_Lib)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/"
)

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(MediaFusionGCV_Tests
    tests/test_main.cpp
    tests/test_api_null_safety.cpp
    tests/test_pipeline_lifecycle.cpp
    tests/test_device_enumeration.cpp
    tests/test_inference.cpp
)

target_include_directories(MediaFusionGCV_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${gstreamer-check_INCLUDE_DIRS}
)

target_link_libraries(MediaFusionGCV_Tests PRIVATE
    MediaFusionGCV_Lib
    PkgConfig::gstreamer-check
)

enable_testing()
add_test(NAME MediaFusionGCV_Tests COMMAND MediaFusionGCV_Tests)
