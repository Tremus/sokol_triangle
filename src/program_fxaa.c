#include "common.h"

#include "program_fxaa.h"

// application state
static struct
{
    sg_image offscreen_img;
    sg_view  offscreen_img_colview;
    sg_view  offscreen_img_texview;
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
} state = {0};

void program_setup()
{
    // offscreen
    {
        sg_image_desc img_desc = {
            .usage.color_attachment = true,
            .width                  = APP_WIDTH,
            .height                 = APP_HEIGHT,
            .pixel_format           = SG_PIXELFORMAT_RGBA8,
            .label                  = "offscreen-image"};
        state.offscreen_img         = sg_make_image(&img_desc);
        state.offscreen_img_colview = sg_make_view(&(sg_view_desc){.color_attachment = state.offscreen_img});
        state.offscreen_img_texview = sg_make_view(&(sg_view_desc){.texture.image = state.offscreen_img});

        state.offscreen.pass = (sg_pass){
            .attachments.colors[0] = state.offscreen_img_colview,
            .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .label  = "offscreen-pass"};

        // a vertex buffer with 3 vertices
        // clang-format off
        float vertices[] = {
            // positions      // colors
             0.0f,  0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,     0.0f, 0.0f, 1.0f, 1.0f
        };
        // clang-format on
        state.offscreen.bind.vertex_buffers[0] =
            sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "offscreen-vertices"});

        // create shader from code-generated sg_shader_desc
        sg_shader shd = sg_make_shader(offscreen_shader_desc(sg_query_backend()));

        // create a pipeline object (default render states are fine for triangle)
        state.offscreen.pip = sg_make_pipeline(
            &(sg_pipeline_desc){// if the vertex layout doesn't have gaps, don't need to provide strides and offsets
                                .layout =
                                    {.attrs =
                                         {[ATTR_offscreen_position].format = SG_VERTEXFORMAT_FLOAT2,
                                          [ATTR_offscreen_color0].format   = SG_VERTEXFORMAT_FLOAT4}},
                                .shader = shd,
                                .depth =
                                    {
                                        .pixel_format = SG_PIXELFORMAT_NONE,
                                    },
                                .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
                                .label                  = "offscreen-pipeline"});
    }

    // display
    {
        // default pass action
        state.display.pass_action = (sg_pass_action){
            .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

        typedef struct
        {
            float x, y;
            float u, v;
        } vertex_t;

        // a vertex buffer
        // clang-format off
        vertex_t vertices[] = {
            {-1.0f,  1.0f, 0, 1},
            { 1.0f,  1.0f, 1, 1},
            { 1.0f, -1.0f, 1, 0},
            {-1.0f, -1.0f, 0, 0},
        };
        // an index buffer with 2 triangles
        uint16_t indices[] = {
            0, 1, 2,
            0, 2, 3,
        };
        // clang-format on
        state.display.bind.vertex_buffers[0] =
            sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});

        state.display.bind.index_buffer = state.display.bind.index_buffer = sg_make_buffer(
            &(sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"});

        // a shader (use separate shader sources here
        sg_shader shd = sg_make_shader(fxaa_shader_desc(sg_query_backend()));

        // a pipeline state object
        state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader     = shd,
            .index_type = SG_INDEXTYPE_UINT16,
            .layout =
                {.attrs =
                     {[ATTR_fxaa_position].format  = SG_VERTEXFORMAT_FLOAT2,
                      [ATTR_fxaa_texcoord0].format = SG_VERTEXFORMAT_FLOAT2}},
            .label = "quad-pipeline"});

        state.display.bind.views[VIEW_tex] = state.offscreen_img_texview;

        // a sampler object
        state.display.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        });
    }
}
void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE)
    {
        // println("Resized %d %d", e->resize.width, e->resize.height);
        sg_destroy_view(state.offscreen_img_colview);
        sg_destroy_image(state.offscreen_img);

        state.offscreen_img         = sg_make_image(&(sg_image_desc){
                    .usage.color_attachment = true,
                    .width                  = e->resize.width,
                    .height                 = e->resize.height,
                    .pixel_format           = SG_PIXELFORMAT_RGBA8,
                    .label                  = "offscreen-image"});
        state.offscreen_img_colview = sg_make_view(&(sg_view_desc){.color_attachment = state.offscreen_img});
        state.offscreen_img_texview = sg_make_view(&(sg_view_desc){.texture.image = state.offscreen_img});
        state.offscreen.pass.attachments.colors[0] = state.offscreen_img_colview;
        state.display.bind.views[VIEW_tex]         = state.offscreen_img_texview;
    }
    return false;
}

void program_tick()
{
    // offscreen
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // main
    sg_begin_pass(&(sg_pass){.action = state.display.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);

    // Note: These magic numbers were taken from this tutorial https://www.youtube.com/watch?v=Z9bYzpwVINA
    // They seem to be okay on edges, but the tips of the triangles corners are sometimes clipped
    // It may be possible to play with the values below and find to find a better configuration
    const fs_fxaa_t fs_fxaa_params = {
        .tex_size        = {APP_WIDTH, APP_HEIGHT},
        .fxaa_span_max   = 8.0f,
        .fxaa_reduce_min = 1.0f / 128.0f,
        .fxaa_reduce_mul = 1.0f / 8.0f,
    };

    sg_apply_uniforms(UB_fs_fxaa, &SG_RANGE(fs_fxaa_params));

    sg_draw(0, 6, 1);
    sg_end_pass();
}