#ifdef TARGET_N3DS

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "gfx_citro3d_emulator.h"
#include "gfx_citro3d_alt.h"
#include "gfx_citro3d_screens.h"
#include "gfx_citro3d_helpers.h"
#include "gfx_citro3d_wrappers.h"
#include "gfx_citro3d_fog_cache.h"
#include "gfx_citro3d_internal_types.h"
#include "gfx_citro3d_context.h"

#include "src/pc/bit_flag.h"
#include "src/pc/pc_macros.h"

#include "src/pc/n3ds/n3ds_hid.h"
#include "src/pc/n3ds/n3ds_config.h"

#include "src/pc/gfx/gfx_pc.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"

#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/gfx_rendering_api.h"

#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/color_conversion.h"
#include "src/pc/gfx/texture_conversion.h"

#define VERTEX_BUFFER_NUM_UNITS (256 * 1024) // 1MB
#define VERTEX_BUFFER_NUM_BYTES (VERTEX_BUFFER_NUM_UNITS * EMU64_STRIDE_UNIT_SIZE) // 1MB
#define MAX_TEXTURES 4096
#define MAX_ASYNC_TEXTURE_COPIES_PER_FRAME 4
#define MAX_VERTEX_BUFFERS  EMU64_NUM_VERTEX_FORMATS
#define MAX_SHADER_PROGRAMS 32
#define MAX_COLOR_COMBINERS 64

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

#define CTX_NOTIFY(flag_)   FLAG_SET(ctx.flags, flag_)
#define CTX_CLEAR(flag_)    FLAG_CLEAR(ctx.flags, flag_)
#define OPT_ENABLED(flag_)  (ENABLE_OPTIMIZATIONS && ((FORCE_OPTIMIZATIONS) || (flag_))) // Optimization flag. Use: if (OPT_ENABLED(flag)) {fast path} else {slow path}
#define OPT_DISABLED(flag_) (!OPT_ENABLED(flag_))                                        // Optimization flag. Use: if (OPT_DISABLED(flag)) {slow path} else {fast path}

struct GameMtxSet {
    C3D_Mtx model_view, transposed_model_view, game_projection;
};

typedef struct
{
    bool consecutive_fog,
         consecutive_stereo_p_mtx,
         alpha_test,
         gpu_textures,
         viewport_and_scissor,
         texture_settings_1,
         texture_settings_2,
         change_shader_on_cc_change,
         consecutive_vertex_load_flags,
         consecutive_color_combiner,
         consecutive_shader,
         consecutive_cc_mappings;
} OptimizationFlags;

// Logs how many drawcalls are rejected to prevent buffer overruns.
static size_t num_rejected_draw_calls;

// If we run out, returns pool[0]
static VertexBuffer vertex_buffers[MAX_VERTEX_BUFFERS];
static uint8_t num_vertex_buffers;

// If we run out, returns pool[0]
static Emu64ShaderProgram shader_program_pool[MAX_SHADER_PROGRAMS];
static uint8_t num_shader_programs;

// If we run out, wraps to 0.
static ColorCombiner color_combiner_pool[MAX_COLOR_COMBINERS];
static uint8_t num_color_combiners;

// If we run out, wraps to 0.
static TexHandle texture_pool[MAX_TEXTURES];
static uint32_t api_texture_index;
static uint8_t num_textures_to_upload_to_vram;
static TexHandle* texture_upload_queue[MAX_ASYNC_TEXTURE_COPIES_PER_FRAME];

// Miscellaneous stuff
static struct FogCache fog_cache;
static struct IodConfig iod_config;
static enum Stereoscopic3dMode stereo_3d_mode;
static float slider_level;

// Selectable groups of N64 matrix sets
static struct GameMtxSet rsp_matrix_sets[NUM_MATRIX_SETS];
static bool recalculate_stereo_matrices;

// Used to update some things whenever the N3DS_DisplayMode changes.
// Not stored in the context because it doesn't really have anything to do with it.
static N3DS_DisplayMode old_display_mode;

// The current matrices.
// projection_2d is the 3DS-specific P-matrix used in 2D rendering.
// projection_left/right are the 3DS-specific P-matrices used in 3D rendering.
// model_view is the N64 MV-matrix.
// game_projection is the N64 P-matrix.
static C3D_Mtx  projection_2d,
                projection_left,
                projection_right,
               *model_view,
               *transposed_model_view,
               *game_projection;

// Determines the clear config for each viewport.
static OptimizationFlags optimize;
static RenderContext ctx;

// --------------- Forward Declarations ---------------
static VertexBuffer* internal_citro3d_setup_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo attr_info);
static VertexBuffer* internal_citro3d_lookup_or_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo attr_info);
static Emu64ShaderProgram* internal_citro3d_create_new_shader(Emu64ProgramFeatureFlags shader_features);
static Emu64ShaderProgram* internal_citro3d_lookup_or_create_shader(Emu64ProgramFeatureFlags shader_features);
static void internal_citro3d_select_shader();
static void internal_citro3d_select_color_combiner(ColorCombiner* cc);
static void internal_citro3d_recalculate_stereo_matrices();
static void internal_citro3d_update_3d_slider();
static void internal_citro3d_init_rendering_state();
static void internal_citro3d_load_default_texture();
static void internal_citro3d_upload_textures_to_vram();
static void internal_citro3d_init_vram_texture_pools();

// --------------- Internal-use functions ---------------

// Recalculates which shader to use. This function uses CTX_SHADER
// as a bit of an optimization, rather than an external flag.
static void internal_citro3d_select_shader()
{
    if (ctx.flags & CTX_SHADER || OPT_DISABLED(optimize.consecutive_shader))
    {
        struct CCFeatures* cc_features = &ctx.color_combiner->cc_features;

        bool hasTex  = cc_features->used_textures[0] || cc_features->used_textures[1],
             hasCol  = cc_features->num_inputs > 0 && !ctx.vertex_load_flags.enable_lighting,
             hasNorm = ctx.vertex_load_flags.enable_lighting;

        Emu64ProgramFeatureFlags shader_features = {
            .position = true,
            .tex = hasTex,
            .color = hasCol,
            .normals = hasNorm
        };

        Emu64ShaderProgram* prg = internal_citro3d_lookup_or_create_shader(shader_features);

        if (ctx.shader_program != prg || OPT_DISABLED(optimize.consecutive_shader))
            ctx.shader_program  = prg;
        else
            CTX_CLEAR(CTX_SHADER);
    }
}

// Forcefully selects a color combiner, ignoring optimization
static void internal_citro3d_select_color_combiner(ColorCombiner* cc)
{
    ColorCombiner* old = ctx.color_combiner;
    ctx.color_combiner = cc;

    CTX_NOTIFY(CTX_TEXENV | CTX_SHADER);

    bool cc_mappings_different = old == NULL || (cc->cc_mapping_identifier != old->cc_mapping_identifier);
    
    if (cc_mappings_different || OPT_DISABLED(optimize.consecutive_cc_mappings))
        CTX_NOTIFY(CTX_SHADER_INPUT_MAPPING);

    if (ctx.fog_enabled != cc->cc_features.opt_fog || OPT_DISABLED(optimize.consecutive_fog)) {
        ctx.fog_enabled  = cc->cc_features.opt_fog;
        CTX_NOTIFY(CTX_FOG);
    }

    uint8_t alpha_test = cc->cc_features.opt_texture_edge & cc->cc_features.opt_alpha;
    if (ctx.alpha_test != alpha_test || OPT_DISABLED(optimize.alpha_test)) {
        ctx.alpha_test  = alpha_test;
        CTX_NOTIFY(CTX_ALPHA_TEST);
    }
}

// Recalculates the stereoscopic projection matrices.
static void internal_citro3d_recalculate_stereo_matrices()
{
    if (stereo_3d_mode == STEREO_MODE_2D) {
        Mtx_Identity(&projection_left);
        citro3d_helpers_apply_projection_mtx_preset(&projection_left);
    } else {
        Mtx_Identity(&projection_left);
        citro3d_helpers_mtx_stereo_tilt(&projection_left, &projection_left, stereo_3d_mode, -iod_config.z, -iod_config.w, slider_level);
        citro3d_helpers_apply_projection_mtx_preset(&projection_left);

        Mtx_Identity(&projection_right);
        citro3d_helpers_mtx_stereo_tilt(&projection_right, &projection_right, stereo_3d_mode, iod_config.z, iod_config.w, slider_level);
        citro3d_helpers_apply_projection_mtx_preset(&projection_right);
    }
}

// --------------- API functions ---------------

void gfx_rapi_draw_triangles(float buf_vbo[], size_t buf_vbo_num_words, size_t buf_vbo_num_tris)
{
    internal_citro3d_select_shader(); // Must be done here because it may need to allocate a shader.
    gfx_citro3d_update_context(&ctx);

    VertexBuffer* vb = ctx.shader_program->prog.vertex_buffer;
    float* const vb_ptr = vb->ptr;
    size_t vb_num_verts = vb->num_verts;
    uint8_t vb_stride = vb->vbo_info->stride;
    size_t num_verts_this_drawcall = buf_vbo_num_tris * 3;
    size_t vb_num_verts_after = vb_num_verts + num_verts_this_drawcall;
    float* vb_head = &vb_ptr[vb_num_verts * vb_stride];

    // Prevent buffer overruns
    if (vb_num_verts_after * vb_stride > VERTEX_BUFFER_NUM_UNITS) {
        num_rejected_draw_calls++;
        return;
    }

    // Copy verts into the GPU buffer
    memcpy(vb_head, buf_vbo, buf_vbo_num_words * EMU64_STRIDE_UNIT_SIZE);

    // Draw
    if (g3dsGfxState.stereo_3d_active)
    {
        if (recalculate_stereo_matrices || OPT_DISABLED(optimize.consecutive_stereo_p_mtx)) {
            recalculate_stereo_matrices = false;
            internal_citro3d_recalculate_stereo_matrices();

            if (stereo_3d_mode == STEREO_MODE_2D)
                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_projection_mtx, &projection_left);
        }

        // left screen
        if (stereo_3d_mode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_projection_mtx, &projection_left);

        // We use SetFrameBuf because it doesn't overwrite viewport or scissor settings.
        C3D_SetFrameBuf(&gTarget->frameBuf);
        C3D_DrawArrays(GPU_TRIANGLES, vb_num_verts, num_verts_this_drawcall);

        // right screen
        if (stereo_3d_mode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_projection_mtx, &projection_right);

        C3D_SetFrameBuf(&gTargetRight->frameBuf);
        // Fall through
    }

    C3D_DrawArrays(GPU_TRIANGLES, vb_num_verts, num_verts_this_drawcall);

    vb->num_verts = vb_num_verts_after;
}

void gfx_rapi_select_texture(int tex_slot, uint32_t texture_id)
{
    TexHandle* tex = &texture_pool[texture_id];
    ctx.current_texture = ctx.gpu_textures[tex_slot] = tex;
    CTX_NOTIFY(CTX_TEXTURE(tex_slot) | CTX_CURRENT_TEXTURE);
    
    if (UNLIKELY(tex->load_status == TEX_FCRAM && num_textures_to_upload_to_vram < MAX_ASYNC_TEXTURE_COPIES_PER_FRAME))
    {
        tex->load_status = TEX_ENQUEUED;
        texture_upload_queue[num_textures_to_upload_to_vram++] = tex;
    }
}

void gfx_rapi_set_sampler_parameters(int tex_slot, bool linear_filter, uint32_t clamp_mode_s, uint32_t clamp_mode_t)
{
    C3D_Tex* tex = &ctx.gpu_textures[tex_slot]->c3d_tex;
    C3D_TexSetFilter(tex, linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(tex, citro3d_helpers_convert_texture_clamp_mode(clamp_mode_s), citro3d_helpers_convert_texture_clamp_mode(clamp_mode_t));
    CTX_NOTIFY(CTX_TEXTURE(tex_slot));
}

void gfx_rapi_set_depth_test(bool depth_test)
{
    ctx.depth_test = depth_test;
    CTX_NOTIFY(CTX_DEPTH_TEST);
}

void gfx_rapi_set_depth_mask(bool depth_update)
{
    ctx.depth_update = depth_update;
    CTX_NOTIFY(CTX_DEPTH_UPDATE);
}

void gfx_rapi_set_zmode_decal(bool zmode_decal)
{
    ctx.zmode_decal = zmode_decal;
    CTX_NOTIFY(CTX_ZMODE_DECAL);
}

void gfx_rapi_set_viewport(int x, int y, int width, int height)
{
    citro3d_helpers_convert_viewport_settings(&ctx.viewport_config, g3dsGfxState.display_mode, x, y, width, height);
    CTX_NOTIFY(CTX_VIEWPORT);
}

void gfx_rapi_set_scissor(int x, int y, int width, int height)
{
    citro3d_helpers_convert_scissor_settings(&ctx.scissor_config, g3dsGfxState.display_mode, x, y, width, height);
    CTX_NOTIFY(CTX_SCISSOR);
}

void gfx_rapi_set_use_alpha(bool use_alpha)
{
    ctx.use_alpha = use_alpha;
    CTX_NOTIFY(CTX_USE_ALPHA);
}

void gfx_rapi_set_fog(uint16_t from, uint16_t to)
{
    // FIXME: The near/far factors are personal preference
    // BOB:  6400, 59392 => 0.16, 116
    // JRB:  1280, 64512 => 0.80, 126
    switch (fog_cache_load(&fog_cache, from, to)) {
        default:
            break;
        case FOGCACHE_MISS:
            FogLut_Exp(fog_cache_current(&fog_cache), 0.05f, 1.5f, 1024 / (float)from, ((float)to) / 512);
            // Fall-through
        case FOGCACHE_HIT:
            C3D_FogLutBind(fog_cache_current(&fog_cache));
            break;
    }
}

void gfx_rapi_set_2d_mode(int mode_2d)
{
    stereo_3d_mode = citro3d_helpers_convert_2d_mode(mode_2d);
    recalculate_stereo_matrices = true;
}

void gfx_rapi_set_iod(float z, float w)
{
    citro3d_helpers_convert_iod_settings(&iod_config, z, w);
    recalculate_stereo_matrices = true;
}

void gfx_rapi_set_backface_culling_mode(uint32_t culling_mode)
{
    ctx.culling_mode = citro3d_helpers_convert_cull_mode(culling_mode);
    CTX_NOTIFY(CTX_CULL_MODE);
}

void gfx_rapi_enable_lighting(bool enable)
{
    ctx.vertex_load_flags.enable_lighting = enable;
    CTX_NOTIFY(CTX_VERTEX_LOAD_ENABLE_LIGHTING);
}

void gfx_rapi_set_num_lights(int num_lights)
{
    ctx.vertex_load_flags.num_lights = num_lights;
    CTX_NOTIFY(CTX_VERTEX_LOAD_NUM_LIGHTS);
}

void gfx_rapi_configure_light(int light_id, Light_t* light)
{
    union RGBA32 color = {
        .u32 = *(uint32_t*) &light->col // Alpha is ignored, so we can put garbage there.
    };

    ASSUME(light_id < MAX_LIGHTS && light_id >= 0);

    C3DW_FVUnifSetRGB(GPU_VERTEX_SHADER, EMU64_ULOC_light_colors(light_id), color);

    // We don't need to scale the direction to [-1, 1] because it will be normalized in the shader
    if (light_id != 0)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_light_directions(light_id - 1), light->dir[0], light->dir[1], light->dir[2], 0.f);
}

void gfx_rapi_enable_texgen(bool enable)
{
    ctx.vertex_load_flags.enable_texgen = enable;
    CTX_NOTIFY(CTX_VERTEX_LOAD_TEXGEN);
}

void gfx_rapi_set_texture_scaling_factor(uint32_t s, uint32_t t)
{
    ctx.vertex_load_flags.texture_scale_s = s;
    ctx.vertex_load_flags.texture_scale_t = t;
    CTX_NOTIFY(CTX_VERTEX_LOAD_TEXTURE_SCALE);
}

void gfx_rapi_set_uv_offset(float offset)
{
    ctx.uv_offset = offset;
    CTX_NOTIFY(CTX_UV_OFFSET);
}

void gfx_rapi_set_texture_settings(int16_t upper_left_s, int16_t upper_left_t, int16_t width, int16_t height)
{
    C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_tex_settings_2, upper_left_s, upper_left_t, width, height);
}

size_t gfx_rapi_lookup_or_create_color_combiner(ColorCombinerId cc_id)
{
    // Find existing CC
    for (size_t i = 0; i < num_color_combiners; i++) {
        if (color_combiner_pool[i].cc_id == cc_id)
            return i;
    }

    // New CC. If we run out of slots, just invalidate them all. 
    size_t cc_index = num_color_combiners;
    num_color_combiners = (num_color_combiners + 1) % MAX_COLOR_COMBINERS;
    
    citro3d_helpers_init_cc(&color_combiner_pool[cc_index], cc_id);
    return cc_index;
}

void gfx_rapi_color_combiner_get_info(size_t cc_index, uint8_t *num_inputs, bool used_textures[2])
{
    ColorCombiner* cc = &color_combiner_pool[cc_index];
    *num_inputs      = cc->cc_features.num_inputs;
    used_textures[0] = cc->cc_features.used_textures[0];
    used_textures[1] = cc->cc_features.used_textures[1];
}

void gfx_rapi_select_color_combiner(size_t cc_index)
{
    ColorCombiner* new = &color_combiner_pool[cc_index];
    ColorCombiner* old = ctx.color_combiner;

    // Different CC: load the mappings
    if (new != old || OPT_DISABLED(optimize.consecutive_color_combiner))
    { 
        internal_citro3d_select_color_combiner(new);
    }
}

void gfx_rapi_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    ctx.fog_color = (union RGBA32) {.r = r, .g = g, .b = b, .a = a};
    CTX_NOTIFY(CTX_FOG_COLOR);
}

void gfx_rapi_set_cc_prim_color(uint32_t color)
{
    ctx.prim_color.u32 = color;
    CTX_NOTIFY(CTX_PRIM_COLOR);
}

void gfx_rapi_set_cc_env_color(uint32_t color)
{
    ctx.env_color.u32 = color; 
    CTX_NOTIFY(CTX_ENV_COLOR);
}

// --------------- Matrices ---------------

void gfx_rapi_select_matrix_set(uint32_t matrix_set_id)
{
    model_view            = &rsp_matrix_sets[matrix_set_id].model_view;
    transposed_model_view = &rsp_matrix_sets[matrix_set_id].transposed_model_view;
    game_projection       = &rsp_matrix_sets[matrix_set_id].game_projection;
}

void gfx_rapi_set_model_view_matrix(float mtx[4][4])
{
    citro3d_helpers_convert_mtx(model_view, mtx);
    citro3d_helpers_copy_and_transpose_mtx(transposed_model_view, model_view);
}

void gfx_rapi_set_projection_matrix(float mtx[4][4])
{
    citro3d_helpers_convert_mtx(game_projection, mtx);
}

void gfx_rapi_apply_model_view_matrix(void)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_model_view_mtx, model_view);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_transposed_model_view_mtx, transposed_model_view);
}

void gfx_rapi_apply_projection_matrix(void)
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_game_projection_mtx, game_projection);
}

// --------------- Uncommonly-used Functions ---------------

// WYATT_TODO there's a leak somewhere that's related to this, since
// textures seemingly keep getting allocated but never used.
// Check the gfx_pc hash function?
COLD uint32_t gfx_rapi_new_texture(void)
{
    if (api_texture_index == MAX_TEXTURES)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return api_texture_index++;
}

#include "inc/gfx_citro3d_texture_upload.inc.c"

COLD static void internal_citro3d_init_rendering_state()
{
    num_rejected_draw_calls = 0;

    num_vertex_buffers = 0;
    num_color_combiners = 0;
    num_shader_programs = 0;

    iod_config     = (struct IodConfig) { .z = 8.0f, .w = 16.0f };
    stereo_3d_mode = STEREO_MODE_2D;
    slider_level   = n3ds_hid_3d_slider();
    recalculate_stereo_matrices = slider_level > 0.0f;

    fog_cache_init(&fog_cache);

    api_texture_index = 0;

    // Default all matrix sets to identity
    for (int i = 0; i < NUM_MATRIX_SETS; i++) {
        Mtx_Identity(&rsp_matrix_sets[i].game_projection);
        Mtx_Identity(&rsp_matrix_sets[i].model_view);
    }

    Mtx_Identity(&projection_2d);
    citro3d_helpers_apply_projection_mtx_preset(&projection_2d);
    model_view            = &rsp_matrix_sets[DEFAULT_MATRIX_SET].model_view,
    transposed_model_view = &rsp_matrix_sets[DEFAULT_MATRIX_SET].transposed_model_view,
    game_projection       = &rsp_matrix_sets[DEFAULT_MATRIX_SET].game_projection;

    // Needs to be one with a 1:1 scale to match the initializers below.
    old_display_mode = N3DS_DISPLAY_2D_400_240;

    optimize = (OptimizationFlags) {
        .consecutive_fog               = true,
        .consecutive_stereo_p_mtx      = true,
        .alpha_test                    = true,
        .gpu_textures                  = true,
        .viewport_and_scissor          = true,
        .texture_settings_1            = true,
        .texture_settings_2            = true,
        .change_shader_on_cc_change    = true,
        .consecutive_vertex_load_flags = true,
        .consecutive_color_combiner    = true,
        .consecutive_shader            = true,
        .consecutive_cc_mappings       = true,
    };

    ctx = (RenderContext) {
        // These are used directly for GPU initialization, so we set them to real values.
        .flags             = CTX_ALL,
        .viewport_config   = (ScreenDimensions) {.x = 0, .y = 0, .width = 400, .height = 240},
        .scissor_config    = (ScreenDimensions) {.x1 = 0, .y1 = 0, .x2 = 400, .y2 = 240},
        .vertex_load_flags = (struct VertexLoadConfig) {.enable_lighting = false, .enable_texgen = false, .num_lights = 0, .texture_scale_s = 1, .texture_scale_t = 1},
        .fog_color         = (union RGBA32) {.u32 = 0xFFFFFFFF},
        .zmode_decal       = false,
        .use_alpha         = false,
        .alpha_test        = true,
        .depth_test        = false,
        .depth_update      = true,
        .fog_enabled       = false,
        .culling_mode      = GPU_CULL_NONE,
        .uv_offset         = 0.0f,
        .env_color         = (union RGBA32) { .u32 = 0 },
        .prim_color        = (union RGBA32) { .u32 = 0 },
        .two_color_texenv  = citro3d_helpers_configure_two_color_tex_env(),

        // These are not used directly for GPU initialization, so we set them to dummy values.
        .color_combiner    = NULL,
        .shader_program    = NULL,
        .uniforms          = {},
        .gpu_textures      = {NULL, NULL},
        .current_texture   = NULL,
    };
}

COLD static void internal_citro3d_load_default_texture()
{
    // Upload a default texture
    uint32_t tex_id = gfx_rapi_new_texture();
    uint8_t data[8 * 8] = {0};

    gfx_rapi_select_texture(0, tex_id);
    gfx_rapi_select_texture(1, tex_id);

    gfx_rapi_upload_texture_i8(data, 8, 8); // We use i8 because PICA200 seemingly hates L4 textures in VRAM
}

COLD void gfx_citro3d_emulator_init(void)
{
    internal_citro3d_init_rendering_state();
    internal_citro3d_select_color_combiner(&color_combiner_pool[gfx_rapi_lookup_or_create_color_combiner(DEFAULT_CC_ID)]);
    internal_citro3d_select_shader(); // Must be done here because it may need to allocate a shader.
    internal_citro3d_load_default_texture();
    
    if (g3dsConfig.vram_textures)
        internal_citro3d_init_vram_texture_pools();

    // Initialize constant uniforms
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, EMU64_CONST_ULOC_texture_const_1, (float*) &emu64_const_uniform_defaults.texture_const_1);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, EMU64_CONST_ULOC_texture_const_2, (float*) &emu64_const_uniform_defaults.texture_const_2);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, EMU64_CONST_ULOC_cc_constants,    (float*) &emu64_const_uniform_defaults.cc_constants);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, EMU64_CONST_ULOC_emu64_const_1,   (float*) &emu64_const_uniform_defaults.emu64_const_1);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, EMU64_CONST_ULOC_emu64_const_2,   (float*) &emu64_const_uniform_defaults.emu64_const_2);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_rsp_colors(EMU64_CC_0), 0, 0, 0, 0);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_rsp_colors(EMU64_CC_1), 1, 1, 1, 1);

    gfx_citro3d_force_update_context(&ctx);
    gfx_citro3d_save_context_uniforms(&ctx, GPU_VERTEX_SHADER);
}

COLD void gfx_citro3d_emulator_exit(void)
{
    for (int i = 0; i < num_shader_programs; i++)
        citro3d_helpers_free_shader(&shader_program_pool[i].prog);
}

COLD void gfx_citro3d_emulator_start_frame(void)
{
    num_rejected_draw_calls = 0;

    for (int i = 0; i < num_vertex_buffers; i++)
        vertex_buffers[i].num_verts = 0;

    internal_citro3d_update_3d_slider();

    citro3d_helpers_rescale_screen_dimensions(&ctx.viewport_config, old_display_mode, g3dsGfxState.display_mode);
    citro3d_helpers_rescale_screen_dimensions(&ctx.scissor_config, old_display_mode, g3dsGfxState.display_mode);
    old_display_mode = g3dsGfxState.display_mode;
    
    // Restore context
    gfx_citro3d_upload_context_uniforms(&ctx, GPU_VERTEX_SHADER);
    gfx_citro3d_force_update_context(&ctx);

    // This is required to set the render targets' "used" flags.
    if (!g3dsGfxState.stereo_3d_active)
    {
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_projection_mtx, &projection_2d);
        C3D_FrameDrawOn(gTarget);
    }
    else
    {
        C3D_FrameDrawOn(gTargetRight);
        C3D_FrameDrawOn(gTarget);
    }
}

COLD void gfx_citro3d_emulator_end_frame(void)
{
    if (num_rejected_draw_calls)
        printf("Drawcalls rejected from full VBO: %u\n", num_rejected_draw_calls);

    if (g3dsConfig.vram_textures)
        internal_citro3d_upload_textures_to_vram();

    gfx_citro3d_save_context_uniforms(&ctx, GPU_VERTEX_SHADER);
    gfx_citro3d_alt_reset_api_state();
}

// Reads the 3D slider and sets a flag if it has changed.
COLD static void internal_citro3d_update_3d_slider()
{
    float new_slider_level = n3ds_hid_3d_slider();

    if (slider_level != new_slider_level) {
        slider_level  = new_slider_level;
        recalculate_stereo_matrices = true;
    }
}

// Allocates and configures a new vertex buffer
COLD static VertexBuffer* internal_citro3d_setup_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo attr_info)
{
    if (num_vertex_buffers == MAX_VERTEX_BUFFERS) {
        printf("Error: too many vertex buffers! (%d)\n", num_vertex_buffers + 1);
        return &vertex_buffers[0];
    }

    VertexBuffer* vb = &vertex_buffers[num_vertex_buffers++];
    vb->vbo_info = vbo_info;
    vb->attr_info = attr_info; // Struct copy
    vb->ptr = linearAlloc(VERTEX_BUFFER_NUM_BYTES);
    vb->num_verts = 0;

    BufInfo_Init(&vb->buf_info);
    BufInfo_Add(&vb->buf_info, vb->ptr, vbo_info->stride * EMU64_STRIDE_UNIT_SIZE, attr_info.attrCount, attr_info.permutation);

    return vb;
}

// Searches for a vertex buffer, or allocates a new one if one was not found.
COLD static VertexBuffer* internal_citro3d_lookup_or_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo attr_info)
{
    // Avoid duplicates
    for (size_t i = 0; i < num_vertex_buffers; i++)
    {
        VertexBuffer* vb = &vertex_buffers[i];
        if (memcmp(&attr_info, &vb->attr_info, sizeof(attr_info)) == 0)
            return vb;
    }

    // not found, create new
    return internal_citro3d_setup_vertex_buffer(vbo_info, attr_info);
}

// Allocates and configures a new shader program
COLD static Emu64ShaderProgram* internal_citro3d_create_new_shader(Emu64ProgramFeatureFlags shader_features)
{
    if (num_shader_programs == MAX_SHADER_PROGRAMS) {
        printf("Error: too many shader programs! (%d)\n", num_shader_programs);
        return &shader_program_pool[0];
    }

    Emu64ShaderProgram* e_prg = &shader_program_pool[num_shader_programs++];
    ShaderProgram* prg = &e_prg->prog;

    e_prg->shader_features.u32 = shader_features.u32;

    const struct n3ds_shader_info* shader_info = emu64_get_shader_info_from_flags(e_prg->shader_features);

    C3D_AttrInfo attr_info = citro3d_helpers_init_attr_info(&shader_info->vbo_info.attributes);
    prg->vertex_buffer = internal_citro3d_lookup_or_create_vertex_buffer(&shader_info->vbo_info, attr_info);
    
    shaderProgramInit(&prg->pica_shader_program);
    shaderProgramSetVsh(&prg->pica_shader_program, &shader_info->binary->dvlb->DVLE[shader_info->dvle_index]);
    shaderProgramSetGsh(&prg->pica_shader_program, NULL, 0);

    return e_prg;
}

// Searches for a shader program, or allocates a new one if one was not found.
COLD static Emu64ShaderProgram* internal_citro3d_lookup_or_create_shader(Emu64ProgramFeatureFlags shader_features)
{
    for (size_t i = 0; i < num_shader_programs; i++) {
        if (shader_program_pool[i].shader_features.u32 == shader_features.u32)
            return &shader_program_pool[i];
    }

    return internal_citro3d_create_new_shader(shader_features);
}

#endif
