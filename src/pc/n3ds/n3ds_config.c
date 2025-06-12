#include "n3ds_config.h"

N3DSConfig g3dsConfig = {
    .run                     = true,
    .vram_framebuffers       = true,
    .vram_textures           = true,
    .enable_new_3ds_speedup  = true,
    .print_menu_interactions = false,
    .syscore_limit           = 80,
    .syscore_limit_idle      = 10,
    .console_screen          = N3DS_SCREEN_NONE,
    .input_poll_mode         = N3DS_INPUT_START_OF_FRAME,
    .enable_multi_threading  = true,
};

N3DS_MenuConfig g3dsMenuConfig = {
    .show_menu = false,
    .use_aa    = false,
    .use_wide  = false,
};
