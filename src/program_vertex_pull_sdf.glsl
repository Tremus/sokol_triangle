@vs vs

// Tightly packed rectangle
struct myvertex
{
    vec2 topleft;
    vec2 bottomright;
    uint sdf_type;
    uint colour1;
    float stroke_width;
    float feather;
    // TODO
    // uint  texid;
};

layout(binding=0) readonly buffer ssbo {
    myvertex vtx[];
};

layout(binding=0) uniform vs_params {
    vec2 size;
};

out vec2 uv;
out flat vec2 uv_xy_scale;
out flat vec4 colour1;
out flat uint sdf_type;
out flat float feather;
out flat float stroke_width;

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    myvertex vert = vtx[v_idx];

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? vert.bottomright.x : vert.topleft.x,
        is_bottom ? vert.bottomright.y : vert.topleft.y
    );

    pos = (pos + pos) / size - vec2(1);
    pos.y = -pos.y;

    float vw = vert.bottomright.x - vert.topleft.x;
    float vh = vert.bottomright.y - vert.topleft.y;

    gl_Position = vec4(pos, 1, 1);

    uv = vec2(is_right  ? 1 : -1, is_bottom ? -1 : 1);
    // uv_xy_scale = vec2(vw > vh ? vw / vh : 1, vh > vw ? vh / vw : 1);
    uv_xy_scale = vec2(vw / vh, 1);
    colour1 = unpackUnorm4x8(vert.colour1).abgr; // swizzle
    sdf_type = vtx[v_idx].sdf_type;
    // Good artical on setting the right feather size
    // https://bohdon.com/docs/smooth-sdf-shape-edges/
    // feather = 16.0 / min(size.x, size.y);
    // feather = 0.01 * (vw > vh ? size.x / vw : size.y / vh);
    // feather = 0.01;
    feather = vert.feather;
    // stroke_width = 0.5 * vert.stroke_width / min(vw, vh);
    stroke_width = 2 * vert.stroke_width / vw;
}
@end

@fs fs
in vec2 uv;
in flat vec2 uv_xy_scale;
in flat vec4 colour1;
in flat uint sdf_type;
in flat float feather;
in flat float stroke_width;

out vec4 frag_color;

#define SDF_TYPE_RECTANGLE_FILL   0
#define SDF_TYPE_RECTANGLE_STROKE 1
#define SDF_TYPE_CIRCLE_FILL      2
#define SDF_TYPE_CIRCLE_STROKE    3
#define SDF_TYPE_TRIANGLE_FILL    4
#define SDF_TYPE_TRIANGLE_STROKE  5

float sdRoundBox(in vec2 p, in vec2 b, in vec4 r)
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

float sdEquilateralTriangle( in vec2 p, in float r )
{
    const float k = sqrt(3.0);
    p.x = abs(p.x);
    p -= vec2(0.5,0.5*k)*max(p.x+k*p.y,0.0);
    p -= vec2(clamp(p.x,-r,r),-r/k );
    return length(p)*sign(-p.y);
}

void main()
{
    float shape = 1;
    if (sdf_type == SDF_TYPE_RECTANGLE_FILL)
    {
        vec2 b = uv_xy_scale;
        vec4 r = vec4(0.5);
        float d = sdRoundBox(uv * uv_xy_scale, b, r);
        shape = smoothstep(feather, 0, d + feather * 0.5);
    }
    else if (sdf_type == SDF_TYPE_RECTANGLE_STROKE)
    {
        vec2 b = uv_xy_scale;
        vec4 r = vec4(0.5);

        float d = sdRoundBox(uv * uv_xy_scale, b, r);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + stroke_width * 4 + feather * 0.5);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_TYPE_CIRCLE_FILL)
    {
        float d = 1 - length(uv);
        float outer = smoothstep(0, feather, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_TYPE_CIRCLE_STROKE)
    {
        float d = 1 - length(uv);
        float outer = smoothstep(0, feather, d + feather * 0.5);
        float inner = smoothstep(0, feather, d + feather * 0.5 - stroke_width);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_TYPE_TRIANGLE_FILL)
    {
        vec2 p = uv;
        // p = -p; // point down
        // p = vec2(p.y, p.x); // point right
        // p = vec2(p.y, -p.x); // point left
        float d = sdEquilateralTriangle(p, 0.86);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_TYPE_TRIANGLE_STROKE)
    {
        vec2 p = uv;
        // p = -p; // point down
        // p = vec2(p.y, p.x); // point right
        // p = vec2(p.y, -p.x); // point left
        float d = sdEquilateralTriangle(p, 0.86);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + feather * 0.5 + stroke_width);
        shape = outer - inner;
    }
    vec4 col = vec4(colour1.rgb, shape);
    // col = shape == 0 ? vec4(1) : col;

    frag_color = col;
}
@end

@program vertexpull vs fs