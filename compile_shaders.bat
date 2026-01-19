CALL sokol-shdc.exe --input src\program_hello_triangle.glsl         --output src\program_hello_triangle.glsl.h         --slang hlsl5
CALL sokol-shdc.exe --input src\program_hello_quad.glsl             --output src\program_hello_quad.glsl.h             --slang hlsl5
CALL sokol-shdc.exe --input src\program_minimal.glsl                --output src\program_minimal.glsl.h                --slang hlsl5
CALL sokol-shdc.exe --input src\program_texquad.glsl                --output src\program_texquad.glsl.h                --slang hlsl5
CALL sokol-shdc.exe --input src\program_offscreen_render.glsl       --output src\program_offscreen_render.glsl.h       --slang hlsl5
CALL sokol-shdc.exe --input src\program_fxaa.glsl                   --output src\program_fxaa.glsl.h                   --slang hlsl5
CALL sokol-shdc.exe --input src\program_msaa.glsl                   --output src\program_msaa.glsl.h                   --slang hlsl5
CALL sokol-shdc.exe --input src\program_animated_vertices.glsl      --output src\program_animated_vertices.glsl.h      --slang hlsl5
CALL sokol-shdc.exe --input src\program_draw_rect.glsl              --output src\program_draw_rect.glsl.h              --slang hlsl5
CALL sokol-shdc.exe --input src\program_line_stroke_texquad_1D.glsl --output src\program_line_stroke_texquad_1D.glsl.h --slang hlsl5
CALL sokol-shdc.exe --input src\program_gaussian_blur.glsl          --output src\program_gaussian_blur.glsl.h          --slang hlsl5
CALL sokol-shdc.exe --input src\program_sdf_shapes.glsl             --output src\program_sdf_shapes.glsl.h             --slang hlsl5
CALL sokol-shdc.exe --input src\program_flatten_quadbez.glsl        --output src\program_flatten_quadbez.glsl.h        --slang hlsl5
CALL sokol-shdc.exe --input src\program_pathkit.glsl                --output src\program_pathkit.glsl.h                --slang hlsl5
CALL sokol-shdc.exe --input src\program_pathkit_msaa.glsl           --output src\program_pathkit_msaa.glsl.h           --slang hlsl5
@REM CALL sokol-shdc.exe --input src\nanovg.glsl                         --output src\nanovg.glsl.glsl                      --slang hlsl5
CALL sokol-shdc.exe --input src\program_msdf.glsl                   --output src\program_msdf.glsl.glsl.h              --slang hlsl5
CALL sokol-shdc.exe --input src\program_dualfilter_blur.glsl        --output src\program_dualfilter_blur.glsl.h        --slang hlsl5
CALL sokol-shdc.exe --input src\program_blur_compute.glsl           --output src\program_blur_compute.glsl.h           --slang hlsl5
CALL sokol-shdc.exe --input src\program_vertex_pull.glsl            --output src\program_vertex_pull.glsl.h            --slang hlsl5
CALL sokol-shdc.exe --input src\program_vertex_pull_square.glsl     --output src\program_vertex_pull_square.glsl.h     --slang hlsl5
CALL sokol-shdc.exe --input src\program_vertex_pull_sdf.glsl        --output src\program_vertex_pull_sdf.glsl.h        --slang hlsl5
CALL sokol-shdc.exe --input src\program_square_no_vbuf.glsl         --output src\program_square_no_vbuf.glsl.h         --slang hlsl5
CALL sokol-shdc.exe --input src\program_sdf_line_segment.glsl       --output src\program_sdf_line_segment.glsl.h       --slang hlsl5
CALL sokol-shdc.exe --input src\program_line_stroke_sdf.glsl        --output src\program_line_stroke_sdf.glsl.h        --slang hlsl5
CALL sokol-shdc.exe --input src\program_sdf_compressed.glsl         --output src\program_sdf_compressed.glsl.h         --slang hlsl5
CALL sokol-shdc.exe --input src\program_sdf_line_tiled.glsl         --output src\program_sdf_line_tiled.glsl.h         --slang hlsl5

shader-hotreloader.exe -i src

cmd /k