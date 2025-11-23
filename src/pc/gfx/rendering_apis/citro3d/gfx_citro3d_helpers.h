#pragma once

/*
 * A file for basic helper functions. These should be generally applicable to C3D,
 * and should not alter rendering state.
 */

#include <stdbool.h>
#include <stdint.h>

#include "src/pc/n3ds/c3d_inc.h"

#include "gfx_citro3d_internal_types.h"

#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/gfx_3ds_constants.h"
#include "src/pc/gfx/gfx_3ds_shaders.h"

// A static definition of a C3D Identity Matrix
#define C3D_STATIC_IDENTITY_MTX {\
        .r = {\
            {.x = 1.0f},\
            {.y = 1.0f},\
            {.z = 1.0f},\
            {.w = 1.0f}\
        }\
    }

// Adjusts a texture's dimensions to fit within the 3DS' limitations (8 pixels minimum, power-of-2 for each dimension)
struct TextureSize citro3d_helpers_adjust_texture_dimensions(struct TextureSize input_size, size_t unit_size, size_t buffer_size);

// Pads a texture with u32 units from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u32(uint32_t* src, uint32_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// Pads a texture with u16 units from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u16(uint16_t* src, uint16_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// Pads a texture with u8 units from w * h to new_w * new_h by repeating data while converting it to the 3DS' tiling layout. Handles endianness.
void citro3d_helpers_pad_and_tile_texture_u8(uint8_t* src, uint8_t* dest, struct TextureSize src_size, struct TextureSize new_size);

// LUT: Returns a GPU_TEVSRC based on which color combiner input is provided.
GPU_TEVSRC citro3d_helpers_cc_input_to_tev_src(int cc_input, bool swap_input);

// Configures a C3D_TexEnv for slot 0.
C3D_TexEnv citro3d_helpers_configure_tex_env(struct CCFeatures* cc_features);

// Configures a C3D_TexEnv for slot 1. The result is always identical!
C3D_TexEnv citro3d_helpers_configure_two_color_tex_env(void);

// LUT: Converts an RSP texture clamp mode to its C3D counterpart.
GPU_TEXTURE_WRAP_PARAM citro3d_helpers_convert_texture_clamp_mode(uint32_t val);

// LUT: Converts an RSP backface culling mode to its C3D counterpart.
GPU_CULLMODE citro3d_helpers_convert_cull_mode(uint32_t culling_mode);

// Converts an RSP matrix to a C3D matrix. RSP matrices are column-major, while C3D matrices are row-major with each row reversed left-right.
void citro3d_helpers_convert_mtx(C3D_Mtx* restrict c3d_mtx, float sm64_mtx[restrict 4][4]);

// Copies and transposes a C3D_Mtx in a single operation.
void citro3d_helpers_copy_and_transpose_mtx(C3D_Mtx* restrict dst, C3D_Mtx* restrict src);

// Applies a stereoscopic tilt to the given C3D_Mtx.
void citro3d_helpers_mtx_stereo_tilt(C3D_Mtx* dst, C3D_Mtx* src, enum Stereoscopic3dMode mode_2d, float z, float w, float strength);

// Initializes a projection matrix transform.
void citro3d_helpers_apply_projection_mtx_preset(C3D_Mtx* mtx);

// Converts an RSP viewport config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_viewport_settings(ScreenDimensions* viewport_config, N3DS_DisplayMode gfx_mode, int x, int y, int width, int height);

// Converts an RSP viewport config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_scissor_settings(ScreenDimensions* scissor_config, N3DS_DisplayMode gfx_mode, int x, int y, int width, int height);

// Alters the scaling of the given ScreenDimensions, based on the given N3DS_DisplayModes.
void citro3d_helpers_rescale_screen_dimensions(ScreenDimensions* viewport_config, N3DS_DisplayMode old_gfx_mode, N3DS_DisplayMode new_gfx_mode);

// Converts an RSP IOD config to its GFX_Citro3D counterpart.
void citro3d_helpers_convert_iod_settings(struct IodConfig* iod_config, float z, float w);

// Converts an RSP 2D mode to its GFX_Citro3D counterpart.
enum Stereoscopic3dMode citro3d_helpers_convert_2d_mode(int mode_2d);

// Converts an RSP fog mode to its GFX_Citro3D counterpart.
GPU_FOGMODE citro3d_helpers_convert_fog_mode(bool enable);

// Converts a Color Combiner source to its Emu64 version.
// Important: Only pass TRUE for fog_enabled when converting mappings for the alpha channel!
enum Emu64ColorCombinerSource citro3d_helpers_convert_cc_mapping_to_emu64(uint8_t cc_mapping, bool fog_enabled);

// Converts a Color Combiner source to its Emu64 version, pre-cast to a float.
// Important: Only pass TRUE for fog_enabled when converting mappings for the alpha channel!
float citro3d_helpers_convert_cc_mapping_to_emu64_float(uint8_t cc_mapping, bool fog_enabled);

// Initializes a C3D_AttrInfo from the given attribute data.
C3D_AttrInfo citro3d_helpers_init_attr_info(const struct n3ds_attribute_data* attributes);

// Initializes a ShaderProgram. If it fails, vb_ptr will be null. To skip allocating the buffer, set vb to null.
ShaderProgram citro3d_helpers_init_shader(const struct n3ds_shader_info* shader_info, VertexBuffer* vb, size_t vbo_size);

// Frees a ShaderProgram.
void citro3d_helpers_free_shader(ShaderProgram* prog);

// Loads a T3X texture into the given C3D_Tex.
bool citro3d_helpers_load_t3x_texture(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size);

// Initializes a color combinr.
void citro3d_helpers_init_cc(ColorCombiner* cc, ColorCombinerId cc_id);
