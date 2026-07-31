#include "sokol_gfx.h"
#include "stb_truetype.h"
#include <stdint.h>

typedef struct vec2_t
{
    float x, y;
} vec2_t;

typedef struct vec4_t
{
    float x, y, z, w;
} vec4_t;

static inline vec2_t vec2(float x, float y)
{
    vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}
static inline vec2_t vec2_add(vec2_t a, vec2_t b) { return vec2(a.x + b.x, a.y + b.y); }
static inline vec2_t vec2_sub(vec2_t a, vec2_t b) { return vec2(a.x - b.x, a.y - b.y); }
static inline vec2_t vec2_mulf(vec2_t a, float s) { return vec2(a.x * s, a.y * s); }

static inline vec4_t vec4(float x, float y, float z, float w)
{
    vec4_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

#define SLUG_TEX_WIDTH (4096)
#define SLUG_MAX_BANDS (16)

typedef struct
{
    vec2_t   p[3];
    uint16_t texture[2];
} slug_curve_t;

typedef struct
{
    int start;
    int count;
} slug_contour_range_t;

typedef struct
{
    int   curve_index;
    float sort_key;
} slug_band_entry_t;

typedef struct
{
    float x0, y0, x1, y1;
} slug_bbox_t;

typedef struct
{
    slug_curve_t*         curves;   // managed via xhl/array.h
    slug_contour_range_t* contours; // managed via xhl/array.h
    slug_bbox_t           bbox;
    float                 advance;
    float                 lsb;
    slug_band_entry_t**   horizontal_bands; // managed via xhl/array.h
    slug_band_entry_t**   vertical_bands;   // managed via xhl/array.h
    vec2_t                band_scale;
    vec2_t                band_offset;
    int32_t               glyph_loc[2];
} slug_glyph_build_t;

typedef struct
{
    const void* ptr;
    size_t      size;
} slug_range_t;

typedef struct
{
    slug_bbox_t bbox;
    float       advance;
    float       lsb;
    float       max_band_x;
    float       max_band_y;
    vec2_t      band_scale;
    vec2_t      band_offset;
    int         glyph_loc[2];
} slug_glyph_t;

typedef struct
{
    uint16_t glyph_id;
    uint16_t palette_index;
} slug_colr_layer_t;

typedef struct
{
    uint16_t glyph_id; // this also serves as hashmap key
    uint16_t first_layer;
    uint16_t num_layers;
    uint16_t _pad;
} slug_colr_base_t;

typedef struct
{
    bool           valid;
    slug_glyph_t*  glyphs; // managed via xhl/array.h
    stbtt_fontinfo info;
    struct
    {
        sg_image img;
        sg_view  tex_view;
        int      height;
    } curve;
    struct
    {
        sg_image img;
        sg_view  tex_view;
        int      height;
    } band;
    vec4_t*            cpal_colors; // managed via xhl/array.h
    slug_colr_base_t*  colr_bases;  // managed via xhl/array.h
    slug_colr_layer_t* colr_layers; // managed via xhl/array.h;
} slug_font_t;

bool                    slug_load_font(slug_font_t* font, const slug_range_t* data);
void                    slug_unload_font(slug_font_t* font);
const slug_glyph_t*     slug_get_glyph(const slug_font_t* font, uint32_t cp);
const slug_colr_base_t* slug_find_colr_base(const slug_font_t* font, uint32_t cp);
