#include <stdint.h>

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
#include <c3d/texture.h>
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

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_shared.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_helpers.h"
#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/texture_conversion.h"

struct TexHandle texture_pool[TEXTURE_POOL_SIZE];
struct TexHandle* current_texture = &texture_pool[0];

static ALIGNED32 union RGBA32 tex_conversion_buffer[16 * 1024]; // For converting textures between formats
static ALIGNED32 union RGBA32 tex_scaling_buffer[16 * 1024];    // For padding and tiling textures
static uint32_t api_texture_index = 0;

// Template function to resize, swizzle, and upload a texture of given format.
#define UPLOAD_TEXTURE_TEMPLATE(type_, swizzle_func_name_, gpu_tex_format_) \
    type_* src = (type_*) data;                                                                                                 \
    GPU_TEXCOLOR format = gpu_tex_format_;                                                                                      \
    size_t unit_size = sizeof(src[0]);                                                                                          \
                                                                                                                                \
    struct TextureSize input_size = { .width = width, .height = height };                                                       \
    struct TextureSize output_size = citro3d_helpers_adjust_texture_dimensions(input_size, unit_size, sizeof(tex_scaling_buffer));  \
                                                                                                                                \
    if (output_size.success) {                                                                                                  \
        swizzle_func_name_(src, (type_*) tex_scaling_buffer, input_size, output_size);                                          \
        citro3d_cold_upload_texture_common((type_*) tex_scaling_buffer, input_size, output_size, format);                                    \
    }

static void citro3d_cold_upload_texture_common(void* data, struct TextureSize input_size, struct TextureSize output_size, GPU_TEXCOLOR format)
{
    C3D_Tex* tex = &current_texture->c3d_tex;

    current_texture->scale.s =   input_size.width  / (float) output_size.width;
    current_texture->scale.t = -(input_size.height / (float) output_size.height);

    if (C3D_TexInit(tex, output_size.width, output_size.height, format)) {
        C3D_TexUpload(tex, data);
        C3D_TexFlush(tex);
    } else
       printf("Tex init failed! Size: %d, %d\n", (int) output_size.width, (int) output_size.height);
}

uint32_t gfx_rapi_new_texture()
{
    if (api_texture_index == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return api_texture_index++;
}

void gfx_rapi_upload_texture_rgba16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_RGBA5551)
}

void gfx_rapi_upload_texture_rgba32(const uint8_t* data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint32_t, citro3d_helpers_pad_and_tile_texture_u32, GPU_RGBA8)
}

/*
* The GPU doesn't support an IA4 format, so we need to convert.
* IA16 is the next format with decent accuracy (3-bit to 8-bit intensity).
* We could use IA8 (4-bit intensity), but this would cause a fairly large error.
*/
void gfx_rapi_upload_texture_ia4(const uint8_t *data, int width, int height)
{
    convert_ia4_to_ia16((union IA16*) tex_conversion_buffer, data, width, height);
    gfx_rapi_upload_texture_ia16((const uint8_t*) tex_conversion_buffer, width, height);
}

void gfx_rapi_upload_texture_ia8(const uint8_t *data, int width, int height) 
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_LA4)
}

void gfx_rapi_upload_texture_ia16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_LA8)
}

// Untested because it's unused in SM64
void gfx_rapi_upload_texture_i4(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L4)
}

// Untested because it's unused in SM64
void gfx_rapi_upload_texture_i8(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L8)
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
void gfx_rapi_upload_texture_ci4(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci4_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
void gfx_rapi_upload_texture_ci8(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci8_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}
