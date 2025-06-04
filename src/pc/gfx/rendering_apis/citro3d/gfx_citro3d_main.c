#include <stddef.h>
#include <stdint.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/n3ds/c3d_inc.h"

#include "gfx_citro3d_internal_types.h"
#include "gfx_citro3d_profiler.h"
#include "gfx_citro3d_helpers.h"
#include "gfx_citro3d_screens.h"
#include "gfx_citro3d_menu.h"
#include "gfx_citro3d_emulator.h"

#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/gfx/gfx_rendering_api.h"
#include "src/pc/profiler_3ds.h"
#include "src/pc/n3ds/n3ds_hid.h"
#include "src/pc/n3ds/n3ds_menu.h"

#define DEFAULT_GXQUEUE_SIZE 32 // This is the default used by C3D.

#ifdef VERSION_EU
#define FRAME_RATE 25
#else
#define FRAME_RATE 30
#endif

static ShaderProgram default_program;

void gfx_rapi_start_frame(void)
{
    // if we just enabled or disabled stereo 3D, reinitialize the top screen
    bool cur_on  = n3ds_hid_3d_slider() > 0.0f,
         prev_on = n3ds_hid_prev_3d_slider() > 0.0f;
    if (cur_on != prev_on)
        update_stereoscopy();
    
    reconfigure_screens(false);
    
    // Must occur after screen init
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // Due to hardware differences, the PC port always clears the depth buffer.
    queue_screen_clear(GFX_C3D_VIEWPORT_TOP, C3D_CLEAR_DEPTH);

    if (g3dsGfxState.bottom_screen_needs_render)
        queue_screen_clear(GFX_C3D_VIEWPORT_BOTTOM, C3D_CLEAR_COLOR);

    clear_render_targets();

    gfx_citro3d_emulator_start_frame();
}

void gfx_rapi_end_frame(void)
{
    gfx_citro3d_emulator_end_frame();
    n3ds_menu_render();
    C3D_FrameEnd(0);
}

void gfx_rapi_init(void)
{
    C3D_InitEx(C3D_DEFAULT_CMDBUF_SIZE, DEFAULT_GXQUEUE_SIZE, true);
    C3D_FrameRate(FRAME_RATE);

    // We do init before stereoscopy to fix a crash caused by
    // VRAM alloc when the game is booted with 3D enabled.
    initialize_screens();
    update_stereoscopy();
    reconfigure_screens(true);

    // A default shader is required for many context-dependent actions.
    // We won't be drawing with it, so don't allocate a buffer.
    default_program = citro3d_helpers_init_shader(&emu64_shader_7, NULL, 0);
    C3D_BindProgram(&default_program.pica_shader_program);
    
    // Prep viewport settings
    for (uint32_t i = 0; i < GFX_C3D_VIEWPORT_COUNT; i++)
        overwrite_screen_clear(i, 0);
    
    // Initialize the components
    gfx_citro3d_emulator_init();
    gfx_citro3d_menu_init();
    
    gfx_citro3d_init_profiler();
}

void gfx_rapi_exit(void)
{
    gfx_citro3d_emulator_exit();
    gfx_citro3d_menu_exit();
    C3D_Fini();
}
