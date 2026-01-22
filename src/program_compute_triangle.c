#include "common.h"

#include <math.h>
#include <xhl/debug.h>

#include "program_compute_triangle.glsl.h"

static struct
{
    sg_sampler smp;
    struct
    {
        sg_pipeline pip;
        sg_image    img;
        sg_view     storage_view;
        sg_view     col_view;
    } compute;

    struct
    {
        sg_pipeline pip;
    } display;

    bool resized;
    int  width, height;
} state;

void program_setup()
{
    state.width   = APP_WIDTH;
    state.height  = APP_HEIGHT;
    state.resized = true;

    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
    });

    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .depth   = {.pixel_format = SG_PIXELFORMAT_NONE},
        .shader  = sg_make_shader(compute_shader_desc(sg_query_backend())),
        .label   = "compute-pipeline",
    });
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label  = "display-pipeline",
    });
}

void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width   = e->resize.width;
        state.height  = e->resize.height;
        state.resized = true;
    }
    return false;
}

void program_tick()
{
    if (state.resized)
    {
        state.resized = false;
        sg_destroy_view(state.compute.col_view);
        sg_destroy_view(state.compute.storage_view);
        sg_destroy_image(state.compute.img);
        state.compute.col_view.id     = 0;
        state.compute.storage_view.id = 0;
        state.compute.img.id          = 0;

        state.compute.img          = sg_make_image(&(sg_image_desc){
                     .usage =
                {
                             .storage_image = true,
                },

                     .width        = state.width,
                     .height       = state.height,
                     .pixel_format = SG_PIXELFORMAT_RGBA8,
                     .label        = "CS image",
        });
        state.compute.storage_view = sg_make_view(&(sg_view_desc){
            .storage_image = state.compute.img,
            .label         = "CS image view",
        });
        state.compute.col_view     = sg_make_view(&(sg_view_desc){
                .texture = {.image = state.compute.img},
                .label   = "CS image view",
        });
    }
    sg_begin_pass(&(sg_pass){.compute = true, .label = "compute-pass"});
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_cs_output] = state.compute.storage_view,
    });
    sg_dispatch(state.width / 16, state.height / 16, 1);
    sg_end_pass();

    // swapchain render pass to display the result
    sg_begin_pass(&(sg_pass){.swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8), .label = "display-pass"});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_disp_tex]   = state.compute.col_view,
        .samplers[SMP_disp_smp] = state.smp,
    });
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}