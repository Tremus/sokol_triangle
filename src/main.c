#include "common.h"

#include <cplug.h>
#include <sokol_gfx.h>
#include <stdio.h>
#include <string.h>
#include <xhl/debug.h>
#include <xhl/files.h>
#include <xhl/time.h>

static void my_sg_logger(
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

void* g_pw = NULL;
void* g_sg = NULL;

void* pw_create_gui(void* _plugin, void* _pw)
{
    xassert(g_pw == NULL);
    g_pw = _pw;

    sg_environment env;
    memset(&env, 0, sizeof(env));
    env.defaults.sample_count = 1;
    // env.defaults.color_format = SG_PIXELFORMAT_SRGB8A8;
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
#if __APPLE__
    env.metal.device = pw_get_metal_device(_pw);
#elif _WIN32
    env.d3d11.device         = pw_get_dx11_device(_pw);
    env.d3d11.device_context = pw_get_dx11_device_context(_pw);
#endif
    g_sg = sg_setup(&(sg_desc){
        .environment        = env,
        .logger             = my_sg_logger,
        .pipeline_pool_size = 512,
    });
    xassert(g_sg);

    program_setup();

    static int data = 1;

    return &data;
}

void pw_destroy_gui(void* _gui)
{
    sg_set_global(g_sg);
    program_shutdown();
    sg_shutdown(g_sg);
}

void pw_get_info(struct PWGetInfo* info)
{
    if (info->type == PW_INFO_INIT_SIZE)
    {
        info->init_size.width  = APP_WIDTH;
        info->init_size.height = APP_HEIGHT;
    }
}

int g_width  = APP_WIDTH;
int g_height = APP_HEIGHT;

bool pw_event(const PWEvent* e)
{
    if (e->type == PW_EVENT_RESIZE_UPDATE)
    {
        g_width  = e->resize.width;
        g_height = e->resize.height;
    }
    return program_event(e);
}

void pw_tick(void* _gui)
{
    sg_set_global(g_sg);

    program_tick();
    sg_commit();

    sg_set_global(NULL);
}

sg_swapchain get_swapchain(sg_pixel_format pixel_format)
{
    return (sg_swapchain){
        .width        = g_width,
        .height       = g_height,
        .sample_count = 1,
        .color_format = pixel_format,
#if __APPLE__
        .metal.current_drawable      = pw_get_metal_drawable(g_pw),
        .metal.depth_stencil_texture = pw_get_metal_depth_stencil_texture(g_pw),
#endif
#if _WIN32
        .d3d11.render_view        = pw_get_dx11_render_target_view(g_pw),
        .d3d11.depth_stencil_view = pw_get_dx11_depth_stencil_view(g_pw),
#endif
    };
}

//=============================================================================
void  cplug_libraryLoad() {}
void  cplug_libraryUnload() {}
void* cplug_createPlugin(CplugHostContext* ctx)
{
    static int data = 0;
    return &data;
}
void cplug_destroyPlugin(void* _p) {}

uint32_t cplug_getNumInputBusses(void* _p) { return 0; }
uint32_t cplug_getNumOutputBusses(void* _p) { return 0; }
uint32_t cplug_getInputBusChannelCount(void* _p, uint32_t bus_idx) { return 2; }
uint32_t cplug_getOutputBusChannelCount(void* _p, uint32_t bus_idx) { return 2; }
void     cplug_getInputBusName(void* _p, uint32_t idx, char* buf, size_t buflen) {}
void     cplug_getOutputBusName(void* _p, uint32_t idx, char* buf, size_t buflen) {}
uint32_t cplug_getLatencyInSamples(void* _p) { return 0; }
uint32_t cplug_getTailInSamples(void* _p) { return 0; }
void     cplug_setSampleRateAndBlockSize(void* _p, double sampleRate, uint32_t maxBlockSize) {}
void     cplug_process(void* userPlugin, CplugProcessContext* ctx) {}
uint32_t cplug_getNumParameters(void* _p) { return 0; }
uint32_t cplug_getParameterID(void* _p, uint32_t paramIndex) { return 0; }
uint32_t cplug_getParameterFlags(void* _p, uint32_t paramId) { return 0; }
void     cplug_getParameterRange(void* _p, uint32_t paramId, double* min, double* max) {}
void     cplug_getParameterName(void* _p, uint32_t paramId, char* buf, size_t buflen) {}
double   cplug_getParameterValue(void* _p, uint32_t paramId) { return 0; }
double   cplug_getDefaultParameterValue(void* _p, uint32_t paramId) { return 0; }
void     cplug_setParameterValue(void* _p, uint32_t paramId, double value) {}
double   cplug_denormaliseParameterValue(void* _p, uint32_t paramId, double value) { return 0; }
double   cplug_normaliseParameterValue(void* _p, uint32_t paramId, double value) { return 0; }
double   cplug_parameterStringToValue(void* _p, uint32_t paramId, const char* str) { return 0; }
void     cplug_parameterValueToString(void* _p, uint32_t paramId, char* buf, size_t bufsize, double value) {}
void     cplug_saveState(void* userPlugin, const void* stateCtx, cplug_writeProc writeProc) {}
void     cplug_loadState(void* userPlugin, const void* stateCtx, cplug_readProc readProc) {}
