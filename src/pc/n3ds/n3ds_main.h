#pragma once

/*
 * Contains the N3DS main loop and some windowing API functions.
 */

void n3ds_handle_events(void);
void n3ds_main_loop(void (*run_one_game_iter)(void));
void n3ds_main_init(void);
