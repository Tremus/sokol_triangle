#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL

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

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

static void init(void)
{
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    setup_hello_world();

    // // a vertex buffer with 3 vertices
    // // clang-format off
    // float vertices[] = {
    //     // positions
    //      0.0f,  0.5f, 0.5f,
    //      0.5f, -0.5f, 0.5f,
    //     -0.5f, -0.5f, 0.5f,
    // };
    // // clang-format on
    // sg_buffer_desc white_buf = (sg_buffer_desc){.data = SG_RANGE(vertices), .label = "white-triangles"};
    // state.bind.vertex_buffers[0] = sg_make_buffer(&white_buf);

    // // create shader from code-generated sg_shader_desc
    // sg_shader shd = sg_make_shader(white_shader_desc(sg_query_backend()));

    // // create a pipeline object (default render states are fine for triangle)
    // state.pip = sg_make_pipeline(&(sg_pipeline_desc){
    //     .shader = shd,
    //     // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
    //     .layout =
    //         {.attrs =
    //              {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3}},
    //     .label = "triangle-pipeline"});

    // // a pass action to clear framebuffer to black
    // state.pass_action =
    //     (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

void frame(void)
{
    tick_hello_world();
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