#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_gaussian_blur.h"

// application state
static struct
{
    uint32_t width;
    uint32_t height;
    sg_image offscreen_img;

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
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;
    // offscreen
    {
        sapp_desc app_desc  = sapp_query_desc();
        state.offscreen_img = sg_make_image(&(sg_image_desc){
            .usage.render_attachment = true,
            .width                   = state.width,
            .height                  = state.height,
            .pixel_format            = SG_PIXELFORMAT_RGBA8,
            .label                   = "offscreen-image"});

        state.offscreen.pass = (sg_pass){
            .attachments = sg_make_attachments(
                &(sg_attachments_desc){.colors[0].image = state.offscreen_img, .label = "offscreen-attachment"}),
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
        state.offscreen.bind = (sg_bindings){
            .vertex_buffers[0] =
                sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "offscreen-vertices"})};

        state.offscreen.pip = sg_make_pipeline(
            &(sg_pipeline_desc){// if the vertex layout doesn't have gaps, don't need to provide strides and offsets
                                .layout =
                                    {.attrs =
                                         {[ATTR_offscreen_position].format = SG_VERTEXFORMAT_FLOAT2,
                                          [ATTR_offscreen_colour0].format  = SG_VERTEXFORMAT_FLOAT4}},
                                .shader = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
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
            float   x, y;
            int16_t u, v;
        } vertex_t;

        // a vertex buffer
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
        state.display.bind.vertex_buffers[0] =
            sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});

        state.display.bind.index_buffer = state.display.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
            .usage.index_buffer = true,
            .usage.immutable    = true,
            .data               = SG_RANGE(indices),
            .label              = "quad-indices"});

        // a pipeline state object
        state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader     = sg_make_shader(display_shader_desc(sg_query_backend())),
            .index_type = SG_INDEXTYPE_UINT16,
            .layout =
                {.attrs =
                     {[ATTR_display_position].format  = SG_VERTEXFORMAT_FLOAT2,
                      [ATTR_display_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
            .label = "quad-pipeline"});

        state.display.bind.images[SMP_smp] = state.offscreen_img;

        // a sampler object
        state.display.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        });
    }
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
        // print("Resized %d %d", e->window_width, e->window_height);
        sg_destroy_attachments(state.offscreen.pass.attachments);
        sg_destroy_image(state.offscreen_img);

        state.offscreen_img              = sg_make_image(&(sg_image_desc){
                         .usage.render_attachment = true,
                         .width                   = e->window_width,
                         .height                  = e->window_height,
                         .pixel_format            = SG_PIXELFORMAT_RGBA8,
                         .label                   = "offscreen-image"});
        state.offscreen.pass.attachments = sg_make_attachments(
            &(sg_attachments_desc){.colors[0].image = state.offscreen_img, .label = "offscreen-attachment"});
        state.display.bind.images[IMG_tex] = state.offscreen_img;

        state.width  = e->window_width;
        state.height = e->window_height;
    }
}

void program_tick()
{
    // print("image: %u", state.offscreen_img.id);
    // offscreen
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&state.offscreen.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();

    // main
    sg_begin_pass(&(sg_pass){.action = state.display.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);

    // const fs_fxaa_t fs_fxaa_params = {
    //     .tex_size        = {APP_WIDTH, APP_HEIGHT},
    //     .fxaa_span_max   = 8.0f,
    //     .fxaa_reduce_min = 1.0f / 128.0f,
    //     .fxaa_reduce_mul = 1.0f / 8.0f,
    // };
    // sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_fxaa, &SG_RANGE(fs_fxaa_params));
    fs_blur_t uniforms = {.u_resolution = {state.width, state.height}};
    sg_apply_uniforms(UB_fs_blur, &SG_RANGE(uniforms));
    sg_draw(0, 6, 1);
    sg_end_pass();
}