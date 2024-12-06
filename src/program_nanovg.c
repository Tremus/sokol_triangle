#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include "nanovg_sokol.h"

#include "program_nanovg.h"

NVGcontext* vg = NULL;

void program_setup()
{
    // vg = nvgCreateSokol(0);
    vg = nvgCreateSokol(NVG_ANTIALIAS);
    SOKOL_ASSERT(vg);
}

void program_event(const sapp_event* event) {}

void program_tick()
{
    sg_pass_action pass_action = {
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
    sg_begin_pass(&(sg_pass){.action = pass_action, .swapchain = sglue_swapchain()});

    int w = sapp_width();
    int h = sapp_height();
    nvgBeginFrame(vg, w, h, sapp_dpi_scale());

    nvgBeginPath(vg);
    nvgCircle(vg, 100, 100, 50);
    nvgFillColor(vg, (NVGcolor){1, 1, 0, 1});
    nvgFill(vg);

    nvgEndFrame(vg);
    sg_end_pass();
}