#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_sdf_shapes.h"

// application state
static struct
{
    unsigned width;
    unsigned height;

    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state = {0};

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    // clang-format off
    vertex_t vertices[] = {
        // xy          uv
        {-1.0f,  1.0f, -32767, -32767},
         {1.0f,  1.0f,  32767, -32767},
         {1.0f, -1.0f,  32767,  32767},
        {-1.0f, -1.0f, -32767,  32767},
    };
    // an index buffer with 2 triangles
    uint16_t indices[] = {
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on
    state.bind.vertex_buffers[0] =
        sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});

    state.bind.index_buffer = sg_make_buffer(
        &(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(indices), .label = "quad-indices"});

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = sg_make_shader(sdf_sahpes_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_vs_texcoord0].format   = SG_VERTEXFORMAT_SHORT2N}},
        .label = "quad-pipeline"});

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
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
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    uint32_t largest_dimension = state.width > state.height ? state.width : state.height;
    fs_uniforms_sdf_shapes_t uniforms = {
        .feather = 4.0f / (float)largest_dimension
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_uniforms_sdf_shapes, &SG_RANGE(uniforms));

    sg_draw(0, 6, 1);
    sg_end_pass();
}