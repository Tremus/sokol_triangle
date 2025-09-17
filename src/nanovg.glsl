// @module nanovg

// @vs vs
// @include internal_shd.vs.glsl
// @end

// @fs fs
// @include internal_shd.fs.glsl
// @end

// @program nvg vs fs

// @module nanovg_aa

@vs vs_aa
@include nanovg_vs.glsl
@end

@fs fs_aa
@include nanovg_fs.glsl
@end

@program nvg_aa vs_aa fs_aa