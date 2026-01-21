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

    // int buffer_begin_idx;
    // int buffer_end_idx;
    int tile_begin_idx;
    int tile_end_idx;

    float stroke_width;
    uint colour;
};

struct buffer_item {
    float y;
};

layout(binding=0) readonly buffer sbo_tiles {
    linetile vtx[];
};

layout(binding=1) readonly buffer sbo_line_stroke_tiled {
    buffer_item sine_buffer[];
};

out vec2 p;
out flat uint colour;
out flat float stroke_width;
out flat vec2 pt_prev;
out flat vec2 pt;
out flat vec2 pt_next;


void main() {
    // uint v_idx = gl_VertexIndex / (6u * 2048u);
    // uint i_idx = gl_VertexIndex - v_idx * (6u * 2048u);
    // uint sample_idx = gl_VertexIndex / 6u;

    uint lmao = gl_VertexIndex / 6u;

    uint v_idx      = lmao >> 11u;
    uint i_idx      = gl_VertexIndex - (lmao * 6u);
    uint sample_idx = lmao & 2047u;

    linetile vert = vtx[v_idx];

    vec2 size = vert.bottomright - vert.topleft;

    float samples_per_pixel = size.x / 2048.0;
    float pixels_per_sample = 2048.0 / size.x;

    vec4 column = vec4(
        // vert.topleft.x + sample_idx * samples_per_pixel,
        floor(vert.topleft.x + sample_idx * samples_per_pixel),
        vert.topleft.y,
        // vert.topleft.x + (sample_idx + 1) * samples_per_pixel,
        0,
        vert.bottomright.y
    );
    column.z = column.x + 1.0;

    column.xy = (column.xy + column.xy) / vert.view_size - vec2(1);
    column.zw = (column.zw + column.zw) / vert.view_size - vec2(1);
    column.y  = -column.y;
    column.w  = -column.w;

    // Read buffer data
    float tile_idx = vert.tile_begin_idx + sample_idx * samples_per_pixel;

    // If we pad the storage buffer by 1 on each side with real values, then we can get nicer looing clipped edges
    uint idx      = min(int(tile_idx),     vert.tile_end_idx - 1);
    uint idx_prev = max(int(tile_idx) - 1, vert.tile_begin_idx);
    uint idx_next = min(int(tile_idx) + 1, vert.tile_end_idx - 1);

    float sine_y      = sine_buffer[idx].y;
    float sine_y_prev = sine_buffer[idx_prev].y;
    float sine_y_next = sine_buffer[idx_next].y;

    // sine_y      = sine_y      * 2 - 1;
    // sine_y_prev = sine_y_prev * 2 - 1;
    // sine_y_next = sine_y_next * 2 - 1;

    // build points
    pt_prev = vec2(p.x - pixels_per_sample, sine_y_prev);
    pt      = vec2(p.x                    , sine_y);
    pt_next = vec2(p.x + pixels_per_sample, sine_y_next);

    pt_prev.x - max(pt_prev.x, vert.topleft.x);
    pt_next.x - min(pt_next.x, vert.bottomright.x);

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,

    // Is odd
    bool is_right  = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    gl_Position = vec4(
        is_right  ? column.z : column.x,
        is_bottom  ? column.w : column.y,
        1,
        1);
    p = vec2(
        -1 + sample_idx * pixels_per_sample,
        is_bottom ? -1 : 1);
    colour = vert.colour;

    stroke_width = vert.stroke_width / vert.view_size.y * 2;
}
@end

@fs fs_line_stroke_tiled

in vec2 p;
in flat uint  colour;
in flat float stroke_width;
in flat vec2 pt_prev;
in flat vec2 pt;
in flat vec2 pt_next;


out vec4 frag_colour;

float sdSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2  ba = b - a;
    vec2  pa = p - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

void main()
{
    float d1 = sdSegment(p, pt_prev, pt);
    float d2 = sdSegment(p, pt, pt_next);
    float d = min(d1, d2);

    float sine_y = pt.y;
    float shape_vertical   = smoothstep(stroke_width, 0, abs(d));
    float shape_horizontal = smoothstep(stroke_width, 0, abs(sine_y - p.y));
    float shape            = max(shape_vertical, shape_horizontal);

    // vec4 col_line = unpackUnorm4x8(colour).abgr;
    // col_line.a *= shape;
    // frag_colour = col_line;

    // Show tiles
    vec4 col_bg = vec4(0, 0.5, 0.5, 1);
    vec4 col_line = unpackUnorm4x8(colour).abgr;
    frag_colour = mix(col_bg, col_line, shape);
}
@end

/* quad shader program */
@program quad_novbuf vs_quad_novbuf fs_quad_novbuf

@program line_stroke_tiled vs_line_stroke_tiled fs_line_stroke_tiled