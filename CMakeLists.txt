cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)

set(MY_SOURCE_FILES 
    src/main.c
    # src/program_hello_world.c
    # src/program_white.c
    # src/program_quad.c
    # src/program_texquad.c
    src/program_offscreen_render.c
)

if (APPLE)
    enable_language(OBJC)
    add_executable(sokol_triangle
        ${MY_SOURCE_FILES}
        src/platform_mac.m
        )
    target_link_libraries(sokol_triangle PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
else()
    add_executable(sokol_triangle
        ${MY_SOURCE_FILES}
        src/platform_win.c
        )
endif()