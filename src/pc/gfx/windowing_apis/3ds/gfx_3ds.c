#ifdef TARGET_N3DS

#include "gfx_3ds.h"

#include <stdio.h>
#include <stdlib.h>

#include "macros.h"


#include "src/pc/n3ds/libctru_inc.h"

#include "src/pc/gfx/gfx_rendering_api.h"

#include "src/pc/n3ds/n3ds_system_info.h"
#include "src/pc/n3ds/n3ds_threading.h"
#include "src/pc/audio/audio_3ds.h"
#include "src/pc/profiler_3ds.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_screens.h"
#include "src/pc/n3ds/n3ds_system_info.h"
#include "src/pc/n3ds/n3ds_hid.h"
#include "src/pc/n3ds/n3ds_config.h"

#define GX_REGION(addr_, val_, end_addr_, control_) (__3ds_u32*) (addr_), (val_), (__3ds_u32*) (end_addr_), (control_)
#define GX_REGION_NONE GX_REGION(NULL, 0, NULL, 0)
#define GX_MEMORYFILL_SINGLE(addr_, val_, end_addr_, control_) GX_MemoryFill(GX_REGION((addr_), (val_), (end_addr_), (control_)), GX_REGION_NONE)

Gfx3DSState g3dsGfxState = {
    .bottom_screen_needs_render = false,
    .stereo_3d_active           = false,
    .reinitialize_top_screen    = false,
    .reinitialize_bottom_screen = false,
    .display_mode               = N3DS_DISPLAY_2D_800_480,
};

static const N3DS_DisplayModeInfo display_mode_info[N3DS_DISPLAY_COUNT] = {
    [N3DS_DISPLAY_2D_400_240] = {.width = 400, .height = 240, .transfer_scaling_flags = GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO), .name = "N3DS_DISPLAY_2D_400_240", .top_mode = MODE_2D},
    [N3DS_DISPLAY_2D_800_240] = {.width = 800, .height = 240, .transfer_scaling_flags = GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO), .name = "N3DS_DISPLAY_2D_800_240", .top_mode = MODE_WIDE},
    [N3DS_DISPLAY_2D_800_480] = {.width = 800, .height = 480, .transfer_scaling_flags = GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_X),  .name = "N3DS_DISPLAY_2D_800_480", .top_mode = MODE_WIDE},
    [N3DS_DISPLAY_3D]         = {.width = 400, .height = 240, .transfer_scaling_flags = GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO), .name = "N3DS_DISPLAY_3D",         .top_mode = MODE_3D},
};

// Synchronously clears the entirety of N3DS VRAM.
static void clear_vram()
{
    GX_MEMORYFILL_SINGLE(OS_VRAM_VADDR, 0, (OS_VRAM_VADDR + OS_VRAM_SIZE), BIT(0) | (2 << 8));
    gspWaitForEvent(GSPGPU_EVENT_PSC0, true);
}

// Initializes the console, based on g3dsConfig. Does nothing if VRAM framebuffers are enabled.
static void init_console()
{
    if (!g3dsConfig.vram_framebuffers)
    {
        switch (g3dsConfig.console_screen)
        {
            default:                 // Default to none
            case N3DS_SCREEN_NONE:   break;
            case N3DS_SCREEN_BOTTOM: consoleInit(GFX_BOTTOM, NULL); break;
            case N3DS_SCREEN_TOP:    consoleInit(GFX_TOP,    NULL); break;
        }
    }
}

static void gfx_3ds_init(UNUSED const char *game_name, UNUSED bool start_in_fullscreen)
{
    gspInit();
    clear_vram();
    gspExit();
    
    gfxInit(N3DS_TOP_FRAMEBUFFER_FORMAT, N3DS_BOTTOM_FRAMEBUFFER_FORMAT, g3dsConfig.vram_framebuffers);
    init_console();
    shprog_emu64_init();
}

static void gfx_3ds_exit(void)
{
    gfxExit();
}

const N3DS_DisplayModeInfo* gfx_3ds_display_mode_info(N3DS_DisplayMode mode)
{
    return &display_mode_info[ENUM_CLAMP(mode, N3DS_DISPLAY_COUNT)];
}

static void gfx_3ds_get_dimensions(uint32_t *width, uint32_t *height)
{
    *width = 400;
    *height = 240;
}

// Stub functions
void gfx_3ds_set_keyboard_callbacks(UNUSED bool (*on_key_down)(int scancode), UNUSED bool (*on_key_up)(int scancode), UNUSED void (*on_all_keys_up)(void)) {}
void gfx_3ds_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(bool is_now_fullscreen)) {}
void gfx_3ds_set_fullscreen(UNUSED bool enable) {}
bool gfx_3ds_start_frame(void) { return true; }
void gfx_3ds_swap_buffers_begin(void) {}
void gfx_3ds_swap_buffers_end(void) {} // Citro3D handles swapping automatically in C3D_FrameEnd()
double gfx_3ds_get_time(void) { return 0.0; }

#include "src/pc/n3ds/n3ds_main.h"
struct GfxWindowManagerAPI gfx_wapi_3ds =
{
    gfx_3ds_init,
    gfx_3ds_exit,
    gfx_3ds_set_keyboard_callbacks,
    gfx_3ds_set_fullscreen_changed_callback,
    gfx_3ds_set_fullscreen,
    n3ds_main_loop,
    gfx_3ds_get_dimensions,
    n3ds_handle_events,
    gfx_3ds_start_frame,
    gfx_3ds_swap_buffers_begin,
    gfx_3ds_swap_buffers_end,
    gfx_3ds_get_time
};

#endif
