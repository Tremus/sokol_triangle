@vs vs_quad_novbuf
out vec2 uv;
out vec4 colour;
out flat float param;

layout(binding = 0) uniform vs_position {
    vec2 u_topleft;
    vec2 u_bottomright;
    vec2 u_size;

    float u_param;
    int u_colour_left;
    int u_colour_right;
};

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? u_bottomright.x : u_topleft.x,
        is_bottom ? u_bottomright.y : u_topleft.y
    );
    pos = (pos + pos) / u_size - vec2(1);
    pos.y = -pos.y;

    gl_Position = vec4(pos, 1, 1);
    uv = vec2(
        is_right  ? 1 : 0,
        is_bottom ? 1 : 0
    );

    uint col = is_right ? u_colour_right : u_colour_left;
    colour = unpackUnorm4x8(col).abgr;
    param = u_param;
}
@end

@fs fs_quad_novbuf
in vec2 uv;
in vec4 colour;
in flat float param;


out vec4 frag_color;

void main() {

    frag_color = uv.x < param ? colour : vec4(0);
}
@end

@vs vs_fullscreen
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end

// Based off of this article https://medium.com/@victorbaro/implementing-a-refractive-glass-shader-in-metal-3f97974fbc24
@fs fs_tex
layout(binding = 0) uniform texture2D tex;
layout(binding = 0) uniform sampler smp;

layout(binding=0) uniform fs_liquidglass {
    float u_radius;
    float u_refraction;
};

in vec2 uv;
out vec4 frag_color;

vec4 layer_sample(in vec2 tex_pos)
{
    return texture(sampler2D(tex, smp), tex_pos);
}

void main() {
    vec2 glassCenter = vec2(0.5);
    vec2 toCenter = uv - glassCenter;

    float dist = length(toCenter);
    float normalizedDist = dist / u_radius;

    if (dist < u_radius)
    {
        float distortion = 1.0 - normalizedDist * normalizedDist;
        vec2 offset = toCenter * distortion * u_refraction;

        frag_color = layer_sample(uv + offset);
        frag_color += vec4(vec3(0.4),1);
        // frag_color = vec4(normalizedDist, normalizedDist, normalizedDist, 1);
    }
    else
    {
        vec4 texcol = layer_sample(uv);
        frag_color = texcol;
    }
}
@end

@program quad_novbuf vs_quad_novbuf fs_quad_novbuf
@program texquad vs_fullscreen fs_tex