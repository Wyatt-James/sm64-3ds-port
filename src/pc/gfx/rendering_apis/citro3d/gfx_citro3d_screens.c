#include "gfx_citro3d_screens.h"

#include "src/pc/n3ds/n3ds_system_info.h"
#include "src/pc/n3ds/n3ds_config.h"
#include "src/pc/n3ds/n3ds_gfx_wrappers.h"
#include "src/pc/n3ds/n3ds_citro3d_helpers.h"
#include "src/pc/n3ds/n3ds_display.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_emulator.h"
#include "src/pc/pc_macros.h"
#include "src/pc/gfx/gfx_rendering_api.h"

#define DISPLAY_TRANSFER_FLAGS                        \
(                                                     \
	   GX_TRANSFER_FLIP_VERT(0)                       \
     | GX_TRANSFER_OUT_TILED(0)                       \
     | GX_TRANSFER_RAW_COPY(0)                        \
)

typedef struct
{
    C3D_RenderTarget **left, **right;
    N3DS_DisplayMode mode;
} TopScreenSetup;

C3D_RenderTarget *gTarget;
C3D_RenderTarget *gTargetRight;
C3D_RenderTarget *gTargetBottom;

static C3D_ClearBits screen_clear_bits[GFX_C3D_VIEWPORT_COUNT];

static C3D_RenderTarget* target_800_480;
static C3D_RenderTarget* target_800_240;
static C3D_RenderTarget* target_400_240_left;
static C3D_RenderTarget* target_400_240_right;
static C3D_RenderTarget* target_bottom;

static N3DS_RenderTargetGroupConfig
    group_800_480 = N3DS_RENDERTARGET_GROUP({&target_800_480,      800, 480, GPU_RB_RGB8,   GPU_RB_DEPTH16}),
    group_800_240 = N3DS_RENDERTARGET_GROUP({&target_800_240,      800, 240, GPU_RB_RGB8,   GPU_RB_DEPTH16}),
    group_stereo  = N3DS_RENDERTARGET_GROUP({&target_400_240_left, 400, 240, GPU_RB_RGB8,   GPU_RB_DEPTH16}, {&target_400_240_right, 400, 240, GPU_RB_RGB8, GPU_RB_DEPTH16}),
    group_bottom  = N3DS_RENDERTARGET_GROUP({&target_bottom,       320, 240, GPU_RB_RGB565, C3D_DEPTH_NONE});

static N3DS_RenderTargetGroupConfig* initializers_top[] = {
    &group_800_480,
    &group_800_240,
    &group_stereo,
};

static N3DS_RenderTargetGroupConfig* initializers_bottom[] = {
    &group_bottom
};

TopScreenSetup top_screen_setups[] = {
    [N3DS_DISPLAY_2D_400_240] = {&target_400_240_left, NULL,                  N3DS_DISPLAY_2D_400_240},
    [N3DS_DISPLAY_2D_800_240] = {&target_800_240,      NULL,                  N3DS_DISPLAY_2D_800_240},
    [N3DS_DISPLAY_2D_800_480] = {&target_800_480,      NULL,                  N3DS_DISPLAY_2D_800_480},
    [N3DS_DISPLAY_3D]         = {&target_400_240_left, &target_400_240_right, N3DS_DISPLAY_3D},
};

static N3DS_DisplayMode parse_video_mode(bool antialias, bool wide, bool stereo3d)
{
    if (stereo3d)               return N3DS_DISPLAY_3D;
    if (!antialias && !wide)    return N3DS_DISPLAY_2D_400_240;
    if (!antialias &&  wide)    return N3DS_DISPLAY_2D_800_240;
    /* ( antialias &&  wide) */ return N3DS_DISPLAY_2D_800_480;
}

static __3ds_u32 create_transfer_flags(C3D_RenderTarget* target, GSPGPU_FramebufferFormat framebuffer_format, __3ds_u32 scaling_flags)
{
    return DISPLAY_TRANSFER_FLAGS
         | GX_TRANSFER_IN_FORMAT(n3ds_gpu_colorbuf_to_gx(target->frameBuf.colorFmt))
         | GX_TRANSFER_OUT_FORMAT(n3ds_gsp_framebuffer_to_gx(framebuffer_format))
         | scaling_flags;
}

static void reconfigure_top_screen()
{
    // Detach
    if (gTarget      != NULL) C3D_RenderTargetDetachOutput(gTarget);
    if (gTargetRight != NULL) C3D_RenderTargetDetachOutput(gTargetRight);
    gTarget = gTargetRight = NULL;
    
    // Grab configs
    bool use_aa   = g3dsMenuConfig.use_aa   && g3dsSystemInfo.supports_800px; // old 2DS does not support 800px
    bool use_wide = g3dsMenuConfig.use_wide && g3dsSystemInfo.supports_800px; // old 2DS does not support 800px
    bool use3d    = g3dsGfxState.stereo_3d_active;

    // Look up display mode
    N3DS_DisplayMode mode = parse_video_mode(use_aa, use_wide, use3d);
    TopScreenSetup* setup = &top_screen_setups[mode];
    const N3DS_DisplayModeInfo* info = gfx_3ds_display_mode_info(setup->mode);

    // Bind the targets
    if (setup->left)  gTarget      = *setup->left;
    if (setup->right) gTargetRight = *setup->right;
    if (gTarget)      C3D_RenderTargetSetOutput(gTarget,      GFX_TOP, GFX_LEFT,  create_transfer_flags(gTarget,      N3DS_TOP_FRAMEBUFFER_FORMAT, info->transfer_scaling_flags));
    if (gTargetRight) C3D_RenderTargetSetOutput(gTargetRight, GFX_TOP, GFX_RIGHT, create_transfer_flags(gTargetRight, N3DS_TOP_FRAMEBUFFER_FORMAT, info->transfer_scaling_flags));
    
    // Write some global state
    gfxWSetTopMode(info->top_mode);
    g3dsGfxState.display_mode = mode;
    printf("Init top LCD to %s\n", info->name);
    
    // Required for cake screen
    queue_screen_clear(GFX_C3D_VIEWPORT_TOP, C3D_CLEAR_COLOR);

    g3dsGfxState.reinitialize_top_screen = false;
}

static void reconfigure_bottom_screen()
{
    if (gTargetBottom != NULL)
    {
        C3D_RenderTargetDetachOutput(gTargetBottom);
    }

    gTargetBottom = target_bottom;
    C3D_RenderTargetSetOutput(gTargetBottom, GFX_BOTTOM, GFX_LEFT, create_transfer_flags(gTargetBottom, N3DS_BOTTOM_FRAMEBUFFER_FORMAT, GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));
}

void reconfigure_screens(bool force)
{
    if (g3dsGfxState.reinitialize_top_screen || force)
    {
        reconfigure_top_screen();
    }
    if (g3dsGfxState.reinitialize_bottom_screen || force)
    {
        reconfigure_bottom_screen();
    }
    g3dsGfxState.reinitialize_top_screen = g3dsGfxState.reinitialize_bottom_screen = false;
}

void initialize_screens(void)
{
    if (target_bottom != NULL)
    {
        printf("Tried to initialize screens twice\n");
        svcBreak(USERBREAK_PANIC);
    }

    if (!n3ds_allocate_overlapping_rendertargets(ARRAY_COUNT(initializers_top), initializers_top))
    {
        printf("Failed to initialize top RenderTarget(s)\n");
        svcBreak(USERBREAK_PANIC);
    }

    if (!n3ds_allocate_overlapping_rendertargets(ARRAY_COUNT(initializers_bottom), initializers_bottom))
    {
        printf("Failed to initialize bottom RenderTarget(s)\n");
        svcBreak(USERBREAK_PANIC);
    }

    reconfigure_screens(true);
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
        C3D_RenderTargetClear(target, clear_bits, 0, 0xFFFFFFFF);
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
