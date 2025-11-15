#include "common.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "program_msaa.h"

#define OFFSCREEN_SAMPLE_COUNT (4)

static struct
{
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
} state = {0};

void program_setup()
{
    // offscreen
    {
        sapp_desc app_desc = sapp_query_desc();

        // create a MSAA render target image, this will be rendered to
        // in the offscreen render pass
        state.msaa_image = sg_make_image(&(sg_image_desc){.usage.color_attachment = true,
                                                          .width                  = app_desc.width,
                                                          .height                 = app_desc.height,
                                                          .pixel_format           = SG_PIXELFORMAT_RGBA8,
                                                          .sample_count           = OFFSCREEN_SAMPLE_COUNT,
                                                          .label                  = "msaa-image"});

        // create a matching resolve-image where the MSAA-rendered content will
        // be resolved to at the end of the offscreen pass, and which will be
        // texture-sampled in the display pass
        state.resolve_image = sg_make_image(&(sg_image_desc){
            .usage.resolve_attachment = true,
            .width                    = app_desc.width,
            .height                   = app_desc.height,
            .pixel_format             = SG_PIXELFORMAT_RGBA8,
            .sample_count             = 1,
            .label                    = "resolve-image",
        });

        state.msaa_colview    = sg_make_view(&(sg_view_desc){
               .color_attachment.image = state.msaa_image,
        });
        state.resolve_colview = sg_make_view(&(sg_view_desc){
            .resolve_attachment.image = state.resolve_image,
        });
        state.resolve_texview = sg_make_view(&(sg_view_desc){
            .texture.image = state.resolve_image,
        });

        state.offscreen.pass.action = (sg_pass_action){
            .colors[0] =
                {.load_action  = SG_LOADACTION_CLEAR,
                 .store_action = SG_STOREACTION_DONTCARE,
                 .clear_value  = {0, 0, 0, 1.0f}},
        };
        state.offscreen.pass.attachments.colors[0]   = state.msaa_colview;
        state.offscreen.pass.attachments.resolves[0] = state.resolve_colview;

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
                sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "triangle-vertices"}),
        };

        state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .layout =
                {.attrs =
                     {[ATTR_offscreen_position].format = SG_VERTEXFORMAT_FLOAT2,
                      [ATTR_offscreen_color0].format   = SG_VERTEXFORMAT_FLOAT4}},
            .shader       = sg_make_shader(offscreen_shader_desc(sg_query_backend())),
            .sample_count = OFFSCREEN_SAMPLE_COUNT,
            .depth =
                {
                    .pixel_format = SG_PIXELFORMAT_NONE,
                },
            .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
            .label                  = "offscreen-pipeline",
        });
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
        state.display.bind = (sg_bindings){
            .vertex_buffers[0] =
                sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"}),
            .index_buffer = sg_make_buffer(
                &(sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"}),
            .views[VIEW_tex]   = state.resolve_texview,
            .samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
                .min_filter = SG_FILTER_LINEAR,
                .mag_filter = SG_FILTER_LINEAR,
            })};

        state.display.pip =
            sg_make_pipeline(&(sg_pipeline_desc){.shader     = sg_make_shader(display_shader_desc(sg_query_backend())),
                                                 .index_type = SG_INDEXTYPE_UINT16,
                                                 .layout =
                                                     {.attrs =
                                                          {[ATTR_display_position].format  = SG_VERTEXFORMAT_FLOAT2,
                                                           [ATTR_display_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
                                                 .label = "quad-pipeline"});
    }
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
        // println("Resized %d %d", e->window_width, e->window_height);
        sg_destroy_view(state.msaa_colview);
        sg_destroy_view(state.resolve_colview);
        sg_destroy_view(state.resolve_texview);
        sg_destroy_image(state.resolve_image);
        sg_destroy_image(state.msaa_image);

        state.msaa_image = sg_make_image(&(sg_image_desc){.usage.color_attachment = true,
                                                          .width                  = e->window_width,
                                                          .height                 = e->window_height,
                                                          .pixel_format           = SG_PIXELFORMAT_RGBA8,
                                                          .sample_count           = OFFSCREEN_SAMPLE_COUNT,
                                                          .label                  = "msaa-image"});

        state.resolve_image = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width                  = e->window_width,
            .height                 = e->window_height,
            .pixel_format           = SG_PIXELFORMAT_RGBA8,
            .sample_count           = 1,
            .label                  = "resolve-image",
        });

        state.msaa_colview    = sg_make_view(&(sg_view_desc){
               .color_attachment.image = state.msaa_image,
        });
        state.resolve_colview = sg_make_view(&(sg_view_desc){
            .color_attachment.image = state.resolve_image,
        });
        state.resolve_texview = sg_make_view(&(sg_view_desc){
            .texture.image = state.resolve_image,
        });

        state.display.bind.views[VIEW_tex] = state.resolve_texview;
    }
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
    sg_begin_pass(&(sg_pass){.action = state.display.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
}