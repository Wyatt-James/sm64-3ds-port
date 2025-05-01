#include "gfx_citro3d_screens.h"

#include "src/pc/n3ds/n3ds_system_info.h"
#include "src/pc/n3ds/n3ds_config.h"
#include "src/pc/n3ds/n3ds_gfx_wrappers.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_emulator.h"
#include "src/pc/pc_macros.h"
#include "src/pc/gfx/gfx_rendering_api.h"

#define DISPLAY_TRANSFER_FLAGS                        \
(                                                     \
	   GX_TRANSFER_FLIP_VERT(0)                       \
     | GX_TRANSFER_OUT_TILED(0)                       \
     | GX_TRANSFER_RAW_COPY(0)                        \
	 | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)   \
     | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)   \
)

C3D_ClearBits screen_clear_bits[GFX_C3D_VIEWPORT_COUNT];

C3D_RenderTarget *gTarget;
C3D_RenderTarget *gTargetRight;
C3D_RenderTarget *gTargetBottom;

static bool top_screen_initialized = false;
static bool bottom_screen_initialized = false;

static N3DS_DisplayMode parse_video_mode(bool antialias, bool wide, bool stereo3d)
{
    if (stereo3d)               return N3DS_DISPLAY_3D;
    if (!antialias && !wide)    return N3DS_DISPLAY_2D_400_240;
    if (!antialias &&  wide)    return N3DS_DISPLAY_2D_800_240;
    /* ( antialias &&  wide) */ return N3DS_DISPLAY_2D_800_480;
}

void deinitialize_top_screen(void)
{
    top_screen_initialized = false;
    if (gTarget != NULL)
    {
        C3D_RenderTargetDelete(gTarget);
        gTarget = NULL;
    }
    if (gTargetRight != NULL)
    {
        C3D_RenderTargetDelete(gTargetRight);
        gTargetRight = NULL;
    }
}

void deinitialize_bottom_screen(void)
{
    bottom_screen_initialized = false;
    if (gTargetBottom != NULL)
    {
        C3D_RenderTargetDelete(gTargetBottom);
        gTargetBottom = NULL;
    }
}

void deinitialize_screens(void)
{
    deinitialize_top_screen();
    deinitialize_bottom_screen();
}

void initialize_top_screen(void)
{
    bool use_aa   = g3dsMenuConfig.use_aa   && g3dsSystemInfo.supports_800px; // old 2DS does not support 800px
    bool use_wide = g3dsMenuConfig.use_wide && g3dsSystemInfo.supports_800px; // old 2DS does not support 800px
    bool use3d    = g3dsGfxState.stereo_3d_active;

    if (!top_screen_initialized)
    {
        const N3DS_DisplayMode display_mode = parse_video_mode(use_aa, use_wide, use3d);
        const N3DS_DisplayModeInfo* display_mode_info = gfx_3ds_display_mode_info(display_mode);
        const __3ds_u32 transferFlags = DISPLAY_TRANSFER_FLAGS | display_mode_info->transfer_scaling_flags;
        const int width  = display_mode_info->width;
        const int height = display_mode_info->height;
        gfxWSetTopMode(gfx_3ds_convert_top_mode(display_mode));

        gTarget = C3D_RenderTargetCreate(height, width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        C3D_RenderTargetSetOutput(gTarget, GFX_TOP, GFX_LEFT, transferFlags);

        if (display_mode == N3DS_DISPLAY_3D) {
            gTargetRight = C3D_RenderTargetCreate(height, width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
            C3D_RenderTargetSetOutput(gTargetRight, GFX_TOP, GFX_RIGHT, transferFlags);
        }
        
        g3dsGfxState.display_mode = display_mode;
        printf("Init top LCD to %s\n", display_mode_info->name);
        
        // Required for cake screen
        queue_screen_clear(GFX_C3D_VIEWPORT_TOP, C3D_CLEAR_COLOR);
    }
    top_screen_initialized = true;
    g3dsGfxState.reinitialize_top_screen = false;
}

void initialize_bottom_screen(void)
{
    if (!bottom_screen_initialized)
    {
        gTargetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        C3D_RenderTargetSetOutput(gTargetBottom, GFX_BOTTOM, GFX_LEFT,
            DISPLAY_TRANSFER_FLAGS | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

        printf("Init bottom LCD\n");
    }

    bottom_screen_initialized = true;
    g3dsGfxState.reinitialize_bottom_screen = false;
}

void initialize_screens(void)
{
    initialize_top_screen();
    initialize_bottom_screen();
}

void reinitialize_top_screen(void)
{
    deinitialize_top_screen();
    initialize_top_screen();
}

void reinitialize_bottom_screen(void)
{
    deinitialize_bottom_screen();
    initialize_bottom_screen();
}

void reinitialize_screens(void)
{
    deinitialize_screens();
    initialize_screens();
}

// Updates some settings and reinitializes the top screen based on the 3D slider position.
void update_stereoscopy(void)
{
	if(n3ds_hid_3d_slider() > 0.0)
    {
		g3dsMenuConfig.use_aa = false;
		g3dsMenuConfig.use_wide = false;
        g3dsGfxState.stereo_3d_active = true;
	}
    else
    {
        // default to 800px + AA
		g3dsMenuConfig.use_aa = true;
		g3dsMenuConfig.use_wide = true;
        g3dsGfxState.stereo_3d_active = false;
	}

    g3dsGfxState.bottom_screen_needs_render = true;
    g3dsGfxState.reinitialize_top_screen = true;
}

static void clear_render_target(C3D_ClearBits clear_bits, C3D_RenderTarget* target)
{
    if (clear_bits && target != NULL)
        C3D_RenderTargetClear(target, clear_bits, 0x000000FF, 0xFFFFFFFF);
}

void clear_render_targets(void)
{
    clear_render_target(screen_clear_bits[GFX_C3D_VIEWPORT_TOP],    gTarget);
    clear_render_target(screen_clear_bits[GFX_C3D_VIEWPORT_TOP],    gTargetRight);
    clear_render_target(screen_clear_bits[GFX_C3D_VIEWPORT_BOTTOM], gTargetBottom);

    // Reset flags
    for (size_t i = 0; i < GFX_C3D_VIEWPORT_COUNT; i++)
        overwrite_screen_clear(i, 0);
}

void queue_screen_clear(GFX_C3D_VIEWPORT viewport_id, C3D_ClearBits clear_bits)
{
    screen_clear_bits[viewport_id] |= clear_bits;
}

void overwrite_screen_clear(GFX_C3D_VIEWPORT viewport_id, C3D_ClearBits clear_bits)
{
    screen_clear_bits[viewport_id] = clear_bits;
}
