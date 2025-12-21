cmake_minimum_required(VERSION 3.19)
project(WHU-8am-Rush LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(WHU-8am-Rush
    WIN32 MACOSX_BUNDLE
    main.cpp
    view/mainwindow.cpp
    view/mainwindow.h
    view/EditorWindow.cpp
    view/EditorWindow.h
    mainwindow.ui
    model/GraphModel.h model/GraphModel.cpp
    model/PathRecommendation.h
    model/MapEditor.h model/MapEditor.cpp
    GraphData.h
    view/MapWidget.h view/MapWidget.cpp
    view/HoverBubble.h view/HoverBubble.cpp
    view/RouteButton.h view/RouteButton.cpp
    
    # 【关键！必须包含这两个文件】
    view/WeatherOverlay.h 
    view/WeatherOverlay.cpp
)

# 【关键！不要忘了这一行，否则会报 QInputDialog 错误】
target_link_libraries(WHU-8am-Rush PRIVATE Qt6::Core Qt6::Widgets)

qt_generate_deploy_app_script(
    TARGET WHU-8am-Rush
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Post-build: copy Data directory
add_custom_command(TARGET WHU-8am-Rush POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Data"
    "$<TARGET_FILE_DIR:WHU-8am-Rush>/Data"
)