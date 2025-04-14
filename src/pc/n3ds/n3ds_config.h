#pragma once

/*
 * N3DS-specific configuration
 */

#include "n3ds_hid.h"
#include "src/pc/pc_macros.h"

typedef struct
{
    bool show_menu,
         use_aa,
         use_wide;
} N3DS_MenuConfig;

typedef enum
{
    N3DS_SCREEN_NONE   = 0,
    N3DS_SCREEN_TOP    = BIT(0),
    N3DS_SCREEN_BOTTOM = BIT(1),
    N3DS_SCREEN_BOTH   = N3DS_SCREEN_TOP | N3DS_SCREEN_BOTTOM,
} N3DS_Screen;

typedef struct
{
    bool run,                        // Set to false to exit the game.
         vram_framebuffers,          // Gives a nice performance boost.
         vram_textures,              // Gives a nice performance boost, but not currently supported.
         enable_new_3ds_speedup,     // Ignored on old models.
         print_menu_interactions,    // If enabled, touch menu interactions are printed.
         enable_multi_threading;     // If enabled, audio runs on a separate core.
    uint8_t syscore_limit,           // Limit for syscore while the game is running. Can be [10-80].
            syscore_limit_idle;      // Limit for syscore while the game is suspended. Can be [10-80].
    N3DS_Screen console_screen;      // Which screens get a console. Only one can be used at a time. Ignored if VRAM framebuffers are enabled. Warning: will change framebuffer format to RGB565!
    N3DS_HidPollMode input_poll_mode;
} N3DSConfig;

extern N3DSConfig g3dsConfig;             // General options
extern N3DS_MenuConfig g3dsMenuConfig; // Touch screen menu options
