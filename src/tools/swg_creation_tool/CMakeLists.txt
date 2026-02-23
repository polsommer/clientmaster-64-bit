add_library(swg_creation_core STATIC
    IffBuilder.cpp
    Json.cpp
)

target_include_directories(swg_creation_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(swg_creation_core PRIVATE /W4 /permissive- /external:anglebrackets /external:W0)
else()
    target_compile_options(swg_creation_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(swg_creation_tool
    main.cpp
)

target_link_libraries(swg_creation_tool PRIVATE swg_creation_core)

if(MSVC)
    target_compile_options(swg_creation_tool PRIVATE /W4 /permissive- /external:anglebrackets /external:W0)
else()
    target_compile_options(swg_creation_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(swg_creation_tool PROPERTIES
    OUTPUT_NAME "swg+creation_tool"
)
