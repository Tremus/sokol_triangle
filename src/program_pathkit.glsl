@vs vs
in vec4 position;

void main() {
    gl_Position = position;
}
@end

@fs fs
out vec4 frag_color;

uniform fs_uniforms {
    vec4 col;
};

void main() {
    frag_color = col;
}
@end

@program pathkit vs fs