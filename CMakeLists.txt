cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)

# set(MY_PROGRAM program_hello_triangle)
# set(MY_PROGRAM program_hello_quad)
# set(MY_PROGRAM program_minimal)
# set(MY_PROGRAM program_texquad)
# set(MY_PROGRAM program_offscreen_render)
# set(MY_PROGRAM program_fxaa)
# set(MY_PROGRAM program_msaa)
# set(MY_PROGRAM program_animated_vertices)
# set(MY_PROGRAM program_draw_rect)
set(MY_PROGRAM program_line_stroke_texquad_1D)

if (APPLE)
    enable_language(OBJC)
    add_executable(sokol_triangle
        src/main.c
        src/${MY_PROGRAM}.c
        src/platform_mac.m
        )
    target_link_libraries(sokol_triangle PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
elseif(WIN32)
    # https://discourse.cmake.org/t/pre-ninja-bash-command/1418/5
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/shaders/shader.h.noexist
            ${CMAKE_CURRENT_BINARY_DIR}/shaders/${MY_PROGRAM}.h
        COMMAND
            sokol-shdc.exe "--input \"${PROJECT_SOURCE_DIR}/src/${MY_PROGRAM}.glsl\" --output \"${CMAKE_CURRENT_BINARY_DIR}/shaders/${MY_PROGRAM}.h\" --slang hlsl5"
    )
    add_custom_target(shader_header
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/shaders/${MY_PROGRAM}.h
    )

    add_executable(sokol_triangle
        src/main.c
        src/${MY_PROGRAM}.c
        src/platform_win.c
        )
    target_include_directories(sokol_triangle PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
    add_dependencies(sokol_triangle shader_header)
endif()

target_compile_options(sokol_triangle PRIVATE
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
)