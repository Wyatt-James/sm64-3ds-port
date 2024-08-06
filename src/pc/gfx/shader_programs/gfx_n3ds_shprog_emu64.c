#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
#define V_POS  EMU64_VBO_POSITION
#define V_TEX  EMU64_VBO_TEXTURE
#define V_COL  EMU64_VBO_COLOR

// Bitwise AND ternary
#define BIT_TERNARY(flag_, v_, res1_, res2_) (((v_ & flag_) ? res1_ : res2_))

// Calculates the stride of a VBO, given a flag
#define VBO_STRIDE(val_) (BIT_TERNARY(V_POS,  (val_), EMU64_STRIDE_POSITION,     0)    \
                        + BIT_TERNARY(V_TEX,  (val_), EMU64_STRIDE_TEXTURE,      0)    \
                        + BIT_TERNARY(V_COL,  (val_), EMU64_STRIDE_VERTEX_COLOR, 0)    \
                        )

// Constructs an n3ds_shader_vbo_info, given a flag
#define VBO_INFO(val_) {BIT_TERNARY(V_POS,  (val_), true, false),    \
                        BIT_TERNARY(V_TEX,  (val_), true, false),    \
                        BIT_TERNARY(V_COL,  (val_), true, false),    \
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
   emu64_uniform_locations = { -1, -1, -1, -1, -1, -1, { -1, -1, -1, -1 } };

struct n3ds_emu64_const_uniform_locations
   emu64_const_uniform_locations = { -1, -1, -1, -1 };
   
const struct n3ds_emu64_const_uniform_defaults 
   emu64_const_uniform_defaults = {
    .texture_const_1 = {  0.0f,    1.0f,    0.0f,     0.0f },
    .texture_const_2 = {  4.0f,   -8.0f, 1/32.0f,     0.0f },
    .cc_constants    = { -1.0f, 3000.0f,    1.0f,     0.0f },
    .emu64_const_1   = {  0.0f,    1.0f,   -1.0f, 1/255.0f }
   };

struct n3ds_shader_binary
    emu64_shader_binary = { emu64_shbin, 0, NULL };

const struct n3ds_shader_info
    emu64_shader_3        = { &emu64_shader_binary, DVLE_01, 1, VBO_INFO(V_POS | V_TEX)         }, // position, texture
    emu64_shader_5        = { &emu64_shader_binary, DVLE_04, 3, VBO_INFO(V_POS | V_COL)         }, // position, color
    emu64_shader_7        = { &emu64_shader_binary, DVLE_05, 4, VBO_INFO(V_POS | V_TEX | V_COL) }; // position, texture, color

const struct n3ds_shader_info* const shaders[] = {
    &emu64_shader_3,
    &emu64_shader_5,
    &emu64_shader_7
};

void shprog_emu64_init()
{
    emu64_shader_binary.size = emu64_shbin_size;
    emu64_shader_binary.dvlb = DVLB_ParseFile((__3ds_u32*)emu64_shader_binary.data, emu64_shader_binary.size);

    DVLE_s* dvle = &emu64_shader_binary.dvlb->DVLE[0]; // Despite the shared uniform space, we need a shader will all of the uniforms declared.
    
    memset(&emu64_uniform_locations,       -1, sizeof(emu64_uniform_locations));
    memset(&emu64_const_uniform_locations, -1, sizeof(emu64_const_uniform_locations));
    
    // Variable uniforms
    emu64_uniform_locations.projection_mtx      = DVLE_GetUniformRegister(dvle, "projection_mtx");
    emu64_uniform_locations.model_view_mtx      = DVLE_GetUniformRegister(dvle, "model_view_mtx");
    emu64_uniform_locations.game_projection_mtx = DVLE_GetUniformRegister(dvle, "game_projection_mtx");
    emu64_uniform_locations.rsp_color_selection = DVLE_GetUniformRegister(dvle, "rsp_color_selection");
    emu64_uniform_locations.tex_settings_1      = DVLE_GetUniformRegister(dvle, "tex_settings_1");
    emu64_uniform_locations.tex_settings_2      = DVLE_GetUniformRegister(dvle, "tex_settings_2");
    emu64_uniform_locations.rsp_colors[0]       = DVLE_GetUniformRegister(dvle, "rsp_colors");
    emu64_uniform_locations.rsp_colors[1]       = emu64_uniform_locations.rsp_colors[0] + 1;
    emu64_uniform_locations.rsp_colors[2]       = emu64_uniform_locations.rsp_colors[0] + 2;
    emu64_uniform_locations.rsp_colors[3]       = emu64_uniform_locations.rsp_colors[0] + 3;

    // Constant uniforms
    emu64_const_uniform_locations.texture_const_1 = DVLE_GetUniformRegister(dvle, "texture_const_1");
    emu64_const_uniform_locations.texture_const_2 = DVLE_GetUniformRegister(dvle, "texture_const_2");
    emu64_const_uniform_locations.cc_constants    = DVLE_GetUniformRegister(dvle, "cc_constants");
    emu64_const_uniform_locations.emu64_const_1   = DVLE_GetUniformRegister(dvle, "emu64_const_1");
}

void shprog_emu64_print_uniform_locations(FILE* out) {
    fprintf(out,
        "projection_mtx       %d\n" // Leading space for single-digit uloc
        "model_view_mtx       %d\n" // Leading space for single-digit uloc
        "game_projection_mtx  %d\n" // Leading space for single-digit uloc
        "rsp_color_selection %d\n"
        "tex_settings_1      %d\n"
        "tex_settings_2      %d\n"
        "rsp_colors[0]       %d\n"
        "rsp_colors[1]       %d\n"
        "rsp_colors[2]       %d\n"
        "rsp_colors[3]       %d\n"
        "texture_const_1     %d\n"
        "texture_const_2     %d\n"
        "cc_constants        %d\n",
        emu64_uniform_locations.projection_mtx,
        emu64_uniform_locations.model_view_mtx,
        emu64_uniform_locations.game_projection_mtx,
        emu64_uniform_locations.rsp_color_selection,
        emu64_uniform_locations.tex_settings_1,
        emu64_uniform_locations.tex_settings_2,
        emu64_uniform_locations.rsp_colors[0],
        emu64_uniform_locations.rsp_colors[1],
        emu64_uniform_locations.rsp_colors[2],
        emu64_uniform_locations.rsp_colors[3],
        emu64_const_uniform_locations.texture_const_1,
        emu64_const_uniform_locations.texture_const_2,
        emu64_const_uniform_locations.cc_constants);
}
