@vs vs_line_stroke_tiled

struct linetile
{
    vec2 topleft;
    vec2 bottomright;
    vec2 view_size;

    int buffer_begin_idx;
    int buffer_end_idx;
    int tile_begin_idx;
    int tile_end_idx;

    float stroke_width;
    uint colour;
};

layout(binding=0) readonly buffer sbo_tiles {
    linetile vtx[];
};

out vec2 p;
out float tile_idx;

out flat uint colour;
out flat int buffer_begin_idx;
out flat int buffer_end_idx;

out flat float px_inc;
out flat vec2 stroke_width;

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    linetile vert = vtx[v_idx];

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    tile_idx = is_right ? vert.tile_end_idx : vert.tile_begin_idx;

    buffer_begin_idx = vert.buffer_begin_idx;
    buffer_end_idx   = vert.buffer_end_idx;
    colour = vert.colour;

    px_inc       = 2.0 / vert.view_size.x;
    stroke_width = 2 * vert.stroke_width / vert.view_size;

    // Fixes tearing at seams of tiles 
    vert.topleft.y     += vert.stroke_width;
    vert.bottomright.y -= vert.stroke_width;

    float vw = vert.bottomright.x - vert.topleft.x;
    float vh = vert.bottomright.y - vert.topleft.y;

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,
    vec2 pos = vec2(
        is_right  ? vert.bottomright.x : vert.topleft.x,
        is_bottom ? vert.bottomright.y : vert.topleft.y
    );

    pos = (pos + pos) / vert.view_size - vec2(1);
    pos.y = -pos.y;

    p = pos;
    gl_Position = vec4(pos, 1, 1);
}
@end

@fs fs_line_stroke_tiled

in vec2 p;
in float tile_idx;

in flat uint  colour;
in flat int buffer_begin_idx;
in flat int buffer_end_idx;

in flat float px_inc;
in flat vec2 stroke_width;

out vec4 frag_colour;

struct buffer_item {
    float y;
};

layout(binding=1) readonly buffer sbo_line_stroke_tiled {
    buffer_item sine_buffer[];
};

float sdSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2  ba = b - a;
    vec2  pa = p - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

void main()
{
    // Read buffer data

    // If we pad the storage buffer by 1 on each side with real values, then we can get nicer looing clipped edges
    uint idx      = min(int(tile_idx),     buffer_end_idx - 1);
    uint idx_prev = max(int(tile_idx) - 1, buffer_begin_idx);
    uint idx_next = min(int(tile_idx) + 1, buffer_end_idx - 1);

    float sine_y      = sine_buffer[idx].y;
    float sine_y_prev = sine_buffer[idx_prev].y;
    float sine_y_next = sine_buffer[idx_next].y;

    sine_y      = sine_y      * 2 - 1;
    sine_y_prev = sine_y_prev * 2 - 1;
    sine_y_next = sine_y_next * 2 - 1;

    // build points
    vec2 a = vec2(p.x - px_inc, sine_y_prev);
    vec2 b = vec2(p.x           , sine_y);
    vec2 c = vec2(p.x + px_inc, sine_y_next);

    float d1 = sdSegment(p, a, b);
    float d2 = sdSegment(p, b, c);
    float d = min(d1, d2);

    float shape_vertical   = smoothstep(stroke_width.x, 0, abs(d));
    float shape_horizontal = smoothstep(stroke_width.y, 0, abs(sine_y - p.y));
    float shape            = max(shape_vertical, shape_horizontal);
    shape = sqrt(shape); // gamma

    vec4 col_line = unpackUnorm4x8(colour).abgr;

    // col_line.a *= shape;
    // frag_colour = col_line;

    // Show tiles
    vec4 col_bg = vec4(0.5, 0, 0.5, 1);
    frag_colour = mix(col_bg, col_line, shape);
}
@end

@vs vs_line_stroke_overdraw
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 p;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    p = pos;
}
@end

@fs fs_line_stroke_overdraw

in vec2 p;
out vec4 frag_colour;

layout(binding=0) uniform fs_uniforms_line_stroke_overdraw {
    vec2  u_view_size;
    float u_stroke_width;
    int   u_buffer_length;
};

struct buffer_item {
    float y;
};

layout(binding=0)readonly buffer sbo_line_stroke_overdraw {
    buffer_item sine_buffer[];
};

float sdSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2  ba = b - a;
    vec2  pa = p - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

void main()
{
    vec2 px_inc = vec2(2.0) / u_view_size;
    vec2 stroke_width = px_inc * u_stroke_width;

    // Read buffer data

    // If we pad the storage buffer by 1 on each side with real values, then we can get nicer looing clipped edges
    float uvx     = p.x * 0.5 + 0.5;
    uint idx      = min(int(uvx * u_view_size.x),     u_buffer_length - 1);
    uint idx_prev = max(int(uvx * u_view_size.x) - 1, 0);
    uint idx_next = min(int(uvx * u_view_size.x) + 1, u_buffer_length - 1);

    float sine_y      = sine_buffer[idx].y;
    float sine_y_prev = sine_buffer[idx_prev].y;
    float sine_y_next = sine_buffer[idx_next].y;

    sine_y      = sine_y      * 2 - 1;
    sine_y_prev = sine_y_prev * 2 - 1;
    sine_y_next = sine_y_next * 2 - 1;

    // build points
    vec2 a = vec2(p.x - px_inc.x, sine_y_prev);
    vec2 b = vec2(p.x           , sine_y);
    vec2 c = vec2(p.x + px_inc.x, sine_y_next);

    float d1 = sdSegment(p, a, b);
    float d2 = sdSegment(p, b, c);
    float d = min(d1, d2);

    float shape_vertical   = smoothstep(stroke_width.x, 0, abs(d));
    float shape_horizontal = smoothstep(stroke_width.y, 0, abs(sine_y - p.y));
    float shape            = max(shape_vertical, shape_horizontal);
    shape = sqrt(shape); // gamma

    frag_colour = vec4(1, 1, 1, shape);
    // vec3 col_bg   = vec3(0);
    // vec3 col_line = vec3(1.0);
    // vec3 col = mix(col_bg, col_line, shape);
    // frag_colour = vec4(col, 1.0);
}
@end

@program line_stroke_overdraw vs_line_stroke_overdraw fs_line_stroke_overdraw

@program line_stroke_tiled vs_line_stroke_tiled fs_line_stroke_tiled