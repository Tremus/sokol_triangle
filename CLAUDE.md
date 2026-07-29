# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A personal graphics playground for prototyping shaders and rendering techniques. Each `src/program_*.c` file is a self-contained, runnable demo. Only one is compiled into the app at a time. There is no "main app" beyond the currently selected program — switching programs means editing `CMakeLists.txt`, not writing new host code.

## Switching / adding a program

`CMakeLists.txt` has a `### SELECT PROGRAM ###` block (~line 102) listing every `src/program_*.c` file, all commented out except one:

```cmake
### SELECT PROGRAM ###
# list(APPEND PLUGIN_SOURCES src/program_hello_triangle.c)
...
list(APPEND PLUGIN_SOURCES src/program_liquidglass.c)   # <- active program
```

To switch programs, comment out the active line and uncomment exactly one other. Re-run cmake configure/build afterward (source-list changes aren't picked up by an incremental build alone).

A couple of programs are known-broken and flagged inline: `program_msaa.c`, `program_pathkit_msaa.cpp`, `program_nanovg.c`.

When adding a new program:
1. Create `src/program_<name>.c` and (if it needs custom shaders) `src/program_<name>.glsl`.
2. Add its `list(APPEND PLUGIN_SOURCES ...)` line to the SELECT PROGRAM block.
3. Add a matching `sokol-shdc` line to `macos_shaders.sh` (and `windows_shaders.bat` if targeting Windows) so its `.glsl.h` gets generated — see Shader workflow below.

## Shader workflow

Each program's shaders live in `src/program_<name>.glsl` and are precompiled by `sokol-shdc` into `src/program_<name>.glsl.h` (gitignored, machine-generated, never hand-edit). The `.c` file includes `"common.h"` then its `.glsl.h`, and calls the generated `<program>_shader_desc(sg_query_backend())`.

After editing any `.glsl` file, regenerate its header:

```sh
# macOS
sokol-shdc --input src/program_<name>.glsl --output src/program_<name>.glsl.h --slang metal_macos
# or regenerate everything:
./macos_shaders.sh

# Windows
sokol-shdc.exe --input src\program_<name>.glsl --output src\program_<name>.glsl.h --slang hlsl5
```

(`macos_shaders.sh` / `windows_shaders.bat` batch-regenerate every program's header in one pass — new programs need a line added there too. Note `windows_shaders.bat` has a couple of pre-existing typos in filenames; don't copy those lines blindly.)

`.glsl` file conventions (sokol-shdc annotation syntax):
- `@vs <name> ... @end` / `@fs <name> ... @end` define shader stages.
- `@program <program_name> <vs_name> <fs_name>` binds a vertex+fragment stage pair; `<program_name>` becomes the generated `<program_name>_shader_desc()` function and `ATTR_<program_name>_<attr>` vertex-attribute macros. A single `.glsl` file can declare multiple `@vs`/`@fs`/`@program` blocks to produce several pipelines (see `program_liquidglass.glsl`).
- `layout(binding = N) uniform <BlockName> { ... };` generates a `<BlockName>_t` struct and a `UB_<BlockName>` bind-point macro, used as `sg_apply_uniforms(UB_<BlockName>, &SG_RANGE(uniforms))`.
- `layout(binding = N) uniform texture2D tex;` / `uniform sampler smp;` generate `VIEW_tex` / `SMP_smp` indices for `sg_bindings.views[...]` / `.samplers[...]`.
- The internal `@program` name does not have to match the `.c` filename — programs are sometimes adapted from another program's shader file and the names are left as-is (e.g. `program_liquidglass.c` uses shader programs named `texquad` and `quad_novbuf`). Check the `.glsl` file directly rather than assuming a name match.

## Architecture

**Windowing/host is CPLUG, not sokol_app.** This project repurposes [CPLUG](https://github.com/Tremus/CPLUG) (an audio plugin framework) plus its `cplug_extensions/window` add-on purely as a cross-platform window/event host — there is no audio here, and CPLUG's plugin-parameter API is stubbed out in `src/main.c` (`cplug_getNum*`/`cplug_getParameter*` return 0).

**`src/main.c`** is the fixed glue layer, always compiled regardless of which program is active:
- Implements CPLUG's required entry points (`cplug_libraryLoad/Unload`, `cplug_createPlugin/destroyPlugin`) and the CPLUG window glue (`pw_create_gui`, `pw_destroy_gui`, `pw_get_info`, `pw_event`, `pw_tick`).
- Owns sokol_gfx global setup/shutdown (`sg_setup`/`sg_shutdown`) and implements `get_swapchain()`, `get_blending()`, `read_file()`, `load_image_file()` (declared in `common.h`).
- Bridges the CPLUG/PW lifecycle to the active program's four entry points every `program_*.c` must implement: `program_setup()`, `program_shutdown()`, `program_tick()`, `program_event(const PWEvent*)`.

**`src/common.h`** (force-included via `/FI` on MSVC, `#include`d first everywhere else) declares that four-function contract, shared constants (`APP_WIDTH`/`APP_HEIGHT`), the `SOKOL_ASSERT` debug-break macro, and the `XFile`/`Image` helper structs.

**A typical `program_*.c`** holds one `static struct { sg_pipeline pip; sg_bindings bind; ... } state;` and implements:
- `program_setup()` — build buffers (`sg_make_buffer`), build shader from the generated `_shader_desc()`, build pipeline(s), load any textures via `load_image_file()`.
- `program_tick()` — `sg_begin_pass` (using `get_swapchain(...)`) → `sg_apply_pipeline` → `sg_apply_bindings` → optional `sg_apply_uniforms` → `sg_draw` → `sg_end_pass`.
- `program_event()` — handle `PWEvent` types (`PW_EVENT_RESIZE_UPDATE`, `PW_EVENT_MOUSE_*`) for interactive demos; a no-op for static ones.

## Conventions

- `#pragma once` everywhere; no `#ifndef` guards.
- Error handling in demo code is minimal — `xassert()` for hard invariants, no return-code checking on `sg_make_*` calls.
- `SRC_DIR` / `BIN_DIR` are injected as compile definitions for building absolute asset paths at runtime (e.g. `SRC_DIR "brain.jpg"`), rather than relying on a working directory.
