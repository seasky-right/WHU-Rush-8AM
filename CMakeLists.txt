cmake_minimum_required(VERSION 3.19)
project(WHU-8am-Rush LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(WHU-8am-Rush
    WIN32 MACOSX_BUNDLE
    main.cpp
    view/mainwindow.cpp
    view/mainwindow.h
    mainwindow.ui
    model/GraphModel.h model/GraphModel.cpp
    model/PathRecommendation.h
    GraphData.h
    view/MapWidget.h view/MapWidget.cpp
    view/HoverBubble.h view/HoverBubble.cpp
    view/RouteButton.h view/RouteButton.cpp
)

qt_generate_deploy_app_script(
    TARGET WHU-8am-Rush
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Post-build: copy Data directory to the target executable directory so runtime can find Data/map.png and data files
add_custom_command(TARGET WHU-8am-Rush POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Data"
    "$<TARGET_FILE_DIR:WHU-8am-Rush>/Data"
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Data DESTINATION ${CMAKE_BINARY_DIR})

# Ensure C++17 and explicitly link required Qt6 modules to avoid undefined references at link time
set_target_properties(WHU-8am-Rush PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_link_libraries(WHU-8am-Rush PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)
