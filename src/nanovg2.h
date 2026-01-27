#ifndef NANOVG_H
#define NANOVG_H

#include <sokol_gfx.h>

#include "linked_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NVG_PI 3.14159265358979323846264338327f

#ifndef NVG_MAX_STATES
#define NVG_MAX_STATES 32
#endif
// #define NVG_MAX_FONTIMAGES 4

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

#ifndef NDEBUG
#define NVG_LABEL(txt) txt
#else
#define NVG_LABEL(...) 0
#endif

#define NVG_ARRLEN(arr) (sizeof(arr) / sizeof(0 [arr]))

#if !defined(NVG_FONT_STB_TRUETYPE) && !defined(NVG_FONT_FREETYPE_SINGLECHANNEL) &&                                    \
    !defined(NVG_FONT_FREETYPE_MULTICHANNEL)
// #define NVG_FONT_STB_TRUETYPE
#ifdef __APPLE__
#define NVG_FONT_FREETYPE_SINGLECHANNEL
#else
#define NVG_FONT_FREETYPE_MULTICHANNEL
#endif

#endif
#if defined(NVG_FONT_FREETYPE_SINGLECHANNEL) || defined(NVG_FONT_FREETYPE_MULTICHANNEL)
#define NVG_FONT_FREETYPE
#endif

#if defined(NVG_FONT_STB_TRUETYPE)
#include <stb_truetype.h>
#endif

typedef struct NVGcolour
{
    union
    {
        float rgba[4];
        struct
        {
            float r, g, b, a;
        };
    };
} NVGcolour;

typedef struct NVGpaint
{
    float     xform[6];
    float     extent[2];
    float     radius;
    float     feather;
    NVGcolour innerColour;
    NVGcolour outerColour;
} NVGpaint;

enum NVGwinding
{
    NVG_CCW = 1, // Winding for solid shapes
    NVG_CW  = 2, // Winding for holes
};

enum NVGsolidity
{
    NVG_SOLID = 1, // CCW
    NVG_HOLE  = 2, // CW
};

enum NVGlineCap
{
    NVG_BUTT,
    NVG_ROUND,
    NVG_SQUARE,
    NVG_BEVEL,
    NVG_MITER,
};

enum NVGblendFactor
{
    NVG_ZERO                 = 1 << 0,
    NVG_ONE                  = 1 << 1,
    NVG_SRC_COLOUR           = 1 << 2,
    NVG_ONE_MINUS_SRC_COLOUR = 1 << 3,
    NVG_DST_COLOUR           = 1 << 4,
    NVG_ONE_MINUS_DST_COLOUR = 1 << 5,
    NVG_SRC_ALPHA            = 1 << 6,
    NVG_ONE_MINUS_SRC_ALPHA  = 1 << 7,
    NVG_DST_ALPHA            = 1 << 8,
    NVG_ONE_MINUS_DST_ALPHA  = 1 << 9,
    NVG_SRC_ALPHA_SATURATE   = 1 << 10,
};

enum NVGcompositeOperation
{
    NVG_SOURCE_OVER,
    NVG_SOURCE_IN,
    NVG_SOURCE_OUT,
    NVG_ATOP,
    NVG_DESTINATION_OVER,
    NVG_DESTINATION_IN,
    NVG_DESTINATION_OUT,
    NVG_DESTINATION_ATOP,
    NVG_LIGHTER,
    NVG_COPY,
    NVG_XOR,
};

typedef struct NVGcompositeOperationState
{
    int srcRGB;
    int dstRGB;
    int srcAlpha;
    int dstAlpha;
} NVGcompositeOperationState;

//
// Internal Render API
//

typedef struct NVGscissor
{
    float xform[6];
    float extent[2];
} NVGscissor;

typedef struct NVGvertex
{
    float x, y, u, v;
} NVGvertex;

typedef struct NVGpath
{
    int           first;
    int           count;
    unsigned char closed;
    int           nbevel;
    NVGvertex*    fill;
    int           nfill;
    NVGvertex*    stroke;
    int           nstroke;
    int           winding;
    int           convex;
} NVGpath;

enum NVGcommands
{
    NVG_MOVETO   = 0,
    NVG_LINETO   = 1,
    NVG_BEZIERTO = 2,
    NVG_CLOSE    = 3,
    NVG_WINDING  = 4,
};

enum NVGpointFlags
{
    NVG_PT_CORNER     = 0x01,
    NVG_PT_LEFT       = 0x02,
    NVG_PT_BEVEL      = 0x04,
    NVG_PR_INNERBEVEL = 0x08,
};

typedef struct NVGstate
{
    NVGcompositeOperationState compositeOperation;

    int        shapeAntiAlias;
    NVGpaint   paint;
    float      miterLimit;
    int        lineJoin;
    int        lineCap;
    float      xform[6];
    NVGscissor scissor;
    float      fontSize;
    // float                      letterSpacing;
    float lineHeight;
    // float                      fontBlur;
    int textAlign;
    int fontId;
} NVGstate;

typedef struct NVGpoint
{
    float         x, y;
    float         dx, dy;
    float         len;
    float         dmx, dmy;
    unsigned char flags;
} NVGpoint;

typedef struct NVGpathCache
{
    NVGpoint*  points;
    int        npoints;
    int        cpoints;
    NVGpath*   paths;
    int        npaths;
    int        cpaths;
    NVGvertex* verts;
    int        nverts;
    int        cverts;
    float      bounds[4];
} NVGpathCache;

// Create flags

enum NVGcreateFlags
{
    // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
    NVG_ANTIALIAS = 1 << 0,
    // Flag indicating if strokes should be drawn using stencil buffer. The rendering will be a little
    // slower, but path overlaps (i.e. self-intersecting or sharp turns) will be drawn just once.
    NVG_STENCIL_STROKES = 1 << 1,
    // Flag indicating that additional debug checks are done.
    NVG_DEBUG = 1 << 2,
};

enum SGNVGshaderType
{
    NSVG_SHADER_FILLGRAD,
    NSVG_SHADER_FILLIMG,
    NSVG_SHADER_SIMPLE,
    NSVG_SHADER_IMG
};

typedef struct SGNVGtexture
{
    sg_image img;
    sg_view  texview;
    int      type;
    int      width, height;
    int      flags;
    uint8_t* imgData;
} SGNVGtexture;

typedef struct SGNVGframebuffer
{
    sg_image img;
    sg_view  img_colview; // View for writing to
    sg_view  img_texview; // View for reading from
    sg_image depth;
    sg_view  depth_view;

    int width;
    int height;
    // In theory this is a double value found on NSWindow and NSScreen, but in practice this is strictly 1, 2, or 3
    int backingScaleFactor;
} SGNVGframebuffer;

typedef struct SGNVGimageFX
{
    SGNVGframebuffer  resolve;
    float             max_radius_px;
    int               max_mip_levels;
    SGNVGframebuffer* mip_levels;
    SGNVGframebuffer* interp_levels;
} SGNVGimageFX;

typedef struct SGNVGblend
{
    sg_blend_factor srcRGB;
    sg_blend_factor dstRGB;
    sg_blend_factor srcAlpha;
    sg_blend_factor dstAlpha;
} SGNVGblend;

enum SGNVGcallType
{
    SGNVG_NONE = 0,
    SGNVG_FILL,
    SGNVG_CONVEXFILL,
    SGNVG_STROKE,
    SGNVG_TRIANGLES,
};

typedef struct SGNVGpath
{
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
} SGNVGpath;

typedef struct SGNVGattribute
{
    float vertex[2];
    float tcoord[2];
} SGNVGattribute;

typedef struct SGNVGvertUniforms
{
    float viewSize[4];
} SGNVGvertUniforms;

typedef struct SGNVGfragUniforms
{
#define NANOVG_SG_UNIFORMARRAY_SIZE 11
    union
    {
        struct
        {
            float            scissorMat[12]; // matrices are actually 3 vec4s
            float            paintMat[12];
            struct NVGcolour innerCol;
            struct NVGcolour outerCol;
            float            scissorExt[2];
            float            scissorScale[2];
            float            extent[2];
            float            radius;
            float            feather;
            float            strokeMult;
            float            strokeThr;
            float            texType;
            float            type;
        };
        float uniformArray[NANOVG_SG_UNIFORMARRAY_SIZE][4];
    };
} SGNVGfragUniforms;

// LRU cache; keep its size relatively small, as items are accessed via a linear search
#define NANOVG_SG_PIPELINE_CACHE_SIZE 32

typedef struct SGNVGpipelineCacheKey
{
    uint32_t blend; // cached as `src_factor_rgb | (dst_factor_rgb << 8) | (src_factor_alpha << 16) | (dst_factor_alpha
                    // << 24)`
    uint32_t lastUse; // updated on each read
} SGNVGpipelineCacheKey;

enum SGNVGpipelineType
{
    // used by sgnvg__convexFill, sgnvg__stroke, sgnvg__triangles
    SGNVG_PIP_BASE = 0,

    // used by sgnvg__fill
    SGNVG_PIP_FILL_STENCIL,
    SGNVG_PIP_FILL_ANTIALIAS, // only used if sg->flags & NVG_ANTIALIAS
    SGNVG_PIP_FILL_DRAW,

    // used by sgnvg__stroke
    SGNVG_PIP_STROKE_STENCIL_DRAW,      // only used if sg->flags & NVG_STENCIL_STROKES
    SGNVG_PIP_STROKE_STENCIL_ANTIALIAS, // only used if sg->flags & NVG_STENCIL_STROKES
    SGNVG_PIP_STROKE_STENCIL_CLEAR,     // only used if sg->flags & NVG_STENCIL_STROKES

    SGNVG_PIP_NUM_
};

typedef struct SGNVGpipelineCache
{
    // keys are stored as a separate array for search performance
    SGNVGpipelineCacheKey keys[NANOVG_SG_PIPELINE_CACHE_SIZE];
    sg_pipeline           pipelines[NANOVG_SG_PIPELINE_CACHE_SIZE][SGNVG_PIP_NUM_];
    uint8_t               pipelinesActive[NANOVG_SG_PIPELINE_CACHE_SIZE];
    uint32_t              currentUse; // incremented on each overwrite
} SGNVGpipelineCache;

typedef struct SGNVGcall
{
    enum SGNVGcallType type;
    int                triangleOffset;
    int                triangleCount;

    SGNVGblend blendFunc;

    int        num_paths;
    SGNVGpath* paths;

    // depending on SGNVGcall.type and NVG_STENCIL_STROKES, this may be 2 consecutive uniforms
    SGNVGfragUniforms* uniforms;

    struct SGNVGcall* next;
} SGNVGcall;

typedef struct SGNVGcommandBeginPass
{
    sg_pass  pass;
    unsigned x, y;
    unsigned width, height;
} SGNVGcommandBeginPass;

typedef struct SGNVGcommandNVG
{
    int               num_calls;
    struct SGNVGcall* calls;
} SGNVGcommandNVG;

typedef struct SGNVGcommandText
{
    int text_buffer_start;
    int text_buffer_end;
    // TODO: support more colours
    NVGcolour colour_fill;
    sg_view   atlas_view;
} SGNVGcommandText;

typedef struct SGNVGcommandImageFX
{
    // These are probably redundant and could be implied by the above params being > 0
    bool apply_lightness_filter;
    bool apply_bloom;

    float lightness_threshold;
    float radius_px;
    float bloom_amount;

    SGNVGframebuffer* src;
    SGNVGimageFX*     fx;
} SGNVGcommandImageFX;

typedef void (*SGNVGcustomFunc)(void* uptr);

typedef struct SGNVGcommandCustom
{
    void*           uptr;
    SGNVGcustomFunc func;
} SGNVGcommandCustom;

enum SGNVGcommandType
{
    SGNVG_CMD_BEGIN_PASS,
    SGNVG_CMD_END_PASS,
    SGNVG_CMD_DRAW_NVG,
    SGNVG_CMD_CUSTOM,
};

typedef struct SGNVGcommand
{
    enum SGNVGcommandType type;
    const char*           label;
    union
    {
        void* data;

        SGNVGcommandBeginPass* beginPass;
        SGNVGcommandNVG*       drawNVG;
        SGNVGcommandText*      text;
        SGNVGcommandImageFX*   fx;
        SGNVGcommandCustom*    custom;
    } payload;

    struct SGNVGcommand* next;
} SGNVGcommand;

typedef struct NVGfontSlot
{
    void*  kbtr_font_ptr;
    void*  data;
    size_t data_size;
    int    owned;
} NVGfontSlot;

// Used to identify a unique glyph.
// TODO: support multiple fonts
typedef union NVGatlasRectHeader
{
    struct
    {
        uint32_t glyph_index;
        // TODO: this could probably be packed into an integer. To support sizes like 12.25, multiply & divide by 4
        // This could make room for font ids in the header
        float font_size;
    };
    uint64_t data;
} NVGatlasRectHeader;

typedef struct NVGatlasRect
{
    union NVGatlasRectHeader header;

    uint8_t x, y, w, h;

    int16_t advance_x;
    int16_t advance_y;

    int8_t bearing_x;
    int8_t bearing_y;

    sg_view img_view;
} NVGatlasRect;

typedef struct NVGatlas
{
    sg_view img_view;
    bool    dirty;
    bool    full;
} NVGatlas;

typedef struct NVGcontext
{
    LinkedArena* arena;
    void*        arena_top;

    float*       commands;
    int          ccommands;
    int          ncommands;
    float        commandx, commandy;
    NVGstate     state;
    NVGpathCache cache;
    float        tessTol;
    float        distTol;
    float        fringeWidth;
    int          backingScaleFactor;

    struct
    {
        int drawCallCount;
        int fillTriCount;
        int strokeTriCount;
        int textTriCount;
        // Track how much data is uploaded to GPU
        size_t uploaded_bytes;
    } frame_stats;

    // SGNVGcontext....

    sg_shader          shader;
    SGNVGvertUniforms  view;
    int                flags;
    sg_buffer          vertBuf;
    sg_buffer          indexBuf;
    SGNVGpipelineCache pipelineCache;

    // Per frame buffers
    SGNVGattribute* verts;
    int             cverts;
    int             nverts;
    int             cverts_gpu;
    uint32_t*       indexes;
    int             cindexes;
    int             nindexes;
    int             cindexes_gpu;

    sg_sampler sampler_linear;
    sg_sampler sampler_nearest;

    // Feel free to allocate anything you want with this at any time in a frame after nvgBeginFrame() is called
    // Note all allocations are dropped when nvgBeginFrame() is called
    // It is also unadvised to release anything you allocate with this.
    // If these rules/guidelines are okay with you, go ahead
    LinkedArena* frame_arena;

    SGNVGcall*       current_call;     // linked list current position
    SGNVGcommandNVG* current_nvg_draw; // linked list current position
    SGNVGcommand*    current_command;  // linked list current position
    SGNVGcommand*    first_command;    // linked list start

    // state
    int            pipelineCacheIndex;
    sg_blend_state blend;
} NVGcontext;

typedef struct NVGglyphPosition2
{
    NVGatlasRect rect;
    int          x, y;
} NVGglyphPosition2;

typedef struct NVGtextLayoutRow
{
    // Indexes into glyphs array in struct NVGtextLayout below
    short begin_idx, end_idx;
    short ymin, ymax;
    short xmin, xmax;
    int   cursor_y_px;
} NVGtextLayoutRow;

// Glyphs are shaped and aligned from left > right along the baseline of row one
// Alignment and translation on a screen should be applied at draw time
// This design is to help reduce the amount of work kbts has do to, and avoid doing multiple runs across the text
// Hopefully there is enough data here to make this possible.
// Handling multiple languages is an aftertought here and this design may prove to be bad.
typedef struct NVGtextLayout
{
    // WARNING: this values are scaled accorting to ctx->backingScaleFactor
    // You are free to use them, however you may need to remember to divide by backingScaleFactor to your work in your
    // own pixel space
    short ascender, descender;
    short line_height;
    short xmax; // The right edge of the longest (in pixels) row

    int total_height;
    int total_height_tight;

    int num_rows, cap_rows;
    int num_glyphs, cap_glyphs;
    int offset_rows;
    int offset_glyphs;
} NVGtextLayout;

static NVGtextLayoutRow* nvgLayoutGetRows(const NVGtextLayout* l)
{
    return (NVGtextLayoutRow*)((char*)l + l->offset_rows);
}
static void nvgLayoutSetRows(NVGtextLayout* l, NVGtextLayoutRow* r) { l->offset_rows = ((char*)r - (char*)l); }
static NVGglyphPosition2* nvgLayoutGetGlyphs(const NVGtextLayout* l)
{
    return (NVGglyphPosition2*)((char*)l + l->offset_glyphs);
}
static void nvgLayoutSetGlyphs(NVGtextLayout* l, NVGglyphPosition2* g) { l->offset_glyphs = ((char*)g - (char*)l); }

NVGcontext* nvgCreateContext(int flags);
void        nvgDestroyContext(NVGcontext* ctx);

// Debug function to dump cached path data.
void nvgDebugDumpPathCache(NVGcontext* ctx);

// Begin drawing a new frame
// Calls to nanovg drawing API should be wrapped in nvgBeginFrame() & nvgEndFrame()
// nvgBeginFrame() defines the size of the window to render to in relation currently
// set viewport (i.e. glViewport on GL backends). Device pixel ration allows to
// control the rendering on Hi-DPI devices.
// For example, GLFW returns two dimension for an opened window: window size and
// frame buffer size. In that case you would set windowWidth/Height to the window size
// backingScaleFactor to: frameBufferWidth / windowWidth.
void nvgBeginFrame(NVGcontext* ctx, int backingScaleFactor);

// Ends drawing flushing remaining render state.
void nvgEndFrame(NVGcontext* ctx);

//
// Composite operation
//
// The composite operations in NanoVG are modeled after HTML Canvas API, and
// the blend func is based on OpenGL (see corresponding manuals for more info).
// The colours in the blending state have premultiplied alpha.

// Sets the composite operation. The op parameter should be one of NVGcompositeOperation.
void nvgSetGlobalCompositeOperation(NVGcontext* ctx, int op);

// Sets the composite operation with custom pixel arithmetic. The parameters should be one of NVGblendFactor.
void nvgSetGlobalCompositeBlendFunc(NVGcontext* ctx, int sfactor, int dfactor);

// Sets the composite operation with custom pixel arithmetic for RGB and alpha components separately. The parameters
// should be one of NVGblendFactor.
void nvgSetGlobalCompositeBlendFuncSeparate(NVGcontext* ctx, int srcRGB, int dstRGB, int srcAlpha, int dstAlpha);

//
// Colour utils
//
// Colours in NanoVG are stored as unsigned ints in ABGR format.

// Returns a colour value from red, green, blue and alpha values.
NVGcolour nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

// Returns a colour value from red, green, blue and alpha values.
NVGcolour nvgRGBAf(float r, float g, float b, float a);

// Linearly interpolates from colour c0 to c1, and returns resulting colour value.
NVGcolour nvgLerpRGBA(NVGcolour c0, NVGcolour c1, float u);

// Returns colour value specified by hue, saturation and lightness.
// HSL values are all in range [0..1], alpha will be set to 255.
NVGcolour nvgHSL(float h, float s, float l);

// Returns colour value specified by hue, saturation and lightness and alpha.
// HSL values are all in range [0..1], alpha in range [0..255]
NVGcolour nvgHSLA(float h, float s, float l, unsigned char a);

// clang-format off
#define nvgHexColour(hex) (NVGcolour){( hex >> 24)         / 255.0f,\
                                      ((hex >> 16) & 0xff) / 255.0f,\
                                      ((hex >>  8) & 0xff) / 255.0f,\
                                      ( hex        & 0xff) / 255.0f}
// clang-format on

static uint32_t nvgCompressColour(NVGcolour col)
{
    uint32_t r, g, b, a;
    r  = col.r * 255;
    g  = col.g * 255;
    b  = col.b * 255;
    a  = col.a * 255;
    r &= 255;
    g &= 255;
    b &= 255;
    a &= 255;
    return (r << 24) | (g << 16) | (b << 8) | (a << 0);
}

//
// Transforms
//
// The paths, gradients, patterns and scissor region are transformed by an transformation
// matrix at the time when they are passed to the API.
// The current transformation matrix is a affine matrix:
//   [sx kx tx]
//   [ky sy ty]
//   [ 0  0  1]
// Where: sx,sy define scaling, kx,ky skewing, and tx,ty translation.
// The last row is assumed to be 0,0,1 and is not stored.
//
// Apart from nvgResetTransform(), each transformation function first creates
// specific transformation matrix and pre-multiplies the current transformation by it.
//
// Current coordinate system (transformation) can be saved and restored using nvgSave() and nvgRestore().

// Resets current transform to a identity matrix.
void nvgResetTransform(NVGcontext* ctx);

// Premultiplies current coordinate system by specified matrix.
// The parameters are interpreted as matrix as follows:
//   [a c e]
//   [b d f]
//   [0 0 1]
void nvgTransform(NVGcontext* ctx, float a, float b, float c, float d, float e, float f);

// Translates current coordinate system.
void nvgTranslate(NVGcontext* ctx, float x, float y);

// Rotates current coordinate system. Angle is specified in radians.
void nvgRotate(NVGcontext* ctx, float angle);

// Skews the current coordinate system along X axis. Angle is specified in radians.
void nvgSkewX(NVGcontext* ctx, float angle);

// Skews the current coordinate system along Y axis. Angle is specified in radians.
void nvgSkewY(NVGcontext* ctx, float angle);

// Scales the current coordinate system.
void nvgScale(NVGcontext* ctx, float x, float y);

// Stores the top part (a-f) of the current transformation matrix in to the specified buffer.
//   [a c e]
//   [b d f]
//   [0 0 1]
// There should be space for 6 floats in the return buffer for the values a-f.
void nvgCurrentTransform(NVGcontext* ctx, float* xform);

// The following functions can be used to make calculations on 2x3 transformation matrices.
// A 2x3 matrix is represented as float[6].

// Sets the transform to identity matrix.
void nvgTransformIdentity(float* dst);

// Sets the transform to translation matrix matrix.
void nvgTransformTranslate(float* dst, float tx, float ty);

// Sets the transform to scale matrix.
void nvgTransformScale(float* dst, float sx, float sy);

// Sets the transform to rotate matrix. Angle is specified in radians.
void nvgTransformRotate(float* dst, float a);

// Sets the transform to skew-x matrix. Angle is specified in radians.
void nvgTransformSkewX(float* dst, float a);

// Sets the transform to skew-y matrix. Angle is specified in radians.
void nvgTransformSkewY(float* dst, float a);

// Sets the transform to the result of multiplication of two transforms, of A = A*B.
void nvgTransformMultiply(float* dst, const float* src);

// Sets the transform to the result of multiplication of two transforms, of A = B*A.
void nvgTransformPremultiply(float* dst, const float* src);

// Sets the destination to inverse of specified transform.
// Returns 1 if the inverse could be calculated, else 0.
int nvgTransformInverse(float* dst, const float* src);

// Transform a point by given transform.
void nvgTransformPoint(float* dstx, float* dsty, const float* xform, float srcx, float srcy);

// Converts degrees to radians and vice versa.
float nvgDegToRad(float deg);
float nvgRadToDeg(float rad);

//
// Render styles
//
// Fill and stroke render style can be either a solid colour or a paint which is a gradient or a pattern.
// Solid colour is simply defined as a colour value, different kinds of paints can be created
// using nvgLinearGradient(), nvgBoxGradient(), nvgRadialGradient() and nvgImagePattern().
//

// Sets current paint style to a solid colour.
void nvgSetColour(NVGcontext* ctx, NVGcolour colour);

// Sets current paint style to a gradient or pattern.
static inline void nvgSetPaint(NVGcontext* ctx, NVGpaint paint)
{
    ctx->state.paint = paint;
    nvgTransformMultiply(ctx->state.paint.xform, ctx->state.xform);
}

// Sets the miter limit of the stroke style.
// Miter limit controls when a sharp corner is beveled.
static inline void nvgSetMiterLimit(NVGcontext* ctx, float limit) { ctx->state.miterLimit = limit; }

// Sets how the end of the line (cap) is drawn,
// Can be one of: NVG_BUTT (default), NVG_ROUND, NVG_SQUARE.
static inline void nvgSetLineCap(NVGcontext* ctx, int cap) { ctx->state.lineCap = cap; }

// Sets how sharp path corners are drawn.
// Can be one of NVG_MITER (default), NVG_ROUND, NVG_BEVEL.
static inline void nvgSetLineJoin(NVGcontext* ctx, int join) { ctx->state.lineJoin = join; }

//
// Paths
//
// Drawing a new shape starts with nvgBeginPath(), it clears all the currently defined paths.
// Then you define one or more paths and sub-paths which describe the shape. The are functions
// to draw common shapes like rectangles and circles, and lower level step-by-step functions,
// which allow to define a path curve by curve.
//
// NanoVG uses even-odd fill rule to draw the shapes. Solid shapes should have counter clockwise
// winding and holes should have counter clockwise order. To specify winding of a path you can
// call nvgSetPathWinding(). This is useful especially for the common shapes, which are drawn CCW.
//
// Finally you can fill the path using current fill style by calling nvgFill(), and stroke it
// with current stroke style by calling nvgStroke().
//
// The curve segments and sub-paths are transformed by the current transform.

// Clears the current path and sub-paths.
void nvgBeginPath(NVGcontext* ctx);

void nvg__appendCommands(NVGcontext* ctx, float* vals, int nvals);

// Starts new sub-path with specified point as first point.
static void nvgMoveTo(NVGcontext* ctx, float x, float y)
{
    float vals[] = {NVG_MOVETO, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds line segment from the last point in the path to the specified point.
static void nvgLineTo(NVGcontext* ctx, float x, float y)
{
    float vals[] = {NVG_LINETO, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds cubic bezier segment from last point in the path via two control points to the specified point.
static void nvgBezierTo(NVGcontext* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    float vals[] = {NVG_BEZIERTO, c1x, c1y, c2x, c2y, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds quadratic bezier segment from last point in the path via a control point to the specified point.
void nvgQuadTo(NVGcontext* ctx, float cx, float cy, float x, float y);

// Adds an arc segment at the corner defined by the last path point, and two specified points.
void nvgArcTo(NVGcontext* ctx, float x1, float y1, float x2, float y2, float radius);

// Closes current sub-path with a line segment.
void nvgClosePath(NVGcontext* ctx);

// Sets the current sub-path winding, see NVGwinding and NVGsolidity.
void nvgSetPathWinding(NVGcontext* ctx, int dir);

// Creates new circle arc shaped sub-path. The arc centre is at cx,cy, the arc radius is r,
// and the arc is drawn from angle a0 to a1, and swept in direction dir (NVG_CCW, or NVG_CW).
// Angles are specified in radians.
void nvgArc(NVGcontext* ctx, float cx, float cy, float r, float a0, float a1, int dir);

// Creates new rectangle shaped sub-path.
void nvgRect(NVGcontext* ctx, float x, float y, float w, float h);
void nvgRect2(NVGcontext* ctx, float left, float top, float right, float bottom);

// Creates new rounded rectangle shaped sub-path.
void nvgRoundedRect(NVGcontext* ctx, float x, float y, float w, float h, float r);
void nvgRoundedRect2(NVGcontext* ctx, float x, float y, float r, float b, float rad);

// Creates new rounded rectangle shaped sub-path with varying radii for each corner.
void nvgRoundedRectVarying(
    NVGcontext* ctx,
    float       l,
    float       t,
    float       w,
    float       h,
    float       radTopLeft,
    float       radTopRight,
    float       radBottomRight,
    float       radBottomLeft);
void nvgRoundedRectVarying2(
    NVGcontext* ctx,
    float       l,
    float       t,
    float       r,
    float       b,
    float       radTopLeft,
    float       radTopRight,
    float       radBottomRight,
    float       radBottomLeft);

// Creates new ellipse shaped sub-path.
void nvgEllipse(NVGcontext* ctx, float cx, float cy, float rx, float ry);

// Creates new circle shaped sub-path.
void nvgCircle(NVGcontext* ctx, float cx, float cy, float r);

// Fills the current path with current fill style.
void nvgFill(NVGcontext* ctx);

// Fills the current path with current stroke style.
void nvgStroke(NVGcontext* ctx, float stroke_width);

//
// Text
//
// NanoVG allows you to load .ttf files and use the font to render text.
//
// The appearance of the text can be defined by setting the current text style
// and by specifying the fill colour. Common text and font settings such as
// font size, letter spacing and text align are supported. Font blur allows you
// to create simple text effects such as drop shadows.
//
// At render time the font face can be set based on the font handles or name.
//
// Font measure functions return values in local space, the calculations are
// carried in the same resolution as the final rendering. This is done because
// the text glyph positions are snapped to the nearest pixels sharp rendering.
//
// The local space means that values are not rotated or scale as per the current
// transformation. For example if you set font size to 12, which would mean that
// line height is 16, then regardless of the current scaling and rotation, the
// returned line height is always 16. Some measures may vary because of the scaling
// since aforementioned pixel snapping.
//
// While this may sound a little odd, the setup allows you to always render the
// same way regardless of scaling. I.e. following works regardless of scaling:
//
//		const char* txt = "Text me up.";
//		nvgTextBounds(vg, x,y, txt, NULL, bounds);
//		nvgBeginPath(vg);
//		nvgRect(vg, bounds[0],bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]);
//		nvgFill(vg);
//
// Note: currently only solid colour fill is supported for text.

void snvg_command_begin_pass(
    NVGcontext* ctx,
    const sg_pass*,
    unsigned    x,
    unsigned    y,
    unsigned    width,
    unsigned    height,
    const char* label);
void snvg_command_end_pass(NVGcontext* ctx, const char* label);
void snvg_command_draw_nvg(NVGcontext* ctx, const char* label);
// 'radius_px' can be animated each frame. For best performance, finish your animations with radius at a power of 2,
// and a minimum of 8px
void snvg_command_fx(
    NVGcontext*       ctx,
    bool              apply_lightness_filter,
    bool              apply_bloom,
    float             lightness_threshold,
    float             radius_px,
    float             bloom_amount,
    SGNVGframebuffer* src,
    SGNVGimageFX*     fx,
    const char*       label);
void snvg_command_custom(NVGcontext* ctx, void* uptr, SGNVGcustomFunc func, const char* label);

typedef struct SNVGcallState
{
    SGNVGcall* start;
    SGNVGcall* end;
    int        count;
} SNVGcallState;

static SNVGcallState snvg_calls_pop(NVGcontext* ctx)
{
    SNVGcallState calls;
    calls.start                      = ctx->current_nvg_draw->calls;
    calls.end                        = ctx->current_call;
    calls.count                      = ctx->current_nvg_draw->num_calls;
    ctx->current_nvg_draw->calls     = NULL;
    ctx->current_call                = NULL;
    ctx->current_nvg_draw->num_calls = 0;
    return calls;
}
static void snvg_calls_set(NVGcontext* ctx, const SNVGcallState* calls)
{
    ctx->current_nvg_draw->calls     = calls->start;
    ctx->current_nvg_draw->num_calls = calls->count;
    ctx->current_call                = calls->end;
}
static void snvg_calls_join(NVGcontext* ctx, const SNVGcallState* calls)
{
    if (ctx->current_call)
        ctx->current_call->next = calls->start;
    ctx->current_call                 = calls->end;
    ctx->current_nvg_draw->num_calls += calls->count;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define NVG_NOTUSED(v)                                                                                                 \
    for (;;)                                                                                                           \
    {                                                                                                                  \
        (void)(1 ? (void)0 : ((void)(v)));                                                                             \
        break;                                                                                                         \
    }

#ifdef __cplusplus
}
#endif

#endif // NANOVG_H