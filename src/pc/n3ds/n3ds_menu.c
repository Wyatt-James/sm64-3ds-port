#include "n3ds_menu.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <PR/os_cont.h>

#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"                  // Globals
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_menu.h"     // Rendering functions
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_menu_tex.h" // Textures
#include "src/pc/n3ds/n3ds_system_info.h"
#include "src/pc/n3ds/n3ds_config.h"
#include "src/pc/n3ds/n3ds_hid.h"
#include "src/pc/controller/controller_3ds.h"
#include "src/pc/click_menu.h"
#include "src/pc/pc_macros.h"
#include "src/pc/auto_addr.h"

// Forward declarations
static void redraw(void);
static void show_menu(Click_Button* button);
static void toggle_aa(Click_Button* button);
static void toggle_width(Click_Button* button);
static void hide_menu(Click_Button* button);
static void redraw_wrapper(Click_Button* button);
static void switch_menu(Click_Button* button);
static void exit_game(Click_Button* button);
static void press_c_button(Click_Button* button);
static void release_c_button(Click_Button* button);

#define MENU(buttons_) (N3DS_Menu) {.num_buttons = ARRAY_COUNT(buttons_), .buttons = buttons_}
#define BUTTON(x_, y_, w_, h_, graphic_, draw_func_, slide_onto_func_, slide_off_func_, release_func_, group_, custom_data_, name_) \
 (Click_Button) {                                                                                                                   \
    .position        = {.x = x_, .y = y_, .w = w_, .h = h_},                                                                        \
    .capture_mode    = CLICK_CAPTURE,                                                                                               \
    .render_groups   = group_,                                                                                                      \
    .click_groups    = group_,                                                                                                      \
    .graphic         = (void*) graphic_,                                                                                            \
    .on_press        = NULL,                                                                                                        \
    .on_slide_onto   = slide_onto_func_,                                                                                            \
    .on_hold         = NULL,                                                                                                        \
    .on_slide_off    = slide_off_func_,                                                                                             \
    .on_release      = release_func_,                                                                                               \
    .render          = draw_func_,                                                                                                  \
    .custom_data.u32 = custom_data_,                                                                                                \
    .name            = name_                                                                                                        \
}
/*                                                                                                                                                    draw        slide on        slide off        release                                   */
#define S_BUTTON(x_, y_, w_, h_, graphic_, draw_func_, slide_on_func_, slide_off_func_, custom_data_, group_, name_) BUTTON(x_, y_, w_, h_, graphic_, draw_func_, slide_on_func_, slide_off_func_, NULL,           group_, custom_data_, name_)
#define R_BUTTON(x_, y_, w_, h_, graphic_, draw_func_, release_func_,                   custom_data_, group_, name_) BUTTON(x_, y_, w_, h_, graphic_, draw_func_, redraw_wrapper, redraw_wrapper,  release_func_,  group_, custom_data_, name_)
#define LABEL(   x_, y_, w_, h_, graphic_, draw_func_,                                  custom_data_, group_, name_) BUTTON(x_, y_, w_, h_, graphic_, draw_func_, NULL,           NULL,            NULL,           group_, custom_data_, name_)

enum
{
    GROUP_C_BUTTONS          = BIT(0),
    GROUP_CONFIGS_OTHER      = BIT(1),
    GROUP_CONFIGS_DISPLAY    = BIT(2),
    GROUP_CONFIG_AREA        = BIT(3),

    GROUP_CONFIGS_800PX      = GROUP_CONFIGS_OTHER | GROUP_CONFIGS_DISPLAY,
    GROUP_ALL                = (GROUP_CONFIG_AREA << 1) - 1
};

enum
{
    GROUP_EXIT_BUTTON           = BIT(0),
    GROUP_EXIT_NOT_INTERACTABLE = BIT(1),

    GROUP_EXIT_ALL              = (GROUP_EXIT_NOT_INTERACTABLE << 1) - 1,
};

enum
{
    MENU_MAIN, // Config and c-buttons
    MENU_EXIT, // Exit confirm/deny
};

typedef struct
{
    size_t num_buttons;
    Click_Button* buttons;
} N3DS_Menu;

static AutoAddr autoaddr_toggle_aa    = AUTOADDR_BOOL(&g3dsMenuConfig.use_aa,   &aa_off_tex,   &aa_on_tex),
                autoaddr_toggle_width = AUTOADDR_BOOL(&g3dsMenuConfig.use_wide, &mode_400_tex, &mode_800_tex);

static Click_Button main_menu_buttons[] = {
    //         X    Y    W    H  Graphic                 Draw Function      Action          Slide Off         Custom      Group                  Name
    R_BUTTON(  0,   0, 160, 240, NULL,                   draw_button_basic,                 show_menu,                 0, GROUP_CONFIG_AREA,     "Show Menu"),
    R_BUTTON( 11,  32,  64,  64, &autoaddr_toggle_aa,    draw_button_auto,                  toggle_aa,                 0, GROUP_CONFIGS_DISPLAY, "Toggle AA"),
    R_BUTTON( 86,  32,  64,  64, &autoaddr_toggle_width, draw_button_auto,                  toggle_width,              0, GROUP_CONFIGS_DISPLAY, "Toggle Width"),
    R_BUTTON( 11, 144,  64,  64, &hide_menu_tex,         draw_button_basic,                 hide_menu,                 0, GROUP_CONFIGS_OTHER,   "Hide Menu"),
    R_BUTTON( 86, 144,  64,  64, &exit_tex,              draw_button_basic,                 switch_menu,       MENU_EXIT, GROUP_CONFIGS_OTHER,   "Exit Game"),
    S_BUTTON(170, 122,  64,  64, &menu_cleft_tex,        draw_button_basic, press_c_button, release_c_button, L_CBUTTONS, GROUP_C_BUTTONS,       "C-Left"),
    S_BUTTON(245, 122,  64,  64, &menu_cright_tex,       draw_button_basic, press_c_button, release_c_button, R_CBUTTONS, GROUP_C_BUTTONS,       "C-Right"),
    S_BUTTON(207, 197,  64,  32, &menu_cdown_tex,        draw_button_basic, press_c_button, release_c_button, D_CBUTTONS, GROUP_C_BUTTONS,       "C-Down"),
    S_BUTTON(207,  79,  64,  32, &menu_cup_tex,          draw_button_basic, press_c_button, release_c_button, U_CBUTTONS, GROUP_C_BUTTONS,       "C-Up"),
};

static Click_Button exit_menu_buttons[] = {
    //         X     Y       W       H   Graphic           Draw Function      Action       Extra Data  Group  Name
    R_BUTTON( 24,  84,  64 * 2, 64 * 2,  &exit_tex,            draw_button_basic, exit_game,            0,    GROUP_EXIT_BUTTON,           "Exit Confirm"),
    R_BUTTON(168,  84,  64 * 2, 64 * 2,  &exit_cancel_tex,     draw_button_basic, switch_menu,  MENU_MAIN,    GROUP_EXIT_BUTTON,           "Exit Cancel"),
    LABEL(    75,  20,  57 * 3, 12 * 3,  &exit_menu_label_tex, draw_button_basic,                       0,    GROUP_EXIT_NOT_INTERACTABLE, "Exit Label"),
};

static N3DS_Menu menus[] = {
    [MENU_MAIN] = MENU(main_menu_buttons),
    [MENU_EXIT] = MENU(exit_menu_buttons),
};

static uint8_t current_menu;

static void redraw(void)
{
    g3dsGfxState.bottom_screen_needs_render = true;
}

static void show_menu(UNUSED Click_Button* button)
{
    g3dsMenuConfig.show_menu = true;
}

static void hide_menu(UNUSED Click_Button* button)
{
    g3dsMenuConfig.show_menu = false;
}

static void switch_menu(Click_Button* button)
{
    current_menu = (uint8_t) button->custom_data.u32;
    if (current_menu > ARRAY_COUNT(menus) - 1)
        current_menu = MENU_MAIN;
}

static void exit_game(UNUSED Click_Button* button)
{
    g3dsConfig.run = false;
}

// WYATT_TODO replace this. Compatible consoles should be able to select all modes.
static void toggle_aa(UNUSED Click_Button* button)
{
    if (!g3dsGfxState.stereo_3d_active)
    {
        g3dsGfxState.reinitialize_top_screen = true;
        g3dsMenuConfig.use_aa = !g3dsMenuConfig.use_aa;

        // Old 2DS cannot use wide without AA
        if (!g3dsMenuConfig.use_aa && !g3dsSystemInfo.supports_800px)
            g3dsMenuConfig.use_wide = false;
    }
}

static void toggle_width(UNUSED Click_Button* button)
{
    if (!g3dsGfxState.stereo_3d_active)
    {
        // Old 2DS cannot use wide without AA
        if (!g3dsSystemInfo.supports_800px && !g3dsMenuConfig.use_aa)
            return;

        g3dsGfxState.reinitialize_top_screen = true;
        g3dsMenuConfig.use_wide = !g3dsMenuConfig.use_wide;
    }
}

static void redraw_wrapper(UNUSED Click_Button* button)
{
    redraw();
}

static void press_c_button(Click_Button* button)
{
    redraw();
    controller_3ds_force_hold |= button->custom_data.u32;
}

static void release_c_button(Click_Button* button)
{
    redraw();
    controller_3ds_force_hold &= ~(u16) button->custom_data.u32;
}

void n3ds_menu_handle_touch()
{
    touchPosition* pos = &n3ds_hid_touch()->position;
    touchPosition* pos_prev = &n3ds_hid_prev_touch()->position;
    bool touched = pos->px || pos->py;
    bool touched_prev = pos_prev->px || pos_prev->py;

    Click_Flags interaction_flags;
    
    switch (current_menu) {
        case MENU_MAIN:
            interaction_flags = GROUP_C_BUTTONS;

            if (g3dsMenuConfig.show_menu)
                interaction_flags |= GROUP_CONFIGS_800PX;
            else
                interaction_flags |= GROUP_CONFIG_AREA;
            break;

        case MENU_EXIT:
            interaction_flags = GROUP_EXIT_BUTTON;
            break;
    }
        
    Click_Action actions;
    Click_Point point;
    N3DS_Menu* menu = &menus[current_menu];

    if (touched)
    {
        actions = CLICK_ACTION_SLIDE_ON | CLICK_ACTION_SLIDE_OFF;
        point = (Click_Point) {.x = pos->px, .y = pos->py};
        click_evaluate_single(menu->num_buttons, menu->buttons, point, interaction_flags, actions, g3dsConfig.print_menu_interactions);
    }
    else if (touched_prev)
    {
        actions = CLICK_ACTION_RELEASE | CLICK_ACTION_SLIDE_OFF;
        point = (Click_Point) {.x = -100, .y = -100};
        click_evaluate_single(menu->num_buttons, menu->buttons, point, interaction_flags, actions, g3dsConfig.print_menu_interactions);
    }
}

void n3ds_menu_init(void)
{
    click_init(ARRAY_COUNT(main_menu_buttons), main_menu_buttons);
    click_init(ARRAY_COUNT(exit_menu_buttons), exit_menu_buttons);
    current_menu = 0;
}

void n3ds_menu_render(void)
{
    if (!g3dsGfxState.bottom_screen_needs_render)
        return;

    Click_Flags draw_flags;

    switch (current_menu) {
        case MENU_MAIN:
            draw_flags = GROUP_C_BUTTONS;
            
            if (g3dsMenuConfig.show_menu)
                draw_flags |= GROUP_CONFIGS_800PX;
            break;

        case MENU_EXIT:
            draw_flags = GROUP_EXIT_ALL;
            break;
    }

    N3DS_Menu* menu = &menus[current_menu];

    gfx_citro3d_menu_on_draw_start();
    click_render(menu->num_buttons, menu->buttons, draw_flags, NULL);
    gfx_citro3d_menu_on_draw_finish();
}
