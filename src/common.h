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

#ifdef __cplusplus
extern "C" {
#endif

extern void* g_pw;
void         println(const char* const fmt, ...);
void         program_setup();
void         program_shutdown();
void         program_tick();
void         program_event(const PWEvent* event);

#ifdef __cplusplus
}
#endif