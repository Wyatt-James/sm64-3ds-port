#ifndef GFX_CITRO3D_H
#define GFX_CITRO3D_H

#include "gfx_rendering_api.h"
#include "multi_viewport/multi_viewport.h"
#include "color_formats.h"

#define NUM_MATRIX_SETS 8
#define DEFAULT_MATRIX_SET 0

enum ViewportId3DS {
    VIEW_MAIN_SCREEN   = 0,
    VIEW_BOTTOM_SCREEN = 1
};

extern struct UniformLocations uniform_locations;

extern struct GfxRenderingAPI gfx_citro3d_api;

// WYATT_TODO figure out how to get these functions into the GfxRenderingAPI. I'm done for tonight.

// Sets the clear color for a Viewport.
void gfx_citro3d_set_viewport_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Sets the clear color for a Viewport.
void gfx_citro3d_set_viewport_clear_color_u32(enum ViewportId3DS viewport, uint32_t color);

// Sets the clear color for a Viewport.
void gfx_citro3d_set_viewport_clear_color_RGBA32(enum ViewportId3DS viewport, union RGBA32 color);

// Sets the clear depth for a Viewport.
void gfx_citro3d_set_viewport_clear_depth(enum ViewportId3DS viewport, uint32_t depth);

// Sets a buffer to be cleared for the given viewport on the next frame.
// All flags provided will be cleared on gfx_citro3d_start_frame().
// Flags are logically OR-ed together.
void gfx_citro3d_set_viewport_clear_buffer_mode(enum ViewportId3DS viewport, enum ViewportClearBuffer mode);

void gfx_citro3d_set_model_view_matrix(float mtx[4][4]);
void gfx_citro3d_set_game_projection_matrix(float mtx[4][4]);
void gfx_citro3d_apply_model_view_matrix();
void gfx_citro3d_apply_game_projection_matrix();

// Selects which matrix set is currently active.
void gfx_citro3d_select_matrix_set(uint32_t matrix_set_id);


void gfx_citro3d_set_backface_culling_mode(uint32_t culling_mode);

#endif
