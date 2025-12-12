#include "common.h"

#include <xhl/time.h>

#include <math.h>

#include <vector>

// #include "pathkit/core/SkStroke.h"
#include "pathkit.h"

#include "program_pathkit_msaa.glsl.h"

#define OFFSCREEN_SAMPLE_COUNT (4)

static struct
{
    int window_width;
    int window_height;

    sg_image msaa_image;
    sg_view  msaa_colview;
    sg_image resolve_image;
    sg_view  resolve_colview;
    sg_view  resolve_texview;

    struct
    {
        sg_pass     pass;
        sg_pipeline pip;
        sg_bindings bind;
    } offscreen;

    struct
    {
        sg_pass_action pass_action;
        sg_pipeline    pip;
        sg_bindings    bind;
    } display;
} state;

float mapf(float v, float s1, float e1, float s2, float e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }

#define BIG_INDICES_BUFFER_CAP  ((1 << 16) - 1)
#define BIG_VERTICES_BUFFER_CAP ((BIG_INDICES_BUFFER_CAP * 2) / 3)

void setup_images()
{
    sg_image_desc img_desc = (sg_image_desc){
        .usage.color_attachment = true,
        .width                  = state.window_width,
        .height                 = state.window_height,
        .pixel_format           = SG_PIXELFORMAT_RGBA8,
        .sample_count           = OFFSCREEN_SAMPLE_COUNT,
        .label                  = "msaa-image"};
    state.msaa_image                 = sg_make_image(&img_desc);
    sg_view_desc msaa_image_viewdesc = {
        .color_attachment.image = state.msaa_image,
    };
    state.msaa_colview = sg_make_view(&msaa_image_viewdesc);

    img_desc = (sg_image_desc){
        .usage.resolve_attachment = true,
        .width                    = state.window_width,
        .height                   = state.window_height,
        .pixel_format             = SG_PIXELFORMAT_RGBA8,
        .sample_count             = 1,
        .label                    = "resolve-image",
    };
    state.resolve_image                    = sg_make_image(&img_desc);
    sg_view_desc resolve_image_colviewdesc = {
        .resolve_attachment.image = state.resolve_image,
    };
    state.resolve_colview                  = sg_make_view(&resolve_image_colviewdesc);
    sg_view_desc resolve_image_texviewdesc = {
        .texture.image = state.resolve_image,
    };
    state.resolve_texview = sg_make_view(&resolve_image_texviewdesc);

    state.offscreen.pass.attachments.colors[0]   = state.msaa_colview;
    state.offscreen.pass.attachments.resolves[0] = state.resolve_colview;
}

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    // offscreen
    {
        state.offscreen.pass.action = (sg_pass_action){
            .colors[0] = {
                .load_action  = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_DONTCARE,
                .clear_value  = {0, 0, 0, 1.0f}}};

        setup_images();

        sg_buffer_desc buffer_desc = (sg_buffer_desc){
            .size                = BIG_VERTICES_BUFFER_CAP * sizeof(float),
            .usage.stream_update = true,
            .label               = "quad-vertices"};
        state.offscreen.bind.vertex_buffers[0] = sg_make_buffer(&buffer_desc);

        sg_pipeline_desc pip_desc;
        memset(&pip_desc, 0, sizeof(pip_desc));
        pip_desc.shader                                     = sg_make_shader(pathkit_shader_desc(sg_query_backend()));
        pip_desc.sample_count                               = OFFSCREEN_SAMPLE_COUNT;
        pip_desc.depth.pixel_format                         = SG_PIXELFORMAT_NONE;
        pip_desc.layout.attrs[ATTR_pathkit_position].format = SG_VERTEXFORMAT_FLOAT2;
        pip_desc.label                                      = "quad-pipeline";
        pip_desc.colors[0].pixel_format                     = SG_PIXELFORMAT_RGBA8;
        state.offscreen.pip                                 = sg_make_pipeline(&pip_desc);
    }

    // Display
    {
        state.display.pass_action = (sg_pass_action){
            .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

        typedef struct
        {
            float   x, y;
            int16_t u, v;
        } vertex_t;

        // clang-format off
        vertex_t vertices[] = {
            {-1.0f,  1.0f, 0,     32767},
            { 1.0f,  1.0f, 32767, 32767},
            { 1.0f, -1.0f, 32767, 0},
            {-1.0f, -1.0f, 0,     0},
        };
        // an index buffer with 2 triangles
        uint16_t indices[] = {
            0, 1, 2,
            0, 2, 3,
        };
        // clang-format on
        sg_buffer_desc vbuf_desc = (sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"};
        sg_buffer_desc ibuf_desc =
            (sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"};
        sg_sampler_desc smpl_desc = (sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
        state.display.bind = (sg_bindings){
            .vertex_buffers[0] = sg_make_buffer(&vbuf_desc),
            .index_buffer      = sg_make_buffer(&ibuf_desc),
            .views[VIEW_tex]   = state.resolve_texview,
            .samplers[SMP_smp] = sg_make_sampler(&smpl_desc)};

        sg_pipeline_desc pip_desc;
        memset(&pip_desc, 0, sizeof(pip_desc));
        pip_desc.shader                                      = sg_make_shader(display_shader_desc(sg_query_backend()));
        pip_desc.index_type                                  = SG_INDEXTYPE_UINT16;
        pip_desc.layout.attrs[ATTR_display_position].format  = SG_VERTEXFORMAT_FLOAT2;
        pip_desc.layout.attrs[ATTR_display_texcoord0].format = SG_VERTEXFORMAT_SHORT2N;
        pip_desc.label                                       = "quad-pipeline";
        state.display.pip                                    = sg_make_pipeline(&pip_desc);
    }
}
void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE)
    {
        state.window_width  = e->resize.width;
        state.window_height = e->resize.height;

        sg_destroy_view(state.resolve_colview);
        sg_destroy_view(state.resolve_texview);
        sg_destroy_view(state.msaa_colview);
        sg_destroy_image(state.resolve_image);
        sg_destroy_image(state.msaa_image);

        setup_images();

        state.display.bind.views[VIEW_tex] = state.resolve_texview;
    }
    return false;
}

void program_tick()
{
    sg_begin_pass(&state.offscreen.pass);

    using namespace pk;

    SkRect             clipBounds({0, 0, (float)state.window_width, (float)state.window_height});
    static SkPath      path;
    static SkPath      line;
    std::vector<float> vertices;

    path.reset();
    line.reset();
    vertices.clear();

    line.moveTo(32, 150);
    line.quadTo(50, 50, 100, 100);
    line.lineTo(100, 200);
    line.cubicTo(20, 20, 300, 300, 300, 20);
    SkStroke stroke;
    stroke.setWidth(5.0f);
    stroke.strokePath(line, &path);

    bool   isLinear      = false;
    size_t num_triangles = path.toTriangles(0.5f, clipBounds, &vertices, &isLinear);
    SOKOL_ASSERT(vertices.size() < BIG_VERTICES_BUFFER_CAP);

    for (int i = 0; i < vertices.size(); i += 2)
    {
        float x = vertices[i];
        float y = vertices[i + 1];

        x = mapf(x, 0, state.window_width, -1.0f, 1.0f);
        y = mapf(y, 0, state.window_height, 1.0f, -1.0f);

        vertices[i]     = x;
        vertices[i + 1] = y;
    }

    if (vertices.size())
    {
        sg_range buf_range = {.ptr = vertices.data(), vertices.size() * sizeof(vertices[0])};
        sg_update_buffer(state.offscreen.bind.vertex_buffers[0], &buf_range);

        sg_apply_pipeline(state.offscreen.pip);
        sg_apply_bindings(&state.offscreen.bind);

        const fs_uniforms_t uniforms = {.col = {1., 0., 1., 1.}};
        sg_range            u_range  = SG_RANGE(uniforms);
        sg_apply_uniforms(UB_fs_uniforms, &u_range);

        sg_draw(0, num_triangles, 1);
    }
    sg_end_pass();

    // main
    sg_pass pass = (sg_pass){.action = state.display.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)};
    sg_begin_pass(&pass);
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
}