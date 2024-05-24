#ifdef TARGET_N3DS
#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include "gfx_3ds_shader_binaries.h"

// Stride values for specific inputs. Unit is one word (uint32_t)
#define STRIDE_RGBA     1
#define STRIDE_RGB      1
#define STRIDE_POSITION 2
#define STRIDE_TEXTURE  2
#define STRIDE_FOG      STRIDE_RGBA

// VBO input flag bitfields
#define V_POS  0b10000
#define V_TEX  0b01000
#define V_FOG  0b00100
#define V_COL1 0b00010
#define V_COL2 0b00001

// Bitwise AND ternary
#define BIT_TERNARY(flag_, v_, res1_, res2_) (((v_ & flag_) ? res1_ : res2_))

// Calculates the stride of a VBO, given a flag
#define VBO_STRIDE(val_) (BIT_TERNARY(V_POS,  (val_), STRIDE_POSITION, 0) + \
                          BIT_TERNARY(V_TEX,  (val_), STRIDE_TEXTURE,  0) + \
                          BIT_TERNARY(V_FOG,  (val_), STRIDE_FOG,      0) + \
                          BIT_TERNARY(V_COL1, (val_), STRIDE_RGBA,     0) + \
                          BIT_TERNARY(V_COL2, (val_), STRIDE_RGBA,     0))

// Constructs an n3ds_shader_vbo_info, given a flag
#define VBO_INFO(val_) {BIT_TERNARY(V_POS,  (val_), true, false), \
                        BIT_TERNARY(V_TEX,  (val_), true, false), \
                        BIT_TERNARY(V_FOG,  (val_), true, false), \
                        BIT_TERNARY(V_COL1, (val_), true, false), \
                        BIT_TERNARY(V_COL2, (val_), true, false), \
                        VBO_STRIDE(val_)}

struct n3ds_shader_vbo_info {
   bool has_position,
        has_texture,
        has_fog,
        has_color,
        has_color2;
   uint8_t stride;
};

struct n3ds_shader {
    const uint8_t* shader_binary;
    const uint32_t* shader_size;
    const uint32_t identifier;
    struct n3ds_shader_vbo_info vbo_info;
};

const struct n3ds_shader
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

const struct n3ds_shader* const shaders[] = {
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

#endif
#endif
