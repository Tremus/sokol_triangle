//------------------------------------------------------------------------------
//  program_slug2.c
//
//  Self-contained demo of using the Slug vector-fill algorithm (see program_slug.c,
//  https://terathon.com/blog/decade-slug.html) to render a plain vector shape instead of text.
//
//  Slug's GPU-side algorithm (banded curve lookup + dual-axis winding-number raycast, see program_slug2.glsl) has no
//  idea it's rendering glyphs - it just fills a closed contour of quadratic Bezier curves. The only font-specific part
//  of the original port (src/slugutil.c) is the CPU-side step that turns a TrueType outline into that contour
//  representation via stb_truetype. This file replaces that step with a much simpler one: it turns a plain array of 2D
//  points (`points` below) into the same curve/band data. Straight edges become "degenerate" quadratic curves whose
//  control point is the edge's midpoint - exactly what slugutil.c already does for TrueType line segments (see its
//  `vline` case). Everything downstream (band building, curve/band texture packing, the vertex layout, the pipeline,
//  the shader) is carried over unchanged in spirit.
//------------------------------------------------------------------------------
#include "common.h"

#include "program_slug2.glsl.h"

#include <xhl/array.h>
#include <xhl/debug.h>
#include <xhl/vector.h>

#include <math.h>
#include <stdlib.h>

// A single closed contour in normalised 0-1 space: consecutive points form straight edges, and the last point repeats
// the first to close the loop. Swap this out (or extend the build code to handle more than one contour, eg. for shapes
// with holes) to draw a different shape.
static const xvec2f points[] = {
    {0.5f, 0.5f},
    {0.8f, 0.8f},
    {0.3f, 0.8f},
    {0.5f, 0.5f},
};

#define SHAPE_TEX_WIDTH (4096) // must match the 12-bit shift/mask baked into program_slug2.glsl's bandLoc()
#define MAX_BANDS       (16)   // max bands per axis, same cap the original algorithm uses

typedef struct
{
    uint16_t x, y;
} u16vec2_t;

// One quadratic bezier curve: p[0] = start, p[1] = control, p[2] = end.
// A straight edge is a "degenerate" curve with control = midpoint.
typedef struct
{
    xvec2f   p[3];
    uint16_t texture[2]; // filled in during packing: texel coords in curve_tex
} shape_curve_t;

typedef struct
{
    int   curve_index;
    float sort_key;
} band_entry_t;

typedef struct
{
    float x0, y0, x1, y1;
} bbox_t;

// Intermediate state while building the shape's curve/band data.
typedef struct
{
    shape_curve_t* curves; // xhl/array
    bbox_t         bbox;
    band_entry_t** horizontal_bands; // xhl/array of xhl/array
    band_entry_t** vertical_bands;   // xhl/array of xhl/array
    xvec2f         band_scale;
    xvec2f         band_offset;
} shape_build_t;

// Per-instance vertex data uploaded to the GPU.
typedef struct
{
    xvec4f   draw_rect;       // xy = screen pos (px), zw = screen size (px)
    xvec4f   shape_bbox;      // xy = min corner, zw = max corner (shape build space)
    xvec4f   band_transform;  // xy = band_scale, zw = band_offset
    int16_t  shape_params[4]; // xy = shape_loc, zw = max_band_x/y
    uint32_t color;
} shape_vertex_t;

static struct
{
    uint32_t    width;
    uint32_t    height;
    sg_buffer   buf;
    sg_pipeline pip;
    sg_sampler  smp;
    sg_image    curve_img;
    sg_view     curve_tex_view;
    sg_image    band_img;
    sg_view     band_tex_view;
    struct
    {
        bbox_t bbox;
        float  max_band_x;
        float  max_band_y;
        xvec2f band_scale;
        xvec2f band_offset;
        int    shape_loc[2];
    } shape;
} state;

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static int   clampi(int val, int lo, int hi) { return val < lo ? lo : (val > hi ? hi : val); }

static uint32_t pack_color(float r, float g, float b, float a)
{
    uint32_t ur = (uint32_t)(r * 255.0f);
    uint32_t ug = (uint32_t)(g * 255.0f);
    uint32_t ub = (uint32_t)(b * 255.0f);
    uint32_t ua = (uint32_t)(a * 255.0f);
    return (ua << 24) | (ub << 16) | (ug << 8) | ur;
}

// Turn the point list into a single contour of quadratic curves and compute
// its bounding box. Mirrors slugutil.c's handling of TrueType's `vline` verts.
static void build_curves(const xvec2f* pts, int count, shape_build_t* out)
{
    bbox_t bbox = {pts[0].x, pts[0].y, pts[0].x, pts[0].y};
    for (int i = 1; i < count; i++)
    {
        bbox.x0 = minf(bbox.x0, pts[i].x);
        bbox.y0 = minf(bbox.y0, pts[i].y);
        bbox.x1 = maxf(bbox.x1, pts[i].x);
        bbox.y1 = maxf(bbox.y1, pts[i].y);
    }
    out->bbox = bbox;

    for (int i = 0; i < count - 1; i++)
    {
        xvec2f prev = pts[i];
        xvec2f cur  = pts[i + 1];
        xvec2f mid  = {(prev.x + cur.x) * 0.5f, (prev.y + cur.y) * 0.5f};
        xarr_push(out->curves, ((shape_curve_t){.p = {prev, mid, cur}}));
    }
}

static int band_cmp(const void* a, const void* b)
{
    const band_entry_t* pa = (const band_entry_t*)a;
    const band_entry_t* pb = (const band_entry_t*)b;
    // NOTE: inverted sort-order is not a bug
    if (pa->sort_key > pb->sort_key)
    {
        return -1;
    }
    else if (pa->sort_key < pb->sort_key)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// Buckets curves into horizontal/vertical bands so the fragment shader only
// has to test the handful of curves that can possibly cross its scanline,
// instead of every curve in the shape.
static void build_bands(shape_build_t* shape)
{
    int num_curves = (int)xarr_len(shape->curves);
    xassert(num_curves > 0);

    float band_width  = maxf(shape->bbox.x1 - shape->bbox.x0, 1.0f);
    float band_height = maxf(shape->bbox.y1 - shape->bbox.y0, 1.0f);
    int   num_bands_h = clampi(num_curves, 1, MAX_BANDS);
    int   num_bands_v = clampi(num_curves, 1, MAX_BANDS);

    xarr_setlen(shape->horizontal_bands, num_bands_h);
    xarr_setlen(shape->vertical_bands, num_bands_v);
    for (int i = 0; i < num_bands_h; i++)
    {
        shape->horizontal_bands[i] = 0;
    }
    for (int i = 0; i < num_bands_v; i++)
    {
        shape->vertical_bands[i] = 0;
    }

    shape->band_scale  = (xvec2f){(float)num_bands_v / band_width, (float)num_bands_h / band_height};
    shape->band_offset = (xvec2f){-shape->bbox.x0 * shape->band_scale.x, -shape->bbox.y0 * shape->band_scale.y};

    float h_band_size = band_height / (float)num_bands_h;
    float v_band_size = band_width / (float)num_bands_v;
    float h_pad       = h_band_size * 0.5f;
    float v_pad       = v_band_size * 0.5f;

    for (int ci = 0; ci < num_curves; ci++)
    {
        shape_curve_t* curve = &shape->curves[ci];
        float          y_min = minf(minf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
        float          y_max = maxf(maxf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
        float          x_min = minf(minf(curve->p[0].x, curve->p[1].x), curve->p[2].x);
        float          x_max = maxf(maxf(curve->p[0].x, curve->p[1].x), curve->p[2].x);

        int band_first = clampi((int)floorf((y_min - h_pad - shape->bbox.y0) / h_band_size), 0, num_bands_h - 1);
        int band_last  = clampi((int)floorf((y_max + h_pad - shape->bbox.y0) / h_band_size), 0, num_bands_h - 1);
        for (int i = band_first; i <= band_last; i++)
        {
            xarr_push(shape->horizontal_bands[i], ((band_entry_t){.curve_index = ci, .sort_key = x_max}));
        }

        band_first = clampi((int)floorf((x_min - v_pad - shape->bbox.x0) / v_band_size), 0, num_bands_v - 1);
        band_last  = clampi((int)floorf((x_max + v_pad - shape->bbox.x0) / v_band_size), 0, num_bands_v - 1);
        for (int i = band_first; i <= band_last; i++)
        {
            xarr_push(shape->vertical_bands[i], ((band_entry_t){.curve_index = ci, .sort_key = y_max}));
        }
    }

    for (int i = 0; i < num_bands_h; i++)
    {
        if (shape->horizontal_bands[i])
        {
            qsort(shape->horizontal_bands[i], xarr_len(shape->horizontal_bands[i]), sizeof(band_entry_t), band_cmp);
        }
    }
    for (int i = 0; i < num_bands_v; i++)
    {
        if (shape->vertical_bands[i])
        {
            qsort(shape->vertical_bands[i], xarr_len(shape->vertical_bands[i]), sizeof(band_entry_t), band_cmp);
        }
    }
}

static void
write_band_set(band_entry_t** bands, shape_curve_t* curves, u16vec2_t* pixels, int header_offset, int* write_offset)
{
    // Write headers: each band stores (count, data_offset) where data_offset is relative to the shape's texture origin,
    // matching how the shader indexes into the texture via bandLoc().
    int data_offset = *write_offset;
    for (int band_index = 0; band_index < (int)xarr_len(bands); band_index++)
    {
        band_entry_t* band                  = bands[band_index];
        pixels[header_offset + band_index]  = (u16vec2_t){(uint16_t)xarr_len(band), (uint16_t)data_offset};
        data_offset                        += (int)xarr_len(band);
    }
    // Write curve references at the offsets declared above.
    data_offset = *write_offset;
    for (int band_index = 0; band_index < (int)xarr_len(bands); band_index++)
    {
        band_entry_t* band = bands[band_index];
        for (int i = 0; i < (int)xarr_len(band); i++)
        {
            shape_curve_t* curve  = &curves[band[i].curve_index];
            pixels[data_offset++] = (u16vec2_t){curve->texture[0], curve->texture[1]};
        }
    }
    *write_offset = data_offset;
}

// Packs the shape's curves and band lookup tables into two textures (curve_tex / band_tex) and uploads them, exactly
// what the shader in program_slug2.glsl expects to sample from. Since there's only ever one shape here, it always
// starts at texture origin (0, 0) - no multi-shape atlas packing needed.
static void pack_textures(shape_build_t* shape)
{
    xvec4f*    curve_pixels = 0; // xhl/array
    u16vec2_t* band_pixels  = 0; // xhl/array

    // Curve texture: N curve entries + 1 trailing texel for the shared end point of the last curve (closing the
    // contour).
    int num_curves = (int)xarr_len(shape->curves);
    int needed     = num_curves + 1;
    xarr_setlen(curve_pixels, needed);
    for (int i = 0; i < num_curves; i++)
    {
        shape_curve_t* curve = &shape->curves[i];
        curve_pixels[i]      = (xvec4f){curve->p[0].x, curve->p[0].y, curve->p[1].x, curve->p[1].y};
        curve->texture[0]    = (uint16_t)(i % SHAPE_TEX_WIDTH);
        curve->texture[1]    = (uint16_t)(i / SHAPE_TEX_WIDTH);
    }
    shape_curve_t* last_curve = &shape->curves[num_curves - 1];
    curve_pixels[num_curves]  = (xvec4f){last_curve->p[2].x, last_curve->p[2].y, 0.0f, 0.0f};

    int curve_height = (needed + SHAPE_TEX_WIDTH - 1) / SHAPE_TEX_WIDTH;
    xarr_setlen(curve_pixels, curve_height * SHAPE_TEX_WIDTH);
    for (int i = needed; i < (int)xarr_len(curve_pixels); i++)
    {
        curve_pixels[i] = (xvec4f){0};
    }

    // Band texture: header entries (count, offset) for every band, followed by the curve-texel references each band's
    // header points at.
    int num_bands_h = (int)xarr_len(shape->horizontal_bands);
    int num_bands_v = (int)xarr_len(shape->vertical_bands);
    int header_size = num_bands_h + num_bands_v;

    int total_entries = header_size;
    for (int i = 0; i < num_bands_h; i++)
    {
        total_entries += (int)xarr_len(shape->horizontal_bands[i]);
    }
    for (int i = 0; i < num_bands_v; i++)
    {
        total_entries += (int)xarr_len(shape->vertical_bands[i]);
    }
    xarr_setlen(band_pixels, total_entries);

    int write_offset = header_size;
    write_band_set(shape->horizontal_bands, shape->curves, band_pixels, 0, &write_offset);
    write_band_set(shape->vertical_bands, shape->curves, band_pixels, num_bands_h, &write_offset);

    int band_height = (total_entries + SHAPE_TEX_WIDTH - 1) / SHAPE_TEX_WIDTH;
    xarr_setlen(band_pixels, band_height * SHAPE_TEX_WIDTH);
    for (int i = total_entries; i < (int)xarr_len(band_pixels); i++)
    {
        band_pixels[i] = (u16vec2_t){0};
    }

    state.curve_img      = sg_make_image(&(sg_image_desc){
             .width              = SHAPE_TEX_WIDTH,
             .height             = curve_height,
             .pixel_format       = SG_PIXELFORMAT_RGBA32F,
             .data.mip_levels[0] = {.ptr = curve_pixels, .size = xarr_len(curve_pixels) * sizeof(xvec4f)},
             .label              = "vecshape-curve-texture",
    });
    state.curve_tex_view = sg_make_view(&(sg_view_desc){.texture.image = state.curve_img});

    state.band_img      = sg_make_image(&(sg_image_desc){
             .width              = SHAPE_TEX_WIDTH,
             .height             = band_height,
             .pixel_format       = SG_PIXELFORMAT_RG16UI,
             .data.mip_levels[0] = {.ptr = band_pixels, .size = xarr_len(band_pixels) * sizeof(u16vec2_t)},
             .label              = "vecshape-band-texture",
    });
    state.band_tex_view = sg_make_view(&(sg_view_desc){.texture.image = state.band_img});

    state.shape.shape_loc[0] = 0;
    state.shape.shape_loc[1] = 0;

    xarr_free(curve_pixels);
    xarr_free(band_pixels);
}

static void build_shape(void)
{
    shape_build_t build = {0};
    build_curves(points, (int)ARRLEN(points), &build);
    build_bands(&build);
    pack_textures(&build);

    state.shape.bbox        = build.bbox;
    state.shape.band_scale  = build.band_scale;
    state.shape.band_offset = build.band_offset;
    state.shape.max_band_x  = (float)(xarr_len(build.vertical_bands) - 1);
    state.shape.max_band_y  = (float)(xarr_len(build.horizontal_bands) - 1);

    xarr_free(build.curves);
    for (int i = 0; i < (int)xarr_len(build.horizontal_bands); i++)
    {
        xarr_free(build.horizontal_bands[i]);
    }
    xarr_free(build.horizontal_bands);
    for (int i = 0; i < (int)xarr_len(build.vertical_bands); i++)
    {
        xarr_free(build.vertical_bands[i]);
    }
    xarr_free(build.vertical_bands);
}

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    // A stream-update buffer holding one shape_vertex_t per drawn shape, expanded 4x via hardware instancing with the 4
    // corner vertex positions synthesized in the vertex shader. Only one shape is drawn here.
    state.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size                = sizeof(shape_vertex_t),
        .label               = "vecshape-instance-buffer",
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(slug2_shader_desc(sg_query_backend())),
        .layout =
            {
                .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                .attrs =
                    {
                        [ATTR_slug2_draw_rect]         = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug2_shape_bbox]        = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug2_in_band_transform] = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug2_in_shape_params]   = {.format = SG_VERTEXFORMAT_SHORT4, .buffer_index = 0},
                        [ATTR_slug2_in_fill_color]     = {.format = SG_VERTEXFORMAT_UBYTE4N, .buffer_index = 0},
                    },
            },
        .index_type     = SG_INDEXTYPE_NONE,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .colors[0] =
            {
                .blend =
                    {
                        .enabled          = true,
                        .src_factor_rgb   = SG_BLENDFACTOR_ONE,
                        .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                        .src_factor_alpha = SG_BLENDFACTOR_ONE,
                        .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
            },
        .label = "vecshape-pipeline",
    });
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
        .label      = "vecshape-sampler",
    });

    build_shape();
}

void program_shutdown()
{
    sg_destroy_image(state.curve_img);
    sg_destroy_view(state.curve_tex_view);
    sg_destroy_image(state.band_img);
    sg_destroy_view(state.band_tex_view);
}

void program_tick()
{
    float             sx        = 2.0f / (float)state.width;
    float             sy        = 2.0f / (float)state.height;
    const vs_params_t vs_params = {
        .xform = {sx, sy, -1.0f, -1.0f},
    };

    // Points are normalised 0-1, so scale x by window width and y by window height directly - this is recomputed every
    // tick, so resizing the window rescales the shape immediately.
    bbox_t bbox = state.shape.bbox;
    float  w    = (float)state.width;
    float  h    = (float)state.height;

    const shape_vertex_t vertex = {
        .draw_rect  = {bbox.x0 * w, bbox.y0 * h, (bbox.x1 - bbox.x0) * w, (bbox.y1 - bbox.y0) * h},
        .shape_bbox = {bbox.x0, bbox.y0, bbox.x1, bbox.y1},
        .band_transform =
            {state.shape.band_scale.x, state.shape.band_scale.y, state.shape.band_offset.x, state.shape.band_offset.y},
        .shape_params =
            {
                (int16_t)state.shape.shape_loc[0],
                (int16_t)state.shape.shape_loc[1],
                (int16_t)state.shape.max_band_x,
                (int16_t)state.shape.max_band_y,
            },
        .color = pack_color(0.95f, 0.55f, 0.2f, 1.0f),
    };
    sg_update_buffer(state.buf, &SG_RANGE(vertex));

    sg_begin_pass(&(sg_pass){
        .action    = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.1f, 0.1f, 0.1f, 1.0f}}},
        .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8),
    });
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.buf,
        .views =
            {
                [VIEW_band_tex]  = state.band_tex_view,
                [VIEW_curve_tex] = state.curve_tex_view,
            },
        .samplers[SMP_point_sampler] = state.smp,
    });
    sg_draw(0, 6, 1);
    sg_end_pass();
}

bool program_event(const PWEvent* event)
{
    switch (event->type)
    {
    case PW_EVENT_RESIZE_UPDATE:
        state.width  = event->resize.width;
        state.height = event->resize.height;
        break;
    default:
        break;
    }
    return false;
}
