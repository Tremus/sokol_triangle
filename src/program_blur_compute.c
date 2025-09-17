#include "common.h"

#include <xhl/debug.h>
#include <xhl/files.h>
#include <math.h>

#include "sokol_gfx.h"
#include "sokol_glue.h"

// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "program_blur_compute.h"

// Adapted from here
// https://github.com/floooh/sokol-samples/blob/29d5e9f4a56ae1884c4ffa7178b1af739d827d07/sapp/imageblur-sapp.c Which was
// ported from here: https://webgpu.github.io/webgpu-samples/?sample=imageBlur#blur.wgsl
// Fairly heavy operation. Probably shouldn't be used

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

static struct
{
    Image      src_img;
    sg_sampler smp;
    struct
    {
        sg_pipeline    pip;
        sg_image       storage_image[2];
        sg_attachments attachments[2];
    } compute;
    struct
    {
        sg_pipeline pip;
    } display;
} state = {0};

void program_setup(void)
{
    // create a non-filtering sampler
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
    });

    static const char* path_brain_jpg = SRC_DIR XFILES_DIR_STR "brain.jpg";
    static const char* path_n64_png   = SRC_DIR XFILES_DIR_STR "n64.png";
    load_image_file(path_brain_jpg, &state.src_img);

    // create two storage textures for the 2-pass blur and two attachments objects
    const char* storage_image_labels[2] = {"storage-image-0", "storage-image-1"};
    const char* compute_pass_labels[2]  = {"compute-pass-0", "compute-pass-1"};
    for (int i = 0; i < 2; i++)
    {
        state.compute.storage_image[i] = sg_make_image(&(sg_image_desc){
            .usage =
                {
                    .storage_attachment = true,
                },
            .width        = state.src_img.width,
            .height       = state.src_img.height,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .label        = storage_image_labels[i],
        });
        state.compute.attachments[i]   = sg_make_attachments(&(sg_attachments_desc){
              .storages[SIMG_cs_outp_tex] = {.image = state.compute.storage_image[i]},
              .label                      = compute_pass_labels[i],
        });
    }

    // create compute shader and pipeline
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute                = true,
        .depth                  = {.pixel_format = SG_PIXELFORMAT_NONE},
        .colors[0].pixel_format = SG_PIXELFORMAT_BGRA8,
        .shader                 = sg_make_shader(compute_shader_desc(sg_query_backend())),
        .label                  = "compute-pipeline",
    });

    // a shader and pipeline to display the result (we'll
    // synthesize the fullscreen vertices in the vertex shader so we
    // don't need any buffers or a pipeline vertex layout, and default
    // render state is fine for rendering a 2D triangle
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .label  = "display-pipeline",
    });
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
    }
}

// perform a horizontal or vertical blur pass in a compute shader
void blur_pass(int flip, sg_attachments pass_attachments, sg_image src_image, int in_filter_size_)
{
    const int         batch       = 4;                   // must match shader
    const int         tile_dim    = 128;                 // must match shader
    const int         filter_size = in_filter_size_ | 1; // must be odd
    const cs_params_t cs_params   = {
          .flip       = flip,
          .filter_dim = filter_size,
          .block_dim  = tile_dim - (filter_size - 1),
    };
    const float src_width        = flip ? state.src_img.height : state.src_img.width;
    const float src_height       = flip ? state.src_img.width : state.src_img.height;
    const int   num_workgroups_x = (int)ceilf(src_width / (float)cs_params.block_dim);
    const int   num_workgroups_y = (int)ceilf(src_height / (float)batch);

    sg_begin_pass(&(sg_pass){.compute = true, .attachments = pass_attachments, .label = "blur-pass"});
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .images[IMG_cs_inp_tex] = src_image,
        .samplers[SMP_cs_smp]   = state.smp,
    });
    sg_apply_uniforms(UB_cs_params, &SG_RANGE(cs_params));
    sg_dispatch(num_workgroups_x, num_workgroups_y, 1);
    sg_end_pass();
}

void program_tick()
{
    // const int filter_size = 1;
    // const int iterations  = 2;
    const int filter_size = 12;
    const int iterations  = 1;

    // ping-pong blur passes starting with the source image
    blur_pass(0, state.compute.attachments[0], state.src_img.img, filter_size);
    blur_pass(1, state.compute.attachments[1], state.compute.storage_image[0], filter_size);
    for (int i = 0; i < iterations - 1; i++)
    {
        blur_pass(0, state.compute.attachments[0], state.compute.storage_image[1], filter_size);
        blur_pass(1, state.compute.attachments[1], state.compute.storage_image[0], filter_size);
    }

    // swapchain render pass to display the result
    sg_begin_pass(&(sg_pass){
        .action =
            {
                .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}},
            },
        .swapchain = sglue_swapchain(),
        .label     = "display-pass"});
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .images[IMG_disp_tex]   = state.compute.storage_image[1],
        .samplers[SMP_disp_smp] = state.smp,
    });
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}