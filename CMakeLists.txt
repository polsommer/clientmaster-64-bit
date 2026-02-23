cmake_minimum_required(VERSION 3.24)

project(SWGGodClientModernized LANGUAGES CXX)

option(SWG_ENABLE_LUA "Enable the embedded Lua automation layer" ON)
option(SWG_ENABLE_TESTS "Build unit tests for modernised tooling" ON)
option(SWG_ENABLE_TERRAIN_AUTOPAINTER_HEADLESS "Build the headless TerrainAutoPainter driver" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swg_plugin_api INTERFACE)
target_include_directories(swg_plugin_api
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared
)

target_compile_features(swg_plugin_api INTERFACE cxx_std_20)

add_library(WorldBuilderProceduralPlugin MODULE
    plugin/examples/world_builder_procedural/WorldBuilderProceduralPlugin.cpp
)

add_library(AiLoadTesterPlugin MODULE
    plugin/ai_load_tester/AiLoadTesterPlugin.cpp
)

target_include_directories(WorldBuilderProceduralPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared
)

target_link_libraries(WorldBuilderProceduralPlugin PRIVATE swg_plugin_api)

target_include_directories(AiLoadTesterPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared
)

target_link_libraries(AiLoadTesterPlugin PRIVATE swg_plugin_api)

if(WIN32)
    add_library(swg_direct3d10_bootstrap STATIC
        src/engine/client/application/Direct3d10/src/win32/Direct3d10.cpp
        src/engine/client/application/Direct3d10/src/win32/Direct3d10Bootstrap.cpp
    )

    target_include_directories(swg_direct3d10_bootstrap PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/client/application/Direct3d10/src/win32
    )

    target_compile_definitions(swg_direct3d10_bootstrap PRIVATE DIRECT3D10_BOOTSTRAP)

    if(MSVC)
        target_compile_options(swg_direct3d10_bootstrap PRIVATE /W4 /permissive-)
        target_link_libraries(swg_direct3d10_bootstrap PUBLIC dxgi d3d10 d3d10_1)
    else()
        target_compile_options(swg_direct3d10_bootstrap PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(swg_direct3d10_bootstrap PUBLIC dxgi d3d10 d3d10_1)
    endif()
endif()

if(MSVC)
    target_compile_options(WorldBuilderProceduralPlugin PRIVATE /W4 /permissive- /external:anglebrackets /external:W0)
else()
    target_compile_options(WorldBuilderProceduralPlugin PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(AiLoadTesterPlugin PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(WorldBuilderProceduralPlugin PROPERTIES
    OUTPUT_NAME "WorldBuilderProceduralPlugin"
)

set_target_properties(AiLoadTesterPlugin PROPERTIES
    OUTPUT_NAME "AiLoadTesterPlugin"
)

install(TARGETS WorldBuilderProceduralPlugin
    RUNTIME DESTINATION plugin/examples/world_builder_procedural
    LIBRARY DESTINATION plugin/examples/world_builder_procedural
    ARCHIVE DESTINATION plugin/examples/world_builder_procedural
)

install(TARGETS AiLoadTesterPlugin
    RUNTIME DESTINATION plugin/ai_load_tester
    LIBRARY DESTINATION plugin/ai_load_tester
    ARCHIVE DESTINATION plugin/ai_load_tester
)

install(FILES
    plugin/examples/world_builder_procedural/plugin.json
    DESTINATION plugin/examples/world_builder_procedural
)

install(FILES
    plugin/ai_load_tester/plugin.json
    plugin/ai_load_tester/scenario.sample.json
    DESTINATION plugin/ai_load_tester
)


add_subdirectory(src/tools/swg_creation_tool)
add_subdirectory(src/tools/swg_tre_gui)

if(SWG_ENABLE_TERRAIN_AUTOPAINTER_HEADLESS)
    add_subdirectory(src/tools/terrain_autopainter_headless)
endif()
