#include "common.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "xhl_time.h"

static void my_logger(
    const char* tag,              // always "sapp"
    uint32_t    log_level,        // 0=panic, 1=error, 2=warning, 3=info
    uint32_t    log_item_id,      // SAPP_LOGITEM_*
    const char* message_or_null,  // a message string, may be nullptr in release mode
    uint32_t    line_nr,          // line number in sokol_app.h
    const char* filename_or_null, // source filename, may be nullptr in release mode
    void*       user_data)
{
    static char* LOG_LEVEL[] = {
        "PANIC",
        "ERROR",
        "WARNING",
        "INFO",
    };
    SOKOL_ASSERT(log_level > 0);
    SOKOL_ASSERT(log_level < ARRLEN(LOG_LEVEL));
    if (!message_or_null)
        message_or_null = "";
    print("[%s] %s %u:%s", LOG_LEVEL[log_level], message_or_null, line_nr, filename_or_null);
}

static void init(void)
{
    xtime_init();
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = my_logger,
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
        .init_cb            = init,
        .frame_cb           = frame,
        .cleanup_cb         = cleanup,
        .event_cb           = program_event,
        .width              = APP_WIDTH,
        .height             = APP_HEIGHT,
        .window_title       = "GFX (sokol-app)",
        .icon.sokol_default = true,
        .logger.func        = my_logger};
}