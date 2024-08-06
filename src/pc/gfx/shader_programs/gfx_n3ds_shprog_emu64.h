#ifndef GFX_N3DS_SHPROG_EMU64_H
#define GFX_N3DS_SHPROG_EMU64_H

#include <stdio.h>

#include "src/pc/gfx/gfx_3ds_shaders.h"

/*
 * A set of shaders for emulating the N64.
 */

// Stride values for specific inputs. Unit is one word (uint32_t)
#define EMU64_STRIDE_RGBA         1
#define EMU64_STRIDE_RGB          1
#define EMU64_STRIDE_POSITION     2
#define EMU64_STRIDE_TEXTURE      1
#define EMU64_STRIDE_VERTEX_COLOR EMU64_STRIDE_RGBA

// Maximum possible stride
#define EMU64_STRIDE_MAX      (EMU64_STRIDE_POSITION    \
                            +  EMU64_STRIDE_TEXTURE     \
                            +  EMU64_STRIDE_RGBA)

// Shader VBO features
enum Emu64ShaderFeature {
   EMU64_VBO_POSITION     = BIT(0),
   EMU64_VBO_TEXTURE      = BIT(1),
   EMU64_VBO_COLOR        = BIT(2)
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

// Uniforms that should be changed freely.
struct n3ds_emu64_uniform_locations {
   int projection_mtx,
       model_view_mtx,
       game_projection_mtx,
       rsp_color_selection,
       tex_settings_1,
       tex_settings_2,
       rsp_colors[4];
};

// Uniforms that should be initialized once and remain constant.
struct n3ds_emu64_const_uniform_locations {
   int texture_const_1,
       texture_const_2,
       cc_constants,
       emu64_const_1;
};

struct n3ds_emu64_const_uniform_defaults {
   float texture_const_1[4],
         texture_const_2[4],
         cc_constants[4],
         emu64_const_1[4];
};

extern struct n3ds_emu64_uniform_locations
   emu64_uniform_locations;

extern struct n3ds_emu64_const_uniform_locations
   emu64_const_uniform_locations;
   
extern const struct n3ds_emu64_uniform_defaults 
   emu64_uniform_defaults;
   
extern const struct n3ds_emu64_const_uniform_defaults 
   emu64_const_uniform_defaults;

extern struct n3ds_shader_binary
   emu64_shader_binary;

extern const struct n3ds_shader_info
   emu64_shader_3, // Pos + Tex
   emu64_shader_5, // Pos + Color
   emu64_shader_7; // Pos + Tex + Color

// Initializes Emu64
void shprog_emu64_init();

// Prints Emu64's uniform locations
void shprog_emu64_print_uniform_locations(FILE* out);

#endif
