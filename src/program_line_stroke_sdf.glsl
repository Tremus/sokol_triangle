@vs vs
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 p;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    p = pos;
}
@end

@fs fs

in vec2 p;
out vec4 frag_colour;

layout(binding=0) uniform fs_uniforms {
    vec2  u_view_size;
    float u_stroke_width;
    int   u_buffer_length;
};

struct buffer_item {
    float y;
};

layout(binding=0)readonly buffer storage_buffer {
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
    float stroke_width = u_stroke_width / u_view_size.y * 2;
    // float y_scale = 1.0;
    // float y_scale = 1.0 - stroke_width;
    float y_scale = 0.8;
    float px_inc = 2.0 / u_view_size.x;

    // Read buffer data

    // If we pad the storage buffer by 1 on each side with real values, then we can get nicer looing clipped edges
    float uvx     = p.x * 0.5 + 0.5;
    uint idx      = min(int(uvx * u_view_size.x),     u_buffer_length - 1);
    uint idx_prev = max(int(uvx * u_view_size.x) - 1, 0);
    uint idx_next = min(int(uvx * u_view_size.x) + 1, u_buffer_length - 1);

    float sine_y      = sine_buffer[idx].y      * y_scale;
    float sine_y_prev = sine_buffer[idx_prev].y * y_scale;
    float sine_y_next = sine_buffer[idx_next].y * y_scale;

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

    vec3 col_bg   = vec3(0);
    vec3 col_line = vec3(1.0);
    vec3 col = mix(col_bg, col_line, shape);
    frag_colour = vec4(col, 1.0);
}
@end

@program line_stroke vs fs