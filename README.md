My graphics playground. Nothing to see here.

Requires the CLI tool **[sokol-shdc](https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md)** to compile the shaders

eg:
- (Windows) `sokol-shdc.exe --input shaders\triangle.glsl --output shaders\triangle.h --slang glsl430:hlsl5:metal_macos`
- (macOS) `sokol-shdc --input shaders/triangle.glsl --output shaders/triangle.h --slang glsl430:hlsl5:metal_macos`