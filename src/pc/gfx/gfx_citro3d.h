#ifndef GFX_CITRO3D_H
#define GFX_CITRO3D_H

#include "gfx_rendering_api.h"
#include "multi_viewport/multi_viewport.h"

enum ViewportId3DS {
    VIEW_MAIN_SCREEN   = 0,
    VIEW_BOTTOM_SCREEN = 1
};

extern struct GfxRenderingAPI gfx_citro3d_api;

// WYATT_TODO figure out how to get these functions into the GfxRenderingAPI. I'm done for tonight.

// Sets the clear color for a Viewport.
void gfx_citro3d_set_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Sets the clear color for a Viewport.
void gfx_citro3d_set_clear_color_RGBA32(enum ViewportId3DS viewport, u32 color);

// Sets a buffer to be cleared for the given viewport on the next frame.
// All buffers provided will be cleared.
void gfx_citro3d_set_viewport_clear_buffer(enum ViewportId3DS viewport, enum ViewportClearBuffer mode);

// Save current screen clear buffer flags (used for 60fps patch compatibility)
void gfx_citro3d_save_viewport_clear_buffer_flags();

// Restore saved screen clear buffer flags (used for 60fps patch compatibility)
void gfx_citro3d_restore_viewport_clear_buffer_flags();

#endif
