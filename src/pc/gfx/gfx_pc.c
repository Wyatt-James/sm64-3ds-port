#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <arm_acle.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "gfx_pc.h"

#include "src/pc/gfx/gfx_rendering_api.h"
#include "gfx_window_manager_api.h"

#include "gfx_cc.h"
#include "gfx_screen_config.h"
#include "color_formats.h"
#include "texture_conversion.h"
#include "src/pc/pc_metrics.h"

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/profiler_3ds.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/gfx/gfx_3ds_constants.h"

#define GRANULAR_PROFILING 0 // Enables fine-grained performance profiling of various things.
#define GRANULAR_FLUSHES   0 // Enables fine-grained profiling of flush hit/avoid rate.
#define FLUSH_COUNTERS    25 // Number of gfx_flush calls to log.

#if GRANULAR_PROFILING == 1
#define granular_log_time(v_)     do { profiler_3ds_log_time(v_); } while (0)
#define non_granular_log_time(v_) do { } while (0)
#else
#define granular_log_time(v_)     do { } while (0)
#define non_granular_log_time(v_) do { profiler_3ds_log_time(v_); } while (0)
#endif

#if GRANULAR_FLUSHES == 1
#define GRANULAR_FLUSH(v_) v_
#define GRANULAR_FLUSH_DO(v_) do { v_; } while (0)
#define gfx_flush(id_) gfx_flush_impl(id_)
static int flushes[FLUSH_COUNTERS], flush_avoids[FLUSH_COUNTERS]; // Read with debugger
static int tris_per_flush[FLUSH_COUNTERS][255];
#else
#define GRANULAR_FLUSH(v_)
#define GRANULAR_FLUSH_DO(v_) do {} while (0)
#define gfx_flush(id_) gfx_flush_impl()
#endif

/*
 *   Flush Totals, updated for commmit 634be2132a77c3c3fc04f66b87a2badd4d41e2ca
 *   Measured 1 frame after BoB benchmark, goomba was visible
 *   
 *   Total Flushes: 182
 *   Total Avoids:  195
 *   Grand Total:   377
 * 
 *   id:          0  1   2    3  4   5  6  7  8   9   10 11 12 13 14 15 16 17 18  19  20 21 22  23 24
 *   flushes:   { 1, 0,  1,  96, 0,  7, 0, 0, 0,  4,  54, 0, 0, 0, 0, 0, 0, 0, 0,  0, 12, 1, 1,  5, 0 }
 *   avoids:    { 3, 0,  9,  11, 0, 22, 0, 3, 2, 14,  88, 0, 8, 2, 0, 0, 0, 0, 5, 12,  0, 5, 1,  9, 1 }
 *   subtotals: { 4, 0, 10, 107, 0, 29, 0, 3, 2, 18, 142, 0, 8, 2, 0, 0, 0, 0, 5, 12, 12, 6, 2, 14, 1 }
 */

// If enabled, shader swaps will be counter and printed each frame.
#define ENABLE_SHADER_SWAP_COUNTER 0
#define ENABLE_OTHER_MODE_SWAP_COUNTER 0
#define ENABLE_ASSERTIONS 0

#if ENABLE_ASSERTIONS == 1
#define SUPPORT_CHECK(x) assert(x)
#else
#define SUPPORT_CHECK(x) do {} while (0)
#endif

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH / 2)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT / 2)

#define RATIO_X (gfx_current_dimensions.width / (2.0f * HALF_SCREEN_WIDTH))
#define RATIO_Y (gfx_current_dimensions.height / (2.0f * HALF_SCREEN_HEIGHT))
#define ARR_INDEX_2D(x_, y_, w_) (x_ + (y_ * w_))

#define MAX_BUFFERED_TRIS 256
#define MAX_BUFFERED_VERTS (MAX_BUFFERED_TRIS * 3)
#define MAX_LIGHTS 2
#define MAX_VERTICES 64
#define MAT_STACK_SIZE 11

#define MATRIX_SET_NORMAL 0
#define MATRIX_SET_IDENTITY 1
#define MATRIX_SET_SCALED_NDC 2
#define MATRIX_SET_INVALID (~MATRIX_SET_NORMAL)
#define NDC_SCALE (INT16_MAX / 4) // Scaling factor for s16 NDC coordinates. See gfx_dimensions.h for more info.

#define DELIBERATELY_INVALID_CC_ID ~0

#define ASSUME(cond) if (!(cond)) __builtin_unreachable()
#define LIKELY(cond)              __builtin_expect(!!(cond), 1)
#define UNLIKELY(cond)            __builtin_expect(!!(cond), 0)
#define EXPECT(val, expected)     __builtin_expect(val, expected)

#define XYWH_EQUAL(vp1_, vp2_) (vp1_.x == vp2_.x && vp1_.y == vp2_.y && vp1_.width == vp2_.width && vp1_.height == vp2_.height) // Compares two XYWidthHeight structs

// Supported Texture formats (see import_texture)
// Max val for format_: 0b100 (range [0-4])
// Max val for size_:   0b101 (range [0-5])
#define TEX_FORMAT(format_, size_) ((((uint32_t) format_) << 3 ) | (uint32_t) size_)

#define TEXFMT_RGBA32 TEX_FORMAT(G_IM_FMT_RGBA, G_IM_SIZ_32b) // Unused by SM64
#define TEXFMT_RGBA16 TEX_FORMAT(G_IM_FMT_RGBA, G_IM_SIZ_16b)
#define TEXFMT_IA4    TEX_FORMAT(G_IM_FMT_IA,   G_IM_SIZ_4b)  // Used by text only
#define TEXFMT_IA8    TEX_FORMAT(G_IM_FMT_IA,   G_IM_SIZ_8b)
#define TEXFMT_IA16   TEX_FORMAT(G_IM_FMT_IA,   G_IM_SIZ_16b)
#define TEXFMT_I4     TEX_FORMAT(G_IM_FMT_I,    G_IM_SIZ_4b)  // Unused by SM64
#define TEXFMT_I8     TEX_FORMAT(G_IM_FMT_I,    G_IM_SIZ_8b)  // Unused by SM64
#define TEXFMT_CI4    TEX_FORMAT(G_IM_FMT_CI,   G_IM_SIZ_4b)  // Unused by SM64
#define TEXFMT_CI8    TEX_FORMAT(G_IM_FMT_CI,   G_IM_SIZ_8b)  // Unused by SM64

#define UCLAMP8(v) ((uint8_t) __usat(v, 8))
#define U32_AS_FLOAT(v) (*(float*) &v)
#define COMBINE_MODE(rgb, alpha) (((uint32_t) rgb) | (((uint32_t) alpha) << 12))

#if ENABLE_SHADER_SWAP_COUNTER == 1
#define SHADER_COUNT_DO(stmt) do {stmt;} while (0)
#else
#define SHADER_COUNT_DO(stmt) do {} while (0)
#endif

#if ENABLE_OTHER_MODE_SWAP_COUNTER == 1
#define MODE_SWAP_COUNT_DO(stmt) do {stmt;} while (0)
#else
#define MODE_SWAP_COUNT_DO(stmt) do {} while (0)
#endif

float MTX_IDENTITY[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f, 1.0f}};

float MTX_NDC_DOWNSCALE[4][4] = {{1.0f / NDC_SCALE, 0.0f,             0.0f, 0.0f},
                                 {0.0f,             1.0f / NDC_SCALE, 0.0f, 0.0f},
                                 {0.0f,             0.0f,             1.0f, 0.0f},
                                 {0.0f,             0.0f,             0.0f, 1.0f}};

struct XYWidthHeight {
    uint16_t x, y, width, height;
};

union int16x2 {
    struct {
        int16_t s16_upper;
        int16_t s16_lower;
    };
    struct {
        int16_t u;
        int16_t v;
    };
    uint32_t u32;
};

union int16x4 {
    struct {
        int16_t s16_1;
        int16_t s16_2;
        int16_t s16_3;
        int16_t s16_4;
    };
    struct {
        int16_t x;
        int16_t y;
        int16_t z;
        int16_t w;
    };
    struct {
        int16_t uls;
        int16_t ult;
        int16_t width;
        int16_t height;
    };
    struct {
        uint32_t u32_upper;
        uint32_t u32_lower;
    };
    uint64_t u64;
    int64_t s64;
    double f64;
};

union boolx2 {
    bool bools[2];
    int16_t either;
};

// Total size: 16 bytes
struct LoadedVertex {
    union int16x4 position; // 8 bytes (w is unused, garbage value)
    union int16x2 uv;       // 4 bytes
    union RGBA32 color;     // 4 bytes
};

struct TextureHashmapNode {
    struct TextureHashmapNode *next;

    const uint8_t *texture_addr;
    uint8_t fmt, siz;

    uint32_t texture_id;
    uint8_t cms, cmt;
    bool linear_filter;
};
static struct {
    struct TextureHashmapNode *hashmap[1024];
    struct TextureHashmapNode pool[512];
    uint32_t pool_pos;
} gfx_texture_cache;

static struct RSP {
    uint32_t matrix_set;
    float modelview_matrix_stack[MAT_STACK_SIZE][4][4];
    uint8_t modelview_matrix_stack_size;
    float P_matrix[4][4];

    Light_t current_lights[MAX_LIGHTS + 1];
    float current_lights_coeffs[MAX_LIGHTS][3];
    float current_lookat_coeffs[2][3]; // lookat_x, lookat_y
    uint8_t current_num_lights; // includes ambient light
    bool lights_changed;

    uint32_t geometry_mode;

    struct {
        // U0.16
        uint16_t s, t;
    } texture_scaling_factor;

    struct LoadedVertex loaded_vertices[MAX_VERTICES];
    struct LoadedVertex rect_vertices[4]; // Used only for rectangle drawing
} rsp;

static struct RDP {
    const uint8_t *palette;
    struct {
        const uint8_t *addr;
        uint8_t siz;
        uint8_t tile_number;
    } texture_to_load;
    struct {
        const uint8_t *addr;
        uint32_t size_bytes;
    } loaded_texture[2];
    struct {
        uint8_t fmt;
        uint8_t siz;
        uint8_t cms, cmt;
        uint32_t line_size_bytes;
        union int16x4 texture_settings;
    } texture_tile;
    union boolx2 textures_changed;

    uint32_t other_mode_l, other_mode_h;
    uint32_t combine_mode;

    union RGBA32 env_color, prim_color, fill_color;
    struct XYWidthHeight viewport;
    void *z_buf_address;
    void *color_image_address;
} rdp;

static struct ShaderState {
    uint32_t cc_id;
    uint8_t other_flags;
    bool use_alpha;
    bool use_fog;
    bool texture_edge;
    bool use_noise;
    uint8_t num_inputs;
    union boolx2 used_textures;
    uint32_t fog_settings;
    uint32_t matrix_set;
    bool p_mtx_changed, mv_mtx_changed;
    union int16x4 texture_settings;
    union RGBA32 prim_color;
    union RGBA32 env_color;
    enum Stereoscopic3dMode stereo_3d_mode;
    enum IodMode iod_mode;
} shader_state;

static struct RenderingState {
    bool depth_test;
    bool depth_mask;
    bool decal_mode;
    bool alpha_blend;
    uint32_t culling_mode;
    struct XYWidthHeight viewport, scissor;
    struct TextureHashmapNode *textures[2];
} rendering_state;

struct GfxDimensions gfx_current_dimensions;

static bool dropped_frame;

static union VertexBuffer {
    float as_float[MAX_BUFFERED_VERTS * EMU64_STRIDE_MAX];
    uint32_t as_u32[MAX_BUFFERED_VERTS * EMU64_STRIDE_MAX];
    uint8_t as_u8[MAX_BUFFERED_VERTS * EMU64_STRIDE_MAX * 4];
} buf_vbo;

static size_t buf_vbo_len = 0;
static size_t buf_vbo_num_verts = 0;

static struct GfxWindowManagerAPI *gfx_wapi;

#if ENABLE_SHADER_SWAP_COUNTER == 1
int num_shader_swaps = 0, avoided_swaps_recalc = 0, avoided_swaps_combine_mode = 0;
#endif

#if ENABLE_OTHER_MODE_SWAP_COUNTER == 1
static int om_h_sets = 0, om_l_sets = 0, om_h_skips = 0, om_l_skips = 0;
#endif

static void set_other_mode_h(uint32_t other_mode_h);
static void set_other_mode_l(uint32_t other_mode_l);
static void gfx_flush_impl(GRANULAR_FLUSH(uint8_t flush_id));

static void gfx_apply_matrices()
{
    bool apply_p_mtx = false, apply_mv_mtx = false;

    if (shader_state.matrix_set != rsp.matrix_set) {
        shader_state.matrix_set  = rsp.matrix_set;
        gfx_rapi_select_matrix_set(rsp.matrix_set);
        apply_p_mtx = apply_mv_mtx = true;
    }

    if (rsp.matrix_set == MATRIX_SET_NORMAL) {
        if (shader_state.p_mtx_changed) {
            shader_state.p_mtx_changed = false;
            gfx_rapi_set_projection_matrix(rsp.P_matrix);
            apply_p_mtx = true;
        }

        if (shader_state.mv_mtx_changed) {
            shader_state.mv_mtx_changed = false;
            gfx_rapi_set_model_view_matrix(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
            apply_mv_mtx = true;
        }
    }

    if (apply_p_mtx)
        gfx_rapi_apply_projection_matrix();

    if (apply_mv_mtx)
        gfx_rapi_apply_model_view_matrix();
}

static void gfx_flush_impl(GRANULAR_FLUSH(uint8_t flush_id)) {
    granular_log_time(0);

    // Over 50% of calls are pointless
    if (UNLIKELY(buf_vbo_num_verts > 0)) {
        GRANULAR_FLUSH_DO(tris_per_flush[flush_id][flushes[flush_id]] += buf_vbo_num_verts / 3);
        GRANULAR_FLUSH_DO(flushes[flush_id]++);
        gfx_apply_matrices();

        gfx_rapi_draw_triangles(buf_vbo.as_float, buf_vbo_len, buf_vbo_num_verts / 3);
        buf_vbo_len = 0;
        buf_vbo_num_verts = 0;
    } else 
        GRANULAR_FLUSH_DO(flush_avoids[flush_id]++);

    granular_log_time(12); // gfx_flush
}

static void gfx_set_2d(int mode_2d)
{
    if (shader_state.stereo_3d_mode != mode_2d) {
        shader_state.stereo_3d_mode  = mode_2d;
        gfx_flush(0);
        
        gfx_rapi_set_2d_mode(mode_2d);
    }
}

static void gfx_set_iod(uint32_t iod)
{
    if (shader_state.iod_mode != iod) {
        shader_state.iod_mode  = iod;
        gfx_flush(1);

        float z, w;
        switch(iod) {
            case IOD_NORMAL :
                z = 8.0f;
                w = 16.0f;
                break;
            case IOD_GODDARD :
                z = 0.5f;
                w = 0.5f;
                break;
            case IOD_FILE_SELECT :
                z = 96.0f;
                w = 128.0f;
                break;
            case IOD_STAR_SELECT :
                z = 128.0f;
                w = 76.0f;
                break;
            case IOD_CANNON :
                z = 0.0f;
                w = -128.0f;
                break;
        }
        gfx_rapi_set_iod(z, w);
    }
}

static bool gfx_texture_cache_lookup(int tile, struct TextureHashmapNode **n, const uint8_t *orig_addr, uint32_t fmt, uint32_t siz) {
    size_t hash = (uintptr_t)orig_addr;
    hash = (hash >> 5) & 0x3ff;
    struct TextureHashmapNode **node = &gfx_texture_cache.hashmap[hash];
    while (*node != NULL && *node - gfx_texture_cache.pool < (int)gfx_texture_cache.pool_pos) {
        if ((*node)->texture_addr == orig_addr && (*node)->fmt == fmt && (*node)->siz == siz) {
            gfx_rapi_select_texture(tile, (*node)->texture_id);
            *n = *node;
            return true;
        }
        node = &(*node)->next;
    }
    if (gfx_texture_cache.pool_pos == ARRAY_COUNT(gfx_texture_cache.pool)) {
        // Pool is full. We just invalidate everything and start over.
        gfx_texture_cache.pool_pos = 0;
        node = &gfx_texture_cache.hashmap[hash];
        //puts("Clearing texture cache");
    }
    *node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];
    if ((*node)->texture_addr == NULL) {
        (*node)->texture_id = gfx_rapi_new_texture();
    }
    gfx_rapi_select_texture(tile, (*node)->texture_id);
    gfx_rapi_set_sampler_parameters(tile, false, 0, 0);
    (*node)->cms = 0;
    (*node)->cmt = 0;
    (*node)->linear_filter = false;
    (*node)->next = NULL;
    (*node)->texture_addr = orig_addr;
    (*node)->fmt = fmt;
    (*node)->siz = siz;
    *n = *node;
    return false;
}

static void import_texture(int tile) {
    uint8_t fmt = rdp.texture_tile.fmt;
    uint8_t siz = rdp.texture_tile.siz;

    if (gfx_texture_cache_lookup(tile, &rendering_state.textures[tile], rdp.loaded_texture[tile].addr, fmt, siz)) {
        return;
    }

    uint32_t line_size = rdp.texture_tile.line_size_bytes,
             tile_size = rdp.loaded_texture[tile].size_bytes;

    int width, height;

    switch (siz) {
        case G_IM_SIZ_32b:
            width = line_size / 2;
            height = (tile_size / 2) / line_size;
            break;
        case G_IM_SIZ_16b:
            width = line_size / 2;
            height = tile_size / line_size;
            break;
        case G_IM_SIZ_8b:
            width = line_size;
            height = tile_size / line_size;
            break;
        case G_IM_SIZ_4b:
            width = line_size * 2;
            height = tile_size / line_size;
            break;
        default:
            abort();
    }

    const uint8_t* data = rdp.loaded_texture[tile].addr;
    const uint8_t *palette = rdp.palette;

    switch (TEX_FORMAT(fmt, siz)) {
        case TEXFMT_RGBA32:
            gfx_rapi_upload_texture_rgba32(data, width, height); // Unused by SM64
            break;
        case TEXFMT_RGBA16:
            gfx_rapi_upload_texture_rgba16(data, width, height);
            break;
        case TEXFMT_IA4:
            gfx_rapi_upload_texture_ia4(data, width, height); // Used by text only
            break;
        case TEXFMT_IA8:
            gfx_rapi_upload_texture_ia8(data, width, height);
            break;
        case TEXFMT_IA16:
            gfx_rapi_upload_texture_ia16(data, width, height);
            break;
        case TEXFMT_I4:
            gfx_rapi_upload_texture_i4(data, width, height); // Unused by SM64
            break;
        case TEXFMT_I8:
            gfx_rapi_upload_texture_i8(data, width, height); // Unused by SM64
            break;
        case TEXFMT_CI4:
            gfx_rapi_upload_texture_ci4(data, palette, width, height); // Unused by SM64
            break;
        case TEXFMT_CI8:
            gfx_rapi_upload_texture_ci8(data, palette, width, height); // Unused by SM64
            break;
        default:
            abort();
    }
}

static void gfx_normalize_vector(float v[3]) {
    float s = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= s;
    v[1] /= s;
    v[2] /= s;
}

static void gfx_transposed_matrix_mul(float res[3], const float a[3], const float b[4][4]) {
    res[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2];
    res[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2];
    res[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2];
}

static void calculate_normal_dir(const Light_t *light, float output[3]) {
    float light_dir[3] = {
        light->dir[0] / 127.0f,
        light->dir[1] / 127.0f,
        light->dir[2] / 127.0f
    };
    gfx_transposed_matrix_mul(output, light_dir, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
    gfx_normalize_vector(output);
}

// lookat_x = {1, 0, 0};
static void calculate_lookat_x(float res[3], const float b[4][4])
{
    res[0] = b[0][0];
    res[1] = b[1][0];
    res[2] = b[2][0];
    gfx_normalize_vector(res);
}

// lookat_y = {0, 1, 0};
static void calculate_lookat_y(float res[3], const float b[4][4])
{
    res[0] = b[0][1];
    res[1] = b[1][1];
    res[2] = b[2][1];
    gfx_normalize_vector(res);
}

// Multiplies the whole matrix. When both funcs are inline, saves ~200us.
static inline void gfx_matrix_mul_unsafe(float res[4][4], const float* restrict a, const float* restrict  b) {
    #define MTA(r_, c_) a[ARR_INDEX_2D(c_, r_, 4)]
    #define MTB(r_, c_) b[ARR_INDEX_2D(c_, r_, 4)]

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            res[r][c] = MTA(r, 0) * MTB(0, c) + MTA(r, 1) * MTB(1, c) + MTA(r, 2) * MTB(2, c) + MTA(r, 3) * MTB(3, c);
        }
    }

    #undef MTA
    #undef MTB
}

// Multiplies the whole matrix, using a temporary variable. Use only when a == res || b == res.
static inline void gfx_matrix_mul_safe(float res[4][4], const float* a, const float* b) {
    float tmp[4][4];
    gfx_matrix_mul_unsafe(tmp, a, b);
    memcpy(res, tmp, sizeof(tmp));
}

const float *last_mv_mtx_addr = NULL, *last_p_mtx_addr = NULL;
static void gfx_sp_matrix(uint8_t parameters, const int32_t *addr) {

#ifndef GBI_FLOATS
    const float matrix[4][4];
    // Original GBI where fixed point matrices are used
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            int32_t int_part = addr[i * 2 + j / 2];
            uint32_t frac_part = addr[8 + i * 2 + j / 2];
            matrix[i][j] = (int32_t)((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
            matrix[i][j + 1] = (int32_t)((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
        }
    }
#else
    const float* matrix = (float*) addr; // WYATT_TODO SHUT UP, COMPILER! UB but it works.
#endif

    const bool is_load = parameters & G_MTX_LOAD,
               is_push = parameters & G_MTX_PUSH; // NOPUSH means multiplication dst and src overlap

    if (UNLIKELY(parameters & G_MTX_PROJECTION)) {

        bool matrix_updated = !(last_p_mtx_addr == matrix && is_load);

        if (matrix_updated) {
            gfx_flush(2);
            last_p_mtx_addr = matrix;
        }

        if (is_load) {
            if (matrix_updated) {
                memcpy(rsp.P_matrix, matrix, sizeof(float[4][4]));
                shader_state.p_mtx_changed = true;
            }
        }
        else {
            gfx_matrix_mul_safe(rsp.P_matrix, matrix, (float*) rsp.P_matrix);
            shader_state.p_mtx_changed = true;
        }

    } else { // G_MTX_MODELVIEW
        float* src = (float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1];
        
        bool matrix_updated = !(last_mv_mtx_addr == matrix && is_load);

        if (matrix_updated) {
            gfx_flush(3);
            last_mv_mtx_addr = matrix;
            rsp.lights_changed = true;
        }
        
        if (is_push && rsp.modelview_matrix_stack_size < 11)
            ++rsp.modelview_matrix_stack_size;

        if (is_load) {
            if (matrix_updated) {
                shader_state.mv_mtx_changed = true;
                memcpy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix, sizeof(float[4][4]));
            }
        } else {
            shader_state.mv_mtx_changed = true;
            if (is_push)
                gfx_matrix_mul_unsafe(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix, src);
            else
                gfx_matrix_mul_safe(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix, src);
        }
    }
}

// SM64 only ever pops 1 matrix at a time, and never 0.
static void gfx_sp_pop_matrix(uint32_t count) {
    gfx_flush(4);

    // If you go below 0, you're already going to get UB, so we might as well not check the range.
    // rsp.modelview_matrix_stack_size = UNLIKELY(count > rsp.modelview_matrix_stack_size) ? rsp.modelview_matrix_stack_size : count;
    rsp.modelview_matrix_stack_size -= count;
    shader_state.mv_mtx_changed = true;
}

static void gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx *vertices) {
    granular_log_time(0);

    // Load each vert
    for (size_t vert = 0, dest = dest_index; vert < n_vertices; vert++, dest++) {
        const Vtx_t *v = &vertices[vert].v;
        struct LoadedVertex *d = &rsp.loaded_vertices[dest];
        
        d->position.u64 = *((uint64_t*) (&v->ob[0])); // W is set to garbage
        d->color.u32 = *((uint32_t*) v->cn);
    }
    granular_log_time(5); // Vertex Copy

    // Calculate lighting
    if (LIKELY(rsp.geometry_mode & G_LIGHTING)) {
        if (rsp.lights_changed) {
            rsp.lights_changed = false;
            for (int light = 0; light < rsp.current_num_lights - 1; light++)
                calculate_normal_dir(&rsp.current_lights[light], rsp.current_lights_coeffs[light]);

            calculate_lookat_x(rsp.current_lookat_coeffs[0], rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
            calculate_lookat_y(rsp.current_lookat_coeffs[1], rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
            granular_log_time(6); // Light Recalculation
        }

        /* Required uniforms:
         * rsp.current_lights color:  u8vec[3]
         * rsp.current_lights_coeffs: fvec[2] (ambient does not need one)
         * rsp.current_num_lights:    u8
         * lighting enable flag:      bool
         * fallback color and alpha:  fvec
         * 
         * Required VBO values:
         * Vertex normals (u8[3], can replace current CC_SHADE colors)
         * Vertex alpha (u8[1], retain)
         * 
         * Shaders required: all with color inputs
         */
        for (size_t vert = 0, dest = dest_index; vert < n_vertices; vert++, dest++) {
            const Vtx_tn *vn = &vertices[vert].n;
            struct LoadedVertex *d = &rsp.loaded_vertices[dest];

            int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];
            int g = rsp.current_lights[rsp.current_num_lights - 1].col[1];
            int b = rsp.current_lights[rsp.current_num_lights - 1].col[2];

            ASSUME(rsp.current_num_lights <= (MAX_LIGHTS + 1));
            for (int light = 0; light < rsp.current_num_lights - 1; light++) {
                float intensity = 0;
                intensity += vn->n[0] * rsp.current_lights_coeffs[light][0];
                intensity += vn->n[1] * rsp.current_lights_coeffs[light][1];
                intensity += vn->n[2] * rsp.current_lights_coeffs[light][2];
                intensity /= 127.0f;
                if (intensity > 0.0f) {
                    r += intensity * rsp.current_lights[light].col[0];
                    g += intensity * rsp.current_lights[light].col[1];
                    b += intensity * rsp.current_lights[light].col[2];
                }
            }

            // Why is UCLAMP8 slower here? It should be faster, but it's like 150us slower.
            d->color.r = r > 255 ? 255 : r;
            d->color.g = g > 255 ? 255 : g;
            d->color.b = b > 255 ? 255 : b;
        }
        
        granular_log_time(7); // Vertex Light Calculation
    }
    
    // Calculate texcoords
    if (UNLIKELY((rsp.geometry_mode & G_LIGHTING) && (rsp.geometry_mode & G_TEXTURE_GEN))) {
        for (size_t vert = 0, dest = dest_index; vert < n_vertices; vert++, dest++) {
            const Vtx_tn *vn = &vertices[vert].n;
            struct LoadedVertex *d = &rsp.loaded_vertices[dest];

            const float
            dotx = vn->n[0] * rsp.current_lookat_coeffs[0][0]
                 + vn->n[1] * rsp.current_lookat_coeffs[0][1]
                 + vn->n[2] * rsp.current_lookat_coeffs[0][2],

            doty = vn->n[0] * rsp.current_lookat_coeffs[1][0]
                 + vn->n[1] * rsp.current_lookat_coeffs[1][1]
                 + vn->n[2] * rsp.current_lookat_coeffs[1][2];

            d->uv.u = (int16_t) ((dotx / 127.0f + 1.0f) / 4.0f * rsp.texture_scaling_factor.s);
            d->uv.v = (int16_t) ((doty / 127.0f + 1.0f) / 4.0f * rsp.texture_scaling_factor.t);
        }
        granular_log_time(8); // Texgen Calculation
    } else {
        for (size_t vert = 0, dest = dest_index; vert < n_vertices; vert++, dest++) {
            const Vtx_t *v = &vertices[vert].v;
            struct LoadedVertex *d = &rsp.loaded_vertices[dest];

            d->uv.u = v->tc[0] * rsp.texture_scaling_factor.s >> 16;
            d->uv.v = v->tc[1] * rsp.texture_scaling_factor.t >> 16;
        }
        granular_log_time(9); // Texcoord Copy
    }
}

static void gfx_sp_tri_update_state()
{
    granular_log_time(0);
    uint32_t cc_id = shader_state.cc_id;

    static uint32_t prev_cc_id = DELIBERATELY_INVALID_CC_ID;

    if (prev_cc_id != cc_id) {
        prev_cc_id  = cc_id;
        SHADER_COUNT_DO(num_shader_swaps++);

        granular_log_time(10); // gfx_sp_tri_update_state
        gfx_flush(5);
        granular_log_time(0);

        size_t cc_index = gfx_rapi_lookup_or_create_color_combiner(cc_id);
        gfx_rapi_color_combiner_get_info(cc_index, &shader_state.num_inputs, shader_state.used_textures.bools);
        gfx_rapi_select_color_combiner(cc_index);
    } else
        SHADER_COUNT_DO(avoided_swaps_recalc++);

    if (UNLIKELY(shader_state.prim_color.u32 != rdp.prim_color.u32)) {
        shader_state.prim_color.u32  = rdp.prim_color.u32;

        granular_log_time(10); // gfx_sp_tri_update_state
        gfx_flush(6);
        granular_log_time(0);

        gfx_rapi_set_cc_prim_color(rdp.prim_color.u32);
    }

    if (UNLIKELY(shader_state.env_color.u32 != rdp.env_color.u32)) {
        shader_state.env_color.u32  = rdp.env_color.u32;

        granular_log_time(10); // gfx_sp_tri_update_state
        gfx_flush(7);
        granular_log_time(0);

        gfx_rapi_set_cc_env_color(rdp.env_color.u32);
    }
        
    const bool linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
    if (shader_state.used_textures.either) {
        static bool linear_filter_old = false;
        if (linear_filter_old != linear_filter) {
            linear_filter_old  = linear_filter;
            granular_log_time(10); // gfx_sp_tri_update_state
            gfx_flush(8);
            granular_log_time(0);
            gfx_rapi_set_uv_offset(linear_filter ? 0.5f : 0.0f);
        }

        if (shader_state.texture_settings.u64 != rdp.texture_tile.texture_settings.u64) {
            shader_state.texture_settings.u64  = rdp.texture_tile.texture_settings.u64;
            granular_log_time(10); // gfx_sp_tri_update_state
            gfx_flush(9);
            granular_log_time(0);
            gfx_rapi_set_texture_settings(rdp.texture_tile.texture_settings.uls, rdp.texture_tile.texture_settings.ult, rdp.texture_tile.texture_settings.width, rdp.texture_tile.texture_settings.height);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (shader_state.used_textures.bools[i]) {
            if (rdp.textures_changed.bools[i]) {
                granular_log_time(10); // gfx_sp_tri_update_state
                gfx_flush(10);
                granular_log_time(0);
                import_texture(i);
                rdp.textures_changed.bools[i] = false;
            }
            if (linear_filter != rendering_state.textures[i]->linear_filter || rdp.texture_tile.cms != rendering_state.textures[i]->cms || rdp.texture_tile.cmt != rendering_state.textures[i]->cmt) {
                granular_log_time(10); // gfx_sp_tri_update_state
                gfx_flush(11);
                granular_log_time(0);
                gfx_rapi_set_sampler_parameters(i, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
                rendering_state.textures[i]->linear_filter = linear_filter;
                rendering_state.textures[i]->cms = rdp.texture_tile.cms;
                rendering_state.textures[i]->cmt = rdp.texture_tile.cmt;
            }
        }
    }

    // 75% savings with good numbers (potentially outdated metric)
    // Handled here to optimize rectangle drawing
    const uint32_t culling_mode = (rsp.geometry_mode & G_CULL_BOTH);
    if (rendering_state.culling_mode != culling_mode) {
        rendering_state.culling_mode = culling_mode;
        gfx_flush(12);
        gfx_rapi_set_backface_culling_mode(culling_mode);
    }

    // Nearly 100% savings with good numbers (potentially outdated metric)
    // Handled here to optimize rectangle drawing
    const bool depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if (rendering_state.depth_test != depth_test) {
        rendering_state.depth_test  = depth_test;
        gfx_flush(13);
        gfx_rapi_set_depth_test(depth_test);
    }

    // Handled here to optimize rectangle drawing
    if (!XYWH_EQUAL(rendering_state.viewport, rdp.viewport)) {
        rendering_state.viewport = rdp.viewport;
        gfx_flush(14);
        gfx_rapi_set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
    }

    granular_log_time(10); // gfx_sp_tri_update_state
}

static void gfx_tri_create_vbo(struct LoadedVertex * v_arr[], uint32_t numTris)
{
    granular_log_time(0);

    // WYATT_TODO fix this for very large batches. Fine for vanilla.
    const uint32_t numVerts = numTris * 3;
    if (buf_vbo_num_verts + numVerts >= MAX_BUFFERED_VERTS) {
        granular_log_time(11); // gfx_tri_create_vbo
        gfx_flush(15);
        granular_log_time(0);
    }
    buf_vbo_num_verts += numVerts;

    const bool use_texture = shader_state.used_textures.either;
    const bool use_color   = shader_state.num_inputs > 0;

    for (uint32_t vtx = 0; vtx < numVerts; vtx++) {
        // Struct copy
        *((union int16x4*) (&buf_vbo.as_u32[buf_vbo_len])) = v_arr[vtx]->position;
        buf_vbo_len += 2;

        if (use_texture)
            buf_vbo.as_u32[buf_vbo_len++] = v_arr[vtx]->uv.u32;

        if (use_color)
            buf_vbo.as_u32[buf_vbo_len++] = v_arr[vtx]->color.u32;
    }
    
    granular_log_time(11); // gfx_tri_create_vbo
}

static void gfx_sp_tri_batched(struct LoadedVertex **v_arr, uint32_t num_tris) {
    gfx_sp_tri_update_state();
    gfx_tri_create_vbo(v_arr, num_tris);
}

static void gfx_sp_geometry_mode(uint32_t clear, uint32_t set) {
    rsp.geometry_mode &= ~clear;
    rsp.geometry_mode |= set;
}

static void gfx_set_viewport(struct XYWidthHeight viewport) {
    rdp.viewport.x = viewport.x;
    rdp.viewport.y = viewport.y;
    rdp.viewport.width = viewport.width;
    rdp.viewport.height = viewport.height;
}

static void gfx_calc_and_set_viewport(const Vp_t *viewport_raw) {
    // 2 bits fraction
    float width = 2.0f * viewport_raw->vscale[0] / 4.0f;
    float height = 2.0f * viewport_raw->vscale[1] / 4.0f;
    float x = (viewport_raw->vtrans[0] / 4.0f) - width / 2.0f;
    float y = SCREEN_HEIGHT - ((viewport_raw->vtrans[1] / 4.0f) + height / 2.0f);

    width *= RATIO_X;
    height *= RATIO_Y;
    x *= RATIO_X;
    y *= RATIO_Y;

    struct XYWidthHeight viewport = {x, y, width, height};
    gfx_set_viewport(viewport);
}

static void gfx_sp_movemem(uint8_t index, uint8_t offset, const void* data) {
#ifdef F3DEX_GBI_2

    // NOTE: reads out of bounds if it is an ambient light
    // This used to avoid overwriting the lookat mtx, but that data no longer even makes it this far.
    if (LIKELY(index == G_MV_LIGHT)) {
        const int lightidx = offset / 24;
        memcpy((rsp.current_lights - 2) + lightidx, data, sizeof(Light_t));
    } else { // G_MV_VIEWPORT
        gfx_calc_and_set_viewport((const Vp_t *) data);
    }

// Original GBI (me no care)
#else
    switch (index) {
        case G_MV_VIEWPORT:
            gfx_calc_and_set_viewport((const Vp_t *) data);
            break;
#if 0
        case G_MV_LOOKATY:
        case G_MV_LOOKATX:
            memcpy(rsp.current_lookat + (index - G_MV_LOOKATY) / 2, data, sizeof(Light_t));
            //rsp.lights_changed = 1;
            break;
#endif
        case G_MV_L0:
        case G_MV_L1:
        case G_MV_L2:
            // NOTE: reads out of bounds if it is an ambient light
            memcpy(rsp.current_lights + (index - G_MV_L0) / 2, data, sizeof(Light_t));
            break;
    }
#endif
}

static void gfx_sp_moveword(uint8_t index, UNUSED uint16_t offset, uint32_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
#ifdef F3DEX_GBI_2
            rsp.current_num_lights = data / 24 + 1; // add ambient light
#else
            // Ambient light is included
            // The 31th bit is a flag that lights should be recalculated
            rsp.current_num_lights = (data - 0x80000000U) / 32;
#endif
            rsp.lights_changed = 1;
            break;
        case G_MW_FOG:
            if (shader_state.fog_settings != data) {
                shader_state.fog_settings  = data;
                gfx_flush(16);
                uint16_t fog_mul = (int16_t)(data >> 16),
                         fog_offset = (int16_t)data;
                gfx_rapi_set_fog(fog_mul, fog_offset);
            }
            break;
    }
}

static void gfx_sp_texture(uint16_t sc, uint16_t tc, UNUSED uint8_t level, UNUSED uint8_t tile, UNUSED uint8_t on) {
    rsp.texture_scaling_factor.s = sc;
    rsp.texture_scaling_factor.t = tc;
}

static void gfx_dp_set_scissor(UNUSED uint32_t mode, uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry) {
    float x = ulx / 4.0f * RATIO_X;
    float y = (SCREEN_HEIGHT - lry / 4.0f) * RATIO_Y;
    float width = (lrx - ulx) / 4.0f * RATIO_X;
    float height = (lry - uly) / 4.0f * RATIO_Y;

    struct XYWidthHeight scissor = {x, y, width, height};
    if (!XYWH_EQUAL(rendering_state.scissor, scissor)) {
        rendering_state.scissor = scissor;
        gfx_flush(17);
        gfx_rapi_set_scissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }
}

static void gfx_dp_set_texture_image(UNUSED uint32_t format, uint32_t size, UNUSED uint32_t width, const void* addr) {
    rdp.texture_to_load.addr = addr;
    rdp.texture_to_load.siz = size;
}

static void gfx_dp_set_tile(uint8_t fmt, uint32_t siz, uint32_t line, uint32_t tmem, uint8_t tile, UNUSED uint32_t palette, uint32_t cmt, UNUSED uint32_t maskt, UNUSED uint32_t shiftt, uint32_t cms, UNUSED uint32_t masks, UNUSED uint32_t shifts) {
    // G_TX_RENDERTILE is always >= G_TX_LOADTILE
    if (LIKELY(tile == G_TX_RENDERTILE)) {
        SUPPORT_CHECK(palette == 0); // palette should set upper 4 bits of color index in 4b mode
        rdp.texture_tile.fmt = fmt;
        rdp.texture_tile.siz = siz;
        rdp.texture_tile.cms = cms;
        rdp.texture_tile.cmt = cmt;
        rdp.texture_tile.line_size_bytes = line * 8;
        rdp.textures_changed.bools[0] = true;
        rdp.textures_changed.bools[1] = true;
    }

    // Only valid data is ever sent.
    else // if (tile == G_TX_LOADTILE) {
        rdp.texture_to_load.tile_number = tmem / 256;
}

static void set_tile_size_internal(uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
    rdp.texture_tile.texture_settings.uls = uls;
    rdp.texture_tile.texture_settings.ult = ult;
    rdp.texture_tile.texture_settings.width  = lrs - uls + 4;
    rdp.texture_tile.texture_settings.height = lrt - ult + 4;
}

static void gfx_dp_set_tile_size(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
    if (tile == G_TX_RENDERTILE) {
        set_tile_size_internal(uls, ult, lrs, lrt);
        rdp.textures_changed.bools[0] = true;
        rdp.textures_changed.bools[1] = true;
    }
}

static void gfx_dp_load_tlut(UNUSED uint8_t tile, UNUSED uint32_t high_index) {
    SUPPORT_CHECK(tile == G_TX_LOADTILE);
    SUPPORT_CHECK(rdp.texture_to_load.siz == G_IM_SIZ_16b);
    rdp.palette = rdp.texture_to_load.addr;
}

static void gfx_dp_load_block(uint8_t tile, UNUSED uint32_t uls, UNUSED uint32_t ult, uint32_t lrs, UNUSED uint32_t dxt) {
    if (tile == 1) return;
    SUPPORT_CHECK(tile == G_TX_LOADTILE);
    SUPPORT_CHECK(uls == 0);
    SUPPORT_CHECK(ult == 0);

    // The lrs field rather seems to be number of pixels to load
    uint32_t word_size_shift;
    switch (rdp.texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0; // Or -1? It's unused in SM64 anyway.
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }
    uint32_t size_bytes = (lrs + 1) << word_size_shift;
    rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;
    SUPPORT_CHECK(size_bytes <= 4096 && "bug: too big texture");
    rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;

    rdp.textures_changed.bools[rdp.texture_to_load.tile_number] = true;
}

static void gfx_dp_load_tile(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
    if (tile == 1) return;
    SUPPORT_CHECK(tile == G_TX_LOADTILE);
    SUPPORT_CHECK(uls == 0);
    SUPPORT_CHECK(ult == 0);

    uint32_t word_size_shift;
    switch (rdp.texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }

    uint32_t size_bytes = (((lrs >> G_TEXTURE_IMAGE_FRAC) + 1) * ((lrt >> G_TEXTURE_IMAGE_FRAC) + 1)) << word_size_shift;
    rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;

    SUPPORT_CHECK(size_bytes <= 4096 && "bug: too big texture");
    rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;
    
    set_tile_size_internal(uls, ult, lrs, lrt);
    rdp.textures_changed.bools[rdp.texture_to_load.tile_number] = true;
}


static uint8_t color_comb_component(uint32_t v) {
    switch (v) {
        case G_CCMUX_TEXEL0:
            return CC_TEXEL0;
        case G_CCMUX_TEXEL1:
            return CC_TEXEL1;
        case G_CCMUX_PRIMITIVE:
            return CC_PRIM;
        case G_CCMUX_SHADE:
            return CC_SHADE;
        case G_CCMUX_ENVIRONMENT:
            return CC_ENV;
        case G_CCMUX_TEXEL0_ALPHA:
            return CC_TEXEL0A;
        case G_CCMUX_LOD_FRACTION:
            return CC_LOD;
        default:
            return CC_0;
    }
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return color_comb_component(a) |
           (color_comb_component(b) << 3) |
           (color_comb_component(c) << 6) |
           (color_comb_component(d) << 9);
}

static void shader_state_init(struct ShaderState* ss)
{
    ss->cc_id = DELIBERATELY_INVALID_CC_ID;
    ss->num_inputs = 0;
    ss->other_flags = 0;
    ss->texture_edge = false;
    ss->use_alpha = false;
    ss->use_fog = false;
    ss->use_noise = false;
    ss->used_textures.either = 0;
    ss->matrix_set = MATRIX_SET_INVALID;
    ss->p_mtx_changed = true;
    ss->mv_mtx_changed = true;
    ss->texture_settings.u64 = ~0;
    ss->prim_color.u32 = ~rdp.prim_color.u32;
    ss->env_color.u32  = ~rdp.env_color.u32;
    ss->stereo_3d_mode = STEREO_MODE_COUNT;
    ss->iod_mode = IOD_COUNT;
}

static void calculate_cc_id()
{
    shader_state.cc_id = rdp.combine_mode;

    if (shader_state.use_fog)      shader_state.cc_id |= SHADER_OPT_FOG;
    if (shader_state.texture_edge) shader_state.cc_id |= SHADER_OPT_TEXTURE_EDGE;
    if (shader_state.use_noise)    shader_state.cc_id |= SHADER_OPT_NOISE;
    if (shader_state.use_alpha)
        shader_state.cc_id |= SHADER_OPT_ALPHA;
    else
        shader_state.cc_id &= ~0xfff000;
}

static void gfx_dp_set_combine_mode(uint32_t combine_mode) {
#if ENABLE_SHADER_SWAP_COUNTER == 1
    // Low savings: usually under 1%
    if (rdp.combine_mode != combine_mode) {
        rdp.combine_mode  = combine_mode;
        calculate_cc_id();
    } else
        avoided_swaps_combine_mode++;
#else
    rdp.combine_mode  = combine_mode;
    calculate_cc_id();
#endif
}

static void gfx_dp_set_env_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rdp.env_color.r = r;
    rdp.env_color.g = g;
    rdp.env_color.b = b;
    rdp.env_color.a = a;
}

static void gfx_dp_set_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rdp.prim_color.r = r;
    rdp.prim_color.g = g;
    rdp.prim_color.b = b;
    rdp.prim_color.a = a;
}

static void gfx_dp_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    gfx_flush(18);
    gfx_rapi_set_fog_color(r, g, b, a);
}

static void gfx_dp_set_fill_color(uint32_t packed_color) {
    uint16_t col16 = (uint16_t)packed_color;
    uint32_t r = col16 >> 11;
    uint32_t g = (col16 >> 6) & 0x1f;
    uint32_t b = (col16 >> 1) & 0x1f;
    uint32_t a = col16 & 1;
    rdp.fill_color.r = SCALE_5_8(r);
    rdp.fill_color.g = SCALE_5_8(g);
    rdp.fill_color.b = SCALE_5_8(b);
    rdp.fill_color.a = a * 255;
}

static void gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    gfx_flush(19);

    uint32_t saved_other_mode_h = rdp.other_mode_h;
    uint32_t cycle_type = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));
    uint32_t saved_matrix_set = rsp.matrix_set;

    if (cycle_type == G_CYC_COPY)
        set_other_mode_h((rdp.other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT);

    rsp.matrix_set = MATRIX_SET_SCALED_NDC;

    // We need to scale our NDCs up here to keep precision with s16s, then back down
    // in the shader with one of the matrices.
    int16_t ulx16 = (  ulx / (4.0f * HALF_SCREEN_WIDTH)   - 1.0f) * NDC_SCALE;
    int16_t uly16 = (-(uly / (4.0f * HALF_SCREEN_HEIGHT)) + 1.0f) * NDC_SCALE;
    int16_t lrx16 = (  lrx / (4.0f * HALF_SCREEN_WIDTH)   - 1.0f) * NDC_SCALE;
    int16_t lry16 = (-(lry / (4.0f * HALF_SCREEN_HEIGHT)) + 1.0f) * NDC_SCALE;

    static struct LoadedVertex* const ul = &rsp.rect_vertices[0];
    static struct LoadedVertex* const ll = &rsp.rect_vertices[1];
    static struct LoadedVertex* const lr = &rsp.rect_vertices[2];
    static struct LoadedVertex* const ur = &rsp.rect_vertices[3];

    ul->position.x = ulx16;
    ul->position.y = uly16;

    ll->position.x = ulx16;
    ll->position.y = lry16;

    lr->position.x = lrx16;
    lr->position.y = lry16;

    ur->position.x = lrx16;
    ur->position.y = uly16;

    uint32_t geometry_mode_saved = rsp.geometry_mode;
    gfx_sp_geometry_mode(~0, 0);

    struct XYWidthHeight viewport_saved = rdp.viewport;
    struct XYWidthHeight viewport = {0, 0, gfx_current_dimensions.width, gfx_current_dimensions.height};
    gfx_set_viewport(viewport);

    static struct LoadedVertex* rect_triangles[] =
       {&rsp.rect_vertices[0],
        &rsp.rect_vertices[1],
        &rsp.rect_vertices[3],
        &rsp.rect_vertices[1],
        &rsp.rect_vertices[2],
        &rsp.rect_vertices[3]};

    gfx_sp_tri_batched(rect_triangles, 2);
    gfx_flush(20);

    rsp.matrix_set = saved_matrix_set;
    
    gfx_sp_geometry_mode(0, geometry_mode_saved);
    gfx_set_viewport(viewport_saved);

    if (cycle_type == G_CYC_COPY)
        set_other_mode_h(saved_other_mode_h);
}

static void gfx_dp_texture_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, UNUSED uint8_t tile, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip) {
    uint32_t saved_combine_mode = rdp.combine_mode;
    if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_COPY) {
        // Per RDP Command Summary Set Tile's shift s and this dsdx should be set to 4 texels
        // Divide by 4 to get 1 instead
        dsdx >>= 2;

        // Color combiner is turned off in copy mode
        gfx_dp_set_combine_mode(COMBINE_MODE(color_comb(0, 0, 0, G_CCMUX_TEXEL0), color_comb(0, 0, 0, G_ACMUX_TEXEL0)));

        // Per documentation one extra pixel is added in these modes to each edge
        // WYATT_TODO should we not adjust the top-left coordinates as well?
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    // uls and ult are S10.5
    // dsdx and dtdy are S5.10
    // lrx, lry, ulx, uly are U10.2
    // lrs, lrt are S10.5
    if (flip) {
        dsdx = -dsdx;
        dtdy = -dtdy;
    }
    int16_t width = !flip ? lrx - ulx : lry - uly;
    int16_t height = !flip ? lry - uly : lrx - ulx;
    float lrs = ((uls << 7) + dsdx * width) >> 7;
    float lrt = ((ult << 7) + dtdy * height) >> 7;

    static struct LoadedVertex* const ul = &rsp.rect_vertices[0];
    static struct LoadedVertex* const ll = &rsp.rect_vertices[1];
    static struct LoadedVertex* const lr = &rsp.rect_vertices[2];
    static struct LoadedVertex* const ur = &rsp.rect_vertices[3];
    
    ul->uv.u = uls;
    ul->uv.v = ult;
    lr->uv.u = lrs;
    lr->uv.v = lrt;
    if (!flip) {
        ll->uv.u = uls;
        ll->uv.v = lrt;
        ur->uv.u = lrs;
        ur->uv.v = ult;
    } else {
        ll->uv.u = lrs;
        ll->uv.v = ult;
        ur->uv.u = uls;
        ur->uv.v = lrt;
    }

    gfx_draw_rectangle(ulx, uly, lrx, lry);
    gfx_dp_set_combine_mode(saved_combine_mode);
}

static void gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    if (UNLIKELY(rdp.color_image_address == rdp.z_buf_address)) {
        // Don't clear Z buffer here since we already did it with glClear
        return;
    }
    uint32_t cycle_type = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    if (cycle_type == G_CYC_COPY || cycle_type == G_CYC_FILL) {
        // Per documentation one extra pixel is added in these modes to each edge
        // WYATT_TODO should we not adjust the top-left coordinates as well?
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    for (int i = 0; i < ARRAY_COUNT(rsp.rect_vertices); i++)
        rsp.rect_vertices[i].color = rdp.fill_color;

    uint32_t saved_combine_mode = rdp.combine_mode;
    gfx_dp_set_combine_mode(COMBINE_MODE(color_comb(0, 0, 0, G_CCMUX_SHADE), color_comb(0, 0, 0, G_ACMUX_SHADE)));
    gfx_draw_rectangle(ulx, uly, lrx, lry);
    gfx_dp_set_combine_mode(saved_combine_mode);
}

static void gfx_dp_set_z_image(void *z_buf_address) {
    rdp.z_buf_address = z_buf_address;
}

static void gfx_dp_set_color_image(UNUSED uint32_t format, UNUSED uint32_t size, UNUSED uint32_t width, void* address) {
    rdp.color_image_address = address;
}

static void set_other_mode_h(uint32_t other_mode_h)
{
#if ENABLE_OTHER_MODE_SWAP_COUNTER == 1
    // About 50% savings, but relatively low in count (aside from goddard)
    if (rdp.other_mode_h != other_mode_h) {
        rdp.other_mode_h  = other_mode_h;
        om_h_sets++;
    }
    else
        om_h_skips++;
#else
    rdp.other_mode_h  = other_mode_h;
#endif
}

static void set_other_mode_l(uint32_t other_mode_l)
{
    // About 66% savings, but relatively low in count (aside from goddard)
    if (LIKELY(rdp.other_mode_l != other_mode_l)) {
        rdp.other_mode_l  = other_mode_l;
        MODE_SWAP_COUNT_DO(om_l_sets++);
        
        const bool z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
        if (z_upd != rendering_state.depth_mask) {
            gfx_flush(21);
            gfx_rapi_set_depth_mask(z_upd);
            rendering_state.depth_mask = z_upd;
        }

        const bool zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
        if (zmode_decal != rendering_state.decal_mode) {
            gfx_flush(22);
            gfx_rapi_set_zmode_decal(zmode_decal);
            rendering_state.decal_mode = zmode_decal;
        }

        shader_state.use_fog      = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
        shader_state.texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) ? 1 : 0;
        shader_state.use_alpha    = shader_state.texture_edge || ((rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0);
        shader_state.use_noise    = (rdp.other_mode_l & G_AC_DITHER) == G_AC_DITHER;

        calculate_cc_id();
        
        if (shader_state.use_alpha != rendering_state.alpha_blend) {
            gfx_flush(23);
            gfx_rapi_set_use_alpha(shader_state.use_alpha);
            rendering_state.alpha_blend = shader_state.use_alpha;
        }
    }
    else
        MODE_SWAP_COUNT_DO(om_l_skips++);
}

static void gfx_sp_set_other_mode(uint32_t shift, uint32_t num_bits, uint64_t mode) {
    uint64_t mask = (((uint64_t)1 << num_bits) - 1) << shift;
    uint64_t om = rdp.other_mode_l | ((uint64_t)rdp.other_mode_h << 32);
    om = (om & ~mask) | mode;
    set_other_mode_h((uint32_t)(om >> 32));
    set_other_mode_l((uint32_t)om);
}

static inline void *seg_addr(uintptr_t w1) {
    return (void *) w1;
}

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))

static void gfx_run_dl(Gfx* cmd) {
    static struct LoadedVertex *tri_batch[MAX_VERTICES * 3];
    static uint32_t num_verts_batched = 0;

    for (;;) {
        uint32_t opcode = cmd->words.w0 >> 24;
        PC_METRIC_DO(num_rsp_commands_run++);

        if (opcode != G_TRI1 && opcode != G_TRI2 && num_verts_batched) {
            gfx_sp_tri_batched(tri_batch, num_verts_batched / 3);
            num_verts_batched = 0;
        }

        switch (opcode) {
            // RSP commands:
            case G_MTX:
#ifdef F3DEX_GBI_2
                gfx_sp_matrix(C0(0, 8) ^ G_MTX_PUSH, (const int32_t *) seg_addr(cmd->words.w1));
#else
                gfx_sp_matrix(C0(16, 8), (const int32_t *) seg_addr(cmd->words.w1));
#endif
                break;
            case (uint8_t)G_POPMTX:
#ifdef F3DEX_GBI_2
                gfx_sp_pop_matrix(cmd->words.w1 / 64);
#else
                gfx_sp_pop_matrix(1);
#endif
                break;
            case G_MOVEMEM:
#ifdef F3DEX_GBI_2
                gfx_sp_movemem(C0(0, 8), C0(8, 8) * 8, seg_addr(cmd->words.w1));
#else
                gfx_sp_movemem(C0(16, 8), 0, seg_addr(cmd->words.w1));
#endif
                break;
            case (uint8_t)G_MOVEWORD:
#ifdef F3DEX_GBI_2
                gfx_sp_moveword(C0(16, 8), C0(0, 16), cmd->words.w1);
#else
                gfx_sp_moveword(C0(0, 8), C0(8, 16), cmd->words.w1);
#endif
                break;
            case (uint8_t)G_TEXTURE:
#ifdef F3DEX_GBI_2
                gfx_sp_texture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(1, 7));
#else
                gfx_sp_texture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(0, 8));
#endif
                break;
            case G_VTX:
#ifdef F3DEX_GBI_2
                gfx_sp_vertex(C0(12, 8), C0(1, 7) - C0(12, 8), seg_addr(cmd->words.w1));
#elif defined(F3DEX_GBI) || defined(F3DLP_GBI)
                gfx_sp_vertex(C0(10, 6), C0(16, 8) / 2, seg_addr(cmd->words.w1));
#else
                gfx_sp_vertex((C0(0, 16)) / sizeof(Vtx), C0(16, 4), seg_addr(cmd->words.w1));
#endif
                break;
            case G_DL:
                if (C0(16, 1) == 0) {
                    // Push return address
                    gfx_run_dl((Gfx *)seg_addr(cmd->words.w1));
                } else {
                    cmd = (Gfx *)seg_addr(cmd->words.w1);
                    --cmd; // increase after break
                }
                break;
            case (uint8_t)G_ENDDL:
                return;
#ifdef F3DEX_GBI_2
            case G_GEOMETRYMODE:
                gfx_sp_geometry_mode(~C0(0, 24), cmd->words.w1);
                break;
#else
            case (uint8_t)G_SETGEOMETRYMODE:
                gfx_sp_geometry_mode(0, cmd->words.w1);
                break;
            case (uint8_t)G_CLEARGEOMETRYMODE:
                gfx_sp_geometry_mode(cmd->words.w1, 0);
                break;
#endif
            case (uint8_t)G_TRI1: {
#ifdef F3DEX_GBI_2
                const uint8_t i1 = C0(16, 8) / 2,
                              i2 = C0(8, 8)  / 2,
                              i3 = C0(0, 8)  / 2;
                // gfx_sp_tri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2);
#elif defined(F3DEX_GBI) || defined(F3DLP_GBI)
                const uint8_t i1 = C1(16, 8) / 2,
                              i2 = C1(8, 8)  / 2,
                              i3 = C1(0, 8)  / 2;
                // gfx_sp_tri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2);
#else
                const uint8_t i1 = C1(16, 8) / 10,
                              i2 = C1(8, 8)  / 10,
                              i3 = C1(0, 8)  / 10;
                // gfx_sp_tri1(C1(16, 8) / 10, C1(8, 8) / 10, C1(0, 8) / 10);
#endif
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i1];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i2];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i3];
                break;
            }
#if defined(F3DEX_GBI) || defined(F3DLP_GBI)
            case (uint8_t)G_TRI2: {
                const uint8_t i1 = C0(16, 8) / 2,
                              i2 = C0(8, 8)  / 2,
                              i3 = C0(0, 8)  / 2,
                              i4 = C1(16, 8) / 2,
                              i5 = C1(8, 8)  / 2,
                              i6 = C1(0, 8)  / 2;
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i1];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i2];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i3];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i4];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i5];
                tri_batch[num_verts_batched++] = &rsp.loaded_vertices[i6];

                // gfx_sp_tri2(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2,
                //             C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2);
                break;
            }
#endif
            case (uint8_t)G_SETOTHERMODE_L:
#ifdef F3DEX_GBI_2
                gfx_sp_set_other_mode(31 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, cmd->words.w1);
#else
                gfx_sp_set_other_mode(C0(8, 8), C0(0, 8), cmd->words.w1);
#endif
                break;
            case (uint8_t)G_SETOTHERMODE_H:
#ifdef F3DEX_GBI_2
                gfx_sp_set_other_mode(63 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, (uint64_t) cmd->words.w1 << 32);
#else
                gfx_sp_set_other_mode(C0(8, 8) + 32, C0(0, 8), (uint64_t) cmd->words.w1 << 32);
#endif
                break;

            // RDP Commands:
            case G_SETTIMG:
                gfx_dp_set_texture_image(C0(21, 3), C0(19, 2), C0(0, 10), seg_addr(cmd->words.w1));
                break;
            case G_LOADBLOCK:
                gfx_dp_load_block(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;
            case G_LOADTILE:
                gfx_dp_load_tile(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;
            case G_SETTILE:
                gfx_dp_set_tile(C0(21, 3), C0(19, 2), C0(9, 9), C0(0, 9), C1(24, 3), C1(20, 4), C1(18, 2), C1(14, 4), C1(10, 4), C1(8, 2), C1(4, 4), C1(0, 4));
                break;
            case G_SETTILESIZE:
                gfx_dp_set_tile_size(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;
            case G_LOADTLUT:
                gfx_dp_load_tlut(C1(24, 3), C1(14, 10));
                break;
            case G_SETENVCOLOR:
                gfx_dp_set_env_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;
            case G_SETPRIMCOLOR:
                gfx_dp_set_prim_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;
            case G_SETFOGCOLOR:
                gfx_dp_set_fog_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;
            case G_SETFILLCOLOR:
                gfx_dp_set_fill_color(cmd->words.w1);
                break;
            case G_SETCOMBINE:
                gfx_dp_set_combine_mode(COMBINE_MODE(
                    color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)),
                    color_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3))));
                    /*color_comb(C0(5, 4), C1(24, 4), C0(0, 5), C1(6, 3)),
                    color_comb(C1(21, 3), C1(3, 3), C1(18, 3), C1(0, 3)));*/
                break;
            // G_SETPRIMCOLOR, G_CCMUX_PRIMITIVE, G_ACMUX_PRIMITIVE, is used by Goddard
            // G_CCMUX_TEXEL1, LOD_FRACTION is used in Bowser room 1
            case G_TEXRECT:
            case G_TEXRECTFLIP:
            {
                int32_t lrx, lry, tile, ulx, uly;
                uint32_t uls, ult, dsdx, dtdy;
#ifdef F3DEX_GBI_2E
                lrx = (int32_t)(C0(0, 24) << 8) >> 8;
                lry = (int32_t)(C1(0, 24) << 8) >> 8;
                ++cmd;
                ulx = (int32_t)(C0(0, 24) << 8) >> 8;
                uly = (int32_t)(C1(0, 24) << 8) >> 8;
                ++cmd;
                uls = C0(16, 16);
                ult = C0(0, 16);
                dsdx = C1(16, 16);
                dtdy = C1(0, 16);
#else
                lrx = C0(12, 12);
                lry = C0(0, 12);
                tile = C1(24, 3);
                ulx = C1(12, 12);
                uly = C1(0, 12);
                ++cmd;
                uls = C1(16, 16);
                ult = C1(0, 16);
                ++cmd;
                dsdx = C1(16, 16);
                dtdy = C1(0, 16);
#endif
                gfx_dp_texture_rectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == G_TEXRECTFLIP);
                break;
            }
            case G_FILLRECT:
#ifdef F3DEX_GBI_2E
            {
                int32_t lrx, lry, ulx, uly;
                lrx = (int32_t)(C0(0, 24) << 8) >> 8;
                lry = (int32_t)(C1(0, 24) << 8) >> 8;
                ++cmd;
                ulx = (int32_t)(C0(0, 24) << 8) >> 8;
                uly = (int32_t)(C1(0, 24) << 8) >> 8;
                gfx_dp_fill_rectangle(ulx, uly, lrx, lry);
                break;
            }
#else
                gfx_dp_fill_rectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
                break;
#endif
            case G_SETSCISSOR:
                gfx_dp_set_scissor(C1(24, 2), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;
            case G_SETZIMG:
                gfx_dp_set_z_image(seg_addr(cmd->words.w1));
                break;
            case G_SETCIMG:
                gfx_dp_set_color_image(C0(21, 3), C0(19, 2), C0(0, 11), seg_addr(cmd->words.w1));
                break;

            // Stereoscopic 3D commands (N3DS only)
            case G_SPECIAL_1:
                gfx_set_2d(cmd->words.w1);
                break;
            case G_SPECIAL_2:
                gfx_set_iod(cmd->words.w1);
                break;
        }
        ++cmd;
    }
}

static void gfx_sp_reset() {
    rsp.modelview_matrix_stack_size = 1;
    rsp.current_num_lights = 2;
    rsp.lights_changed = true;
}

void gfx_init(struct GfxWindowManagerAPI *wapi, const char *game_name, bool start_in_fullscreen) {
    gfx_wapi = wapi;
    gfx_wapi->init(game_name, start_in_fullscreen);
    gfx_rapi_init();

    // dimensions won't change on 3DS, so just do this once
    gfx_wapi->get_dimensions(&gfx_current_dimensions.width, &gfx_current_dimensions.height);
    if (gfx_current_dimensions.height == 0) {
        // Avoid division by zero
        gfx_current_dimensions.height = 1;
    }
    gfx_current_dimensions.aspect_ratio = (float)gfx_current_dimensions.width / (float)gfx_current_dimensions.height;
    gfx_current_dimensions.aspect_ratio_factor = (4.0f / 3.0f) * (1.0f / gfx_current_dimensions.aspect_ratio);

    shader_state_init(&shader_state);

    // Screen-space rect Z will always be -1.
    rsp.rect_vertices[0].position.z =
    rsp.rect_vertices[1].position.z =
    rsp.rect_vertices[2].position.z =
    rsp.rect_vertices[3].position.z = -1;

    // Initialize the matstack to identity
    for (int i = 0; i < MAT_STACK_SIZE; i++)
        memcpy(rsp.modelview_matrix_stack[i], MTX_IDENTITY, sizeof(MTX_IDENTITY));
    
    memcpy(rsp.P_matrix, MTX_IDENTITY, sizeof(MTX_IDENTITY));

    // Initialize constant matrices and set normal MTX mode
    gfx_rapi_select_matrix_set(MATRIX_SET_IDENTITY);
    gfx_rapi_set_model_view_matrix(MTX_IDENTITY);
    gfx_rapi_set_projection_matrix(MTX_IDENTITY);
    
    gfx_rapi_select_matrix_set(MATRIX_SET_SCALED_NDC);
    gfx_rapi_set_model_view_matrix(MTX_NDC_DOWNSCALE);
    gfx_rapi_set_projection_matrix(MTX_IDENTITY);

    gfx_rapi_select_matrix_set(MATRIX_SET_NORMAL);
    rsp.matrix_set = MATRIX_SET_NORMAL;
}

void gfx_start_frame(void) {
    gfx_wapi->handle_events();
}

void gfx_run(Gfx *commands) {
    gfx_sp_reset();

    if (!gfx_wapi->start_frame()) {
        dropped_frame = true;
        return;
    }
    dropped_frame = false;

    profiler_3ds_log_time(0);
    gfx_rapi_start_frame();
    profiler_3ds_log_time(4); // GFX Rendering API Start Frame (VSync)
    gfx_rapi_set_backface_culling_mode(rsp.geometry_mode & G_CULL_BOTH);
    last_mv_mtx_addr = last_p_mtx_addr = NULL;

    non_granular_log_time(0);
    gfx_run_dl(commands);
    non_granular_log_time(5); // GFX Run DL

    gfx_flush(24);
    gfx_rapi_end_frame();
    gfx_wapi->swap_buffers_begin();

#if ENABLE_SHADER_SWAP_COUNTER == 1
    printf("Swaps %d  RC %d  CM %d\n", num_shader_swaps, avoided_swaps_recalc, avoided_swaps_combine_mode);
    num_shader_swaps = avoided_swaps_recalc = avoided_swaps_combine_mode = 0;
#endif

#if ENABLE_OTHER_MODE_SWAP_COUNTER == 1
    printf("OMH %d SK %d  OML %d SK %d\n", om_h_sets, om_h_skips, om_l_sets, om_l_skips);
    om_h_sets = om_l_sets = om_h_skips = om_l_skips = 0;
#endif

    // GDB print commands:
    // set print repeats 0
    // set print elements 10000
    // set logging on
    GRANULAR_FLUSH_DO(bzero(flushes, sizeof(flushes)));
    GRANULAR_FLUSH_DO(bzero(flush_avoids, sizeof(flush_avoids)));
    GRANULAR_FLUSH_DO(bzero(tris_per_flush, sizeof(tris_per_flush)));
}

void gfx_end_frame(void) {
    if (!dropped_frame) {
        gfx_rapi_finish_render();
        gfx_wapi->swap_buffers_end();
    }
}
