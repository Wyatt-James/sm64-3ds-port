#include <PR/gbi.h>
#include <stdio.h>

#include "gfx_cc.h"
#include "gfx_citro3d_helpers.h"

#define FAST_SINGLE_MOD(v_, max_) (((v_ >= max_) ? (v_ - max_) : (v_))) // v_ % max_, but only once.
#define ARR_INDEX_2D(x_, y_, w_) (y_ * w_ + x_)

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
