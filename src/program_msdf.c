#include "common.h"

#include "stb_image.h"

#include "program_msdf.glsl.h"

#include <stdio.h>
#include <xhl/debug.h>
#include <xhl/files.h>

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

typedef struct XFile
{
    void*  data;
    size_t size;
} XFile;
static XFile read_file(const char* path)
{
    XFile file = {0};
    xfiles_read(path, &file.data, &file.size);
    return file;
}

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
        &(sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"});

    // a shader (use separate shader sources here
    sg_shader shd = sg_make_shader(msdf_shader_desc(sg_query_backend()));

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_msdf_position].format  = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_msdf_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
        .label = "quad-pipeline"});

    // default pass action
    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

    // a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
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
    XFile file = read_file(path_buf);

    stbi_set_flip_vertically_on_load(1);
    int            comp = 0;
    unsigned char* data = stbi_load_from_memory(file.data, file.size, &tex_width, &tex_height, &comp, STBI_rgb_alpha);
    println("img size %dx%d", tex_width, tex_height);
    xassert(comp == STBI_rgb_alpha);
    xassert(data);

    sg_image img               = sg_make_image(&(sg_image_desc){
                      .width              = tex_width,
                      .height             = tex_height,
                      .pixel_format       = SG_PIXELFORMAT_RGBA8,
                      .data.mip_levels[0] = {
                          .ptr  = data,
                          .size = comp * tex_width * tex_height,
        }});
    state.bind.views[VIEW_tex] = sg_make_view(&(sg_view_desc){.texture.image = img});
}
void program_shutdown() {}

bool program_event(const PWEvent* event) { return false; }

void program_tick()
{
    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    const fs_uniforms_t uniforms = {.texSize = {tex_width, tex_height}};
    sg_range            u_range  = SG_RANGE(uniforms);
    sg_apply_uniforms(UB_fs_uniforms, &u_range);
    sg_draw(0, 6, 1);
    sg_end_pass();
}