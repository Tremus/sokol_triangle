#include "common.h"

#include "program_offscreen_render.glsl.h"

// application state
static struct
{
    bool     resized;
    sg_image offscreen_img;
    sg_view  offscreen_img_colview;
    sg_view  offscreen_img_texview;

    struct
    {
        sg_pipeline pip;
        sg_buffer   vbo;
    } offscreen;

    struct
    {
        sg_pipeline pip;

        sg_buffer  vbo;
        sg_buffer  ibo;
        sg_sampler smp;
    } display;

    int width, height;
} state = {0};

void program_setup()
{
    state.width   = APP_WIDTH;
    state.height  = APP_HEIGHT;
    state.resized = true;
    // offscreen
    {
        // a vertex buffer with 3 vertices
        // clang-format off
        float vertices[] = {
            // positions      // colors
             0.0f,  0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,     0.0f, 0.0f, 1.0f, 1.0f
        };
        // clang-format on
        state.offscreen.vbo =
            sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "offscreen-vertices"});

        // create shader from code-generated sg_shader_desc

        // create a pipeline object (default render states are fine for triangle)
        state.offscreen.pip = sg_make_pipeline(
            &(sg_pipeline_desc){// if the vertex layout doesn't have gaps, don't need to provide strides and offsets
                                .layout =
                                    {.attrs =
                                         {[ATTR_display_position].format = SG_VERTEXFORMAT_FLOAT2,
                                          [ATTR_offscreen_color0].format = SG_VERTEXFORMAT_FLOAT4}},
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
        state.display.vbo = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "quad-vertices"});
        state.display.ibo = sg_make_buffer(
            &(sg_buffer_desc){.usage.index_buffer = true, .data = SG_RANGE(indices), .label = "quad-indices"});

        // a pipeline state object
        state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader     = sg_make_shader(display_shader_desc(sg_query_backend())),
            .index_type = SG_INDEXTYPE_UINT16,
            .layout =
                {.attrs =
                     {[ATTR_display_position].format  = SG_VERTEXFORMAT_FLOAT2,
                      [ATTR_display_texcoord0].format = SG_VERTEXFORMAT_SHORT2N}},
            .label = "quad-pipeline"});

        // a sampler object
        state.display.smp = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        });
    }
}
void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        println("Resized %d %d", e->resize.width, e->resize.height);

        state.width   = e->resize.width;
        state.height  = e->resize.height;
        state.resized = true;
    }
    return false;
}

void program_tick()
{
    if (state.resized == true)
    {
        state.resized = false;
        sg_destroy_view(state.offscreen_img_colview);
        sg_destroy_view(state.offscreen_img_texview);
        sg_destroy_image(state.offscreen_img);

        state.offscreen_img         = sg_make_image(&(sg_image_desc){
                    .usage.color_attachment = true,
                    .width                  = state.width,
                    .height                 = state.height,
                    .pixel_format           = SG_PIXELFORMAT_RGBA8,
                    .label                  = "offscreen-image"});
        state.offscreen_img_colview = sg_make_view(&(sg_view_desc){.color_attachment = state.offscreen_img});
        state.offscreen_img_texview = sg_make_view(&(sg_view_desc){.texture = state.offscreen_img});
    }
    // println("image: %u", state.offscreen_img.id);
    // offscreen
    sg_begin_pass(&(sg_pass){
        .attachments.colors[0] = state.offscreen_img_colview,
        .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .label  = "offscreen-pass"});
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers = state.offscreen.vbo,
    });
    sg_draw(0, 3, 1);
    sg_end_pass();

    // main
    sg_begin_pass(&(sg_pass){
        .action =
            (sg_pass_action){
                .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbo,
        .index_buffer      = state.display.ibo,
        .views[VIEW_tex]   = state.offscreen_img_texview,
        .samplers[SMP_smp] = state.display.smp,
    });
    sg_draw(0, 6, 1);
    sg_end_pass();
}