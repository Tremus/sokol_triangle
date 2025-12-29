#include "common.h"

#include "program_sdf_line_segment.glsl.h"

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
    sg_shader shd = sg_make_shader(minimal_shader_desc(sg_query_backend()));
    state.pip     = sg_make_pipeline(&(sg_pipeline_desc){.shader = shd, .label = "quad-pipeline"});

    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;
}
void program_shutdown() {}

bool program_event(const PWEvent* event)
{
    if (event->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width  = event->resize.width;
        state.height = event->resize.height;
    }
    return false;
}

void program_tick()
{
    sg_pass_action pass_action = {
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
    sg_begin_pass(&(sg_pass){.action = pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});

    sg_apply_pipeline(state.pip);

    vs_position_t uniforms = {
        .u_topleft      = {0, 0},
        .u_bottomright  = {state.width, state.height},
        .u_size         = {state.width, state.height},
        .u_stroke_width = 2,
    };
    sg_apply_uniforms(UB_vs_position, &SG_RANGE(uniforms));
    sg_draw(0, 6, 1);
    sg_end_pass();
}