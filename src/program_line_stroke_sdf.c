#include "common.h"

#include "program_line_stroke_sdf.glsl.h"

#include <xhl/maths.h>
#include <xhl/time.h>

static struct
{
    int window_width;
    int window_height;

    float mouse_xy[2];

    sg_buffer      buf;
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state = {0};

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

bool         init_buffer = false;
static float AUDIO_BUFFER[2048];

void program_setup()
{
    xtime_init();

    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    state.mouse_xy[0] = 1;
    state.mouse_xy[1] = 0.5;

    state.pip = sg_make_pipeline(&(
        sg_pipeline_desc){.shader = sg_make_shader(line_stroke_shader_desc(sg_query_backend())), .label = "pipeline"});

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

    state.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(AUDIO_BUFFER),
        .label                = "sine-buffer",
    });

    state.bind.views[VIEW_storage_buffer] = sg_make_view(&(sg_view_desc){.storage_buffer = state.buf});
}
void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.window_width  = e->resize.width;
        state.window_height = e->resize.height;
        init_buffer         = false;
    }
    else if (e->type == PW_EVENT_MOUSE_MOVE || e->type == PW_EVENT_MOUSE_ENTER || e->type == PW_EVENT_MOUSE_EXIT)
    {
        state.mouse_xy[0] = e->mouse.x / (float)state.window_width;
        state.mouse_xy[1] = e->mouse.y / (float)state.window_height;
    }
    return false;
}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});

#ifdef __APPLE__
    // Note: this code was written with the assumption the app will only ever run on a macbook retina screen
    // Other devices will likely see bad results
    // Unfortunately this will cause our lovely 2px line to look like a 1px line. This is because we will now need to
    // read from twice the amount of pixels from the left and right
    // This means the shader will likely need to be rewritten to support macOS.
    const int backingScaleFactor = 2;
#else
    const int backingScaleFactor = 1;
#endif
    const size_t N = state.window_width * backingScaleFactor;

    const size_t job_len = xm_minull(ARRLEN(AUDIO_BUFFER), N + 1);

    // Animated sine wave
    // double now_sec = 0;
    double now_sec  = xtime_convert_ns_to_sec(xtime_now_ns());
    now_sec        *= 0.125 * 0.25; // 0.25hz

    const size_t buflen = ARRLEN(AUDIO_BUFFER);
    const float  phase  = (now_sec - (int)now_sec) * XM_TAUf;
    const float  inc    = XM_TAUf / (float)buflen * 8;
    for (unsigned i = 0; i < job_len; i++)
    {
        AUDIO_BUFFER[i] = sinf(phase + i * inc); // sin
        // AUDIO_BUFFER[i] = (((i >> 4) & 3) >> 1) ? -1 : 1; // square
    }

    sg_update_buffer(state.buf, &(sg_range){.ptr = AUDIO_BUFFER, sizeof(AUDIO_BUFFER[0]) * job_len});

    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    const fs_uniforms_t uniforms = {
        .u_view_size[0]  = state.window_width * backingScaleFactor,
        .u_view_size[1]  = state.window_height * backingScaleFactor,
        .u_stroke_width  = 2.0f,
        .u_buffer_length = ARRLEN(AUDIO_BUFFER),
    };
    sg_apply_uniforms(UB_fs_uniforms, &SG_RANGE(uniforms));

    sg_draw(0, 6, 1);
    sg_end_pass();
}