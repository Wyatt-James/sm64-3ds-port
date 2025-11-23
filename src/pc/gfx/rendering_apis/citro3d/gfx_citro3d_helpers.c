#include "gfx_citro3d_helpers.h"

#include <PR/gbi.h>
#include <stdio.h>

#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/pc_macros.h"

#define FAST_SINGLE_MOD(v_, max_) (((v_ >= max_) ? (v_ - max_) : (v_))) // v_ % max_, but only once.
#define ARR_INDEX_2D(x_, y_, w_) (x_ + (y_ * w_))
#define U32_AS_FLOAT(v_) (*(float*) &v_)

typedef struct
{
    u32 x, y;
} ScaleFactor;

// Add W to Z coordinate
static const C3D_Mtx DEPTH_ADD_W_MTX = {
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

                dest[out_index] = BSWAP_32(src[in_index]);
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

                dest[out_index] = BSWAP_16(src[in_index]);
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

static void configure_tev_internal(struct CCFeatures* cc_features, C3D_TexEnv* texenv, bool swap_input, C3D_TexEnvMode mode)
{
    if (cc_features->do_single[0])
    {
        C3D_TexEnvFunc(texenv, mode, GPU_REPLACE);
        C3D_TexEnvSrc (texenv, mode, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[3], swap_input), 0, 0);

        C3D_TexEnvOpRgb(texenv,
            cc_features->cc.rgb[3] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
            GPU_TEVOP_RGB_SRC_COLOR,
            GPU_TEVOP_RGB_SRC_COLOR);
    }
    else if (cc_features->do_multiply[0])
    {
        C3D_TexEnvFunc(texenv, mode, GPU_MODULATE);
        C3D_TexEnvSrc (texenv, mode, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[0], swap_input),
                                     citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[2], swap_input), 0);
        C3D_TexEnvOpRgb(texenv,
            cc_features->cc.rgb[0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
            cc_features->cc.rgb[2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
            GPU_TEVOP_RGB_SRC_COLOR);
    }
    else if (cc_features->do_mix[0])
    {
        C3D_TexEnvFunc(texenv, mode, GPU_INTERPOLATE);
        C3D_TexEnvSrc (texenv, mode, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[0], swap_input),
                                     citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[1], swap_input),
                                     citro3d_helpers_cc_input_to_tev_src(cc_features->cc.rgb[2], swap_input));
        C3D_TexEnvOpRgb(texenv,
            cc_features->cc.rgb[0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
            cc_features->cc.rgb[1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
            cc_features->cc.rgb[2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
    }
}

C3D_TexEnv citro3d_helpers_configure_tex_env(struct CCFeatures* cc_features)
{
    C3D_TexEnv texenv_;
    C3D_TexEnv* texenv = &texenv_;

    const bool swap_input = (cc_features->num_inputs == 2) ? true : false;
    union RGBA32 color = { .u32 = 0 };

    C3D_TexEnvInit(texenv);

    if (cc_features->opt_alpha && !cc_features->color_alpha_same)
    {
        // RGB
        configure_tev_internal(cc_features, texenv, swap_input, C3D_RGB);

        // Alpha
        C3D_TexEnvOpAlpha(texenv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (cc_features->do_single[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_REPLACE);
            C3D_TexEnvSrc (texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[3], swap_input), 0, 0);
        }
        else if (cc_features->do_multiply[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc (texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[0], swap_input),
                                              citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[2], swap_input), 0);
        }
        else if (cc_features->do_mix[1])
        {
            C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc (texenv, C3D_Alpha, citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[0], swap_input),
                                              citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[1], swap_input),
                                              citro3d_helpers_cc_input_to_tev_src(cc_features->cc.alpha[2], swap_input));
        }
    }

    // RGB and Alpha are same
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(texenv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        configure_tev_internal(cc_features, texenv, swap_input, C3D_Both);
    }

    // If alpha is disabled, overwrite its prior settings
    if (!cc_features->opt_alpha)
    {
        color.a = 0xFF;
        C3D_TexEnvFunc(texenv, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvSrc(texenv, C3D_Alpha, GPU_CONSTANT, 0, 0);
    }
    
    C3D_TexEnvColor(texenv, color.u32);

    return texenv_;
}

C3D_TexEnv citro3d_helpers_configure_two_color_tex_env(void)
{
    C3D_TexEnv texenv;

    C3D_TexEnvInit(&texenv);
    C3D_TexEnvColor(&texenv, 0);
    C3D_TexEnvFunc(&texenv, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(&texenv, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvOpRgb(&texenv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
    C3D_TexEnvOpAlpha(&texenv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);

    return texenv;
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

void citro3d_helpers_convert_mtx(C3D_Mtx* restrict c3d_mtx, float sm64_mtx[restrict 4][4])
{
    for (int i = 0; i < 4; i++) {
        c3d_mtx->r[i].x = sm64_mtx[0][i];
        c3d_mtx->r[i].y = sm64_mtx[1][i];
        c3d_mtx->r[i].z = sm64_mtx[2][i];
        c3d_mtx->r[i].w = sm64_mtx[3][i];
    }
}

void citro3d_helpers_copy_and_transpose_mtx(C3D_Mtx* restrict dst, C3D_Mtx* restrict src)
{
    for (int i = 1; i <= 4; i++) {
        dst->m[4  - i] = src->m[4 * i - 1]; //  3  2  1  0  |  3  7 11 15
        dst->m[8  - i] = src->m[4 * i - 2]; //  7  6  5  4  |  2  6 10 14
        dst->m[12 - i] = src->m[4 * i - 3]; // 11 10  9  8  |  1  5  9 13
        dst->m[16 - i] = src->m[4 * i - 4]; // 15 14 13 12  |  0  4  8 12
    }
}

void citro3d_helpers_mtx_stereo_tilt(C3D_Mtx* restrict dst, C3D_Mtx* restrict src, enum Stereoscopic3dMode mode_2d, float z, float w, float strength)
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

// WYATT_TODO this scaling is ridiculous and should be done at the GFX WAPI level,
// but the PC port doesn't support anamorphic resolutions, breaking rectangle drawing.
static inline ScaleFactor gfx_mode_scale_factor(N3DS_DisplayMode gfx_mode)
{
    switch (gfx_mode) {
        default:                      // Same as 400x240
        case N3DS_DISPLAY_3D:         // Same as 400x240
        case N3DS_DISPLAY_2D_400_240: return (ScaleFactor) {0, 0};
        case N3DS_DISPLAY_2D_800_240: return (ScaleFactor) {1, 0};
        case N3DS_DISPLAY_2D_800_480: return (ScaleFactor) {1, 1};
    }
}

void citro3d_helpers_convert_viewport_settings(ScreenDimensions* viewport_config, N3DS_DisplayMode gfx_mode, int x, int y, int width, int height)
{
    ScaleFactor scale = gfx_mode_scale_factor(gfx_mode);
    viewport_config->x       = x      << scale.x;
    viewport_config->y       = y      << scale.y;
    viewport_config->width   = width  << scale.x;
    viewport_config->height  = height << scale.y;
}

void citro3d_helpers_convert_scissor_settings(ScreenDimensions* scissor_config, N3DS_DisplayMode gfx_mode, int x, int y, int width, int height)
{
    ScaleFactor scale = gfx_mode_scale_factor(gfx_mode);
    scissor_config->x1     = x << scale.x;
    scissor_config->y1     = y << scale.y;
    scissor_config->x2     = (x + width)  << scale.x;
    scissor_config->y2     = (y + height) << scale.y;
}

// Placed in the COLD section because it should be called at most once per-frame.
COLD void citro3d_helpers_rescale_screen_dimensions(ScreenDimensions* c, N3DS_DisplayMode old_gfx_mode, N3DS_DisplayMode new_gfx_mode)
{
    ScaleFactor old = gfx_mode_scale_factor(old_gfx_mode);
    ScaleFactor new = gfx_mode_scale_factor(new_gfx_mode);
    c->x      = c->x      >> old.x << new.x;
    c->y      = c->y      >> old.y << new.y;
    c->width  = c->width  >> old.x << new.x;
    c->height = c->height >> old.y << new.y;
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

GPU_FOGMODE citro3d_helpers_convert_fog_mode(bool enable)
{
    return enable ? GPU_FOG : GPU_NO_FOG;
}

enum Emu64ColorCombinerSource citro3d_helpers_convert_cc_mapping_to_emu64(uint8_t cc_mapping, bool fog_enabled)
{
    // Note: the Peach painting uses LoD for RGB only, not alpha

    switch (cc_mapping) {
        case CC_PRIM:
            return EMU64_CC_PRIM;
        case CC_SHADE:
            if (fog_enabled)
                return EMU64_CC_1;
            else
                return EMU64_CC_SHADE;
        case CC_ENV:
            return EMU64_CC_ENV;
        case CC_LOD:
            return EMU64_CC_LOD;
        case CC_0:
        default:
            return EMU64_CC_0;
    }
}

float citro3d_helpers_convert_cc_mapping_to_emu64_float(uint8_t cc_mapping, bool fog_enabled)
{
    return citro3d_helpers_convert_cc_mapping_to_emu64(cc_mapping, fog_enabled);
}

C3D_AttrInfo citro3d_helpers_init_attr_info(const struct n3ds_attribute_data* attributes)
{
    C3D_AttrInfo attr_info;

    AttrInfo_Init(&attr_info);

    for (size_t i = 0; i < attributes->num_attribs; i++) {
        GPU_FORMATS format = attributes->data[i].format;
        int count = attributes->data[i].count;
        AttrInfo_AddLoader(&attr_info, i, format, count);
    }

    return attr_info;
}

ShaderProgram citro3d_helpers_init_shader(const struct n3ds_shader_info* shader_info, VertexBuffer* vb, size_t vbo_size)
{
    ShaderProgram prog;
    prog.vertex_buffer = vb;

    const struct n3ds_shader_vbo_info* vbo_info = &shader_info->vbo_info;
    
    if (vb)
    {
        vb->attr_info = citro3d_helpers_init_attr_info(&shader_info->vbo_info.attributes);

        if (vbo_size > 0)
            vb->ptr = linearAlloc(vbo_size);

        
        BufInfo_Init(&vb->buf_info);
        BufInfo_Add(&vb->buf_info, vb->ptr, vbo_info->stride * EMU64_STRIDE_UNIT_SIZE, vb->attr_info.attrCount, vb->attr_info.permutation);
    }
    
    // It is assumed that these will not fail
    shaderProgramInit(&prog.pica_shader_program);
    shaderProgramSetVsh(&prog.pica_shader_program, &shader_info->binary->dvlb->DVLE[shader_info->dvle_index]);
    shaderProgramSetGsh(&prog.pica_shader_program, NULL, 0);

    return prog;
}

void citro3d_helpers_free_shader(ShaderProgram* prog)
{
    if (prog->vertex_buffer != NULL && prog->vertex_buffer->ptr != NULL)
    {
        linearFree(prog->vertex_buffer->ptr);
        prog->vertex_buffer->ptr = NULL;
    }
}

bool citro3d_helpers_load_t3x_texture(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
    Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, false);
    if (!t3x)
        return false;
    Tex3DS_TextureFree(t3x);
    return true;
}

void citro3d_helpers_init_cc(ColorCombiner* cc, ColorCombinerId cc_id)
{
    union CCInputMapping mapping;

    {
        CCShaderId shader_id;
        gfx_cc_generate_cc(cc_id, &mapping, &shader_id);
        gfx_cc_get_features(shader_id, &cc->cc_features);
    }

    // If num inputs >= 2, we need to reverse the mappings' A and B params (hack for goddard)
    if (cc->cc_features.num_inputs >= 2) {
        union CCInputMapping mapping_temp;
        for (int i = 0; i <= 1; i++) {
            mapping_temp.arr[i][0] = mapping.arr[i][1];
            mapping_temp.arr[i][1] = mapping.arr[i][0];

            mapping.arr[i][0] = mapping_temp.arr[i][0];
            mapping.arr[i][1] = mapping_temp.arr[i][1];
        }
    }

    cc->cc_id = cc_id;

    cc->c3d_shader_input_mapping.c1_rgb = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.rgb[0], false);
    cc->c3d_shader_input_mapping.c2_rgb = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.rgb[1], false);

    cc->c3d_shader_input_mapping.c1_a = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.alpha[0], cc->cc_features.opt_fog);
    cc->c3d_shader_input_mapping.c2_a = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.alpha[1], cc->cc_features.opt_fog);

    // Fixes the pause tint being too light.
    cc->use_env_color = mapping.rgb[1] == CC_ENV;

    // N3DS only cares about the first two mappings, so we want to make an identifier for specifically this to enhance performance
    // RGBA32 works fine since it's four u8s
    cc->cc_mapping_identifier = (union RGBA32) {
        .r = mapping.rgb[0],
        .g = mapping.rgb[1],
        .b = mapping.alpha[0],
        .a = mapping.alpha[1],
    }.u32;

    // Preconfigure TEV settings
    cc->texenv = citro3d_helpers_configure_tex_env(&cc->cc_features);
}
