#include "n3ds_citro3d_helpers.h"
#include "src/pc/pc_macros.h"

/**
 * Validates function input data.
 * Returns true if:
 * - All C3D_RenderTarget** are non-null
 * - All C3D_RenderTarget* are null (i.e. rendertargets not already initialized)
 */
static bool is_single_well_formed(N3DS_RenderTargetConfig* config)
{
    if (config->target == NULL)
    {
        fprintf(stderr, "C3D_RenderTarget** is null\n");
        return false;
    }

    if (*config->target != NULL)
    {
        fprintf(stderr, "C3D_RenderTarget already exists\n");
        return false;
    }

    return true;
}

/**
 * Validates function input data.
 * Returns true if:
 * - All N3DS_RenderTargetGroupConfig* are non-null
 * - All C3D_RenderTarget** are non-null
 * - All C3D_RenderTarget* are null (i.e. rendertargets not already initialized)
 */
static bool is_group_well_formed(size_t num_groups, N3DS_RenderTargetGroupConfig* groups[num_groups])
{
    for (size_t i = 0; i < num_groups; i++)
    {
        if (groups[i] == NULL)
        {
            fprintf(stderr, "Nullptr N3DS_RenderTargetGroupConfig %u\n", i);
            return false;
        }

        for (size_t j = 0; j < groups[i]->count; j++)
        {
            if (!is_single_well_formed(&groups[i]->configs[j]))
            {
                fprintf(stderr, "Index that failed: %u, %u\n", i, j);
                return false;
            }
        }
    }

    return true;
}

// Frees the framebufs and NULLs their pointers
static void free_target_bufs(C3D_RenderTarget* t)
{
    if (t->frameBuf.colorBuf != NULL) vramFree(t->frameBuf.colorBuf);
    if (t->frameBuf.depthBuf != NULL) vramFree(t->frameBuf.depthBuf);
    t->frameBuf.colorBuf = t->frameBuf.depthBuf = NULL;
    // We don't zero ownsColor or ownsDepth quite yet because we use those flags for other purposes
}

// Initializes the C3D_RenderTarget and sets the given config's pointer
static inline C3D_RenderTarget* init_target(N3DS_RenderTargetConfig* c)
{
    return (*c->target = C3D_RenderTargetCreate(c->height, c->width, c->color_format, c->depth_format));
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
		if (depth_buf == NULL) depth_buf = vramAllocAt(depth_buf_size, color_bank);
        if (depth_buf == NULL) goto fail;
	}

    *out_color_buf = color_buf;
    *out_depth_buf = depth_buf;
    return true;

fail:
    if (depth_buf != NULL) vramFree(depth_buf);
    if (color_buf != NULL) vramFree(color_buf);
    color_buf = depth_buf = *out_color_buf = *out_depth_buf = NULL;
    return false;
}

/* This version is commented out because it is unused, but it was left in a fully functional state.
bool n3ds_allocate_overlapping_rendertargets_simple(size_t num_targets, N3DS_RenderTargetConfig configs[num_targets])
{
    size_t largest_color_size = 0, largest_depth_size = 0;
    
    // Avoid working with invalid inputs
    for (size_t i = 0; i < num_targets; i++)
    {
        if (!is_single_well_formed(&configs[i])) return false;
    }

    // Find the maximum required size for each buffer and create our targets
    for (size_t i = 0; i < num_targets; i++)
    {
        N3DS_RenderTargetConfig* c = &configs[i];
        C3D_RenderTarget* t = init_target(c);
        if (t == NULL) goto fail;

        size_t color_size = t->ownsColor ? C3D_CalcColorBufSize(c->width, c->height, c->color_format) : 0;
        size_t depth_size = t->ownsDepth ? C3D_CalcDepthBufSize(c->width, c->height, c->depth_format) : 0;
        largest_color_size = MAX(largest_color_size, color_size);
        largest_depth_size = MAX(largest_depth_size, depth_size);
        free_target_bufs(t);
    }

    // Allocate our real buffers
    void* color_buf;
    void* depth_buf;
    if(!allocate_buffers(largest_color_size, largest_depth_size, &color_buf, &depth_buf)) // 0x80-aligned
    {
        fprintf(stderr, "Could not allocate RenderTarget buffers: %x, %x\n", largest_color_size, largest_depth_size);
        goto fail;
    }

    // Copy our buffers to each target
    for (size_t i = 0; i < num_targets; i++)
    {
        C3D_RenderTarget* t = *configs[i].target;
        t->frameBuf.colorBuf = t->ownsColor ? color_buf : NULL;
        t->frameBuf.depthBuf = t->ownsDepth ? depth_buf : NULL;
        t->ownsColor = t->ownsDepth = false;
    }

    return true;

fail:
    // If alloc failed, we need to delete all targets
    for (size_t i = 0; i < num_targets; i++)
    {
        C3D_RenderTarget* t = *configs[i].target;
        if (t != NULL)
        {
            t->ownsColor = t->ownsDepth = false;
            free_target_bufs(t);
            C3D_RenderTargetDelete(t);
            *configs[i].target = NULL;
        }
    }

    return false;
}
*/

bool n3ds_allocate_overlapping_rendertargets(size_t num_groups, N3DS_RenderTargetGroupConfig* groups[num_groups])
{
    size_t largest_color_size = 0, largest_depth_size = 0;

    // Avoid working with invalid inputs
    if (!is_group_well_formed(num_groups, groups)) return false;

    // Find the maximum required size for each buffer and create our C3D_RenderTargets
    for (size_t i = 0; i < num_groups; i++)
    {
        N3DS_RenderTargetGroupConfig* group = groups[i];
        size_t group_color_size = 0, group_depth_size = 0;
        for (size_t j = 0; j < group->count; j++)
        {
            N3DS_RenderTargetConfig* c = &group->configs[j]; // Can nullptr deref if data is malformed
            C3D_RenderTarget* t = init_target(c);
            if (t == NULL) goto fail;
            
            size_t color_size = t->ownsColor ? C3D_CalcColorBufSize(c->width, c->height, c->color_format) : 0;
            size_t depth_size = t->ownsDepth ? C3D_CalcDepthBufSize(c->width, c->height, c->depth_format) : 0;
            group_color_size += ROUND_UP((size_t)0x80, color_size);
            group_depth_size += ROUND_UP((size_t)0x80, depth_size);
            free_target_bufs(t);
        }
        
        largest_color_size = MAX(largest_color_size, group_color_size);
        largest_depth_size = MAX(largest_depth_size, group_depth_size);
    }

    // Allocate our real buffers
    void* color_buf;
    void* depth_buf;
    if(!allocate_buffers(largest_color_size, largest_depth_size, &color_buf, &depth_buf)) // 0x80-aligned by default
    {
        fprintf(stderr, "Could not allocate RenderTarget buffers: %x, %x\n", largest_color_size, largest_depth_size);
        goto fail;
    }

    // Set up the pointers into our actual buffers
    for (size_t i = 0; i < num_groups; i++)
    {
        N3DS_RenderTargetGroupConfig* group = groups[i];
        size_t color_offset = 0, depth_offset = 0;
        for (size_t j = 0; j < group->count; j++)
        {
            N3DS_RenderTargetConfig* c = &group->configs[j];
            C3D_RenderTarget* t = *c->target;

            if (t->ownsColor)
            {
                size_t color_size = C3D_CalcColorBufSize(c->width, c->height, c->color_format);
                t->frameBuf.colorBuf = color_buf + color_offset;
                color_offset = ROUND_UP((size_t) 0x80, color_offset + color_size);
            }

            if (t->ownsDepth)
            {
                size_t depth_size = C3D_CalcDepthBufSize(c->width, c->height, c->depth_format);
                t->frameBuf.depthBuf = depth_buf + depth_offset;
                depth_offset = ROUND_UP((size_t) 0x80, depth_offset + depth_size);
            }

            t->ownsColor = t->ownsDepth = false;
        }
    }

    return true;

fail:

    // If alloc failed, we need to delete all targets
    for (size_t i = 0; i < num_groups; i++)
    {
        N3DS_RenderTargetGroupConfig* group = groups[i];
        for (size_t j = 0; j < group->count; j++)
        {
            C3D_RenderTarget* t = *group->configs[j].target;
            if (t != NULL)
            {
                t->ownsColor = t->ownsDepth = false;
                free_target_bufs(t);
                C3D_RenderTargetDelete(t);
                *group->configs[j].target = NULL;
            }
        }
    }

    return false;
}
