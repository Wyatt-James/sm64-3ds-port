#pragma once

/*
 * N3DS-specific configuration
 */

#include "libctru_inc.h"
#include "n3ds_hid.h"
#include "src/pc/pc_macros.h"

typedef enum
{
    N3DS_SCREEN_NONE   = 0,
    N3DS_SCREEN_TOP    = BIT(0),
    N3DS_SCREEN_BOTTOM = BIT(1),
    N3DS_SCREEN_BOTH   = N3DS_SCREEN_TOP | N3DS_SCREEN_BOTTOM,
} N3DS_Screen;

// General-purpose configurations. Values marked read-only have no effect after boot.
typedef struct
{
    bool run                           : 1,  // Set to false to exit the game.
         vram_framebuffers             : 1,  // Read-only. Gives a nice performance boost.
         vram_textures                 : 1,  // Read-only. Gives a nice performance boost.
         enable_new_3ds_speedup        : 1,  // Read-only. Ignored on old models.
         print_menu_interactions       : 1,  // If enabled, touch menu interactions are printed.
         enable_multi_threading        : 1,  // Read-only. Global toggle for multi-threading.
         enable_audio_thread           : 1,  // Read-only. If enabled, audio runs on a separate thread.
         enable_async_thread           : 1,  // Read-only. If enabled, misc tasks run asynchronously.
         level_script_waits_for_audio  : 1;  // If enabled, the level script will always wait for the audio thread before running.
    uint8_t syscore_limit,                   // Limit for syscore while the game is running. Can be [10-80].
            syscore_limit_idle;              // Limit for syscore while the game is suspended. Can be [10-80].
    N3DS_Screen console_screen;              // Read-only. Which screens get a console. Only one can be used at a time. Ignored if VRAM framebuffers are enabled. Warning: will change framebuffer format to RGB565!
    N3DS_HidPollMode input_poll_mode;        // Determines how often HID is polled.
    __3ds_s32 desired_main_thread_priority,  // Desired priority of the main thread.
              desired_audio_thread_priority, // Desired priority of the audio thread, if enabled.
              desired_async_thread_priority; // Desired priority of the async thread, if enabled.
} N3DS_Config;

// Configuration for the menu. For changes to appear, g3dsGfxState.bottom_screen_needs_render must be set.
typedef struct
{
    bool show_menu, // If true, the touch-screen menu will display the configuration buttons.
         use_aa,    // If enabled, the top screen will attempt to use anti-aliasing. Not currently supported on Old 2DS.
         use_wide;  // If enabled, the top screen will attempt to use 800px width. Not supported on Old 2DS.
} N3DS_MenuConfig;

extern N3DS_Config g3dsConfig;         // General options
extern N3DS_MenuConfig g3dsMenuConfig; // Touch-screen menu options
