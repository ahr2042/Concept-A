cmake_minimum_required(VERSION 3.28)
project(GUIMediaFusion VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



set(Qt6_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../dependencies/Qt6/Debug/lib/cmake/Qt6/)


find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
qt_standard_project_setup()


include_directories(CMAKE_CURRENT_SOURCE_DIR

)

set(SOURCE_FILES
GuiMediaFusion.cpp
GuiMediaFusionController.cpp
GuiMediaFusionModel.cpp
main.cpp
stdafx.cpp

GuiMediaFusion.ui
PreLaunchSettings.ui
GuiMediaFusion.qrc
)

set(HEADER_FILES 
errorStateGui.h
guiElements.h
GuiMediaFusion.h
GuiMediaFusionController.h
GuiMediaFusionModel.h
stdafx.h
)



qt_add_executable(${PROJECT_NAME}
${SOURCE_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
Qt6::Widgets
${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/libMediaFusionGCV_Lib.so
)

target_include_directories(${PROJECT_NAME}
 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../MediaFusionGCV/
  ${CMAKE_CURRENT_SOURCE_DIR}/build/GUIMediaFusion_autogen/include
  )

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/") 


set_target_properties( ${PROJECT_NAME} 
    PROPERTIES
    EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../x64_debug/"
)


