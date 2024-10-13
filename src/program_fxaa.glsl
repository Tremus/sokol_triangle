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

/* fxaa vertex shader */
@vs fxaa_vs
in vec4 position;
in vec2 texcoord0;
out vec2 tex_uv;

void main() {
    gl_Position = position;
    tex_uv = vec2(texcoord0.x, -texcoord0.y);
}
@end

/* fxaa fragment shader */
@fs fxaa_fs

// Source
// https://github.com/BennyQBD/3DEngineCpp/blob/054c2dcd7c52adcf8c9da335a2baee78850504b8/res/shaders/filter-fxaa.fs

// FXAA shader, GLSL code adapted from:
// http://horde3d.org/wiki/index.php5?title=Shading_Technique_-_FXAA
// Whitepaper describing the technique:
// http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf

in vec2 tex_uv;
out vec4 frag_color;

uniform texture2D tex;
uniform sampler smp;

uniform fs_fxaa {
    vec2 tex_size;
    float fxaa_span_max;
    float fxaa_reduce_min;
    float fxaa_reduce_mul;
};

void main()
{
	vec2 inv_size = 1.0 / tex_size;

	vec3 luma = vec3(0.299, 0.587, 0.114);

    vec2 uv_offset_TL = tex_uv.xy + vec2(-1.0, -1.0) * inv_size;
    vec2 uv_offset_TR = tex_uv.xy + vec2( 1.0, -1.0) * inv_size;
    vec2 uv_offset_BL = tex_uv.xy + vec2(-1.0,  1.0) * inv_size;
    vec2 uv_offset_BR = tex_uv.xy + vec2( 1.0,  1.0) * inv_size;

	float lumaTL = dot(luma, texture(sampler2D(tex, smp), uv_offset_TL).xyz);
	float lumaTR = dot(luma, texture(sampler2D(tex, smp), uv_offset_TR).xyz);
	float lumaBL = dot(luma, texture(sampler2D(tex, smp), uv_offset_BL).xyz);
	float lumaBR = dot(luma, texture(sampler2D(tex, smp), uv_offset_BR).xyz);
	float lumaM  = dot(luma, texture(sampler2D(tex, smp), tex_uv.xy).xyz);

	vec2 dir;
	dir.x = -((lumaTL + lumaTR) - (lumaBL + lumaBR));
	dir.y =  ((lumaTL + lumaBL) - (lumaTR + lumaBR));

	float dirReduce = max((lumaTL + lumaTR + lumaBL + lumaBR) * (fxaa_reduce_mul * 0.25), fxaa_reduce_min);
	float inverseDirAdjustment = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = min(vec2(fxaa_span_max, fxaa_span_max), 
		      max(vec2(-fxaa_span_max, -fxaa_span_max), dir * inverseDirAdjustment)) * inv_size;

	vec3 result1 = (1.0/2.0) * (
		texture(sampler2D(tex, smp), tex_uv.xy + (dir * vec2(1.0/3.0 - 0.5))).xyz +
		texture(sampler2D(tex, smp), tex_uv.xy + (dir * vec2(2.0/3.0 - 0.5))).xyz);

	vec3 result2 = result1 * (1.0/2.0) + (1.0/4.0) * (
		texture(sampler2D(tex, smp), tex_uv.xy + (dir * vec2(0.0/3.0 - 0.5))).xyz +
		texture(sampler2D(tex, smp), tex_uv.xy + (dir * vec2(3.0/3.0 - 0.5))).xyz);

	float lumaMin = min(lumaM, min(min(lumaTL, lumaTR), min(lumaBL, lumaBR)));
	float lumaMax = max(lumaM, max(max(lumaTL, lumaTR), max(lumaBL, lumaBR)));
	float lumaResult2 = dot(luma, result2);

	if(lumaResult2 < lumaMin || lumaResult2 > lumaMax)
		frag_color = vec4(result1, 1.0);
	else
		frag_color = vec4(result2, 1.0);

    // frag_color = texture(sampler2D(tex, smp), tex_uv);
}
@end

/* texquad shader program */
@program fxaa fxaa_vs fxaa_fs