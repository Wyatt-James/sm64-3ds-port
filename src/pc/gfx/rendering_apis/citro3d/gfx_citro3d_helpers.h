#ifndef GFX_CITRO3D_HELPERS_H
#define GFX_CITRO3D_HELPERS_H

/*
 * A file for basic helper functions. These should be generally applicable to C3D,
 * and should be pure functions (i.e. they should not use external state).
 */

#include <stdbool.h>
#include <stdint.h>
#include <3ds/gpu/enums.h> // Doesn't define or use any types, so can be outside of the guard.

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
#include <c3d/types.h>
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

#include "macros.h"
#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/gfx_3ds_constants.h"
#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/gfx_3ds_types.h"

// A static definition of a C3D Identity Matrix
#define C3D_STATIC_IDENTITY_MTX {\
        .r = {\
            {.x = 1.0f},\
            {.y = 1.0f},\
            {.z = 1.0f},\
            {.w = 1.0f}\
        }\
    }

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

struct TextureSize {
    uint16_t width, height;
    bool success;
};

// Constant matrices, set during initialization.
extern const C3D_Mtx IDENTITY_MTX, DEPTH_ADD_W_MTX;

// Calculates an Emu64 shader code based on the given feature flags
Emu64ShaderCode citro3d_helpers_calculate_shader_code(union ShaderProgramFeatureFlags feature_flags);

// Looks up an n3ds_shader_info struct from the given Emu64 shader code
const struct n3ds_shader_info* citro3d_helpers_get_shader_info(Emu64ShaderCode shader_code);

// Looks up an n3ds_shader_info struct from the given feature flags.
const struct n3ds_shader_info* citro3d_helpers_get_shader_info_from_flags(union ShaderProgramFeatureFlags feature_flags);

// Adjusts a texture's dimensions to fit within the 3DS' limitations (8 pixels minimum, power-of-2 for each dimension)
struct TextureSize citro3d_helpers_adjust_texture_dimensions(struct TextureSize input_size, size_t unit_size, size_t buffer_size);

// Pads a texture with u32 from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u32(uint32_t* src, uint32_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// Pads a texture with u16 units from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u16(uint16_t* src, uint16_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// Pads a texture with u8 units from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u8(uint8_t* src, uint8_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// LUT: Returns a GPU_TEVSRC based on which color combiner input is provided.
GPU_TEVSRC citro3d_helpers_cc_input_to_tev_src(int cc_input, bool swap_input);

// Configures a C3D_TexEnv for slot 0.
void citro3d_helpers_configure_tex_env_slot_0(struct CCFeatures* cc_features, C3D_TexEnv* texenv);

// Configures a C3D_TexEnv for slot 1. The result is always identical!
void citro3d_helpers_configure_tex_env_slot_1(C3D_TexEnv* texenv);

// LUT: Converts an RSP texture clamp mode to its C3D counterpart.
GPU_TEXTURE_WRAP_PARAM citro3d_helpers_convert_texture_clamp_mode(uint32_t val);

// LUT: Converts an RSP backface culling mode to its C3D counterpart.
GPU_CULLMODE citro3d_helpers_convert_cull_mode(uint32_t culling_mode);

// Converts an RSP matrix to a C3D matrix, which has the elements reversed within each row.
void citro3d_helpers_convert_mtx(float sm64_mtx[4][4], C3D_Mtx* c3d_mtx);

// Applies a stereoscopic tilt to the given C3D_Mtx.
void citro3d_helpers_mtx_stereo_tilt(C3D_Mtx* dst, C3D_Mtx* src, enum Stereoscopic3dMode mode_2d, float z, float w, float strength);

// Initializes a projection matrix transform.
void citro3d_helpers_apply_projection_mtx_preset(C3D_Mtx* mtx);

// Converts an RSP viewport config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_viewport_settings(struct ViewportConfig* viewport_config, Gfx3DSMode gfx_mode, int x, int y, int width, int height);

// Converts an RSP viewport config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_scissor_settings(struct ScissorConfig* scissor_config, Gfx3DSMode gfx_mode, int x, int y, int width, int height);

// Converts an RSP IOD config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_iod_settings(struct IodConfig* iod_config, float z, float w);

// Converts an RSP 2D mode to its GFX_Citro3D counterpart.
enum Stereoscopic3dMode citro3d_helpers_convert_2d_mode(int mode_2d);

// Sets a C3D float uniform from a vector of floats.
void citro3d_helpers_set_fv_unif_array(GPU_SHADER_TYPE type, int id, float vec[4]);

// Sets a C3D float uniform from an RGBA32 union. Scales by 1/255.
void citro3d_helpers_set_fv_unif_rgba32(GPU_SHADER_TYPE type, int id, union RGBA32 color);

// Converts a Color Combiner source to its Emu64 version.
// Important: Only pass TRUE for fog_enabled when converting mappings for the alpha channel!
enum Emu64ColorCombinerSource citro3d_helpers_convert_cc_mapping_to_emu64(uint8_t cc_mapping, bool fog_enabled);

// Converts a Color Combiner source to its Emu64 version, pre-cast to a float.
// Important: Only pass TRUE for fog_enabled when converting mappings for the alpha channel!
float citro3d_helpers_convert_cc_mapping_to_emu64_float(uint8_t cc_mapping, bool fog_enabled);

// Initializes a C3D_AttrInfo from the given attribute data.
void citro3d_helpers_init_attr_info(const struct n3ds_attribute_data* attributes, C3D_AttrInfo* out_attr_info);

#endif
