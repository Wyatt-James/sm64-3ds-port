#include "gfx_3ds_shaders.h"

const struct n3ds_shader_info
    shader_default  = { shader_shbin,    &shader_shbin_size,     0, VBO_INFO(V_POS | V_TEX | V_COL1)          }, // position, texture, color rgba
    shader_1        = { shader_1_shbin,  &shader_1_shbin_size,   1, VBO_INFO(V_POS | V_TEX)                   }, // position, texture
 // shader_3        = { shader_3_shbin,  &shader_3_shbin_size,   2, VBO_INFO(V_POS | V_TEX | V_FOG)           }, // position, texture, fog
    shader_4        = { shader_4_shbin,  &shader_4_shbin_size,   3, VBO_INFO(V_POS | V_COL1)                  }, // position, color rgba
    shader_5        = { shader_5_shbin,  &shader_5_shbin_size,   4, VBO_INFO(V_POS | V_TEX | V_COL1)          }, // position, texture, color rgba
 // shader_6        = { shader_6_shbin,  &shader_6_shbin_size,   5, VBO_INFO(V_POS | V_FOG | V_COL1)          }, // position, fog, color rgba
 // shader_7        = { shader_7_shbin,  &shader_7_shbin_size,   6, VBO_INFO(V_POS | V_TEX | V_FOG | V_COL1)  }, // position, texture, fog, color rgba
    shader_8        = { shader_8_shbin,  &shader_8_shbin_size,   7, VBO_INFO(V_POS | V_COL1)                  }, // position, color rgb
    shader_9        = { shader_9_shbin,  &shader_9_shbin_size,   8, VBO_INFO(V_POS | V_TEX | V_COL1)          }, // position, texture, color rgb
    shader_20       = { shader_20_shbin, &shader_20_shbin_size,  9, VBO_INFO(V_POS | V_COL1 | V_COL2)         }, // position, 2 colors rgba
    shader_41       = { shader_41_shbin, &shader_41_shbin_size, 10, VBO_INFO(V_POS | V_TEX | V_COL1 | V_COL2) }; // position, texture, 2 colors rgb

const struct n3ds_shader_info* const shaders[] = {
    &shader_default,
    &shader_1,
//  &shader_3,
    &shader_4,
    &shader_5,
//  &shader_6,
//  &shader_7,
    &shader_8,
    &shader_9,
    &shader_20,
    &shader_41
};
