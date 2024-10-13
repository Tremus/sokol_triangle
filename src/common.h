#pragma once

#ifdef _WIN32
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif

#define APP_WIDTH  640
#define APP_HEIGHT 480

#define ARRLEN(a) (sizeof(a) / sizeof(a[0]))

void print(const char* const fmt, ...);
void program_setup();
void program_tick();