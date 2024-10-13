#include "common.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

static void init(void)
{
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    program_setup();
}

void frame(void)
{
    program_tick();
    sg_commit();
}

void cleanup(void) { sg_shutdown(); }

sapp_desc sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .cleanup_cb = cleanup,
        // .event_cb           = __dbgui_event,
        .width              = APP_WIDTH,
        .height             = APP_HEIGHT,
        .window_title       = "GFX (sokol-app)",
        .icon.sokol_default = true,
    };
}