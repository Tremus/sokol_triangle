// Adapted from Raph Leviens code
// https://raphlinus.github.io/graphics/curves/2019/12/23/flatten-quadbez.html
// https://github.com/raphlinus/raphlinus.github.io/blob/main/_posts/2019-12-23-flatten-quadbez.md?plain=1

#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include <xhl/time.h>

#include "program_flatten_quadbez.h"

#include <math.h>

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
    float    x, y;
    uint32_t colour;
} vertex_t;

#define BIG_INDICES_BUFFER_CAP  ((1 << 16) - 1)
#define BIG_VERTICES_BUFFER_CAP ((BIG_INDICES_BUFFER_CAP * 2) / 3)

struct vertex_t BIG_VERTICES_BUFFER[BIG_VERTICES_BUFFER_CAP];
size_t          BIG_VERTICES_BUFFER_LENGTH = 0;
uint16_t        BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_CAP];
size_t          BIG_INDICES_BUFFER_LENGTH = 0;

float mapf(float v, float s1, float e1, float s2, float e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }

void draw_rect(float x, float y, float width, float height, uint32_t hex_rgba /*0xAABBGGRR*/)
{
    SOKOL_ASSERT(BIG_VERTICES_BUFFER_LENGTH + 4 < ARRLEN(BIG_VERTICES_BUFFER));
    SOKOL_ASSERT(BIG_INDICES_BUFFER_LENGTH + 6 < ARRLEN(BIG_INDICES_BUFFER));

    float l = x;
    float t = y;
    float r = x + width;
    float b = y + height;

    l = mapf(l, 0, state.window_width, -1.0f, 1.0f);
    t = mapf(t, 0, state.window_height, 1.0f, -1.0f);
    r = mapf(r, 0, state.window_width, -1.0f, 1.0f);
    b = mapf(b, 0, state.window_height, 1.0f, -1.0f);

    // Add vertices clockwise
    BIG_VERTICES_BUFFER[BIG_VERTICES_BUFFER_LENGTH + 0] = (vertex_t){l, t, hex_rgba}; // TL
    BIG_VERTICES_BUFFER[BIG_VERTICES_BUFFER_LENGTH + 1] = (vertex_t){r, t, hex_rgba}; // TR
    BIG_VERTICES_BUFFER[BIG_VERTICES_BUFFER_LENGTH + 2] = (vertex_t){r, b, hex_rgba}; // BR
    BIG_VERTICES_BUFFER[BIG_VERTICES_BUFFER_LENGTH + 3] = (vertex_t){l, b, hex_rgba}; // BL

    // Tri 1
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 0] = BIG_VERTICES_BUFFER_LENGTH;
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 1] = BIG_VERTICES_BUFFER_LENGTH + 1;
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 2] = BIG_VERTICES_BUFFER_LENGTH + 2;
    // Tri 2
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 3] = BIG_VERTICES_BUFFER_LENGTH;
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 4] = BIG_VERTICES_BUFFER_LENGTH + 2;
    BIG_INDICES_BUFFER[BIG_INDICES_BUFFER_LENGTH + 5] = BIG_VERTICES_BUFFER_LENGTH + 3;

    BIG_VERTICES_BUFFER_LENGTH += 4;
    BIG_INDICES_BUFFER_LENGTH  += 6;
}

typedef struct point_t
{
    float x, y;
} point_t;

static struct point_t points[] = {
    {200, 450},
    {400, 450},
    {600, 50},
};
static int drag_point_idx = -1;

#define BIG_PATH_BUFFER_CAP 9001
struct point_t BIG_PATH_BUFFER[BIG_PATH_BUFFER_CAP];
size_t         BIG_PATH_BUFFER_LEN = 0;
const float    POINT_RADIUS        = 5.0f;

void draw_point(float x, float y, uint32_t hex_rgba)
{
    draw_rect(x - POINT_RADIUS, y - POINT_RADIUS, POINT_RADIUS * 2, POINT_RADIUS * 2, hex_rgba);
}

void push_point(point_t pt)
{
    SOKOL_ASSERT(BIG_PATH_BUFFER_LEN < BIG_PATH_BUFFER_CAP);
    BIG_PATH_BUFFER[BIG_PATH_BUFFER_LEN] = pt;

    BIG_PATH_BUFFER_LEN++;
}

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    state.bind.vertex_buffers[0] = sg_make_buffer(
        &(sg_buffer_desc){.size = sizeof(BIG_VERTICES_BUFFER), .usage.stream_update = true, .label = "quad-vertices"});
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer  = true,
        .usage.stream_update = true,
        .size                = sizeof(BIG_VERTICES_BUFFER),
        .label               = "quad-indices"});

    sg_shader shd = sg_make_shader(draw_rect_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_draw_rect_position].format = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_draw_rect_color0].format   = SG_VERTEXFORMAT_UBYTE4N}},
        .label = "quad-pipeline"});

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
    else if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN)
    {
        // Check user is dragging points
        for (int i = 0; i < ARRLEN(points); i++)
        {
            // Hit test
            const point_t* pt  = &points[i];
            point_t        tl  = {pt->x - POINT_RADIUS, pt->y - POINT_RADIUS};
            point_t        br  = {pt->x + POINT_RADIUS, pt->y + POINT_RADIUS};
            const bool     hit = e->mouse_x >= tl.x && e->mouse_y >= tl.y && e->mouse_x <= br.x && e->mouse_y <= br.y;
            if (hit)
            {
                drag_point_idx = i;
                break;
            }
        }
    }
    else if (e->type == SAPP_EVENTTYPE_MOUSE_UP)
    {
        drag_point_idx = -1;
    }
    else if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE)
    {
        // Drag point
        if (drag_point_idx >= 0)
        {
            points[drag_point_idx].x = e->mouse_x;
            points[drag_point_idx].y = e->mouse_y;
        }
    }
}

point_t eval(float t)
{
    const float mt = 1 - t;
    const float x  = points[0].x * mt * mt + 2 * points[1].x * t * mt + points[2].x * t * t;
    const float y  = points[0].y * mt * mt + 2 * points[1].y * t * mt + points[2].y * t * t;
    return (point_t){x, y};
}

// Compute an approximation to int (1 + 4x^2) ^ -0.25 dx
// This isn't especially good but will do.
float approx_myint(float x)
{
    const float d = 0.67;
    return x / (1 - d + powf(powf(d, 4) + 0.25 * x * x, 0.25));
}
// Approximate the inverse of the function above.
// This is better.
float approx_inv_myint(float x)
{
    const float b = 0.39;
    return x * (1 - b + sqrtf(b * b + 0.25f * x * x));
}
// Subdivide using fancy algorithm.
void subdiv_levien(float tol)
{
    // const params = this.map_to_basic();
    const float ddx   = 2 * points[1].x - points[0].x - points[2].x;
    const float ddy   = 2 * points[1].y - points[0].y - points[2].y;
    float       u0    = (points[1].x - points[0].x) * ddx + (points[1].y - points[0].y) * ddy;
    float       u2    = (points[2].x - points[1].x) * ddx + (points[2].y - points[1].y) * ddy;
    const float cross = (points[2].x - points[0].x) * ddy - (points[2].y - points[0].y) * ddx;
    const float x0    = u0 / cross;
    const float x2    = u2 / cross;
    // There's probably a more elegant formulation of this...
    const float scale = fabsf(cross) / (hypotf(ddx, ddy) * fabsf(x2 - x0));

    const float a0    = approx_myint(x0);
    const float a2    = approx_myint(x2);
    const float count = 0.5f * fabsf(a2 - a0) * sqrtf(scale / tol);
    const int   n     = ceilf(count);
    u0                = approx_inv_myint(a0);
    u2                = approx_inv_myint(a2);

    point_t pt = eval(0);
    push_point(pt);
    for (int i = 1; i < n; i++)
    {
        const float u = approx_inv_myint(a0 + ((a2 - a0) * i) / n);
        const float t = (u - u0) / (u2 - u0);

        pt = eval(t);
        push_point(pt);
    }
    pt = eval(1);
    push_point(pt);
}

// Subdivide using method from Sederberg's CAGD notes
void subdiv_sederberg(float tolerance, bool round_to_pow2)
{
    const float ddx = 2 * points[1].x - points[0].x - points[2].x;
    const float ddy = 2 * points[1].y - points[0].y - points[2].y;
    const float dd  = hypotf(ddx, ddy);
    uint32_t    N   = ceilf(sqrtf((0.25f * dd) / tolerance));
    if (round_to_pow2) // round up
    {
        N = 1 << (32 - __builtin_clz(N - 1));
    }

    for (int i = 0; i <= N; i++)
    {
        point_t pt = eval((float)i / (float)N);
        push_point(pt);
    }
}

void program_tick()
{
    BIG_VERTICES_BUFFER_LENGTH = 0;
    BIG_INDICES_BUFFER_LENGTH  = 0;
    BIG_PATH_BUFFER_LEN        = 0;

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});

    // Draw subdivision points
    const float threshold = 0.5f;
    subdiv_levien(threshold);
    // subdiv_sederberg(threshold, false); // Sederberg
    // subdiv_sederberg(threshold, true); // Wang

    for (int i = 0; i < BIG_PATH_BUFFER_LEN; i++)
    {
        point_t pt = BIG_PATH_BUFFER[i];
        draw_point(pt.x, pt.y, 0xff0000ff);
    }

    // Draw handles
    for (int i = 0; i < ARRLEN(points); i++)
    {
        draw_point(points[i].x, points[i].y, 0xffffffff);
    }

    if (BIG_INDICES_BUFFER_LENGTH)
    {
        sg_update_buffer(
            state.bind.vertex_buffers[0],
            &(sg_range){.ptr = BIG_VERTICES_BUFFER, BIG_VERTICES_BUFFER_LENGTH * sizeof(BIG_VERTICES_BUFFER[0])});
        sg_update_buffer(
            state.bind.index_buffer,
            &(sg_range){.ptr = BIG_INDICES_BUFFER, BIG_INDICES_BUFFER_LENGTH * sizeof(BIG_INDICES_BUFFER[0])});

        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_draw(0, BIG_INDICES_BUFFER_LENGTH, 1);
    }
    sg_end_pass();
}