@vs vs

// Tightly packed rectangle
struct sb_rect {
    vec2 topleft;
    vec2 bottomright;
};

layout(binding=0) readonly buffer ssbo {
    sb_rect rects[];
};

void main() {
    int rect_idx = gl_VertexIndex / 6;
    int indices_idx = gl_VertexIndex - rect_idx * 6;

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,
    // sb_rect rect = rects[rect_idx];

    vec4 pos = vec4(rects[rect_idx].topleft, 1, 1);
    int is_odd = gl_VertexIndex & 1;
    if (is_odd == 1)
        pos.x = rects[rect_idx].bottomright.x;
    if (indices_idx >= 2 && indices_idx <= 4) // idx 2/3/4
        pos.y = rects[rect_idx].bottomright.y;

    gl_Position = pos;
}
@end

@fs fs
out vec4 frag_color;

void main() {
    frag_color = vec4(1);;
}
@end

@program vertexpull vs fs