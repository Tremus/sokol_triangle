#ifdef _WIN32
#define SOKOL_D3D11
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>

static void log_win32(const char* const fmt, ...)
{
    char    buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n > 0)
    {
        if (n < sizeof(buf) && buf[n - 1] != '\n')
        {
            buf[n] = '\n';
            n++;
        }

        OutputDebugStringA(buf);
    }
}
#define print log_win32
#endif // _WIN32
#ifdef __APPLE__
#define SOKOL_METAL
#define SOKOL_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#include <stdarg.h>
#include <stdio.h>

static void log_macos(const char* const fmt, ...)
{
    char    buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n > 0)
    {
        if (n < sizeof(buf) && buf[n - 1] != '\n')
        {
            buf[n] = '\n';
            n++;
        }

        fwrite(buf, 1, n, stderr);
    }
}
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "common.h"

static void init(void)
{
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    program_setup();
}

void frame(void)
{
    program_tick();
    sg_commit();
}

void cleanup(void) { sg_shutdown(); }

sapp_desc sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .cleanup_cb = cleanup,
        // .event_cb           = __dbgui_event,
        .width              = 640,
        .height             = 480,
        .window_title       = "GFX (sokol-app)",
        .icon.sokol_default = true,
    };
}