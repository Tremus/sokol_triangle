@vs vs
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end

@fs fs

in vec2 uv;
out vec4 frag_color;

layout(binding=0) uniform fs_uniforms {
    vec2  mouse_xy;
    float buffer_max_idx;
    float quad_height_max_idx;
};

struct buffer_item {
    float y;
};

layout(binding=0)readonly buffer storage_buffer {
    buffer_item sine_buffer[];
};


void main() {
    uint idx = uint(min(uv.x * buffer_max_idx, buffer_max_idx));
    float sine_y = sine_buffer[idx].y;
    float sine_norm = 0.5 + sine_y * 0.5;

    float sine_denorm = sine_norm * quad_height_max_idx;
    float pixel_y_denorm = uv.y * quad_height_max_idx;

    float y_distance = abs(sine_denorm - pixel_y_denorm);

    float mouse_distance = abs(distance(uv, mouse_xy));

    // Stroke with of 6 (3 pixels in both N & S directions)
    float v = y_distance < 3 ? (1 - mouse_distance) : 0;

    frag_color = vec4(v, v, v, 1);
}
@end

@program line_stroke vs fs