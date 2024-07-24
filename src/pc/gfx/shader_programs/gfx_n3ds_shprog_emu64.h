#ifndef GFX_N3DS_SHPROG_EMU64_H
#define GFX_N3DS_SHPROG_EMU64_H

#include "src/pc/gfx/gfx_3ds_shaders.h"

/*
 * A set of shaders for emulating the N64.
 */

// Stride values for specific inputs. Unit is one word (uint32_t)
#define EMU64_STRIDE_RGBA     1
#define EMU64_STRIDE_RGB      1
#define EMU64_STRIDE_POSITION 2
#define EMU64_STRIDE_TEXTURE  2
#define EMU64_STRIDE_FOG      EMU64_STRIDE_RGBA

// Maximum possible stride
#define EMU64_STRIDE_MAX      (EMU64_STRIDE_POSITION    \
                            +  EMU64_STRIDE_TEXTURE     \
                         /* +  EMU64_STRIDE_FOG */      \
                            + (EMU64_STRIDE_RGBA * 2))

struct n3ds_emu64_uniform_locations {
   int projection_mtx,
       model_view_mtx,
       game_projection_mtx,
       tex_scale;
   // int draw_fog;
};

extern struct n3ds_emu64_uniform_locations
   emu64_uniform_locations;

extern struct n3ds_shader_binary
   emu64_shader_binary;

extern const struct n3ds_shader_info
   emu64_shader_1,
// emu64_shader_3,
   emu64_shader_4,
   emu64_shader_5,
// emu64_shader_6,
// emu64_shader_7,
   emu64_shader_8,
   emu64_shader_9,
   emu64_shader_20,
   emu64_shader_41;

void gfx_3ds_shprog_emu64_init();

#endif
