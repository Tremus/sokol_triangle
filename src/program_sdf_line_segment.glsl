@vs vs
out vec2 uv;
out flat float x_scale;
out flat float stroke_width;

layout(binding = 0) uniform vs_position {
    vec2 u_topleft;
    vec2 u_bottomright;
    vec2 u_size;

    float u_stroke_width;
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
    x_scale      = u_size.x / u_size.y;
    stroke_width = u_stroke_width / u_size.y * 0.5;
}
@end

@fs fs
in       vec2  uv;
in  flat float x_scale;
in  flat float stroke_width;

out      vec4  frag_colour;

float sdSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2  ba = b - a;
    vec2  pa = p - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

void main() {
    // points
    vec2 p = uv * 2 - 1;
    p.x *= x_scale;
    vec2 a = vec2(-1.0, -0.5);
    vec2 b = vec2(0.8, 0.5);
    vec2 c = vec2(1.2, -0.5);

    float d1 = sdSegment(p, a, b);
    float d2 = sdSegment(p, b, c);
    float d = min(d1, d2);

    vec3 col_bg   = vec3(0.9, 0.6, 0.3);
    vec3 col_line = vec3(1.0);
    float feather = 0.005;
    float shape = smoothstep(stroke_width + feather, stroke_width, abs(d));
    vec3 col = mix(col_bg, col_line, shape);
    frag_colour = vec4(col, 1.0);
}
@end

@program minimal vs fs