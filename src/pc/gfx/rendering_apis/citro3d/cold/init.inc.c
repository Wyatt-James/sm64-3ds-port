#include <math.h>
#include <stddef.h>

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
#include <3ds/gpu/enums.h>
#include <citro3d.h>
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

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_defines.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_wrappers.h"
#include "src/pc/bit_flag.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"

void c3d_cold_init_matrix_sets(struct GameMtxSet game_matrix_sets[], size_t num_sets)
{
    // Default all mat sets to identity
    for (size_t i = 0; i < num_sets; i++) {
        game_matrix_sets[i].game_projection =
        game_matrix_sets[i].model_view      = (C3D_Mtx) C3D_STATIC_IDENTITY_MTX;
    }
}

void c3d_cold_init_constant_uniforms()
{
    // Initialize constant uniforms
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, emu64_const_uniform_locations.texture_const_1, (float*) &emu64_const_uniform_defaults.texture_const_1);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, emu64_const_uniform_locations.texture_const_2, (float*) &emu64_const_uniform_defaults.texture_const_2);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, emu64_const_uniform_locations.cc_constants,    (float*) &emu64_const_uniform_defaults.cc_constants);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, emu64_const_uniform_locations.emu64_const_1,   (float*) &emu64_const_uniform_defaults.emu64_const_1);
    C3DW_FVUnifSetArray(GPU_VERTEX_SHADER, emu64_const_uniform_locations.emu64_const_2,   (float*) &emu64_const_uniform_defaults.emu64_const_2);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_0], 0, 0, 0, 0);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, emu64_uniform_locations.rsp_colors[EMU64_CC_1], 1, 1, 1, 1);
}

void c3d_cold_init_render_state(struct RenderState* render_state)
{
    *render_state = (struct RenderState) {
        .flags = FLAG_ALL,
        .fog_enabled = 0xFF,
        .fog_lut = NULL,
        .alpha_test = 0xFF,
        .cur_target = NULL,
        .texture_scale.f64 = INFINITY,
        .uv_offset = INFINITY,
        .current_gfx_mode = GFX_3DS_MODE_INVALID,
        .prev_slider_level = INFINITY,
        .shader_program = NULL
    };
}

void c3d_cold_init_vertex_load_flags(struct VertexLoadFlags* vertex_load_flags)
{
    *vertex_load_flags = (struct VertexLoadFlags) {
        .enable_lighting = false,
        .num_lights = 0,
        .enable_texgen = false,
        .texture_scale_s = 1,
        .texture_scale_t = 1
    };
}

void c3d_cold_init_optimization_flags(struct OptimizationFlags* optimizations)
{
    *optimizations = (struct OptimizationFlags) {
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
}
