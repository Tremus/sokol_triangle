cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)

set(MY_SOURCE_FILES 
    src/program_hello_world.c
)

if (APPLE)
    enable_language(OBJC)
    add_executable(sokol_triangle
        src/main.m
        ${MY_SOURCE_FILES}
        )
    target_link_libraries(sokol_triangle PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
else()
    add_executable(sokol_triangle
        src/main.c
        ${MY_SOURCE_FILES}
        )
endif()