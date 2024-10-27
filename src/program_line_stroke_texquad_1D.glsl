@vs vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = texcoord0;
}
@end

@fs fs

in vec2 uv;
out vec4 frag_color;

uniform fs_uniforms {
    float buffer_max_idx;
    float quad_height_max_idx;
};

struct buffer_item {
    float y;
};

readonly buffer storage_buffer {
    buffer_item sine_buffer[];
};


void main() {
    uint idx = uint(min(uv.x * buffer_max_idx, buffer_max_idx));
    float sine_y = sine_buffer[idx].y;
    float sine_norm = 0.5 + sine_y * 0.5;

    float sine_denorm = sine_norm * quad_height_max_idx;
    float pixel_y_denorm = uv.y * quad_height_max_idx;

    float y_distance = abs(sine_denorm - pixel_y_denorm);

    // Stroke with of 6 (3 pixels in both N & S directions)
    float v = y_distance < 3 ? 1 : 0;

    frag_color = vec4(v, v, v, 1);
}
@end

@program line_stroke vs fs