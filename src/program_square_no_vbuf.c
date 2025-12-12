#include "common.h"

#include "program_square_no_vbuf.glsl.h"

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