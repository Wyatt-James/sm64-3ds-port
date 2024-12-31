#include "gfx_citro3d_wrappers.h"

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
#include <c3d/uniforms.h>
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


// Sets a C3D float uniform from a vector of floats.
void C3DW_FVUnifSetArray(GPU_SHADER_TYPE type, int id, float vec[restrict 4])
{
    C3D_FVUnifSet(type, id, vec[0], vec[1], vec[2], vec[3]);
}

// Sets a C3D float uniform from an RGBA32 union. Scales by 1/255.
void C3DW_FVUnifSetRGBA(GPU_SHADER_TYPE type, int id, union RGBA32 color)
{
    float r = color.r / 255.0f,
          g = color.g / 255.0f,
          b = color.b / 255.0f,
          a = color.a / 255.0f;
    C3D_FVUnifSet(type, id, r, g, b, a);
}

// Sets a C3D float uniform from an RGBA32 union, but sets Alpha to 0. Scales by 1/255.
void C3DW_FVUnifSetRGB(GPU_SHADER_TYPE type, int id, union RGBA32 color)
{
    float r = color.r / 255.0f,
          g = color.g / 255.0f,
          b = color.b / 255.0f;
    C3D_FVUnifSet(type, id, r, g, b, 0);
}
