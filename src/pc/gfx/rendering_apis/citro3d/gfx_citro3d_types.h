#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/gpu/shaderProgram.h>
#include <citro3d.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/multi_viewport/multi_viewport.h"

// See f32x2_note.txt
union f32x2 {
    struct {
        float f32_upper;
        float f32_lower;
    };
    struct {
        float s;
        float t;
    };
    uint64_t u64;
    int64_t s64;
    double f64;
};

union ShaderProgramFeatureFlags {
    struct {
        bool position, tex, color, normals;
    };

    uint32_t u32;
};

// Represents a vertex buffer.
// Any number of shaders can use a buffer, but their attribute count and stride must be identical.
struct VertexBuffer {
    const struct n3ds_shader_vbo_info* vbo_info;
    C3D_BufInfo buf_info;
    C3D_AttrInfo* attr_info; // Only used to avoid duplicates.
    float* ptr;
    size_t num_verts;
};

/*
 * Represents a 3DS shader program.
 * Can use any video buffer with the correct stride and attribute count.
 */ 
struct ShaderProgram {
    union ShaderProgramFeatureFlags shader_features;
    shaderProgram_s pica_shader_program; // pica shader program
    struct VertexBuffer* vertex_buffer;
    C3D_AttrInfo attr_info;              // Describes VBO structure
};

// Stored as float to send to GPU
struct ShaderInputMapping {
    float c1_rgb,
          c1_a,
          c2_rgb,
          c2_a;
};

struct ColorCombiner {
    bool use_env_color;
    ColorCombinerId cc_id;
    struct ShaderInputMapping c3d_shader_input_mapping; // Sent to GPU
    C3D_TexEnv texenv_slot_0; // Sent to GPU
    uint32_t cc_mapping_identifier; // Used to improve performance
    struct CCFeatures cc_features;
};

struct TexHandle {
    C3D_Tex c3d_tex;
    union f32x2 scale;
};

struct TextureSize {
    uint16_t width, height;
    bool success;
};

struct ScreenClearConfig {
    enum ViewportClearBuffer bufs;
    union RGBA32 color;
    uint32_t depth;
};

union ScreenClearConfigsN3ds {
    struct {
        struct ScreenClearConfig top;
        struct ScreenClearConfig bottom;
    };
    struct ScreenClearConfig array[2];
};

struct GameMtxSet {
    C3D_Mtx model_view, transposed_model_view, game_projection;
};

struct OptimizationFlags {
    bool consecutive_fog;
    bool consecutive_stereo_p_mtx;
    bool alpha_test;
    bool gpu_textures;
    bool consecutive_framebuf;
    bool viewport_and_scissor;
    bool texture_settings_1;
    bool texture_settings_2;
    bool change_shader_on_cc_change;
    bool consecutive_vertex_load_flags;
};

struct RenderState {
    uint32_t flags;

    bool fog_enabled;
    bool alpha_test;

    C3D_FogLut* fog_lut;
    C3D_RenderTarget *cur_target;
    float uv_offset;             // Depends on linear filter.
    union f32x2 texture_scale;   // Varies per-texture.
    Gfx3DSMode current_gfx_mode;
    float prev_slider_level;
    struct ShaderProgram* shader_program;
};

struct VertexLoadFlags {
    bool enable_lighting;
    uint8_t num_lights;
    bool enable_texgen;
    uint32_t texture_scale_s, texture_scale_t;
};

struct ScissorConfig {
    int x1, y1, x2, y2;
    bool enable;
};

struct ViewportConfig {
    int x, y, width, height;
};

struct IodConfig {
    float z, w;
};
