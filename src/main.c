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
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
#if __APPLE__
    env.metal.device = pw_get_metal_device(_p);
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

void pw_tick(void* _gui)
{
    sg_set_global(g_sg);

    program_tick();
    sg_commit();

    sg_set_global(NULL);
}

//=============================================================================
void  cplug_libraryLoad() {}
void  cplug_libraryUnload() {}
void* cplug_createPlugin(CplugHostContext* ctx)
{
    static int data = 0;
    return &data;
}
void cplug_destroyPlugin(void*) {}

uint32_t cplug_getNumInputBusses(void*) { return 0; }
uint32_t cplug_getNumOutputBusses(void*) { return 0; }
uint32_t cplug_getInputBusChannelCount(void*, uint32_t bus_idx) { return 2; }
uint32_t cplug_getOutputBusChannelCount(void*, uint32_t bus_idx) { return 2; }
void     cplug_getInputBusName(void*, uint32_t idx, char* buf, size_t buflen) {}
void     cplug_getOutputBusName(void*, uint32_t idx, char* buf, size_t buflen) {}
uint32_t cplug_getLatencyInSamples(void*) { return 0; }
uint32_t cplug_getTailInSamples(void*) { return 0; }
void     cplug_setSampleRateAndBlockSize(void*, double sampleRate, uint32_t maxBlockSize) {}
void     cplug_process(void* userPlugin, CplugProcessContext* ctx) {}
uint32_t cplug_getNumParameters(void*) { return 0; }
uint32_t cplug_getParameterID(void*, uint32_t paramIndex) { return 0; }
uint32_t cplug_getParameterFlags(void*, uint32_t paramId) { return 0; }
void     cplug_getParameterRange(void*, uint32_t paramId, double* min, double* max) {}
void     cplug_getParameterName(void*, uint32_t paramId, char* buf, size_t buflen) {}
double   cplug_getParameterValue(void*, uint32_t paramId) { return 0; }
double   cplug_getDefaultParameterValue(void*, uint32_t paramId) { return 0; }
void     cplug_setParameterValue(void*, uint32_t paramId, double value) {}
double   cplug_denormaliseParameterValue(void*, uint32_t paramId, double value) { return 0; }
double   cplug_normaliseParameterValue(void*, uint32_t paramId, double value) { return 0; }
double   cplug_parameterStringToValue(void*, uint32_t paramId, const char*) { return 0; }
void     cplug_parameterValueToString(void*, uint32_t paramId, char* buf, size_t bufsize, double value) {}
void     cplug_saveState(void* userPlugin, const void* stateCtx, cplug_writeProc writeProc) {}
void     cplug_loadState(void* userPlugin, const void* stateCtx, cplug_readProc readProc) {}
