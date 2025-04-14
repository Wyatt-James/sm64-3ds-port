#pragma once

/*
 * Citro3D drawing and initialization functions for the touch-screen menu
 */

#include "src/pc/click_menu.h"

void draw_button_basic(Click_Button* button, void* common_data);  // Draws a button with a WrappedTexture* graphic
void draw_button_single(Click_Button* button, void* common_data); // Draws a button with a AutoAddr_Single* graphic
void draw_button_bool(Click_Button* button, void* common_data);   // Draws a button with a AutoAddr_Bool* graphic
void draw_button_enum(Click_Button* button, void* common_data);   // Draws a button with a AutoAddr_Enum* graphic
void draw_button_auto(Click_Button* button, void* common_data);   // Draws a button with a AutoAddr* graphic

void gfx_citro3d_menu_init(void); // Initializes the graphics for the menu.
void gfx_citro3d_menu_exit(void); // Deinitializes the graphics for the menu.
void gfx_citro3d_menu_on_draw_start(void);
void gfx_citro3d_menu_on_draw_finish(void);
