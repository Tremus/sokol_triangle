@vs vs_pathkit
in vec4 position;

void main() {
    gl_Position = position;
}
@end

@fs fs_pathkit
out vec4 frag_color;

uniform fs_uniforms {
    vec4 col;
};

void main() {
    frag_color = col;
}
@end

@program pathkit vs_pathkit fs_pathkit

/* texquad vertex shader */
@vs vs_display
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = vec2(texcoord0.x, -texcoord0.y);
}
@end

/* texquad fragment shader */
@fs fs_display
uniform texture2D tex;
uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

/* texquad shader program */
@program display vs_display fs_display