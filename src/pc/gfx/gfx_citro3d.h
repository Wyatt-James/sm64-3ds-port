#ifndef GFX_CITRO3D_H
#define GFX_CITRO3D_H

#include "gfx_rendering_api.h"
#include "multi_viewport/multi_viewport.h"

struct ScreenFlags3DS {
    enum ViewportClearMode top;
    enum ViewportClearMode right;
    enum ViewportClearMode bottom;
};

extern struct GfxRenderingAPI gfx_citro3d_api;

extern struct ScreenFlags3DS gfx_screen_clear_flags;

#endif
