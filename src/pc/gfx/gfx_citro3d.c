#ifdef TARGET_N3DS

#include "macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "gfx_3ds.h"
#include "gfx_3ds_menu.h"

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_3ds_shaders.h"

#include "gfx_citro3d.h"
#include "color_conversion.h"
#include "texture_conversion.h"

#define TEXTURE_POOL_SIZE 4096
#define FOG_LUT_SIZE 32

#define NTSC_FRAMERATE(fps) ((float) fps * (1000.0f / 1001.0f))
#define U32_AS_FLOAT(v) (*(float*) &v)
#define ARR_INDEX_2D(x_, y_, w_) (y_ * w_ + x_)
#define FAST_SINGLE_MOD(v_, max_) (((v_ >= max_) ? (v_ - max_) : (v_))) // v_ % max_, but only once.

#define STRIDE_POSITION 3
#define STRIDE_TEXTURE  2
#define STRIDE_RGBA     4
#define STRIDE_RGB      3
#define STRIDE_FOG      STRIDE_RGBA

#define NUM_LEADING_ZEROES(v_) (__builtin_clz(v_))


static C3D_Mtx IDENTITY_MTX, DEPTH_ADD_W_MTX;

static Gfx3DSMode sCurrentGfx3DSMode = GFX_3DS_MODE_NORMAL;

static DVLB_s* sVShaderDvlb;
static shaderProgram_s sShaderProgram;
static float* sVboBuffer;

extern const u8 shader_shbin[];
extern const u32 shader_shbin_size;

struct ShaderProgram {
    uint32_t shader_id;
    uint8_t program_id;
    uint8_t buffer_id;
    struct CCFeatures cc_features;
    bool swap_input;
    C3D_TexEnv texenv0;
    C3D_TexEnv texenv1;
};

struct video_buffer {
    uint8_t id;
    float *ptr;
    uint8_t stride;
    uint32_t offset;
    shaderProgram_s shader_program; // pica vertex shader
    C3D_AttrInfo attr_info;
    C3D_BufInfo buf_info;
};

static int uLoc_tex_scale;

static struct video_buffer *current_buffer;
static struct video_buffer video_buffers[16];
static uint8_t video_buffers_size;

static struct ShaderProgram sShaderProgramPool[32];
static uint8_t sShaderProgramPoolSize;

struct FogLut {
    uint32_t id;
    C3D_FogLut lut;
};

static struct FogLut fog_lut[FOG_LUT_SIZE];
static uint8_t fog_lut_size;
static uint8_t current_fog_idx;
static uint32_t fog_color;

static u32 sTexBuf[16 * 1024] __attribute__((aligned(32)));
static C3D_Tex sTexturePool[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleS[TEXTURE_POOL_SIZE];
static float sTexturePoolScaleT[TEXTURE_POOL_SIZE];
static u32 sTextureIndex;
static int sTexUnits[2];

static int sCurTex = 0;
static int sCurShader = 0;

static bool sDepthTestOn = false;
static bool sDepthUpdateOn = false;
static bool sDepthDecal = false;
static bool sUseBlend = false;

// calling FrameDrawOn resets viewport
static int viewport_x, viewport_y;
static int viewport_width, viewport_height;
// calling SetViewport resets scissor!
static int scissor_x, scissor_y;
static int scissor_width, scissor_height;
static bool scissor;

static C3D_Mtx modelView, gameProjection, projection;
static C3D_Mtx *currentModelView      = &modelView,
               *currentGameProjection = &gameProjection;

static int original_offset;
static int s2DMode;
float iodZ = 8.0f;
float iodW = 16.0f;

// Data storage type for the screen clear buf configs
union ScreenClearBufConfig3ds {
    struct {
        enum ViewportClearBuffer top;
        enum ViewportClearBuffer bottom;
    } struc;
    enum ViewportClearBuffer array[3];
};

// Determines the clear mode for the viewports.
static union ScreenClearBufConfig3ds screen_clear_bufs = {{
    VIEW_CLEAR_BUFFER_NONE,      // top
    VIEW_CLEAR_BUFFER_NONE       // bottom
}};

// Determines the clear colors for the viewports
static union {
    struct {
        u32 top;
        u32 bottom;
    } struc;
    u32 array[3];
} screen_clear_colors = {{
    COLOR_RGBA_PARAMS_TO_RGBA32(0, 0, 0, 255),    // top: 0x000000FF
    COLOR_RGBA_PARAMS_TO_RGBA32(0, 0, 0, 255),    // bottom: 0x000000FF
}};

// Handles 3DS screen clearing
static void clear_buffers()
{
    enum ViewportClearBuffer clear_top = screen_clear_bufs.struc.top;
    enum ViewportClearBuffer clear_bottom = screen_clear_bufs.struc.bottom;

    // Clear top screen
    if (clear_top)
        C3D_RenderTargetClear(gTarget, (C3D_ClearBits) clear_top, screen_clear_colors.struc.top, 0xFFFFFFFF);
        
    // Clear right-eye view
    // We check gGfx3DSMode because clearing in 800px modes causes a crash.
    if (clear_top && (gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22))
        C3D_RenderTargetClear(gTargetRight, (C3D_ClearBits) clear_top, screen_clear_colors.struc.top, 0xFFFFFFFF);

    // Clear bottom screen only if it needs re-rendering.
    if (clear_bottom)
        C3D_RenderTargetClear(gTargetBottom, (C3D_ClearBits) clear_bottom, screen_clear_colors.struc.bottom, 0xFFFFFFFF);
}

void stereoTilt(C3D_Mtx* mtx, float z, float w)
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

    Mtx_Identity(mtx);

    switch (s2DMode) {
        case 0 : // 3D
            break;
        case 1 : // pure 2D
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, mtx);
            return;
        case 2 : // goddard hand and press start text
            z = (z < 0) ? -32.0f : 32.0f;
            w = (w < 0) ? -32.0f : 32.0f;
            break;
        case 3 : // credits
            z = (z < 0) ? -64.0f : 64.0f;
            w = (w < 0) ? -64.0f : 64.0f;
            break;
        case 4 : // the goddamn score menu
            return;
    }

    mtx->r[1].z = (z == 0) ? 0 : gSliderLevel / z; // view frustum separation? (+ = deep)
    mtx->r[1].w = (w == 0) ? 0 : gSliderLevel / w; // camera-to-viewport separation? (+ = pop)
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, mtx);
}

static void gfx_citro3d_set_2d(int mode_2d)
{
    s2DMode = mode_2d;
}

void gfx_citro3d_set_iod(float z, float w)
{
    iodZ = z;
    iodW = w;
}

static bool gfx_citro3d_z_is_from_0_to_1(void)
{
    return true;
}

static void gfx_citro3d_vertex_array_set_attribs(UNUSED struct ShaderProgram *prg)
{
}

static void gfx_citro3d_unload_shader(UNUSED struct ShaderProgram *old_prg)
{
}

static GPU_TEVSRC getTevSrc(int input, bool swap_input)
{
    switch (input)
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
    }
    return GPU_CONSTANT;
}

static void update_tex_env(struct ShaderProgram *prg, bool swap_input)
{
    if (prg->cc_features.num_inputs == 2)
    {
        C3D_TexEnvInit(&prg->texenv1);
        C3D_TexEnvColor(&prg->texenv1, 0);
        C3D_TexEnvFunc(&prg->texenv1, C3D_Both, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv1, C3D_Both, GPU_CONSTANT, 0, 0);
        C3D_TexEnvOpRgb(&prg->texenv1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvOpAlpha(&prg->texenv1, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
    }

    C3D_TexEnvInit(&prg->texenv0);
    C3D_TexEnvColor(&prg->texenv0, 0);
    if (prg->cc_features.opt_alpha && !prg->cc_features.color_alpha_same)
    {
        // RGB first
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                        getTevSrc(prg->cc_features.c[0][1], swap_input),
                                        getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
        // now Alpha
        C3D_TexEnvOpAlpha(&prg->texenv0,  GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][3], swap_input), 0, 0);
        }
        else if (prg->cc_features.do_multiply[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input), 0);
        }
        else if (prg->cc_features.do_mix[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, getTevSrc(prg->cc_features.c[1][0], swap_input),
                                          getTevSrc(prg->cc_features.c[1][1], swap_input),
                                          getTevSrc(prg->cc_features.c[1][2], swap_input));
        }
    }
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(&prg->texenv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, getTevSrc(prg->cc_features.c[0][0], swap_input),
                                         getTevSrc(prg->cc_features.c[0][1], swap_input),
                                         getTevSrc(prg->cc_features.c[0][2], swap_input));
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][1] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
        }
    }
    if (!prg->cc_features.opt_alpha)
    {
        C3D_TexEnvColor(&prg->texenv0, 0xFF000000);
        C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, GPU_CONSTANT, 0, 0);
    }

    prg->swap_input = swap_input;
}

static void update_shader(bool swap_input)
{
    struct ShaderProgram *prg = &sShaderProgramPool[sCurShader];

    // only Goddard
    if (prg->swap_input != swap_input)
    {
        update_tex_env(prg, swap_input);
    }

    if (prg->cc_features.num_inputs == 2)
    {
        C3D_SetTexEnv(0, &prg->texenv1);
        C3D_SetTexEnv(1, &prg->texenv0);
    } else {
        C3D_SetTexEnv(0, &prg->texenv0);
        C3D_TexEnvInit(C3D_GetTexEnv(1));
    }

    if (prg->cc_features.opt_fog)
    {
        C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, true);
        C3D_FogColor(fog_color);
        C3D_FogLutBind(&fog_lut[current_fog_idx].lut);
    } else {
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    }

    if (prg->cc_features.opt_texture_edge && prg->cc_features.opt_alpha)
        C3D_AlphaTest(true, GPU_GREATER, 77);
    else
        C3D_AlphaTest(true, GPU_GREATER, 0);
}

static void gfx_citro3d_load_shader(struct ShaderProgram *new_prg)
{
    sCurShader = new_prg->program_id;
    current_buffer = &video_buffers[new_prg->buffer_id];

    C3D_BindProgram(&current_buffer->shader_program);

    // Update uniforms
    uLoc_projection =     shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "projection");
    uLoc_modelView =      shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "modelView");
    uLoc_gameProjection = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "gameProjection");
    
    if (new_prg->cc_features.used_textures[0] || new_prg->cc_features.used_textures[1])
        uLoc_tex_scale = shaderInstanceGetUniformLocation((&current_buffer->shader_program)->vertexShader, "tex_scale");

    // Update buffer info
    C3D_SetBufInfo(&current_buffer->buf_info);
    C3D_SetAttrInfo(&current_buffer->attr_info);

    gfx_citro3d_vertex_array_set_attribs(new_prg);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    gfx_citro3d_apply_model_view_matrix();

    update_shader(false);
}

static uint8_t setup_new_buffer_etc(bool has_texture, UNUSED bool has_fog, bool has_alpha,
                                    bool has_color, bool has_color2)
{
    // 1 => texture
    // 2 => fog (disabled)
    // 4 => 1 color RGBA
    // 8 => 1 color RGB
    // 16 => 2 colors RGBA
    // 32 => 2 colors RGB

    u8 shader_code = 0;

    if (has_texture)
        shader_code += 1;
    // if (has_fog)
    //     shader_code += 2;
    if (has_color)
        shader_code += has_alpha ? 4 : 8;
    if (has_color2)
        shader_code += has_alpha ? 16 : 32;

    for (int i = 0; i < video_buffers_size; i++)
    {
        if (shader_code == video_buffers[i].id)
            return i;
    }

    // not found, create new
    int id = video_buffers_size;
    struct video_buffer *cb = &video_buffers[video_buffers_size++];

    cb->id = shader_code;

    u8 *current_shader_shbin = NULL;
    u32 current_shader_shbin_size = 0;

    switch(shader_code)
    {
        case 1:
            current_shader_shbin = shader_1_shbin;
            current_shader_shbin_size = shader_1_shbin_size;
            break;
        // case 3:
        //     current_shader_shbin = shader_3_shbin;
        //     current_shader_shbin_size = shader_3_shbin_size;
        //     break;
        case 4:
            current_shader_shbin = shader_4_shbin;
            current_shader_shbin_size = shader_4_shbin_size;
            break;
        case 5:
            current_shader_shbin = shader_5_shbin;
            current_shader_shbin_size = shader_5_shbin_size;
            break;
        // case 6:
        //     current_shader_shbin = shader_6_shbin;
        //     current_shader_shbin_size = shader_6_shbin_size;
        //     break;
        // case 7:
        //     current_shader_shbin = shader_7_shbin;
        //     current_shader_shbin_size = shader_7_shbin_size;
        //     break;
        case 8:
            current_shader_shbin = shader_8_shbin;
            current_shader_shbin_size = shader_8_shbin_size;
            break;
        case 9:
            current_shader_shbin = shader_9_shbin;
            current_shader_shbin_size = shader_9_shbin_size;
            break;
        case 20:
            current_shader_shbin = shader_20_shbin;
            current_shader_shbin_size = shader_20_shbin_size;
            break;
        case 41:
            current_shader_shbin = shader_41_shbin;
            current_shader_shbin_size = shader_41_shbin_size;
            break;
        default:
            current_shader_shbin = shader_shbin;
            current_shader_shbin_size = shader_shbin_size;
            fprintf(stderr, "Warning! Using default for %u\n", shader_code);
            break;
    }

    DVLB_s* sVShaderDvlb = DVLB_ParseFile((__3ds_u32*)current_shader_shbin, current_shader_shbin_size);

    shaderProgramInit(&cb->shader_program);
    shaderProgramSetVsh(&cb->shader_program, &sVShaderDvlb->DVLE[0]);

    // Configure attributes for use with the vertex shader
    int attr = 0;
    uint32_t attr_mask = 0;
    cb->stride = 0;

    // Position is always present
    {
        AttrInfo_Init(&cb->attr_info);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 3); // XYZ (W is implicitly 1.0f)
        cb->stride += STRIDE_POSITION;
    }
    if (has_texture)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 2);
        cb->stride += STRIDE_TEXTURE;
    }
    // if (has_fog)
    // {
    //     attr_mask += attr * (1 << 4 * attr);
    //     AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 4);
    //     cb->stride += STRIDE_FOG;
    // }
    if (has_color)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_UNSIGNED_BYTE, 4);
        cb->stride += 1; // 4 bytes are packed into one u32
    }
    if (has_color2)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_UNSIGNED_BYTE, 4);
        cb->stride += 1; // 4 bytes are packed into one u32
    }

    // Create the VBO (vertex buffer object)
    cb->ptr = linearAlloc(256 * 1024); // sizeof(float) * 10000 vertexes * 10 floats per vertex?
    // Configure buffers
    BufInfo_Init(&cb->buf_info);
    BufInfo_Add(&cb->buf_info, cb->ptr, cb->stride * sizeof(float), attr, attr_mask);

    return id;
}

static struct ShaderProgram *gfx_citro3d_create_and_load_new_shader(uint32_t shader_id)
{
    int id = sShaderProgramPoolSize;
    struct ShaderProgram *prg = &sShaderProgramPool[sShaderProgramPoolSize++];

    prg->program_id = id;

    prg->shader_id = shader_id;
    gfx_cc_get_features(shader_id, &prg->cc_features);

    prg->buffer_id = setup_new_buffer_etc(prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1],
                                          prg->cc_features.opt_fog,
                                          prg->cc_features.opt_alpha,
                                          prg->cc_features.num_inputs > 0,
                                          prg->cc_features.num_inputs > 1);

    update_tex_env(prg, false);

    gfx_citro3d_load_shader(prg);

    return prg;
}

static struct ShaderProgram *gfx_citro3d_lookup_shader(uint32_t shader_id)
{
    for (size_t i = 0; i < sShaderProgramPoolSize; i++)
    {
        if (sShaderProgramPool[i].shader_id == shader_id)
        {
            return &sShaderProgramPool[i];
        }
    }
    return NULL;
}

static void gfx_citro3d_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2])
{
    *num_inputs = prg->cc_features.num_inputs;
    used_textures[0] = prg->cc_features.used_textures[0];
    used_textures[1] = prg->cc_features.used_textures[1];
}

static uint32_t gfx_citro3d_new_texture(void)
{
    if (sTextureIndex == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return sTextureIndex++;
}

static void gfx_citro3d_select_texture(int tile, uint32_t texture_id)
{
    C3D_TexBind(tile, &sTexturePool[texture_id]);
    sCurTex = texture_id;
    sTexUnits[tile] = texture_id;
}

static int sTileOrder[] =
{
    0,  1,   4,  5,
    2,  3,   6,  7,

    8,  9,  12, 13,
    10, 11, 14, 15
};

// Performs a texture swizzle from RGBA32 to RGBA32.
// Pads the texture from w * h to new_w * new_h by simply repeating data.
static void performTexSwizzle(union RGBA32* src, union RGBA32* dest, u32 src_w, u32 src_h, u32 new_w, u32 new_h)
{
    for (u32 y = 0; y < new_h; y += 8)
    {
        for (u32 x = 0; x < new_w; x += 8)
        {
            for (u32 i = 0; i < 64; i++)
            {
                int x2 = i % 8; // Tiling nonsense
                int y2 = i / 8;

                u32 src_x = FAST_SINGLE_MOD(x + x2, src_w);
                u32 src_y = FAST_SINGLE_MOD(y + y2, src_h);

                union RGBA32 color = src[ARR_INDEX_2D(src_x, src_y, src_w)];
                u32 out_index = sTileOrder[x2 % 4 + y2 % 4 * 4] + 16 * (x2 / 4) + 32 * (y2 / 4);

                dest[out_index].rgba.r = color.rgba.a;
                dest[out_index].rgba.g = color.rgba.b;
                dest[out_index].rgba.b = color.rgba.g;
                dest[out_index].rgba.a = color.rgba.r;
            }
            dest += 64;
        }
    }
}

static void gfx_citro3d_upload_texture(const uint8_t *rgba32_buf, int width, int height)
{
    union RGBA32* src_as_rgba32 = (union RGBA32*) rgba32_buf;
    union RGBA32* dest_as_rgba32 = (union RGBA32*) sTexBuf;
    u32 output_width = width, output_height = height;

    // Dimensions must each be a power-of-2 >= 8.
    if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1))) {
        // Round the dimensions up to the nearest power-of-2 >= 8
        output_width  = width  < 8 ? 8 : (1 << (32 - NUM_LEADING_ZEROES(width  - 1)));
        output_height = height < 8 ? 8 : (1 << (32 - NUM_LEADING_ZEROES(height - 1)));

        if (output_width * output_height > ARRAY_COUNT(sTexBuf)) {
            printf("Scaled texture too big: %d,%d\n", output_width, output_height);
            return;
        }
    } else {
        if (width * height > ARRAY_COUNT(sTexBuf)) {
            printf("Unscaled texture too big: %d,%d\n", width, height);
            return;
        }
    }

    sTexturePoolScaleS[sCurTex] = width / (float)output_width;
    sTexturePoolScaleT[sCurTex] = height / (float)output_height;
    performTexSwizzle(src_as_rgba32, dest_as_rgba32, width, height, output_width, output_height);
    C3D_TexInit(&sTexturePool[sCurTex], output_width, output_height, GPU_RGBA8);
    C3D_TexUpload(&sTexturePool[sCurTex], sTexBuf);
    C3D_TexFlush(&sTexturePool[sCurTex]);
}

static uint32_t gfx_cm_to_opengl(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GPU_CLAMP_TO_EDGE;
    return (val & G_TX_MIRROR) ? GPU_MIRRORED_REPEAT : GPU_REPEAT;
}

static void gfx_citro3d_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    C3D_TexSetFilter(&sTexturePool[sTexUnits[tile]], linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(&sTexturePool[sTexUnits[tile]], gfx_cm_to_opengl(cms), gfx_cm_to_opengl(cmt));
}

static void update_depth()
{
    C3D_DepthTest(sDepthTestOn, GPU_LEQUAL, sDepthUpdateOn ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
    C3D_DepthMap(true, -1.0f, sDepthDecal ? -0.001f : 0);
}

static void gfx_citro3d_set_depth_test(bool depth_test)
{
    sDepthTestOn = depth_test;
    update_depth();
}

static void gfx_citro3d_set_depth_mask(bool z_upd)
{
    sDepthUpdateOn = z_upd;
    update_depth();
}

static void gfx_citro3d_set_zmode_decal(bool zmode_decal)
{
    sDepthDecal = zmode_decal;
    update_depth();
}

static void gfx_citro3d_set_viewport(int x, int y, int width, int height)
{
    if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
    {
        viewport_x = x * 2;
        viewport_y = y * 2;
        viewport_width = width * 2;
        viewport_height = height * 2;
    }
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
    {
        viewport_x = x * 2;
        viewport_y = y;
        viewport_width = width * 2;
        viewport_height = height;
    }
    else // gGfx3DSMode == GFX_3DS_MODE_NORMAL
    {
        viewport_x = x;
        viewport_y = y;
        viewport_width = width;
        viewport_height = height;
    }
}

static void gfx_citro3d_set_scissor(int x, int y, int width, int height)
{
    scissor = true;
    if (gGfx3DSMode == GFX_3DS_MODE_AA_22 || gGfx3DSMode == GFX_3DS_MODE_WIDE_AA_12)
    {
        scissor_x = x * 2;
        scissor_y = y * 2;
        scissor_width = (x + width) * 2;
        scissor_height = (y + height) * 2;
    }
    else if (gGfx3DSMode == GFX_3DS_MODE_WIDE)
    {
        scissor_x = x * 2;
        scissor_y = y;
        scissor_width = (x + width) * 2;
        scissor_height = y + height;
    }
    else // gGfx3DSMode == GFX_3DS_MODE_NORMAL
    {
        scissor_x = x;
        scissor_y = y;
        scissor_width = x + width;
        scissor_height = y + height;
    }
}

static void applyBlend()
{
    if (sUseBlend)
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    else
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

static void gfx_citro3d_set_use_alpha(bool use_alpha)
{
    sUseBlend = use_alpha;
    applyBlend();
}

static u32 vec4ToU32Color(float r, float g, float b, float a)
{
    u8 r2 = MAX(0, MIN(255, r * 255));
    u8 g2 = MAX(0, MIN(255, g * 255));
    u8 b2 = MAX(0, MIN(255, b * 255));
    u8 a2 = MAX(0, MIN(255, a * 255));
    
    return (a2 << 24) | (b2 << 16) | (g2 << 8) | r2;
}

static void renderTwoColorTris(float buf_vbo[], UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    struct ShaderProgram* curShader = &sShaderProgramPool[sCurShader];
    bool hasTex = curShader->cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    bool hasAlpha = curShader->cc_features.opt_alpha;


    const int color_1_offset = hasTex ? STRIDE_POSITION + STRIDE_TEXTURE : STRIDE_POSITION;

    // Removed a hack from before vert shaders. This new implementation
    // probably isn't completely kosher, but it works.
    // The endianness used to be reversed, but I think that this was actually an error.
    // If I set G to 0 here, it gives magenta, as expected. If endianness were reversed,
    // it would be yellow.
    union RGBA32 env_color = ((union RGBA32*) buf_vbo)[color_1_offset];
    if (!hasAlpha)
        env_color.rgba.a = 255;

    update_shader(true);
    C3D_TexEnvColor(C3D_GetTexEnv(0), env_color.u32);

    if (hasTex)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);
    
    memcpy(current_buffer->ptr + current_buffer->offset * current_buffer->stride,
        buf_vbo,
        buf_vbo_num_tris * 3 * current_buffer->stride * sizeof(float));

    C3D_DrawArrays(GPU_TRIANGLES, current_buffer->offset, buf_vbo_num_tris * 3);
    current_buffer->offset += buf_vbo_num_tris * 3;
}

static void gfx_citro3d_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (current_buffer->offset * current_buffer->stride > 256 * 1024 / 4)
    {
        printf("vertex buffer full!\n");
        return;
    }

    if (sShaderProgramPool[sCurShader].cc_features.num_inputs > 1)
    {
        renderTwoColorTris(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }

    if (sShaderProgramPool[sCurShader].cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1])
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);

    memcpy(current_buffer->ptr + current_buffer->offset * current_buffer->stride,
        buf_vbo,
        buf_vbo_num_tris * 3 * current_buffer->stride * sizeof(float));

    C3D_DrawArrays(GPU_TRIANGLES, current_buffer->offset, buf_vbo_num_tris * 3);
    current_buffer->offset += buf_vbo_num_tris * 3;
}

void gfx_citro3d_frame_draw_on(C3D_RenderTarget* target)
{
    target->used = true;
    C3D_SetFrameBuf(&target->frameBuf);
    C3D_SetViewport(viewport_y, viewport_x, viewport_height, viewport_width);
    if (scissor)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, scissor_y, scissor_x, scissor_height, scissor_width);
}

static void gfx_citro3d_draw_triangles_helper(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (gGfx3DEnabled)
    {
        // left screen
        original_offset = current_buffer->offset;
        stereoTilt(&projection, -iodZ, -iodW);
        gfx_citro3d_frame_draw_on(gTarget);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);

        // right screen
        current_buffer->offset = original_offset;
        stereoTilt(&projection, iodZ, iodW);
        gfx_citro3d_frame_draw_on(gTargetRight);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }
    gfx_citro3d_frame_draw_on(gTarget);
    gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
}

static void gfx_citro3d_init(void)
{
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0);
    C3D_DepthTest(false, GPU_LEQUAL, GPU_WRITE_ALL);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);

#ifdef VERSION_EU
    C3D_FrameRate(25);
#else
    C3D_FrameRate(30);
#endif

    Mtx_Identity(&IDENTITY_MTX);
    
    Mtx_Identity(&DEPTH_ADD_W_MTX);
    DEPTH_ADD_W_MTX.r[2].w = 1.0f;
}

static void gfx_citro3d_start_frame(void)
{
    for (int i = 0; i < video_buffers_size; i++)
    {
        video_buffers[i].offset = 0;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    scissor = false;
    // reset viewport if video mode changed
    if (gGfx3DSMode != sCurrentGfx3DSMode)
    {
        gfx_citro3d_set_viewport(0, 0, 400, 240);
        sCurrentGfx3DSMode = gGfx3DSMode;
    }

    // Due to hardware differences, the PC port always clears the depth buffer,
    // rather than just when the N64 would clear it.
    gfx_citro3d_set_viewport_clear_buffer(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_DEPTH);

    clear_buffers();

    // Reset screen clear buffer flags
    screen_clear_bufs.struc.top = 
    screen_clear_bufs.struc.bottom = VIEW_CLEAR_BUFFER_NONE;

    Mtx_Identity(&projection);

    // 3DS screen is rotated 90 degrees
    Mtx_RotateZ(&projection, 0.75f*M_TAU, false);

    // 3DS depth needs a -0.5x scale, and correct the aspect ratio too
    const uint32_t float_as_int = 0x3F4CCCCD;
    Mtx_Scale(&projection, U32_AS_FLOAT(float_as_int), 1.0, -0.5);

    // z = (z + w) * -0.5
    Mtx_Multiply(&projection, &projection, &DEPTH_ADD_W_MTX);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection,     &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_gameProjection, &IDENTITY_MTX);
}

void gfx_citro3d_convert_mtx(float sm64_mtx[4][4], C3D_Mtx* c3d_mtx)
{ 
    c3d_mtx->r[0].x = sm64_mtx[0][0]; c3d_mtx->r[0].y = sm64_mtx[1][0]; c3d_mtx->r[0].z = sm64_mtx[2][0]; c3d_mtx->r[0].w = sm64_mtx[3][0];
    c3d_mtx->r[1].x = sm64_mtx[0][1]; c3d_mtx->r[1].y = sm64_mtx[1][1]; c3d_mtx->r[1].z = sm64_mtx[2][1]; c3d_mtx->r[1].w = sm64_mtx[3][1];
    c3d_mtx->r[2].x = sm64_mtx[0][2]; c3d_mtx->r[2].y = sm64_mtx[1][2]; c3d_mtx->r[2].z = sm64_mtx[2][2]; c3d_mtx->r[2].w = sm64_mtx[3][2];
    c3d_mtx->r[3].x = sm64_mtx[0][3]; c3d_mtx->r[3].y = sm64_mtx[1][3]; c3d_mtx->r[3].z = sm64_mtx[2][3]; c3d_mtx->r[3].w = sm64_mtx[3][3];
}

void gfx_citro3d_set_model_view_matrix(float mtx[4][4])
{
    gfx_citro3d_convert_mtx(mtx, &modelView);
}

void gfx_citro3d_set_game_projection_matrix(float mtx[4][4])
{
    gfx_citro3d_convert_mtx(mtx, &gameProjection);
}

void gfx_citro3d_apply_model_view_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, currentModelView);
}

void gfx_citro3d_apply_game_projection_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_gameProjection, currentGameProjection);
}

void gfx_citro3d_temporarily_use_identity_matrix(bool use_identity)
{
    currentModelView      = use_identity ? &IDENTITY_MTX : &modelView;
    currentGameProjection = use_identity ? &IDENTITY_MTX : &gameProjection;
}

void gfx_citro3d_set_backface_culling_mode(uint32_t culling_mode)
{
    GPU_CULLMODE mode;
    switch (culling_mode & G_CULL_BOTH) {
        case 0:
            mode = GPU_CULL_NONE;
            break;
        case G_CULL_FRONT:
            mode = GPU_CULL_FRONT_CCW;
            break;
        default:
            mode = GPU_CULL_BACK_CCW;
            break;
    }
    C3D_CullFace(mode);
}

static void gfx_citro3d_on_resize(void)
{
}

static void gfx_citro3d_end_frame(void)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection,     &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_gameProjection, &IDENTITY_MTX);
    C3D_CullFace(GPU_CULL_NONE);

    // TOOD: draw the minimap here
    gfx_3ds_menu_draw(current_buffer->ptr, current_buffer->offset, gShowConfigMenu);

    // set the texenv back
    update_shader(false);

    C3D_FrameEnd(0); // Swap is handled automatically within this function
}

static void gfx_citro3d_finish_render(void)
{
}

static void gfx_citro3d_set_fog(uint16_t from, uint16_t to)
{
    // dumb enough
    uint32_t id = (from << 16) | to;
    // current already loaded
    if (fog_lut[current_fog_idx].id == id)
        return;
    // lut already calculated
    for (int i = 0; i < fog_lut_size; i++)
    {
        if (fog_lut[i].id == id)
        {
            current_fog_idx = i;
            return;
        }
    }
    // new lut required
    if (fog_lut_size == FOG_LUT_SIZE)
    {
        printf("Fog exhausted!\n");
        return;
    }

    current_fog_idx = fog_lut_size++;
    (&fog_lut[current_fog_idx])->id = id;

    // FIXME: The near/far factors are personal preference
    // BOB:  6400, 59392 => 0.16, 116
    // JRB:  1280, 64512 => 0.80, 126
    FogLut_Exp(&fog_lut[current_fog_idx].lut, 0.05f, 1.5f, 1024 / (float)from, ((float)to) / 512);
}

static void gfx_citro3d_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    fog_color = (a << 24) | (b << 16) | (g << 8) | r; // Why is this reversed? Weird endianness?
}

void gfx_citro3d_set_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    screen_clear_colors.array[viewport] = COLOR_RGBA_PARAMS_TO_RGBA32(r, g, b, a);
}

void gfx_citro3d_set_clear_color_RGBA32(enum ViewportId3DS viewport, u32 color)
{
    screen_clear_colors.array[viewport] = color;
}

void gfx_citro3d_set_viewport_clear_buffer(enum ViewportId3DS viewport, enum ViewportClearBuffer mode)
{
    screen_clear_bufs.array[viewport] |= mode;
}

struct GfxRenderingAPI gfx_citro3d_api = {
    gfx_citro3d_z_is_from_0_to_1,
    gfx_citro3d_unload_shader,
    gfx_citro3d_load_shader,
    gfx_citro3d_create_and_load_new_shader,
    gfx_citro3d_lookup_shader,
    gfx_citro3d_shader_get_info,
    gfx_citro3d_new_texture,
    gfx_citro3d_select_texture,
    gfx_citro3d_upload_texture,
    gfx_citro3d_set_sampler_parameters,
    gfx_citro3d_set_depth_test,
    gfx_citro3d_set_depth_mask,
    gfx_citro3d_set_zmode_decal,
    gfx_citro3d_set_viewport,
    gfx_citro3d_set_scissor,
    gfx_citro3d_set_use_alpha,
    gfx_citro3d_draw_triangles_helper,
    gfx_citro3d_init,
    gfx_citro3d_on_resize,
    gfx_citro3d_start_frame,
    gfx_citro3d_end_frame,
    gfx_citro3d_finish_render,
    gfx_citro3d_set_fog,
    gfx_citro3d_set_fog_color,
    gfx_citro3d_set_2d,
    gfx_citro3d_set_iod
};

#endif
