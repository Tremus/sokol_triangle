#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_gaussian_blur.h"

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

typedef struct RenderTarget
{
    sg_image       img;
    sg_attachments att;
    int            width;
    int            height;
} RenderTarget;

RenderTarget make_render_target(int width, int height)
{
    RenderTarget rt = {0};

    sg_image img_colour = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width                   = width,
        .height                  = height,
        .pixel_format            = SG_PIXELFORMAT_BGRA8,
        .sample_count            = 1,
        .label                   = "RenderTarget image"});

    rt.img = img_colour;
    rt.att = sg_make_attachments(&(sg_attachments_desc){.colors[0].image = rt.img, .label = "RenderTarget attachment"});
    rt.width  = width;
    rt.height = height;

    return rt;
}

// application state
static struct
{
    uint32_t width;
    uint32_t height;
    sg_image offscreen_img;

    RenderTarget targets_blur[2];

    sg_buffer vertices_triangle;
    sg_buffer vertices_texquad;
    sg_buffer indices_quad;

    sg_pipeline pip_triangle;
    sg_pipeline pip_blur;
    sg_pipeline pip_texquad;

    sg_sampler smp_linear;
    sg_sampler smp_nearest;
} state = {0};

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    state.targets_blur[0] = make_render_target(state.width, state.height);
    state.targets_blur[1] = make_render_target(state.width, state.height);

    // clang-format off
    // 2 Triangles, forming a quad
    static const float vertices_triangle[] = {
        // positions      // colors
         0.0f,  0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };
    static const vertex_t texquad_vertices[] = {
        {-1.0f,  1.0f, 0,     32767},
        { 1.0f,  1.0f, 32767, 32767},
        { 1.0f, -1.0f, 32767, 0},
        {-1.0f, -1.0f, 0,     0},
    };
    static const uint16_t quad_indices[] = {
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on
    state.vertices_triangle = sg_make_buffer(
        &(sg_buffer_desc){.usage.vertex_buffer = true, .usage.immutable = true, .data = SG_RANGE(vertices_triangle)});
    state.vertices_texquad = sg_make_buffer(
        &(sg_buffer_desc){.usage.vertex_buffer = true, .usage.immutable = true, .data = SG_RANGE(texquad_vertices)});
    state.indices_quad = sg_make_buffer(
        &(sg_buffer_desc){.usage.index_buffer = true, .usage.immutable = true, .data = SG_RANGE(quad_indices)});

    state.pip_triangle = sg_make_pipeline(&(sg_pipeline_desc){
        .layout =
            {.attrs =
                 {[ATTR_triangle_position].format = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_triangle_colour0].format  = SG_VERTEXFORMAT_FLOAT4}},
        .shader                 = sg_make_shader(triangle_shader_desc(sg_query_backend())),
        .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
        .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});

    state.pip_blur    = sg_make_pipeline(&(sg_pipeline_desc){
           .shader     = sg_make_shader(blur_shader_desc(sg_query_backend())),
           .depth      = {.pixel_format = SG_PIXELFORMAT_NONE},
           .index_type = SG_INDEXTYPE_UINT16,
           .layout     = {
                   .attrs = {
                [ATTR_blur_position].format  = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_blur_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}}});
    state.pip_texquad = sg_make_pipeline(&(sg_pipeline_desc){
        .shader                 = sg_make_shader(texquad_shader_desc(sg_query_backend())),
        .index_type             = SG_INDEXTYPE_UINT16,
        .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8,
        .layout                 = {
                            .attrs = {
                [ATTR_texquad_position].format  = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_texquad_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}}});

    state.smp_linear  = sg_make_sampler(&(sg_sampler_desc){
         .min_filter = SG_FILTER_LINEAR,
         .mag_filter = SG_FILTER_LINEAR,
    });
    state.smp_nearest = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
    }
}

void program_tick()
{
    // Draw triangle to offscreen texture
    sg_begin_pass(&(sg_pass){
        .action      = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .attachments = state.targets_blur[0].att,
    });
    sg_apply_pipeline(state.pip_triangle);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vertices_triangle,
    });
    sg_draw(0, 3, 1);
    sg_end_pass();

    // Do blur loop
    // const int blur_distance_pixels = 1;
    const int blur_distance_pixels = 64;
    fs_blur_t uniforms[2]          = {0};
    uniforms[0].u_pixel_size[0]    = 1.0f / state.width;
    uniforms[1].u_pixel_size[1]    = 1.0f / state.height;

    for (int i = 0; i < blur_distance_pixels; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            sg_color_attachment_action att_action = {0};

            sg_image       read_img  = state.targets_blur[j].img;
            sg_attachments write_att = state.targets_blur[(j + 1) & 1].att;

            sg_begin_pass(
                &(sg_pass){.action = {.colors[0] = {.load_action = SG_LOADACTION_DONTCARE}}, .attachments = write_att});
            sg_apply_pipeline(state.pip_blur);
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = state.vertices_texquad,
                .index_buffer      = state.indices_quad,
                .images[0]         = read_img,
                .samplers[0]       = state.smp_nearest,
            });
            sg_apply_uniforms(UB_fs_blur, &SG_RANGE(uniforms[j]));
            sg_draw(0, 6, 1);
            sg_end_pass();
        }
    }

    // Draw to swapchain
    sg_begin_pass(&(sg_pass){
        .action    = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip_texquad);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vertices_texquad,
        .index_buffer      = state.indices_quad,
        .images[0]         = state.targets_blur[0].img,
        .samplers[0]       = state.smp_linear,
    });
    sg_draw(0, 6, 1);
    sg_end_pass();
}