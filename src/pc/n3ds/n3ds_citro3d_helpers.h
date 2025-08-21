#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "c3d_inc.h"

// Handy macro for initializing an N3DS_RenderTargetGroupConfig
#define N3DS_RENDERTARGET_GROUP(...) (N3DS_RenderTargetGroupConfig) {   \
    .count = VARARGS_COUNT(N3DS_RenderTargetConfig, __VA_ARGS__), \
    .configs = {__VA_ARGS__},                               \
}

// A config for a single RenderTarget
typedef struct
{
    C3D_RenderTarget** target;
    __3ds_u32 width, height;
    GPU_COLORBUF color_format;
    GPU_DEPTHBUF depth_format;
} N3DS_RenderTargetConfig;

// A config for a group of RenderTargets, allocated sequentially
typedef struct
{
    size_t count;
    N3DS_RenderTargetConfig configs[]; // Flexible array member
} N3DS_RenderTargetGroupConfig;

/**
 * Allocates overlapping C3D_RenderTargets, based on the given configurations. This version does not support grouping.
 * Returns true if everything succeeds, else false.
 *   - Succeeds if allocation succeeded for both color and depth, or fails if it failed for either.
 *   - Fails if any config has a NULL C3D_RenderTarget**.
 *   - Fails if any config has any config's C3D_RenderTarget already exists.
 * 
 * If allocation succeeds, all C3D_RenderTargets will have overlapping buffers. Any buffers with a format that C3D wouldn't allocate for will be NULL.
 * If allocation fails, all C3D_RenderTargets allocated will be deleted and all memory freed.
 * 
 * This version is commented out because it is unused, but it was left in a fully functional state.
 */
// bool n3ds_allocate_overlapping_rendertargets_simple(size_t num_targets, N3DS_RenderTargetConfig configs[num_targets]);

/**
 * Allocates overlapping C3D_RenderTargets, based on the given configurations. This version supports grouping.
 * Returns true if everything succeeds, else false.
 *   - Succeeds if allocation succeeded for both color and depth, or fails if it failed for either.
 *   - Fails if any N3DS_RenderTargetGroupConfig* in the top-level array is NULL.
 *   - Fails if any config has a NULL C3D_RenderTarget**.
 *   - Fails if any config has any config's C3D_RenderTarget already exists.
 * 
 * If allocation succeeds, all C3D_RenderTarget groups will have overlapping buffers. Any buffers with a format that C3D wouldn't allocate for will be NULL.
 * If allocation fails, all C3D_RenderTargets allocated will be deleted and all memory freed.
 */
bool n3ds_allocate_overlapping_rendertargets(size_t num_groups, N3DS_RenderTargetGroupConfig* configs[num_groups]);
