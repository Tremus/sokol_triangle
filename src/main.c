#include "common.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include <stdio.h>
#include <xhl/debug.h>
#include <xhl/files.h>
#include <xhl/time.h>

static void my_logger(
    const char* tag,              // always "sapp"
    uint32_t    log_level,        // 0=panic, 1=error, 2=warning, 3=info
    uint32_t    log_item_id,      // SAPP_LOGITEM_*
    const char* message_or_null,  // a message string, may be nullptr in release mode
    uint32_t    line_nr,          // line number in sokol_app.h
    const char* filename_or_null, // source filename, may be nullptr in release mode
    void*       user_data)
{
    static const char* LOG_LEVEL[] = {
        "PANIC",
        "ERROR",
        "WARNING",
        "INFO",
    };
    xassert(log_level > 1);
    xassert(log_level < ARRLEN(LOG_LEVEL));
    if (!message_or_null)
        message_or_null = "";
    printf("[%s] %s %u:%s\n", LOG_LEVEL[log_level], message_or_null, line_nr, filename_or_null);
}

xfiles_watch_context_t g_filewatch_ctx = NULL;

void cb_filewatch(enum XFILES_WATCH_TYPE type, const char* path, void* udata)
{
    switch (type)
    {
    case XFILES_WATCH_CREATED:
        println("Created: %s", path);
        break;
    case XFILES_WATCH_DELETED:
        println("Deleted: %s", path);
        break;
    case XFILES_WATCH_MODIFIED:
        println("Modified: %s", path);
        break;
    }
}

static void init(void)
{
    sg_setup(&(sg_desc){
        .environment        = sglue_environment(),
        .logger.func        = my_logger,
        .pipeline_pool_size = 512,
    });

    program_setup();

    g_filewatch_ctx = xfiles_watch_create(SRC_DIR, 0, cb_filewatch);
}

void frame(void)
{
    program_tick();
    sg_commit();

    xfiles_watch_flush(g_filewatch_ctx);
}

void cleanup(void)
{
    xfiles_watch_destroy(g_filewatch_ctx);

    sg_shutdown();
}

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