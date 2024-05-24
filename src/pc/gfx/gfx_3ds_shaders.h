#ifdef TARGET_N3DS
#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include "gfx_3ds_shader_binaries.h"

struct n3ds_shader {
    const uint8_t* shader_binary;
    const uint32_t* shader_size;
    const uint32_t identifier;
};

const struct n3ds_shader
    shader_def  = { shader_shbin,    &shader_shbin_size,     0 },
    shader_1    = { shader_1_shbin,  &shader_1_shbin_size,   1 },
 // shader_3    = { shader_3_shbin,  &shader_3_shbin_size,   2 },
    shader_4    = { shader_4_shbin,  &shader_4_shbin_size,   3 },
    shader_5    = { shader_5_shbin,  &shader_5_shbin_size,   4 },
 // shader_6    = { shader_6_shbin,  &shader_6_shbin_size,   5 },
 // shader_7    = { shader_7_shbin,  &shader_7_shbin_size,   6 },
    shader_8    = { shader_8_shbin,  &shader_8_shbin_size,   7 },
    shader_9    = { shader_9_shbin,  &shader_9_shbin_size,   8 },
    shader_20   = { shader_20_shbin, &shader_20_shbin_size,  9 },
    shader_41   = { shader_41_shbin, &shader_41_shbin_size, 10 };

const struct n3ds_shader shaders[] = {
    shader_def,
    shader_1,
 // shader_3,
    shader_4,
    shader_5,
 // shader_6,
 // shader_7,
    shader_8,
    shader_9,
    shader_20,
    shader_41
};

#endif
#endif
