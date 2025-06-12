#pragma once

#ifdef TARGET_N3DS

#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/n3ds/n3ds_gfx_wrappers.h"

#include "src/pc/gfx/gfx_window_manager_api.h"

#define N3DS_TOP_FRAMEBUFFER_FORMAT    GSP_BGR8_OES
#define N3DS_BOTTOM_FRAMEBUFFER_FORMAT GSP_RGB565_OES

typedef enum
{
    N3DS_DISPLAY_2D_400_240,    // 400px 2D
    N3DS_DISPLAY_2D_800_240,    // 800px no AA
    N3DS_DISPLAY_2D_800_480,    // 800px +  AA
    N3DS_DISPLAY_3D,            // Dual 400px in 3D
    N3DS_DISPLAY_COUNT          // Not a valid mode
} N3DS_DisplayMode;

typedef struct
{
    uint32_t width, height;
    __3ds_u32 transfer_scaling_flags;
    char* name;
    N3DS_TopScreenMode top_mode;
} N3DS_DisplayModeInfo;

typedef struct
{
    bool bottom_screen_needs_render,
         reinitialize_top_screen,
         reinitialize_bottom_screen,
         stereo_3d_active;
    N3DS_DisplayMode display_mode;
} Gfx3DSState;

extern Gfx3DSState g3dsGfxState;
extern struct GfxWindowManagerAPI gfx_wapi_3ds;

const N3DS_DisplayModeInfo* gfx_3ds_display_mode_info(N3DS_DisplayMode mode);

#endif
