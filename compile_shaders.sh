#!/bin/sh
sokol-shdc --input src/program_hello_triangle.glsl         --output src/program_hello_triangle.h         --slang metal_macos
sokol-shdc --input src/program_hello_quad.glsl             --output src/program_hello_quad.h             --slang metal_macos
sokol-shdc --input src/program_minimal.glsl                --output src/program_minimal.h                --slang metal_macos
sokol-shdc --input src/program_texquad.glsl                --output src/program_texquad.h                --slang metal_macos
sokol-shdc --input src/program_offscreen_render.glsl       --output src/program_offscreen_render.h       --slang metal_macos
sokol-shdc --input src/program_fxaa.glsl                   --output src/program_fxaa.h                   --slang metal_macos
sokol-shdc --input src/program_msaa.glsl                   --output src/program_msaa.h                   --slang metal_macos
sokol-shdc --input src/program_animated_vertices.glsl      --output src/program_animated_vertices.h      --slang metal_macos
sokol-shdc --input src/program_draw_rect.glsl              --output src/program_draw_rect.h              --slang metal_macos
sokol-shdc --input src/program_line_stroke_texquad_1D.glsl --output src/program_line_stroke_texquad_1D.h --slang metal_macos
sokol-shdc --input src/program_gaussian_blur.glsl          --output src/program_gaussian_blur.h          --slang metal_macos
sokol-shdc --input src/program_sdf_shapes.glsl             --output src/program_sdf_shapes.h             --slang metal_macos
sokol-shdc --input src/program_flatten_quadbez.glsl        --output src/program_flatten_quadbez.h        --slang metal_macos
sokol-shdc --input src/program_pathkit.glsl                --output src/program_pathkit.h                --slang metal_macos
sokol-shdc --input src/program_pathkit_msaa.glsl           --output src/program_pathkit_msaa.h           --slang metal_macos
sokol-shdc --input src/nanovg.glsl                         --output src/nanovg.glsl.h                    --slang metal_macos
sokol-shdc --input src/program_msdf.glsl                   --output src/program_msdf.glsl.h              --slang metal_macos
sokol-shdc --input src/program_dualfilter_blur.glsl        --output src/program_dualfilter_blur.h        --slang metal_macos
# sokol-shdc --input src/program_blur_compute.glsl           --output src/program_blur_compute.h           --slang metal_macos
sokol-shdc --input src/program_vertex_pull.glsl            --output src/program_vertex_pull.h            --slang metal_macos
sokol-shdc --input src/program_vertex_pull_square.glsl     --output src/program_vertex_pull_square.h     --slang metal_macos
sokol-shdc --input src/program_vertex_pull_sdf.glsl        --output src/program_vertex_pull_sdf.h        --slang metal_macos