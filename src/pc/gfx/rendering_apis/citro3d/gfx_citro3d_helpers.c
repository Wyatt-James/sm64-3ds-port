#include <PR/gbi.h>
#include <stdio.h>

#include "gfx_citro3d_helpers.h"
#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"

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
#define ARR_INDEX_2D(x_, y_, w_) (x_ + (y_ * w_))
#define U32_AS_FLOAT(v_) (*(float*) &v_)

#define BSWAP32(v_) (__builtin_bswap32(v_))        // u32
#define BSWAP16(v_) (__builtin_bswap16(v_))        // u16
#define NUM_LEADING_ZEROES(v_) (__builtin_clz(v_)) // u32
#define NUM_ONES(v_) (__builtin_popcount(v_))      // u32

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

static const int texture_tile_order[4][4] =
{
    {0,  1,   4,  5},
    {2,  3,   6,  7},

    {8,  9,  12, 13},
    {10, 11, 14, 15}
};

uint8_t citro3d_helpers_calculate_shader_code(bool has_texture,
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

const struct n3ds_shader_info* citro3d_helpers_get_shader_info_from_shader_code(uint8_t shader_code)
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
            shader = &emu64_shader_1;
            break;
        // case 3:
        //     shader = &emu64_shader_3;
        //     break;
        case 4:
            shader = &emu64_shader_4;
            break;
        case 5:
            shader = &emu64_shader_5;
            break;
        // case 6:
        //     shader = &emu64_shader_6;
        //     break;
        // case 7:
        //     shader = &emu64_shader_7;
        //     break;
        case 8:
            shader = &emu64_shader_8;
            break;
        case 9:
            shader = &emu64_shader_9;
            break;
        case 20:
            shader = &emu64_shader_20;
            break;
        case 41:
            shader = &emu64_shader_41;
            break;
        default:
            shader = &emu64_shader_9;
            fprintf(stderr, "Warning! Using default shader for %u\n", shader_code);
            break;
    }

    return shader;
}

struct TextureSize citro3d_helpers_adjust_texture_dimensions(struct TextureSize input_size, size_t unit_size, size_t buffer_size)
{
    struct TextureSize result = input_size; // Struct copy
    result.success = true;
    bool padded = false;
    
    // Dimensions must be >= 8
    if (result.width  < 8 || result.height < 8) padded = true;
    if (result.width  < 8) result.width  = 8;
    if (result.height < 8) result.height = 8;

    // Dimensions must be powers of 2
    if (NUM_ONES(result.width | result.height << 16) != 2) {
        result.width  = (uint16_t) (1 << (32 - NUM_LEADING_ZEROES(result.width  - 1))); // NUM_LEADING_ZEROES does int promotion.
        result.height = (uint16_t) (1 << (32 - NUM_LEADING_ZEROES(result.height - 1)));
        padded = true;

        if (result.width * result.height * unit_size > buffer_size) {
            printf("Scaled tex too big: %d, %d\n", (int) result.width, (int) result.height);
            result.success = false;
        }
    } else {
        if (input_size.width * input_size.height * unit_size > buffer_size) {
            printf("Unscaled tex too big: %d, %d\n", (int) input_size.width, (int) input_size.height);
            result.success = false;
        }
    }
    
    if (padded)
        printf("Padding tex from %d,%d to %d,%d\n", input_size.width, input_size.height, result.width, result.height);

    return result;
}

void citro3d_helpers_pad_and_tile_texture_u32(uint32_t* src,
                                          uint32_t* dest,
                                          struct TextureSize src_size,
                                          struct TextureSize new_size)
{
    uint32_t src_w = src_size.width,
             src_h = src_size.height,
             new_w = new_size.width,
             new_h = new_size.height;


    for (u32 y = 0; y < new_h; y += 8) {
        for (u32 x = 0; x < new_w; x += 8) {
            for (u32 i = 0; i < 64; i++)
            {
                int tile_x = i % 8;
                int tile_y = i / 8;

                u32 src_x = FAST_SINGLE_MOD(x + tile_x, src_w);
                u32 src_y = FAST_SINGLE_MOD(y + tile_y, src_h);

                u32 in_index = ARR_INDEX_2D(src_x, src_y, src_w);
                u32 out_index = texture_tile_order[tile_y % 4][tile_x % 4] + 16 * (tile_x / 4) + 32 * (tile_y / 4);

                dest[out_index] = BSWAP32(src[in_index]);
            }
            dest += 64;
        }
    }
}

void citro3d_helpers_pad_and_tile_texture_u16(uint16_t* src,
                                          uint16_t* dest,
                                          struct TextureSize src_size,
                                          struct TextureSize new_size)
{
    uint32_t src_w = src_size.width,
             src_h = src_size.height,
             new_w = new_size.width,
             new_h = new_size.height;

    for (u32 y = 0; y < new_h; y += 8) {
        for (u32 x = 0; x < new_w; x += 8) {
            for (u32 i = 0; i < 64; i++)
            {
                int tile_x = i % 8;
                int tile_y = i / 8;

                u32 src_x = FAST_SINGLE_MOD(x + tile_x, src_w);
                u32 src_y = FAST_SINGLE_MOD(y + tile_y, src_h);

                u32 in_index = ARR_INDEX_2D(src_x, src_y, src_w);
                u32 out_index = texture_tile_order[tile_y % 4][tile_x % 4] + 16 * (tile_x / 4) + 32 * (tile_y / 4);

                dest[out_index] = BSWAP16(src[in_index]);
            }
            dest += 64;
        }
    }
}

void citro3d_helpers_pad_and_tile_texture_u8(uint8_t* src,
                                         uint8_t* dest,
                                         struct TextureSize src_size,
                                         struct TextureSize new_size)
{
    uint32_t src_w = src_size.width,
             src_h = src_size.height,
             new_w = new_size.width,
             new_h = new_size.height;

    for (u32 y = 0; y < new_h; y += 8) {
        for (u32 x = 0; x < new_w; x += 8) {
            for (u32 i = 0; i < 64; i++)
            {
                int tile_x = i % 8;
                int tile_y = i / 8;

                u32 src_x = FAST_SINGLE_MOD(x + tile_x, src_w);
                u32 src_y = FAST_SINGLE_MOD(y + tile_y, src_h);

                u32 in_index = ARR_INDEX_2D(src_x, src_y, src_w);
                u32 out_index = texture_tile_order[tile_y % 4][tile_x % 4] + 16 * (tile_x / 4) + 32 * (tile_y / 4);

                dest[out_index] = src[in_index];
            }
            dest += 64;
        }
    }
}

union RGBA32 citro3d_helpers_get_env_color_from_vbo(float buf_vbo[], struct CCFeatures* cc_features)
{
    const bool hasTex = cc_features->used_textures[0] || cc_features->used_textures[1];
    const bool hasAlpha = cc_features->opt_alpha;
    const int color_1_offset = hasTex ? EMU64_STRIDE_POSITION + EMU64_STRIDE_TEXTURE : EMU64_STRIDE_POSITION;

    // Removed a hack from before vertex shaders. This new implementation
    // probably isn't completely kosher, but it works.
    // The endianness used to be reversed, but I think that this was actually an error.
    // If I set G to 0 here, it gives magenta, as expected. If endianness were reversed,
    // it would be yellow.
    union RGBA32 env_color = ((union RGBA32*) buf_vbo)[color_1_offset];
    if (!hasAlpha)
        env_color.a = 255;

    return env_color;
}

GPU_TEVSRC citro3d_helpers_cc_input_to_tev_src(int cc_input, bool swap_input)
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

void citro3d_helpers_configure_tex_env_slot_0(struct CCFeatures* cc_features, C3D_TexEnv* texenv)
{
    const bool swap_input = (cc_features->num_inputs == 2) ? true : false;

    C3D_TexEnvInit(texenv);
    C3D_TexEnvColor(texenv, 0);
    if (cc_features->opt_alpha && !cc_features->color_alpha_same)
    {
        // RGB first
        if (cc_features->do_single[0])
        {
            C3D_TexEnvFunc(texenv, C3D_RGB, GPU_REPLACE);
            C3D_TexEnvSrc(texenv, C3D_RGB, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][3], swap_input), 0, 0);
            if (cc_features->c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(texenv, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(texenv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (cc_features->do_multiply[0])
        {
            C3D_TexEnvFunc(texenv, C3D_RGB, GPU_MODULATE);
            C3D_TexEnvSrc(texenv, C3D_RGB, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][0], swap_input),
                                        citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(texenv,
                cc_features->c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (cc_features->do_mix[0])
        {
            C3D_TexEnvFunc(texenv, C3D_RGB, GPU_INTERPOLATE);
            C3D_TexEnvSrc(texenv, C3D_RGB, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][0], swap_input),
                                        citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][1], swap_input),
                                        citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][2], swap_input));
            C3D_TexEnvOpRgb(texenv,
                cc_features->c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
        // now Alpha
        C3D_TexEnvOpAlpha(texenv,  GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (cc_features->do_single[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_REPLACE);
            C3D_TexEnvSrc(texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][3], swap_input), 0, 0);
        }
        else if (cc_features->do_multiply[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc(texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][0], swap_input),
                                          citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][2], swap_input), 0);
        }
        else if (cc_features->do_mix[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc(texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][0], swap_input),
                                          citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][1], swap_input),
                                          citro3d_helpers_cc_input_to_tev_src(cc_features->c[1][2], swap_input));
        }
    }
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(texenv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (cc_features->do_single[0])
        {
            C3D_TexEnvFunc(texenv, C3D_Both, GPU_REPLACE);
            C3D_TexEnvSrc(texenv, C3D_Both, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][3], swap_input), 0, 0);
            if (cc_features->c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(texenv, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(texenv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (cc_features->do_multiply[0])
        {
            C3D_TexEnvFunc(texenv, C3D_Both, GPU_MODULATE);
            C3D_TexEnvSrc(texenv, C3D_Both, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][0], swap_input),
                                         citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(texenv,
                cc_features->c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (cc_features->do_mix[0])
        {
            C3D_TexEnvFunc(texenv, C3D_Both, GPU_INTERPOLATE);
            C3D_TexEnvSrc(texenv, C3D_Both, citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][0], swap_input),
                                         citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][1], swap_input),
                                         citro3d_helpers_cc_input_to_tev_src(cc_features->c[0][2], swap_input));
            C3D_TexEnvOpRgb(texenv,
                cc_features->c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                cc_features->c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
    }
    if (!cc_features->opt_alpha)
    {
        C3D_TexEnvColor(texenv, 0xFF000000);
        C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvSrc(texenv, C3D_Alpha, GPU_CONSTANT, 0, 0);
    }
}

void citro3d_helpers_configure_tex_env_slot_1(C3D_TexEnv* texenv)
{
    C3D_TexEnvInit(texenv);
    C3D_TexEnvColor(texenv, 0);
    C3D_TexEnvFunc(texenv, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(texenv, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvOpRgb(texenv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
    C3D_TexEnvOpAlpha(texenv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
}

GPU_TEXTURE_WRAP_PARAM citro3d_helpers_convert_texture_clamp_mode(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GPU_CLAMP_TO_EDGE;
    else if (val & G_TX_MIRROR)
        return GPU_MIRRORED_REPEAT;
    else
        return GPU_REPEAT;
}

GPU_CULLMODE citro3d_helpers_convert_cull_mode(uint32_t culling_mode)
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

void citro3d_helpers_convert_mtx(float sm64_mtx[4][4], C3D_Mtx* c3d_mtx)
{ 
    c3d_mtx->r[0].x = sm64_mtx[0][0]; c3d_mtx->r[0].y = sm64_mtx[1][0]; c3d_mtx->r[0].z = sm64_mtx[2][0]; c3d_mtx->r[0].w = sm64_mtx[3][0];
    c3d_mtx->r[1].x = sm64_mtx[0][1]; c3d_mtx->r[1].y = sm64_mtx[1][1]; c3d_mtx->r[1].z = sm64_mtx[2][1]; c3d_mtx->r[1].w = sm64_mtx[3][1];
    c3d_mtx->r[2].x = sm64_mtx[0][2]; c3d_mtx->r[2].y = sm64_mtx[1][2]; c3d_mtx->r[2].z = sm64_mtx[2][2]; c3d_mtx->r[2].w = sm64_mtx[3][2];
    c3d_mtx->r[3].x = sm64_mtx[0][3]; c3d_mtx->r[3].y = sm64_mtx[1][3]; c3d_mtx->r[3].z = sm64_mtx[2][3]; c3d_mtx->r[3].w = sm64_mtx[3][3];
}

void citro3d_helpers_mtx_stereo_tilt(C3D_Mtx* dst, C3D_Mtx* src, enum Stereoscopic3dMode mode_2d, float z, float w, float strength)
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
        case STEREO_MODE_3D:
            break;
        case STEREO_MODE_3D_GODDARD_HAND: 
            z = (z < 0) ? -32.0f : 32.0f;
            w = (w < 0) ? -32.0f : 32.0f;
            break;
        case STEREO_MODE_3D_CREDITS:
            z = (z < 0) ? -64.0f : 64.0f;
            w = (w < 0) ? -64.0f : 64.0f;
            break;
        default:
        case STEREO_MODE_2D:
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

void citro3d_helpers_apply_projection_mtx_preset(C3D_Mtx* mtx)
{
    // 3DS screen is rotated 90 degrees
    Mtx_RotateZ(mtx, 0.75f*M_TAU, false);

    // 3DS depth needs a -0.5x scale, and correct the aspect ratio too
    const uint32_t aspect_ratio_factor = 0x3F4CCCCD;
    Mtx_Scale(mtx, U32_AS_FLOAT(aspect_ratio_factor), 1.0f, -0.5f);

    // z = (z + w) * -0.5
    Mtx_Multiply(mtx, mtx, &DEPTH_ADD_W_MTX);
}

void citro3d_helpers_convert_viewport_settings(struct ViewportConfig* viewport_config, Gfx3DSMode gfx_mode, int x, int y, int width, int height)
{
    if (gfx_mode == GFX_3DS_MODE_AA_22 || gfx_mode == GFX_3DS_MODE_WIDE_AA_12)
    {
        viewport_config->x = x * 2;
        viewport_config->y = y * 2;
        viewport_config->width = width * 2;
        viewport_config->height = height * 2;
    }
    else if (gfx_mode == GFX_3DS_MODE_WIDE)
    {
        viewport_config->x = x * 2;
        viewport_config->y = y;
        viewport_config->width = width * 2;
        viewport_config->height = height;
    }
    else // gfx_mode == GFX_3DS_MODE_NORMAL
    {
        viewport_config->x = x;
        viewport_config->y = y;
        viewport_config->width = width;
        viewport_config->height = height;
    }
}

void citro3d_helpers_convert_scissor_settings(struct ScissorConfig* scissor_config, Gfx3DSMode gfx_mode, int x, int y, int width, int height)
{
    scissor_config->enable = true;
    if (gfx_mode == GFX_3DS_MODE_AA_22 || gfx_mode == GFX_3DS_MODE_WIDE_AA_12)
    {
        scissor_config->x1 = x * 2;
        scissor_config->y1 = y * 2;
        scissor_config->x2 = (x + width) * 2;
        scissor_config->y2 = (y + height) * 2;
    }
    else if (gfx_mode == GFX_3DS_MODE_WIDE)
    {
        scissor_config->x1 = x * 2;
        scissor_config->y1 = y;
        scissor_config->x2 = (x + width) * 2;
        scissor_config->y2 = y + height;
    }
    else // gfx_mode == GFX_3DS_MODE_NORMAL
    {
        scissor_config->x1 = x;
        scissor_config->y1 = y;
        scissor_config->x2 = x + width;
        scissor_config->y2 = y + height;
    }
}

void citro3d_helpers_convert_iod_settings(struct IodConfig* iod_config, float z, float w)
{
    iod_config->z = z;
    iod_config->w = w;
}

enum Stereoscopic3dMode citro3d_helpers_convert_2d_mode(int mode_2d)
{
    if (mode_2d < 0 || mode_2d > STEREO_MODE_COUNT)
        mode_2d = STEREO_MODE_3D;

    return (enum Stereoscopic3dMode) mode_2d;
}
