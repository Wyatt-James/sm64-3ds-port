#include "n3ds_config.h"

N3DS_Config g3dsConfig = {
    .run                           = true,
    .vram_framebuffers             = true,
    .vram_textures                 = true,
    .enable_new_3ds_speedup        = true,
    .print_menu_interactions       = false,
    .enable_multi_threading        = true,
    .enable_audio_thread           = true,
    .enable_async_thread           = true,
    .level_script_waits_for_audio  = true,
    .syscore_limit                 = 80,
    .syscore_limit_idle            = 10,
    .console_screen                = N3DS_SCREEN_NONE,
    .input_poll_mode               = N3DS_INPUT_START_OF_FRAME,
    .desired_main_thread_priority  = 0x30,
    .desired_audio_thread_priority = 0x30 - 1,
    .desired_async_thread_priority = 0x30 + 1, // Bigger number is lower priority
};

N3DS_MenuConfig g3dsMenuConfig = {
    .show_menu = false,
    .use_aa    = false,
    .use_wide  = false,
};
