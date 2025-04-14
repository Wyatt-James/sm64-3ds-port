#pragma once

#include <stdio.h>
#include "src/pc/n3ds/libctru_inc.h"

#include "src/pc/gfx/gfx_3ds_shaders.h"

/*
 * A set of shaders for emulating the N64.
 *
 * Note: moving shader uniforms from runtime to compile-time saved ~80us.
 */

#define EMU64_USE_UNSAFE

// WYATT_TODO picasso can generate this for us. Use that instead.
#ifdef EMU64_USE_UNSAFE
#define EMU64_ULOC_projection_mtx                 0
#define EMU64_ULOC_model_view_mtx                 4
#define EMU64_ULOC_game_projection_mtx            8
#define EMU64_ULOC_transposed_model_view_mtx      12
#define EMU64_ULOC_rsp_color_selection            16
#define EMU64_ULOC_tex_settings_1                 17
#define EMU64_ULOC_tex_settings_2                 18
#define EMU64_ULOC_vertex_load_flags              19
#define EMU64_ULOC_light_colors_ambient           20
#define EMU64_ULOC_light_colors_directional(n_)  (21 + (n_))
#define EMU64_ULOC_light_colors(n_)              (20 + (n_))
#define EMU64_ULOC_light_directions(n_)          (23 + (n_))
#define EMU64_ULOC_rsp_colors(n_)                (25 + (n_))
#define EMU64_CONST_ULOC_texture_const_1          29
#define EMU64_CONST_ULOC_texture_const_2          30
#define EMU64_CONST_ULOC_cc_constants             31
#define EMU64_CONST_ULOC_emu64_const_1            32
#define EMU64_CONST_ULOC_emu64_const_2            33
#else
#define EMU64_ULOC_projection_mtx                 emu64_uniform_locations_.projection_mtx
#define EMU64_ULOC_model_view_mtx                 emu64_uniform_locations_.model_view_mtx
#define EMU64_ULOC_game_projection_mtx            emu64_uniform_locations_.game_projection_mtx
#define EMU64_ULOC_transposed_model_view_mtx      emu64_uniform_locations_.transposed_model_view_mtx
#define EMU64_ULOC_rsp_color_selection            emu64_uniform_locations_.rsp_color_selection
#define EMU64_ULOC_tex_settings_1                 emu64_uniform_locations_.tex_settings_1
#define EMU64_ULOC_tex_settings_2                 emu64_uniform_locations_.tex_settings_2
#define EMU64_ULOC_vertex_load_flags              emu64_uniform_locations_.vertex_load_flags
#define EMU64_ULOC_light_colors_ambient           emu64_uniform_locations_.light_colors.ambient
#define EMU64_ULOC_light_colors_directional(n_)   emu64_uniform_locations_.light_colors.directional[n_]
#define EMU64_ULOC_light_colors(n_)               emu64_uniform_locations_.light_colors.all[n_]
#define EMU64_ULOC_light_directions(n_)           emu64_uniform_locations_.light_directions[n_]
#define EMU64_ULOC_rsp_colors(n_)                 emu64_uniform_locations_.rsp_colors[n_]
#define EMU64_CONST_ULOC_texture_const_1          emu64_const_uniform_locations_.texture_const_1
#define EMU64_CONST_ULOC_texture_const_2          emu64_const_uniform_locations_.texture_const_2
#define EMU64_CONST_ULOC_cc_constants             emu64_const_uniform_locations_.cc_constants
#define EMU64_CONST_ULOC_emu64_const_1            emu64_const_uniform_locations_.emu64_const_1
#define EMU64_CONST_ULOC_emu64_const_2            emu64_const_uniform_locations_.emu64_const_2
#endif

#define EMU64_NUM_VERTEX_FORMATS 5
#define EMU64_UNSAFE_NUM_FV_UNIFS 34

#define EMU64_MAX_LIGHTS 2 // Does NOT include ambient
#define EMU64_NUM_RSP_COLORS 4

// Stride values for specific inputs. Unit is one word (uint32_t)
#define EMU64_STRIDE_UNIT_SIZE sizeof(float)
#define EMU64_STRIDE_RGBA         1
#define EMU64_STRIDE_XYZA         1
#define EMU64_STRIDE_POSITION     2
#define EMU64_STRIDE_TEXTURE      1
#define EMU64_STRIDE_VERTEX_COLOR EMU64_STRIDE_RGBA
#define EMU64_STRIDE_VERTEX_NORMAL_AND_ALPHA EMU64_STRIDE_XYZA

// Maximum possible stride. RGBA and XYZA are mutually exclusive.
#define EMU64_STRIDE_MAX      (EMU64_STRIDE_POSITION    \
                            +  EMU64_STRIDE_TEXTURE     \
                            +  EMU64_STRIDE_RGBA)

typedef uint8_t Emu64ShaderCode; // EMU64 shader code

typedef union
{
    struct
    {
        bool position, tex, color, normals;
    };

    uint32_t u32;
} Emu64ProgramFeatureFlags;

// Shader VBO features
enum Emu64ShaderFeature {
   EMU64_VBO_POSITION     = 1 << 0,
   EMU64_VBO_TEXTURE      = 1 << 1,
   EMU64_VBO_COLOR        = 1 << 2, // Mutually exclusive
   EMU64_VBO_NORMALS      = 1 << 3  // Mutually exclusive
};
                            
// Negative values are special cases.
// Unspecified values give undefined behavior.
enum Emu64ColorCombinerSource {
    EMU64_CC_LOD       = -2,  // LoD calculation
    EMU64_CC_SHADE     = -1,  // vertex color passthrough
    EMU64_CC_PRIM      =  0,  // RDP PRIMITIVE color
    EMU64_CC_ENV       =  1,  // RDP ENV color
    EMU64_CC_0         =  2,  // { 0, 0, 0, 0 }
    EMU64_CC_1         =  3   // { 1, 1, 1, 1 }. Used for shade fog when alpha is enabled.
};

// Uniforms for light colors. Allows handling ambient homogenously or as a special case.
union n3ds_emu64_light_color_uniform_locations {
   struct {
      int ambient, // Special-case ambient
          directional[EMU64_MAX_LIGHTS];
   };
   int all[EMU64_MAX_LIGHTS + 1]; // Homogenous ambient
};

// Uniforms that should be changed freely.
struct n3ds_emu64_uniform_locations {
   int projection_mtx,
       model_view_mtx,
       game_projection_mtx,
       transposed_model_view_mtx,
       rsp_color_selection,
       tex_settings_1,
       tex_settings_2,
       vertex_load_flags;
   union n3ds_emu64_light_color_uniform_locations light_colors;
   int light_directions[EMU64_MAX_LIGHTS],
       rsp_colors[EMU64_NUM_RSP_COLORS];
};

// Uniforms that should be initialized once and remain constant.
struct n3ds_emu64_const_uniform_locations {
   int texture_const_1,
       texture_const_2,
       cc_constants,
       emu64_const_1,
       emu64_const_2;
};

struct n3ds_emu64_const_uniform_defaults {
   float texture_const_1[4],
         texture_const_2[4],
         cc_constants[4],
         emu64_const_1[4],
         emu64_const_2[4];
};

#ifndef EMU64_USE_UNSAFE
extern struct n3ds_emu64_uniform_locations
   emu64_uniform_locations_;

extern struct n3ds_emu64_const_uniform_locations
   emu64_const_uniform_locations_;
#endif
   
extern const struct n3ds_emu64_uniform_defaults 
   emu64_uniform_defaults;
   
extern const struct n3ds_emu64_const_uniform_defaults 
   emu64_const_uniform_defaults;

extern struct n3ds_shader_binary
   emu64_shader_binary;

extern const struct n3ds_shader_info
   emu64_shader_3,    // Pos + Tex
   emu64_shader_5,    // Pos + Color
   emu64_shader_7,    // Pos + Tex + Color
   emu64_shader_9,    // Pos + Normals + Alpha
   emu64_shader_11,   // Pos + Tex + Normals + Alpha
   emu64_shader_menu; // Pos + Tex (alternate)

extern const struct n3ds_emu64_vertex_attribute
   emu64_vertex_format_3[],
   emu64_vertex_format_5[],
   emu64_vertex_format_7[],
   emu64_vertex_format_9[],
   emu64_vertex_format_11[],
   emu64_vertex_format_menu[];

// Initializes Emu64
void shprog_emu64_init();

// Prints Emu64's uniform locations
void shprog_emu64_print_uniform_locations(FILE* out);

// Looks up an n3ds_shader_info struct from the given Emu64 shader code
const struct n3ds_shader_info* emu64_get_shader_info(Emu64ShaderCode shader_code);

// Looks up an n3ds_shader_info struct from the given feature flags.
const struct n3ds_shader_info* emu64_get_shader_info_from_flags(Emu64ProgramFeatureFlags feature_flags);

// Calculates an Emu64 shader code based on the given feature flags
Emu64ShaderCode emu64_calculate_shader_code(Emu64ProgramFeatureFlags feature_flags);
