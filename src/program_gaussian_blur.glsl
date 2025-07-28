@vs triangle_vs
in vec4 position;
in vec4 colour0;

out vec4 colour;

void main() {
    gl_Position = position;
    colour = colour0;
}
@end

@fs triangle_fs
in vec4 colour;
out vec4 frag_colour;

void main() {
    frag_colour = colour;
}
@end

@vs texquad_vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = vec2(texcoord0.x, -texcoord0.y);
}
@end

@fs texquad_fs
layout(binding=0)uniform texture2D tex;
layout(binding=0)uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@fs blur_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_blur {
    vec2 u_pixel_size;
};

void main() {
    // Shader naively yoinked from here:
    // https://github.com/VitalAudio/visage/blob/main/visage_graphics/shaders/fs_blur.sc
    // Visage appears to also do some downsampling with their bloom effects. I recall watching YT video about bloom effects where the guy
    // explaining the effect mentions how he did the same thing, and how downsampling has much better performance too.
    // TODO: rewatch this YT video

    vec2 offset = 1.3333333333333333 * u_pixel_size.xy;
    frag_colour = texture(sampler2D(tex, smp), uv) * 0.29411764705882354;
    frag_colour += texture(sampler2D(tex, smp), uv + offset) * 0.35294117647058826;
    frag_colour += texture(sampler2D(tex, smp), uv - offset) * 0.35294117647058826;
}
@end

@program triangle triangle_vs triangle_fs
@program texquad texquad_vs texquad_fs
@program blur texquad_vs blur_fs