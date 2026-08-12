#include "gfx_n3ds_shprog_emu64.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/pc_macros.h"

#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/gfx_3ds_shader_binaries.h"

#define VBO_S8(count_)  {.format = GPU_BYTE,          .count = (count_)}
#define VBO_U8(count_)  {.format = GPU_UNSIGNED_BYTE, .count = (count_)}
#define VBO_S16(count_) {.format = GPU_SHORT,         .count = (count_)}
#define VBO_F32(count_) {.format = GPU_FLOAT,         .count = (count_)}

#define VBO_POS      VBO_S16(4)
#define VBO_TEX      VBO_S16(2)
#define VBO_PAD      VBO_S16(2)
#define VBO_RGBA     VBO_U8(4)
#define VBO_ALPHA    VBO_U8(1)
#define VBO_NORMALS  VBO_S8(3)

// VBO input flag bitfields
#define V_POS  EMU64_VBO_POSITION
#define V_TEX  EMU64_VBO_TEXTURE
#define V_COL  EMU64_VBO_COLOR
#define V_NOR  EMU64_VBO_NORMALS
#define V_PAD  (EMU64_VBO_NORMALS << 1)

// Bitwise AND ternary
#define BIT_TERNARY(flag_, v_, res1_, res2_) (((v_ & flag_) ? res1_ : res2_))

// Calculates the stride of a VBO, given a flag
#define VBO_STRIDE(val_) (BIT_TERNARY(V_POS, (val_), EMU64_STRIDE_POSITION,                0)    \
                        + BIT_TERNARY(V_TEX, (val_), EMU64_STRIDE_TEXTURE,                 0)    \
                        + BIT_TERNARY(V_PAD, (val_), EMU64_STRIDE_TEXTURE,                 0)    \
                        + BIT_TERNARY(V_COL, (val_), EMU64_STRIDE_VERTEX_COLOR,            0)    \
                        + BIT_TERNARY(V_NOR, (val_), EMU64_STRIDE_VERTEX_NORMAL_AND_ALPHA, 0)    \
                        )

// Constructs part of an n3ds_shader_vbo_info, given a flag
#define VBO_INFO_VEC(val_) .has_position = BIT_TERNARY(V_POS, (val_), true, false),    \
                           .has_texture  = BIT_TERNARY(V_TEX, (val_), true, false),    \
                           .has_color    = BIT_TERNARY(V_COL, (val_), true, false),    \
                           .has_normals  = BIT_TERNARY(V_NOR, (val_), true, false),    \
                           .stride       = VBO_STRIDE(val_)


#define VBO_ATTR(attrs_) { .data = attrs_, .num_attribs = ARRAY_COUNT(attrs_) }
#define VBO_INFO(val_, attrs_) { VBO_INFO_VEC(val_), .attributes = VBO_ATTR(attrs_) }

enum n3ds_shader_emu64_dvle_index { 
   DVLE_03 = 0,
   DVLE_05,
   DVLE_07,
   DVLE_09,
   DVLE_11,
   DVLE_MENU,
};

const struct n3ds_emu64_const_uniform_defaults 
   emu64_const_uniform_defaults = {
    .texture_const_1 = {   0.0f,    1.0f,  1/65536.0f, 1/508.0f },
    .texture_const_2 = {   4.0f,   -8.0f,     1/32.0f,   1/4.0f },
    .cc_constants    = {  -1.0f, 3000.0f,        1.0f,     0.0f },
    .emu64_const_1   = {   0.0f,    1.0f,    1/127.0f, 1/255.0f },
    .emu64_const_2   = { 255.0f,  256.0f,      127.0f,   128.0f },
   };

struct n3ds_shader_binary
    emu64_shader_binary = { emu64_shbin, 0, NULL };

const struct n3ds_emu64_vertex_attribute
   emu64_vertex_format_3[]     = { VBO_POS, VBO_TEX                                   },
   emu64_vertex_format_5[]     = { VBO_POS, VBO_PAD, VBO_RGBA                         }, // VBO_TEX is present but ignored. We write extra data to optimize gfx_tri_create_vbo.
   emu64_vertex_format_7[]     = { VBO_POS, VBO_TEX, VBO_RGBA                         },
   emu64_vertex_format_9[]     = { VBO_POS, VBO_PAD,           VBO_NORMALS, VBO_ALPHA }, // VBO_TEX is present but ignored. We write extra data to optimize gfx_tri_create_vbo.
   emu64_vertex_format_11[]    = { VBO_POS, VBO_TEX,           VBO_NORMALS, VBO_ALPHA },
   emu64_vertex_format_menu[]  = { VBO_POS, VBO_TEX                                   };

const struct n3ds_shader_info
    emu64_shader_3        = { &emu64_shader_binary, DVLE_03,   1, VBO_INFO(V_POS | V_TEX              , emu64_vertex_format_3)    }, // position, texture
    emu64_shader_5        = { &emu64_shader_binary, DVLE_05,   2, VBO_INFO(V_POS | V_PAD | V_COL      , emu64_vertex_format_5)    }, // position, color
    emu64_shader_7        = { &emu64_shader_binary, DVLE_07,   3, VBO_INFO(V_POS | V_TEX | V_COL      , emu64_vertex_format_7)    }, // position, texture, color
    emu64_shader_9        = { &emu64_shader_binary, DVLE_09,   4, VBO_INFO(V_POS | V_PAD |       V_NOR, emu64_vertex_format_9)    }, // position, normals
    emu64_shader_11       = { &emu64_shader_binary, DVLE_11,   5, VBO_INFO(V_POS | V_TEX |       V_NOR, emu64_vertex_format_11)   }, // position, texture, normals
    emu64_shader_menu     = { &emu64_shader_binary, DVLE_MENU, 6, VBO_INFO(V_POS | V_TEX              , emu64_vertex_format_menu) }; // position, texture (alternate)

void shprog_emu64_init()
{
    emu64_shader_binary.size = emu64_shbin_size;
    emu64_shader_binary.dvlb = DVLB_ParseFile((__3ds_u32*)emu64_shader_binary.data, emu64_shader_binary.size);
}

void shprog_emu64_print_uniform_locations(FILE* out) {
    fprintf(out,
        "projection_mtx             %d\n" // Leading space for single-digit uloc
        "model_view_mtx             %d\n" // Leading space for single-digit uloc
        "game_projection_mtx        %d\n" // Leading space for single-digit uloc
        "transposed_model_view_mtx %d\n"
        "rsp_color_selection       %d\n"
        "tex_settings_1            %d\n"
        "tex_settings_2            %d\n"
        "vertex_load_flags         %d\n"
        "ambient_light_color       %d\n"
        "light_colors[0]           %d\n"
        "light_colors[1]           %d\n"
        "light_directions[0]       %d\n"
        "light_directions[1]       %d\n"
        "rsp_colors[0]             %d\n"
        "rsp_colors[1]             %d\n"
        "rsp_colors[2]             %d\n"
        "rsp_colors[3]             %d\n"
        "texture_const_1           %d\n"
        "texture_const_2           %d\n"
        "cc_constants              %d\n"
        "emu64_const_1             %d\n"
        "emu64_const_2             %d\n",
        EMU64_ULOC_projection_mtx,
        EMU64_ULOC_model_view_mtx,
        EMU64_ULOC_game_projection_mtx,
        EMU64_ULOC_transposed_model_view_mtx,
        EMU64_ULOC_rsp_color_selection,
        EMU64_ULOC_tex_settings_1,
        EMU64_ULOC_tex_settings_2,
        EMU64_ULOC_vertex_load_flags,
        EMU64_ULOC_light_colors_ambient,
        EMU64_ULOC_light_colors_directional(0),
        EMU64_ULOC_light_colors_directional(1),
        EMU64_ULOC_light_directions(0),
        EMU64_ULOC_light_directions(1),
        EMU64_ULOC_rsp_colors(0),
        EMU64_ULOC_rsp_colors(1),
        EMU64_ULOC_rsp_colors(2),
        EMU64_ULOC_rsp_colors(3),
        EMU64_CONST_ULOC_texture_const_1,
        EMU64_CONST_ULOC_texture_const_2,
        EMU64_CONST_ULOC_cc_constants,
        EMU64_CONST_ULOC_emu64_const_1,
        EMU64_CONST_ULOC_emu64_const_2);
}

const struct n3ds_shader_info* emu64_get_shader_info(Emu64ShaderFeatures shader_code)
{
    // fprintf(stderr, "SH %c%c%c%c\n",
    //     (shader_code & EMU64_VBO_POSITION) ? 'P' : '-',
    //     (shader_code & EMU64_VBO_TEXTURE)  ? 'T' : '-',
    //     (shader_code & EMU64_VBO_COLOR)    ? 'C' : '-',
    //     (shader_code & EMU64_VBO_NORMALS)  ? 'N' : '-');
    switch(shader_code)
    {
        case EMU64_VBO_POSITION | EMU64_VBO_TEXTURE:                     return &emu64_shader_3;
        case EMU64_VBO_POSITION | EMU64_VBO_COLOR:                       return &emu64_shader_5;
        case EMU64_VBO_POSITION | EMU64_VBO_TEXTURE | EMU64_VBO_COLOR:   return &emu64_shader_7;
        case EMU64_VBO_POSITION | EMU64_VBO_NORMALS:                     return &emu64_shader_9;
        case EMU64_VBO_POSITION | EMU64_VBO_TEXTURE | EMU64_VBO_NORMALS: return &emu64_shader_11;
        default:
            fprintf(stderr, "Invalid shader code: %c%c%c%c\n",
                (shader_code & EMU64_VBO_POSITION) ? 'P' : '-',
                (shader_code & EMU64_VBO_TEXTURE)  ? 'T' : '-',
                (shader_code & EMU64_VBO_COLOR)    ? 'C' : '-',
                (shader_code & EMU64_VBO_NORMALS)  ? 'N' : '-');
            return &emu64_shader_7;
    }
}
