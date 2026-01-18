#include "common.h"

#include "program_sdf_line_tiled.glsl.h"

#include <xhl/maths.h>
#include <xhl/time.h>

static struct
{
    int window_width;
    int window_height;

    sg_buffer      buf;
    sg_buffer      tiles;
    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
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

    state.pip = sg_make_pipeline(&(
        sg_pipeline_desc){.shader = sg_make_shader(line_stroke_shader_desc(sg_query_backend())), .label = "pipeline"});

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};

    state.buf   = sg_make_buffer(&(sg_buffer_desc){
          .usage.storage_buffer = true,
          .usage.stream_update  = true,
          .size                 = sizeof(AUDIO_BUFFER),
          .label                = "sine-buffer",
    });
    state.tiles = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(TILES),
        .label                = "tiles-buffer",
    });

    state.bind.views[VIEW_ssbo]           = sg_make_view(&(sg_view_desc){.storage_buffer = state.tiles});
    state.bind.views[VIEW_storage_buffer] = sg_make_view(&(sg_view_desc){.storage_buffer = state.buf});
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

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});

#ifdef __APPLE__
    // Note: this code was written with the assumption the app will only ever run on a macbook retina screen
    // Other devices will likely see bad results
    // Unfortunately this will cause our lovely 2px line to look like a 1px line. This is because we will now need to
    // read from twice the amount of pixels from the left and right
    // This means the shader will likely need to be rewritten to support macOS.
    // const int backingScaleFactor = 2;
    const int backingScaleFactor = 1;
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
            // v  = v * 0.5f + 0.5f;
            // v *= 0.8f;
            AUDIO_BUFFER[i] = v;
            // AUDIO_BUFFER[i] = (((i >> 4) & 3) >> 1) ? -1 : 1; // square
        }
    }
    // Create tiles
    {
        const int MAX_TILE_LEN = 16;
        for (int tile_begin_idx = 0; tile_begin_idx < N; tile_begin_idx += MAX_TILE_LEN)
        {
            int   tile_end_idx = xm_mini(N, tile_begin_idx + MAX_TILE_LEN);
            float max_v        = AUDIO_BUFFER[tile_begin_idx];
            float min_v        = AUDIO_BUFFER[tile_begin_idx];
            for (int i = tile_begin_idx + 1; i < tile_end_idx; i++)
            {
                float v = AUDIO_BUFFER[i];
                if (v > max_v)
                    max_v = v;
                if (v < min_v)
                    min_v = v;
            }
            if (tile_begin_idx - 1 > 0)
            {
                float v = AUDIO_BUFFER[tile_begin_idx - 1];
                if (v > max_v)
                    max_v = v;
                if (v < min_v)
                    min_v = v;
            }
            if (tile_end_idx + 1 < N)
            {
                float v = AUDIO_BUFFER[tile_end_idx + 1];
                if (v > max_v)
                    max_v = v;
                if (v < min_v)
                    min_v = v;
            }

            // make line
            if (TILES_LEN < ARRLEN(TILES))
            {
                // linetile_t* tile = &TILES[TILES_LEN];
                float x = tile_begin_idx;
                float r = tile_end_idx;
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
                    .stroke_width     = 2,
                };
                TILES_LEN++;
            }
        }
    }

    if (TILES_LEN)
    {
        if (N)
        {
            sg_update_buffer(state.buf, &(sg_range){.ptr = AUDIO_BUFFER, N * sizeof(AUDIO_BUFFER[0])});
        }

        sg_update_buffer(state.tiles, &(sg_range){.ptr = TILES, TILES_LEN * sizeof(TILES[0])});

        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);

        sg_draw(0, 6 * TILES_LEN, 1);
        sg_end_pass();
    }
}