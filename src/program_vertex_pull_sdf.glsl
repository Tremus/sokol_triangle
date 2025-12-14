@vs vs

// Tightly packed rectangle
struct myvertex
{
    vec2 topleft;
    vec2 bottomright;

    uint sdf_type;
    uint col_type;
    uint colour1;
    uint colour2;

    vec4 border_radius; // rounded rectangles

    float stroke_width;
    float feather;

    float start_angle; // Arcs, pies, 
    float end_angle;

    vec2 linear_gradient_begin;
    vec2 linear_gradient_end;

    vec2 radial_gradient_pos;
    vec2 radial_gradient_radius;
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
out flat uint sdf_type;
out flat uint col_type;
out flat uint colour1;
out flat uint colour2;
out flat vec4 border_radius;
out flat float feather;
out flat float stroke_width;
out flat float start_angle;
out flat float end_angle;
out flat vec2 linear_gradient_begin;
out flat vec2 linear_gradient_end;
out flat vec2 radial_gradient_pos;
out flat vec2 radial_gradient_radius_scale;

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
    // colour1 = unpackUnorm4x8(vert.colour1).abgr; // swizzle
    // colour2 = unpackUnorm4x8(vert.colour2).abgr; // swizzle
    colour1 = vert.colour1;
    colour2 = vert.colour2;
    sdf_type = vtx[v_idx].sdf_type;
    col_type = vtx[v_idx].col_type;

    float smallest_dimension = min(vw, vh);
    border_radius = vtx[v_idx].border_radius / vec4(smallest_dimension * 0.5);
    // Good artical on setting the right feather size
    // https://bohdon.com/docs/smooth-sdf-shape-edges/
    // feather = 16.0 / min(size.x, size.y);
    // feather = 0.01 * (vw > vh ? size.x / vw : size.y / vh);
    // feather = 0.01;
    feather = vert.feather;
    // stroke_width = 0.5 * vert.stroke_width / min(vw, vh);
    stroke_width = 2 * vert.stroke_width / vw;

    start_angle = vtx[v_idx].start_angle;
    end_angle   = vtx[v_idx].end_angle;

    linear_gradient_begin        = (vtx[v_idx].linear_gradient_begin - vtx[v_idx].topleft) / vec2(vw, vh);
    linear_gradient_end          = (vtx[v_idx].linear_gradient_end   - vtx[v_idx].topleft) / vec2(vw, vh);

    radial_gradient_pos          = (vtx[v_idx].radial_gradient_pos   - vtx[v_idx].topleft) / vec2(vw, vh);
    radial_gradient_radius_scale = vec2(vw, vh) / vtx[v_idx].radial_gradient_radius;
}
@end

@fs fs
in vec2 uv;
in flat vec2 uv_xy_scale;
in flat uint sdf_type;
in flat uint col_type;
in flat uint colour1;
in flat uint colour2;
in flat vec4 border_radius;
in flat float feather;
in flat float stroke_width;
in flat float start_angle;
in flat float end_angle;
in flat vec2 linear_gradient_begin;
in flat vec2 linear_gradient_end;
in flat vec2 radial_gradient_pos;
in flat vec2 radial_gradient_radius_scale;

out vec4 frag_color;

#define PI 3.141592653589793
#define SDF_TYPE_RECTANGLE_FILL   0
#define SDF_TYPE_RECTANGLE_STROKE 1
#define SDF_TYPE_CIRCLE_FILL      2
#define SDF_TYPE_CIRCLE_STROKE    3
#define SDF_TYPE_TRIANGLE_FILL    4
#define SDF_TYPE_TRIANGLE_STROKE  5
#define SDF_TYPE_PIE_FILL         6
#define SDF_TYPE_PIE_STROKE       7
#define SDF_TYPE_ARC_ROUND_STROKE 8
#define SDF_TYPE_ARC_BUTT_STROKE  9

#define SDF_COLOUR_SOLID           0
#define SDF_COLOUR_LINEAR_GRADEINT 1
#define SDF_COLOUR_RADIAL_GRADEINT 2
#define SDF_COLOUR_CONIC_GRADEINT  3
#define SDF_COLOUR_BOX_GRADEINT    4

// The MIT License
// Copyright © 2017 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// b.x = half width
// b.y = half height
// r.x = roundness top-right  
// r.y = roundness boottom-right
// r.z = roundness top-left
// r.w = roundness bottom-left
float sdRoundBox(in vec2 p, in vec2 b, in vec4 r)
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

// (r is half the base)
float sdEquilateralTriangle( in vec2 p, in float r )
{
    const float k = sqrt(3.0);
    p.x = abs(p.x);
    p -= vec2(0.5,0.5*k)*max(p.x+k*p.y,0.0);
    p -= vec2(clamp(p.x,-r,r),-r/k );
    return length(p)*sign(-p.y);
}

// c is the sin/cos of the angle. r is the radius
float sdPie( in vec2 p, in vec2 c, in float r )
{
    p.x = abs(p.x);
    float l = length(p) - r;
	float m = length(p - c*clamp(dot(p,c),0.0,r) );
    return max(l,m*sign(c.y*p.x-c.x*p.y));
}

// sc is the sin/cos of the aperture
float sdArc( in vec2 p, in vec2 sc, in float ra, float rb )
{
    p.x = abs(p.x);
    return ((sc.y*p.x>sc.x*p.y) ? length(p-sc*ra) : 
                                  abs(length(p)-ra)) - rb;
}

float sdRing( in vec2 p, in vec2 n, in float r, float th )
{
    p.x = abs(p.x);
    p = mat2x2(n.x,n.y,-n.y,n.x)*p;
    return max( abs(length(p)-r)-th*0.5,
                length(vec2(p.x,max(0.0,abs(r-p.y)-th*0.5)))*sign(p.x) );
}

void main()
{
    float shape = 1;
    vec4 col = vec4(0);
    if (sdf_type == SDF_TYPE_RECTANGLE_FILL)
    {
        vec2 b = uv_xy_scale;
        float d = sdRoundBox(uv * uv_xy_scale, b, border_radius);
        shape = smoothstep(feather, 0, d + feather * 0.5);
    }
    else if (sdf_type == SDF_TYPE_RECTANGLE_STROKE)
    {
        vec2 b = uv_xy_scale;
        float d = sdRoundBox(uv * uv_xy_scale, b, border_radius);
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
    else if (sdf_type == SDF_TYPE_PIE_FILL)
    {
        // TODO handle rotaion
        float amt = (end_angle - start_angle);
        vec2 c = vec2(sin(amt), cos(amt));
        float d = sdPie(uv, c, 1.0);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_TYPE_PIE_STROKE)
    {
        float amt = (end_angle - start_angle);
        vec2 c = vec2(sin(amt), cos(amt));
        float d = sdPie(uv, c, 1.0);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + feather * 0.5 + stroke_width);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_TYPE_ARC_ROUND_STROKE)
    {
        float amt = (end_angle - start_angle);
        vec2 c = vec2(sin(amt), cos(amt));
        float d = sdArc(uv, c, 1.0 - stroke_width * 0.5, stroke_width * 0.5);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_TYPE_ARC_BUTT_STROKE)
    {
        float amt = (end_angle - start_angle);
        vec2 c = vec2(cos(amt), sin(amt));
        float d = sdRing(uv, c, 1.0 - stroke_width * 0.5, stroke_width);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }

    if (col_type == SDF_COLOUR_SOLID)
    {
        col = unpackUnorm4x8(colour1).abgr; // swizzle
    }
    else if (col_type == SDF_COLOUR_LINEAR_GRADEINT)
    {
        vec2 lmao = vec2(uv.x * 0.5 + 0.5,  uv.y * -0.5 + 0.5);

        vec2 v  = linear_gradient_begin - linear_gradient_end;
        vec2 w  = linear_gradient_begin - lmao;
        float t = dot(v, w) / dot(v, v);
        t = clamp(t, 0, 1);
        col = mix(unpackUnorm4x8(colour1).abgr, unpackUnorm4x8(colour2).abgr, t);
    }
    else if (col_type == SDF_COLOUR_RADIAL_GRADEINT)
    {
        vec2 lmao = vec2(uv.x * 0.5 + 0.5,  uv.y * -0.5 + 0.5);
        vec2 ellipse_space = (lmao - radial_gradient_pos) * radial_gradient_radius_scale;
        float t = clamp(length(ellipse_space), 0.0, 1.0);
        col = mix(unpackUnorm4x8(colour1).abgr, unpackUnorm4x8(colour2).abgr, t);
    }
    else if (col_type == SDF_COLOUR_CONIC_GRADEINT)
    {
        // Rotate the gradient
        float rotate_amt = PI * 0.75;
        vec2 rotated = vec2(uv.x * cos(rotate_amt) - uv.y * sin(rotate_amt),
                            uv.x * sin(rotate_amt) + uv.y * cos(rotate_amt));
        float angle = atan(rotated.x, rotated.y);
        
        float angle_start = -PI * 0.5;
        float angle_end = PI * 0.5;

        // Clamps the gradient
        float t = smoothstep(angle_start, angle_end, angle);

        col = mix(unpackUnorm4x8(colour1).abgr, unpackUnorm4x8(colour2).abgr, t);
    }

    col.a *= shape;
    // col = shape == 0 ? (vec4(1) - col) : col;

    frag_color = col;
}
@end

@program vertexpull vs fs