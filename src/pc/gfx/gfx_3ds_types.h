#ifndef GFX_3DS_TYPES_H
#define GFX_3DS_TYPES_H

#include <stdint.h>

typedef uint8_t Emu64ShaderCode; // EMU64 shader code

union ShaderProgramFeatureFlags {
    struct {
        bool position, tex, color, normals;
    };

    uint32_t u32;
};

#endif
