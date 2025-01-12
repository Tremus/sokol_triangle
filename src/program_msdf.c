#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "stb_image.h"

#include "program_msdf.h"

#include <xhl/debug.h>

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

int tex_width = 0, tex_height = 0;

void program_setup()
{
    // a vertex buffer
    // clang-format off
    vertex_t vertices[] = {
        // xy          uv
        {-1.0f,  1.0f, 0,     32767},
         {1.0f,  1.0f, 32767, 32767},
         {1.0f, -1.0f, 32767, 0},
        {-1.0f, -1.0f, 0,     0},
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
    sg_shader shd = sg_make_shader(msdf_shader_desc(sg_query_backend()));

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

    char path_buf[1024];
    snprintf(path_buf, sizeof(path_buf), "%s/msdf_arial_A.png", SRC_DIR);
    char* c = path_buf;
#ifdef _WIN32
    while (*c != 0)
    {
        if (*c == '/')
            *c = '\\';
        c++;
    }
#endif
    stbi_set_flip_vertically_on_load(1);
    int            comp = 0;
    unsigned char* data = stbi_load(path_buf, &tex_width, &tex_height, &comp, STBI_rgb_alpha);
    print("img size %dx%d", tex_width, tex_height);
    xassert(comp == STBI_rgb_alpha);
    xassert(data);

    sg_init_image(
        state.bind.fs.images[SLOT_tex],
        &(sg_image_desc){
            .width               = tex_width,
            .height              = tex_height,
            .pixel_format        = SG_PIXELFORMAT_RGBA8,
            .data.subimage[0][0] = {
                .ptr  = data,
                .size = comp * tex_width * tex_height,
            }});
}

void program_event(const sapp_event* event) {}

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    const fs_uniforms_t uniforms = {.texSize = {tex_width, tex_height}};
    sg_range            u_range  = SG_RANGE(uniforms);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_uniforms, &u_range);
    sg_draw(0, 6, 1);
    sg_end_pass();
}