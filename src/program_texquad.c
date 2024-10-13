#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_texquad.h"

// application state
static struct
{
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

void program_setup()
{
    // a vertex buffer
    // clang-format off
    vertex_t vertices[] = {
        // xy          uv
        {-0.5f,  0.5f, 0,     32767},
         {0.5f,  0.5f, 32767, 32767},
         {0.5f, -0.5f, 32767, 0},
        {-0.5f, -0.5f, 0,     0},
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

    // a shader (use separate shader sources here
    sg_shader shd = sg_make_shader(texquad_shader_desc(sg_query_backend()));

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_vs_position].format  = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
        .label = "quad-pipeline"});

    // default pass action
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

    state.bind.fs.images[SLOT_tex] = sg_alloc_image();

    // a sampler object
    state.bind.fs.samplers[SLOT_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    union RGBA8
    {
        struct
        {
            uint8_t r, g, b, a;
        };
        uint32_t abgr;
    };
    static union RGBA8 img_buf[APP_WIDTH * APP_HEIGHT];
    for (int i = 0; i < ARRLEN(img_buf); i++)
    {
        // img_buf[i].abgr = 0xffffffff; // white image
        // img_buf[i].abgr = 0xff00ffff; // yellow image
        img_buf[i].abgr = 0xffff00ff; // fuscia image
        // img_buf[i].abgr = 0xffffff00; // cyan image
    }

    sg_init_image(
        state.bind.fs.images[SLOT_tex],
        &(sg_image_desc){
            .width               = APP_WIDTH,
            .height              = APP_HEIGHT,
            .pixel_format        = SG_PIXELFORMAT_RGBA8,
            .data.subimage[0][0] = {
                .ptr  = img_buf,
                .size = (size_t)(APP_WIDTH * APP_HEIGHT * sizeof(img_buf[0])),
            }});
}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
}