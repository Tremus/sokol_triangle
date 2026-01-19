#include "common.h"

#include "program_3d_wavetable_stroked.glsl.h"

#include <string.h>
#include <xhl/debug.h>
#include <xhl/maths.h>

// application state
static struct
{
    unsigned width;
    unsigned height;

    sg_buffer   sbo_line_data;
    sg_view     sbv_line_data;
    sg_buffer   sbo_tiles; // vertex pulling
    sg_view     sbv_tiles; // vertex pulling
    sg_pipeline pip_tiles;

    sg_pipeline pip_ui;

    float param_value;
    bool  mouse_down_slider;
    float slider_left;
    float slider_top;
    float slider_right;
    float slider_bottom;
} state = {0};

enum
{
    WT_LEN = 2048
};
typedef struct Wavetable
{
    float data[WT_LEN];
} Wavetable;

static Wavetable WAVETABLES[256] = {0};
size_t           TILES_LEN       = 0;
linetile_t       TILES[512]      = {0};

void set_slider_boundaries()
{
    // TODO: set boundaries

    state.slider_bottom = state.height - 40;
    state.slider_top    = state.height - 80;
    state.slider_left   = 100;
    state.slider_right  = state.width - 100;
}

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    // INIT AUDIO DATA
    {
        // Do saw wave
        Wavetable* wt  = &WAVETABLES[0];
        float      inc = 2.0f / ARRLEN(wt->data);
        for (int i = 0; i < ARRLEN(wt->data); i++)
            wt->data[i] = 1.0f - i * inc;

        // TODO: apply moving filter across wavetables? eg. lowpass, simple RBJ
        for (int i = 1; i < ARRLEN(WAVETABLES); i++)
        {
            Wavetable* wt2 = &WAVETABLES[i];
            memcpy(wt2, wt, sizeof(*wt2));
        }
    }

    state.sbo_line_data = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        // .usage.stream_update  = true,
        .size  = sizeof(WAVETABLES),
        .data  = SG_RANGE(WAVETABLES),
        .label = "sine-buffer",
    });
    state.sbv_line_data = sg_make_view(&(sg_view_desc){.storage_buffer = state.sbo_line_data});

    state.pip_tiles = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(line_stroke_tiled_shader_desc(sg_query_backend())),
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
        .label = "tiles-pipeline                                                                        "});

    state.sbo_tiles = sg_make_buffer(&(sg_buffer_desc){
        .usage.storage_buffer = true,
        .usage.stream_update  = true,
        .size                 = sizeof(TILES),
        .label                = "tiles-buffer",
    });
    state.sbv_tiles = sg_make_view(&(sg_view_desc){.storage_buffer = state.sbo_tiles});

    state.pip_ui = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(quad_novbuf_shader_desc(sg_query_backend())),
        .colors[0] =
            {
                .write_mask = SG_COLORMASK_RGBA,
                .blend =
                    {
                        .enabled          = true,
                        .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
                        .src_factor_alpha = SG_BLENDFACTOR_ONE,
                        .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                        .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                    },
            },
        .label = "quad-pipeline                                                                    "});

    state.param_value = 0.5f;

    set_slider_boundaries();
}
void program_shutdown() {}

bool program_event(const PWEvent* event)
{
    if (event->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width  = event->resize.width;
        state.height = event->resize.height;

        set_slider_boundaries();
    }
    if (event->type == PW_EVENT_MOUSE_LEFT_UP)
        state.mouse_down_slider = false;
    if (event->type == PW_EVENT_MOUSE_LEFT_DOWN)
    {
        // hittest
        bool hit = event->mouse.x >= state.slider_left && event->mouse.x < state.slider_right &&
                   event->mouse.y >= state.slider_top && event->mouse.y < state.slider_bottom;
        state.mouse_down_slider = hit;
    }
    if (event->type == PW_EVENT_MOUSE_MOVE)
    {
        if (state.mouse_down_slider)
        {
            float next_param  = xm_normf(event->mouse.x, state.slider_left, state.slider_right);
            state.param_value = xm_clampf(next_param, 0, 1);
        }
    }
    return false;
}

void program_tick()
{
    sg_pass_action pass_action = {
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
    sg_begin_pass(&(sg_pass){.action = pass_action, .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});

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

    // Draw wavetables
    {
        // int num_wavetables = 8;
        int            num_wavetables = 1;
        int            MAX_TILE_LEN   = WT_LEN;
        const float    stroke_width   = 1.2f;
        const uint32_t stroke_colour  = 0xff0000ff;

        // TODO: calculate these values based on the number of wavetables
        // The series of wavetables will probably want a fixed spread vertically and horizontally
        float x_begin        = 0;
        float y_begin        = 0;
        float x_displacement = 0;
        float y_displacement = 0;

        for (int wt_idx = 0; wt_idx < num_wavetables; wt_idx++)
        {
            int pt_idx = wt_idx * WT_LEN;

            Wavetable* wt = &WAVETABLES[wt_idx];

            for (int tile_begin_idx = 0; tile_begin_idx < WT_LEN; tile_begin_idx += MAX_TILE_LEN)
            {
                int tile_end_idx = xm_mini(tile_begin_idx + MAX_TILE_LEN, WT_LEN);

                int   i_begin = xm_maxi(tile_begin_idx - 1, 0);
                int   i_end   = xm_mini(tile_end_idx + 1, WT_LEN);
                float max_v   = 1;
                float min_v   = -1;
                // float max_v   = wt->data[i_begin];
                // float min_v   = wt->data[i_begin];
                // for (int i = i_begin + 1; i < i_end; i++)
                // {
                //     float v = AUDIO_BUFFER[i];
                //     if (v > max_v)
                //         max_v = v;
                //     if (v < min_v)
                //         min_v = v;
                // }

                // make line
                if (TILES_LEN < ARRLEN(TILES))
                {
                    // linetile_t* tile = &TILES[TILES_LEN];
                    xassert((tile_begin_idx % backingScaleFactor) == 0);
                    xassert((tile_end_idx % backingScaleFactor) == 0);

                    float x = 0;
                    float r = state.width;
                    float y = state.height - min_v * state.height;
                    float b = state.height - max_v * state.height;

                    TILES[TILES_LEN] = (linetile_t){
                        .topleft          = {x, y},
                        .bottomright      = {r, b},
                        .view_size        = {state.width, state.height},
                        .buffer_begin_idx = 0,
                        .buffer_end_idx   = WT_LEN,
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
            sg_update_buffer(state.sbo_tiles, &(sg_range){.ptr = TILES, .size = num_bytes});

            sg_apply_pipeline(state.pip_tiles);
            sg_apply_bindings(&(sg_bindings){
                .views[VIEW_sbo_tiles]             = state.sbv_tiles,
                .views[VIEW_sbo_line_stroke_tiled] = state.sbv_line_data,
            });

            sg_draw(0, 6 * TILES_LEN, 1);
        }
    }

    // Draw UI
    {
        sg_apply_pipeline(state.pip_ui);

        unsigned half_width     = state.width / 2;
        unsigned quarter_width  = state.width / 4;
        unsigned half_height    = state.height / 2;
        unsigned quarter_height = state.height / 4;

        // Param bg
        {
            uint32_t      col      = 0x303030ff;
            vs_position_t uniforms = {
                .u_topleft      = {state.slider_left, state.slider_top},
                .u_bottomright  = {state.slider_right, state.slider_bottom},
                .u_size         = {state.width, state.height},
                .u_colour_left  = col,
                .u_colour_right = col,
                .u_param        = 1.0f,
            };
            sg_apply_uniforms(UB_vs_position, &SG_RANGE(uniforms));
            sg_draw(0, 6, 1);
        }
        // Param value
        {
            vs_position_t uniforms = {
                .u_topleft      = {state.slider_left, state.slider_top},
                .u_bottomright  = {state.slider_right, state.slider_bottom},
                .u_size         = {state.width, state.height},
                .u_colour_left  = 0x00ffffff,
                .u_colour_right = 0xff00ffff,
                .u_param        = state.param_value,
            };
            sg_apply_uniforms(UB_vs_position, &SG_RANGE(uniforms));
            sg_draw(0, 6, 1);
        }
    }
    sg_end_pass();
}