cmake_minimum_required(VERSION 3.24)

option(SWG_ENABLE_TRE_GUI "Build the TRE/TRES GUI archive studio" ON)

find_package(ZLIB REQUIRED)

add_library(swg_tre_common STATIC
    TreArchive.cpp
    TreArchive.h
)

target_link_libraries(swg_tre_common PUBLIC ZLIB::ZLIB swg_creation_core)
target_include_directories(swg_tre_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../swg_creation_tool)

add_executable(swg_tre_tool
    cli.cpp
)

target_link_libraries(swg_tre_tool PRIVATE swg_tre_common)

if(MSVC)
    target_compile_options(swg_tre_tool PRIVATE /W4 /permissive-)
else()
    target_compile_options(swg_tre_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SWG_ENABLE_TRE_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    set(QT_PACKAGE "Qt6")
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets QUIET)
        set(QT_PACKAGE "Qt5")
    endif()

    if(Qt6_FOUND OR Qt5_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        add_executable(swg_tool_gui
            MainWindow.cpp
            MainWindow.h
            main.cpp
        )

        target_link_libraries(swg_tool_gui PRIVATE ${QT_PACKAGE}::Widgets swg_tre_common)

        if(MSVC)
            target_compile_options(swg_tool_gui PRIVATE /W4 /permissive-)
        else()
            target_compile_options(swg_tool_gui PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_executable(swg_tre_gui ALIAS swg_tool_gui)

        install(TARGETS swg_tool_gui RUNTIME DESTINATION tools)
    else()
        message(STATUS "Qt Widgets not found, skipping swg_tre_gui target")
    endif()
else()
    message(STATUS "TRE GUI disabled (SWG_ENABLE_TRE_GUI=OFF)")
endif()

install(TARGETS swg_tre_tool RUNTIME DESTINATION tools)
