if(NOT WIN32)
    message(STATUS "Skipping TerrainAutoPainterHeadless build: Windows-only dependencies")
    return()
endif()

add_executable(TerrainAutoPainterHeadless
    main.cpp
)

target_include_directories(TerrainAutoPainterHeadless PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/client/application/TerrainEditor/src/win32
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/shared
)

# The driver reuses the existing editor components and therefore relies on
# the same MFC/ATL link dependencies as the desktop editor. Consumers should
# extend the link interface to match their environment when enabling this
# optional target.
