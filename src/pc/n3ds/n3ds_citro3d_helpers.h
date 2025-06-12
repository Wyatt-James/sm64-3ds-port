#pragma once

#include "c3d_inc.h"

typedef struct
{
    C3D_RenderTarget** target;
    __3ds_u32 width, height;
    GPU_COLORBUF color_format;
    GPU_DEPTHBUF depth_format;
} N3DS_RenderTargetConfiguration;

/**
 * Allocates overlapping C3D_RenderTargets, based on the given configurations.
 * Returns true if allocation succeeded for both color and depth, or false if it failed for either.
 * If allocation succeeds, all C3D_RenderTargets will have overlapping buffers, EXCEPT buffers which were marked as not using depth buffers; these will have NULL.
 * If allocation fails, all C3D_RenderTargets will have NULL buffers.
 * WYATT_TODO support stereoscopic framebuffers. Array of arrays?
 */
bool n3ds_allocate_overlapping_rendertargets(size_t num_targets, N3DS_RenderTargetConfiguration configs[num_targets]);
