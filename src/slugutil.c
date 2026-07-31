//------------------------------------------------------------------------------
//  Slug utility functions.
//  NOTE: memory management during font loading is *really* unoptimized.
//
//  PS: should the font processing actually be moved into an offline tool?
//------------------------------------------------------------------------------

#include "slugutil.h"
#include "xhl/array.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

typedef struct
{
    uint16_t x, y;
} u16vec2_t;

typedef struct
{
    vec4_t*    curve_pixels; // managed by xhl/array.h
    int        curve_height;
    u16vec2_t* band_pixels; // managed by xhl/array.h
    int        band_height;
} pack_textures_t;

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data);
static bool parse_cpal(slug_font_t* font, const slug_range_t* data);
static void init_build_glyph(const stbtt_fontinfo* info, int glyph_index, float scale, slug_glyph_build_t* out);
static void build_bands(slug_glyph_build_t* glyph);
static void free_build_glyph(slug_glyph_build_t* glyph);
static pack_textures_t pack_textures(slug_glyph_build_t* glyphs, int num_glyphs);

const slug_glyph_t* slug_get_glyph(const slug_font_t* font, uint32_t codepoint)
{
    int idx = stbtt_FindGlyphIndex(&font->info, codepoint);
    if ((idx >= 0) && (idx < xarr_len(font->glyphs)))
    {
        return &font->glyphs[idx];
    }
    else
    {
        return 0;
    }
}

static int colr_base_cmp(const void* a, const void* b)
{
    const slug_colr_base_t* pa = (slug_colr_base_t*)a;
    const slug_colr_base_t* pb = (slug_colr_base_t*)b;
    if (pa->glyph_id < pb->glyph_id)
    {
        return -1;
    }
    else if (pa->glyph_id > pb->glyph_id)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

const slug_colr_base_t* slug_find_colr_base(const slug_font_t* font, uint32_t codepoint)
{
    int idx = stbtt_FindGlyphIndex(&font->info, codepoint);
    if (idx <= 0)
    {
        return 0;
    }
    size_t num = xarr_len(font->colr_bases);
    if (num == 0)
    {
        return 0;
    }
    const slug_colr_base_t key = {.glyph_id = (uint16_t)idx};
    return (slug_colr_base_t*)bsearch(&key, font->colr_bases, num, sizeof(slug_colr_base_t), colr_base_cmp);
}

bool slug_load_font(slug_font_t* font, const slug_range_t* data)
{
    assert(font);
    assert(data && data->ptr && data->size > 0);
    assert(!font->valid);
    *font = (slug_font_t){0};

    if (!stbtt_InitFont(&font->info, data->ptr, 0))
    {
        slug_unload_font(font);
        return false;
    }
    float em_scale = stbtt_ScaleForMappingEmToPixels(&font->info, 1.0f);

    // colored emoji-fonts...
    if (!parse_colr_v0(font, data))
    {
        slug_unload_font(font);
        return false;
    }
    if (!parse_cpal(font, data))
    {
        slug_unload_font(font);
        return false;
    }

    slug_glyph_build_t* build_glyphs = 0;
    xarr_setlen(build_glyphs, font->info.numGlyphs);
    for (int i = 0; i < xarr_len(build_glyphs); i++)
    {
        init_build_glyph(&font->info, i, em_scale, &build_glyphs[i]);
        build_bands(&build_glyphs[i]);
    }

    pack_textures_t res = pack_textures(build_glyphs, (int)xarr_len(build_glyphs));
    font->curve.height  = res.curve_height;
    font->band.height   = res.band_height;

    font->curve.img      = sg_make_image(&(sg_image_desc){
             .width        = SLUG_TEX_WIDTH,
             .height       = font->curve.height,
             .pixel_format = SG_PIXELFORMAT_RGBA32F,
             .data.mip_levels[0] =
            {
                     .ptr  = res.curve_pixels,
                     .size = xarr_len(res.curve_pixels) * sizeof(vec4_t),
            },
    });
    font->curve.tex_view = sg_make_view(&(sg_view_desc){.texture.image = font->curve.img});

    font->band.img      = sg_make_image(&(sg_image_desc){
             .width        = SLUG_TEX_WIDTH,
             .height       = font->band.height,
             .pixel_format = SG_PIXELFORMAT_RG16UI,
             .data.mip_levels[0] =
            {
                     .ptr  = res.band_pixels,
                     .size = xarr_len(res.band_pixels) * sizeof(u16vec2_t),
            },
    });
    font->band.tex_view = sg_make_view(&(sg_view_desc){.texture.image = font->band.img});

    int num_glyphs = (int)xarr_len(build_glyphs);
    xarr_setlen(font->glyphs, num_glyphs);
    for (int i = 0; i < num_glyphs; i++)
    {
        const slug_glyph_build_t* bg = &build_glyphs[i];
        font->glyphs[i]              = (slug_glyph_t){.bbox        = bg->bbox,
                                                      .advance     = bg->advance,
                                                      .lsb         = bg->lsb,
                                                      .max_band_x  = xarr_len(bg->vertical_bands) - 1,
                                                      .max_band_y  = xarr_len(bg->horizontal_bands) - 1,
                                                      .band_scale  = bg->band_scale,
                                                      .band_offset = bg->band_offset,
                                                      .glyph_loc   = {
                                             [0] = bg->glyph_loc[0],
                                             [1] = bg->glyph_loc[1],
                                         }};
    }

    xarr_free(res.curve_pixels);
    xarr_free(res.band_pixels);
    for (int i = 0; i < xarr_len(build_glyphs); i++)
    {
        free_build_glyph(&build_glyphs[i]);
    }
    xarr_free(build_glyphs);
    font->valid = true;
    return true;
}

void slug_unload_font(slug_font_t* font)
{
    sg_destroy_image(font->curve.img);
    sg_destroy_view(font->curve.tex_view);
    sg_destroy_image(font->band.img);
    sg_destroy_view(font->band.tex_view);
    xarr_free(font->glyphs);
    xarr_free(font->cpal_colors);
    xarr_free(font->colr_bases);
    xarr_free(font->colr_layers);
    *font = (slug_font_t){0};
}

static uint32_t make_tag(char a, char b, char c, char d) { return (a << 24) | (b << 16) | (c << 8) | d; }

static uint16_t read_u16be(const slug_range_t* data, size_t offset)
{
    assert((offset + sizeof(uint16_t)) <= data->size);
    const uint8_t* p  = (uint8_t*)data->ptr;
    uint32_t       b0 = p[offset];
    uint32_t       b1 = p[offset + 1];
    return (b0 << 8) | b1;
}

static uint32_t read_u32be(const slug_range_t* data, size_t offset)
{
    assert((offset + sizeof(uint32_t)) <= data->size);
    const uint8_t* p  = (uint8_t*)data->ptr;
    uint32_t       b0 = p[offset];
    uint32_t       b1 = p[offset + 1];
    uint32_t       b2 = p[offset + 2];
    uint32_t       b3 = p[offset + 3];
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static int find_otf_table(const slug_range_t* data, uint32_t tag)
{
    if (data->size < 12)
    {
        return -1;
    }
    uint16_t num_tables = read_u16be(data, 4);
    for (uint16_t i = 0; i < num_tables; i++)
    {
        size_t record_offset = 12 + i * 16;
        if ((record_offset + 16) > data->size)
        {
            break;
        }
        uint32_t record_tag = read_u32be(data, record_offset);
        if (record_tag == tag)
        {
            return (int)read_u32be(data, record_offset + 8);
        }
    }
    return -1;
}

static bool parse_colr_v0(slug_font_t* font, const slug_range_t* data)
{
    int table_offset = find_otf_table(data, make_tag('C', 'O', 'L', 'R'));
    if (table_offset < 0)
    {
        // not a but, COLR table is optional
        return true;
    }
    uint16_t version = read_u16be(data, table_offset);
    // dont support COLRv1
    if (version != 0)
    {
        return false;
    }
    // Table header (14 bytes from table start):
    //  	Offset +0:  u16 version               (must be 0 for COLRv0)
    //  	Offset +2:  u16 numBaseGlyphRecords
    //  	Offset +4:  u32 offsetBaseGlyphRecord  (from table start)
    //  	Offset +8:  u32 offsetLayerRecord      (from table start)
    //  	Offset +12: u16 numLayerRecords
    int num_base_glyphs = (int)read_u16be(data, table_offset + 2);
    int offset_base     = table_offset + (int)read_u32be(data, table_offset + 4);
    int offset_layer    = table_offset + (int)read_u32be(data, table_offset + 8);
    int num_layers      = (int)read_u16be(data, table_offset + 12);
    xarr_setlen(font->colr_bases, num_base_glyphs);
    for (int i = 0; i < num_base_glyphs; i++)
    {
        slug_colr_base_t* ptr    = &font->colr_bases[i];
        int               offset = offset_base + i * 6; // each record is 6 bytes
        //[glyphID: u16, firstLayerIndex: u16, numLayers: u16]
        ptr->glyph_id    = read_u16be(data, offset);
        ptr->first_layer = read_u16be(data, offset + 2);
        ptr->num_layers  = read_u16be(data, offset + 4);
        ptr->_pad        = 0;
    }
    qsort(font->colr_bases, xarr_len(font->colr_bases), sizeof(slug_colr_base_t), colr_base_cmp);
    xarr_setlen(font->colr_layers, num_layers);
    for (int i = 0; i < num_layers; i++)
    {
        slug_colr_layer_t* ptr    = &font->colr_layers[i];
        int                offset = offset_layer + i * 4; // each record is 4 bytes
        ptr->glyph_id             = read_u16be(data, offset);
        ptr->palette_index        = read_u16be(data, offset + 2);
    }
    return true;
}

static int mini(int a, int b) { return a < b ? a : b; }

static int clampi(int val, int minval, int maxval)
{
    if (val < minval)
        return minval;
    else if (val > maxval)
        return maxval;
    else
        return val;
}

static float minf(float a, float b) { return a < b ? a : b; }

static float maxf(float a, float b) { return a > b ? a : b; }

static bool parse_cpal(slug_font_t* font, const slug_range_t* data)
{
    int table_offset = find_otf_table(data, make_tag('C', 'P', 'A', 'L'));
    if (table_offset < 0)
    {
        // not a bug, CPAL is optional
        return true;
    }
    // Table header (12 bytes from table start):
    //   Offset +0: u16 version
    //   Offset +2: u16 numPaletteEntries   (number of colors per palette)
    //   Offset +4: u16 numPalettes         (usually 1)
    //   Offset +6: u16 numColorRecords     (total colors across all palettes)
    //   Offset +8: u32 offsetFirstColorRecord  (from table start)
    int num_entries       = (int)read_u16be(data, table_offset + 2);
    int num_color_records = (int)read_u16be(data, table_offset + 6);
    int color_offset      = table_offset + (int)read_u32be(data, table_offset + 8);
    int count             = mini(num_entries, num_color_records);
    xarr_setlen(font->cpal_colors, count);
    for (int i = 0; i < count; i++)
    {
        const uint8_t* src = (uint8_t*)data->ptr + color_offset + i * 4;
        vec4_t*        dst = &font->cpal_colors[i];
        dst->z             = ((float)*src++) / 255.0f;
        dst->y             = ((float)*src++) / 255.0f;
        dst->x             = ((float)*src++) / 255.0f;
        dst->w             = ((float)*src) / 255.0f;
    }
    return true;
}

static void free_build_glyph(slug_glyph_build_t* glyph)
{
    xarr_free(glyph->curves);
    xarr_free(glyph->contours);
    for (int i = 0; i < xarr_len(glyph->horizontal_bands); i++)
    {
        xarr_free(glyph->horizontal_bands[i]);
    }
    xarr_free(glyph->horizontal_bands);
    for (int i = 0; i < xarr_len(glyph->vertical_bands); i++)
    {
        xarr_free(glyph->vertical_bands[i]);
    }
    xarr_free(glyph->vertical_bands);
}

static void init_build_glyph(const stbtt_fontinfo* info, int glyph_index, float em_scale, slug_glyph_build_t* out)
{
    slug_glyph_build_t* glyph = out;
    memset(glyph, 0, sizeof(slug_glyph_build_t));
    int adv, lsb_raw;
    stbtt_GetGlyphHMetrics(info, glyph_index, &adv, &lsb_raw);
    glyph->advance = (float)adv * em_scale;
    glyph->lsb     = (float)lsb_raw * em_scale;

    int ix0, iy0, ix1, iy1;
    if (stbtt_GetGlyphBox(info, glyph_index, &ix0, &iy0, &ix1, &iy1) == 0)
    {
        return;
    }
    glyph->bbox = (slug_bbox_t){
        .x0 = (float)ix0 * em_scale,
        .y0 = (float)iy0 * em_scale,
        .x1 = (float)ix1 * em_scale,
        .y1 = (float)iy1 * em_scale,
    };

    stbtt_vertex* verts;
    int           nv = stbtt_GetGlyphShape(info, glyph_index, &verts);
    if (nv <= 0)
    {
        return;
    }
    bool   in_contour    = false;
    int    contour_start = 0;
    vec2_t previous      = vec2(0.0f, 0.0f);
    for (int i = 0; i < nv; i++)
    {
        stbtt_vertex* vert = &verts[i];
        switch (vert->type)
        {
        case 1:
            // vmove
            {
                if (in_contour)
                {
                    int count = (int)xarr_len(glyph->curves) - contour_start;
                    if (count > 0)
                    {
                        xarr_push(glyph->contours, ((slug_contour_range_t){.start = contour_start, .count = count}));
                    }
                }
                previous      = vec2((float)vert->x * em_scale, (float)vert->y * em_scale);
                contour_start = (int)xarr_len(glyph->curves);
                in_contour    = true;
            }
            break;
        case 2:
            // vline
            {
                vec2_t current = vec2((float)vert->x * em_scale, (float)vert->y * em_scale);
                vec2_t mid     = vec2_mulf(vec2_add(previous, current), 0.5f);
                xarr_push(glyph->curves, ((slug_curve_t){.p = {previous, mid, current}}));
                previous = current;
            }
            break;
        case 3:
            // vcurve
            {
                vec2_t current = vec2((float)vert->x * em_scale, (float)vert->y * em_scale);
                vec2_t control = vec2((float)vert->cx * em_scale, (float)vert->cy * em_scale);
                xarr_push(glyph->curves, ((slug_curve_t){.p = {previous, control, current}}));
                previous = current;
            }
            break;
        case 4:
            // vcubic — approximate with three quadratic Beziers
            // Split cubic P0,C1,C2,P3 at t=1/3 and t=2/3 via de Casteljau,
            // then approximate each sub-cubic as a quadratic with ctrl=(c1+c2)/2.
            {
                vec2_t p3 = vec2((float)vert->x * em_scale, (float)vert->y * em_scale);
                vec2_t c1 = vec2((float)vert->cx * em_scale, (float)vert->cy * em_scale);
                vec2_t c2 = vec2((float)vert->cx1 * em_scale, (float)vert->cy1 * em_scale);
                vec2_t p0 = previous;
                // de Casteljau split at t=1/3
                const float t   = 1.0f / 3.0f;
                vec2_t      ab  = vec2_add(p0, vec2_mulf(vec2_sub(c1, p0), t));
                vec2_t      bc  = vec2_add(c1, vec2_mulf(vec2_sub(c2, c1), t));
                vec2_t      cd  = vec2_add(c2, vec2_mulf(vec2_sub(p3, c2), t));
                vec2_t      abc = vec2_add(ab, vec2_mulf(vec2_sub(bc, ab), t));
                vec2_t      bcd = vec2_add(bc, vec2_mulf(vec2_sub(cd, bc), t));
                vec2_t      e1  = vec2_add(abc, vec2_mulf(vec2_sub(bcd, abc), t)); // point on curve at t=1/3
                // Sub-cubic 1: p0, ab, abc, e1 → quadratic ctrl = (ab + abc) * 0.5
                vec2_t q1 = vec2_mulf(vec2_add(ab, abc), 0.5f);
                // de Casteljau split remaining cubic (e1, bcd, cd, p3) at t=0.5 (= t=2/3 of original)
                vec2_t ab2  = vec2_add(e1, vec2_mulf(vec2_sub(bcd, e1), 0.5f));
                vec2_t bc2  = vec2_add(bcd, vec2_mulf(vec2_sub(cd, bcd), 0.5f));
                vec2_t cd2  = vec2_add(cd, vec2_mulf(vec2_sub(p3, cd), 0.5f));
                vec2_t abc2 = vec2_add(ab2, vec2_mulf(vec2_sub(bc2, ab2), 0.5f));
                vec2_t bcd2 = vec2_add(bc2, vec2_mulf(vec2_sub(cd2, bc2), 0.5f));
                vec2_t e2   = vec2_add(abc2, vec2_mulf(vec2_sub(bcd2, abc2), 0.5f)); // point on curve at t=2/3
                // Sub-cubic 2: e1, ab2, abc2, e2 → quadratic ctrl = (ab2 + abc2) * 0.5
                vec2_t q2 = vec2_mulf(vec2_add(ab2, abc2), 0.5f);
                vec2_t q3 = vec2_mulf(vec2_add(bcd2, cd2), 0.5f);
                xarr_push(glyph->curves, ((slug_curve_t){.p = {p0, q1, e1}}));
                xarr_push(glyph->curves, ((slug_curve_t){.p = {e1, q2, e2}}));
                xarr_push(glyph->curves, ((slug_curve_t){.p = {e2, q3, p3}}));
                previous = p3;
            }
            break;
        }
    }
    if (in_contour)
    {
        int count = (int)xarr_len(glyph->curves) - contour_start;
        if (count > 0)
        {
            xarr_push(glyph->contours, ((slug_contour_range_t){.start = contour_start, .count = count}));
        }
    }
    stbtt_FreeShape(info, verts);
}

static int band_cmp(const void* a, const void* b)
{
    const slug_band_entry_t* pa = (slug_band_entry_t*)a;
    const slug_band_entry_t* pb = (slug_band_entry_t*)b;
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

static void build_bands(slug_glyph_build_t* glyph)
{
    int num_curves = (int)xarr_len(glyph->curves);
    if (0 == num_curves)
    {
        return;
    }

    float band_width             = maxf(glyph->bbox.x1 - glyph->bbox.x0, 1.0f);
    float band_height            = maxf(glyph->bbox.y1 - glyph->bbox.y0, 1.0f);
    int   number_of_bands_height = clampi(num_curves, 1, SLUG_MAX_BANDS);
    int   number_of_bands_width  = clampi(num_curves, 1, SLUG_MAX_BANDS);

    xarr_setlen(glyph->horizontal_bands, number_of_bands_height);
    xarr_setlen(glyph->vertical_bands, number_of_bands_width);
    for (int i = 0; i < number_of_bands_height; i++)
    {
        glyph->horizontal_bands[i] = 0;
    }
    for (int i = 0; i < number_of_bands_width; i++)
    {
        glyph->vertical_bands[i] = 0;
    }
    glyph->band_scale  = vec2((float)number_of_bands_width / band_width, (float)number_of_bands_height / band_height);
    glyph->band_offset = vec2(-glyph->bbox.x0 * glyph->band_scale.x, -glyph->bbox.y0 * glyph->band_scale.y);

    float horizontal_band_height = band_height / (float)number_of_bands_height;
    float vertical_band_width    = band_width / (float)number_of_bands_width;
    float horizontal_pad         = horizontal_band_height * 0.5f;
    float vertical_pad           = vertical_band_width * 0.5f;

    int band_first, band_last;
    for (int curve_index = 0; curve_index < xarr_len(glyph->curves); curve_index++)
    {
        slug_curve_t* curve       = &glyph->curves[curve_index];
        float         curve_y_min = minf(minf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
        float         curve_y_max = maxf(maxf(curve->p[0].y, curve->p[1].y), curve->p[2].y);
        float         curve_x_min = minf(minf(curve->p[0].x, curve->p[1].x), curve->p[2].x);
        float         curve_x_max = maxf(maxf(curve->p[0].x, curve->p[1].x), curve->p[2].x);

        band_first = clampi(
            (int)floorf((curve_y_min - horizontal_pad - glyph->bbox.y0) / horizontal_band_height),
            0,
            number_of_bands_height - 1);
        band_last = clampi(
            (int)floorf((curve_y_max + horizontal_pad - glyph->bbox.y0) / horizontal_band_height),
            0,
            number_of_bands_height - 1);
        for (int i = band_first; i <= band_last; i++)
        {
            xarr_push(
                glyph->horizontal_bands[i],
                ((slug_band_entry_t){.curve_index = curve_index, .sort_key = curve_x_max}));
        }

        band_first = clampi(
            (int)floorf((curve_x_min - vertical_pad - glyph->bbox.x0) / vertical_band_width),
            0,
            number_of_bands_width - 1);
        band_last = clampi(
            (int)floorf((curve_x_max + vertical_pad - glyph->bbox.x0) / vertical_band_width),
            0,
            number_of_bands_width - 1);
        for (int i = band_first; i <= band_last; i++)
        {
            xarr_push(
                glyph->vertical_bands[i],
                ((slug_band_entry_t){.curve_index = curve_index, .sort_key = curve_y_max}));
        }
    }
    int num_horizontal_bands = (int)xarr_len(glyph->horizontal_bands);
    for (int i = 0; i < num_horizontal_bands; i++)
    {
        if (glyph->horizontal_bands[i])
        {
            size_t num_band_entries = xarr_len(glyph->horizontal_bands[i]);
            qsort(glyph->horizontal_bands[i], num_band_entries, sizeof(slug_band_entry_t), band_cmp);
        }
    }
    int num_vertical_bands = (int)xarr_len(glyph->vertical_bands);
    for (int i = 0; i < num_vertical_bands; i++)
    {
        if (glyph->vertical_bands[i])
        {
            size_t num_band_entries = xarr_len(glyph->vertical_bands[i]);
            qsort(glyph->vertical_bands[i], num_band_entries, sizeof(slug_band_entry_t), band_cmp);
        }
    }
}

static void pad_to_row_curve_pixels(pack_textures_t* res, int needed)
{
    int curlen = (int)xarr_len(res->curve_pixels);
    int column = curlen % SLUG_TEX_WIDTH;
    if ((column + needed) > SLUG_TEX_WIDTH)
    {
        xarr_setlen(res->curve_pixels, curlen + SLUG_TEX_WIDTH - column);
        int newlen = (int)xarr_len(res->curve_pixels);
        for (int i = curlen; i < newlen; i++)
        {
            res->curve_pixels[i] = (vec4_t){0};
        }
    }
}

static void finalize_curve_pixels(pack_textures_t* res)
{
    int cur_size = (int)xarr_len(res->curve_pixels);
    int new_size = 0;
    if (cur_size == 0)
    {
        xarr_setlen(res->curve_pixels, SLUG_TEX_WIDTH);
        new_size          = (int)xarr_len(res->curve_pixels);
        res->curve_height = 1;
    }
    else
    {
        res->curve_height = ((int)xarr_len(res->curve_pixels) + SLUG_TEX_WIDTH - 1) / SLUG_TEX_WIDTH;
        xarr_setlen(res->curve_pixels, res->curve_height * SLUG_TEX_WIDTH);
        new_size = (int)xarr_len(res->curve_pixels);
    }
    for (int i = cur_size; i < new_size; i++)
    {
        res->curve_pixels[i] = (vec4_t){0};
    }
}

static void pad_to_row_band_pixels(pack_textures_t* res, int needed)
{
    int curlen = (int)xarr_len(res->band_pixels);
    int column = curlen % SLUG_TEX_WIDTH;
    if ((column + needed) > SLUG_TEX_WIDTH)
    {
        xarr_setlen(res->band_pixels, curlen + SLUG_TEX_WIDTH - column);
        int newlen = (int)xarr_len(res->band_pixels);
        for (int i = curlen; i < newlen; i++)
        {
            res->band_pixels[i] = (u16vec2_t){0};
        }
    }
}

static void finalize_band_pixels(pack_textures_t* res)
{
    int cur_size = (int)xarr_len(res->band_pixels);
    int new_size = 0;
    if (cur_size == 0)
    {
        xarr_setlen(res->band_pixels, SLUG_TEX_WIDTH);
        new_size         = (int)xarr_len(res->band_pixels);
        res->band_height = 1;
    }
    else
    {
        res->band_height = ((int)xarr_len(res->band_pixels) + SLUG_TEX_WIDTH - 1) / SLUG_TEX_WIDTH;
        xarr_setlen(res->band_pixels, res->band_height * SLUG_TEX_WIDTH);
        new_size = (int)xarr_len(res->band_pixels);
    }
    for (int i = cur_size; i < new_size; i++)
    {
        res->band_pixels[i] = (u16vec2_t){0};
    }
}

static void write_band_set(
    slug_band_entry_t** bands,
    slug_curve_t*       curves,
    u16vec2_t*          pixels,
    int                 glyph_start,
    int                 header_offset,
    int*                write_offset)
{
    // Write headers: each band stores (count, data_offset) where data_offset
    // is relative to glyph_start, matching how the shader indexes into the texture.
    int data_offset = *write_offset;
    for (int band_index = 0; band_index < xarr_len(bands); band_index++)
    {
        slug_band_entry_t* band                           = bands[band_index];
        u16vec2_t          pixel                          = {(uint16_t)xarr_len(band), (uint16_t)data_offset};
        pixels[glyph_start + header_offset + band_index]  = pixel;
        data_offset                                      += (int)xarr_len(band);
    }
    // Write curve references at the offsets declared above
    data_offset = *write_offset;
    for (int band_index = 0; band_index < xarr_len(bands); band_index++)
    {
        slug_band_entry_t* band = bands[band_index];
        for (int entry_index = 0; entry_index < xarr_len(band); entry_index++)
        {
            slug_band_entry_t* entry           = &band[entry_index];
            slug_curve_t*      curve           = &curves[entry->curve_index];
            u16vec2_t          pixel           = {curve->texture[0], curve->texture[1]};
            pixels[glyph_start + data_offset]  = pixel;
            data_offset                       += 1;
        }
    }
    *write_offset = data_offset;
}

static pack_textures_t pack_textures(slug_glyph_build_t* glyphs, int num_glyphs)
{
    pack_textures_t res = {0};

    // Count how many texels we'll need so we can reserve upfront.
    // This avoids repeated realloc+copy as the dynamic arrays grow.
    int estimated_curve_size = 0;
    int estimated_band_size  = 0;
    for (int glyph_index = 0; glyph_index < num_glyphs; glyph_index++)
    {
        slug_glyph_build_t* glyph = &glyphs[glyph_index];
        // Each contour needs (count + 1) curve texels:
        //   count = number of bezier curves in this contour
        //   +1 for the shared endpoint texel
        for (int contour_index = 0; contour_index < xarr_len(glyph->contours); contour_index++)
        {
            slug_contour_range_t* contour  = &glyph->contours[contour_index];
            estimated_curve_size          += contour->count + 1;
        }
        int num_h = (int)xarr_len(glyph->horizontal_bands);
        int num_v = (int)xarr_len(glyph->vertical_bands);
        if ((num_h == 0) && (num_v == 0))
        {
            continue;
        }
        // Band data per glyph:
        //   num_h + num_v = one header texel per band
        //   + sum of all curve references in each band
        int band_size = num_h + num_v;
        for (int i = 0; i < xarr_len(glyph->horizontal_bands); i++)
        {
            band_size += (int)xarr_len(glyph->horizontal_bands[i]);
        }
        for (int i = 0; i < xarr_len(glyph->vertical_bands); i++)
        {
            band_size += (int)xarr_len(glyph->vertical_bands[i]);
        }
        estimated_band_size += band_size;
    }
    xarr_setcap(res.curve_pixels, (estimated_curve_size * 6) / 5);
    xarr_setcap(res.band_pixels, (estimated_band_size * 6) / 5);

    for (int glyph_index = 0; glyph_index < num_glyphs; glyph_index++)
    {
        slug_glyph_build_t* glyph = &glyphs[glyph_index];
        // Pack curves into texture, recording each curve's texture coordinates
        for (int contour_index = 0; contour_index < xarr_len(glyph->contours); contour_index++)
        {
            slug_contour_range_t* contour        = &glyph->contours[contour_index];
            int                   entries_needed = contour->count + 1;
            pad_to_row_curve_pixels(&res, entries_needed);
            for (int i = 0; i < contour->count; i++)
            {
                slug_curve_t* curve       = &glyph->curves[contour->start + i];
                int           pixel_index = (int)xarr_len(res.curve_pixels);
                xarr_push(res.curve_pixels, vec4(curve->p[0].x, curve->p[0].y, curve->p[1].x, curve->p[1].y));
                curve->texture[0] = (uint16_t)(pixel_index % SLUG_TEX_WIDTH);
                curve->texture[1] = (uint16_t)(pixel_index / SLUG_TEX_WIDTH);
            }
            slug_curve_t* last_curve = &glyph->curves[contour->start + contour->count - 1];
            xarr_push(res.curve_pixels, vec4(last_curve->p[2].x, last_curve->p[2].y, 0.0f, 0.0f));
        }

        // Pack band lookup tables into texture, referencing the curve coords set above
        int num_h_bands = (int)xarr_len(glyph->horizontal_bands);
        int num_v_bands = (int)xarr_len(glyph->vertical_bands);
        if ((num_h_bands == 0) && (num_v_bands == 0))
        {
            continue;
        }
        int header_size = num_h_bands + num_v_bands;
        pad_to_row_band_pixels(&res, header_size);

        int glyph_start     = (int)xarr_len(res.band_pixels);
        glyph->glyph_loc[0] = (int32_t)glyph_start % SLUG_TEX_WIDTH;
        glyph->glyph_loc[1] = (int32_t)glyph_start / SLUG_TEX_WIDTH;

        int total_entries = header_size;
        for (int i = 0; i < xarr_len(glyph->horizontal_bands); i++)
        {
            total_entries += (int)xarr_len(glyph->horizontal_bands[i]);
        }
        for (int i = 0; i < xarr_len(glyph->vertical_bands); i++)
        {
            total_entries += (int)xarr_len(glyph->vertical_bands[i]);
        }
        xarr_setlen(res.band_pixels, glyph_start + total_entries);

        int write_offset = header_size;
        write_band_set(glyph->horizontal_bands, glyph->curves, res.band_pixels, glyph_start, 0, &write_offset);
        write_band_set(glyph->vertical_bands, glyph->curves, res.band_pixels, glyph_start, num_h_bands, &write_offset);
    }
    finalize_curve_pixels(&res);
    finalize_band_pixels(&res);
    return res;
}
