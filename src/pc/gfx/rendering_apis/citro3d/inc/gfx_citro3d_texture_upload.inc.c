/*
 * A .C inclusion file containing texture uploading code.
 * This just simplifies gfx_citro3d_emulator.c a bit.
 */

static ALIGNED(32) union RGBA32 tex_conversion_buffer[16 * 1024]; // For converting textures between formats
static ALIGNED(32) union RGBA32 tex_scaling_buffer[16 * 1024];    // For padding and tiling textures

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
        internal_citro3d_upload_texture_common((type_*) tex_scaling_buffer, input_size, output_size, format);                                    \
    }

COLD static void internal_citro3d_upload_textures_to_vram()
{
    bool failed = false;

    for (size_t i = 0; i < num_textures_to_upload_to_vram; i++)
    {
        TexHandle* tex = texture_upload_queue[i];
        C3D_Tex temp;

        if(C3D_TexInitVRAM(&temp, tex->c3d_tex.width, tex->c3d_tex.height, tex->c3d_tex.fmt))
        {
            tex->addr_vram = tex->c3d_tex.data = temp.data;
        }
        else
        {
            failed = true;
            break;
        }
    }

    // Deferred to avoid transferring to textures that will be deallocated.
    if (!failed)
    {
        for (size_t i = 0; i < num_textures_to_upload_to_vram; i++)
        {
            TexHandle* tex = texture_upload_queue[i];
            tex->load_status = TEX_VRAM;
            C3D_TexUpload(&tex->c3d_tex, tex->addr_fcram);
            C3D_TexFlush(&tex->c3d_tex);
        }
    }
    
    // If any failed, assume that VRAM is full and clear it
    else
    {
        printf("Failed to upload a texture to VRAM.\n");
        for (size_t i = 0; i < api_texture_index; i++)
        {
            TexHandle* tex = &texture_pool[i];

            switch (tex->load_status)
            {
                case TEX_VRAM:
                    C3D_TexDelete(&tex->c3d_tex);        // Free the VRAM data
                    tex->c3d_tex.data = tex->addr_fcram; // Swap back to FCRAM
                case TEX_ENQUEUED:
                    tex->load_status = TEX_FCRAM;        // Reset to FCRAM mode
                default:
                    break;
            }
        }
    }

    num_textures_to_upload_to_vram = 0;
}

COLD static void internal_citro3d_upload_texture_common(void* data, struct TextureSize input_size, struct TextureSize output_size, GPU_TEXCOLOR format)
{
    TexHandle* handle = ctx.current_texture;
    C3D_Tex* tex = &handle->c3d_tex;

    ctx.current_texture->scale.s =   input_size.width  / (float) output_size.width;
    ctx.current_texture->scale.t = -(input_size.height / (float) output_size.height);

    switch (handle->load_status)
    {
        case TEX_VRAM:
            C3D_TexDelete(tex);
            tex->data = handle->addr_fcram; // Delete VRAM texture and swap back to FCRAM
        case TEX_ENQUEUED:
        case TEX_FCRAM:
            C3D_TexDelete(tex);             // Delete FCRAM texture because the size might be different
        case TEX_UNINITIALIZED:
            break;
    }

    if (C3D_TexInit(tex, output_size.width, output_size.height, format)) {
        C3D_TexUpload(tex, data);
        C3D_TexFlush(tex);
        handle->addr_fcram = tex->data;
        handle->load_status = TEX_FCRAM;
    } else {
        printf("Tex init failed! Size: %d, %d\n", (int) output_size.width, (int) output_size.height);
        handle->load_status = TEX_UNINITIALIZED;
    }

    CTX_NOTIFY(CTX_CURRENT_TEXTURE);
}

// --------------- API functions ---------------

COLD void gfx_rapi_upload_texture_rgba16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_RGBA5551)
}

COLD void gfx_rapi_upload_texture_rgba32(const uint8_t* data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint32_t, citro3d_helpers_pad_and_tile_texture_u32, GPU_RGBA8)
}

/*
* The GPU doesn't support an IA4 format, so we need to convert.
* IA16 is the next format with decent accuracy (3-bit to 8-bit intensity).
* We could use IA8 (4-bit intensity), but this would cause a fairly large error.
*/
COLD void gfx_rapi_upload_texture_ia4(const uint8_t *data, int width, int height)
{
    convert_ia4_to_ia16((union IA16*) tex_conversion_buffer, data, width, height);
    gfx_rapi_upload_texture_ia16((const uint8_t*) tex_conversion_buffer, width, height);
}

COLD void gfx_rapi_upload_texture_ia8(const uint8_t *data, int width, int height) 
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_LA4)
}

COLD void gfx_rapi_upload_texture_ia16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_LA8)
}

// Untested because it's unused in SM64. This will also probably crash with VRAM textures enabled;
// it seems that L4 is extremely buggy when read from VRAM.
COLD void gfx_rapi_upload_texture_i4(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L4)
}

// Untested because it's unused in SM64
COLD void gfx_rapi_upload_texture_i8(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L8)
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
COLD void gfx_rapi_upload_texture_ci4(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci4_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
COLD void gfx_rapi_upload_texture_ci8(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci8_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}
