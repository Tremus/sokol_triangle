//------------------------------------------------------------------------------
//  program_slug.c
//
//  Port of sokol-samples/sapp/slug-sapp.c, a demo of Eric Lyengel's Slug text
//  rendering algorithm: https://terathon.com/blog/decade-slug.html
//
//  Ported from sokol_app + Dear ImGui + sokol_fetch to this project's CPLUG
//  host contract (program_setup/tick/event/shutdown). Notable differences
//  from the original sample:
//  - No Dear ImGui in this project: the font-size slider and gfx/app debug
//    menus are dropped; font size is a fixed FONT_SIZE constant instead.
//  - No sokol_fetch: fonts are loaded synchronously via read_file() in
//    program_setup() instead of async fetch callbacks.
//  - PWEvent's mouse-move gives absolute x/y (not dx/dy like sapp_event), so
//    drag panning tracks the previous mouse position manually.
//  - PWEvent's mouse-wheel event repurposes mouse.x/mouse.y as the scroll
//    delta (see cplug_extensions/window_osx.m / window_win.c), used here as
//    the zoom delta instead of sapp_event's scroll_y.
//  - The shader's vs_params uniform was changed from a mat4 mvp (which would
//    have required vendoring vecmath.h's matrix type just for the uniform)
//    to a flat vec4 xform (sx, sy, tx, ty), matching the convention already
//    used by program_liquidglass.c of avoiding matrix uniforms.
//  - slugutil, vecmath (used internally by slugutil for vec2_t/vec4_t) and
//    stb_ds (dynamic arrays used by slugutil) are vendored into src/, same
//    as the original sample vendors them.
//------------------------------------------------------------------------------
#include "common.h"

#include "slugutil.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "program_slug.glsl.h"

#include <xhl/debug.h>
#include <xhl/files.h>
#include <xhl/maths.h>

#define MAX_DRAWN_GLYPHS  (16 * 1024)
#define MAX_DRAW_COMMANDS (128)
#define TOTAL_LINES       (6)
#define FONT_SIZE         (48.0f)
#define MIN_ZOOM          (0.1f)
#define MAX_ZOOM          (50.0f)

static uint32_t line[TOTAL_LINES][128];

// per-glyph data in glyph buffer, expanded 4x via hardware-instancing
typedef struct
{
    vec4_t   draw_rect;
    vec4_t   glyph_bbox;
    vec4_t   band_transform;
    int16_t  glyph_params[4];
    uint32_t color;
} glyph_vertex_t;

typedef struct
{
    int     base_instance;
    int     num_instances;
    sg_view curve_tex_view;
    sg_view band_tex_view;
} draw_command_t;

static glyph_vertex_t glyph_vertices[MAX_DRAWN_GLYPHS];
static draw_command_t draw_commands[MAX_DRAW_COMMANDS];

static struct
{
    uint32_t    width;
    uint32_t    height;
    sg_buffer   buf;
    sg_pipeline pip;
    sg_sampler  smp;
    struct
    {
        float zoom;
        float pan_x;
        float pan_y;
        bool  dragging;
        float prev_mouse_x;
        float prev_mouse_y;
    } inp;
    struct
    {
        slug_font_t cairo;
        slug_font_t lucide;
        slug_font_t twemoji;
    } fonts;
    struct
    {
        int                start_glyph_vertex;
        int                cur_glyph_vertex;
        int                cur_draw_command;
        const slug_font_t* cur_font;
    } draw;
} state;

static float measure_line(const slug_font_t* font, const uint32_t* text)
{
    float    total = 0.0f;
    uint32_t ucp;
    while ((ucp = *text++) != 0)
    {
        const slug_glyph_t* glyph = slug_get_glyph(font, ucp);
        if (glyph)
        {
            total += glyph->advance * FONT_SIZE;
        }
    }
    return total;
}

static void begin_push_glyphs(void)
{
    state.draw.start_glyph_vertex = 0;
    state.draw.cur_glyph_vertex   = 0;
    state.draw.cur_draw_command   = 0;
    state.draw.cur_font           = 0;
}

static void push_draw_command(void)
{
    if ((state.draw.cur_draw_command < MAX_DRAW_COMMANDS) &&
        (state.draw.cur_glyph_vertex > state.draw.start_glyph_vertex))
    {
        xassert(state.draw.cur_font);
        draw_commands[state.draw.cur_draw_command++] = (draw_command_t){
            .base_instance  = state.draw.start_glyph_vertex,
            .num_instances  = state.draw.cur_glyph_vertex - state.draw.start_glyph_vertex,
            .curve_tex_view = state.draw.cur_font->curve.tex_view,
            .band_tex_view  = state.draw.cur_font->band.tex_view,
        };
        state.draw.start_glyph_vertex = state.draw.cur_glyph_vertex;
    }
}

static void end_push_glyphs(void)
{
    // push final draw command
    push_draw_command();
    // update the glyph instance buffer
    sg_update_buffer(
        state.buf,
        &(sg_range){
            .ptr  = glyph_vertices,
            .size = state.draw.cur_glyph_vertex * sizeof(glyph_vertex_t),
        });
}

static void push_glyph_vertex(const glyph_vertex_t* v)
{
    if (state.draw.cur_glyph_vertex < MAX_DRAWN_GLYPHS)
    {
        glyph_vertices[state.draw.cur_glyph_vertex++] = *v;
    }
}

static uint32_t pack_color_u32(vec4_t color)
{
    uint32_t r = (uint32_t)(color.x * 255);
    uint32_t g = (uint32_t)(color.y * 255);
    uint32_t b = (uint32_t)(color.z * 255);
    uint32_t a = (uint32_t)(color.w * 255);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static void push_glyph(const slug_font_t* font, const slug_glyph_t* glyph, float x, float y, vec4_t color)
{
    if ((glyph->max_band_x < 0.0f) || (glyph->max_band_y < 0.0f))
    {
        return;
    }
    if (font != state.draw.cur_font)
    {
        if (state.draw.cur_font != 0)
        {
            push_draw_command();
        }
        state.draw.cur_font = font;
    }
    const glyph_vertex_t glyph_vertex = {
        .draw_rect =
            {
                x + (glyph->bbox.x0 * FONT_SIZE),
                y + (glyph->bbox.y0 * FONT_SIZE),
                (glyph->bbox.x1 - glyph->bbox.x0) * FONT_SIZE,
                (glyph->bbox.y1 - glyph->bbox.y0) * FONT_SIZE,
            },
        .glyph_bbox =
            {
                glyph->bbox.x0,
                glyph->bbox.y0,
                glyph->bbox.x1,
                glyph->bbox.y1,
            },
        .band_transform =
            {
                glyph->band_scale.x,
                glyph->band_scale.y,
                glyph->band_offset.x,
                glyph->band_offset.y,
            },
        .glyph_params =
            {
                glyph->glyph_loc[0],
                glyph->glyph_loc[1],
                (int)glyph->max_band_x,
                (int)glyph->max_band_y,
            },
        .color = pack_color_u32(color),
    };
    push_glyph_vertex(&glyph_vertex);
}

static void push_line(const slug_font_t* font, const uint32_t* text, float x, float y)
{
    uint32_t cp = 0;
    while ((cp = *text++) != 0)
    {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph)
        {
            push_glyph(font, glyph, x, y, (vec4_t){1.0f, 1.0f, 1.0f, 1.0f});
            x += glyph->advance * FONT_SIZE;
        }
    }
}

static void push_emoji(const slug_font_t* font, uint32_t codepoint, float x, float y)
{
    const slug_colr_base_t* colr_base = slug_find_colr_base(font, codepoint);
    if (colr_base == 0)
    {
        return;
    }
    // draw each layer as its own glyph
    for (uint16_t i = 0; i < colr_base->num_layers; i++)
    {
        slug_colr_layer_t* layer = &font->colr_layers[colr_base->first_layer + i];
        if (layer->glyph_id >= arrlen(font->glyphs))
        {
            continue;
        }
        slug_glyph_t* glyph = &font->glyphs[layer->glyph_id];
        vec4_t        color = {1.0f, 1.0f, 1.0f, 1.0f};
        if (layer->palette_index < arrlen(font->cpal_colors))
        {
            color = font->cpal_colors[layer->palette_index];
        }
        push_glyph(font, glyph, x, y, color);
    }
}

static void push_line_emoji(const slug_font_t* font, const uint32_t* text, float x, float y)
{
    uint32_t cp = 0;
    while ((cp = *text++) != 0)
    {
        const slug_glyph_t* glyph = slug_get_glyph(font, cp);
        if (glyph)
        {
            push_emoji(font, cp, x, y);
            x += glyph->advance * FONT_SIZE;
        }
    }
}

static void push_centered_line(const slug_font_t* font, const uint32_t* text, int line_nr)
{
    const float line_height  = FONT_SIZE * 1.5f;
    const float block_height = (float)TOTAL_LINES * line_height;
    float       line_width   = measure_line(font, text);
    float       base_x       = ((float)state.width - line_width) * 0.5f;
    float       base_y       = ((float)state.height + block_height) * 0.5f - (float)line_nr * line_height;
    push_line(font, text, base_x, base_y);
}

static void push_centered_line_emoji(const slug_font_t* font, const uint32_t* text, int line_nr)
{
    const float line_height  = FONT_SIZE * 1.5f;
    const float block_height = (float)TOTAL_LINES * line_height;
    float       line_width   = measure_line(font, text);
    float       base_x       = ((float)state.width - line_width) * 0.5f;
    float       base_y       = ((float)state.height + block_height) * 0.5f - (float)line_nr * line_height;
    push_line_emoji(font, text, base_x, base_y);
}

static void load_font(slug_font_t* font, const char* path)
{
    XFile file = read_file(path);
    xassert(file.data);
    if (file.data)
    {
        slug_load_font(font, &(slug_range_t){.ptr = file.data, .size = file.size});
    }
}

void program_setup()
{
    state.width    = APP_WIDTH;
    state.height   = APP_HEIGHT;
    state.inp.zoom = 1.0f;

    // populate unicode lines, first the latin characters
    for (uint32_t i = 0; i < 32; i++)
    {
        line[0][i] = 0x40 + i;
        if (i != 31)
        {
            line[1][i] = 0x60 + i;
        }
        line[2][i] = 0x20 + i;
    }
    // the arabic alphabet
    for (uint32_t i = 0, cp = 0x0627; cp <= 0x064A; cp++)
    {
        if ((cp < 0x063B) || (cp > 0x063F))
        {
            line[3][i++] = cp;
        }
    }
    // icons from lucide font, note that the unicode assignment
    // of icons is very messy, it has gaps and duplicates. The
    // range picked here is somewhat 'orderly'.
    for (uint32_t i = 0, cp = 0xE29A; cp <= 0xE2BA; cp++)
    {
        line[4][i++] = cp;
    }
    // emojis
    line[5][0]  = 0x1F600; // grinning face
    line[5][1]  = 0x1F60D; // heart eyes
    line[5][2]  = 0x1F60E; // sunglasses
    line[5][3]  = 0x1F525; // fire
    line[5][4]  = 0x1F44D; // thumbs up
    line[5][5]  = 0x1F389; // party popper
    line[5][6]  = 0x1F680; // rocket
    line[5][7]  = 0x2764;  // red heart
    line[5][8]  = 0x1F308; // rainbow
    line[5][9]  = 0x1F31F; // glowing star
    line[5][10] = 0x1F3B5; // musical note
    line[5][11] = 0x1F40D; // snake
    line[5][12] = 0x1F436; // dog face
    line[5][13] = 0x1F431; // cat face
    line[5][14] = 0x1F34E; // red apple
    line[5][15] = 0x1F370; // shortcake

    // A stream-update buffer which holds one glyph_vertex_t per glyph, this
    // is expanded 4x via hardware instancing with the 4 corner vertex positions
    // synthesized in the vertex shader.
    state.buf = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size                = MAX_DRAWN_GLYPHS * sizeof(glyph_vertex_t),
        .label               = "slug-glyph-buffer",
    });

    // the pipeline is configured with a single instance-stepped buffer which
    // provides the per-glyph data, also note that rendering is non-indexed
    // and each glyph is rendered as a 4-vertex triangle-strip
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(slug_shader_desc(sg_query_backend())),
        .layout =
            {
                .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                .attrs =
                    {
                        [ATTR_slug_draw_rect]         = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug_glyph_bbox]        = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug_in_band_transform] = {.format = SG_VERTEXFORMAT_FLOAT4, .buffer_index = 0},
                        [ATTR_slug_in_glyph_params]   = {.format = SG_VERTEXFORMAT_SHORT4, .buffer_index = 0},
                        [ATTR_slug_in_text_color]     = {.format = SG_VERTEXFORMAT_UBYTE4N, .buffer_index = 0},
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
        .label = "slug-pipeline",
    });
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
        .label      = "slug-sampler",
    });

    load_font(&state.fonts.cairo, SRC_DIR XFILES_DIR_STR "Cairo.ttf");
    load_font(&state.fonts.lucide, SRC_DIR XFILES_DIR_STR "lucide.ttf");
    load_font(&state.fonts.twemoji, SRC_DIR XFILES_DIR_STR "twemoji.ttf");
}

void program_shutdown()
{
    slug_unload_font(&state.fonts.cairo);
    slug_unload_font(&state.fonts.lucide);
    slug_unload_font(&state.fonts.twemoji);
}

void program_tick()
{
    float             sx        = 2.0f / ((float)state.width / state.inp.zoom);
    float             sy        = 2.0f / ((float)state.height / state.inp.zoom);
    float             tx        = -1.0f - state.inp.pan_x * sx;
    float             ty        = -1.0f - state.inp.pan_y * sy;
    const vs_params_t vs_params = {
        .xform = {sx, sy, tx, ty},
    };

    // record text into glyph buffer and draw commands
    bool any_valid = state.fonts.cairo.valid || state.fonts.lucide.valid || state.fonts.twemoji.valid;
    if (any_valid)
    {
        begin_push_glyphs();
        if (state.fonts.cairo.valid)
        {
            for (int i = 0; i < 4; i++)
            {
                push_centered_line(&state.fonts.cairo, line[i], i);
            }
        }
        if (state.fonts.lucide.valid)
        {
            push_centered_line(&state.fonts.lucide, line[4], 4);
        }
        if (state.fonts.twemoji.valid)
        {
            push_centered_line_emoji(&state.fonts.twemoji, line[5], 5);
        }
        end_push_glyphs();
    }

    sg_begin_pass(&(sg_pass){
        .action    = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.1f, 0.1f, 0.1f, 1.0f}}},
        .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8),
    });
    if (any_valid)
    {
        sg_apply_pipeline(state.pip);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
        for (int i = 0; i < state.draw.cur_draw_command; i++)
        {
            const draw_command_t* cmd = &draw_commands[i];
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0]        = state.buf,
                .vertex_buffer_offsets[0] = cmd->base_instance * sizeof(glyph_vertex_t),
                .views =
                    {
                        [VIEW_band_tex]  = cmd->band_tex_view,
                        [VIEW_curve_tex] = cmd->curve_tex_view,
                    },
                .samplers[SMP_point_sampler] = state.smp,
            });
            sg_draw(0, 6, cmd->num_instances);
        }
    }
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
    case PW_EVENT_MOUSE_WHEEL:
    case PW_EVENT_MOUSE_TOUCHPAD_BEGIN:
    case PW_EVENT_MOUSE_TOUCHPAD_MOVE:
    {
        float scroll_scale   = -1.0f;
        float h              = (float)state.height;
        float mx             = state.inp.prev_mouse_x;
        float my             = state.inp.prev_mouse_y;
        float mouse_world_x  = state.inp.pan_x + mx / state.inp.zoom;
        float mouse_world_y  = state.inp.pan_y + (h - my) / state.inp.zoom;
        state.inp.zoom      *= 1.0f + event->mouse.y * scroll_scale * 0.001f;
        state.inp.zoom       = xm_clampf(state.inp.zoom, MIN_ZOOM, MAX_ZOOM);
        // Adjust pan so the world point under the mouse stays fixed
        state.inp.pan_x = mouse_world_x - mx / state.inp.zoom;
        state.inp.pan_y = mouse_world_y - (h - my) / state.inp.zoom;
        break;
    }
    case PW_EVENT_MOUSE_LEFT_DOWN:
        state.inp.dragging     = true;
        state.inp.prev_mouse_x = event->mouse.x;
        state.inp.prev_mouse_y = event->mouse.y;
        break;
    case PW_EVENT_MOUSE_LEFT_UP:
    case PW_EVENT_MOUSE_EXIT:
        state.inp.dragging = false;
        break;
    case PW_EVENT_MOUSE_MOVE:
        if (state.inp.dragging)
        {
            float dx         = event->mouse.x - state.inp.prev_mouse_x;
            float dy         = event->mouse.y - state.inp.prev_mouse_y;
            state.inp.pan_x -= dx / state.inp.zoom;
            state.inp.pan_y += dy / state.inp.zoom;
        }
        state.inp.prev_mouse_x = event->mouse.x;
        state.inp.prev_mouse_y = event->mouse.y;
        break;
    default:
        break;
    }
    return false;
}
