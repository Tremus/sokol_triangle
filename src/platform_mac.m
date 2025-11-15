#define SOKOL_METAL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define XHL_TIME_IMPL
#define NANOVG_SOKOL_IMPLEMENTATION

#include "common.h"

#include <stdarg.h>
#include <stdio.h>

void println(const char* const fmt, ...)
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

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

// #include "nanovg_sokol.h"
#include "sokol_app.c"
#include "sokol_gfx.c"
#include <xhl/time.h>