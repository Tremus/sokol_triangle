@vs vs
layout(binding = 0) uniform vs_uniforms {
    vec2 topleft;
    vec2 bottomright;
    vec2 size;

    vec2 img_topleft;
    vec2 img_bottomright;
};

out vec2 texcoord;

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

    texcoord = vec2(
        is_right  ? img_bottomright.x : img_topleft.x,
        is_bottom ? img_bottomright.y : img_topleft.y
    );
}
@end

@fs fs
layout(binding = 0) uniform texture2D tex;
layout(binding = 0) uniform sampler smp;

in vec2 texcoord;
out vec4 frag_color;

void main() {

    ivec2 itexcoord = ivec2(texcoord);
    frag_color = texelFetch(sampler2D(tex, smp), itexcoord, 0);


    // frag_color = texture(sampler2D(tex, smp), uv);
    // frag_color += vec4(uv, 1, 1);
}
@end

/* texquad shader program */
@program nanosvg vs fs