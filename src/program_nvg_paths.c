#define XHL_ALLOC_IMPL
#define NANOSVG_IMPLEMENTATION

#include "common.h"

#include <math.h>
#include <string.h>
#include <xhl/alloc.h>
#include <xhl/files.h>

#define NVG_MALLOC(sz)       xmalloc(sz)
#define NVG_REALLOC(ptr, sz) xrealloc(ptr, sz)
#define NVG_FREE(ptr)        xfree(ptr)
#define NVG_ASSERT           xassert

#include "nanosvg.h"
#include "nanovg2.h"

/*
It's probably possible to remove so much of Nanovg that all that's left is path rendering
- No unnecesarry transforms
- no global alpha
- no blend modes
just straight SVG rendering

From there it should be to start removing features from the shader, and compressing data, making it as small as possible

Finally, NanoSVG could be converted to a code generator by simply printing to the conosle nvgMoveTo() etc commands with
the respective coords, and taking an x, y, and scale parameter for growing/shrinking the SVG

This would be ideal for drawing an SVG icon directly into a texture map
*/

// clang-format off
#define nvgHexColour2(hex) (NVGcolour){\
                                        ( hex >>  0  & 0xff) / 255.0f,\
                                        ((hex >>  8) & 0xff) / 255.0f,\
                                        ((hex >> 16) & 0xff) / 255.0f,\
                                        ( hex >> 24)         / 255.0f,\
                                      }
// clang-format on

static struct
{
    NVGcontext* nvg;
    NSVGimage*  svg;

    int width, height;
} state;

void program_setup()
{
    xalloc_init();

    state.nvg    = nvgCreateContext(NVG_ANTIALIAS);
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;

    static const char* path_tiger_svg = SRC_DIR XFILES_DIR_STR "Ghostscript_Tiger.svg";

    state.svg = nsvgParseFromFile(path_tiger_svg, "px", 96);
    println("size: %f x %f", state.svg->width, state.svg->height);
}
void program_shutdown()
{
    nvgDestroyContext(state.nvg);

    nsvgDelete(state.svg);

    xalloc_shutdown();
}

bool program_event(const PWEvent* event)
{
    if (event->type == PW_EVENT_RESIZE_UPDATE)
    {
        state.width  = event->resize.width;
        state.height = event->resize.height;
    }
    return false;
}

NVGpaint createLinearGradient(NVGcontext* vg, NSVGgradient* gradient, float alpha)
{
    float inverse[6];
    float sx, sy, ex, ey;

    nvgTransformInverse(inverse, gradient->xform);
    nvgTransformPoint(&sx, &sy, inverse, 0, 0);
    nvgTransformPoint(&ex, &ey, inverse, 0, 1);

    NVGcolour col1 = nvgHexColour2(gradient->stops[0].color);
    NVGcolour col2 = nvgHexColour2(gradient->stops[gradient->nstops - 1].color);

    return nvgLinearGradient(vg, sx, sy, ex, ey, col1, col2);
}

void program_tick()
{
    nvgBeginFrame(state.nvg, 1);
    state.nvg->view.viewSize[0] = 0;
    state.nvg->view.viewSize[1] = 0;
    state.nvg->view.viewSize[2] = state.width;
    state.nvg->view.viewSize[3] = state.height;

    snvg_command_begin_pass(
        state.nvg,
        &(sg_pass){
            .action =
                (sg_pass_action){
                    .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {1.0f, 1.0f, 1.0f, 1.0f}}},
            .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)},
        0,
        0,
        state.width,
        state.height,
        "begin pass");

    snvg_command_draw_nvg(state.nvg, "nvg");

    NVGcontext* vg = state.nvg;

    for (NSVGshape* shape = state.svg->shapes; shape != NULL; shape = shape->next)
    {

        if (!(shape->flags & NSVG_FLAGS_VISIBLE))
            continue;

        for (NSVGpath* path = shape->paths; path != NULL; path = path->next)
        {
            nvgBeginPath(vg);
            nvgMoveTo(vg, path->pts[0], path->pts[1]);
            for (int i = 0; i < path->npts - 1; i += 3)
            {
                float* p = &path->pts[i * 2];
                nvgBezierTo(vg, p[2], p[3], p[4], p[5], p[6], p[7]);
            }
            if (path->closed)
            {
                nvgClosePath(vg);
            }

            if (shape->fill.type)
            {
                xassert(shape->fill.type == NSVG_PAINT_COLOR); // todo, handle linear and radial gradients
                nvgSetColour(vg, nvgHexColour2(shape->fill.color));
                nvgFill(vg);
            }

            if (shape->stroke.type)
            {
                xassert(shape->stroke.type == NSVG_PAINT_COLOR);
                nvgSetColour(vg, nvgHexColour2(shape->stroke.color));
                nvgStroke(vg, shape->strokeWidth);
            }
        }
    }

    snvg_command_end_pass(vg, "end pass");

    nvgEndFrame(vg);
}