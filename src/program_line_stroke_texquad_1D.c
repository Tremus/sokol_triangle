#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_line_stroke_texquad_1D.h"

#include <math.h>

static struct
{
    int window_width;
    int window_height;

    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

static float SINE_BUFFER[512];

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    // clang-format off
    vertex_t vertices[] = {
        // xy          uv
        {-1.0f,  1.0f, 0,     32767},
        { 1.0f,  1.0f, 32767, 32767},
        { 1.0f, -1.0f, 32767, 0},
        {-1.0f, -1.0f, 0,     0},
    };
    uint16_t indices[] = {
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on

    state.bind.vertex_buffers[0] =
        sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});

    state.bind.index_buffer = sg_make_buffer(
        &(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(indices), .label = "quad-indices"});

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = sg_make_shader(line_stroke_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_vs_position].format  = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
        .label = "quad-pipeline"});

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

    const size_t buflen = ARRLEN(SINE_BUFFER);
    float        phase  = 0;
    const float  inc    = 1.0f / (float)buflen;
    for (int i = 0; i < ARRLEN(SINE_BUFFER); i++)
    {
        SINE_BUFFER[i]  = sinf(phase * 3.14159f * 2);
        phase          += inc;
    }

    state.bind.fs.storage_buffers[SLOT_storage_buffer] = sg_make_buffer(&(sg_buffer_desc){
        .type  = SG_BUFFERTYPE_STORAGEBUFFER,
        .data  = SG_RANGE(SINE_BUFFER),
        .label = "sine-buffer",
    });
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

    const size_t        buf_maxidx = ARRLEN(SINE_BUFFER) - 1;
    const fs_uniforms_t uniforms   = {.buffer_max_idx = buf_maxidx, .quad_height_max_idx = state.window_height - 1};
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_uniforms, &SG_RANGE(uniforms));

    sg_draw(0, 6, 1);
    sg_end_pass();
}