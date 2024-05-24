#ifndef GFX_CITRO3D_HELPERS_H
#define GFX_CITRO3D_HELPERS_H

/*
 * A file for basic helper functions. These should be generally applicable to C3D,
 * and should be pure functions (i.e. they should not use external state).
 */

#include <stdbool.h>
#include <stdint.h>
#include <3ds/gpu/enums.h>

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
#include "gfx_3ds_shaders.h"
#include "color_formats.h"

// Calculates a GFX_Citro3D shader code based on the provided RSP flags
uint8_t gfx_citro3d_calculate_shader_code(bool has_texture, UNUSED bool has_fog, bool has_alpha, bool has_color1, bool has_color2);

// Looks up an n3ds_shader_info struct from the given GFX_Citro3D shader code
const struct n3ds_shader_info* get_shader_info_from_shader_code(uint8_t shader_code);

// Pads a texture from w * h to new_w * new_h by simply repeating data.
void gfx_citro3d_pad_texture_rgba32(union RGBA32* src, union RGBA32* dest, uint32_t src_w, uint32_t src_h, uint32_t new_w, uint32_t new_h);

// LUT: Returns a GPU_TEVSRC based on which color combiner input is provided.
GPU_TEVSRC gfx_citro3d_cc_input_to_tev_src(int cc_input, bool swap_input);

// LUT: Converts an RSP texture clamp mode to its C3D counterpart.
GPU_TEXTURE_WRAP_PARAM gfx_citro3d_convert_texture_clamp_mode(uint32_t val);

// LUT: Converts an RSP backface culling mode to its C3D counterpart.
GPU_CULLMODE gfx_citro3d_convert_cull_mode(uint32_t culling_mode);

// Converts an RSP matrix to a C3D matrix, which has the elements reversed within each row.
void gfx_citro3d_convert_mtx(float sm64_mtx[4][4], C3D_Mtx* c3d_mtx);

#endif