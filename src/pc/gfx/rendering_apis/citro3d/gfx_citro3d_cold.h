#pragma once

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
#include <c3d/attribs.h>
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

#include "src/pc/gfx/gfx_3ds_shaders.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_fog_cache.h"

void c3d_cold_clear_buffers();
void c3d_cold_reset_vertex_buffers();

struct VertexBuffer* c3d_cold_lookup_or_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo* attr_info);
struct ShaderProgram* c3d_cold_init_shader(struct ShaderProgram* prg, union ShaderProgramFeatureFlags shader_features);
void c3d_cold_init_color_combiner(struct ColorCombiner* cc, ColorCombinerId cc_id);

void c3d_cold_init_matrix_sets(struct GameMtxSet game_matrix_sets[], size_t num_sets);
void c3d_cold_init_constant_uniforms();
void c3d_cold_init_render_state(struct RenderState* render_state);
void c3d_cold_init_vertex_load_flags(struct VertexLoadFlags* vertex_load_flags);
void c3d_cold_init_optimization_flags(struct OptimizationFlags* optimizations);
