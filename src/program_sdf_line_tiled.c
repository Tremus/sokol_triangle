#include "common.h"

#include "program_sdf_line_tiled.glsl.h"

#include <xhl/debug.h>
#include <xhl/maths.h>
#include <xhl/time.h>

static struct
{
    int window_width;
    int window_height;

    sg_buffer sbo_line_data;
    sg_view   sbv_line_data;

    struct
    {
        sg_buffer   sbo; // vertex pulling
        sg_view     sbv; // vertex pulling
        sg_pipeline pip;
    } tiles;

    sg_pipeline pip_overdraw;
} state = {0};

bool         init_buffer = false;
static float AUDIO_BUFFER[2048];

size_t     TILES_LEN = 0;
linetile_t TILES[512];

void program_setup()
{
    xtime_init();

    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    state.sbo_line_data = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(AUDIO_BUFFER),
        .label                = "sine-buffer",
    });
    state.sbv_line_data = sg_make_view(&(sg_view_desc){.storage_buffer = state.sbo_line_data});

    state.tiles.pip = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = sg_make_shader(line_stroke_tiled_shader_desc(sg_query_backend())),
                            .colors[0] =
                                {.write_mask = SG_COLORMASK_RGBA,
                                 .blend =
                                     {
                                         .enabled          = true,
                                         .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
                                         .src_factor_alpha = SG_BLENDFACTOR_ONE,
                                         .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                         .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                                     }},
                            .label = "tiles-pipeline"});

    state.tiles.sbo = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(TILES),
        .label                = "tiles-buffer",
    });
    state.tiles.sbv = sg_make_view(&(sg_view_desc){.storage_buffer = state.tiles.sbo});

    state.pip_overdraw = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = sg_make_shader(line_stroke_overdraw_shader_desc(sg_query_backend())),
                            .colors[0] =
                                {.write_mask = SG_COLORMASK_RGBA,
                                 .blend =
                                     {
                                         .enabled          = true,
                                         .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
                                         .src_factor_alpha = SG_BLENDFACTOR_ONE,
                                         .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                         .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                                     }},
                            .label = "overdraw-pipeline"});
}
void program_shutdown() {}

bool program_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.window_width  = e->resize.width;
        state.window_height = e->resize.height;
        init_buffer         = false;
    }
    return false;
}

void program_tick()
{
    TILES_LEN = 0;

    sg_begin_pass(&(sg_pass){
        .action    = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});

#ifdef __APPLE__
    // Note: this code was written with the assumption the app will only ever run on a macbook retina screen
    // Other devices will likely see bad results
    // Unfortunately this will cause our lovely 2px line to look like a 1px line. This is because we will now need to
    // read from twice the amount of pixels from the left and right
    // This means the shader will likely need to be rewritten to support macOS.
    const int backingScaleFactor = 2;
    // const int backingScaleFactor = 1;
#else
    const int backingScaleFactor = 1;
#endif
    const size_t N = state.window_width * backingScaleFactor;

    // Build line
    {
        // Animated sine wave
        // double now_sec = 0;
        double now_sec  = xtime_convert_ns_to_sec(xtime_now_ns());
        now_sec        *= 0.125 * 0.25; // 0.25hz

        const float phase = (now_sec - (int)now_sec) * XM_TAUf;
        const float inc   = XM_TAUf / (float)N * 8;
        for (unsigned i = 0; i < N; i++)
        {
            float v = sinf(phase + i * inc); // sin
            v       = v * -0.5f + 0.5f;      // normalise to [0-1], where 0 is bottom of screen
            // v *= 0.8f;
            AUDIO_BUFFER[i] = v;
            // AUDIO_BUFFER[i] = (((i >> 4) & 3) >> 1) ? -1 : 1; // square
        }
    }
    const bool draw_tile     = 1;
    const bool draw_overdraw = 0;
    // const float stroke_width  = 1.2f;
    const float stroke_width = 2.0f;
    // const uint32_t stroke_colour = 0xffffffff;
    const uint32_t stroke_colour = 0x7fffffff;
    const int      MAX_TILE_LEN  = 8 * backingScaleFactor;

    if (N)
    {
        sg_update_buffer(state.sbo_line_data, &(sg_range){.ptr = AUDIO_BUFFER, N * sizeof(AUDIO_BUFFER[0])});
    }

    if (draw_overdraw)
    {
        sg_apply_pipeline(state.pip_overdraw);
        sg_apply_bindings(&(sg_bindings){
            .views[VIEW_sbo_line_stroke_overdraw] = state.sbv_line_data,
        });

        const fs_uniforms_line_stroke_overdraw_t uniforms = {
            .u_view_size[0]  = state.window_width * backingScaleFactor,
            .u_view_size[1]  = state.window_height * backingScaleFactor,
            .u_stroke_width  = stroke_width,
            .u_buffer_length = N,
        };
        sg_apply_uniforms(UB_fs_uniforms_line_stroke_overdraw, &SG_RANGE(uniforms));

        sg_draw(0, 6, 1);
    }

    if (draw_tile)
    {
        // Create tiles
        {
            for (int tile_begin_idx = 0; tile_begin_idx < N; tile_begin_idx += MAX_TILE_LEN)
            {
                int tile_end_idx = xm_mini(tile_begin_idx + MAX_TILE_LEN, N);

                int   i_begin = xm_maxi(tile_begin_idx - 1, 0);
                int   i_end   = xm_mini(tile_end_idx + 1, N);
                float max_v   = AUDIO_BUFFER[i_begin];
                float min_v   = AUDIO_BUFFER[i_begin];
                for (int i = i_begin + 1; i < i_end; i++)
                {
                    float v = AUDIO_BUFFER[i];
                    if (v > max_v)
                        max_v = v;
                    if (v < min_v)
                        min_v = v;
                }

                // make line
                if (TILES_LEN < ARRLEN(TILES))
                {
                    // linetile_t* tile = &TILES[TILES_LEN];
                    xassert((tile_begin_idx % backingScaleFactor) == 0);
                    xassert((tile_end_idx % backingScaleFactor) == 0);
                    float x = tile_begin_idx / backingScaleFactor;
                    float r = tile_end_idx / backingScaleFactor;
                    float y = state.window_height - min_v * state.window_height;
                    float b = state.window_height - max_v * state.window_height;

                    TILES[TILES_LEN] = (linetile_t){
                        .topleft          = {x, y},
                        .bottomright      = {r, b},
                        .view_size        = {state.window_width, state.window_height},
                        .buffer_begin_idx = 0,
                        .buffer_end_idx   = N,
                        .tile_begin_idx   = tile_begin_idx,
                        .tile_end_idx     = tile_end_idx,
                        .stroke_width     = stroke_width,
                        .colour           = stroke_colour,
                    };
                    TILES_LEN++;
                }
            }
        }

        if (TILES_LEN)
        {
            size_t num_bytes = TILES_LEN * sizeof(TILES[0]);
            sg_update_buffer(state.tiles.sbo, &(sg_range){.ptr = TILES, .size = num_bytes});

            sg_apply_pipeline(state.tiles.pip);
            sg_apply_bindings(&(sg_bindings){
                .views[VIEW_sbo_tiles]             = state.tiles.sbv,
                .views[VIEW_sbo_line_stroke_tiled] = state.sbv_line_data,
            });

            sg_draw(0, 6 * TILES_LEN, 1);
        }
    }

    sg_end_pass();

    size_t size_line  = N * sizeof(AUDIO_BUFFER[0]);
    size_t size_tiles = TILES_LEN * sizeof(TILES[0]);
    println("Line: %zu bytes, Tiles: %zu bytes, Total: %zu bytes", size_line, size_tiles, (size_line + size_tiles));
}