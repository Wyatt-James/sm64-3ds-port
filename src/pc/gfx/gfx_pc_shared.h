#pragma once

#include <stdint.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/gfx_3ds_constants.h"

// Supported Texture formats (see upload_texture_to_rendering_api)
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

#define MATRIX_SET_NORMAL 0
#define MATRIX_SET_IDENTITY 1
#define MATRIX_SET_SCALED_NDC 2
#define MATRIX_SET_INVALID (~MATRIX_SET_NORMAL)
#define NDC_SCALE (INT16_MAX / 4) // Scaling factor for s16 NDC coordinates. See gfx_dimensions.h for more info.

#define MAX_LIGHTS 2
#define MAX_VERTICES 64
#define MAT_STACK_SIZE 11

#define STATIC_LIGHT_DEFAULT { \
    .col = {0, 0, 0},          \
    .dir = {1, 1, 1}           \
}

#define STATIC_IDENTITY_MTX {{1.0f, 0.0f, 0.0f, 0.0f},  \
                             {0.0f, 1.0f, 0.0f, 0.0f},  \
                             {0.0f, 0.0f, 1.0f, 0.0f},  \
                             {0.0f, 0.0f, 0.0f, 1.0f}}
                             
typedef uint32_t CombineMode; // To be used with the COMBINE_MODE macro.

union boolx2 {
    bool bools[2];
    int16_t either;
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

// Total size: 16 bytes
struct LoadedVertex {
    union int16x4 position; // 8 bytes (w is unused, garbage value)
    union int16x2 uv;       // 4 bytes
    union RGBA32 color;     // 4 bytes. Also contains normals.
};

// These parameters are stored as u0.16s in the DList,
// but we use u16.16 here for a hack (see gfx_dp_texture_rectangle)
union TextureScalingFactor {
    struct {
        // U16.16
        uint32_t s, t;
    };
    uint64_t u64;
};

union XYWidthHeight {
    struct {
        uint16_t x, y, width, height;
    };
    uint64_t u64;
};

struct RSP {
    uint32_t matrix_set;
    float modelview_matrix_stack[MAT_STACK_SIZE][4][4];
    uint8_t modelview_matrix_stack_size;
    float P_matrix[4][4];

    Light_t* current_lights[MAX_LIGHTS + 1]; // MUST be populated with valid pointers during init! Use LIGHT_DEFAULT.
    uint8_t lights_changed_bitfield;
    uint8_t current_num_lights; // includes ambient light

    uint32_t geometry_mode;

    union TextureScalingFactor texture_scaling_factor;

    struct LoadedVertex* loaded_vertices[MAX_VERTICES];
    struct LoadedVertex rect_vertices[4]; // Used only for rectangle drawing
};

struct RDP {
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
    CombineMode combine_mode;

    union RGBA32 env_color, prim_color, fill_color;
    union XYWidthHeight viewport;
    void *z_buf_address;
    void *color_image_address;
};

struct ShaderState {
    ColorCombinerId cc_id;
    bool use_alpha;
    bool use_fog;
    bool texture_edge;
    bool use_noise;
    uint8_t num_inputs;
    union boolx2 used_textures;
};

struct RenderingState {
    uint32_t fog_settings;
    uint32_t matrix_set;
    bool p_mtx_changed, mv_mtx_changed;
    enum Stereoscopic3dMode stereo_3d_mode;
    enum IodMode iod_mode;
    uint8_t current_num_lights;
    bool enable_lighting;
    bool enable_texgen;
    union TextureScalingFactor texture_scaling_factor; // Why is this slow as hell when in RenderingState instead of ShaderState? 7.01 vs 7.18ms in castle courtyard
    
    ColorCombinerId cc_id;
    union RGBA32 prim_color;
    union RGBA32 env_color;
    bool linear_filter;
    union int16x4 texture_settings;
    uint32_t culling_mode;
    bool depth_test;
    union XYWidthHeight viewport, scissor;

    bool depth_mask;
    bool decal_mode;
    bool alpha_blend;
    struct TextureHashmapNode* textures[2];
    const float *last_mv_mtx_addr, *last_p_mtx_addr;
};
