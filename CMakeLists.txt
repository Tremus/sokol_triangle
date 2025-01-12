cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)
set(CMAKE_CXX_STANDARD 20)

### PATHKIT ###
add_library(pathkit STATIC
    modules/pathkit/unity_pathkit_core.cpp
    modules/pathkit/unity_pathkit_misc.cpp
    modules/pathkit/unity_pathkit_pathops1.cpp
    modules/pathkit/unity_pathkit_pathops2.cpp
    )
target_include_directories(pathkit PRIVATE modules/pathkit/)

### SELECT PROGRAM ###
# set(SRC_FILE program_hello_triangle.c)
# set(SRC_FILE program_hello_quad.c)
# set(SRC_FILE program_minimal.c)
# set(SRC_FILE program_texquad.c)
# set(SRC_FILE program_offscreen_render.c)
# set(SRC_FILE program_fxaa.c)
# set(SRC_FILE program_msaa.c)
# set(SRC_FILE program_animated_vertices.c)
# set(SRC_FILE program_draw_rect.c)
# set(SRC_FILE program_line_stroke_texquad_1D.c)
# set(SRC_FILE program_gaussian_blur.c)
# set(SRC_FILE program_sdf_shapes.c)
# set(SRC_FILE program_flatten_quadbez.c)
# set(SRC_FILE program_pathkit.cpp)
# set(SRC_FILE program_pathkit_msaa.cpp)
# set(SRC_FILE program_nanovg.c)
set(SRC_FILE program_msdf.c)

string(REPLACE ".cpp" "" FILENAME ${SRC_FILE})
string(REPLACE ".c" "" FILENAME ${FILENAME})

set(LIBS pathkit)
set(SOURCES
    src/main.c
    src/${SRC_FILE}
    src/nanovg.c)
set(COMPILE_SHADER_CMD "")

if (APPLE)
    enable_language(OBJC)
    
    list(APPEND SOURCES src/platform_mac.m)
    list(APPEND LIBS "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
    set(COMPILE_SHADER_CMD sokol-shdc --input ${PROJECT_SOURCE_DIR}/src/${FILENAME}.glsl --output ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.h --slang metal_macos)
elseif(WIN32)
    list(APPEND SOURCES src/platform_win.c)
    set(COMPILE_SHADER_CMD sokol-shdc "--input \"${PROJECT_SOURCE_DIR}/src/${FILENAME}.glsl\" --output \"${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.h\" --slang hlsl5")
endif()

### COMPILE SHADERS ###
# https://discourse.cmake.org/t/pre-ninja-bash-command/1418/5
add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/shader.h.noexist
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.h
    COMMAND
        ${COMPILE_SHADER_CMD}
)
add_custom_target(shader_header
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.h
)

### MAIN TARGET ###
add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} shader_header)

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/shaders/
    modules/pathkit
    modules/xhl/include
    src/
    )
target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror=absolute-value
    -Werror=return-type
    -Werror=shadow
    -Werror=incompatible-pointer-types
    -Werror=parentheses
    -Werror=excess-initializers
    # -Wunused-function
    # -Wunused-variable

    -Wno-deprecated
    -Wno-multichar
    -Wno-nullability-completeness
    -Wno-writable-strings
    -Wno-c2x-extensions
    -Wno-c++14-extensions
    -Wno-c++17-extensions
    -Wno-c++20-extensions
    -Wno-microsoft-enum-forward-reference
    -Wno-c99-designator
)
target_compile_definitions(${PROJECT_NAME} PRIVATE SRC_DIR="${PROJECT_SOURCE_DIR}")