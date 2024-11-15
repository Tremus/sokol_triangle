#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "xhl_time.h"

#include "program_pathkit.h"

#include <math.h>

#include <vector>

#include "pathkit/core/SkStroke.h"
#include "pathkit/pathkit.h"

static struct
{
    int window_width;
    int window_height;

    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

typedef struct vertex_t
{
    float x, y;
} vertex_t;

float mapf(float v, float s1, float e1, float s2, float e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }

#define BIG_INDICES_BUFFER_CAP  ((1 << 16) - 1)
#define BIG_VERTICES_BUFFER_CAP ((BIG_INDICES_BUFFER_CAP * 2) / 3)

std::vector<float> BIG_VERTICES_BUFFER;

typedef struct point_t
{
    float x, y;
} point_t;

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    auto buffer_desc = (sg_buffer_desc){
        .size  = BIG_VERTICES_BUFFER_CAP * sizeof(float),
        .usage = SG_USAGE_STREAM,
        .label = "quad-vertices"};
    state.bind.vertex_buffers[0] = sg_make_buffer(&buffer_desc);

    sg_shader shd = sg_make_shader(minimal_shader_desc(sg_query_backend()));

    auto pipeline_desc = (sg_pipeline_desc){
        .shader = shd,
        .layout = {.attrs = {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2}},
        .label  = "quad-pipeline"};
    state.pip = sg_make_pipeline(&pipeline_desc);

    state.pass_action =
        (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

void program_event(const sapp_event* e)
{
    if (e->type == SAPP_EVENTTYPE_RESIZED)
    {
        state.window_width  = e->window_width;
        state.window_height = e->window_height;
    }
}

void program_tick()
{
    BIG_VERTICES_BUFFER.clear();

    sg_pass pass = (sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()};
    sg_begin_pass(&pass);

    using namespace pk;

    SkRect        clipBounds({0, 0, (float)state.window_width, (float)state.window_height});
    static SkPath path;
    static SkPath line;

    path.reset();
    line.reset();

    line.moveTo(32, 150);
    line.quadTo(50, 50, 100, 100);
    line.lineTo(100, 200);
    line.cubicTo(20, 20, 300, 300, 300, 20);
    SkStroke stroke;
    stroke.setWidth(5.0f);
    stroke.strokePath(line, &path);

    bool   isLinear = false;
    size_t sz       = path.toTriangles(0.5f, clipBounds, &BIG_VERTICES_BUFFER, &isLinear);

    for (int i = 0; i < BIG_VERTICES_BUFFER.size(); i += 2)
    {
        float x = BIG_VERTICES_BUFFER[i];
        float y = BIG_VERTICES_BUFFER[i + 1];

        x = mapf(x, 0, state.window_width, -1.0f, 1.0f);
        y = mapf(y, 0, state.window_height, 1.0f, -1.0f);

        BIG_VERTICES_BUFFER[i]     = x;
        BIG_VERTICES_BUFFER[i + 1] = y;
    }

    if (BIG_VERTICES_BUFFER.size())
    {
        auto range =
            (sg_range){.ptr = BIG_VERTICES_BUFFER.data(), BIG_VERTICES_BUFFER.size() * sizeof(BIG_VERTICES_BUFFER[0])};
        sg_update_buffer(state.bind.vertex_buffers[0], &range);

        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_draw(0, sz, 1);
    }
    sg_end_pass();
}