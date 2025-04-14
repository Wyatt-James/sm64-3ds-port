#pragma once

/*
 * N3DS APT hook functionality
 */

#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"

extern bool n3ds_apt_suspended;

void n3ds_apt_hook_init(void);
void n3ds_apt_hook_exit(void);
