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
#include "gfx_citro3d_fog_cache.h"
#include "color_conversion.h"

#define TEXTURE_POOL_SIZE 4096
#define MAX_VIDEO_BUFFERS 16
#define MAX_SHADER_PROGRAMS 32

// Valid values: ON, SELECTABLE, OFF. Never define an option to be 0, or the #else will not work!
#define OPT_ON 1
#define OPT_SELECTABLE 2
#define OPT_OFF 3

#define OPTIMIZATION_SETTING OPT_ON

#if OPTIMIZATION_SETTING == OPT_ON
#define ENABLE_OPTIMIZATIONS          true  // If disabled, optimizations are forced OFF.
#define FORCE_OPTIMIZATIONS           true  // If enabled, optimizations are forced ON, unless ENABLE_OPTIMIZATIONS is false.

#elif OPTIMIZATION_SETTING == OPT_SELECTABLE
#define ENABLE_OPTIMIZATIONS          true  // If disabled, optimizations are forced OFF.
#define FORCE_OPTIMIZATIONS           false // If enabled, optimizations are forced ON, unless ENABLE_OPTIMIZATIONS is false.

#elif OPTIMIZATION_SETTING == OPT_OFF
#define ENABLE_OPTIMIZATIONS          false // If disabled, optimizations are forced OFF.
#define FORCE_OPTIMIZATIONS           false // If enabled, optimizations are forced ON, unless ENABLE_OPTIMIZATIONS is false.

#else
prevent compile // Invalid OPTIMIZATION_SETTING
#endif

#define NTSC_FRAMERATE(fps_) ((float) fps_ * (1000.0f / 1001.0f))

#define NUM_LEADING_ZEROES(v_) (__builtin_clz(v_))
#define BSWAP32(v_) (__builtin_bswap32(v_))
#define BOOL_INVERT(v_) do {v_ = !v_;} while (0)
#define OPT_ENABLED(flag_)   (ENABLE_OPTIMIZATIONS && ((FORCE_OPTIMIZATIONS) || (flag_))) // Optimization flag. Use: if (OPT_ENABLED(flag)) {fast path} else {slow path}
#define OPT_DISABLED(flag_)  (!OPT_ENABLED(flag_))                                        // Optimization flag. Use: if (OPT_DISABLED(flag)) {slow path} else {fast path}

// N64 shader program
struct ShaderProgram {
    uint32_t shader_id; // N64 shader_id
    struct VideoBuffer* video_buffer;
    struct CCFeatures cc_features;
    C3D_TexEnv texenvs[2];
};

// 3DS shader's video buffer. May be shared by multiple ShaderPrograms.
struct VideoBuffer {
    const struct n3ds_shader_info* shader_info;
    struct UniformLocations uniform_locations;
    shaderProgram_s shader_program; // pica vertex shader
    float *ptr;
    uint32_t offset;
    C3D_AttrInfo attr_info;
    C3D_BufInfo buf_info;
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

struct TexHandle {
    C3D_Tex c3d_tex;
    float scale_s, scale_t;
};

struct OptimizationFlags {
    bool consecutive_fog;
    bool consecutive_fog_lut;
    bool consecutive_stereo_p_mtx;
};

struct RenderState {
    bool fog_enabled;
    C3D_FogLut* fog_lut;
};

static struct VideoBuffer video_buffers[MAX_VIDEO_BUFFERS];
static struct VideoBuffer* current_video_buffer = NULL;
static uint8_t num_video_buffers = 0;

static struct ShaderProgram shader_program_pool[MAX_SHADER_PROGRAMS];
static struct ShaderProgram* current_shader_program = NULL;
static uint8_t num_shader_programs = 0;
struct UniformLocations uniform_locations; // Uniform locations for the current shader program

struct FogCache fog_cache;

static struct TexHandle texture_pool[TEXTURE_POOL_SIZE];
static struct TexHandle* current_texture = &texture_pool[0];
static struct TexHandle* gpu_textures[2] = { &texture_pool[0], &texture_pool[0] };
static uint32_t api_texture_index = 0;

static bool sDepthTestOn = false;
static bool sDepthUpdateOn = false;
static bool sDepthDecal = false;

// calling FrameDrawOn resets viewport
struct ViewportConfig viewport_config = { 0, 0, 0, 0 };

// calling SetViewport resets scissor
struct ScissorConfig scissor_config = { .enable = false };

static enum Stereoscopic3dMode s2DMode = 0;
static struct IodConfig iod_config = { .z = 8.0f, .w = 16.0f };
static bool recalculate_stereo_p_mtx = true;

// Selectable groups of game matrix sets
static struct GameMtxSet game_matrix_sets[NUM_MATRIX_SETS];

// The current matrices.
// Projection is the 3DS-specific P-matrix.
// Model_view is the N64 MV-matrix.
// Game_projection is the N64 P-matrix.
static C3D_Mtx  projection_2d        = C3D_STATIC_IDENTITY_MTX,
                projection_left      = C3D_STATIC_IDENTITY_MTX,
                projection_right     = C3D_STATIC_IDENTITY_MTX,
               *model_view      = &game_matrix_sets[DEFAULT_MATRIX_SET].model_view,
               *game_projection = &game_matrix_sets[DEFAULT_MATRIX_SET].game_projection;

// Determines the clear config for each viewport.
static union ScreenClearConfigsN3ds screen_clear_configs = {
    .top    = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
    .bottom = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
};

struct OptimizationFlags optimize = {
    .consecutive_fog = true,
    .consecutive_fog_lut = true,
    .consecutive_stereo_p_mtx = true
};

struct RenderState render_state = {
    .fog_enabled = 0xFF,
    .fog_lut = NULL
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
    recalculate_stereo_p_mtx = true;
}

void gfx_citro3d_set_iod(float z, float w)
{
    gfx_citro3d_convert_iod_settings(&iod_config, z, w);
    recalculate_stereo_p_mtx = true;
}

static void gfx_citro3d_unload_shader(UNUSED struct ShaderProgram *old_prg)
{
}

static void gfx_citro3d_load_shader(struct ShaderProgram *prg)
{
    current_shader_program = prg;
    current_video_buffer = prg->video_buffer;

    C3D_BindProgram(&current_video_buffer->shader_program);

    // Update uniforms
    memcpy(&uniform_locations, &current_video_buffer->uniform_locations, sizeof(struct UniformLocations));

    // Update buffer info
    C3D_SetBufInfo(&current_video_buffer->buf_info);
    C3D_SetAttrInfo(&current_video_buffer->attr_info);

    // Set up matrices
    if (!gGfx3DEnabled)
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection_2d);
    gfx_citro3d_apply_model_view_matrix();
    gfx_citro3d_apply_game_projection_matrix();

    // Configure TEV
    if (prg->cc_features.num_inputs == 2)
    {
        C3D_SetTexEnv(0, &prg->texenvs[1]);
        C3D_SetTexEnv(1, &prg->texenvs[0]);
    } else {
        C3D_SetTexEnv(0, &prg->texenvs[0]);
        C3D_TexEnvInit(C3D_GetTexEnv(1));
    }

    if (render_state.fog_enabled != prg->cc_features.opt_fog || OPT_DISABLED(optimize.consecutive_fog)) {
        render_state.fog_enabled  = prg->cc_features.opt_fog;
        GPU_FOGMODE mode = prg->cc_features.opt_fog ? GPU_FOG : GPU_NO_FOG;
        C3D_FogGasMode(mode, GPU_PLAIN_DENSITY, true);
    }

    if (prg->cc_features.opt_texture_edge && prg->cc_features.opt_alpha)
        C3D_AlphaTest(true, GPU_GREATER, 77);
    else
        C3D_AlphaTest(true, GPU_GREATER, 0);
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

    DVLB_s* vertex_shader_dvlb = DVLB_ParseFile((__3ds_u32*)shader_info->shader_binary, *shader_info->shader_size); 

    shaderProgramInit(&cb->shader_program);
    shaderProgramSetVsh(&cb->shader_program, &vertex_shader_dvlb->DVLE[0]);

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
    struct VideoBuffer* video_buffer = prg->video_buffer;
    shaderProgram_s shader_program = video_buffer->shader_program;

    video_buffer->uniform_locations.projection_mtx =      shaderInstanceGetUniformLocation(shader_program.vertexShader, "projection_mtx");
    video_buffer->uniform_locations.model_view_mtx =      shaderInstanceGetUniformLocation(shader_program.vertexShader, "model_view_mtx");
    video_buffer->uniform_locations.game_projection_mtx = shaderInstanceGetUniformLocation(shader_program.vertexShader, "game_projection_mtx");
    
    if (prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1])
        video_buffer->uniform_locations.tex_scale = shaderInstanceGetUniformLocation(shader_program.vertexShader, "tex_scale");
}

static struct ShaderProgram *gfx_citro3d_create_and_load_new_shader(uint32_t shader_id)
{
    int id = num_shader_programs++;
    struct ShaderProgram *prg = &shader_program_pool[id];

    prg->shader_id = shader_id;
    gfx_cc_get_features(shader_id, &prg->cc_features);

    uint8_t shader_code = gfx_citro3d_calculate_shader_code(prg->cc_features.used_textures[0] || prg->cc_features.used_textures[1],
                                                prg->cc_features.opt_fog,
                                                prg->cc_features.opt_alpha,
                                                prg->cc_features.num_inputs > 0,
                                                prg->cc_features.num_inputs > 1);
    const struct n3ds_shader_info* shader = get_shader_info_from_shader_code(shader_code);
    prg->video_buffer = &video_buffers[setup_video_buffer(shader)];

    // Preconfigure TEV settings
    gfx_citro3d_configure_tex_env(&prg->cc_features, &prg->texenvs[0], &prg->texenvs[1]);
    get_uniform_locations(prg);
    
    gfx_citro3d_load_shader(prg);
    return prg;
}

static struct ShaderProgram *gfx_citro3d_lookup_shader(uint32_t shader_id)
{
    for (size_t i = 0; i < num_shader_programs; i++)
    {
        struct ShaderProgram* prog = &shader_program_pool[i];

        if (prog->shader_id == shader_id)
            return prog;
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
    if (api_texture_index == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return api_texture_index++;
}

// WYATT_TODO move this optimiaztion to the emulation layer.
static void gfx_citro3d_select_texture(int tex_slot, uint32_t texture_id)
{
    current_texture = &texture_pool[texture_id];
    if (gpu_textures[tex_slot] != current_texture) {
        gpu_textures[tex_slot]  = current_texture;
        C3D_TexBind(tex_slot, &current_texture->c3d_tex);
    }
}

static void gfx_citro3d_upload_texture(const uint8_t *rgba32_buf, int width, int height)
{
    static union RGBA32 tex_staging_buffer[16 * 1024] __attribute__((aligned(32)));

    union RGBA32* src_as_rgba32 = (union RGBA32*) rgba32_buf;
    u32 output_width = width, output_height = height;

    // Dimensions must each be a power-of-2 >= 8.
    if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1))) {
        // Round the dimensions up to the nearest power-of-2 >= 8
        output_width  = width  < 8 ? 8 : (1 << (32 - NUM_LEADING_ZEROES(width  - 1)));
        output_height = height < 8 ? 8 : (1 << (32 - NUM_LEADING_ZEROES(height - 1)));

        if (output_width * output_height > ARRAY_COUNT(tex_staging_buffer)) {
            printf("Scaled texture too big: %d,%d\n", output_width, output_height);
            return;
        }
    } else {
        if (width * height > ARRAY_COUNT(tex_staging_buffer)) {
            printf("Unscaled texture too big: %d,%d\n", width, height);
            return;
        }
    }

    C3D_Tex* c3d_tex = &current_texture->c3d_tex;

    current_texture->scale_s = width / (float)output_width;
    current_texture->scale_t = height / (float)output_height;

    gfx_citro3d_pad_texture_rgba32(src_as_rgba32, tex_staging_buffer, width, height, output_width, output_height);

    C3D_TexInit(c3d_tex, output_width, output_height, GPU_RGBA8);
    C3D_TexUpload(c3d_tex, tex_staging_buffer);
    C3D_TexFlush(c3d_tex);
}

static void gfx_citro3d_set_sampler_parameters(int tex_slot, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    C3D_Tex* tex = &gpu_textures[tex_slot]->c3d_tex;
    C3D_TexSetFilter(tex, linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(tex, gfx_citro3d_convert_texture_clamp_mode(cms), gfx_citro3d_convert_texture_clamp_mode(cmt));
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

static void gfx_citro3d_draw_triangles(float buf_vbo[], size_t buf_vbo_num_tris)
{
    struct CCFeatures* cc_features = &current_shader_program->cc_features;

    const bool hasTex = cc_features->used_textures[0] || cc_features->used_textures[1];

    if (cc_features->num_inputs > 1)
        C3D_TexEnvColor(C3D_GetTexEnv(0), gfx_citro3d_get_env_color_from_vbo(buf_vbo, cc_features).u32);

    if (hasTex)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, uniform_locations.tex_scale, current_texture->scale_s, -current_texture->scale_t, 1, 1);

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

// Called only when 3D is enabled.
static void recalculate_stereo_matrices()
{
    if (s2DMode == STEREO_MODE_2D) {
        Mtx_Identity(&projection_left);
        gfx_citro3d_apply_projection_mtx_preset(&projection_left);
    } else {
        Mtx_Identity(&projection_left);
        gfx_citro3d_mtx_stereo_tilt(&projection_left, &projection_left, s2DMode, -iod_config.z, -iod_config.w, gSliderLevel);
        gfx_citro3d_apply_projection_mtx_preset(&projection_left);

        Mtx_Identity(&projection_right);
        gfx_citro3d_mtx_stereo_tilt(&projection_right, &projection_right, s2DMode, iod_config.z, iod_config.w, gSliderLevel);
        gfx_citro3d_apply_projection_mtx_preset(&projection_right);
    }
}

static void gfx_citro3d_draw_triangles_helper(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (current_video_buffer->offset * current_video_buffer->shader_info->vbo_info.stride > 256 * 1024 / sizeof(float))
    {
        printf("vertex buffer full!\n");
        return;
    }

    // WYATT_TODO actually prevent buffer overruns.
    float* const buf_vbo_head = current_video_buffer->ptr + current_video_buffer->offset * current_video_buffer->shader_info->vbo_info.stride;
    memcpy(buf_vbo_head, buf_vbo, buf_vbo_len * sizeof(float));

    if (gGfx3DEnabled)
    {
        if (recalculate_stereo_p_mtx || OPT_DISABLED(optimize.consecutive_stereo_p_mtx)) {
            recalculate_stereo_p_mtx = false;
            recalculate_stereo_matrices();

            if (s2DMode == STEREO_MODE_2D)
                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection_left);
        }

        // left screen
        if (s2DMode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection_left);

        gfx_citro3d_select_render_target(gTarget);
        gfx_citro3d_draw_triangles(buf_vbo, buf_vbo_num_tris);

        // right screen
        if (s2DMode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.projection_mtx, &projection_right);

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

    Mtx_Identity(&projection_2d);
    gfx_citro3d_apply_projection_mtx_preset(&projection_2d);

    fog_cache_init(&fog_cache);

    render_state.fog_enabled = 0xFF;
    render_state.fog_lut = NULL;
    optimize.consecutive_fog = true;
    optimize.consecutive_fog_lut = true;
    optimize.consecutive_stereo_p_mtx = true;
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

    static float prev_slider_level = -1.0f;
    if (prev_slider_level != gSliderLevel) {
        prev_slider_level  = gSliderLevel;
        recalculate_stereo_p_mtx = true;
    }

    // Due to hardware differences, the PC port always clears the depth buffer,
    // rather than just when the N64 would clear it.
    gfx_citro3d_set_viewport_clear_buffer_mode(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_DEPTH);

    clear_buffers();

    // Reset screen clear buffer flags
    screen_clear_configs.top.bufs = 
    screen_clear_configs.bottom.bufs = VIEW_CLEAR_BUFFER_NONE;

    if (!gGfx3DEnabled)
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_locations.model_view_mtx, &projection_2d);
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

    C3D_FrameEnd(0); // Swap is handled automatically within this function
}

static void gfx_citro3d_set_fog(uint16_t from, uint16_t to)
{
    // FIXME: The near/far factors are personal preference
    // BOB:  6400, 59392 => 0.16, 116
    // JRB:  1280, 64512 => 0.80, 126
    enum FogCacheResult res = fog_cache_load(&fog_cache, from, to);
    C3D_FogLut* lut = fog_cache_current(&fog_cache);

    if (res == FOGCACHE_MISS)
        FogLut_Exp(lut, 0.05f, 1.5f, 1024 / (float)from, ((float)to) / 512);

    // Hefty savings in areas with fog, especially JRB.
    if (render_state.fog_lut != lut || OPT_DISABLED(optimize.consecutive_fog_lut)) {
        render_state.fog_lut  = lut;
        C3D_FogLutBind(lut);
    }
}

static void gfx_citro3d_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    union RGBA32 fog_color = {.r = r, .g = g, .b = b, .a = a};
    C3D_FogColor(fog_color.u32);
}

void gfx_citro3d_set_viewport_clear_color(enum ViewportId3DS viewport, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    screen_clear_configs.array[viewport].color.r = r;
    screen_clear_configs.array[viewport].color.g = g;
    screen_clear_configs.array[viewport].color.b = b;
    screen_clear_configs.array[viewport].color.a = a;
}

void gfx_citro3d_set_viewport_clear_color_u32(enum ViewportId3DS viewport, uint32_t color)
{
    screen_clear_configs.array[viewport].color.u32 = color;
}

void gfx_citro3d_set_viewport_clear_color_RGBA32(enum ViewportId3DS viewport, union RGBA32 color)
{
    screen_clear_configs.array[viewport].color.u32 = color.u32;
}

void gfx_citro3d_set_viewport_clear_depth(enum ViewportId3DS viewport, uint32_t depth)
{
    screen_clear_configs.array[viewport].depth = depth;
}

void gfx_citro3d_set_viewport_clear_buffer_mode(enum ViewportId3DS viewport, enum ViewportClearBuffer mode)
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
