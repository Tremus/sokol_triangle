My graphics playground. Nothing to see here.

Requires the CLI tool **[sokol-shdc](https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md)** to compile the shaders

eg:

-   (Windows) `sokol-shdc.exe --input src\program_hello_triangle.glsl --output src\program_hello_triangle.glsl.h --slang hlsl5`
-   (macOS) `sokol-shdc --input src/program_hello_triangle.glsl --output src/program_hello_triangle.glsl.h --slang metal_macos`

Alternatively run: `macos_shaders.sh` (macOS) or `windows_shaders.bat` (Windows) to regenerate every program's shader header at once.

See `CLAUDE.md` for the full build/architecture rundown.