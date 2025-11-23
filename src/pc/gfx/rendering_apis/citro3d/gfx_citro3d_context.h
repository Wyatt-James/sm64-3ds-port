#pragma once

/*
 * A rendering context (a-la OpenGL/Citro3D) for gfx_citro3d_emulator's render state.
 */

#include <stdint.h>
#include <stdbool.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/n3ds/c3d_inc.h"

#include "gfx_citro3d_internal_types.h"
#include "gfx_citro3d_wrappers.h"

#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/gfx/color_formats.h"
#include "src/pc/pc_macros.h"

#define CTX_TEXTURE_COUNT 2
#define CTX_TEXTURE(n_) (CTX_TEXTURE_0 << (n_))

typedef enum
{
    CTX_UV_OFFSET                   = BIT(0),
    CTX_VERTEX_LOAD_NUM_LIGHTS      = BIT(1),
    CTX_VERTEX_LOAD_TEXGEN          = BIT(2),
    CTX_VERTEX_LOAD_TEXTURE_SCALE   = BIT(3),
    CTX_CULL_MODE                   = BIT(4),
    CTX_FOG_COLOR                   = BIT(5),
    CTX_PRIM_COLOR                  = BIT(6),
    CTX_ENV_COLOR                   = BIT(7),
    CTX_FOG                         = BIT(8),
    CTX_USE_ALPHA                   = BIT(9),
    CTX_ZMODE_DECAL                 = BIT(10),
    CTX_VIEWPORT                    = BIT(11),
    CTX_SCISSOR                     = BIT(12),
    CTX_DEPTH_UPDATE                = BIT(13),
    CTX_DEPTH_TEST                  = BIT(14),
    CTX_CURRENT_TEXTURE             = BIT(15),
    CTX_TEXTURE_0                   = BIT(16),
    CTX_TEXTURE_1                   = BIT(17),
    CTX_ALPHA_TEST                  = BIT(18),
    CTX_TEXENV                      = BIT(19),
    CTX_SHADER                      = BIT(20),
    CTX_SHADER_INPUT_MAPPING        = BIT(21),
    
    CTX_VERTEX_LOAD_ENABLE_LIGHTING = CTX_SHADER,
    CTX_VERTEX_LOAD_CONFIG_UNIF     = CTX_VERTEX_LOAD_NUM_LIGHTS | CTX_VERTEX_LOAD_TEXGEN | CTX_VERTEX_LOAD_TEXTURE_SCALE,
    CTX_ALL                         = (CTX_SHADER_INPUT_MAPPING << 1) - 1
} ContextFlags;

// This does not include most uniforms for performance reasons.
typedef struct
{
    ContextFlags flags;

    Emu64ShaderProgram* shader_program;
    ColorCombiner* color_combiner;
    struct VertexLoadConfig vertex_load_flags;
    ScreenDimensions viewport_config,
                     scissor_config;
    TexHandle* gpu_textures[CTX_TEXTURE_COUNT];
    TexHandle* current_texture;

    bool zmode_decal,
         use_alpha,
         alpha_test,
         depth_test,
         depth_update,
         fog_enabled;
    uint8_t culling_mode; // GPU_CULLMODE

    union RGBA32 fog_color,
                 env_color,
                 prim_color;

    float uv_offset;
    
    C3D_TexEnv two_color_texenv;
    C3D_FVec uniforms[EMU64_UNSAFE_NUM_FV_UNIFS];
} RenderContext;

// Uploads this context to the rendering API.
static inline void gfx_citro3d_update_context(RenderContext* ctx)
{
    uint32_t flags = ctx->flags;

    if (flags & CTX_TEXENV)
    {
        ColorCombiner* cc = ctx->color_combiner;

        // Configure TEV
        if (cc->cc_features.num_inputs == 2)
        {
            C3D_SetTexEnv(0, &ctx->two_color_texenv);
            C3D_SetTexEnv(1, &cc->texenv);
        } else {
            C3D_SetTexEnv(0, &cc->texenv);
            C3D_TexEnvInit(C3D_GetTexEnv(1));
        }
    }

    if (flags & CTX_SHADER)
    {
        ShaderProgram* prg = &ctx->shader_program->prog;
        C3D_BindProgram(&prg->pica_shader_program);
        C3D_SetAttrInfo(&prg->vertex_buffer->attr_info);
        C3D_SetBufInfo(&prg->vertex_buffer->buf_info);
    }

    if (flags & CTX_SHADER_INPUT_MAPPING)
    {
        ColorCombiner* cc = ctx->color_combiner;

        if (cc->cc_features.num_inputs != 0)
            C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_rsp_color_selection, cc->c3d_shader_input_mapping.c1_rgb, cc->c3d_shader_input_mapping.c1_a, cc->c3d_shader_input_mapping.c2_rgb, cc->c3d_shader_input_mapping.c2_a);
    }

    for (int i = 0; i < CTX_TEXTURE_COUNT; i++)
    {
        if (flags & CTX_TEXTURE(i))
        {
            TexHandle* tex = ctx->gpu_textures[i];
            // if (tex->load_status != TEX_UNINITIALIZED) // Disabling this isn't kosher but c'mon
                C3D_TexBind(i, &tex->c3d_tex);
        }
    }

    if (flags & (CTX_UV_OFFSET | CTX_CURRENT_TEXTURE))
    {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_tex_settings_1, ctx->current_texture->scale.s, ctx->current_texture->scale.t, ctx->uv_offset, 1);
    }

    if (flags & CTX_VERTEX_LOAD_CONFIG_UNIF)
    {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_vertex_load_flags, ctx->vertex_load_flags.num_lights, ctx->vertex_load_flags.enable_texgen, ctx->vertex_load_flags.texture_scale_s, ctx->vertex_load_flags.texture_scale_t);
    }

    if (flags & CTX_CULL_MODE)
    {
        C3D_CullFace(ctx->culling_mode);
    }

    if (flags & CTX_FOG_COLOR)
    {
        C3D_FogColor(ctx->fog_color.u32);
    }

    // We have to update the TEV color if any of these have changed
    if (flags & (CTX_TEXENV | CTX_ENV_COLOR | CTX_PRIM_COLOR))
    {
        if (ctx->color_combiner->cc_features.num_inputs > 1)
        {
            if (ctx->color_combiner->use_env_color)
                C3D_TexEnvColor(C3D_GetTexEnv(0), ctx->env_color.u32);
            else
                C3D_TexEnvColor(C3D_GetTexEnv(0), ctx->prim_color.u32);
        }
        
        if (flags & CTX_PRIM_COLOR)
        {
            C3DW_FVUnifSetRGBA(GPU_VERTEX_SHADER, EMU64_ULOC_rsp_colors(EMU64_CC_PRIM), ctx->prim_color);
        }

        if (flags & CTX_ENV_COLOR)
        {
            C3DW_FVUnifSetRGBA(GPU_VERTEX_SHADER, EMU64_ULOC_rsp_colors(EMU64_CC_ENV),  ctx->env_color);
        }
    }

    if (flags & CTX_FOG)
    {
        C3DW_FogGasMode(ctx->fog_enabled);
    }

    if (flags & CTX_USE_ALPHA)
    {
        C3DW_AlphaBlend(ctx->use_alpha);
    }

    if (flags & CTX_ZMODE_DECAL)
    {
        C3DW_DepthMap(ctx->zmode_decal);
    }

    if (flags & CTX_VIEWPORT)
    {
        C3DW_SetViewport(&ctx->viewport_config);
    }
        
    if (flags & (CTX_VIEWPORT | CTX_SCISSOR))
    {
        C3DW_SetScissor(&ctx->scissor_config);
    }

    if (flags & (CTX_DEPTH_TEST | CTX_DEPTH_UPDATE))
    {
        C3DW_DepthTest(ctx->depth_test, ctx->depth_update);
    }

    if (flags & CTX_ALPHA_TEST)
    {
        C3DW_AlphaTest(ctx->alpha_test);
    }

    ctx->flags &= ~CTX_ALL;
}

static inline void gfx_citro3d_force_update_context(RenderContext* ctx)
{
    ctx->flags |= CTX_ALL;
    gfx_citro3d_update_context(ctx);
}

// Saves the current C3D float uniforms to this context.
static inline void gfx_citro3d_save_context_uniforms(RenderContext* ctx, GPU_SHADER_TYPE shader_type)
{
    memcpy(ctx->uniforms, C3D_FVUnif[shader_type], sizeof(ctx->uniforms));
}

// Uploads this context's saved float uniforms to the rendering API.
static inline void gfx_citro3d_upload_context_uniforms(RenderContext* ctx, GPU_SHADER_TYPE shader_type)
{
    memcpy(C3D_FVUnifWritePtr(shader_type, 0, ARRAY_COUNT(ctx->uniforms)), ctx->uniforms, sizeof(ctx->uniforms));
}
