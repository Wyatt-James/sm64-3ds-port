#pragma once

#include <stdio.h>
#include "src/pc/n3ds/libctru_inc.h"

#include "src/pc/gfx/gfx_3ds_shaders.h"

#include "src/pc/gfx/shaders/emu64.h" // Generated dynamically by Picasso

/*
 * A set of shaders for emulating the N64.
 */

#define EMU64_ULOC_projection_mtx                 VSH_FVEC_projection_mtx
#define EMU64_ULOC_model_view_mtx                 VSH_FVEC_model_view_mtx
#define EMU64_ULOC_game_projection_mtx            VSH_FVEC_game_projection_mtx
#define EMU64_ULOC_transposed_model_view_mtx      VSH_FVEC_transposed_model_view_mtx
#define EMU64_ULOC_rsp_color_selection            VSH_FVEC_rsp_color_selection
#define EMU64_ULOC_tex_settings_1                 VSH_FVEC_tex_settings_1
#define EMU64_ULOC_tex_settings_2                 VSH_FVEC_tex_settings_2
#define EMU64_ULOC_vertex_load_flags              VSH_FVEC_vertex_load_flags
#define EMU64_ULOC_light_colors_ambient           VSH_FVEC_ambient_light_color
#define EMU64_ULOC_light_colors_directional(n_)  ((n_) + VSH_FVEC_light_colors)
#define EMU64_ULOC_light_colors(n_)              ((n_) + VSH_FVEC_ambient_light_color)
#define EMU64_ULOC_light_directions(n_)          ((n_) + VSH_FVEC_light_directions)
#define EMU64_ULOC_rsp_colors(n_)                ((n_) + VSH_FVEC_rsp_colors)
#define EMU64_CONST_ULOC_texture_const_1          VSH_FVEC_texture_const_1
#define EMU64_CONST_ULOC_texture_const_2          VSH_FVEC_texture_const_2
#define EMU64_CONST_ULOC_cc_constants             VSH_FVEC_cc_constants
#define EMU64_CONST_ULOC_emu64_const_1            VSH_FVEC_emu64_const_1
#define EMU64_CONST_ULOC_emu64_const_2            VSH_FVEC_emu64_const_2


#define EMU64_NUM_VERTEX_FORMATS 5
#define EMU64_NUM_UNIFS (EMU64_CONST_ULOC_emu64_const_2 + 1)

#define EMU64_MAX_DIRECTIONAL_LIGHTS 2 // Does NOT include ambient
#define EMU64_NUM_RSP_COLORS 4

// Stride values for specific inputs. Unit is one word (uint32_t)
#define EMU64_STRIDE_UNIT_SIZE               sizeof(float)
#define EMU64_STRIDE_RGBA                    1
#define EMU64_STRIDE_XYZA                    1
#define EMU64_STRIDE_POSITION                2
#define EMU64_STRIDE_TEXTURE                 1
#define EMU64_STRIDE_VERTEX_COLOR            EMU64_STRIDE_RGBA
#define EMU64_STRIDE_VERTEX_NORMAL_AND_ALPHA EMU64_STRIDE_XYZA

// Maximum possible stride. RGBA and XYZA are mutually exclusive.
#define EMU64_STRIDE_MAX   (EMU64_STRIDE_POSITION \
                         +  EMU64_STRIDE_TEXTURE  \
                         +  EMU64_STRIDE_RGBA)

typedef uint8_t Emu64ShaderFeatures; // EMU64 shader code

// Shader VBO features
enum Emu64ShaderFeature {
   EMU64_VBO_POSITION     = BIT(0),
   EMU64_VBO_TEXTURE      = BIT(1),
   EMU64_VBO_COLOR        = BIT(2), // Mutually exclusive
   EMU64_VBO_NORMALS      = BIT(3)  // Mutually exclusive
#define EMU64_VBO_PERMUTATIONS (EMU64_VBO_NORMALS << 1)
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
          directional[EMU64_MAX_DIRECTIONAL_LIGHTS];
   };
   int all[EMU64_MAX_DIRECTIONAL_LIGHTS + 1]; // Homogenous ambient
};

struct n3ds_emu64_const_uniform_defaults {
   float texture_const_1[4],
         texture_const_2[4],
         cc_constants[4],
         emu64_const_1[4],
         emu64_const_2[4];
};

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
const struct n3ds_shader_info* emu64_get_shader_info(Emu64ShaderFeatures shader_code);
