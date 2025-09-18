#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_hello_quad.h"

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

void program_setup()
{
    // a vertex buffer
    // clang-format off
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };
    // an index buffer with 2 triangles
    uint16_t indices[] = {
        // 0, 1, 2,
        // 0, 2, 3,
        // 0, 1, 2,
        // 0, 1, 2,
        0, 1, 2,
        1, 2, 3,
    };
    // clang-format on
    state.bind.vertex_buffers[0] =
        sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});

    state.bind.index_buffer = sg_make_buffer(
        &(sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"});

    // a shader (use separate shader sources here
    sg_shader shd = sg_make_shader(hello_quad_shader_desc(sg_query_backend()));

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_hello_quad_position].format = SG_VERTEXFORMAT_FLOAT3,
                  [ATTR_hello_quad_color0].format   = SG_VERTEXFORMAT_FLOAT4}},
        .label = "quad-pipeline"});

    // default pass action
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

void program_event(const sapp_event* event) {}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
}