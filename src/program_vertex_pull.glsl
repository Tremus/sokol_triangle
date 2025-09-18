@vs vs

struct sb_vertex {
    vec2 pos;
};

layout(binding=0) readonly buffer ssbo {
    sb_vertex vtx[];
};

void main() {
    vec4 position = vec4(vtx[gl_VertexIndex].pos, 1, 1);
    gl_Position = position;
}
@end

@fs fs
out vec4 frag_color;

void main() {
    frag_color = vec4(1);;
}
@end

@program vertexpull vs fs