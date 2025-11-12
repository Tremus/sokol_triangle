#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include <xhl/debug.h>
#include <xhl/vector.h>

#include "program_vertex_pull_sdf.glsl.h"

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
    int            window_width;
    int            window_height;
} state;

// TODO do scissoring

// Playing with code from here:
// https://iquilezles.org/articles/distfunctions2d/
enum
{
    SDF_TYPE_RECTANGLE_FILL,
    SDF_TYPE_RECTANGLE_STROKE,
    SDF_TYPE_CIRCLE_FILL,
    SDF_TYPE_CIRCLE_STROKE,
    // SDF_TYPE_TRIANGLE_FILL,
    // SDF_TYPE_TRIANGLE_STROKE,
    // SDF_TYPE_PIE_FILL,
    // SDF_TYPE_PIE_STROKE,
    // SDF_TYPE_ARC_ROUND_FILL,
    // SDF_TYPE_ARC_ROUND_STROKE,
    // SDF_TYPE_ARC_BUTT_FILL,
    // SDF_TYPE_ARC_BUTT_STROKE,
    // SDF_TYPE_HEART_FILL,
    // SDF_TYPE_HEART_STROKE,
    // SDF_TYPE_PENTAGRAM_FILL,
    // SDF_TYPE_PENTAGRAM_STROKE,
};

// clang-format off
// Compressed squares
static const myvertex_t vertices[] ={
    {
        .topleft = {10, 10},
        .bottomright = {80, 80},
        .colour1 = 0xffff00ff,
        .type = SDF_TYPE_CIRCLE_FILL,
        .feather = 0.05,
    },
    {
        .topleft = {10, 90},
        .bottomright = {80, 160},
        .colour1 = 0xff00ffff,
        .type = SDF_TYPE_CIRCLE_STROKE,
        .stroke_width = 4,
        .feather = 0.05,
    },
    {
        .topleft = {110, 10},
        .bottomright = {210, 80},
        .colour1 = 0x00ffffff,
        .type = SDF_TYPE_RECTANGLE_FILL,
        .feather = 0.04,
    },
    {
        .topleft = {110, 90},
        .bottomright = {410, 160},
        .colour1 = 0x00ff00ff,
        .type = SDF_TYPE_RECTANGLE_STROKE,
        .stroke_width = 20,
        .feather = 0.05,
    },
};
// clang-format on

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    for (int i = 0; i < ARRLEN(vertices); i++)
    {
        const myvertex_t* v = vertices + i;
        xassert(v->topleft[0] < v->bottomright[0]);
        xassert(v->topleft[1] < v->bottomright[1]);
    }

    sg_buffer sbuf   = sg_make_buffer(&(sg_buffer_desc){
          .usage.storage_buffer = true,
          .data                 = SG_RANGE(vertices),
          .label                = "vertices",
    });
    sg_view   sbview = sg_make_view(&(sg_view_desc){
          .storage_buffer = sbuf,
    });

    // state.bind.storage_buffers[SBUF_ssbo] = sbuf;
    state.bind.views[VIEW_ssbo] = sbview;

    // Note we don't pass any index type or vertex layout
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(vertexpull_shader_desc(sg_query_backend())),
        .colors[0] =
            {.write_mask = SG_COLORMASK_RGBA,
             .blend =
                 {
                     .enabled          = true,
                     .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
                     .src_factor_alpha = SG_BLENDFACTOR_ONE,
                     .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                     .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                 }},
        .label = "pipeline"});

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