#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include <xhl/time.h>

#include "program_pathkit.h"

#include <math.h>

#include <vector>

#include "pathkit.h"

static struct
{
    int window_width;
    int window_height;

    sg_pipeline    pip;
    sg_bindings    bind;
    sg_pass_action pass_action;
} state;

float mapf(float v, float s1, float e1, float s2, float e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }

#define BIG_INDICES_BUFFER_CAP  ((1 << 16) - 1)
#define BIG_VERTICES_BUFFER_CAP ((BIG_INDICES_BUFFER_CAP * 2) / 3)

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    auto buffer_desc = (sg_buffer_desc){
        .size                = BIG_VERTICES_BUFFER_CAP * sizeof(float),
        .usage.stream_update = true,
        .label               = "quad-vertices"};
    state.bind.vertex_buffers[0] = sg_make_buffer(&buffer_desc);

    sg_shader shd = sg_make_shader(pathkit_shader_desc(sg_query_backend()));

    auto pipeline_desc = (sg_pipeline_desc){
        .shader = shd,
        .layout = {.attrs = {[ATTR_pathkit_position].format = SG_VERTEXFORMAT_FLOAT2}},
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
    sg_pass pass = (sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()};
    sg_begin_pass(&pass);

    using namespace pk;

    SkRect clipBounds({0, 0, (float)state.window_width * 0.5f, (float)state.window_height * 0.5f});

    // static SkPath path;
    static SkPath      line;
    static SkStrokeRec stroker(SkStrokeRec::kHairline_InitStyle);

    static std::vector<float> vertices;

    // #define NO_AA 1
#define NO_AA 0

    // path.reset();
    line.reset();
    vertices.clear();
    vertices.reserve(2000);

    line.moveTo(32, 150);
    line.quadTo(50, 50, 100, 100);
    line.lineTo(100, 200);
    line.cubicTo(20, 20, 300, 300, 300, 20);
    // SkStroke stroke;
    // stroke.setWidth(5.0f);
    // stroke.strokePath(line, &path);
    stroker.setStrokeStyle(5.0f, 0);
    bool did_apply = stroker.applyToPath(&line, line);

    bool        isLinear         = false;
    const float DefaultTolerance = 0.25f;

#if NO_AA
    size_t num_triangles = line.toTriangles(DefaultTolerance, clipBounds, &vertices, &isLinear);
#else
    size_t num_triangles = line.toAATriangles(DefaultTolerance, clipBounds, &vertices);
#endif
    SOKOL_ASSERT(vertices.size() < BIG_VERTICES_BUFFER_CAP);

    size_t N = vertices.size();
#if NO_AA
    for (int i = 0; i < N; i += 2)
#else
    // TODO: figure out what the third float in the AA vertices is supposed to be
    for (int i = 0, k = 0; i < N; i += 3, k += 2)
#endif
    {
        float x = vertices[i];
        float y = vertices[i + 1];

        x = mapf(x, 0, state.window_width, -1.0f, 1.0f);
        y = mapf(y, 0, state.window_height, 1.0f, -1.0f);

#if NO_AA
        vertices[i]     = x;
        vertices[i + 1] = y;
#else
        vertices[k]     = x;
        vertices[k + 1] = y;
#endif
    }

    if (vertices.size())
    {
        sg_range buf_range = {.ptr = vertices.data(), vertices.size() * sizeof(vertices[0])};
        sg_update_buffer(state.bind.vertex_buffers[0], &buf_range);

        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);

        const fs_uniforms_t uniforms = {.col = {1., 0., 1., 1.}};
        sg_range            u_range  = SG_RANGE(uniforms);
        sg_apply_uniforms(UB_fs_uniforms, &u_range);

        sg_draw(0, num_triangles, 1);
    }
    sg_end_pass();
}