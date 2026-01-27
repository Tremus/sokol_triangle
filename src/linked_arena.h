#pragma once
/*
MIT No Attribution

Copyright 2025 Tré Dudman

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stddef.h>
#include <stdint.h>
#include <xhl/debug.h>

// Very dumb linked list + arena/bump allocator structure

typedef struct LinkedArena
{
    size_t capacity;
    size_t size;

    size_t _padding;

    struct LinkedArena* next;
} LinkedArena;

LinkedArena* linked_arena_create(size_t min_cap);
LinkedArena* linked_arena_create_ex(void* hint, size_t cap);
void*        linked_arena_make_hint(LinkedArena* arena);
void         linked_arena_destroy(LinkedArena* arena);
void*        linked_arena_alloc_aligned(LinkedArena* arena, size_t size, size_t alignment);
static void* linked_arena_alloc(LinkedArena* arena, size_t size) { return linked_arena_alloc_aligned(arena, size, 32); }
void* linked_arena_alloc_clear(LinkedArena* arena, size_t size); // Additionally zeros returned memory for convenience
void  linked_arena_release(LinkedArena* arena, const void* const ptr);
// Releases all pointers allocated by arena, and arenas linked further down the chain
void linked_arena_clear(LinkedArena* arena);
// Destroy unused arenas. Won't destroy first item
void linked_arena_prune(LinkedArena* arena);
// Finds the top of the allocation stack
void* linked_arena_get_top(const LinkedArena* arena);

#ifdef NDEBUG
#define LINKED_ARENA_TAGGED_LEAK_DETECT_BEGIN(arena, tag)
#define LINKED_ARENA_TAGGED_LEAK_DETECT_END(arena, tag)
#else
#define LINKED_ARENA_TAGGED_LEAK_DETECT_BEGIN(arena, tag) size_t tag = arena->size;
#define LINKED_ARENA_TAGGED_LEAK_DETECT_END(arena, tag)   xassert(tag == arena->size);
#endif

#define LINKED_ARENA_LEAK_DETECT_BEGIN(arena) LINKED_ARENA_TAGGED_LEAK_DETECT_BEGIN(arena, _arena_size)
#define LINKED_ARENA_LEAK_DETECT_END(arena)   LINKED_ARENA_TAGGED_LEAK_DETECT_END(arena, _arena_size)