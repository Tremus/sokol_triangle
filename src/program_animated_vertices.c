#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_animated_vertices.h"

static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;

} state;

// a vertex buffer with 3 vertices
// clang-format off
static float vertices[] = {
    // positions
     0.0f,  0.5f,
     0.5f, -0.5f,
    -0.5f, -0.5f,
};
// clang-format on

void program_setup()
{
    state.bind.vertex_buffers[0] =
        sg_make_buffer(&(sg_buffer_desc){.size = sizeof(vertices), .usage = SG_USAGE_STREAM, .label = "triangle"});

    sg_shader shd = sg_make_shader(animated_vertices_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {.attrs = {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2}},
        .label  = "pipeline"});

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

void program_event(const sapp_event* event) {}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});

    static float x_pos = 0;
    static float x_inc = 2.0 / 60.0f;
    sg_update_buffer(state.bind.vertex_buffers[0], &SG_RANGE(vertices));
    // Animate the triangles tip x-axis
    x_pos += x_inc;
    if (x_pos >= 0.25f)
    {
        x_pos = 0.25f;
        x_inc = -x_inc;
    }
    if (x_pos <= -0.25f)
    {
        x_pos = -0.25f;
        x_inc = -x_inc;
    }
    vertices[0] = x_pos;

    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
}