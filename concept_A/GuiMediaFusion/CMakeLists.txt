cmake_minimum_required(VERSION 3.28)
project(GUIMediaFusion VERSION 2.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
qt_standard_project_setup()

# GStreamer is used by StreamReceiver (unixfdsrc display). The GUI talks to the
# backend over the control socket (ControlClient), so it does NOT link the
# MediaFusionGCV library — it only needs the shared type headers (SourceType etc).
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-video-1.0)

set(SOURCE_FILES
    main.cpp
    MainWindow.cpp
    ControlClient.cpp
    StreamReceiver.cpp
    core/AppLog.cpp
    core/BackendService.cpp
    core/DeviceParser.cpp
    core/SystemMonitor.cpp
    theme/Theme.cpp
    widgets/Components.cpp
    widgets/Shell.cpp
    widgets/VideoTile.cpp
    pages/DashboardPage.cpp
    pages/PipelinePage.cpp
    pages/MultiGridPage.cpp
    pages/AnalyticsPage.cpp
    pages/ComparePage.cpp
    pages/SettingsPage.cpp
    pages/LogsPage.cpp
    dialogs/DeviceManagerDialog.cpp

    resources.qrc
)

set(HEADER_FILES
    MainWindow.h
    ControlClient.h
    StreamReceiver.h
    core/AppLog.h
    core/BackendService.h
    core/DeviceParser.h
    core/SystemMonitor.h
    theme/Theme.h
    widgets/Components.h
    widgets/Shell.h
    widgets/VideoTile.h
    pages/DashboardPage.h
    pages/PipelinePage.h
    pages/MultiGridPage.h
    pages/AnalyticsPage.h
    pages/ComparePage.h
    pages/SettingsPage.h
    pages/LogsPage.h
    dialogs/DeviceManagerDialog.h
)

qt_add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

target_include_directories(${PROJECT_NAME}
 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../MediaFusionGCV/
)

target_link_libraries(${PROJECT_NAME}
 PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    PkgConfig::GST
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/"
)
