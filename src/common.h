#pragma once

#ifndef NDEBUG
#ifdef _WIN32
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif
#endif // NDEBUG

enum
{
    APP_WIDTH  = 640,
    APP_HEIGHT = 480,
};

#define ARRLEN(a) (sizeof(a) / sizeof(a[0]))

#include <cplug_extensions/window.h>
#include <sokol_gfx.h>

#include <xhl/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void* g_pw;

void println(const char* const fmt, ...);
void program_setup();
void program_shutdown();
void program_tick();
bool program_event(const PWEvent* event);

sg_swapchain get_swapchain(sg_pixel_format pixel_format);

sg_color_target_state get_blending();

typedef struct XFile
{
    void*  data;
    size_t size;
} XFile;
XFile read_file(const char* path);

typedef struct Image
{
    sg_image img;
    sg_view  texview;
    int      width;
    int      height;
} Image;
Image load_image_file(const char* path);

#ifdef __cplusplus
}
#endif