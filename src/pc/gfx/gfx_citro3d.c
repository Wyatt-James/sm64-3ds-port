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
#include "gfx_citro3d_helpers.h"
#include "color_conversion.h"

#define TEXTURE_POOL_SIZE 4096
#define MAX_VIDEO_BUFFERS 16
#define MAX_FOG_LUTS 32
#define MAX_SHADER_PROGRAMS 32

#define NTSC_FRAMERATE(fps_) ((float) fps_ * (1000.0f / 1001.0f))

#define NUM_LEADING_ZEROES(v_) (__builtin_clz(v_))
#define BSWAP32(v_) (__builtin_bswap32(v_))

static DVLB_s* sVShaderDvlb;
static shaderProgram_s sShaderProgram;
static float* sVboBuffer;

struct UniformLocations uniform_locations;

struct ShaderProgram {
    uint32_t shader_id;
    uint8_t program_id;
    uint8_t buffer_id;
    struct CCFeatures cc_features;
    bool swap_input;
    C3D_TexEnv texenv0;
    C3D_TexEnv texenv1;
    struct UniformLocations uniform_locations;
};

struct VideoBuffer {
    const struct n3ds_shader_info* shader_info;
    float *ptr;
    uint32_t offset;
    shaderProgram_s shader_program; // pica vertex shader
    C3D_AttrInfo attr_info;
    C3D_BufInfo buf_info;
};

struct FogLutHandle {
    uint32_t id;
    C3D_FogLut lut;
};

struct ScreenClearConfig {
    enum ViewportClearBuffer bufs;
    union RGBA32 color;
    uint32_t depth;
};

union ScreenClearConfigsN3ds {
    struct {
        struct ScreenClearConfig top;
        struct ScreenClearConfig bottom;
    };
    struct ScreenClearConfig array[2];
};

struct GameMtxSet {
    C3D_Mtx model_view, game_projection;
};

static struct VideoBuffer video_buffers[MAX_VIDEO_BUFFERS];
static struct VideoBuffer *current_video_buffer = NULL;
static uint8_t num_video_buffers = 0;

static struct ShaderProgram sShaderProgramPool[MAX_SHADER_PROGRAMS];
static uint8_t sShaderProgramPoolSize;

static struct FogLutHandle fog_lut[MAX_FOG_LUTS];
static struct FogLutHandle* current_fog_lut = NULL;
static uint8_t num_fog_luts = 0;
static union RGBA32 fog_color;

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

// calling FrameDrawOn resets viewport
struct ViewportConfig viewport_config;

// calling SetViewport resets scissor
struct ScissorConfig scissor_config = { .enable = false };

static struct IodConfig iod_config = { .z = 8.0f, .w = 16.0f };

static enum Stereoscopic3dMode s2DMode = 0;

// Selectable groups of game matrix sets
static struct GameMtxSet game_matrix_sets[NUM_MATRIX_SETS];

// The current matrices.
// Projection is the 3DS-specific P-matrix.
// Model_view is the N64 MV-matrix.
// Game_projection is the N64 P-matrix.
static C3D_Mtx  projection,
               *model_view      = &game_matrix_sets[DEFAULT_MATRIX_SET].model_view,
               *game_projection = &game_matrix_sets[DEFAULT_MATRIX_SET].game_projection;

// Determines the clear config for each viewport.
static union ScreenClearConfigsN3ds screen_clear_configs = {
    .top    = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
    .bottom = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
};

// Handles 3DS screen clearing
static void clear_buffers()
{
    C3D_ClearBits clear_top    = (C3D_ClearBits) screen_clear_configs.top.bufs,
                  clear_bottom = (C3D_ClearBits) screen_clear_configs.bottom.bufs;

    uint32_t color_top    = BSWAP32(screen_clear_configs.top.color.u32),
             color_bottom = BSWAP32(screen_clear_configs.bottom.color.u32),
             depth_top    = screen_clear_configs.top.depth,
             depth_bottom = screen_clear_configs.bottom.depth;

    // Clear top screen
    if (clear_top)
        C3D_RenderTargetClear(gTarget, clear_top, color_top, depth_top);
        
    // Clear right-eye view
    // We check gGfx3DSMode because clearing in 800px modes causes a crash.
    if (clear_top && (gGfx3DSMode == GFX_3DS_MODE_NORMAL || gGfx3DSMode == GFX_3DS_MODE_AA_22))
        C3D_RenderTargetClear(gTargetRight, clear_top, color_top, depth_top);

    // Clear bottom screen only if it needs re-rendering.
    if (clear_bottom)
        C3D_RenderTargetClear(gTargetBottom, clear_bottom, color_bottom, depth_bottom);
}

static void gfx_citro3d_set_2d_mode(int mode_2d)
{
    s2DMode = gfx_citro3d_convert_2d_mode(mode_2d);
}

void gfx_citro3d_set_iod(float z, float w)
{
    gfx_citro3d_convert_iod_settings(&iod_config, z, w);
}

static void gfx_citro3d_unload_shader(UNUSED struct ShaderProgram *old_prg)
{
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
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][0], swap_input),
                                        gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_RGB, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_RGB, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][0], swap_input),
                                        gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][1], swap_input),
                                        gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][2], swap_input));
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
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][3], swap_input), 0, 0);
        }
        else if (prg->cc_features.do_multiply[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][0], swap_input),
                                          gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][2], swap_input), 0);
        }
        else if (prg->cc_features.do_mix[1])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Alpha, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Alpha, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][0], swap_input),
                                          gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][1], swap_input),
                                          gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[1][2], swap_input));
        }
    }
    else
    {
        // RBGA
        C3D_TexEnvOpAlpha(&prg->texenv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
        if (prg->cc_features.do_single[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_REPLACE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][3], swap_input), 0, 0);
            if (prg->cc_features.c[0][3] == SHADER_TEXEL0A)
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_ALPHA, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
            else
                C3D_TexEnvOpRgb(&prg->texenv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_multiply[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_MODULATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][0], swap_input),
                                         gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][2], swap_input), 0);
            C3D_TexEnvOpRgb(&prg->texenv0,
                prg->cc_features.c[0][0] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                prg->cc_features.c[0][2] == SHADER_TEXEL0A ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
                GPU_TEVOP_RGB_SRC_COLOR);
        }
        else if (prg->cc_features.do_mix[0])
        {
            C3D_TexEnvFunc(&prg->texenv0, C3D_Both, GPU_INTERPOLATE);
            C3D_TexEnvSrc(&prg->texenv0, C3D_Both, gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][0], swap_input),
                                         gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][1], swap_input),
                                         gfx_citro3d_cc_input_to_tev_src(prg->cc_features.c[0][2], swap_input));
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
        C3D_FogColor(fog_color.u32);
        C3D_FogLutBind(&current_fog_lut->lut);
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
    current_video_buffer = &video_buffers[new_prg->buffer_id];

    C3D_BindProgram(&current_video_buffer->shader_program);

    // Update uniforms
    memcpy(&uniform_locations, &new_prg->uniform_locations, sizeof(struct UniformLocations));

    // Update buffer info
    C3D_SetBufInfo(&current_video_buffer->buf_info);
    C3D_SetAttrInfo(&current_video_buffer->attr_info);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection);
    gfx_citro3d_apply_model_view_matrix();
    gfx_citro3d_apply_game_projection_matrix();

    update_shader(false);
}

static uint8_t setup_video_buffer(const struct n3ds_shader_info* shader_info)
{
    // Search for the existing shader to avoid loading it twice
    for (int i = 0; i < num_video_buffers; i++)
    {
        if (shader_info->identifier == video_buffers[i].shader_info->identifier)
            return i;
    }

    // not found, create new
    int id = num_video_buffers++;
    struct VideoBuffer *cb = &video_buffers[id];
    cb->shader_info = shader_info;

    DVLB_s* sVShaderDvlb = DVLB_ParseFile((__3ds_u32*)shader_info->shader_binary, *shader_info->shader_size); 

    shaderProgramInit(&cb->shader_program);
    shaderProgramSetVsh(&cb->shader_program, &sVShaderDvlb->DVLE[0]);

    // Configure attributes for use with the vertex shader
    int attr = 0;
    uint32_t attr_mask = 0;

    AttrInfo_Init(&cb->attr_info);

    if (shader_info->vbo_info.has_position)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_SHORT, 4); // XYZW (W is set to 1.0f in the shader)
    }
    if (shader_info->vbo_info.has_texture)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 2);
    }
    // if (shader_info->vbo_info.has_fog)
    // {
    //     attr_mask += attr * (1 << 4 * attr);
    //     AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_FLOAT, 4);
    // }
    if (shader_info->vbo_info.has_color1)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_UNSIGNED_BYTE, 4);
    }
    if (shader_info->vbo_info.has_color2)
    {
        attr_mask += attr * (1 << 4 * attr);
        AttrInfo_AddLoader(&cb->attr_info, attr++, GPU_UNSIGNED_BYTE, 4);
    }

    // Create the VBO (vertex buffer object)
    cb->ptr = linearAlloc(256 * 1024); // sizeof(float) * 10000 vertexes * 10 floats per vertex?
    cb->offset = 0;

    // Configure buffers
    BufInfo_Init(&cb->buf_info);
    BufInfo_Add(&cb->buf_info, cb->ptr, shader_info->vbo_info.stride * sizeof(float), attr, attr_mask);

    return id;
}

// Finds and saves the uniform locations of the given shader. Must already be initialized.
static void get_uniform_locations(struct ShaderProgram *prg)
{
    shaderProgram_s shader_program = video_buffers[prg->buffer_id].shader_program;

    prg->uniform_locations.projection_mtx =      shaderInstanceGetUniformLocation(shader_program.vertexShader, "projection_mtx");
    prg->uniform_locations.model_view_mtx =      shaderInstanceGetUniformLocation(shader_program.vertexShader, "model_view_mtx");
    prg->uniform_locations.game_projection_mtx = shaderInstanceGetUniformLocation(shader_program.vertexShader, "game_projection_mtx");
    
    if (prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1])
        prg->uniform_locations.tex_scale = shaderInstanceGetUniformLocation(shader_program.vertexShader, "tex_scale");
}

static struct ShaderProgram *gfx_citro3d_create_and_load_new_shader(uint32_t shader_id)
{
    int id = sShaderProgramPoolSize;
    struct ShaderProgram *prg = &sShaderProgramPool[sShaderProgramPoolSize++];

    prg->program_id = id;

    prg->shader_id = shader_id;
    gfx_cc_get_features(shader_id, &prg->cc_features);

    uint8_t shader_code = gfx_citro3d_calculate_shader_code(prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1],
                                                prg->cc_features.opt_fog,
                                                prg->cc_features.opt_alpha,
                                                prg->cc_features.num_inputs > 0,
                                                prg->cc_features.num_inputs > 1);
    const struct n3ds_shader_info* shader = get_shader_info_from_shader_code(shader_code);
    prg->buffer_id = setup_video_buffer(shader);

    update_tex_env(prg, false);
    get_uniform_locations(prg);
    
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
    gfx_citro3d_pad_texture_rgba32(src_as_rgba32, dest_as_rgba32, width, height, output_width, output_height);
    C3D_TexInit(&sTexturePool[sCurTex], output_width, output_height, GPU_RGBA8);
    C3D_TexUpload(&sTexturePool[sCurTex], sTexBuf);
    C3D_TexFlush(&sTexturePool[sCurTex]);
}

static void gfx_citro3d_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    C3D_TexSetFilter(&sTexturePool[sTexUnits[tile]], linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(&sTexturePool[sTexUnits[tile]], gfx_citro3d_convert_texture_clamp_mode(cms), gfx_citro3d_convert_texture_clamp_mode(cmt));
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
    gfx_citro3d_convert_viewport_settings(&viewport_config, gGfx3DSMode, x, y, width, height);
}

static void gfx_citro3d_set_scissor(int x, int y, int width, int height)
{
    gfx_citro3d_convert_scissor_settings(&scissor_config, gGfx3DSMode, x, y, width, height);
}

static void gfx_citro3d_set_use_alpha(bool use_alpha)
{
    if (use_alpha)
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    else
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

static void adjust_state_for_two_color_tris(float buf_vbo[])
{
    struct ShaderProgram* curShader = &sShaderProgramPool[sCurShader];
    const bool hasTex = curShader->cc_features.used_textures[0] || sShaderProgramPool[sCurShader].cc_features.used_textures[1];
    const bool hasAlpha = curShader->cc_features.opt_alpha;
    const int color_1_offset = hasTex ? STRIDE_POSITION + STRIDE_TEXTURE : STRIDE_POSITION;

    // Removed a hack from before vertex shaders. This new implementation
    // probably isn't completely kosher, but it works.
    // The endianness used to be reversed, but I think that this was actually an error.
    // If I set G to 0 here, it gives magenta, as expected. If endianness were reversed,
    // it would be yellow.
    union RGBA32 env_color = ((union RGBA32*) buf_vbo)[color_1_offset];
    if (!hasAlpha)
        env_color.a = 255;

    update_shader(true); // WYATT_TODO should this be reversed after this function?
    C3D_TexEnvColor(C3D_GetTexEnv(0), env_color.u32);
}

static void adjust_state_for_one_color_tris()
{
    // Nothing needs to be in here at the moment.
}

static void gfx_citro3d_draw_triangles(float buf_vbo[], size_t buf_vbo_num_tris)
{
    struct ShaderProgram* curShader = &sShaderProgramPool[sCurShader];
    const bool hasTex = curShader->cc_features.used_textures[0] || curShader->cc_features.used_textures[1];

    if (sShaderProgramPool[sCurShader].cc_features.num_inputs > 1)
        adjust_state_for_two_color_tris(buf_vbo);
    else
        adjust_state_for_one_color_tris();

    if (hasTex)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uniform_locations.tex_scale,
            sTexturePoolScaleS[sCurTex], -sTexturePoolScaleT[sCurTex], 1, 1);

    C3D_DrawArrays(GPU_TRIANGLES, current_video_buffer->offset, buf_vbo_num_tris * 3);
}

void gfx_citro3d_select_render_target(C3D_RenderTarget* target)
{
    target->used = true;
    C3D_SetFrameBuf(&target->frameBuf);
    C3D_SetViewport(viewport_config.y, viewport_config.x, viewport_config.height, viewport_config.width);
    if (scissor_config.enable)
        C3D_SetScissor(GPU_SCISSOR_NORMAL, scissor_config.y1, scissor_config.x1, scissor_config.y2, scissor_config.x2); // WYATT_TODO FIXME bug? should the last two params be reversed?
}

static void gfx_citro3d_draw_triangles_helper(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (current_video_buffer->offset * current_video_buffer->shader_info->vbo_info.stride > 256 * 1024 / 4)
    {
        printf("vertex buffer full!\n");
        return;
    }

    // WYATT_TODO actually prevent buffer overruns.

    float* const buf_vbo_head = current_video_buffer->ptr + current_video_buffer->offset * current_video_buffer->shader_info->vbo_info.stride;
    memcpy(buf_vbo_head, buf_vbo, buf_vbo_len * sizeof(float));

    if (gGfx3DEnabled)
    {
        // left screen
        Mtx_Identity(&projection);
        gfx_citro3d_mtx_stereo_tilt(&projection, &projection, s2DMode, -iod_config.z, -iod_config.w, gSliderLevel);
        gfx_citro3d_apply_projection_mtx_preset(&projection);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection);
        gfx_citro3d_select_render_target(gTarget);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_num_tris);

        // right screen
        Mtx_Identity(&projection);
        gfx_citro3d_mtx_stereo_tilt(&projection, &projection, s2DMode, iod_config.z, iod_config.w, gSliderLevel);
        gfx_citro3d_apply_projection_mtx_preset(&projection);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection);
        gfx_citro3d_select_render_target(gTargetRight);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_num_tris);
    } else {
        gfx_citro3d_select_render_target(gTarget);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_num_tris);
    }

    current_video_buffer->offset += buf_vbo_num_tris * 3;
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

    // Default all mat sets to identity
    for (int i = 0; i < NUM_MATRIX_SETS; i++) {
        memcpy(&game_matrix_sets[i].game_projection, &IDENTITY_MTX, sizeof(C3D_Mtx));
        memcpy(&game_matrix_sets[i].model_view,      &IDENTITY_MTX, sizeof(C3D_Mtx));
    }
}

static void gfx_citro3d_start_frame(void)
{
    for (int i = 0; i < num_video_buffers; i++)
    {
        video_buffers[i].offset = 0;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    scissor_config.enable = false;
    
    static Gfx3DSMode current_gfx_mode = GFX_3DS_MODE_NORMAL;

    // reset viewport if video mode changed
    if (gGfx3DSMode != current_gfx_mode)
    {
        gfx_citro3d_set_viewport(0, 0, 400, 240);
        current_gfx_mode = gGfx3DSMode;
    }

    // Due to hardware differences, the PC port always clears the depth buffer,
    // rather than just when the N64 would clear it.
    gfx_citro3d_set_viewport_clear_buffer(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_DEPTH);

    clear_buffers();

    // Reset screen clear buffer flags
    screen_clear_configs.top.bufs = 
    screen_clear_configs.bottom.bufs = VIEW_CLEAR_BUFFER_NONE;

    Mtx_Identity(&projection);
    gfx_citro3d_apply_projection_mtx_preset(&projection);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx,      &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.model_view_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.game_projection_mtx, &IDENTITY_MTX);
}

void gfx_citro3d_set_model_view_matrix(float mtx[4][4])
{
    gfx_citro3d_convert_mtx(mtx, model_view);
}

void gfx_citro3d_set_game_projection_matrix(float mtx[4][4])
{
    gfx_citro3d_convert_mtx(mtx, game_projection);
}

void gfx_citro3d_apply_model_view_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.model_view_mtx, model_view);
}

void gfx_citro3d_apply_game_projection_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.game_projection_mtx, game_projection);
}

void gfx_citro3d_select_matrix_set(uint32_t matrix_set_id)
{
    model_view      = &game_matrix_sets[matrix_set_id].model_view;
    game_projection = &game_matrix_sets[matrix_set_id].game_projection;
}

void gfx_citro3d_set_backface_culling_mode(uint32_t culling_mode)
{
    C3D_CullFace(gfx_citro3d_convert_cull_mode(culling_mode));
}

static void gfx_citro3d_end_frame(void)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.model_view_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.game_projection_mtx, &IDENTITY_MTX);
    C3D_CullFace(GPU_CULL_NONE);

    // TOOD: draw the minimap here
    gfx_3ds_menu_draw(current_video_buffer->ptr, current_video_buffer->offset, gShowConfigMenu);

    // set the texenv back
    update_shader(false);

    C3D_FrameEnd(0); // Swap is handled automatically within this function
}

static void gfx_citro3d_set_fog(uint16_t from, uint16_t to)
{
    // dumb enough
    uint32_t id = (from << 16) | to;

    // current already loaded
    if (current_fog_lut != NULL && current_fog_lut->id == id)
        return;

    // lut already calculated
    for (int i = 0; i < num_fog_luts; i++)
    {
        if (fog_lut[i].id == id)
        {
            current_fog_lut = &fog_lut[i];
            return;
        }
    }

    // new lut required
    if (num_fog_luts == MAX_FOG_LUTS)
    {
        printf("Fog exhausted!\n");
        return;
    }

    current_fog_lut = &fog_lut[num_fog_luts++];
    current_fog_lut->id = id;

    // FIXME: The near/far factors are personal preference
    // BOB:  6400, 59392 => 0.16, 116
    // JRB:  1280, 64512 => 0.80, 126
    FogLut_Exp(&current_fog_lut->lut, 0.05f, 1.5f, 1024 / (float)from, ((float)to) / 512);
}

static void gfx_citro3d_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    fog_color.r = r;
    fog_color.g = g;
    fog_color.b = b;
    fog_color.a = a;
}

void gfx_citro3d_set_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    screen_clear_configs.array[viewport].color.r = r;
    screen_clear_configs.array[viewport].color.g = g;
    screen_clear_configs.array[viewport].color.b = b;
    screen_clear_configs.array[viewport].color.a = a;
}

void gfx_citro3d_set_clear_color_u32(enum ViewportId3DS viewport, uint32_t color)
{
    screen_clear_configs.array[viewport].color.u32 = color;
}

void gfx_citro3d_set_clear_color_RGBA32(enum ViewportId3DS viewport, union RGBA32 color)
{
    screen_clear_configs.array[viewport].color.u32 = color.u32;
}

void gfx_citro3d_set_viewport_clear_depth(enum ViewportId3DS viewport, uint32_t depth)
{
    screen_clear_configs.array[viewport].depth = depth;
}

void gfx_citro3d_set_viewport_clear_buffer(enum ViewportId3DS viewport, enum ViewportClearBuffer mode)
{
    screen_clear_configs.array[viewport].bufs |= mode;
}

struct GfxRenderingAPI gfx_citro3d_api = {
    stub_return_true, // z_is_from_0_to_1,
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
    stub_void, // on_resize
    gfx_citro3d_start_frame,
    gfx_citro3d_end_frame,
    stub_void, // finish_render
    gfx_citro3d_set_fog,
    gfx_citro3d_set_fog_color,
    gfx_citro3d_set_2d_mode,
    gfx_citro3d_set_iod
};

#endif
