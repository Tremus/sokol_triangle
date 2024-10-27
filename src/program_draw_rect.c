#include "common.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "xhl_time.h"

#include "program_draw_rect.h"

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

void program_setup()
{
    state.window_width  = APP_WIDTH;
    state.window_height = APP_HEIGHT;

    state.bind.vertex_buffers[0] = sg_make_buffer(
        &(sg_buffer_desc){.size = sizeof(BIG_VERTICES_BUFFER), .usage = SG_USAGE_STREAM, .label = "quad-vertices"});
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type  = SG_BUFFERTYPE_INDEXBUFFER,
        .size  = sizeof(BIG_VERTICES_BUFFER),
        .usage = SG_USAGE_STREAM,
        .label = "quad-indices"});

    sg_shader shd = sg_make_shader(draw_rect_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {.attrs =
                 {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                  [ATTR_vs_color0].format   = SG_VERTEXFORMAT_UBYTE4N}},
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
}

void program_tick()
{
    BIG_VERTICES_BUFFER_LENGTH = 0;
    BIG_INDICES_BUFFER_LENGTH  = 0;

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});

    draw_rect(20, 20, 50, 80, 0xffe7700d);
    draw_rect(state.window_width - 120, 120, 70, 20, 0xff7adc06);

    // Draw vertical 'pong' like bouncing square
    int        cx         = state.window_width / 2;
    static int pong_y     = APP_HEIGHT / 2;
    static int pong_y_inc = 4;
    draw_rect(cx - 10, pong_y, 20, 20, 0xffffffff);
    pong_y += pong_y_inc;
    if (pong_y <= 0)
    {
        pong_y     = 0;
        pong_y_inc = -pong_y_inc;
    }
    if ((pong_y + 20) >= state.window_height)
    {
        pong_y     = state.window_height - 20;
        pong_y_inc = -pong_y_inc;
    }

    // Conditionally draw flashing red square
    uint64_t   frame_time_ms       = xtime_now_ns() / 1000000LLU;
    const bool should_flash_square = !((frame_time_ms / 500) & 1);
    if (should_flash_square)
    {
        int x = state.window_width / 4;
        int y = state.window_height / 2;
        draw_rect(x - 25, y - 25, 50, 50, 0xff0000ff);
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