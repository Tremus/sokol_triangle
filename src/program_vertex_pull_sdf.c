#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include <xhl/vector.h>

#include "program_vertex_pull_sdf.h"

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
    int            window_width;
    int            window_height;
} state;

// clang-format off
// Compressed squares
static const myvertex_t vertices[] ={
    {
        .topleft = {10, 10},
        .bottomright = {80, 80},
        .colour1 = 0xffff00ff,
        .type = 0,
        // .texid = 0,
    },
    {
        .topleft = {10, 110},
        .bottomright = {80, 180},
        .colour1 = 0xff00ffff,
        .type = 1,
        .stroke_width = 4,
        // .texid = 0,
    }
};
// clang-format on

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    sg_buffer sbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .data                 = SG_RANGE(vertices),
        .label                = "vertices",
    });

    state.bind.storage_buffers[SBUF_ssbo] = sbuf;

    // Note we don't pass any index type or vertex layout
    state.pip = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = sg_make_shader(vertexpull_shader_desc(sg_query_backend())), .label = "pipeline"});

    // a pass action to clear framebuffer to black
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
        state.window_width  = e->window_width;
        state.window_height = e->window_height;
    }
}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    vs_params_t uniforms = {.size = {state.window_width, state.window_height}};

    sg_apply_uniforms(UB_vs_params, &SG_RANGE(uniforms));

    int N = ARRLEN(vertices);
    sg_draw(0, N * 6, 1);
    sg_end_pass();
}