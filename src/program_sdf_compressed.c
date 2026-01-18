#include "common.h"

#include <xhl/debug.h>
#include <xhl/maths.h>
#include <xhl/time.h>
#include <xhl/vector.h>

#include "program_sdf_compressed.glsl.h"

/*
NanoVG NOTES
Some rough calculations on data usage by nanovg
Note that NVG uses indices for looking up verts. Indices have an additional cost, but we aren't tracking that here

Verts are 16 bytes
Vert uniform buffer is 16 bytes
Frags are 16 bytes
Frag uniform buffer is 176 bytes

Text:
6 verts per glyph
16(v) * 6(nv) = 96 bytes per glyph
96 * num_glyphs + 176(fub) = 272 bytes if num_glyphs is 1. Smallest draw, single letter
96 * num_glyphs + 176(fub) = 752 bytes if num_glyphs is 6. Typical label
96 * num_glyphs + 176(fub) = 2096 bytes if num_glyphs is 20. Short sentance

Rounded Rectangle (Fill):
Typically 62 verts
16(v) * 62(nv) + 176(fub) = 1168 bytes

Rounded Rectangle (stroke):
Typically 42 verts
16(v) * 42(nv) + 176(fub) = 848 bytes

Circle
Radius 12px: 96 verts
Radius 3px: 50 verts
The larger the radius, the more verts are requires. Fortunately its not that much more
Small circles use an incredible amount of tris
16(v) * 50(nv) + 176(fub) = 976 bytes
16(v) * 96(nv) + 176(fub) = 1712 bytes

ARC:
Typically used for rotary parameters. For a standard 7:30 to 5:30 (wall clock angle) stroked arc, nanovg will tesselate
54 verts.
16(v) * 54(nv) + 176(fub) = 1,040 bytes

Lines (stroked)
The larger and more complex the line, the more vertices used.
Large complex lines: 1700 - 3400 vertices
16(v) * 1700(nv) + 176(fub) = 27,376 bytes
16(v) * 3400(nv) + 176(fub) = 54,576 bytes

Whole GUI:
The more stuff you have on screen, the more vertices you use.
In a couple of private projects I'm working on with a few parameters and big display of with animated lines, expect at
leas 200-400kb of uploaded data every frame

Comparisons with techniques from this shader:
Lets say each nanovg 'shape' (rounded rect, arc, circle) uses 1000 bytes on average
At the time of writing, the items in my SBO are 48 bytes, but that could grow to 64-80 bytes each
This is 92-95.2% less data per shape, and should hopefully see a 12.5-20x performance improvement

Text doesn't really improve all that much, and probably isn't a bottleneck

Lines are currently unsolved. They have always been the biggest bottleneck

Icons:
In production apps, most complex filled/stroked polygons are static icons. They're small, often and have simple colours.
They could easily fit in a sprite sheet type thing / megatexture. Very plain coloured icon could fit in a megatexture
using a single colour cannel. Emojis will need an RGBA megatexture
*/

// application state
static struct
{
    sg_pipeline pip;
    sg_bindings bind;

    sg_buffer sbo;
    sg_view   sbv;

    int      window_width;
    int      window_height;
    uint64_t last_frame_time;
    double   frame_delta_sec;
} state = {0};

// TODO do scissoring

// Playing with code from here:
// https://iquilezles.org/articles/distfunctions2d/

// Good artical on setting the right feather size
// https://bohdon.com/docs/smooth-sdf-shape-edges/

enum
{
    SDF_SHAPE_NONE,
    SDF_SHAPE_ROUNDED_RECTANGLE_FILL,
    SDF_SHAPE_ROUNDED_RECTANGLE_STROKE,
    SDF_SHAPE_CIRCLE_FILL,
    SDF_SHAPE_CIRCLE_STROKE,
    SDF_SHAPE_TRIANGLE_FILL,
    SDF_SHAPE_TRIANGLE_STROKE,
    SDF_SHAPE_PIE_FILL,
    SDF_SHAPE_PIE_STROKE,
    SDF_SHAPE_ARC_ROUND_STROKE,
    SDF_SHAPE_ARC_BUTT_STROKE,
};

enum
{
    SDF_GRADEINT_NONE,
    SDF_GRADEINT_LINEAR,
    SDF_GRADEINT_RADIAL,
    SDF_GRADEINT_CONIC,
    SDF_GRADEINT_BOX,
};

// Compressed squares

// NOTE
// sdf_type, grad_type, stroke_width, feather could probably all be packed into one 4x8unorm int
typedef struct myvertex_t SDFShape;

static const size_t struct_size = sizeof(SDFShape);
static size_t       OBJECTS_LEN = 0;
static SDFShape     OBJECTS[10000];

void add_obj(const SDFShape* obj)
{
    if (OBJECTS_LEN < ARRLEN(OBJECTS))
    {
        OBJECTS[OBJECTS_LEN] = *obj;
        OBJECTS_LEN++;
    }
}

uint32_t compress_sdf_data(unsigned sdf_type, unsigned grad_type, float feather, float stroke_width)
{
    xassert(stroke_width >= 0 && stroke_width < 16);
    xvecu compressed = {
        .r = sdf_type,
        .g = grad_type,
        .b = (uint8_t)(xm_minf(255, feather * 255)),
        .a = (uint8_t)(xm_minf(255, stroke_width * 16)),
    };
    return compressed.u32;
}

uint32_t compress_border_radius(float tr, float br, float tl, float bl)
{
    xvecu compressed = {
        .r = tr,
        .g = br,
        .b = tl,
        .a = bl,
    };
    return compressed.u32;
}

uint32_t packUnorm2x16(float low_f, float high_f)
{
    uint16_t low_u16  = (uint16_t)(xm_clampf(low_f, 0.0f, 1.0f) * 65535.0f);
    uint16_t high_u16 = (uint16_t)(xm_clampf(high_f, 0.0f, 1.0f) * 65535.0f);
    return low_u16 | (high_u16 << 16);
}
uint32_t packUnorm4x8(float x_f, float y_f, float z_f, float w_f)
{
    xvecu compressed = {
        .r = (uint8_t)(xm_clampf(x_f, 0.0f, 1.0f) * 255.0f),
        .g = (uint8_t)(xm_clampf(y_f, 0.0f, 1.0f) * 255.0f),
        .b = (uint8_t)(xm_clampf(z_f, 0.0f, 1.0f) * 255.0f),
        .a = (uint8_t)(xm_clampf(w_f, 0.0f, 1.0f) * 255.0f),
    };
    return compressed.u32;
}
// Hopefully we can avoid worrying about signedness
// uint32_t packSnorm2x16(float low_f, float high_f)
// {
//     uint16_t low_u16  = (int16_t)(xm_clampf(low_f, -1.0f, 1.0f) * 32767.0f);
//     uint16_t high_u16 = (int16_t)(xm_clampf(high_f, -1.0f, 1.0f) * 32767.0f);
//     return low_u16 | (high_u16 << 16);
// }
// uint32_t packSnorm4x8(float x_f, float y_f, float z_f, float w_f)
// {
//     xvecu compressed = {
//         .r = (int8_t)(xm_clampf(x_f, -1.0f, 1.0f) * 127.0f),
//         .g = (int8_t)(xm_clampf(y_f, -1.0f, 1.0f) * 127.0f),
//         .b = (int8_t)(xm_clampf(z_f, -1.0f, 1.0f) * 127.0f),
//         .a = (int8_t)(xm_clampf(w_f, -1.0f, 1.0f) * 127.0f),
//     };
//     return compressed.u32;
// }

uint32_t compress_arc_rotate_and_range(float rotate_radians, float range_radians)
{
    // remap from [-PI, PI] to [0-1]
    float rotate_turns = rotate_radians / XM_TAUf;
    float range_turns  = range_radians / XM_TAUf;
    float rotate_norm  = rotate_turns - floorf(rotate_turns);
    float range_norm   = range_turns - floorf(range_turns);
    xassert(rotate_norm >= 0 && rotate_norm <= 1);
    xassert(range_norm >= 0 && range_norm <= 1);
    return packUnorm2x16(rotate_norm, range_norm);
}

void draw_circle_fill(float cx, float cy, float radius_px, uint32_t colour)
{
    float feather = 2.0f / radius_px;
    add_obj(&(SDFShape){
        .topleft     = {cx - radius_px, cy - radius_px},
        .bottomright = {cx + radius_px, cy + radius_px},
        .colour1     = colour,
        .sdf_data    = compress_sdf_data(SDF_SHAPE_CIRCLE_FILL, 0, feather, 0),
    });
}

void draw_circle_stroke(float cx, float cy, float radius_px, float stroke_width, uint32_t colour)
{
    float feather = 2.0f / radius_px;
    add_obj(&(SDFShape){
        .topleft     = {cx - radius_px, cy - radius_px},
        .bottomright = {cx + radius_px, cy + radius_px},
        .colour1     = colour,
        .sdf_data    = compress_sdf_data(SDF_SHAPE_CIRCLE_STROKE, 0, feather, stroke_width),
    });
}

void draw_rounded_rectangle_fill(float x, float y, float w, float h, float border_radius, uint32_t colour)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft             = {x, y},
        .bottomright         = {x + w, y + h},
        .sdf_data            = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_FILL, 0, feather, 0),
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),
        .colour1             = colour,
    });
}

void draw_rounded_rectangle_fill_linear(
    float    x,
    float    y,
    float    w,
    float    h,
    float    border_radius,
    float    x_stop_1,
    float    y_stop_1,
    uint32_t col_stop_1,
    float    x_stop_2,
    float    y_stop_2,
    uint32_t col_stop_2)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft             = {x, y},
        .bottomright         = {x + w, y + h},
        .sdf_data            = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_FILL, SDF_GRADEINT_LINEAR, feather, 0),
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),

        .colour1    = col_stop_1,
        .colour2    = col_stop_2,
        .gradient_a = {x_stop_1, y_stop_1},
        .gradient_b = {x_stop_2, y_stop_2},
    });
}

void draw_rounded_rectangle_fill_radial(
    float    x,
    float    y,
    float    w,
    float    h,
    float    border_radius,
    float    cx_stop_1,
    float    cy_stop_1,
    uint32_t col_stop_1,
    float    x_radius_stop_2,
    float    y_radius_stop_2,
    uint32_t col_stop_2)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft     = {x, y},
        .bottomright = {x + w, y + h},
        .sdf_data    = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_FILL, SDF_GRADEINT_RADIAL, feather, 0),

        .colour1             = col_stop_1,
        .colour2             = col_stop_2,
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),
        .gradient_a          = {cx_stop_1, cy_stop_1},
        .gradient_b          = {x_radius_stop_2, y_radius_stop_2},
    });
}

void draw_rounded_rectangle_fill_conic(
    float    x,
    float    y,
    float    w,
    float    h,
    float    border_radius,
    float    radians_stop_1,
    uint32_t col_stop_1,
    float    radians_stop_2,
    uint32_t col_stop_2)
{
    float feather = 4.0f / xm_minf(w, h);

    float range = radians_stop_2 - radians_stop_1;
    float a     = XM_PIf + radians_stop_1;
    float b     = range / XM_TAUf;

    add_obj(&(SDFShape){
        .topleft     = {x, y},
        .bottomright = {x + w, y + h},
        .sdf_data    = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_FILL, SDF_GRADEINT_CONIC, feather, 0),

        .colour1             = col_stop_1,
        .colour2             = col_stop_2,
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),

        .gradient_a = {a, a},
        .gradient_b = {b, b},
    });
}

void draw_rounded_rectangle_fill_box(
    float    x,
    float    y,
    float    w,
    float    h,
    float    border_radius,
    float    x_translate,
    float    y_translate,
    float    blur_radius,
    uint32_t col_stop_outer,
    uint32_t col_stop_inner)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft     = {x, y},
        .bottomright = {x + w, y + h},

        .sdf_data = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_FILL, SDF_GRADEINT_BOX, feather, 0),

        .colour1             = col_stop_outer,
        .colour2             = col_stop_inner,
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),
        .gradient_a          = {x_translate, y_translate},
        .gradient_b          = {blur_radius, blur_radius},
    });
}

void draw_rounded_rectangle_stroke(
    float    x,
    float    y,
    float    w,
    float    h,
    float    border_radius,
    float    stroke_width,
    uint32_t colour)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft     = {x, y},
        .bottomright = {x + w, y + h},
        .sdf_data    = compress_sdf_data(SDF_SHAPE_ROUNDED_RECTANGLE_STROKE, 0, feather, stroke_width),

        .colour1             = colour,
        .borderradius_arcpie = compress_border_radius(border_radius, border_radius, border_radius, border_radius),
    });
}

void draw_triangle_fill(float x, float y, float w, float h, float rotate_radians, uint32_t colour)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft     = {x, y},
        .bottomright = {x + w, y + h},
        .colour1     = colour,
        .sdf_data    = compress_sdf_data(SDF_SHAPE_TRIANGLE_FILL, 0, feather, 0),

        .borderradius_arcpie = compress_arc_rotate_and_range(rotate_radians, 0),
    });
}

void draw_triangle_stroke(float x, float y, float w, float h, float rotate_radians, float stroke_width, uint32_t colour)
{
    float feather = 4.0f / xm_minf(w, h);
    add_obj(&(SDFShape){
        .topleft             = {x, y},
        .bottomright         = {x + w, y + h},
        .sdf_data            = compress_sdf_data(SDF_SHAPE_TRIANGLE_STROKE, 0, feather, stroke_width),
        .borderradius_arcpie = compress_arc_rotate_and_range(rotate_radians, 0),
        .colour1             = colour,
    });
}

void draw_pie_fill(float cx, float cy, float radius_px, float start_radians, float end_radians, uint32_t colour)
{
    float feather      = 2.0f / radius_px;
    float angle_range  = end_radians - start_radians;
    float angle_rotate = (end_radians + start_radians);
    add_obj(&(SDFShape){
        .topleft             = {cx - radius_px, cy - radius_px},
        .bottomright         = {cx + radius_px, cy + radius_px},
        .sdf_data            = compress_sdf_data(SDF_SHAPE_PIE_FILL, 0, feather, 0),
        .borderradius_arcpie = compress_arc_rotate_and_range(angle_rotate * 0.5f, angle_range * 0.5f),
        .colour1             = colour,

    });
}

void draw_pie_stroke(
    float    cx,
    float    cy,
    float    radius_px,
    float    start_radians,
    float    end_radians,
    float    stroke_width,
    uint32_t colour)
{
    float feather      = 2.0f / radius_px;
    float angle_range  = end_radians - start_radians;
    float angle_rotate = (end_radians + start_radians);
    add_obj(&(SDFShape){
        .topleft             = {cx - radius_px, cy - radius_px},
        .bottomright         = {cx + radius_px, cy + radius_px},
        .sdf_data            = compress_sdf_data(SDF_SHAPE_PIE_STROKE, 0, feather, stroke_width),
        .borderradius_arcpie = compress_arc_rotate_and_range(angle_rotate * 0.5f, angle_range * 0.5f),
        .colour1             = colour,
    });
}

void draw_arc_stroke(
    float    cx,
    float    cy,
    float    radius_px,
    float    start_radians,
    float    end_radians,
    float    stroke_width,
    bool     butt,
    uint32_t colour)
{
    float feather      = 2.0f / radius_px;
    float angle_range  = end_radians - start_radians;
    float angle_rotate = (end_radians + start_radians);
    add_obj(&(SDFShape){
        .topleft     = {cx - radius_px, cy - radius_px},
        .bottomright = {cx + radius_px, cy + radius_px},
        .sdf_data =
            compress_sdf_data(butt ? SDF_SHAPE_ARC_BUTT_STROKE : SDF_SHAPE_ARC_ROUND_STROKE, 0, feather, stroke_width),
        .borderradius_arcpie = compress_arc_rotate_and_range(angle_rotate * 0.5f, angle_range * 0.5f),
        .colour1             = colour,
    });
}

void program_setup()
{
    xtime_init();

    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    state.sbo = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(OBJECTS),
        .label                = "objects",
    });
    state.sbv = sg_make_view(&(sg_view_desc){
        .storage_buffer = state.sbo,
    });

    // state.bind.storage_buffers[SBUF_ssbo] = sbuf;
    state.bind.views[VIEW_ssbo] = state.sbv;

    // Note we don't pass any index type or vertex layout
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(vertexpull_shader_desc(sg_query_backend())),
        .colors[0] =
            {.write_mask = SG_COLORMASK_RGBA,
             // .pixel_format = PIXEL_FORMAT,
             .blend =
                 {
                     .enabled          = true,
                     .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
                     .src_factor_alpha = SG_BLENDFACTOR_ONE,
                     .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                     .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                 }},
        .label = "pipeline"});
}

void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.window_width  = e->resize.width;
        state.window_height = e->resize.height;
    }
    return false;
}

void stress_test()
{
    static float sec          = 0;
    const float  frame_delta  = state.frame_delta_sec;
    sec                      += frame_delta;
    sec                      -= (int)sec;

    {
        const unsigned w = APP_WIDTH / 64;
        const unsigned h = APP_HEIGHT / 64;

        float amt = sec * 2;
        if (amt > 1.0f)
            amt = 2.0f - amt;

        float x_delta = amt * w;
        for (unsigned k = 0; k < 64; k++)
        {
            float y = k * h;
            for (unsigned i = 0; i < 64; i++)
            {
                float x = i * w;

                float r = x + w;
                float b = y + h;

                draw_rounded_rectangle_fill_linear(
                    x,
                    y,
                    w,
                    h,
                    2,
                    x + x_delta,
                    y,
                    0x00ffff7f,
                    r - x_delta,
                    b,
                    0xff00ff7f);
            }
        }
    }

    for (unsigned k = 0; k < 24; k++)
    {
        float y = 10 + k * 20;
        for (unsigned i = 0; i < 32; i++)
        {
            float x   = 10 + i * 20;
            float amt = sec * 2;
            if (amt > 1.0f)
                amt = 2.0f - amt;
            draw_arc_stroke(x, y, 8, XM_TAUf * -0.375, XM_TAUf * amt * 0.5f, 1, false, 0x13e369a4);
        }
    }

    for (int k = 0; k < 14; k++)
    {
        float y = 30 + k * 30;
        for (int i = 0; i < 20; i++)
        {
            draw_circle_fill(30 + i * 30, y, 12, 0xff000079);
        }
    }
}

void program_tick()
{
    OBJECTS_LEN = 0;

    uint64_t now_ns         = xtime_now_ns();
    uint64_t frame_delta_ns = now_ns - state.last_frame_time;
    state.last_frame_time   = now_ns;
    state.frame_delta_sec   = xtime_convert_ns_to_sec(frame_delta_ns);

    draw_circle_fill(45, 45, 35, 0xffff00ff);
    draw_circle_stroke(45, 125, 35, 4, 0xff00ffff);
    draw_rounded_rectangle_fill(110, 10, 100, 70, 16, 0x00ffffff);
    draw_rounded_rectangle_stroke(110, 90, 300, 70, 16, 10, 0x00ff00ff);
    draw_rounded_rectangle_stroke(110, 170, 100, 140, 16, 2, 0x00ff00ff);
    draw_triangle_fill(420, 10, 70, 70, XM_PIf * 0.5, 0xff0000ff);
    draw_triangle_stroke(420, 90, 70, 70, XM_PIf, 10, 0xffff00ff);

    draw_pie_fill(535, 45, 35, XM_TAUf * -0.125, XM_TAUf * 0.125, 0xff9321ff);
    draw_pie_stroke(535, 125, 35, XM_TAUf * -0.375, XM_TAUf * 0.375, 10, 0xff00ffff);
    draw_arc_stroke(45, 205, 35, XM_TAUf * -0.375, XM_TAUf * 0.375, 12, 0, 0x45beffff);
    draw_arc_stroke(45, 275, 35, XM_TAUf * -0.375, XM_TAUf * 0.375, 12, 1, 0xc1ff45ff);

    draw_rounded_rectangle_fill_linear(10, 320, 80, 130, 0, 10, 320, 0xff0000ff, 90, 450, 0x00ff00ff);
    draw_rounded_rectangle_fill_radial(110, 320, 80, 130, 0, 170, 420, 0xffff00ff, 40, 30, 0x0000ffff);
    draw_rounded_rectangle_fill_conic(210, 320, 80, 130, 0, XM_TAUf * 0.125, 0xffff00ff, XM_TAUf * 0.625, 0x0000ffff);

    draw_rounded_rectangle_fill_box(310, 320, 80, 130, 8, 0, 0, 20, 0xff0000ff, 0x0000ffff);
    draw_rounded_rectangle_fill_box(410, 320, 130, 80, 8, 0, 0, 20, 0xffff00ff, 0x0000ffff);

    stress_test();

    if (OBJECTS_LEN)
    {
        sg_range range = {.ptr = OBJECTS, sizeof(OBJECTS[0]) * OBJECTS_LEN};
        sg_update_buffer(state.sbo, &range);
        sg_begin_pass(&(sg_pass){
            .action =
                (sg_pass_action){
                    .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .swapchain = get_swapchain(0)});
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);

        vs_params_t uniforms = {.size = {state.window_width, state.window_height}};

        sg_apply_uniforms(UB_vs_params, &SG_RANGE(uniforms));

        sg_draw(0, OBJECTS_LEN * 6, 1);
        sg_end_pass();
    }
    // println("Draw %d objects, %d verts, %d bytes", OBJECTS_LEN, OBJECTS_LEN * 6, OBJECTS_LEN * sizeof(OBJECTS[0]));
}
