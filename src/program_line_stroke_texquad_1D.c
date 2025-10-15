#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_line_stroke_texquad_1D.h"

#include <math.h>

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

static float SINE_BUFFER[1024];

void program_setup()
{
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
        .size                 = sizeof(SINE_BUFFER),
        .label                = "sine-buffer",
    });

    state.bind.views[VIEW_storage_buffer] = sg_make_view(&(sg_view_desc){.storage_buffer = state.buf});
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
        state.window_width  = e->window_width;
        state.window_height = e->window_height;
    }
    else if (
        e->type == SAPP_EVENTTYPE_MOUSE_MOVE || e->type == SAPP_EVENTTYPE_MOUSE_ENTER ||
        e->type == SAPP_EVENTTYPE_MOUSE_LEAVE)
    {
        state.mouse_xy[0] = e->mouse_x / (float)state.window_width;
        state.mouse_xy[1] = e->mouse_y / (float)state.window_height;
    }
}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});

    // Animated sine wave
    static float START_PHASE = 0.0f;

    const size_t buflen = ARRLEN(SINE_BUFFER);
    float        phase  = START_PHASE;
    const float  inc    = 1.0f / (float)buflen;
    for (int i = 0; i < ARRLEN(SINE_BUFFER); i++)
    {
        SINE_BUFFER[i]  = sinf(phase * 3.14159f * 2);
        phase          += inc;
    }

    START_PHASE += 0.5f / 60.0f;
    START_PHASE -= (int)START_PHASE;

    sg_update_buffer(state.buf, &(sg_range){.ptr = SINE_BUFFER, sizeof(SINE_BUFFER)});

    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    const size_t        buf_maxidx = ARRLEN(SINE_BUFFER) - 1;
    const fs_uniforms_t uniforms   = {
          .mouse_xy[0]         = state.mouse_xy[0],
          .mouse_xy[1]         = state.mouse_xy[1],
          .buffer_max_idx      = buf_maxidx,
          .quad_height_max_idx = state.window_height - 1};
    sg_apply_uniforms(UB_fs_uniforms, &SG_RANGE(uniforms));

    sg_draw(0, 6, 1);
    sg_end_pass();
}