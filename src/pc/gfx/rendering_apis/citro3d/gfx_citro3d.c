#ifdef TARGET_N3DS

#include "macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "src/pc/bit_flag.h"

#include "gfx_citro3d.h"
#include "gfx_citro3d_helpers.h"
#include "gfx_citro3d_fog_cache.h"
#include "src/pc/gfx/gfx_3ds_types.h"

#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"

#include "src/pc/gfx/gfx_3ds_menu.h"

#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/gfx_rendering_api.h"

#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/color_conversion.h"
#include "src/pc/gfx/texture_conversion.h"
#include "src/pc/pc_metrics.h"

#define VERTEX_BUFFER_UNIT_SIZE sizeof(float)
#define VERTEX_BUFFER_NUM_UNITS (256 * 1024) // 1MB
#define VERTEX_BUFFER_NUM_BYTES (VERTEX_BUFFER_NUM_UNITS * VERTEX_BUFFER_UNIT_SIZE) // 1MB
#define TEXTURE_POOL_SIZE   4096
#define MAX_VERTEX_BUFFERS  EMU64_NUM_VERTEX_FORMATS // Create as many as there are vertex formats.
#define MAX_SHADER_PROGRAMS 32
#define MAX_COLOR_COMBINERS 64

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

// #define FLAG_FOG_ENABLED                 BIT(0)
// #define FLAG_ALPHA_TEST                  BIT(1)
#define RFLAG_VIEWPORT_CHANGED             BIT(2)
#define RFLAG_SCISSOR_CHANGED              BIT(3)
#define RFLAG_VIEWPORT_OR_SCISSOR_CHANGED (RFLAG_VIEWPORT_CHANGED | RFLAG_SCISSOR_CHANGED)
#define RFLAG_TEX_SETTINGS_CHANGED         BIT(4) // Set explicitly by RDP commands.
#define RFLAG_CC_MAPPING_CHANGED           BIT(5)
#define RFLAG_RECALCULATE_SHADER           BIT(6)
#define RFLAG_VERT_LOAD_FLAGS_CHANGED      BIT(7) // Excludes lighting_enable since it's handled by RFLAG_RECALCULATE_SHADER

#define RFLAG_FRAME_START (RFLAG_VIEWPORT_OR_SCISSOR_CHANGED | RFLAG_TEX_SETTINGS_CHANGED | RFLAG_CC_MAPPING_CHANGED | RFLAG_RECALCULATE_SHADER | RFLAG_VERT_LOAD_FLAGS_CHANGED)

#define RFLAG_ON(flag_)    FLAG_ON(render_state.flags,    flag_)
#define RFLAG_SET(flag_)   FLAG_SET(render_state.flags,   flag_)
#define RFLAG_CLEAR(flag_) FLAG_CLEAR(render_state.flags, flag_)
#define OPT_ENABLED(flag_)   (ENABLE_OPTIMIZATIONS && ((FORCE_OPTIMIZATIONS) || (flag_))) // Optimization flag. Use: if (OPT_ENABLED(flag)) {fast path} else {slow path}
#define OPT_DISABLED(flag_)  (!OPT_ENABLED(flag_))                                        // Optimization flag. Use: if (OPT_DISABLED(flag)) {slow path} else {fast path}

#define NTSC_FRAMERATE(fps_) ((float) fps_ * (1000.0f / 1001.0f))

#define BSWAP32(v_)          (__builtin_bswap32(v_))
#define BOOL_INVERT(v_)      do {v_ = !v_;} while (0)

// See f32x2_note.txt
union f32x2 {
    struct {
        float f32_upper;
        float f32_lower;
    };
    struct {
        float s;
        float t;
    };
    uint64_t u64;
    int64_t s64;
    double f64;
};

struct ShaderInputMapping {
    float c1_rgb,
          c1_a,
          c2_rgb,
          c2_a;
};

// Represents a vertex buffer.
// Any number of shaders can use a buffer, but their attribute count and stride must be identical.
struct VertexBuffer {
    const struct n3ds_shader_vbo_info* vbo_info;
    C3D_BufInfo buf_info;
    C3D_AttrInfo* attr_info; // Only used to avoid duplicates.
    float *ptr;
    size_t num_verts;
};

/*
 * Represents a 3DS shader program.
 * Can use any video buffer with the correct stride and attribute count.
 */ 
struct ShaderProgram {
    union ShaderProgramFeatureFlags shader_features;
    shaderProgram_s pica_shader_program; // pica shader program
    struct VertexBuffer* vertex_buffer;
    C3D_AttrInfo attr_info;              // Describes VBO structure
};

struct ColorCombiner {
    bool use_env_color;
    ColorCombinerId cc_id;
    struct ShaderInputMapping c3d_shader_input_mapping; // Sent to GPU
    C3D_TexEnv texenv_slot_0; // Sent to GPU
    uint32_t cc_mapping_identifier; // Used to improve performance
    struct CCFeatures cc_features;
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

struct TextureSettings {
    float uv_offset;
    int16_t uls, ult;
    int16_t width, height;
};

struct GameMtxSet {
    C3D_Mtx model_view, transposed_model_view, game_projection;
};

struct TexHandle {
    C3D_Tex c3d_tex;
    union f32x2 scale;
};

struct OptimizationFlags {
    bool consecutive_fog;
    bool consecutive_stereo_p_mtx;
    bool alpha_test;
    bool gpu_textures;
    bool consecutive_framebuf;
    bool viewport_and_scissor;
    bool texture_settings_1;
    bool texture_settings_2;
    bool change_shader_on_cc_change;
    bool consecutive_vertex_load_flags;
};

struct RenderState {
    uint32_t flags;

    bool fog_enabled;
    bool alpha_test;

    C3D_FogLut* fog_lut;
    C3D_RenderTarget *cur_target;
    float uv_offset;             // Depends on linear filter.
    union f32x2 texture_scale;   // Varies per-texture.
    Gfx3DSMode current_gfx_mode;
    float prev_slider_level;
    struct ShaderProgram* shader_program;
};

struct VertexLoadFlags {
    bool enable_lighting;
    uint8_t num_lights;
    bool enable_texgen;
    uint32_t texture_scale_s, texture_scale_t;
};

static struct VertexBuffer vertex_buffers[MAX_VERTEX_BUFFERS];
static struct VertexBuffer* current_vertex_buffer = NULL;
static uint8_t num_vertex_buffers = 0;
static int num_rejected_draw_calls = 0;

static struct ShaderProgram shader_program_pool[MAX_SHADER_PROGRAMS];
static struct ShaderProgram* current_shader_program = NULL;
static uint8_t num_shader_programs = 0;
static C3D_TexEnv texenv_slot_1;
static union RGBA32 rdp_env_color  = { .u32 = 0 }, // Input for TEV
                    rdp_prim_color = { .u32 = 0 }; // Input for TEV

static struct ColorCombiner color_combiner_pool[MAX_COLOR_COMBINERS];
static struct ColorCombiner* current_color_combiner = NULL;
static uint8_t num_color_combiners = 0;

static struct FogCache fog_cache;

static struct TexHandle texture_pool[TEXTURE_POOL_SIZE];
static struct TexHandle* current_texture = &texture_pool[0];
static struct TexHandle* gpu_textures[2] = { &texture_pool[0], &texture_pool[0] };
static uint32_t api_texture_index = 0;
static struct TextureSettings texture_settings;
static union RGBA32 tex_conversion_buffer[16 * 1024] __attribute__((aligned(32))); // For converting textures between formats
static union RGBA32 tex_scaling_buffer[16 * 1024] __attribute__((aligned(32)));    // For padding and tiling textures

static bool sDepthTestOn = false;
static bool sDepthUpdateOn = false;

// calling FrameDrawOn resets viewport
static struct ViewportConfig viewport_config = { 0, 0, 0, 0 };

// calling SetViewport resets scissor
static struct ScissorConfig scissor_config = { .enable = false };

static enum Stereoscopic3dMode s2DMode = 0;
static struct IodConfig iod_config = { .z = 8.0f, .w = 16.0f };
static bool recalculate_stereo_p_mtx = true;

// Selectable groups of game matrix sets
static struct GameMtxSet game_matrix_sets[NUM_MATRIX_SETS];

// The current matrices.
// Projection is the 3DS-specific P-matrix.
// Model_view is the N64 MV-matrix.
// Game_projection is the N64 P-matrix.
static C3D_Mtx  projection_2d         = C3D_STATIC_IDENTITY_MTX,
                projection_left       = C3D_STATIC_IDENTITY_MTX,
                projection_right      = C3D_STATIC_IDENTITY_MTX,
               *model_view            = &game_matrix_sets[DEFAULT_MATRIX_SET].model_view,
               *transposed_model_view = &game_matrix_sets[DEFAULT_MATRIX_SET].transposed_model_view,
               *game_projection       = &game_matrix_sets[DEFAULT_MATRIX_SET].game_projection;

// Determines the clear config for each viewport.
static union ScreenClearConfigsN3ds screen_clear_configs = {
    .top    = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
    .bottom = {.bufs = VIEW_CLEAR_BUFFER_NONE, .color = {{0, 0, 0, 255}}, .depth = 0xFFFFFFFF},
};

static struct OptimizationFlags optimize = {
    .consecutive_fog = true,
    .consecutive_stereo_p_mtx = true,
    .alpha_test = true,
    .gpu_textures = true,
    .consecutive_framebuf = true,
    .viewport_and_scissor = true,
    .texture_settings_1 = true,
    .texture_settings_2 = true,
    .change_shader_on_cc_change = true,
    .consecutive_vertex_load_flags = true
};

static struct RenderState render_state = {
    .flags = RFLAG_FRAME_START,
    .fog_enabled = 0xFF,
    .fog_lut = NULL,
    .alpha_test = 0xFF,
    .cur_target = NULL,
    .texture_scale = {.f64 = INFINITY},
    .uv_offset = INFINITY,
    .current_gfx_mode = GFX_3DS_MODE_INVALID,
    .prev_slider_level = INFINITY,
    .shader_program = NULL
};

static struct VertexLoadFlags vertex_load_flags = {
    .enable_lighting = false,
    .num_lights = 0,
    .enable_texgen = false,
    .texture_scale_s = 1,
    .texture_scale_t = 1
};

// --------------- Internal-use functions ---------------

// Handles 3DS screen clearing
static void internal_citro3d_clear_buffers()
{
    C3D_ClearBits clear_top    = (C3D_ClearBits) screen_clear_configs.top.bufs    | C3D_CLEAR_ALL,
                  clear_bottom = (C3D_ClearBits) screen_clear_configs.bottom.bufs | C3D_CLEAR_ALL;

    uint32_t color_top    = BSWAP32(screen_clear_configs.top.color.u32),
             color_bottom = BSWAP32(screen_clear_configs.bottom.color.u32),
             depth_top    = screen_clear_configs.top.depth,
             depth_bottom = screen_clear_configs.bottom.depth;

    // Clear top screen
    if (clear_top)
        C3D_RenderTargetClear(gTarget, clear_top, color_top, depth_top);
        
    // Clear right-eye view
    if (clear_top && gTargetRight != NULL)
        C3D_RenderTargetClear(gTargetRight, clear_top, color_top, depth_top);

    // Clear bottom screen only if it needs re-rendering.
    if (clear_bottom)
        C3D_RenderTargetClear(gTargetBottom, clear_bottom, color_bottom, depth_bottom);
}

static void internal_citro3d_select_vertex_buffer(struct VertexBuffer* vb)
{
    current_vertex_buffer = vb;
    C3D_SetBufInfo(&vb->buf_info);
}

static struct VertexBuffer* internal_citro3d_setup_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo* attr_info)
{
    if (num_vertex_buffers == MAX_VERTEX_BUFFERS) {
        printf("Error: too many vertex buffers! (%d)\n", num_vertex_buffers + 1);
        return &vertex_buffers[0];
    }

    struct VertexBuffer* vb = &vertex_buffers[num_vertex_buffers++];
    vb->vbo_info = vbo_info;

    // Create the vertex buffer
    vb->ptr = linearAlloc(VERTEX_BUFFER_NUM_BYTES);
    vb->num_verts = 0;

    // Configure buffers
    BufInfo_Init(&vb->buf_info);
    BufInfo_Add(&vb->buf_info, vb->ptr, vbo_info->stride * VERTEX_BUFFER_UNIT_SIZE, attr_info->attrCount, attr_info->permutation);
    vb->attr_info = attr_info;

    return vb;
}

static struct VertexBuffer* internal_citro3d_lookup_or_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo* attr_info)
{
    // Avoid duplicates
    for (size_t i = 0; i < num_vertex_buffers; i++)
    {
        struct VertexBuffer* vb = &vertex_buffers[i];
        if (memcmp(attr_info, vb->attr_info, sizeof(*attr_info)) == 0)
            return vb;
    }

    // not found, create new
    return internal_citro3d_setup_vertex_buffer(vbo_info, attr_info);
}

static void internal_citro3d_load_shader(struct ShaderProgram *prg)
{
    current_shader_program = prg;

    C3D_BindProgram(&prg->pica_shader_program);
    C3D_SetAttrInfo(&prg->attr_info);
    internal_citro3d_select_vertex_buffer(prg->vertex_buffer);
}

static struct ShaderProgram* internal_citro3d_create_new_shader(union ShaderProgramFeatureFlags shader_features)
{
    if (num_shader_programs == MAX_SHADER_PROGRAMS) {
        printf("Error: too many shader programs! (%d)\n", num_shader_programs + 1);
        return &shader_program_pool[0];
    }

    struct ShaderProgram* prg = &shader_program_pool[num_shader_programs++];

    prg->shader_features.u32 = shader_features.u32;

    const struct n3ds_shader_info* shader_info = citro3d_helpers_get_shader_info_from_flags(prg->shader_features);

    citro3d_helpers_init_attr_info(&shader_info->vbo_info.attributes, &prg->attr_info);
    prg->vertex_buffer = internal_citro3d_lookup_or_create_vertex_buffer(&shader_info->vbo_info, &prg->attr_info);
    
    shaderProgramInit(&prg->pica_shader_program);
    shaderProgramSetVsh(&prg->pica_shader_program, &shader_info->binary->dvlb->DVLE[shader_info->dvle_index]);
    shaderProgramSetGsh(&prg->pica_shader_program, NULL, 0);

    return prg;
}

static struct ShaderProgram* internal_citro3d_lookup_shader(union ShaderProgramFeatureFlags shader_features)
{
    for (size_t i = 0; i < num_shader_programs; i++)
    {
        struct ShaderProgram* prog = &shader_program_pool[i];

        if (prog->shader_features.u32 == shader_features.u32)
            return prog;
    }
    return NULL;
}

static struct ShaderProgram* internal_citro3d_lookup_or_create_shader(union ShaderProgramFeatureFlags shader_features)
{
    struct ShaderProgram* shader_prog = internal_citro3d_lookup_shader(shader_features);
    
    if (shader_prog == NULL)
        shader_prog = internal_citro3d_create_new_shader(shader_features);
    
    return shader_prog;
}

static void internal_citro3d_upload_texture_common(void* data, struct TextureSize input_size, struct TextureSize output_size, GPU_TEXCOLOR format)
{
    C3D_Tex* tex = &current_texture->c3d_tex;

    current_texture->scale.s =   input_size.width  / (float) output_size.width;
    current_texture->scale.t = -(input_size.height / (float) output_size.height);

    if (C3D_TexInit(tex, output_size.width, output_size.height, format)) {
        C3D_TexUpload(tex, data);
        C3D_TexFlush(tex);
    } else
       printf("Tex init failed! Size: %d, %d\n", (int) output_size.width, (int) output_size.height);
}

void internal_citro3d_select_render_target(C3D_RenderTarget* target)
{
    if (render_state.cur_target != target || OPT_DISABLED(optimize.consecutive_framebuf)) {
        render_state.cur_target  = target;
        target->used = true;
        C3D_SetFrameBuf(&target->frameBuf);
    }

    // Must set viewport before scissor.
    // C3D_SetFrameBuf: only overwrites framebuf settings
    // C3D_FrameDrawOn: overwrites framebuf settings, viewport, and disables scissor (through viewport)
    // C3D_SetViewport: overwrites viewport and disables scissor
    // C3D_SetScissor: overwrites scissor.
    if (RFLAG_ON(RFLAG_VIEWPORT_OR_SCISSOR_CHANGED) || OPT_DISABLED(optimize.viewport_and_scissor)) {
        RFLAG_CLEAR(RFLAG_VIEWPORT_CHANGED);
        RFLAG_SET(RFLAG_SCISSOR_CHANGED);
        C3D_SetViewport(viewport_config.y, viewport_config.x, viewport_config.height, viewport_config.width);
    }

    if (RFLAG_ON(RFLAG_SCISSOR_CHANGED) || OPT_DISABLED(optimize.viewport_and_scissor)) {
        RFLAG_CLEAR(RFLAG_SCISSOR_CHANGED);
        if (scissor_config.enable)
            C3D_SetScissor(GPU_SCISSOR_NORMAL, scissor_config.y1, scissor_config.x1, scissor_config.y2, scissor_config.x2);
    }
}

// Called only when 3D is enabled.
static void internal_citro3d_recalculate_stereo_matrices()
{
    if (s2DMode == STEREO_MODE_2D) {
        Mtx_Identity(&projection_left);
        citro3d_helpers_apply_projection_mtx_preset(&projection_left);
    } else {
        Mtx_Identity(&projection_left);
        citro3d_helpers_mtx_stereo_tilt(&projection_left, &projection_left, s2DMode, -iod_config.z, -iod_config.w, gSliderLevel);
        citro3d_helpers_apply_projection_mtx_preset(&projection_left);

        Mtx_Identity(&projection_right);
        citro3d_helpers_mtx_stereo_tilt(&projection_right, &projection_right, s2DMode, iod_config.z, iod_config.w, gSliderLevel);
        citro3d_helpers_apply_projection_mtx_preset(&projection_right);
    }
}

static void internal_citro3d_update_depth()
{
    C3D_DepthTest(sDepthTestOn, GPU_LEQUAL, sDepthUpdateOn ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
}

// Template function to resize, swizzle, and upload a texture of given format.
#define UPLOAD_TEXTURE_TEMPLATE(type_, swizzle_func_name_, gpu_tex_format_) \
    type_* src = (type_*) data;                                                                                                 \
    GPU_TEXCOLOR format = gpu_tex_format_;                                                                                      \
    size_t unit_size = sizeof(src[0]);                                                                                          \
                                                                                                                                \
    struct TextureSize input_size = { .width = width, .height = height };                                                       \
    struct TextureSize output_size = citro3d_helpers_adjust_texture_dimensions(input_size, unit_size, sizeof(tex_scaling_buffer));  \
                                                                                                                                \
    if (output_size.success) {                                                                                                  \
        swizzle_func_name_(src, (type_*) tex_scaling_buffer, input_size, output_size);                                          \
        internal_citro3d_upload_texture_common((type_*) tex_scaling_buffer, input_size, output_size, format);                                    \
    }

// --------------- API functions ---------------

bool gfx_rapi_z_is_from_0_to_1()
{
    return true;
}

uint32_t gfx_rapi_new_texture()
{
    if (api_texture_index == TEXTURE_POOL_SIZE)
    {
        printf("Out of textures!\n");
        return 0;
    }
    return api_texture_index++;
}

// WYATT_TODO move this optimiaztion to the emulation layer.
void gfx_rapi_select_texture(int tex_slot, uint32_t texture_id)
{
    current_texture = &texture_pool[texture_id];
    if (gpu_textures[tex_slot] != current_texture || OPT_DISABLED(optimize.gpu_textures)) {
        gpu_textures[tex_slot]  = current_texture;
        C3D_TexBind(tex_slot, &current_texture->c3d_tex);
    }
}

// Optimized in the emulation layer. This optimization is technically incomplete, as filter & cms/cmt can be updated
// independently, but in practice this does not matter because this function is rarely called at all.
void gfx_rapi_set_sampler_parameters(int tex_slot, bool linear_filter, uint32_t clamp_mode_s, uint32_t clamp_mode_t)
{
    C3D_Tex* tex = &gpu_textures[tex_slot]->c3d_tex;
    C3D_TexSetFilter(tex, linear_filter ? GPU_LINEAR : GPU_NEAREST, linear_filter ? GPU_LINEAR : GPU_NEAREST);
    C3D_TexSetWrap(tex, citro3d_helpers_convert_texture_clamp_mode(clamp_mode_s), citro3d_helpers_convert_texture_clamp_mode(clamp_mode_t));
}

// Optimized in the emulation layer
void gfx_rapi_set_depth_test(bool depth_test)
{
    sDepthTestOn = depth_test;
    internal_citro3d_update_depth();
}

// Optimized in the emulation layer
void gfx_rapi_set_depth_mask(bool z_upd)
{
    sDepthUpdateOn = z_upd;
    internal_citro3d_update_depth();
}

// Optimized in the emulation layer
void gfx_rapi_set_zmode_decal(bool zmode_decal)
{
    C3D_DepthMap(true, -1.0f, zmode_decal ? -0.001f : 0);
}

// Optimized in the emulation layer only for normal use; draw_rectangle is unoptomized.
void gfx_rapi_set_viewport(int x, int y, int width, int height)
{
    RFLAG_SET(RFLAG_VIEWPORT_CHANGED);
    citro3d_helpers_convert_viewport_settings(&viewport_config, gGfx3DSMode, x, y, width, height);
}

// Optimized in the emulation layer
void gfx_rapi_set_scissor(int x, int y, int width, int height)
{
    RFLAG_SET(RFLAG_SCISSOR_CHANGED);
    citro3d_helpers_convert_scissor_settings(&scissor_config, gGfx3DSMode, x, y, width, height);
}

// Optimized in the emulation layer
void gfx_rapi_set_use_alpha(bool use_alpha)
{
    if (use_alpha)
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    else
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

void gfx_rapi_draw_triangles(float buf_vbo[], size_t buf_vbo_num_bytes, size_t buf_vbo_num_tris)
{
    struct ColorCombiner* cc = current_color_combiner;
    struct CCFeatures* cc_features = &cc->cc_features;
    const bool hasTex  = cc_features->used_textures[0] || cc_features->used_textures[1],
               hasCol  = cc_features->num_inputs > 0 && !vertex_load_flags.enable_lighting,
               hasNorm = vertex_load_flags.enable_lighting;

    // Select shader and vertex buffer. Must be done before using current_shader_program or current_vertex_buffer.
    if (RFLAG_ON(RFLAG_RECALCULATE_SHADER) || OPT_DISABLED(optimize.change_shader_on_cc_change)) {
        RFLAG_CLEAR(RFLAG_RECALCULATE_SHADER);

        const union ShaderProgramFeatureFlags shader_features = {
            .position = true,
            .tex = hasTex,
            .color = hasCol,
            .normals = hasNorm
        };

        struct ShaderProgram* desired_shader = internal_citro3d_lookup_or_create_shader(shader_features);

        if (render_state.shader_program != desired_shader) {
            render_state.shader_program  = desired_shader;
            internal_citro3d_load_shader(desired_shader);
        }
    }

    float* const vb_ptr = current_vertex_buffer->ptr;
    const size_t vb_num_verts = current_vertex_buffer->num_verts;
    const uint8_t vb_stride = current_vertex_buffer->vbo_info->stride;
    const size_t num_verts_this_drawcall = buf_vbo_num_tris * 3;
    const size_t vb_num_verts_after = vb_num_verts + num_verts_this_drawcall;

    // Prevent buffer overruns
    if (vb_num_verts_after * vb_stride > VERTEX_BUFFER_NUM_UNITS) {
        num_rejected_draw_calls++;
        return;
    }

    // Copy verts into the GPU buffer
    float* const vb_head = &vb_ptr[vb_num_verts * vb_stride];
    memcpy(vb_head, buf_vbo, buf_vbo_num_bytes * VERTEX_BUFFER_UNIT_SIZE);

    // Update vertex loading flags
    if (RFLAG_ON(RFLAG_VERT_LOAD_FLAGS_CHANGED) || OPT_DISABLED(optimize.consecutive_vertex_load_flags)) {
        RFLAG_CLEAR(RFLAG_VERT_LOAD_FLAGS_CHANGED);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.vertex_load_flags, vertex_load_flags.num_lights, vertex_load_flags.enable_texgen, vertex_load_flags.texture_scale_s, vertex_load_flags.texture_scale_t);
    }

    if (cc_features->num_inputs > 1) {
        if (cc->use_env_color)
            C3D_TexEnvColor(C3D_GetTexEnv(0), rdp_env_color.u32);
        else
            C3D_TexEnvColor(C3D_GetTexEnv(0), rdp_prim_color.u32);
    }

    // See f32x2_note.txt for an explanation of why we use U64 comparison instead of F64.
    if (hasTex) {
        struct TexHandle* tex = current_texture;
        if (render_state.texture_scale.u64 != tex->scale.u64 || *(uint32_t*) &render_state.uv_offset != *(uint32_t*) &texture_settings.uv_offset || OPT_DISABLED(optimize.texture_settings_1)) {
            render_state.texture_scale.u64  = tex->scale.u64;
            render_state.uv_offset = texture_settings.uv_offset;
            C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.tex_settings_1, tex->scale.s, tex->scale.t, texture_settings.uv_offset, 1);
        }

        if (RFLAG_ON(RFLAG_TEX_SETTINGS_CHANGED) || OPT_DISABLED(optimize.texture_settings_2)) {
            RFLAG_CLEAR(RFLAG_TEX_SETTINGS_CHANGED);
            C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.tex_settings_2, texture_settings.uls, texture_settings.ult, texture_settings.width, texture_settings.height);
        }
    }

    if (gGfx3DEnabled)
    {
        if (recalculate_stereo_p_mtx || OPT_DISABLED(optimize.consecutive_stereo_p_mtx)) {
            recalculate_stereo_p_mtx = false;
            internal_citro3d_recalculate_stereo_matrices();

            if (s2DMode == STEREO_MODE_2D)
                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.projection_mtx, &projection_left);
        }

        // left screen
        if (s2DMode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.projection_mtx, &projection_left);

        internal_citro3d_select_render_target(gTarget);
        C3D_DrawArrays(GPU_TRIANGLES, current_vertex_buffer->num_verts, num_verts_this_drawcall);

        // right screen
        if (s2DMode != STEREO_MODE_2D)
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.projection_mtx, &projection_right);

        internal_citro3d_select_render_target(gTargetRight);
        C3D_DrawArrays(GPU_TRIANGLES, current_vertex_buffer->num_verts, num_verts_this_drawcall);
    } else {
        internal_citro3d_select_render_target(gTarget);
        C3D_DrawArrays(GPU_TRIANGLES, current_vertex_buffer->num_verts, num_verts_this_drawcall);
    }

    current_vertex_buffer->num_verts = vb_num_verts_after;
}

void gfx_rapi_init()
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
    citro3d_helpers_apply_projection_mtx_preset(&projection_2d);

    fog_cache_init(&fog_cache);

    citro3d_helpers_configure_tex_env_slot_1(&texenv_slot_1);

    // Load a default shader. C3D requires this for many context-dependent operations.
    union ShaderProgramFeatureFlags shader_features = {.position = true, .tex = false, .color = false, .normals = false };
    internal_citro3d_load_shader(internal_citro3d_lookup_or_create_shader(shader_features));

    // Initialize constant uniforms
    citro3d_helpers_set_fv_unif_array(GPU_VERTEX_SHADER, emu64_const_uniform_locations.texture_const_1, (float*) &emu64_const_uniform_defaults.texture_const_1);
    citro3d_helpers_set_fv_unif_array(GPU_VERTEX_SHADER, emu64_const_uniform_locations.texture_const_2, (float*) &emu64_const_uniform_defaults.texture_const_2);
    citro3d_helpers_set_fv_unif_array(GPU_VERTEX_SHADER, emu64_const_uniform_locations.cc_constants,    (float*) &emu64_const_uniform_defaults.cc_constants);
    citro3d_helpers_set_fv_unif_array(GPU_VERTEX_SHADER, emu64_const_uniform_locations.emu64_const_1,   (float*) &emu64_const_uniform_defaults.emu64_const_1);
    citro3d_helpers_set_fv_unif_array(GPU_VERTEX_SHADER, emu64_const_uniform_locations.emu64_const_2,   (float*) &emu64_const_uniform_defaults.emu64_const_2);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_0], 0, 0, 0, 0);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_1], 1, 1, 1, 1);

    RFLAG_SET(FLAG_ALL);
    render_state.fog_enabled = 0xFF;
    render_state.fog_lut = NULL;
    render_state.alpha_test = 0xFF;
    render_state.cur_target = NULL;
    render_state.texture_scale.f64 = INFINITY;
    render_state.uv_offset = INFINITY;
    render_state.current_gfx_mode = GFX_3DS_MODE_INVALID;
    render_state.prev_slider_level = INFINITY;
    render_state.shader_program = NULL;
    
    vertex_load_flags.enable_lighting = false;
    vertex_load_flags.num_lights = 0;
    vertex_load_flags.enable_texgen = false;
    vertex_load_flags.texture_scale_s = 1;
    vertex_load_flags.texture_scale_t = 1;

    optimize.consecutive_fog = true;
    optimize.consecutive_stereo_p_mtx = true;
    optimize.alpha_test = true;
    optimize.gpu_textures = true;
    optimize.consecutive_framebuf = true;
    optimize.viewport_and_scissor = true;
    optimize.texture_settings_1 = true;
    optimize.texture_settings_2 = true;
    optimize.change_shader_on_cc_change = true;
    optimize.consecutive_vertex_load_flags = true;
}

void gfx_rapi_on_resize()
{

}

void gfx_rapi_start_frame()
{
    for (int i = 0; i < num_vertex_buffers; i++) {
        vertex_buffers[i].num_verts = 0;
    }

    // if (frames_touch_screen_held == 1) {
    //     shprog_emu64_print_uniform_locations(stdout);
    // }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    scissor_config.enable = false;

    // reset viewport if video mode changed
    if (render_state.current_gfx_mode != gGfx3DSMode) {
        render_state.current_gfx_mode  = gGfx3DSMode;
        gfx_rapi_set_viewport(0, 0, 400, 240);
    }

    if (render_state.prev_slider_level != gSliderLevel) {
        render_state.prev_slider_level  = gSliderLevel;
        recalculate_stereo_p_mtx = true;
    }

    // Due to hardware differences, the PC port always clears the depth buffer,
    // rather than just when the N64 would clear it.
    gfx_rapi_enable_viewport_clear_buffer_flag(VIEW_MAIN_SCREEN, VIEW_CLEAR_BUFFER_DEPTH);

    internal_citro3d_clear_buffers();

    // Reset screen clear buffer flags
    screen_clear_configs.top.bufs = 
    screen_clear_configs.bottom.bufs = VIEW_CLEAR_BUFFER_NONE;

    if (!gGfx3DEnabled)
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.projection_mtx, &projection_2d);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.model_view_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.game_projection_mtx, &IDENTITY_MTX);

    // Set each frame to ensure that target->used is set to true.
    // WYATT_TODO we can go further, but it would require hooking screen initialization.
    render_state.cur_target = NULL;
    render_state.texture_scale.f64 = INFINITY;
    render_state.uv_offset = INFINITY;
    render_state.shader_program = NULL;
    RFLAG_SET(RFLAG_FRAME_START);
}

void gfx_rapi_end_frame()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.projection_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.model_view_mtx,      &IDENTITY_MTX);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.game_projection_mtx, &IDENTITY_MTX);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.tex_settings_1, 1, 1, 1, 1);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.tex_settings_2, 1, 1, 1, 1);
    C3D_CullFace(GPU_CULL_NONE);

    // TOOD: draw the minimap here
    gfx_3ds_menu_draw(current_vertex_buffer->ptr, current_vertex_buffer->num_verts, gShowConfigMenu);
    
    if (num_rejected_draw_calls) {
        printf("Drawcalls rejected b/c full VBO: %d\n", num_rejected_draw_calls);
        num_rejected_draw_calls = 0;
    }

    // Requires <3ds/gpu/gpu.h>
    // printf("%c C %d RSP %d\n", OPT_ENABLED(optimize.consecutive_vertex_load_flags) ? 'Y' : '-', (int) gpuCmdBufOffset, num_rsp_commands_run);
    // PC_METRIC_DO(num_rsp_commands_run = 0);

    C3D_FrameEnd(0); // Swap is handled automatically within this function
}

void gfx_rapi_finish_render()
{

}

void gfx_rapi_upload_texture_rgba16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_RGBA5551)
}

void gfx_rapi_upload_texture_rgba32(const uint8_t* data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint32_t, citro3d_helpers_pad_and_tile_texture_u32, GPU_RGBA8)
}

/*
* The GPU doesn't support an IA4 format, so we need to convert.
* IA16 is the next format with decent accuracy (3-bit to 8-bit intensity).
* We could use IA8 (4-bit intensity), but this would cause a fairly large error.
*/
void gfx_rapi_upload_texture_ia4(const uint8_t *data, int width, int height)
{
    convert_ia4_to_ia16((union IA16*) tex_conversion_buffer, data, width, height);
    gfx_rapi_upload_texture_ia16((const uint8_t*) tex_conversion_buffer, width, height);
}

void gfx_rapi_upload_texture_ia8(const uint8_t *data, int width, int height) 
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_LA4)
}

void gfx_rapi_upload_texture_ia16(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint16_t, citro3d_helpers_pad_and_tile_texture_u16, GPU_LA8)
}

// Untested because it's unused in SM64
void gfx_rapi_upload_texture_i4(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L4)
}

// Untested because it's unused in SM64
void gfx_rapi_upload_texture_i8(const uint8_t *data, int width, int height)
{
    UPLOAD_TEXTURE_TEMPLATE(uint8_t, citro3d_helpers_pad_and_tile_texture_u8, GPU_L8)
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
void gfx_rapi_upload_texture_ci4(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci4_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}

// Untested because it's unused in SM64
// The GPU doesn't support palletized textures, so we need to convert.
void gfx_rapi_upload_texture_ci8(const uint8_t *data, const uint8_t* palette, int width, int height)
{
    convert_ci8_to_rgba16((union RGBA16*) tex_conversion_buffer, data, palette, width, height);
    gfx_rapi_upload_texture_rgba16((const uint8_t*) tex_conversion_buffer, width, height);
}

// Optimized in the emulation layer
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

// Optimizations seem pretty fruitless
void gfx_rapi_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    union RGBA32 fog_color = {.r = r, .g = g, .b = b, .a = a};
    C3D_FogColor(fog_color.u32);
}

// Optimized in the emulation layer
void gfx_rapi_set_2d_mode(int mode_2d)
{
    s2DMode = citro3d_helpers_convert_2d_mode(mode_2d);
    recalculate_stereo_p_mtx = true;
}

// Optimized in the emulation layer
void gfx_rapi_set_iod(float z, float w)
{
    citro3d_helpers_convert_iod_settings(&iod_config, z, w);
    recalculate_stereo_p_mtx = true;
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

// Optimized in the emulation layer
void gfx_rapi_apply_model_view_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.model_view_mtx, model_view);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.transposed_model_view_mtx, transposed_model_view);
}

// Optimized in the emulation layer
void gfx_rapi_apply_projection_matrix()
{
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, emu64_uniform_locations.game_projection_mtx, game_projection);
}

void gfx_rapi_select_matrix_set(uint32_t matrix_set_id)
{
    model_view            = &game_matrix_sets[matrix_set_id].model_view;
    transposed_model_view = &game_matrix_sets[matrix_set_id].transposed_model_view;
    game_projection       = &game_matrix_sets[matrix_set_id].game_projection;
}

// Optimized in the emulation layer.
void gfx_rapi_set_backface_culling_mode(uint32_t culling_mode)
{
    C3D_CullFace(citro3d_helpers_convert_cull_mode(culling_mode));
}

// Optimized in the emulation layer. Only set once per-drawcall.
void gfx_rapi_enable_lighting(bool enable)
{
    RFLAG_SET(RFLAG_RECALCULATE_SHADER);
    vertex_load_flags.enable_lighting = enable;
}

// Optimized in the emulation layer. Only set once per-drawcall.
void gfx_rapi_set_num_lights(int num_lights)
{
    RFLAG_SET(RFLAG_VERT_LOAD_FLAGS_CHANGED);
    vertex_load_flags.num_lights = num_lights;
}

// Optimized in the emulation layer. Each light is updated at most one time per-drawcall.
void gfx_rapi_configure_light(int light_id, Light_t* light)
{
    union RGBA32 color = {
        .u32 = *(uint32_t*) &light->col // Alpha is ignored, so we can put garbage there.
    };

    citro3d_helpers_set_fv_unif_rgb32(GPU_VERTEX_SHADER, emu64_uniform_locations.light_colors.all[light_id], color);

    // We don't need to scale the direction to [-1, 1] because it will be normalized
    if (light_id != 0)
        C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.light_directions[light_id - 1], light->dir[0], light->dir[1], light->dir[2], 0.f);
}

// Optimized in the emulation layer. Only set once per-drawcall.
void gfx_rapi_enable_texgen(bool enable)
{
    RFLAG_SET(RFLAG_VERT_LOAD_FLAGS_CHANGED);
    vertex_load_flags.enable_texgen = enable;
}

// Optimized in the emulation layer. Only set once per-drawcall.
void gfx_rapi_set_texture_scaling_factor(uint32_t s, uint32_t t)
{
    RFLAG_SET(RFLAG_VERT_LOAD_FLAGS_CHANGED);
    vertex_load_flags.texture_scale_s = s;
    vertex_load_flags.texture_scale_t = t;
}

void gfx_rapi_set_viewport_clear_color(uint32_t viewport_id, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    union RGBA32 color = {.r = r, .g = g, .b = b, .a = a};
    screen_clear_configs.array[viewport_id].color.u32 = color.u32;
}

void gfx_rapi_set_viewport_clear_color_u32(uint32_t viewport_id, uint32_t color)
{
    screen_clear_configs.array[viewport_id].color.u32 = color;
}

void gfx_rapi_set_viewport_clear_depth(uint32_t viewport_id, uint32_t depth)
{
    screen_clear_configs.array[viewport_id].depth = depth;
}

void gfx_rapi_enable_viewport_clear_buffer_flag(uint32_t viewport_id, enum ViewportClearBuffer mode)
{
    screen_clear_configs.array[viewport_id].bufs |= mode;
}

void gfx_rapi_set_uv_offset(float offset)
{
    texture_settings.uv_offset = offset;
}

// Optimized in the emulation layer
void gfx_rapi_set_texture_settings(int16_t upper_left_s, int16_t upper_left_t, int16_t width, int16_t height)
{
    texture_settings.uls = upper_left_s;
    texture_settings.ult = upper_left_t;
    texture_settings.width = width;
    texture_settings.height = height;
    RFLAG_SET(RFLAG_TEX_SETTINGS_CHANGED);
}

// Redundant CCIDs are optimized in the emulation layer.
void gfx_rapi_select_color_combiner(size_t cc_index)
{
    struct ColorCombiner* cc = &color_combiner_pool[cc_index];

    // Different CC: load the mappings
    if (current_color_combiner != cc) {

        const bool cc_mappings_different = current_color_combiner == NULL || (cc->cc_mapping_identifier != current_color_combiner->cc_mapping_identifier);
        const int shader_num_inputs = cc->cc_features.num_inputs;

        // Load the mappings only if different and they're enabled by the shader
        if (shader_num_inputs != 0 && cc_mappings_different)
            C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_color_selection, cc->c3d_shader_input_mapping.c1_rgb, cc->c3d_shader_input_mapping.c1_a, cc->c3d_shader_input_mapping.c2_rgb, cc->c3d_shader_input_mapping.c2_a);

        // Configure TEV
        if (cc->cc_features.num_inputs == 2)
        {
            C3D_SetTexEnv(0, &texenv_slot_1);
            C3D_SetTexEnv(1, &cc->texenv_slot_0);
        } else {
            C3D_SetTexEnv(0, &cc->texenv_slot_0);
            C3D_TexEnvInit(C3D_GetTexEnv(1));
        }

        if (render_state.fog_enabled != cc->cc_features.opt_fog || OPT_DISABLED(optimize.consecutive_fog)) {
            render_state.fog_enabled  = cc->cc_features.opt_fog;
            GPU_FOGMODE mode = cc->cc_features.opt_fog ? GPU_FOG : GPU_NO_FOG;
            C3D_FogGasMode(mode, GPU_PLAIN_DENSITY, true);
        }

        const uint8_t alpha_test = cc->cc_features.opt_texture_edge & cc->cc_features.opt_alpha;
        if (render_state.alpha_test != alpha_test || OPT_DISABLED(optimize.alpha_test)) {
            render_state.alpha_test  = alpha_test;
            C3D_AlphaTest(alpha_test, GPU_GREATER, 77);
        }
        
        current_color_combiner = cc;
        RFLAG_SET(RFLAG_RECALCULATE_SHADER);
    }
}

// Redundant CCIDs are optimized in the emulation layer.
size_t gfx_rapi_lookup_or_create_color_combiner(ColorCombinerId cc_id)
{
    // Find existing CC
    for (size_t i = 0; i < num_color_combiners; i++) {
        if (color_combiner_pool[i].cc_id == cc_id)
            return i;
    }

    // New CC
    const size_t cc_index = num_color_combiners;
    struct ColorCombiner* cc = &color_combiner_pool[cc_index];
    num_color_combiners = (num_color_combiners + 1) % MAX_COLOR_COMBINERS;

    union CCInputMapping mapping;

    {
        CCShaderId shader_id;
        gfx_cc_generate_cc(cc_id, &mapping, &shader_id);
        gfx_cc_get_features(shader_id, &cc->cc_features);
    }

    // If num inputs >= 2, we need to reverse the mappings' A and B params (hack for goddard)
    // WYATT_TODO put something better here
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

    // WYATT_TODO this is probably incorrect, but it works for now. Fixes the pause tint being too light.
    cc->use_env_color = mapping.rgb[1] == CC_ENV;

    // N3DS only cares about the first two mappings, so we want to make an identifier for specifically this to enhance performance
    // RGBA32 works fine since it's four u8s
    union RGBA32 mapping_id = {
        .r = mapping.rgb[0],
        .g = mapping.rgb[1],
        .b = mapping.alpha[0],
        .a = mapping.alpha[1],
    };

    cc->cc_mapping_identifier = mapping_id.u32;

    // Preconfigure TEV settings
    citro3d_helpers_configure_tex_env_slot_0(&cc->cc_features, &cc->texenv_slot_0);

    return cc_index;
}

void gfx_rapi_color_combiner_get_info(size_t cc_index, uint8_t *num_inputs, bool used_textures[2])
{
    struct ColorCombiner* const cc = &color_combiner_pool[cc_index];
    *num_inputs =      cc->cc_features.num_inputs;
    used_textures[0] = cc->cc_features.used_textures[0];
    used_textures[1] = cc->cc_features.used_textures[1];
}

// Optimized in the emulation layer
void gfx_rapi_set_cc_prim_color(uint32_t color)
{
    rdp_prim_color.u32 = color;
    citro3d_helpers_set_fv_unif_rgba32(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_PRIM], rdp_prim_color);
}

// Optimized in the emulation layer
void gfx_rapi_set_cc_env_color(uint32_t color)
{
    rdp_env_color.u32 = color;
    citro3d_helpers_set_fv_unif_rgba32(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_ENV], rdp_env_color);
}

#endif
