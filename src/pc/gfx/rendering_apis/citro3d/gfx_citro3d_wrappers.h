#pragma once

/*
 * Convenience wrappers for C3D functions, usually for directly passing internal-use types.
 */

#include <stdbool.h>
#include <stdint.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/n3ds/c3d_inc.h"

#include "gfx_citro3d_helpers.h"

#include "src/pc/gfx/color_formats.h"

/*
 * This file contains convenience wrappers for C3D functions.
 * This file is allowed to change C3D context state.
 */

// Sets a C3D float uniform from a vector of floats.
static inline void C3DW_FVUnifSetArray(GPU_SHADER_TYPE type, int id, float vec[4])
{
    C3D_FVUnifSet(type, id, vec[0], vec[1], vec[2], vec[3]);
}

// Sets a C3D float uniform from an RGBA32 union. Scales by 1/255.
static inline void C3DW_FVUnifSetRGBA(GPU_SHADER_TYPE type, int id, union RGBA32 color)
{
    float r = color.r / 255.0f,
          g = color.g / 255.0f,
          b = color.b / 255.0f,
          a = color.a / 255.0f;
    C3D_FVUnifSet(type, id, r, g, b, a);
}

// Sets a C3D float uniform from an RGBA32 union, but sets Alpha to 0. Scales by 1/255.
static inline void C3DW_FVUnifSetRGB(GPU_SHADER_TYPE type, int id, union RGBA32 color)
{
    float r = color.r / 255.0f,
          g = color.g / 255.0f,
          b = color.b / 255.0f;
    C3D_FVUnifSet(type, id, r, g, b, 0);
}

static inline void C3DW_DepthMap(bool zmode_decal)
{
    C3D_DepthMap(true, -1.0f, zmode_decal ? -0.001f : 0);
}

static inline void C3DW_AlphaBlend(bool use_alpha)
{
    if (use_alpha)
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    else
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

static inline void C3DW_AlphaTest(bool alpha_test)
{
    C3D_AlphaTest(alpha_test, GPU_GREATER, 77);
}

static inline void C3DW_DepthTest(bool depth_test, bool depth_update)
{
    C3D_DepthTest(depth_test, GPU_LEQUAL, depth_update ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
}

static inline void C3DW_FogGasMode(bool fog_enabled)
{
    C3D_FogGasMode(citro3d_helpers_convert_fog_mode(fog_enabled), GPU_PLAIN_DENSITY, true);
}

static inline void C3DW_SetViewport(ScreenDimensions* cfg)
{
    C3D_SetViewport(cfg->y, cfg->x, cfg->height, cfg->width);
}

static inline void C3DW_SetScissor(ScreenDimensions* cfg)
{
    C3D_SetScissor(GPU_SCISSOR_NORMAL, cfg->y1, cfg->x1, cfg->y2, cfg->x2);
}

