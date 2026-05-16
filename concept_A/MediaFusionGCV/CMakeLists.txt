cmake_minimum_required(VERSION 3.28)
project(MediaFusionGCV_Lib VERSION 1.0 LANGUAGES CXX)


find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTLIBS REQUIRED
        gobject-2.0
        glib-2.0
        gstreamer-pbutils-1.0
        gstreamer-check-1.0
        )

        find_package(PkgConfig REQUIRED)
        pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
        pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
        pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
        
set(CMAKE_CXX_STANDARD 20)



include_directories(CMAKE_CURRENT_SOURCE_DIR)

link_directories(${GSTLIBS_LIBRARY_DIRS})


############################################################
# Create a library
############################################################

set(SOURCE_FILES
GStreamerSink.cpp
GStreamerSinkScreen.cpp
GStreamerSource.cpp
GStreamerSourceCamera.cpp
PipelineManager.cpp
MediaFusionGCV_API.cpp
)

set(HEADER_FILES 
errorState.h,
GStreamerSink.h,
GStreamerSinkScreen.h,
GStreamerSource.h,
GStreamerSourceCamera.h,
PipelineConfig.h,
PipelineManager.h,
ScreenSinks.h,
SinkType.h,
SourceType.h,
MediaFusionGCV_API.h
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})




target_link_libraries(${PROJECT_NAME}
    PkgConfig::gstreamer
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video
)


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/)

set_target_properties( ${PROJECT_NAME} 
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/"
)