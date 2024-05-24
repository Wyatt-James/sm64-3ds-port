#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include <stdbool.h>

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
        has_color1,
        has_color2;
   uint8_t stride;
};

struct n3ds_shader_info {
    const uint8_t* shader_binary;
    const uint32_t* shader_size;
    const uint32_t identifier;
    struct n3ds_shader_vbo_info vbo_info;
};

extern const struct n3ds_shader_info
    shader_default,
    shader_1,
 // shader_3,
    shader_4,
    shader_5,
 // shader_6,
 // shader_7,
    shader_8,
    shader_9,
    shader_20,
    shader_41;

extern const struct n3ds_shader_info* const shaders[];

#endif
