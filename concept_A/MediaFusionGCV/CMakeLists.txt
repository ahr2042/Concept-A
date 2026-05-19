cmake_minimum_required(VERSION 3.28)
project(MediaFusionGCV VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_search_module(gstreamer       REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app   REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
pkg_check_modules(gstreamer-check REQUIRED IMPORTED_TARGET gstreamer-check-1.0)

# ── Shared library (everything except main.cpp) ───────────────────────────────
set(LIB_SOURCES
    GStreamerSink.cpp
    GStreamerSinkScreen.cpp
    GStreamerSource.cpp
    GStreamerSourceCamera.cpp
    PipelineManager.cpp
    MediaFusionGCV_API.cpp
)

add_library(MediaFusionGCV_Lib STATIC ${LIB_SOURCES})

target_include_directories(MediaFusionGCV_Lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MediaFusionGCV_Lib PUBLIC
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
)

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
