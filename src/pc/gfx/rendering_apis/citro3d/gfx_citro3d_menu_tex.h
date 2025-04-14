#pragma once

/*
 * A file for initializing the touch-screen menu's textures.
 */

#include "src/pc/n3ds/c3d_inc.h"

typedef enum
{
    MENU_TEX_MODE_400,
    MENU_TEX_MODE_800,
    MENU_TEX_AA_ON,
    MENU_TEX_AA_OFF,
    MENU_TEX_RESUME,
    MENU_TEX_EXIT,
    MENU_TEX_CLEFT,
    MENU_TEX_CRIGHT,
    MENU_TEX_CDOWN,
    MENU_TEX_CUP,
    MENU_TEX_FULLSCREEN_TEST,
    MENU_TEX_FULLSCREEN_TEST_PADDED,
    MENU_TEX_COUNT,
} MenuTexId;

typedef struct
{
    C3D_Tex c3d_tex;
    float size_s, size_t;     // Top-right corner - bottom-left corner
    float origin_s, origin_t; // Bottom-left corner
} WrappedTexture;

extern WrappedTexture mode_400_tex,
                      mode_800_tex,
                      aa_on_tex,
                      aa_off_tex,
                      hide_menu_tex,
                      exit_tex,
                      menu_cleft_tex,
                      menu_cright_tex,
                      menu_cdown_tex,
                      menu_cup_tex,
                      fullscreen_test_tex,
                      fullscreen_test_padded_tex;

void gfx_citro3d_menu_init_tex_single(MenuTexId id);  // Initialize a single texture
void gfx_citro3d_menu_init_tex_release();             // Initialize all textures, excluding debug
void gfx_citro3d_menu_init_tex_all();                 // Initialize all textures, including debug
