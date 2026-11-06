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
#include "src/pc/n3ds/n3ds_config.h"
#include "src/pc/time.h"
#include "src/pc/pc_macros.h"

#define DEFAULT_GXQUEUE_SIZE 32 // This is the default used by C3D.

#ifdef VERSION_EU
#define FRAME_RATE 25
#else
#define FRAME_RATE 30
#endif

USED C3D_GpuCmdBuffer prev_cmd_buffer;
__3ds_u32 gpuRegs[0x100 / sizeof(__3ds_u32)];
Result gpuRegResults[ARRAY_COUNT(gpuRegs)];
bool gpuRegsFetched;

static ShaderProgram default_program;

static Thread watchdog_thread;
static volatile enum {COMPLETE, TIMEOUT} lastQueue = COMPLETE;

#define ADJ(v_) ((__3ds_u32) (v_) + (0x1EF00000 - 0x1EB00000))
#define REG(v_) (gpuRegs[(v_ / sizeof(__3ds_u32))])
#define RES(v_) (gpuRegResults[(v_ / sizeof(__3ds_u32))])

static void timeout_print_gpu_regs(UNUSED void* unused)
{
    for (__3ds_u32 reg = 0; reg < ARRAY_COUNT(gpuRegs); reg++) {
        gpuRegResults[reg] = GSPGPU_ReadHWRegs((ADJ(reg * 4)), &gpuRegs[reg], sizeof(gpuRegs[0]));
    }

    gpuRegsFetched = true;

    if (g3dsConfig.vram_framebuffers) {
        g3dsConfig.vram_framebuffers = false;
        g3dsConfig.console_screen = N3DS_SCREEN_NONE; // Force console reinit
        gfx_3ds_reinitialize();
    }

    if (g3dsConfig.console_screen == N3DS_SCREEN_NONE) {
        g3dsConfig.console_screen = N3DS_SCREEN_BOTTOM;
        gfx_3ds_init_console();
    }
    printf("GPU freeze? GX timed out in %llu ms\nHere are the first 0x100 GPU registers:\n", N3DS_NANOS_TO_MILLIS(C3D_GetTimeoutDuration()));
    
    // Console is 40 columns
    for (size_t i = 0; i < ARRAY_COUNT(gpuRegs); i++) {
        Result r = gpuRegResults[i];
        __3ds_u32 val = gpuRegs[i];
        __3ds_u32 regNum = i * 4;
        
        if (i % 4 == 0)
            printf("%02lx: ", regNum); // 4 columns

        if (R_SUCCEEDED(r))
            printf("%08lx ", val); // 9 columns
        else
            printf("--%03lx--- ", R_DESCRIPTION(r));
        
        // if (i % 4 == 3)     we get a double newline because we printed
        //     printf("\n");   exactly 40 chars and libctru console is buggy
    }

    printf("\nHold the power button to quit.\n");
    NOP; // Breakpoint

    // TODO: launch into Doom (1993)
}

static void watchdog_handler(UNUSED void* unused)
{
    uint32_t timer = 38273024; // On the order of 1 second. This number can be an ARM immediate.
    if (g3dsConfig.enable_new_3ds_speedup) // Rough compensation for 3x clock
        timer *= 4;

    // This must be a software timer because hardware timers keep running while
    // the Rosalina menu is open. That could cause false positives but this doesn't.
    while (timer--) {
        if (lastQueue == COMPLETE) {
            printf("Watchdog recovered from GX hang\n");
            return;
        }
    }

    timeout_print_gpu_regs(NULL);
}

static void timeout_hook(UNUSED void* unused)
{
    if (lastQueue == COMPLETE) {
        lastQueue = TIMEOUT;
        printf("GX hang detected, starting watchdog\n");

        if (watchdog_thread)
            threadJoin(watchdog_thread, U64_MAX);

        watchdog_thread = threadCreate(watchdog_handler, NULL, 0x100, 0x3f, -2, true);
    }
}

void gfx_rapi_start_frame(void)
{
    // if we just enabled or disabled stereo 3D, reinitialize the top screen
    bool cur_on  = n3ds_hid_3d_slider() > 0.0f,
         prev_on = n3ds_hid_prev_3d_slider() > 0.0f;
    if (cur_on != prev_on)
        update_stereoscopy();
    
    reconfigure_screens(false);
    
    // Must occur after screen init
    C3D_FrameBegin(0);

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
    profiler_3ds_log_time(0);
    C3D_FrameSync();
    profiler_3ds_log_time(4); // VSync
    C3D_FrameEnd(0);

    // Must be after C3D waits for the GX queue.
    // FrameEnd for 2-queue, FrameBegin for 1-queue.
    lastQueue = COMPLETE;

    prev_cmd_buffer = C3D_GetLastGpuCmdBuffer();
    NOP;
}

void gfx_rapi_init(void)
{
    C3D_InitEx(C3D_DEFAULT_CMDBUF_SIZE, DEFAULT_GXQUEUE_SIZE, true);
    C3D_FrameRate(FRAME_RATE);

    C3D_SetTimeoutDuration(N3DS_SECONDS_TO_NANOS(g3dsConfig.gx_timeout_duration));
    C3D_TimeoutHook(timeout_hook, NULL);

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
    lastQueue = COMPLETE;
    if (watchdog_thread) {
        threadJoin(watchdog_thread, U64_MAX);
        watchdog_thread = NULL;
    }

    gfx_citro3d_emulator_exit();
    gfx_citro3d_menu_exit();
    C3D_Fini();
}
