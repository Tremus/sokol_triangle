#pragma once

#ifdef __APPLE__
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif

void print(const char* const fmt, ...);
void program_setup();
void program_tick();