#pragma once

/*
 * Wrapper functions for libctru's gfx.h/c. This mostly exists to work around a messy API.
 */

#include "src/pc/n3ds/libctru_inc.h"

// Copy of gfxTopMode from gfx.c. The API is a mess.
typedef enum 
{
	MODE_2D   = 0, // 2D
	MODE_3D   = 1, // 3D
	MODE_WIDE = 2, // 2D doublewide
} N3DS_TopScreenMode;

// A wrapper for libctru's gfxSet3D and gfxSetWide
static inline void gfxWSetTopMode(N3DS_TopScreenMode mode)
{
    switch(mode)
    {
        default:        // Same as 2D 400px
        case MODE_2D:   gfxSet3D(false);  break;
        case MODE_3D:   gfxSet3D(true);   break;
        case MODE_WIDE: gfxSetWide(true); break;
    }
}
