#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "common.h"

#include <math.h>
#include <string.h>
#include <xhl/alloc.h>
#include <xhl/files.h>
#include <xhl/string.h>
#include <xhl/time.h>

#include "nanosvg3.h"
#include "nanosvgrast3.h"
#include "stb_image_write.h"

#include "program_nanosvg.glsl.h"

void parse_svg(const char* path)
{
    NSVGimage2* svg = nsvgParseFromFile2(path, "px", 96);

    const char* name = xfiles_get_name(path);
    const char* ext  = xfiles_get_extension(name);

    // Print buffer to console
    {
        int       i           = 0;
        int       n           = 0;
        char      buffer[256] = {0};
        uint64_t* data        = (uint64_t*)svg;
        xassert((svg->buffer_size & 7) == 0);
        const int N = svg->buffer_size / 8;

        printf(
            "// clang-format off\n"
            "const unsigned long long SVG_DATA_%.*s[] = {\n",
            (int)(ext - name),
            name);
        while (i < N)
        {
            // Try to print the shortest amount we can
            uint64_t v = data[i];
            if (v == 0)
            {
                n += xtr_fmt(buffer, sizeof(buffer), n, "0,");
            }
            else if (v < 100000000000000000llu) // 18 places
            {
                n += xtr_fmt(buffer, sizeof(buffer), n, "%llu,", v);
            }
            else
            {
                n += xtr_fmt(buffer, sizeof(buffer), n, "0x%llX,", v);
            }

            if (n > 120)
            {
                printf("%s\n", buffer);
                n = 0;
            }

            i++;
        }
        if (n)
            printf("%s\n", buffer);
        printf("};\n"
               "// clang-format on\n");
        fflush(stdout);
    }
}

// clang-format off
const unsigned long long SVG_DATA_Retrig_icon[] = {
0x41E0000041E00000,4294968272,8589934594,56,1958505086976,3985729651168,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,4287728258,
0,0,0,0,0,0,0,0,0,0,0x3F8000003F800000,0x4080000000000000,0,0,0,0,562954248388608,1,4287728258,0,0,0,0,0,0,0,0,0,0,0x3F8000003F800000,
0x4080000000000000,0,0,0,0,8589934592,0,4295819264,4297785370,0x40C0000040C00000,0x40C0000040D55555,0x40C0000040EAAAAB,0x40C0000041000000,
0x4135555641000000,0x4185555541000000,0x41B0000041000000,0x41B0000040EAAAAB,0x41B0000040D55555,0x41B0000040C00000,0x4185555540C00000,
0x4135555640C00000,0x40C0000040C00000,0x4190000041880000,0x41900000419E1759,0x417C2EB241B00000,0x4150000041B00000,0x4123D14E41B00000,
0x41000000419E1759,0x4100000041880000,0x41000000417AAAAB,0x4100000041655555,0x4100000041500000,0x410AAAAB41500000,0x4115555541500000,
0x4120000041500000,0x4120000041655555,0x41200000417AAAAB,0x4120000041880000,0x4120000041954155,0x41357D5641A00000,0x4150000041A00000,
0x416A82AA41A00000,0x4180000041954155,0x4180000041880000,0x418000004182AAAB,0x41800000417AAAAB,0x4180000041700000,0x4170000041700000,
0x4160000041700000,0x4150000041700000,0x41655555415AAAAB,0x417AAAAB41455555,0x4188000041300000,0x4192AAAB41455555,0x419D5555415AAAAB,
0x41A8000041700000,0x41A0000041700000,0x4198000041700000,0x4190000041700000,0x41900000417AAAAB,0x419000004182AAAB,0x4190000041880000,
0x4190000041880000,0x4190000041880000,0x4190000041880000,
};
// clang-format on

static struct
{
    LinkedArena*   arena;
    NSVGrasterizer rast;

    NSVGimage2* svg;

    sg_image   svg_img;
    sg_view    svg_view;
    sg_sampler svg_smp;

    sg_pipeline pip;

    int width, height;
} state = {0};

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

    state.arena  = linked_arena_create_ex(0, 64 * 1024);
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    {
        uint64_t time_start = xtime_now_ns();

        // static const char* path = SRC_DIR XFILES_DIR_STR "Ghostscript_Tiger.svg";
        static const char* path = SRC_DIR XFILES_DIR_STR "Retrig_icon.svg";
        state.svg               = nsvgParseFromFile2(path, "px", 96);

        parse_svg(path);

        uint64_t time_end = xtime_now_ns();

        // println("Read file in: %.3fms", xtime_convert_ns_to_ms(time_end - time_start));
    }

    NSVGimage2* svg  = state.svg;
    NSVGimage2* svg2 = (NSVGimage2*)SVG_DATA_Retrig_icon;

    xassert(svg->buffer_size == sizeof(SVG_DATA_Retrig_icon));
    int cmp = memcmp(svg, SVG_DATA_Retrig_icon, sizeof(SVG_DATA_Retrig_icon));
    xassert(cmp == 0);

    println("SVG size: %f x %f", svg->width, svg->height);
    float scale = APP_HEIGHT / svg->height; // ~800us
    // float          scale = 1; // ~40us
    int            w   = (int)ceilf(svg->width * scale);
    int            h   = (int)ceilf(svg->height * scale);
    unsigned char* img = img = malloc(w * h * 4);
    println("IMG size (scaled): %d x %d", w, h);

    {
        uint64_t time_start = xtime_now_ns();

        nsvgRasterize(&state.rast, svg, 0, 0, scale, img, w, h, w * 4, state.arena);

        uint64_t time_end = xtime_now_ns();
        println("Raster image in: %.3fms", xtime_convert_ns_to_ms(time_end - time_start));
    }

    // Write img to desktop
    // {
    //     int  n = 0;
    //     char filepath[1024];
    //     n = xfiles_get_user_directory(filepath, sizeof(filepath), XFILES_USER_DIRECTORY_DESKTOP);
    //     if (n)
    //     {
    //         n += xfmt(filepath, n, "%cmy_img.png", XFILES_DIR_CHAR);
    //         stbi_write_png(filepath, w, h, 4, img, w * 4);
    //     }
    // }

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
    linked_arena_destroy(state.arena);

    if (state.svg)
        nsvgDelete2(state.svg);

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