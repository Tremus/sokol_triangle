cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)

set(MY_SOURCE_FILES 
    src/main.c
    # src/program_hello_triangle.c
    # src/program_hello_quad.c
    # src/program_minimal.c
    # src/program_texquad.c
    # src/program_offscreen_render.c
    # src/program_fxaa.c
    # src/program_msaa.c
    # src/program_animated_vertices.c
    src/program_draw_rect.c
)

if (APPLE)
    enable_language(OBJC)
    add_executable(sokol_triangle
        ${MY_SOURCE_FILES}
        src/platform_mac.m
        )
    target_link_libraries(sokol_triangle PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
elseif(WIN32)
    add_executable(sokol_triangle
        ${MY_SOURCE_FILES}
        src/platform_win.c
        )
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