#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "common.h"

#include <math.h>
#include <string.h>
#include <xhl/alloc.h>
#include <xhl/files.h>
#include <xhl/time.h>

#include "nanosvg.h"
#include "nanosvgrast.h"

#include "program_nanosvg.glsl.h"

static struct
{

    NSVGrasterizer* rast;
    NSVGimage*      svg;

    sg_image   svg_img;
    sg_view    svg_view;
    sg_sampler svg_smp;

    sg_pipeline pip;

    int width, height;
} state;

static const sg_color_target_state BLEND_DEFAULT = {
    .write_mask = SG_COLORMASK_RGBA,
    .blend      = {
             .enabled          = true,
             .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
             .src_factor_alpha = SG_BLENDFACTOR_ONE,
             .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
             .dst_factor_alpha = SG_BLENDFACTOR_ONE,
    }};

void program_setup()
{
    xalloc_init();
    xtime_init();

    state.rast   = nsvgCreateRasterizer();
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    {
        xtime_stopwatch_start();

        // static const char* path_tiger_svg = SRC_DIR XFILES_DIR_STR "Ghostscript_Tiger.svg";
        static const char* path_tiger_svg = SRC_DIR XFILES_DIR_STR "Retrig_icon.svg";
        state.svg                         = nsvgParseFromFile(path_tiger_svg, "px", 96);

        xtime_stopwatch_log_ms("Read file in:");
    }

    println("SVG size: %f x %f", state.svg->width, state.svg->height);
    float scale = APP_HEIGHT / state.svg->height;
    // float          scale = 1;
    int            w   = (int)ceilf(state.svg->width * scale);
    int            h   = (int)ceilf(state.svg->height * scale);
    unsigned char* img = img = malloc(w * h * 4);
    println("IMG size (scaled): %d x %d", w, h);

    {
        xtime_stopwatch_start();

        nsvgRasterize(state.rast, state.svg, 0, 0, scale, img, w, h, w * 4);

        xtime_stopwatch_log_ms("Raster image in");
    }

    state.svg_img  = sg_make_image(&(sg_image_desc){
         .width              = w,
         .height             = h,
         .pixel_format       = SG_PIXELFORMAT_RGBA8,
         .data.mip_levels[0] = {
             .ptr  = img,
             .size = w * h * 4,
        }});
    state.svg_view = sg_make_view(&(sg_view_desc){.texture = state.svg_img});
    state.svg_smp  = sg_make_sampler(&(sg_sampler_desc){
         .min_filter    = SG_FILTER_NEAREST,
         .mag_filter    = SG_FILTER_NEAREST,
         .mipmap_filter = SG_FILTER_NEAREST,
         .wrap_u        = SG_WRAP_CLAMP_TO_EDGE,
         .wrap_v        = SG_WRAP_CLAMP_TO_EDGE,
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader    = sg_make_shader(nanosvg_shader_desc(sg_query_backend())),
        .colors[0] = BLEND_DEFAULT,
    });

    free(img);
}

void program_shutdown()
{
    nsvgDeleteRasterizer(state.rast);

    nsvgDelete(state.svg);

    xalloc_shutdown();
}

bool program_event(const PWEvent* event)
{
    if (event->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width  = event->resize.width;
        state.height = event->resize.height;
        println("resize: %d x %d", state.width, state.height);
    }
    return false;
}

void program_tick()
{
    sg_begin_pass(&(sg_pass){
        .action =
            (sg_pass_action){
                .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&(sg_bindings){
        .views[VIEW_tex]   = state.svg_view,
        .samplers[SMP_smp] = state.svg_smp,
    });

    sg_image_desc img_desc = sg_query_image_desc(state.svg_img);

    vs_uniforms_t uniforms = {
        .topleft     = {0, 0},
        .bottomright = {img_desc.width, img_desc.height},
        .size        = {state.width, state.height},

        .img_topleft     = {0, 0},
        .img_bottomright = {img_desc.width, img_desc.height},
    };
    sg_apply_uniforms(UB_vs_uniforms, &SG_RANGE(uniforms));

    sg_draw(0, 6, 1);
    sg_end_pass();
}