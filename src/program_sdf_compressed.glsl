@vs vs

// Tightly packed rectangle
struct myvertex
{
    vec2 topleft;
    vec2 bottomright;

    // unorm4x8 comprised of:
    //   - uint sdf_type;   // could probably be compressed to one byte
    //   - uint grad_type;
    //     stroking is usually in the range of 0.8-8px. We could set an arbitrary maximum of 16px
    //     stroke widths such as 1.2, 2.5px etc are common
    //   - float stroke_width;
    //   - float feather;
    uint sdf_data;

    // packed with either:
    // - border radius (unorm4x8)
    // - arc/pie rotate and range (unorm2x16)
    uint borderradius_arcpie;

    uint colour1;
    uint colour2;

    vec2 gradient_a;
    vec2 gradient_b;

    // uint  texid; // ???
};

layout(binding=0) readonly buffer ssbo {
    myvertex vtx[];
};

layout(binding=0) uniform vs_params {
    vec2 size;
};

#define PI 3.141592653589793

out vec2 uv;
out flat vec2 uv_xy_scale;

out flat uint sdf_type;
out flat uint grad_type;
out flat float feather;
out flat float stroke_width;

out flat uint colour1;
out flat uint colour2;

// either:
// - border_radius
// - vec4(cos(rotate), sin(rotate), sin(range), cos(range))
out flat vec4 borderradius_arcpie;

// linear_gradient_begin
// radial_gradient_pos
// conic_gradient_rotate
out flat vec2 gradient_a;
// linear_gradient_end
// radial_gradient_radius_scale
// conic_gradient_angle
out flat vec2 gradient_b;

#define SDF_SHAPE_RECTANGLE_FILL   1
#define SDF_SHAPE_RECTANGLE_STROKE 2
#define SDF_SHAPE_CIRCLE_FILL      3
#define SDF_SHAPE_CIRCLE_STROKE    4
#define SDF_SHAPE_TRIANGLE_FILL    5
#define SDF_SHAPE_TRIANGLE_STROKE  6
#define SDF_SHAPE_PIE_FILL         7
#define SDF_SHAPE_PIE_STROKE       8
#define SDF_SHAPE_ARC_ROUND_STROKE 9
#define SDF_SHAPE_ARC_BUTT_STROKE  10

#define SDF_GRADEINT_SOLID  0
#define SDF_GRADEINT_LINEAR 1
#define SDF_GRADEINT_RADIAL 2
#define SDF_GRADEINT_CONIC  3
#define SDF_GRADEINT_BOX    4

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

    colour1 = vert.colour1;
    colour2 = vert.colour2;

    vec4 sdf_data = unpackUnorm4x8(vert.sdf_data);
    vec2 arcpie   = 2 * PI * unpackUnorm2x16(vert.borderradius_arcpie);

    sdf_type     = uint(sdf_data.x * 255);
    grad_type    = uint(sdf_data.y * 255); 
    feather      =      sdf_data.z * 1;
    stroke_width =      sdf_data.w * 16;
    stroke_width = 2 * stroke_width / vw * uv_xy_scale.x;

    if (sdf_type == SDF_SHAPE_RECTANGLE_FILL ||
        sdf_type == SDF_SHAPE_RECTANGLE_STROKE)
    {
        borderradius_arcpie = (unpackUnorm4x8(vert.borderradius_arcpie) * 255) / vec4(vh * 0.5);        
    }

    if (sdf_type == SDF_SHAPE_TRIANGLE_FILL    ||
        sdf_type == SDF_SHAPE_TRIANGLE_STROKE  ||
        sdf_type == SDF_SHAPE_PIE_FILL         ||
        sdf_type == SDF_SHAPE_PIE_STROKE       ||
        sdf_type == SDF_SHAPE_ARC_ROUND_STROKE ||
        sdf_type == SDF_SHAPE_ARC_BUTT_STROKE)
    {
        borderradius_arcpie.xy = vec2(cos(arcpie.x), sin(arcpie.x));
        borderradius_arcpie.zw = vec2(sin(arcpie.y), cos(arcpie.y));
    }

    if (grad_type == SDF_GRADEINT_LINEAR)
    {
        gradient_a = (vert.gradient_a - vert.topleft) / vec2(vw, vh);  // stop 1 xy
        gradient_b = (vert.gradient_b   - vert.topleft) / vec2(vw, vh); // stop 2 xy
    }
    else if (grad_type == SDF_GRADEINT_RADIAL)
    {
        gradient_a = (vert.gradient_a   - vert.topleft) / vec2(vw, vh); // stop 2 cx,cy
        gradient_b = vec2(vw, vh) / vert.gradient_b;                    // stop 1 radius
    }
    else if (grad_type == SDF_GRADEINT_CONIC)
    {
        gradient_a = vec2(cos(vert.gradient_a.x), sin(vert.gradient_a.x)); // rotation, radians
        gradient_b = vert.gradient_b;                                      // range, radians
    }
    else if (grad_type == SDF_GRADEINT_BOX)
    {
        gradient_a = vec2(vert.gradient_a) / vec2(-vw, vh); // translate x/y
        gradient_b = vec2(vert.gradient_b     / vh);        // blur radius
    }
}
@end

@fs fs
precision mediump float;

in vec2 uv;
in flat vec2 uv_xy_scale;

in flat uint sdf_type;
in flat uint grad_type;
in flat float feather;
in flat float stroke_width;

in flat uint colour1;
in flat uint colour2;

in flat vec4 borderradius_arcpie;

in flat vec2 gradient_a;
in flat vec2 gradient_b;

out vec4 frag_color;

#define PI 3.141592653589793
#define SDF_SHAPE_RECTANGLE_FILL   1
#define SDF_SHAPE_RECTANGLE_STROKE 2
#define SDF_SHAPE_CIRCLE_FILL      3
#define SDF_SHAPE_CIRCLE_STROKE    4
#define SDF_SHAPE_TRIANGLE_FILL    5
#define SDF_SHAPE_TRIANGLE_STROKE  6
#define SDF_SHAPE_PIE_FILL         7
#define SDF_SHAPE_PIE_STROKE       8
#define SDF_SHAPE_ARC_ROUND_STROKE 9
#define SDF_SHAPE_ARC_BUTT_STROKE  10

#define SDF_GRADEINT_SOLID  0
#define SDF_GRADEINT_LINEAR 1
#define SDF_GRADEINT_RADIAL 2
#define SDF_GRADEINT_CONIC  3
#define SDF_GRADEINT_BOX    4

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
    p = mat2x2(-n.x,-n.y,n.y,-n.x)*p;
    return max( abs(length(p)-r)-th*0.5,
                length(vec2(p.x,max(0.0,abs(r-p.y)-th*0.5)))*sign(p.x) );
}

void main()
{
    float shape = 1;
    vec4 col = vec4(0);
    if (sdf_type == SDF_SHAPE_RECTANGLE_FILL)
    {
        vec2 b = uv_xy_scale;
        float d = sdRoundBox(uv * uv_xy_scale, b, borderradius_arcpie);
        shape = smoothstep(feather, 0, d + feather * 0.5);
    }
    else if (sdf_type == SDF_SHAPE_RECTANGLE_STROKE)
    {
        vec2 b = uv_xy_scale;
        float d = sdRoundBox(uv * uv_xy_scale, b, borderradius_arcpie);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + stroke_width + feather * 0.5);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_SHAPE_CIRCLE_FILL)
    {
        float d = 1 - length(uv);
        float outer = smoothstep(0, feather, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_SHAPE_CIRCLE_STROKE)
    {
        float d = 1 - length(uv);
        float outer = smoothstep(0, feather, d + feather * 0.5);
        float inner = smoothstep(0, feather, d + feather * 0.5 - stroke_width);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_SHAPE_TRIANGLE_FILL)
    {
        vec2 p = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                      uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdEquilateralTriangle(p, 0.86);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_SHAPE_TRIANGLE_STROKE)
    {
        vec2 p = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                      uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdEquilateralTriangle(p, 0.86);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + feather * 0.5 + stroke_width);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_SHAPE_PIE_FILL)
    {
        vec2 uv_rotated = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                               uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdPie(uv_rotated, borderradius_arcpie.zw, 1.0);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_SHAPE_PIE_STROKE)
    {
        vec2 uv_rotated = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                               uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdPie(uv_rotated, borderradius_arcpie.zw, 1.0);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        float inner = smoothstep(feather, 0, d + feather * 0.5 + stroke_width);
        shape = outer - inner;
    }
    else if (sdf_type == SDF_SHAPE_ARC_ROUND_STROKE)
    {
        vec2 uv_rotated = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                               uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdArc(uv_rotated, borderradius_arcpie.zw, 1.0 - stroke_width * 0.5, stroke_width * 0.5);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }
    else if (sdf_type == SDF_SHAPE_ARC_BUTT_STROKE)
    {
        vec2 uv_rotated = vec2(uv.x * borderradius_arcpie.x - uv.y * borderradius_arcpie.y,
                               uv.x * borderradius_arcpie.y + uv.y * borderradius_arcpie.x);
        float d = sdRing(uv_rotated, borderradius_arcpie.zw, 1.0 - stroke_width * 0.5, stroke_width);
        float outer = smoothstep(feather, 0, d + feather * 0.5);
        shape = outer;
    }

    float t = 0;
    if (grad_type == SDF_GRADEINT_SOLID)
    {
        col = unpackUnorm4x8(colour1).abgr; // swizzle
    }
    else if (grad_type == SDF_GRADEINT_LINEAR)
    {
        vec2 uv_norm = vec2(uv.x * 0.5 + 0.5,  uv.y * -0.5 + 0.5);

        vec2 v  = gradient_a - gradient_b;
        vec2 w  = gradient_a - uv_norm;
        t = dot(v, w) / dot(v, v);
        t = clamp(t, 0, 1);
    }
    else if (grad_type == SDF_GRADEINT_RADIAL)
    {
        // translate & scale
        vec2 uv_norm       = vec2(uv.x * 0.5 + 0.5,  uv.y * -0.5 + 0.5);
        vec2 ellipse_space = (uv_norm - gradient_a) * gradient_b;

        t = clamp(length(ellipse_space), 0.0, 1.0);
    }
    else if (grad_type == SDF_GRADEINT_CONIC)
    {
        // Change start/end position of the gradient
        vec2 p = uv * uv_xy_scale;
        vec2 uv_rotated = vec2(p.x * gradient_a.x - p.y * gradient_a.y,
                               p.x * gradient_a.y + p.y * gradient_a.x);
        float angle = atan(uv_rotated.x, uv_rotated.y);

        // Crops the gradient range
        float range = gradient_b.x;
        t = angle / (PI * 2) + 0.5;
        t = smoothstep(0, range, t);
    }
    else if (grad_type == SDF_GRADEINT_BOX)
    {
        vec2  xy_offset   = gradient_a;
        float blur_radius = gradient_b.x;

        vec2 p = (uv + xy_offset) * uv_xy_scale;
        vec2 half_wh  = uv_xy_scale - blur_radius * 2;
        vec4 br = borderradius_arcpie - blur_radius;

        float d = sdRoundBox(p, half_wh, br);
        t = 1 - d / (blur_radius * 2);
        t = clamp(t, 0, 1);
    }
    col = mix(unpackUnorm4x8(colour1).abgr, unpackUnorm4x8(colour2).abgr, t);

    col.a *= shape;
    // col = shape == 0 ? (vec4(1) - col) : col;

    frag_color = col;
}
@end

@program vertexpull vs fs