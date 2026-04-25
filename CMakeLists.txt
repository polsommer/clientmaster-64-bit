cmake_minimum_required(VERSION 3.24)

project(SWGGodClientModernized LANGUAGES CXX)

set(_swg_detected_arch "x64")
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(_swg_detected_arch "x86")
endif()

set(SWG_TARGET_ARCH "${_swg_detected_arch}" CACHE STRING "Target architecture (x86 or x64)")
set_property(CACHE SWG_TARGET_ARCH PROPERTY STRINGS x86 x64)

if(NOT SWG_TARGET_ARCH MATCHES "^(x86|x64)$")
    message(FATAL_ERROR "Invalid SWG_TARGET_ARCH='${SWG_TARGET_ARCH}'. Supported values are: x86, x64.")
endif()

if(MSVC AND CMAKE_GENERATOR_PLATFORM)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _swg_generator_platform)
    if(SWG_TARGET_ARCH STREQUAL "x86" AND NOT _swg_generator_platform STREQUAL "win32")
        message(FATAL_ERROR "SWG_TARGET_ARCH is x86, but MSVC generator platform is '${CMAKE_GENERATOR_PLATFORM}'. Use -A Win32.")
    elseif(SWG_TARGET_ARCH STREQUAL "x64" AND NOT _swg_generator_platform STREQUAL "x64")
        message(FATAL_ERROR "SWG_TARGET_ARCH is x64, but MSVC generator platform is '${CMAKE_GENERATOR_PLATFORM}'. Use -A x64.")
    endif()
endif()

if(CMAKE_SIZEOF_VOID_P)
    if(SWG_TARGET_ARCH STREQUAL "x86" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "SWG_TARGET_ARCH is x86, but the active toolchain is ${CMAKE_SIZEOF_VOID_P}-byte pointers.")
    elseif(SWG_TARGET_ARCH STREQUAL "x64" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "SWG_TARGET_ARCH is x64, but the active toolchain is ${CMAKE_SIZEOF_VOID_P}-byte pointers.")
    endif()
endif()

if(WIN32)
    if(SWG_TARGET_ARCH STREQUAL "x86")
        set(SWG_OUTPUT_PLATFORM_ARCH "win32")
    else()
        set(SWG_OUTPUT_PLATFORM_ARCH "win64")
    endif()
elseif(APPLE)
    set(SWG_OUTPUT_PLATFORM_ARCH "macos-${SWG_TARGET_ARCH}")
else()
    set(SWG_OUTPUT_PLATFORM_ARCH "linux-${SWG_TARGET_ARCH}")
endif()

set(SWG_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/compile/${SWG_OUTPUT_PLATFORM_ARCH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SWG_OUTPUT_ROOT}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SWG_OUTPUT_ROOT}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SWG_OUTPUT_ROOT}/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${SWG_OUTPUT_ROOT}/pdb")

foreach(_swg_cfg ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${_swg_cfg}" _swg_cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_swg_cfg_upper} "${SWG_OUTPUT_ROOT}/${_swg_cfg}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_swg_cfg_upper} "${SWG_OUTPUT_ROOT}/${_swg_cfg}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_swg_cfg_upper} "${SWG_OUTPUT_ROOT}/${_swg_cfg}/lib")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_${_swg_cfg_upper} "${SWG_OUTPUT_ROOT}/${_swg_cfg}/pdb")
endforeach()

if(SWG_TARGET_ARCH STREQUAL "x86")
    add_compile_definitions(SWG_TARGET_ARCH_X86=1 SWG_TARGET_ARCH_X64=0)
else()
    add_compile_definitions(SWG_TARGET_ARCH_X86=0 SWG_TARGET_ARCH_X64=1)
endif()

message(STATUS "SWG configure summary: target architecture='${SWG_TARGET_ARCH}', output root='${SWG_OUTPUT_ROOT}'")

option(SWG_ENABLE_LUA "Enable the embedded Lua automation layer" ON)
option(SWG_ENABLE_TESTS "Build unit tests for modernised tooling" ON)
option(SWG_ENABLE_TERRAIN_AUTOPAINTER_HEADLESS "Build the headless TerrainAutoPainter driver" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swg_plugin_api INTERFACE)

add_library(swg_plugin_manifest STATIC
    src/engine/shared/plugin/PluginManifest.cpp
)

target_include_directories(swg_plugin_manifest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shared/plugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/3rd/library/webAPI
)

set_target_properties(swg_plugin_manifest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

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
    RUNTIME DESTINATION plugin/examples/world_builder_procedural/${SWG_OUTPUT_PLATFORM_ARCH}
    LIBRARY DESTINATION plugin/examples/world_builder_procedural/${SWG_OUTPUT_PLATFORM_ARCH}
    ARCHIVE DESTINATION plugin/examples/world_builder_procedural/${SWG_OUTPUT_PLATFORM_ARCH}
)

install(TARGETS AiLoadTesterPlugin
    RUNTIME DESTINATION plugin/ai_load_tester/${SWG_OUTPUT_PLATFORM_ARCH}
    LIBRARY DESTINATION plugin/ai_load_tester/${SWG_OUTPUT_PLATFORM_ARCH}
    ARCHIVE DESTINATION plugin/ai_load_tester/${SWG_OUTPUT_PLATFORM_ARCH}
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
