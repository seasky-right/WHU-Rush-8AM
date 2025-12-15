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
    GraphData.h
    view/MapWidget.h view/MapWidget.cpp
    view/HoverBubble.h view/HoverBubble.cpp
)

target_link_libraries(WHU-8am-Rush
    PRIVATE
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS WHU-8am-Rush
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
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
