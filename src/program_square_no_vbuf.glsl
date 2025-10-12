@vs vs
out vec2 uv;

layout(binding = 0) uniform vs_position {
    vec2 topleft;
    vec2 bottomright;
    vec2 size;
};

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? bottomright.x : topleft.x,
        is_bottom ? bottomright.y : topleft.y
    );
    pos = (pos + pos) / size - vec2(1);
    pos.y = -pos.y;

    gl_Position = vec4(pos, 1, 1);
    uv = vec2(
        is_right  ? 1 : 0,
        is_bottom ? 1 : 0
    );
}
@end

@fs fs
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(uv, 0, 1);
}
@end

/* quad shader program */
@program quad_novbuf vs fs