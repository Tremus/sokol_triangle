#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_offscreen_render.h"

// application state
static struct
{
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
    // offscreen
    {
        sapp_desc app_desc  = sapp_query_desc();
        state.offscreen_img = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .width         = app_desc.width,
            .height        = app_desc.height,
            .pixel_format  = SG_PIXELFORMAT_RGBA8,
            .label         = "offscreen-image"});

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

        // create shader from code-generated sg_shader_desc
        sg_shader shd = sg_make_shader(offscreen_shader_desc(sg_query_backend()));

        // create a pipeline object (default render states are fine for triangle)
        state.offscreen.pip = sg_make_pipeline(
            &(sg_pipeline_desc){// if the vertex layout doesn't have gaps, don't need to provide strides and offsets
                                .layout =
                                    {.attrs =
                                         {[ATTR_offscreen_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                                          [ATTR_offscreen_vs_color0].format   = SG_VERTEXFORMAT_FLOAT4}},
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

        state.display.bind.index_buffer = state.display.bind.index_buffer = sg_make_buffer(
            &(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(indices), .label = "quad-indices"});

        // a shader (use separate shader sources here
        sg_shader shd = sg_make_shader(display_shader_desc(sg_query_backend()));

        // a pipeline state object
        state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader     = shd,
            .index_type = SG_INDEXTYPE_UINT16,
            .layout =
                {.attrs =
                     {[ATTR_display_vs_position].format  = SG_VERTEXFORMAT_FLOAT2,
                      [ATTR_display_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
            .label = "quad-pipeline"});

        state.display.bind.fs.images[SLOT_tex] = state.offscreen_img;

        // a sampler object
        state.display.bind.fs.samplers[SLOT_smp] = sg_make_sampler(&(sg_sampler_desc){
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
                         .render_target = true,
                         .width         = e->window_width,
                         .height        = e->window_height,
                         .pixel_format  = SG_PIXELFORMAT_RGBA8,
                         .label         = "offscreen-image"});
        state.offscreen.pass.attachments = sg_make_attachments(
            &(sg_attachments_desc){.colors[0].image = state.offscreen_img, .label = "offscreen-attachment"});
        state.display.bind.fs.images[SLOT_tex] = state.offscreen_img;
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
    sg_draw(0, 6, 1);
    sg_end_pass();
}