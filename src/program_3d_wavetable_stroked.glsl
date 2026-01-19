@vs vs_quad_novbuf
out vec2 uv;
out vec4 colour;
out flat float param;

layout(binding = 0) uniform vs_position {
    vec2 u_topleft;
    vec2 u_bottomright;
    vec2 u_size;

    float u_param;
    int u_colour_left;
    int u_colour_right;
};

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? u_bottomright.x : u_topleft.x,
        is_bottom ? u_bottomright.y : u_topleft.y
    );
    pos = (pos + pos) / u_size - vec2(1);
    pos.y = -pos.y;

    gl_Position = vec4(pos, 1, 1);
    uv = vec2(
        is_right  ? 1 : 0,
        is_bottom ? 1 : 0
    );

    uint col = is_right ? u_colour_right : u_colour_left;
    colour = unpackUnorm4x8(col).abgr;
    param = u_param;
}
@end

@fs fs_quad_novbuf
in vec2 uv;
in vec4 colour;
in flat float param;


out vec4 frag_color;

void main() {

    frag_color = uv.x < param ? colour : vec4(0);
}
@end

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
out flat float stroke_width;


void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    linetile vert = vtx[v_idx];
    
    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,

    // Is odd
    bool is_right  = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? vert.bottomright.x : vert.topleft.x,
        is_bottom ? vert.bottomright.y : vert.topleft.y
    );

    pos = (pos + pos) / vert.view_size - vec2(1);
    pos.y = -pos.y;

    float vw = vert.bottomright.x - vert.topleft.x;
    float vh = vert.bottomright.y - vert.topleft.y;

    gl_Position = vec4(pos, 1, 1);
    // p = vec2(is_right  ? 1 : -1, is_bottom ? -1 : 1);
    p = pos;
    tile_idx = is_right ? vert.tile_end_idx : vert.tile_begin_idx;
    colour = vert.colour;

    px_inc = 2.0 / vert.view_size.x;

    stroke_width = vert.stroke_width / vert.view_size.y * 2;

    buffer_begin_idx = vert.buffer_begin_idx;
    buffer_end_idx   = vert.buffer_end_idx;
}
@end

@fs fs_line_stroke_tiled

in vec2 p;
in float tile_idx;

in flat uint  colour;
in flat int buffer_begin_idx;
in flat int buffer_end_idx;

in flat float px_inc;
in flat float stroke_width;


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

    // sine_y      = sine_y      * 2 - 1;
    // sine_y_prev = sine_y_prev * 2 - 1;
    // sine_y_next = sine_y_next * 2 - 1;

    // build points
    vec2 a = vec2(p.x - px_inc, sine_y_prev);
    vec2 b = vec2(p.x         , sine_y);
    vec2 c = vec2(p.x + px_inc, sine_y_next);

    float d1 = sdSegment(p, a, b);
    float d2 = sdSegment(p, b, c);
    float d = min(d1, d2);

    float shape_vertical   = smoothstep(stroke_width, 0, abs(d));
    float shape_horizontal = smoothstep(stroke_width, 0, abs(sine_y - p.y));
    float shape            = max(shape_vertical, shape_horizontal);

    vec4 col_line = unpackUnorm4x8(colour).abgr;
    col_line.a *= shape;
    frag_colour = col_line;

    // Show tiles
    // vec4 col_bg = vec4(0.5, 0, 0.5, 1);
    // vec4 col_line = unpackUnorm4x8(colour).abgr;
    // frag_colour = mix(col_bg, col_line, shape);
}
@end

/* quad shader program */
@program quad_novbuf vs_quad_novbuf fs_quad_novbuf

@program line_stroke_tiled vs_line_stroke_tiled fs_line_stroke_tiled