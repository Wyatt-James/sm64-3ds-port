
#include <stdint.h>

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <citro3d.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_macros.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/multi_viewport/multi_viewport.h"

// Determines the clear config for each viewport.
static union ScreenClearConfigsN3ds screen_clear_configs = {
    .top    = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
    .bottom = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
};

// Handles 3DS screen clearing
void c3d_cold_clear_buffers()
{
    C3D_ClearBits clear_top    = (C3D_ClearBits) screen_clear_configs.top.bufs    | C3D_CLEAR_ALL,
                  clear_bottom = (C3D_ClearBits) screen_clear_configs.bottom.bufs | C3D_CLEAR_ALL;

    uint32_t color_top    = BSWAP32(screen_clear_configs.top.color.u32),
             color_bottom = BSWAP32(screen_clear_configs.bottom.color.u32),
             depth_top    = screen_clear_configs.top.depth,
             depth_bottom = screen_clear_configs.bottom.depth;

    // Clear top screen
    if (clear_top)
        C3D_RenderTargetClear(gTarget, clear_top, color_top, depth_top);
        
    // Clear right-eye view
    if (clear_top && gTargetRight != NULL)
        C3D_RenderTargetClear(gTargetRight, clear_top, color_top, depth_top);

    // Clear bottom screen only if it needs re-rendering.
    if (clear_bottom)
        C3D_RenderTargetClear(gTargetBottom, clear_bottom, color_bottom, depth_bottom);

    // Reset screen clear buffer flags
    screen_clear_configs.top.bufs = 
    screen_clear_configs.bottom.bufs = VIEW_CLEAR_BUFFER_NONE;
}

void gfx_rapi_set_viewport_clear_color(uint32_t viewport_id, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    screen_clear_configs.array[viewport_id].color.u32 = (union RGBA32) {.r = r, .g = g, .b = b, .a = a}.u32;
}

void gfx_rapi_set_viewport_clear_color_u32(uint32_t viewport_id, uint32_t color)
{
    screen_clear_configs.array[viewport_id].color.u32 = color;
}

void gfx_rapi_set_viewport_clear_depth(uint32_t viewport_id, uint32_t depth)
{
    screen_clear_configs.array[viewport_id].depth = depth;
}

void gfx_rapi_enable_viewport_clear_buffer_flag(uint32_t viewport_id, enum ViewportClearBuffer mode)
{
    screen_clear_configs.array[viewport_id].bufs |= mode;
}
