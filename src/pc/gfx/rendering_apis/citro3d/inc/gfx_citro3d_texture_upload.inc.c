/*
 * A .C inclusion file containing texture uploading code.
 * This just simplifies gfx_citro3d_emulator.c a bit.
 */

#define MAX_VRAM_TEX 256
#define VRAM_POOL_SIZE KIB_TO_BYTE(MAX_VRAM_TEX * 2) // 2KiB per-texture seems like a good match for vanilla
#define VRAM_TEX_ALIGNMENT_BYTES 8

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

typedef struct
{
    void* vram_pool_base;                    // Base pointer as returned by vramAlloc
    void* vram_pool_offset;                  // Current position within the data pool
    size_t num_vram_tex;                     // Number of currently allocated VRAM textures
    size_t data_size;                        // Total number of bytes allocated to textures, including padding
    TexHandle* vram_textures[MAX_VRAM_TEX];  // Array of TexHandles present in VRAM, used to optimize deallocation
} VramAllocationPool;

VramAllocationPool vram_texture_pool;

COLD static void internal_citro3d_init_vram_texture_pools()
{
    VramAllocationPool* p = &vram_texture_pool;
    
    if (!(p->vram_pool_base = p->vram_pool_offset = vramAlloc(VRAM_POOL_SIZE)))
    {
        printf("Failed to allocate VRAM pool of %luKiB\nVRAM textures have been disabled\n", BYTE_TO_KIB(VRAM_POOL_SIZE));
        g3dsConfig.vram_textures = false;
    }
    p->num_vram_tex = p->data_size = 0;
}

/**
 * Uploads the currently queued textures to VRAM. If the VRAM pool is full,
 * the pool and queue are both cleared instead.
 */
COLD static void internal_citro3d_upload_textures_to_vram()
{
    VramAllocationPool* p = &vram_texture_pool;
    size_t data_size_to_upload = 0;

    // Sum up size of all textures
    for (size_t i = 0; i < num_textures_to_upload_to_vram; i++)
    {
        data_size_to_upload += ROUND_UP(VRAM_TEX_ALIGNMENT_BYTES, texture_upload_queue[i]->c3d_tex.size);
    }

    // If we have slots available and VRAM space, upload all textures
    if (p->num_vram_tex + num_textures_to_upload_to_vram < MAX_VRAM_TEX &&  p->data_size + data_size_to_upload < VRAM_POOL_SIZE)
    {
        for (size_t i = 0; i < num_textures_to_upload_to_vram; i++)
        {
            TexHandle* handle = texture_upload_queue[i];

            handle->c3d_tex.data = p->vram_pool_offset;
            handle->load_status = TEX_VRAM;
            
            p->vram_textures[p->num_vram_tex++] = handle;
            p->vram_pool_offset += ROUND_UP(VRAM_TEX_ALIGNMENT_BYTES, handle->c3d_tex.size);

            C3D_TexUpload(&handle->c3d_tex, handle->addr_fcram);
            C3D_TexFlush(&handle->c3d_tex);
        }
        p->data_size += data_size_to_upload;
    }

    // If our pool is full, clear it and the queue
    else
    {
        // Clear pool
        for (size_t i = 0; i < p->num_vram_tex; i++)
        {
            TexHandle* handle = p->vram_textures[i];
            handle->c3d_tex.data = handle->addr_fcram;
            handle->load_status = TEX_FCRAM;
        }

        // Clear upload queue
        for (size_t i = 0; i < num_textures_to_upload_to_vram; i++)
        {
            texture_upload_queue[i]->load_status = TEX_FCRAM;
        }

        p->num_vram_tex = p->data_size = 0;
        p->vram_pool_offset = p->vram_pool_base;
    }

    num_textures_to_upload_to_vram = 0;
}

/**
 * Allocates a C3D_Tex for the context's current texture and uploads data if needed.
 * Handles replacement of existing textures.
 * If the texture is in VRAM mode, it is reverted to FCRAM mode.
 */
COLD static void internal_citro3d_upload_texture_common(void* data, struct TextureSize input_size, struct TextureSize output_size, GPU_TEXCOLOR format)
{
    TexHandle* handle = ctx.current_texture;
    C3D_Tex* tex = &handle->c3d_tex;

    ctx.current_texture->scale.s =   input_size.width  / (float) output_size.width;
    ctx.current_texture->scale.t = -(input_size.height / (float) output_size.height);

    switch (handle->load_status)
    {
        case TEX_VRAM:
            tex->data = handle->addr_fcram; // Swap back to FCRAM. We'll leave a hole in the VRAM pool, but that's OK.
        case TEX_FCRAM:
            C3D_TexDelete(tex);             // Delete FCRAM texture because the size might be different
        case TEX_UNINITIALIZED:
            break;
    }

    if (C3D_TexInit(tex, output_size.width, output_size.height, format))
    {
        C3D_TexUpload(tex, data);
        C3D_TexFlush(tex);
        handle->addr_fcram = tex->data;
        handle->load_status = TEX_FCRAM;
    }
    else
    {
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
