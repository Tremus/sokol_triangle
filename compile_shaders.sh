#!/bin/sh
sokol-shdc --input src/program_hello_triangle.glsl         --output src/program_hello_triangle.glsl.h         --slang metal_macos
sokol-shdc --input src/program_hello_quad.glsl             --output src/program_hello_quad.glsl.h             --slang metal_macos
sokol-shdc --input src/program_minimal.glsl                --output src/program_minimal.glsl.h                --slang metal_macos
sokol-shdc --input src/program_texquad.glsl                --output src/program_texquad.glsl.h                --slang metal_macos
sokol-shdc --input src/program_offscreen_render.glsl       --output src/program_offscreen_render.glsl.h       --slang metal_macos
sokol-shdc --input src/program_fxaa.glsl                   --output src/program_fxaa.glsl.h                   --slang metal_macos
sokol-shdc --input src/program_msaa.glsl                   --output src/program_msaa.glsl.h                   --slang metal_macos
sokol-shdc --input src/program_animated_vertices.glsl      --output src/program_animated_vertices.glsl.h      --slang metal_macos
sokol-shdc --input src/program_draw_rect.glsl              --output src/program_draw_rect.glsl.h              --slang metal_macos
sokol-shdc --input src/program_line_stroke_texquad_1D.glsl --output src/program_line_stroke_texquad_1D.glsl.h --slang metal_macos
sokol-shdc --input src/program_gaussian_blur.glsl          --output src/program_gaussian_blur.glsl.h          --slang metal_macos
sokol-shdc --input src/program_sdf_shapes.glsl             --output src/program_sdf_shapes.glsl.h             --slang metal_macos
sokol-shdc --input src/program_flatten_quadbez.glsl        --output src/program_flatten_quadbez.glsl.h        --slang metal_macos
sokol-shdc --input src/program_pathkit.glsl                --output src/program_pathkit.glsl.h                --slang metal_macos
sokol-shdc --input src/program_pathkit_msaa.glsl           --output src/program_pathkit_msaa.glsl.h           --slang metal_macos
# sokol-shdc --input src/nanovg.glsl                         --output src/nanovg.glsl.glsl.h                    --slang metal_macos
sokol-shdc --input src/program_msdf.glsl                   --output src/program_msdf.glsl.h                   --slang metal_macos
sokol-shdc --input src/program_dualfilter_blur.glsl        --output src/program_dualfilter_blur.glsl.h        --slang metal_macos
# sokol-shdc --input src/program_blur_compute.glsl           --output src/program_blur_compute.glsl.h           --slang metal_macos
sokol-shdc --input src/program_vertex_pull.glsl            --output src/program_vertex_pull.glsl.h            --slang metal_macos
sokol-shdc --input src/program_vertex_pull_square.glsl     --output src/program_vertex_pull_square.glsl.h     --slang metal_macos
sokol-shdc --input src/program_vertex_pull_sdf.glsl        --output src/program_vertex_pull_sdf.glsl.h        --slang metal_macos
sokol-shdc --input src/program_square_no_vbuf.glsl         --output src/program_square_no_vbuf.glsl.h         --slang metal_macos