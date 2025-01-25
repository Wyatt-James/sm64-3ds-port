#pragma once

#include "gfx_citro3d_defines.h"
#include "src/pc/bit_flag.h"

#define BSWAP32(v_) (__builtin_bswap32(v_))
#define ALIGNED32 __attribute__((aligned(32)))

#define RFLAG_ON(flag_)    FLAG_ON(render_state.flags,    flag_)
#define RFLAG_SET(flag_)   FLAG_SET(render_state.flags,   flag_)
#define RFLAG_CLEAR(flag_) FLAG_CLEAR(render_state.flags, flag_)
