cmake_minimum_required(VERSION 3.15)
project(sokol_triangle VERSION 0.0.1)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_STANDARD 11)
else()
    set(CMAKE_C_STANDARD 99)
endif()

set(SRC_DIR ${PROJECT_SOURCE_DIR})
set(BIN_DIR ${CMAKE_BINARY_DIR})
if (WIN32)
    # Windows paths are complicated
    string(REPLACE "/" "\\\\" SRC_DIR ${SRC_DIR})
    string(REPLACE "/" "\\\\" BIN_DIR ${BIN_DIR})
endif()

set(PLUGIN_OPTIONS "")

# https://clang.llvm.org/docs/ClangCommandLineReference.html
if (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    list(APPEND PLUGIN_OPTIONS
        -Werror=absolute-value
        -Werror=return-type
        -Werror=shadow
        -Werror=incompatible-pointer-types
        -Werror=parentheses
        -Werror=excess-initializers
        -Werror=array-bounds
        # -Wunused-function
        # -Wunused-variable

        -Wno-deprecated-declarations
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
endif()
# list(APPEND CMAKE_C_FLAGS -fsanitize=address)
# list(APPEND CMAKE_CXX_FLAGS -fsanitize=address)

if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    list(APPEND PLUGIN_OPTIONS /GR- /EHasc- "/FI${PROJECT_SOURCE_DIR}/src/common.h")
    list(APPEND PLUGIN_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:/Zi>")
    add_link_options("$<$<CONFIG:RELEASE>:/DEBUG>")
    if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        list(APPEND PLUGIN_OPTIONS /Od)
    endif()
else()
    list(APPEND PLUGIN_OPTIONS -fno-rtti -fno-exceptions)
    if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        list(APPEND PLUGIN_OPTIONS -fno-inline-functions)
    endif()
endif()

set(PLUGIN_INCLUDE
    modules/xhl/include
    modules/CPLUG/src/
    modules/sokol_gfx_multiinstance
    # modules/wip_libs/src/

    modules/pathkit
    )

set(PLUGIN_LIBRARIES "")

set(PLUGIN_DEFINITIONS
    CPLUG_COMPANY_NAME="${COMPANY_NAME}"
    CPLUG_COMPANY_EMAIL="contact@exacoustics.com"
    CPLUG_PLUGIN_NAME="${PROJECT_NAME}"
    CPLUG_PLUGIN_URI="${PROJECT_HOMEPAGE_URL}"
    CPLUG_PLUGIN_VERSION="${PROJECT_VERSION}"
    _HAS_EXCEPTIONS=0
    STBI_NO_STDIO
    STBI_NO_LINEAR
    NVG_NO_STB

    SRC_DIR="${SRC_DIR}"
    BIN_DIR="${BIN_DIR}"
    )

set(PLUGIN_SOURCES
    src/main.c
    src/stb_image.c
    )

### SELECT PROGRAM ###
# list(APPEND PLUGIN_SOURCES src/program_hello_triangle.c)
# list(APPEND PLUGIN_SOURCES src/program_hello_quad.c)
# list(APPEND PLUGIN_SOURCES src/program_minimal.c)
# list(APPEND PLUGIN_SOURCES src/program_texquad.c)
# list(APPEND PLUGIN_SOURCES src/program_offscreen_render.c)
# list(APPEND PLUGIN_SOURCES src/program_fxaa.c)
# list(APPEND PLUGIN_SOURCES src/program_msaa.c) # this one isn't working and Im too lazy to fix it
# list(APPEND PLUGIN_SOURCES src/program_animated_vertices.c)
# list(APPEND PLUGIN_SOURCES src/program_draw_rect.c)
# list(APPEND PLUGIN_SOURCES src/program_line_stroke_texquad_1D.c)
# list(APPEND PLUGIN_SOURCES src/program_gaussian_blur.c)
# list(APPEND PLUGIN_SOURCES src/program_sdf_shapes.c)
# list(APPEND PLUGIN_SOURCES src/program_flatten_quadbez.c)
# list(APPEND PLUGIN_SOURCES src/program_pathkit.cpp)
# list(APPEND PLUGIN_SOURCES src/program_pathkit_msaa.cpp) # also not working
# list(APPEND PLUGIN_SOURCES src/program_nanovg.c) # NOTE: nanovg doesn't work in its current state. The sokol backend still needs a bit of work
# list(APPEND PLUGIN_SOURCES src/program_msdf.c)
# list(APPEND PLUGIN_SOURCES src/program_dualfilter_blur.c)
# list(APPEND PLUGIN_SOURCES src/program_blur_compute.c)
# list(APPEND PLUGIN_SOURCES src/program_vertex_pull.c)
# list(APPEND PLUGIN_SOURCES src/program_vertex_pull_square.c)
# list(APPEND PLUGIN_SOURCES src/program_square_no_vbuf.c)
# list(APPEND PLUGIN_SOURCES src/program_vertex_pull_sdf.c)
# list(APPEND PLUGIN_SOURCES src/program_sdf_line_segment.c)
# list(APPEND PLUGIN_SOURCES src/program_line_stroke_sdf.c)
# list(APPEND PLUGIN_SOURCES src/program_sdf_compressed.c)
# list(APPEND PLUGIN_SOURCES src/program_sdf_line_tiled.c)
# list(APPEND PLUGIN_SOURCES src/program_3d_wavetable_stroked.c)
# list(APPEND PLUGIN_SOURCES src/program_compute_triangle.c)
list(APPEND PLUGIN_SOURCES src/program_compute_line_stroke.c)

if (WIN32)
    list(APPEND PLUGIN_DEFINITIONS UNICODE _UNICODE _USE_MATH_DEFINES PW_DX11)

    if (CMAKE_BUILD_TYPE MATCHES Release)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            list(APPEND PLUGIN_OPTIONS /arch:AVX)
        else()
            list(APPEND PLUGIN_OPTIONS -march=corei7-avx)
        endif()
    else()
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            list(APPEND PLUGIN_OPTIONS /arch:AVX2)
        else()
            list(APPEND PLUGIN_OPTIONS -march=haswell)
        endif()
    endif()
    list(APPEND PLUGIN_SOURCES
        src/platform_win.c
        modules/CPLUG/src/cplug_extensions/window_win.c
        modules/sokol_gfx_multiinstance/sokol_gfx_d3d11.c
        )
    list(APPEND PLUGIN_LIBRARIES dxguid)
elseif(APPLE)
    enable_language(OBJC)
    # if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    list(APPEND PLUGIN_OPTIONS -g)
    # endif()

    list(APPEND PLUGIN_SOURCES
        src/platform_mac.m
        modules/CPLUG/src/cplug_extensions/window_osx.m
        modules/sokol_gfx_multiinstance/sokol_gfx_metal.m
        )
    list(APPEND PLUGIN_LIBRARIES
        "-framework Quartz"
        "-framework Cocoa"
        "-framework Metal"
        "-framework MetalKit"
        "-framework AudioToolbox"
        )
    list(APPEND PLUGIN_DEFINITIONS PW_METAL)
endif()

### PATHKIT ###
add_library(pathkit STATIC
    modules/pathkit/unity_pathkit_core.cpp
    modules/pathkit/unity_pathkit_misc.cpp
    modules/pathkit/unity_pathkit_pathops1.cpp
    modules/pathkit/unity_pathkit_pathops2.cpp
    )
target_include_directories(pathkit PRIVATE modules/pathkit/)

# ██╗  ██╗ ██████╗ ████████╗██████╗ ███████╗██╗      ██████╗  █████╗ ██████╗
# ██║  ██║██╔═══██╗╚══██╔══╝██╔══██╗██╔════╝██║     ██╔═══██╗██╔══██╗██╔══██╗
# ███████║██║   ██║   ██║   ██████╔╝█████╗  ██║     ██║   ██║███████║██║  ██║
# ██╔══██║██║   ██║   ██║   ██╔══██╗██╔══╝  ██║     ██║   ██║██╔══██║██║  ██║
# ██║  ██║╚██████╔╝   ██║   ██║  ██║███████╗███████╗╚██████╔╝██║  ██║██████╔╝
# ╚═╝  ╚═╝ ╚═════╝    ╚═╝   ╚═╝  ╚═╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═════╝

set(HOTRELOAD_LIB_NAME ${PROJECT_NAME}_hotreload_lib)
add_library(${HOTRELOAD_LIB_NAME} MODULE ${PLUGIN_SOURCES})
target_compile_definitions(${HOTRELOAD_LIB_NAME} PRIVATE ${PLUGIN_DEFINITIONS} CPLUG_SHARED CPLUG_BUILD_STANDALONE)
target_include_directories(${HOTRELOAD_LIB_NAME} PRIVATE ${PLUGIN_INCLUDE})
target_compile_options(${HOTRELOAD_LIB_NAME} PRIVATE ${PLUGIN_OPTIONS})
target_link_libraries(${HOTRELOAD_LIB_NAME} PRIVATE ${PLUGIN_LIBRARIES} pathkit)

set(HOTRELOAD_WATCH_DIR "")
set(HOTRELOAD_LIB_PATH "")
set(HOTRELOAD_HOST_LIBS "")

if (WIN32)
    add_executable(${PROJECT_NAME}_hotreload WIN32 modules/CPLUG/src/cplug_standalone_win.c)
    set(HOTRELOAD_WATCH_DIR "${SRC_DIR}\\\\src")
    set(HOTRELOAD_LIB_PATH "${BIN_DIR}\\\\${CMAKE_BUILD_TYPE}\\\\${HOTRELOAD_LIB_NAME}.dll")
endif()
if (APPLE)
    add_executable(${PROJECT_NAME}_hotreload MACOSX_BUNDLE modules/CPLUG/src/cplug_standalone_osx.m)
    file(TOUCH_NOCREATE "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}_hotreload.app/Contents/PkgInfo")
    file(WRITE "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}_hotreload.app/Contents/PkgInfo" "APPL????")

    set(HOTRELOAD_WATCH_DIR "${PROJECT_SOURCE_DIR}/src")
    set(HOTRELOAD_LIB_PATH "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib${HOTRELOAD_LIB_NAME}.so")
    set(HOTRELOAD_HOST_LIBS "-framework Cocoa -framework CoreMIDI -framework CoreAudio -framework CoreServices")
endif() # APPLE

target_compile_definitions(${PROJECT_NAME}_hotreload PRIVATE
    HOTRELOAD_WATCH_DIR="${HOTRELOAD_WATCH_DIR}"
    HOTRELOAD_LIB_PATH="${HOTRELOAD_LIB_PATH}"
    HOTRELOAD_BUILD_COMMAND="cmake --build ${BIN_DIR} --config Debug --target ${HOTRELOAD_LIB_NAME}"
    CPLUG_SHARED
    CPLUG_BUILD_STANDALONE
    )
target_compile_definitions(${PROJECT_NAME}_hotreload PRIVATE ${PLUGIN_DEFINITIONS} CPLUG_SHARED)
target_include_directories(${PROJECT_NAME}_hotreload PRIVATE ${PLUGIN_INCLUDE})
target_compile_options(${PROJECT_NAME}_hotreload PRIVATE ${PLUGIN_OPTIONS})
target_link_libraries(${PROJECT_NAME}_hotreload PRIVATE ${HOTRELOAD_HOST_LIBS})
# Forces plugin to be built before the host
add_dependencies(${PROJECT_NAME}_hotreload ${HOTRELOAD_LIB_NAME})


# ███████╗████████╗ █████╗ ███╗   ██╗██████╗  █████╗ ██╗      ██████╗ ███╗   ██╗███████╗
# ██╔════╝╚══██╔══╝██╔══██╗████╗  ██║██╔══██╗██╔══██╗██║     ██╔═══██╗████╗  ██║██╔════╝
# ███████╗   ██║   ███████║██╔██╗ ██║██║  ██║███████║██║     ██║   ██║██╔██╗ ██║█████╗  
# ╚════██║   ██║   ██╔══██║██║╚██╗██║██║  ██║██╔══██║██║     ██║   ██║██║╚██╗██║██╔══╝  
# ███████║   ██║   ██║  ██║██║ ╚████║██████╔╝██║  ██║███████╗╚██████╔╝██║ ╚████║███████╗
# ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝ ╚═╝  ╚═╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝

if (WIN32)
    add_executable(${PROJECT_NAME}_standalone WIN32 ${PLUGIN_SOURCES} modules/CPLUG/src/cplug_standalone_win.c)
    target_link_libraries(${PROJECT_NAME}_standalone PRIVATE ${PLUGIN_LIBRARIES})
elseif(APPLE)
    add_executable(${PROJECT_NAME}_standalone MACOSX_BUNDLE ${PLUGIN_SOURCES} modules/CPLUG/src/cplug_standalone_osx.m)
    target_link_libraries(${PROJECT_NAME}_standalone PRIVATE ${PLUGIN_LIBRARIES} "-framework CoreMIDI -framework CoreAudio -framework CoreServices")

    # cplug_configure_info_plist(${PROJECT_NAME}_standalone ${APP_BUNDLE_ID} "APPL" "app")

    file(TOUCH_NOCREATE "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.app/Contents/PkgInfo")
    file(WRITE "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.app/Contents/PkgInfo" "APPL????")
endif()
target_include_directories(${PROJECT_NAME}_standalone PRIVATE ${PLUGIN_INCLUDE})
target_compile_definitions(${PROJECT_NAME}_standalone PRIVATE ${PLUGIN_DEFINITIONS} CPLUG_BUILD_STANDALONE)
target_compile_options(${PROJECT_NAME}_standalone PRIVATE ${PLUGIN_OPTIONS})
target_precompile_headers(${PROJECT_NAME}_standalone PRIVATE src/common.h)
# add_dependencies(${PROJECT_NAME}_standalone shader_header)