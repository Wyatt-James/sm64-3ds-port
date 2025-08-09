#include "n3ds_main.h"

#include <stdio.h>

#include "n3ds_hid.h"
#include "n3ds_apt_hook.h"
#include "n3ds_system_info.h"
#include "n3ds_config.h"
#include "n3ds_threading.h"
#include "src/pc/audio/audio_3ds_threading.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "n3ds_menu.h"

#include "src/pc/profiler_3ds.h"

static void set_up_threading() {
    __3ds_s32 main_priority = g3dsConfig.desired_main_thread_priority;

    // Set main thread priority to desired value.
    if (R_SUCCEEDED(svcSetThreadPriority(CUR_THREAD_HANDLE, main_priority)))
        fprintf(stdout, "Set main thread priority to 0x%lx.\n", main_priority);
    else
        fprintf(stderr, "Couldn't set main thread priority to 0x%lx.\n", main_priority);

    // We only really need the one extra core.
    if (!g3dsSystemInfo.is_new_3ds && g3dsConfig.enable_multi_threading)
        n3ds_enable_old_core_1();

    // Set desired thread constants
    if (g3dsConfig.enable_multi_threading) {
        if (g3dsSystemInfo.is_new_3ds)
            n3ds_desired_audio_cpu = NEW_CORE_2; // n3ds 3rd core
        else if (n3ds_old_core_1_is_available)
            n3ds_desired_audio_cpu = OLD_CORE_1; // o3ds 2nd core
        else
            n3ds_desired_audio_cpu = OLD_CORE_0; // Run in Thread5
    } else
        n3ds_desired_audio_cpu = OLD_CORE_0; // Run in Thread5
}

/* Order:
 * main_func
 *    n3ds_main_init
 *       n3ds_system_info_init
 *       set_up_threading
 *          n3ds_enable_old_core_1 if old 3DS and MT is enabled
 *       osSetSpeedupEnable if new 3DS
 *       n3ds_handle_events
 *       profiler_3ds_init
 *       n3ds_menu_init
 *    gfx_init
 *       gfx_wapi->
 *          gfxInit
 *          clear_vram
 *          init_console if enabled
 *       gfx_rapi_init
 *          C3D_InitEx
 *          reinitialize_screens
 *       <RSP init>
 *    audio_3ds.init
 * wm_api->main_loop
 *    n3ds_apt_hook_init
 *    loop:
 *       aptMainLoop
 *       produce_one_frame
 *          gfx_start_frame
 *             gfx_wapi->handle_events
 *               n3ds_handle_events
 *                  n3ds_hid_start_frame
 *                  n3ds_menu_handle_touch
 *          game_loop_one_iteration
 *             level_script_execute
 *                N3DS synchronize with audio thread (if running MT audio)
 *                <execute level script commands>
 *                audio_3ds_run_one_frame OR queue N3DS audio frame (ST vs MT audio)
 *             display_and_vsync
 *                send_display_list
 *                   gfx_run
 *                      gfx_rapi_start_frame
 *                         C3D_FrameStart
 *                   gfx_run_dl
 *                   gfx_rapi_end_frame
 *                      n3ds_menu_render
 *                      C3D_FrameEnd -> handles vsync and present asynchronously
 *          gfx_end_frame
 *             gfx_rapi_finish_render (nothing on n3ds)
 *             gfx_wapi->swap_buffers_end (nothing on n3ds)
 *    n3ds_apt_hook_exit
 *    C3D_Fini
 *    gfxExit
 * atexit: save config file
 */
void n3ds_main_loop(void (*run_one_game_iter)(void))
{
    n3ds_apt_hook_init();
    aptSetSleepAllowed(true);

    while (aptMainLoop() && g3dsConfig.run)
    {
        if (!n3ds_apt_suspended) {
            profiler_3ds_advance_frame();
            run_one_game_iter();
            profiler_3ds_snoop(0);
        } else
            N3DS_SLEEP_FUNC(N3DS_MILLIS_TO_NANOS(33));
    }

    aptSetSleepAllowed(false);
    n3ds_apt_hook_exit();
}

void n3ds_handle_events(void)
{
    n3ds_hid_start_frame();
    n3ds_menu_handle_touch();
}

void n3ds_main_init(void)
{
    n3ds_system_info_init();
    set_up_threading();

    if(g3dsSystemInfo.is_new_3ds && g3dsConfig.enable_new_3ds_speedup)
        osSetSpeedupEnable(true);

    n3ds_handle_events();
    profiler_3ds_init();
    n3ds_menu_init();
}
