#pragma once

#include <stdint.h>
#include <stddef.h>

#include "gfx_citro3d_defines.h"
#include "gfx_citro3d_macros.h"
#include "gfx_citro3d_types.h"

extern struct TexHandle texture_pool[TEXTURE_POOL_SIZE];
extern struct TexHandle* current_texture;
