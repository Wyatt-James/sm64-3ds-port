#include <PR/gbi.h>
#include <stdio.h>

#include "gfx_cc.h"
#include "gfx_citro3d_helpers.h"

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
#include <c3d/maths.h>
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

#define FAST_SINGLE_MOD(v_, max_) (((v_ >= max_) ? (v_ - max_) : (v_))) // v_ % max_, but only once.
#define ARR_INDEX_2D(x_, y_, w_) (y_ * w_ + x_)
#define U32_AS_FLOAT(v_) (*(float*) &v_)

const C3D_Mtx IDENTITY_MTX = C3D_STATIC_IDENTITY_MTX;

// Add W to Z coordinate
const C3D_Mtx DEPTH_ADD_W_MTX = {
                    .r = {
                        {.x = 1.0f},
                        {.y = 1.0f},
                        {.z = 1.0f, .w = 1.0f},
                        {.w = 1.0f}
                    }
               };

uint8_t gfx_citro3d_calculate_shader_code(bool has_texture,
                                          UNUSED bool has_fog,
                                          bool has_alpha,
                                          bool has_color1,
                                          bool has_color2)
{
    // 1 => texture
    // 2 => fog (disabled)
    // 4 => 1 color RGBA
    // 8 => 1 color RGB
    // 16 => 2 colors RGBA
    // 32 => 2 colors RGB

    uint8_t shader_code = 0;

    if (has_texture)
        shader_code += 1;
    // if (has_fog)
    //     shader_code += 2;
    if (has_color1)
        shader_code += has_alpha ? 4 : 8;
    if (has_color2)
        shader_code += has_alpha ? 16 : 32;

    return shader_code;
}

const struct n3ds_shader_info* get_shader_info_from_shader_code (uint8_t shader_code)
{
    const struct n3ds_shader_info* shader = NULL;

    /* 
     * Used shaders
     * 1:  0 inputs
     * 4:  1 input
     * 5:  1 input
     * 8:  1 input
     * 9:  1 input
     * 20: 2 inputs
     * 41: 2 inputs
     */
    switch(shader_code)
    {
        case 1:
            shader = &shader_1;
            break;
        // case 3:
        //     shader = &shader_3;
        //     break;
        case 4:
            shader = &shader_4;
            break;
        case 5:
            shader = &shader_5;
            break;
        // case 6:
        //     shader = &shader_6;
        //     break;
        // case 7:
        //     shader = &shader_7;
        //     break;
        case 8:
            shader = &shader_8;
            break;
        case 9:
            shader = &shader_9;
            break;
        case 20:
            shader = &shader_20;
            break;
        case 41:
            shader = &shader_41;
            break;
        default:
            shader = &shader_default;
            fprintf(stderr, "Warning! Using default shader for %u\n", shader_code);
            break;
    }

    return shader;
}

void gfx_citro3d_pad_texture_rgba32(union RGBA32* src,
                                    union RGBA32* dest,
                                    uint32_t src_w,
                                    uint32_t src_h,
                                    uint32_t new_w,
                                    uint32_t new_h)
{
    static const int sTileOrder[] =
    {
        0,  1,   4,  5,
        2,  3,   6,  7,

        8,  9,  12, 13,
        10, 11, 14, 15
    };

    for (u32 y = 0; y < new_h; y += 8) {
        for (u32 x = 0; x < new_w; x += 8) {
            for (u32 i = 0; i < 64; i++)
            {
                int x2 = i % 8; // Tiling nonsense
                int y2 = i / 8;

                u32 src_x = FAST_SINGLE_MOD(x + x2, src_w);
                u32 src_y = FAST_SINGLE_MOD(y + y2, src_h);

                union RGBA32 color = src[ARR_INDEX_2D(src_x, src_y, src_w)];
                u32 out_index = sTileOrder[x2 % 4 + y2 % 4 * 4] + 16 * (x2 / 4) + 32 * (y2 / 4);

                dest[out_index].r = color.a;
                dest[out_index].g = color.b;
                dest[out_index].b = color.g;
                dest[out_index].a = color.r;
            }
            dest += 64;
        }
    }
}

GPU_TEVSRC gfx_citro3d_cc_input_to_tev_src(int cc_input, bool swap_input)
{
    switch (cc_input)
    {
        case SHADER_0:
            return GPU_CONSTANT;
        case SHADER_INPUT_1:
            return swap_input ? GPU_PREVIOUS : GPU_PRIMARY_COLOR;
        case SHADER_INPUT_2:
            return swap_input ? GPU_PRIMARY_COLOR : GPU_PREVIOUS;
        case SHADER_INPUT_3:
            return GPU_CONSTANT;
        case SHADER_INPUT_4:
            return GPU_CONSTANT;
        case SHADER_TEXEL0:
        case SHADER_TEXEL0A:
            return GPU_TEXTURE0;
        case SHADER_TEXEL1:
            return GPU_TEXTURE1;
        default:
            return GPU_CONSTANT;
    }
}

GPU_TEXTURE_WRAP_PARAM gfx_citro3d_convert_texture_clamp_mode(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GPU_CLAMP_TO_EDGE;
    else if (val & G_TX_MIRROR)
        return GPU_MIRRORED_REPEAT;
    else
        return GPU_REPEAT;
}

GPU_CULLMODE gfx_citro3d_convert_cull_mode(uint32_t culling_mode)
{
    switch (culling_mode & G_CULL_BOTH) {
        case 0:
            return GPU_CULL_NONE;
        case G_CULL_FRONT:
            return GPU_CULL_FRONT_CCW;
        default:
            return GPU_CULL_BACK_CCW;
    }
}

void gfx_citro3d_convert_mtx(float sm64_mtx[4][4], C3D_Mtx* c3d_mtx)
{ 
    c3d_mtx->r[0].x = sm64_mtx[0][0]; c3d_mtx->r[0].y = sm64_mtx[1][0]; c3d_mtx->r[0].z = sm64_mtx[2][0]; c3d_mtx->r[0].w = sm64_mtx[3][0];
    c3d_mtx->r[1].x = sm64_mtx[0][1]; c3d_mtx->r[1].y = sm64_mtx[1][1]; c3d_mtx->r[1].z = sm64_mtx[2][1]; c3d_mtx->r[1].w = sm64_mtx[3][1];
    c3d_mtx->r[2].x = sm64_mtx[0][2]; c3d_mtx->r[2].y = sm64_mtx[1][2]; c3d_mtx->r[2].z = sm64_mtx[2][2]; c3d_mtx->r[2].w = sm64_mtx[3][2];
    c3d_mtx->r[3].x = sm64_mtx[0][3]; c3d_mtx->r[3].y = sm64_mtx[1][3]; c3d_mtx->r[3].z = sm64_mtx[2][3]; c3d_mtx->r[3].w = sm64_mtx[3][3];
}

void gfx_citro3d_mtx_stereo_tilt(C3D_Mtx* dst, C3D_Mtx* src, enum Stereoscopic3dMode mode_2d, float z, float w, float strength)
{
    /** ********************** Default L/R stereo perspective function with x/y tilt removed **********************

        Preserving this to show what the proper function *should* look like.
        TODO: move to gfx_pc before RDP's mv*p happens, for proper and portable stereoscopic support

    float fovy_tan = tanf(fovy * 0.5f * M_PI / 180.0f); // equals 1.0 when FOV is 90
    float fovy_tan_aspect = fovy_tan * aspect; // equals 1.0 because we are being passed an existing mv*p matrix
    float shift = iod / (2.0f*screen);

    Mtx_Zeros(mtx); // most values revert to identity matrix anyway, including several that are necessary

    mtx->r[0].x = 1.0f / fovy_tan_aspect; // equals 1.0
    mtx->r[1].y = 1.0f / fovy_tan; // equals 1.0
    mtx->r[1].z = -mtx->r[3].z * shift / fovx_tan_invaspect; // equivalent in value to r[1].w at focallen = 1.0
    mtx->r[1].w = iod / 2.0f; // equivalent in value to r[1].z at focallen = 1.0
    mtx->r[2].z = -mtx->r[3].z * near / (near - far); // kills zbuffer
    mtx->r[2].w = near * far / (near - far); // kills clipping plane
    mtx->r[3].z = isLeftHanded ? 1.0f : -1.0f; // kills fog (viewplane data?)
    ************************************************************************************************************ */

    switch (mode_2d) {
        case STEREO_3D_NORMAL:
            break;
        case STEREO_3D_GODDARD_HAND: 
            z = (z < 0) ? -32.0f : 32.0f;
            w = (w < 0) ? -32.0f : 32.0f;
            break;
        case STEREO_3D_CREDITS:
            z = (z < 0) ? -64.0f : 64.0f;
            w = (w < 0) ? -64.0f : 64.0f;
            break;
        default:
        case STEREO_3D_SCORE_MENU: // WYATT_TODO FIXME this is broken as hell
        case STEREO_2D_NORMAL:
            strength = 0.0f;
            break;
    }

    if (strength != 0.0f) {
        static C3D_Mtx iod_mtx = C3D_STATIC_IDENTITY_MTX;

        iod_mtx.r[0].z = (z == 0) ? 0 : -1 * strength / z; // view frustum separation? (+ = deep)
        iod_mtx.r[0].w = (w == 0) ? 0 : -1 * strength / w; // camera-to-viewport separation? (+ = pop)
        Mtx_Multiply(dst, src, &iod_mtx);
    }
    else if (src != dst)
        memcpy(dst, src, sizeof(C3D_Mtx));
}

void gfx_citro3d_apply_projection_mtx_preset(C3D_Mtx* mtx)
{
    // 3DS screen is rotated 90 degrees
    Mtx_RotateZ(mtx, 0.75f*M_TAU, false);

    // 3DS depth needs a -0.5x scale, and correct the aspect ratio too
    const uint32_t float_as_int = 0x3F4CCCCD;
    Mtx_Scale(mtx, U32_AS_FLOAT(float_as_int), 1.0, -0.5);

    // z = (z + w) * -0.5
    Mtx_Multiply(mtx, mtx, &DEPTH_ADD_W_MTX);
}
