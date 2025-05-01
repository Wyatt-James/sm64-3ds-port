#pragma once

/*
 * A file for managing the C3D rendering API's render buffers and LCD interactions.
 */

#include "src/pc/n3ds/c3d_inc.h"
#include "src/pc/n3ds/libctru_inc.h"

typedef enum
{
    GFX_C3D_VIEWPORT_TOP,
    GFX_C3D_VIEWPORT_BOTTOM,
    GFX_C3D_VIEWPORT_COUNT,
} GFX_C3D_VIEWPORT;

extern C3D_RenderTarget *gTarget;
extern C3D_RenderTarget *gTargetRight;
extern C3D_RenderTarget *gTargetBottom;

void deinitialize_top_screen(void);
void deinitialize_bottom_screen(void);
void deinitialize_screens(void);
void initialize_top_screen(void);
void initialize_bottom_screen(void);
void initialize_screens(void);
void reinitialize_top_screen(void);
void reinitialize_bottom_screen(void);
void reinitialize_screens(void);

void update_stereoscopy(void);

void clear_render_targets(void);                                                 // Clears screens based on their current clear bits.
void queue_screen_clear(GFX_C3D_VIEWPORT viewport_id, C3D_ClearBits clear_bits); // Enables clear bits for a screen.
void overwrite_screen_clear(GFX_C3D_VIEWPORT viewport_id, C3D_ClearBits clear_bits);   // Overwrites the clear bits for a screen.
