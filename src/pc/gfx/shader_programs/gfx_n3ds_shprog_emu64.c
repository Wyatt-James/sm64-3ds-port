#include <stddef.h>

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/types.h> // shbin.h forgot to include types
#include <3ds/gpu/shbin.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include "gfx_n3ds_shprog_emu64.h"
#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/gfx_3ds_shader_binaries.h"

// VBO input flag bitfields
#define V_POS  0b10000
#define V_TEX  0b01000
#define V_FOG  0b00100
#define V_COL1 0b00010
#define V_COL2 0b00001

// Bitwise AND ternary
#define BIT_TERNARY(flag_, v_, res1_, res2_) (((v_ & flag_) ? res1_ : res2_))

// Calculates the stride of a VBO, given a flag
#define VBO_STRIDE(val_) (BIT_TERNARY(V_POS,  (val_), EMU64_STRIDE_POSITION, 0) + \
                          BIT_TERNARY(V_TEX,  (val_), EMU64_STRIDE_TEXTURE,  0) + \
                          BIT_TERNARY(V_FOG,  (val_), EMU64_STRIDE_FOG,      0) + \
                          BIT_TERNARY(V_COL1, (val_), EMU64_STRIDE_RGBA,     0) + \
                          BIT_TERNARY(V_COL2, (val_), EMU64_STRIDE_RGBA,     0))

// Constructs an n3ds_shader_vbo_info, given a flag
#define VBO_INFO(val_) {BIT_TERNARY(V_POS,  (val_), true, false), \
                        BIT_TERNARY(V_TEX,  (val_), true, false), \
                        BIT_TERNARY(V_FOG,  (val_), true, false), \
                        BIT_TERNARY(V_COL1, (val_), true, false), \
                        BIT_TERNARY(V_COL2, (val_), true, false), \
                        VBO_STRIDE(val_)}

enum n3ds_shader_emu64_dvle_index { 
   DVLE_01 = 0,
// DVLE_03,
   DVLE_04,
   DVLE_05,
// DVLE_06,
// DVLE_07,
   DVLE_08,
   DVLE_09,
   DVLE_20,
   DVLE_41
};

struct n3ds_emu64_uniform_locations
   emu64_uniform_locations = { -1, -1, -1, -1, -1 };

struct n3ds_shader_binary
    emu64_shader_binary = { emu64_shbin, 0, NULL, NULL };

const struct n3ds_shader_info
    emu64_shader_1        = { &emu64_shader_binary, DVLE_01,       1, VBO_INFO(V_POS | V_TEX)                   }, // position, texture
 // emu64_shader_3        = { &emu64_shader_binary, DVLE_03,       2, VBO_INFO(V_POS | V_TEX | V_FOG)           }, // position, texture, fog
    emu64_shader_4        = { &emu64_shader_binary, DVLE_04,       3, VBO_INFO(V_POS | V_COL1)                  }, // position, color rgba
    emu64_shader_5        = { &emu64_shader_binary, DVLE_05,       4, VBO_INFO(V_POS | V_TEX | V_COL1)          }, // position, texture, color rgba
 // emu64_shader_6        = { &emu64_shader_binary, DVLE_06,       5, VBO_INFO(V_POS | V_FOG | V_COL1)          }, // position, fog, color rgba
 // emu64_shader_7        = { &emu64_shader_binary, DVLE_07,       6, VBO_INFO(V_POS | V_TEX | V_FOG | V_COL1)  }, // position, texture, fog, color rgba
    emu64_shader_8        = { &emu64_shader_binary, DVLE_08,       7, VBO_INFO(V_POS | V_COL1)                  }, // position, color rgb
    emu64_shader_9        = { &emu64_shader_binary, DVLE_09,       8, VBO_INFO(V_POS | V_TEX | V_COL1)          }, // position, texture, color rgb
    emu64_shader_20       = { &emu64_shader_binary, DVLE_20,       9, VBO_INFO(V_POS | V_COL1 | V_COL2)         }, // position, 2 colors rgba
    emu64_shader_41       = { &emu64_shader_binary, DVLE_41,      10, VBO_INFO(V_POS | V_TEX | V_COL1 | V_COL2) }; // position, texture, 2 colors rgb

const struct n3ds_shader_info* const shaders[] = {
    &emu64_shader_1,
//  &emu64_shader_3,
    &emu64_shader_4,
    &emu64_shader_5,
//  &emu64_shader_6,
//  &emu64_shader_7,
    &emu64_shader_8,
    &emu64_shader_9,
    &emu64_shader_20,
    &emu64_shader_41
};

void gfx_3ds_shprog_emu64_init()
{
    emu64_shader_binary.size = emu64_shbin_size;
    emu64_shader_binary.dvlb = DVLB_ParseFile((__3ds_u32*)emu64_shader_binary.data, emu64_shader_binary.size);
    emu64_shader_binary.uniform_locations_size = sizeof(emu64_uniform_locations);
    emu64_shader_binary.uniform_locations = (void*) &emu64_uniform_locations;

    DVLE_s* dvle = &emu64_shader_binary.dvlb->DVLE[0]; // The DVLB has a shared uniform space.
    
    emu64_uniform_locations.projection_mtx      = DVLE_GetUniformRegister(dvle, "projection_mtx");
    emu64_uniform_locations.model_view_mtx      = DVLE_GetUniformRegister(dvle, "model_view_mtx");
    emu64_uniform_locations.game_projection_mtx = DVLE_GetUniformRegister(dvle, "game_projection_mtx");
    emu64_uniform_locations.tex_settings_1      = DVLE_GetUniformRegister(dvle, "tex_settings_1");
    emu64_uniform_locations.tex_settings_2      = DVLE_GetUniformRegister(dvle, "tex_settings_2");
    // emu64_uniform_locations.draw_fog         = DVLE_GetUniformRegister(dvle, "draw_fog");
}
