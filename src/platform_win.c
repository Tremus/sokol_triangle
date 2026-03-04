#define XHL_TIME_IMPL
#define XHL_FILES_IMPL
#define XHL_STRING_IMPL

#include "common.h"

#include <Windows.h>
#include <stdarg.h>
#include <xhl/string.h>

void println(const char* const fmt, ...)
{
    char    buf[256] = {0};
    va_list args;
    va_start(args, fmt);
    int n = xtr_fmt_va(buf, sizeof(buf), fmt, args);
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

#include <xhl/files.h>
#include <xhl/time.h>
