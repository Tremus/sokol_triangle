#include "common.h"

#include <math.h>
#include <xhl/debug.h>
#include <xhl/maths.h>
#include <xhl/time.h>

#include "program_compute_line_stroke.glsl.h"

static struct
{
    sg_sampler smp;

    sg_buffer line_buf;
    sg_view   line_buf_view;

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
} state = {0};

static float AUDIO_BUFFER[2048];

void program_setup()
{
    xtime_init();

    state.width   = APP_WIDTH;
    state.height  = APP_HEIGHT;
    state.resized = true;

    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
    });

    state.line_buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(AUDIO_BUFFER),
        .label                = "sine-buffer",
    });

    state.line_buf_view = sg_make_view(&(sg_view_desc){.storage_buffer = state.line_buf});

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
    // Animated sine wave
    // double now_sec = 0;
    double now_sec  = xtime_convert_ns_to_sec(xtime_now_ns());
    now_sec        *= 0.125 * 0.25; // 0.25hz

    const size_t N      = xm_minull(ARRLEN(AUDIO_BUFFER), state.width);
    const size_t buflen = ARRLEN(AUDIO_BUFFER);
    const float  phase  = (now_sec - (int)now_sec) * XM_TAUf;
    const float  inc    = XM_TAUf / (float)buflen * 8;
    for (unsigned i = 0; i < N; i++)
    {
        AUDIO_BUFFER[i] = sinf(phase + i * inc); // sin
        // AUDIO_BUFFER[i] = (((i >> 4) & 3) >> 1) ? -1 : 1; // square
    }
    sg_update_buffer(state.line_buf, &(sg_range){.ptr = AUDIO_BUFFER, sizeof(AUDIO_BUFFER[0]) * N});

    sg_begin_pass(&(sg_pass){
        .compute = true,
        .action  = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .label   = "compute-pass"});
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_cs_output]         = state.compute.storage_view,
        .views[VIEW_cs_storage_buffer] = state.line_buf_view,
    });
    const cs_uniforms_t uniforms = {
        .u_buffer_length = N,
        .u_stroke_width  = 2.0f,
    };
    sg_apply_uniforms(UB_cs_uniforms, &SG_RANGE(uniforms));
    sg_dispatch(state.width / 64, state.height, 1);
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