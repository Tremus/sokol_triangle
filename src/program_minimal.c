#include "common.h"

#include "program_minimal.glsl.h"

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

void program_setup()
{
    // a vertex buffer with 3 vertices
    // clang-format off
    static float vertices[] = {
        // positions
         0.0f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f,
    };
    // clang-format on
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "triangle"});

    // create shader from code-generated sg_shader_desc
    sg_shader shd = sg_make_shader(minimal_shader_desc(sg_query_backend()));

    // create a pipeline object (default render states are fine for triangle)
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
        .layout = {.attrs = {[ATTR_minimal_position].format = SG_VERTEXFORMAT_FLOAT2}},
        .label  = "triangle-pipeline"});

    // a pass action to clear framebuffer to black
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}
void program_shutdown() {}

bool program_event(const PWEvent* event) { return false; }

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
}