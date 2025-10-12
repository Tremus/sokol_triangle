#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_square_no_vbuf.h"

// application state
static struct
{
    sg_pipeline pip;
    sg_bindings bind;
    unsigned    width;
    unsigned    height;
} state;

void program_setup()
{
    sg_shader shd = sg_make_shader(quad_novbuf_shader_desc(sg_query_backend()));
    state.pip     = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = shd, .label = "quad-pipeline"});

    state.width = APP_WIDTH;
    state.height = APP_HEIGHT;
}

void program_event(const sapp_event* event)
{
    if (event->type == SAPP_EVENTTYPE_RESIZED)
    {
        state.width  = event->window_width;
        state.height = event->window_height;
    }
}

void program_tick()
{
    sg_pass_action pass_action = {
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
    sg_begin_pass(&(sg_pass){.action = pass_action, .swapchain = sglue_swapchain()});

    sg_apply_pipeline(state.pip);

    unsigned half_width     = state.width / 2;
    unsigned quarter_width  = state.width / 4;
    unsigned half_height    = state.height / 2;
    unsigned quarter_height = state.height / 4;

    vs_position_t uniforms = {
        .topleft     = {quarter_width, quarter_height},
        .bottomright = {half_width + quarter_width, half_height + quarter_height},
        .size        = {state.width, state.height}};
    sg_apply_uniforms(UB_vs_position, &SG_RANGE(uniforms));
    sg_draw(0, 6, 1);
    sg_end_pass();
}