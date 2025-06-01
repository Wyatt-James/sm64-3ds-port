#pragma once

/*
 * Various internal-use types for the Citro3D rendering API.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/n3ds/c3d_inc.h"

#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"

// Represents a vertex buffer.
// Any number of shaders can use a buffer, but their attribute count and stride must be identical.
typedef struct
{
    const struct n3ds_shader_vbo_info* vbo_info;
    C3D_BufInfo buf_info;
    C3D_AttrInfo attr_info; // Used to avoid duplicates.
    float *ptr;
    size_t num_verts;
} VertexBuffer;

/*
 * Represents a 3DS shader program.
 * Can use any video buffer with the correct stride and attribute count.
 */
typedef struct
{
    shaderProgram_s pica_shader_program; // pica shader program
    VertexBuffer* vertex_buffer;
} ShaderProgram;

// Represents a loaded Emu64 Shader Program
typedef struct
{
    Emu64ProgramFeatureFlags shader_features;
    ShaderProgram prog;
} Emu64ShaderProgram;

typedef struct
{
    float c1_rgb,
          c1_a,
          c2_rgb,
          c2_a;
} ShaderInputMapping;

typedef struct
{
    bool use_env_color;
    ColorCombinerId cc_id;
    ShaderInputMapping c3d_shader_input_mapping; // Sent to GPU
    C3D_TexEnv texenv; // Sent to GPU
    uint32_t cc_mapping_identifier; // Used to improve performance
    struct CCFeatures cc_features;
} ColorCombiner;

struct ScissorConfig
{
    int x1, y1, x2, y2;
    bool enable;
};

struct ViewportConfig
{
    int x, y, width, height;
};

struct IodConfig
{
    float z, w;
};

struct TextureSize
{
    uint16_t width, height;
    bool success;
};

struct VertexLoadConfig
{
    bool enable_lighting, enable_texgen;
    uint8_t num_lights;
    uint32_t texture_scale_s, texture_scale_t;
};

struct TextureSettings
{
    float uv_offset;
    int16_t uls, ult;
    int16_t width, height;
};

// See f32x2_note.txt
union f32x2
{
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

typedef enum
{
    TEX_UNINITIALIZED,  // Not yet uploaded, or upload failed
    TEX_FCRAM,          // Uploaded to FCRAM only
    TEX_ENQUEUED,       // Uploaded to FCRAM, queued for VRAM
    TEX_VRAM,           // Uploaded to both FCRAM and VRAM
} TexLoadStatus;

typedef struct
{
    C3D_Tex c3d_tex;
    union f32x2 scale;
    void* addr_fcram;
    void* addr_vram;
    TexLoadStatus load_status;
} TexHandle;
