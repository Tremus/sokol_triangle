#include "common.h"

#include "program_liquidglass.glsl.h"

#include <xhl/files.h>
#include <xhl/maths.h>

typedef struct UIRect
{
    float left;
    float top;
    float right;
    float bottom;
} UIRect;

typedef struct UISlider
{
    UIRect bounds;
    float  value;
} UISlider;

enum
{
    SLIDER_RADIUS,
    SLIDER_REFRACTION,
    SLIDER_COUNT,
};

// application state
static struct
{
    uint32_t width;
    uint32_t height;

    Image img;

    sg_pipeline pip_liquidglass;
    sg_pipeline pip_ui;

    sg_sampler smp_linear;

    int mouse_down_idx;

    UISlider sliders[SLIDER_COUNT];
} state;

void set_boundaries()
{
    float x_padding = xm_maxf(state.width * 0.15, 20);
    float y_padding = xm_maxf(state.height * 0.05, 20);

    float slider_height = 20;

    float y = state.height - y_padding;
    for (int i = ARRLEN(state.sliders); i-- > 0;)
    {
        state.sliders[i].bounds.bottom = y;
        state.sliders[i].bounds.top    = y - slider_height;
        state.sliders[i].bounds.left   = x_padding;
        state.sliders[i].bounds.right  = state.width - x_padding;

        y -= y_padding * 0.5f + slider_height;
    }
}

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    static const char* path_brain_jpg = SRC_DIR XFILES_DIR_STR "brain.jpg";
    static const char* path_n64_png   = SRC_DIR XFILES_DIR_STR "n64.png";

    state.img = load_image_file(path_brain_jpg);

    state.pip_liquidglass = sg_make_pipeline(&(sg_pipeline_desc){
        .shader    = sg_make_shader(texquad_shader_desc(sg_query_backend())),
        .colors[0] = get_blending(),
        .label     = "quad-pipeline"});

    state.smp_linear = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

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

    state.mouse_down_idx = -1;
    // for (int i = 0; i < ARRLEN(state.sliders); i++)
    //     state.sliders[i].value = 0.5f;
    state.sliders[SLIDER_RADIUS].value     = 0.25;
    state.sliders[SLIDER_REFRACTION].value = 0.5;
    set_boundaries();
}
void program_shutdown() {}

bool program_event(const PWEvent* event)
{
    if (event->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width  = event->resize.width;
        state.height = event->resize.height;

        set_boundaries();
    }
    if (event->type == PW_EVENT_MOUSE_LEFT_UP)
        state.mouse_down_idx = -1;
    if (event->type == PW_EVENT_MOUSE_LEFT_DOWN)
    {
        for (int i = 0; i < ARRLEN(state.sliders); i++)
        {
            UISlider* sl = &state.sliders[i];
            // hittest
            bool hit = event->mouse.x >= sl->bounds.left && event->mouse.x < sl->bounds.right &&
                       event->mouse.y >= sl->bounds.top && event->mouse.y < sl->bounds.bottom;
            if (hit)
            {
                state.mouse_down_idx = i;
                break;
            }
        }
    }
    if (event->type == PW_EVENT_MOUSE_MOVE)
    {
        if (state.mouse_down_idx >= 0)
        {
            UISlider* sl         = &state.sliders[state.mouse_down_idx];
            float     next_param = xm_normf(event->mouse.x, sl->bounds.left, sl->bounds.right);
            sl->value            = xm_clampf(next_param, 0, 1);
        }
    }
    return false;
}

void program_tick()
{
    // Liquid glass
    {
        sg_begin_pass(&(sg_pass){
            .action    = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)});
        sg_apply_pipeline(state.pip_liquidglass);
        sg_apply_bindings(&(sg_bindings){
            .samplers[SMP_smp] = state.smp_linear,
            .views[VIEW_tex]   = state.img.texview,
        });

        fs_liquidglass_t uniforms = {
            .u_radius = state.sliders[SLIDER_RADIUS].value,
            // .u_refraction = 0.9f,
            .u_refraction = state.sliders[SLIDER_REFRACTION].value,

            .u_size = {state.width, state.height},
        };
        sg_apply_uniforms(UB_fs_liquidglass, &SG_RANGE(uniforms));
        sg_draw(0, 3, 1);
    }

    // UI
    {
        sg_apply_pipeline(state.pip_ui);

        for (int i = 0; i < ARRLEN(state.sliders); i++)
        {
            UISlider* sl = &state.sliders[i];
            // Param bg
            {
                uint32_t      col      = 0x303030ff;
                vs_position_t uniforms = {
                    .u_topleft      = {sl->bounds.left, sl->bounds.top},
                    .u_bottomright  = {sl->bounds.right, sl->bounds.bottom},
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
                    .u_topleft      = {sl->bounds.left, sl->bounds.top},
                    .u_bottomright  = {sl->bounds.right, sl->bounds.bottom},
                    .u_size         = {state.width, state.height},
                    .u_colour_left  = 0x00ffffff,
                    .u_colour_right = 0xff00ffff,
                    .u_param        = sl->value,
                };
                sg_apply_uniforms(UB_vs_position, &SG_RANGE(uniforms));
                sg_draw(0, 6, 1);
            }
        }
    }

    sg_end_pass();
}