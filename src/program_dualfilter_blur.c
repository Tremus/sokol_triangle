#include "common.h"

#include <xhl/debug.h>
#include <xhl/files.h>

#include "sokol_gfx.h"
#include "sokol_glue.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "program_dualfilter_blur.h"

// Kawase blur https://www.youtube.com/watch?v=ml-5OGZC7vE
// Dual filter blur. Marius Bjorge - Bandwith-Efficient Rendering, Siggraph 2015
// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf
// Cheapest operation I've seen so far.
// Seems tricky to adapt into a variable blur
// It may be possible to place iterations of the Kawase blur in the right place while upsampling,
// or to increase the pixel offset on certain stages of the downsample/upsample chain

typedef struct
{
    float   x, y;
    int16_t u, v;
} vertex_t;

typedef struct Framebuffer
{
    sg_image       img;
    sg_attachments att;
    int            width;
    int            height;
} Framebuffer;

Framebuffer make_framebuffer(int width, int height)
{
    Framebuffer rt = {0};
    xassert(width > 0);
    xassert(height > 0);

    sg_image img_colour = sg_make_image(&(sg_image_desc){
        .usage.render_attachment = true,
        .width                   = width,
        .height                  = height,
        .pixel_format            = SG_PIXELFORMAT_BGRA8,
        .sample_count            = 1,
        .label                   = "Framebuffer image"});

    rt.img = img_colour;
    rt.att = sg_make_attachments(&(sg_attachments_desc){.colors[0].image = rt.img, .label = "Framebuffer attachment"});
    rt.width  = width;
    rt.height = height;

    return rt;
}

typedef struct Image
{
    sg_image img;
    int      width;
    int      height;
} Image;

bool load_image_file(const char* path, Image* img)
{
    int            comp;
    unsigned char* data = stbi_load(path, &img->width, &img->height, &comp, STBI_rgb_alpha);
    print("%s. size %dx%d", path, img->width, img->height);
    xassert(data);

    img->img = sg_make_image(&(sg_image_desc){
        .width               = img->width,
        .height              = img->height,
        .pixel_format        = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = {
            .ptr  = data,
            .size = 4 * img->width * img->height,
        }});

    free(data);

    return true;
}

// application state
static struct
{
    uint32_t width;
    uint32_t height;

    Image brain;
    Image n64;

    Framebuffer fb_dualfilter_stages[4];

    Framebuffer fb_blur[2];

    sg_pipeline pip_lightness_filter;
    sg_pipeline pip_downsample;
    sg_pipeline pip_upsample;
    sg_pipeline pip_blur;
    sg_pipeline pip_bloom;
    sg_pipeline pip_texquad_framebuffer;
    sg_pipeline pip_texquad_swapchain;

    sg_sampler smp_linear;
    sg_sampler smp_nearest;
} state = {0};

void program_setup()
{
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    static const char* path_brain_jpg = SRC_DIR XFILES_DIR_STR "brain.jpg";
    static const char* path_n64_png   = SRC_DIR XFILES_DIR_STR "n64.png";

    load_image_file(path_brain_jpg, &state.brain);
    load_image_file(path_n64_png, &state.n64);

    _Static_assert(ARRLEN(state.fb_dualfilter_stages) > 1);
    for (int i = 0; i < ARRLEN(state.fb_dualfilter_stages); i++)
    {
        // /= 1, 2, 4, 8 etc
        int w = state.width >> i;
        int h = state.height >> i;

        state.fb_dualfilter_stages[i] = make_framebuffer(w, h);
    }

    state.fb_blur[0] = make_framebuffer(state.width / 4, state.height / 4);
    state.fb_blur[1] = make_framebuffer(state.width / 4, state.height / 4);

    state.pip_lightness_filter = sg_make_pipeline(&(sg_pipeline_desc){
        .shader                 = sg_make_shader(lightfilter_shader_desc(sg_query_backend())),
        .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
        .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});
    state.pip_downsample       = sg_make_pipeline(&(sg_pipeline_desc){
              .shader                 = sg_make_shader(downsample_shader_desc(sg_query_backend())),
              .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
              .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});
    state.pip_upsample         = sg_make_pipeline(&(sg_pipeline_desc){
                .shader                 = sg_make_shader(upsample_shader_desc(sg_query_backend())),
                .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
                .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});
    state.pip_blur             = sg_make_pipeline(&(sg_pipeline_desc){
                    .shader                 = sg_make_shader(kawase_blur_shader_desc(sg_query_backend())),
                    .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
                    .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});

    state.pip_texquad_framebuffer = sg_make_pipeline(&(sg_pipeline_desc){
        .shader                 = sg_make_shader(texquad_shader_desc(sg_query_backend())),
        .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
        .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});
    state.pip_texquad_swapchain   = sg_make_pipeline(&(sg_pipeline_desc){
          .shader = sg_make_shader(texquad_shader_desc(sg_query_backend())),
        //   .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
          .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});
    state.pip_bloom               = sg_make_pipeline(&(sg_pipeline_desc){
                      .shader = sg_make_shader(bloom_shader_desc(sg_query_backend())),
        //   .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
                      .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8});

    state.smp_linear  = sg_make_sampler(&(sg_sampler_desc){
         .min_filter = SG_FILTER_LINEAR,
         .mag_filter = SG_FILTER_LINEAR,
    });
    state.smp_nearest = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
    }
}

void do_dualfilter(Framebuffer fb[], size_t N)
{
    for (int i = 0; i < N - 1; i++)
    {
        Framebuffer* a = fb + i;
        Framebuffer* b = fb + i + 1;

        sg_begin_pass(&(sg_pass){
            .action = {.colors[0] = {.load_action = SG_LOADACTION_DONTCARE, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .attachments = b->att,
        });
        sg_apply_pipeline(state.pip_downsample);
        sg_apply_bindings(&(sg_bindings){
            .images[0]   = a->img,
            .samplers[0] = state.smp_linear,
        });
        float           halfpixel_x = 0.5f / a->width;
        float           halfpixel_y = 0.5f / a->height;
        fs_downsample_t uniforms    = {.u_offset = {halfpixel_x, halfpixel_y}};
        sg_apply_uniforms(UB_fs_downsample, &SG_RANGE(uniforms));
        sg_draw(0, 3, 1);
        sg_end_pass();
    }

    for (int i = N - 2; i >= 0; i--)
    {
        Framebuffer* a = fb + i + 1;
        Framebuffer* b = fb + i;

        sg_begin_pass(&(sg_pass){
            .action      = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .attachments = b->att,
        });
        sg_apply_pipeline(state.pip_upsample);
        sg_apply_bindings(&(sg_bindings){
            .images[0]   = a->img,
            .samplers[0] = state.smp_linear,
        });
        float           halfpixel_x = 0.5f / a->width;
        float           halfpixel_y = 0.5f / a->height;
        fs_downsample_t uniforms    = {.u_offset = {halfpixel_x, halfpixel_y}};
        sg_apply_uniforms(UB_fs_downsample, &SG_RANGE(uniforms));
        sg_draw(0, 3, 1);
        sg_end_pass();
    }
}

void program_tick()
{
    const bool apply_lightfilter = false;
    const bool apply_bloom       = false;

    const float lightfilter_threshold = 0.7;
    const float bloom_amount          = 0.5;

    sg_image src_img = state.n64.img; // colourful
    // sg_image src_img = state.brain.img; // greyscale

    {
        sg_begin_pass(&(sg_pass){
            .action      = {.colors[0] = {.load_action = SG_LOADACTION_DONTCARE}},
            .attachments = state.fb_dualfilter_stages[0].att,
        });
        if (apply_lightfilter)
            sg_apply_pipeline(state.pip_lightness_filter);
        else
            sg_apply_pipeline(state.pip_texquad_framebuffer);

        sg_apply_bindings(&(sg_bindings){
            .images[0]   = src_img,
            .samplers[0] = state.smp_nearest,
        });
        if (apply_lightfilter)
        {
            fs_lightfilter_t lightfilter_uniforms = {.u_threshold = lightfilter_threshold};
            sg_apply_uniforms(UB_fs_lightfilter, &SG_RANGE(lightfilter_uniforms));
        }
        sg_draw(0, 3, 1);
        sg_end_pass();
    }

    size_t num_stages = ARRLEN(state.fb_dualfilter_stages);
    do_dualfilter(state.fb_dualfilter_stages, ARRLEN(state.fb_dualfilter_stages));

    // Do blur loop (kawase filter)
    // const int N = 2;
    // int       i;
    // for (i = 0; i < N; i++)
    // {
    //     sg_image       read_img  = state.fb_blur[i & 1].img;
    //     sg_attachments write_att = state.fb_blur[(i + 1) & 1].att;

    //     sg_begin_pass(&(sg_pass){
    //         .action      = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f,
    //         0.0f, 1.0f}}}, .attachments = write_att});
    //     sg_apply_pipeline(state.pip_blur);
    //     sg_apply_bindings(&(sg_bindings){
    //         .images[0]         = read_img,
    //         .samplers[0]       = state.smp_linear,
    //     });
    //     float     offset   = (float)i + 0.5f;
    //     fs_blur_t uniforms = {.u_offset = {offset / state.fb_blur[0].width, offset / state.fb_blur[0].height}};

    //     sg_apply_uniforms(UB_fs_blur, &SG_RANGE(uniforms));
    //     sg_draw(0, 3, 1);
    //     sg_end_pass();
    // }

    // Draw to swapchain
    sg_begin_pass(&(sg_pass){
        .action    = {.colors[0] = {.load_action = SG_LOADACTION_DONTCARE, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
        .swapchain = sglue_swapchain()});

    if (apply_bloom)
        sg_apply_pipeline(state.pip_bloom);
    else // blur
        sg_apply_pipeline(state.pip_texquad_swapchain);

    sg_apply_bindings(&(sg_bindings){
        .images[0]   = state.fb_dualfilter_stages[0].img,
        .images[1]   = src_img,
        .samplers[0] = state.smp_nearest,
    });
    if (apply_bloom)
    {
        fs_bloom_t bloom_uniforms = {.u_amount = bloom_amount};
        sg_apply_uniforms(UB_fs_bloom, &SG_RANGE(bloom_uniforms));
    }
    sg_draw(0, 3, 1);
    sg_end_pass();
}