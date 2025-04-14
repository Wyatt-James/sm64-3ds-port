#pragma once

/*
 * The main RSP emulation file for the Citro3D rendering API.
 */

#define NUM_MATRIX_SETS 8
#define DEFAULT_MATRIX_SET 0

void gfx_citro3d_emulator_init(void);
void gfx_citro3d_emulator_start_frame(void);
void gfx_citro3d_emulator_end_frame(void);
void gfx_citro3d_emulator_init(void);
void gfx_citro3d_emulator_exit(void);
