@vs offscreen_vs
in vec4 position;
in vec4 color0;

out vec4 color;

void main() {
    gl_Position = position;
    color = color0;
}
@end

@fs offscreen_fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program offscreen offscreen_vs offscreen_fs

/* texquad vertex shader */
@vs display_vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = texcoord0;
}
@end

/* texquad fragment shader */
@fs display_fs
uniform texture2D tex;
uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

/* texquad shader program */
@program display display_vs display_fs