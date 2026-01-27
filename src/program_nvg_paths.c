#define XHL_ALLOC_IMPL

#include "common.h"

#include <math.h>
#include <string.h>
#include <xhl/alloc.h>

#define NVG_MALLOC(sz)       xmalloc(sz)
#define NVG_REALLOC(ptr, sz) xrealloc(ptr, sz)
#define NVG_FREE(ptr)        xfree(ptr)
#define NVG_ASSERT           xassert

#include "nanovg2.h"

static struct
{
    NVGcontext* nvg;
    int         width, height;
} state;

void program_setup()
{
    xalloc_init();

    state.nvg    = nvgCreateContext(NVG_ANTIALIAS);
    state.width  = APP_WIDTH;
    state.height = APP_HEIGHT;
}
void program_shutdown()
{
    nvgDestroyContext(state.nvg);

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
                    .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}},
            .swapchain = get_swapchain(SG_PIXELFORMAT_RGBA8)},
        0,
        0,
        state.width,
        state.height,
        "begin pass");

    snvg_command_draw_nvg(state.nvg, "nvg");

    nvgBeginPath(state.nvg);

    // rectangle
    int x, y, w, h;
    x = y = 20;
    w = h = 200;

    float vals[] = {NVG_MOVETO, x, y, NVG_LINETO, x, y + h, NVG_LINETO, x + w, y + h, NVG_LINETO, x + w, y, NVG_CLOSE};
    nvg__appendCommands(state.nvg, vals, NVG_ARRLEN(vals));

    nvgSetColour(state.nvg, (NVGcolour){1, 0, 0, 1});
    nvgFill(state.nvg);
    nvgSetColour(state.nvg, (NVGcolour){0, 1, 0, 1});
    nvgStroke(state.nvg, 2);

    snvg_command_end_pass(state.nvg, "end pass");

    nvgEndFrame(state.nvg);
}