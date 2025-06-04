#include "n3ds_citro3d_helpers.h"
#include "src/pc/pc_macros.h"

#define USE_DEPTH(f_) ((f_) >= 0)

static void free_target_bufs(C3D_RenderTarget* t)
{
    if (t->frameBuf.colorBuf) vramFree(t->frameBuf.colorBuf);
    if (t->frameBuf.depthBuf) vramFree(t->frameBuf.depthBuf);
    t->ownsColor = t->ownsDepth = false;
}

static inline C3D_RenderTarget* init_target(N3DS_RenderTargetConfiguration c)
{
    return C3D_RenderTargetCreate(c.height, c.width, c.color_format, c.depth_format);
}

// Copied from C3D
static inline vramAllocPos get_vram_bank_from_vaddr(const void* addr)
{
	__3ds_u32 vaddr = (__3ds_u32) addr;
	return vaddr < OS_VRAM_VADDR + OS_VRAM_SIZE / 2 ? VRAM_ALLOC_A : VRAM_ALLOC_B;
}

/**
 * Based on C3D's render target allocator.
 * Attempts to allocate buffers, but only if their respective sizes are > 0.
 * Returns true if all requested buffers were allocated successfully, otherwise false.
 * If false is returned, both buffers are freed automatically.
 */
static bool allocate_buffers(size_t color_buf_size, size_t depth_buf_size, void** out_color_buf, void** out_depth_buf)
{
	void* depth_buf = NULL;
    void* color_buf = NULL;
    vramAllocPos color_bank = 0;

    if (color_buf_size > 0)
    {
	    color_buf = vramAlloc(color_buf_size);
        if (color_buf == NULL) goto fail;
        color_bank = get_vram_bank_from_vaddr(color_buf);
    }

	if (depth_buf_size > 0)
	{
		depth_buf = vramAllocAt(depth_buf_size, VRAM_ALLOC_ANY ^ color_bank);
		if (!depth_buf) depth_buf = vramAllocAt(depth_buf_size, color_bank);
        if (depth_buf == NULL) goto fail;
	}

    *out_color_buf = color_buf;
    *out_depth_buf = depth_buf;
    return true;

fail:
    if (depth_buf != NULL) vramFree(depth_buf);
    if (color_buf != NULL) vramFree(color_buf);
    color_buf = depth_buf = NULL;
    return false;
}

bool n3ds_allocate_overlapping_rendertargets(size_t num_targets, N3DS_RenderTargetConfiguration configs[num_targets])
{
    size_t largest_color_size = 0, largest_depth_size = 0;
    bool success = true;

    // Find the maximum required size for each buffer and create our targets
    for (size_t i = 0; i < num_targets; i++)
    {
        N3DS_RenderTargetConfiguration* c = &configs[i];
        C3D_RenderTarget* t = init_target(configs[i]);
        size_t color_size = t->frameBuf.colorBuf != NULL ? C3D_CalcColorBufSize(c->width, c->height, c->color_format) : 0;
        size_t depth_size = t->frameBuf.depthBuf != NULL ? C3D_CalcDepthBufSize(c->width, c->height, c->depth_format) : 0;
        largest_color_size = MAX(largest_color_size, color_size);
        largest_depth_size = MAX(largest_depth_size, depth_size);
        free_target_bufs(t);
        *c->target = t;
    }

    // Allocate our real buffers
    void* color_buf;
    void* depth_buf;
    success = allocate_buffers(largest_color_size, largest_depth_size, &color_buf, &depth_buf);

    // Copy our buffers to each target
    for (size_t i = 0; i < num_targets; i++)
    {
        C3D_RenderTarget* t = *configs[i].target;
        t->frameBuf.colorBuf = color_buf;
        t->frameBuf.depthBuf = USE_DEPTH(configs[i].depth_format) ? depth_buf : NULL; // Only copy depth if it was requested, else NULL
    }

    if (!success) printf("Could not initialize RenderTargets.\n");

    return success;
}
