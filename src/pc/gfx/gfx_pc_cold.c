#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>
#include <include/macros.h>

#include "gfx_pc_cold.h"

#include "gfx_pc.h"
#include "gfx_cc.h"
#include "gfx_pc_shared.h"
#include "gfx_3ds_constants.h"
#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"
#include "src/pc/bit_flag.h"
#include "src/pc/profiler_3ds.h"

static bool dropped_frame;
static struct GfxWindowManagerAPI *gfx_wapi;

// A valid default value for a Light_t pointer.
static Light_t LIGHT_DEFAULT = (Light_t) STATIC_LIGHT_DEFAULT;

float MTX_IDENTITY[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                         {0.0f, 1.0f, 0.0f, 0.0f},
                                         {0.0f, 0.0f, 1.0f, 0.0f},
                                         {0.0f, 0.0f, 0.0f, 1.0f}};
                                         
float MTX_NDC_DOWNSCALE[4][4] = {{1.0f / NDC_SCALE, 0.0f,             0.0f, 0.0f},
                                              {0.0f,             1.0f / NDC_SCALE, 0.0f, 0.0f},
                                              {0.0f,             0.0f,             1.0f, 0.0f},
                                              {0.0f,             0.0f,             0.0f, 1.0f}};

static void gfx_cold_sp_reset(struct RSP* rsp)
{
    rsp->modelview_matrix_stack_size = 1;
    rsp->current_num_lights = 2;
    rsp->lights_changed_bitfield = SET_BITS(MAX_LIGHTS + 1);
}

void gfx_cold_rsp_init(struct RSP* rsp)
{
    *rsp = (struct RSP) {
        // Screen-space rect Z will always be -1.
        .rect_vertices[0].position.z = -1,
        .rect_vertices[1].position.z = -1,
        .rect_vertices[2].position.z = -1,
        .rect_vertices[3].position.z = -1,
        .matrix_set = MATRIX_SET_NORMAL,
        .P_matrix = STATIC_IDENTITY_MTX
    };
    
    // Initialize lights to all black
    for (int i = 0; i < MAX_LIGHTS + 1; i++)
        rsp->current_lights[i] = &LIGHT_DEFAULT;

    // Initialize the matstack to identity
    for (int i = 0; i < MAT_STACK_SIZE; i++)
        memcpy(rsp->modelview_matrix_stack[i], MTX_IDENTITY, sizeof(MTX_IDENTITY));
}

static void shader_state_init(struct ShaderState* ss)
{
    *ss = (struct ShaderState) {
        .cc_id = DELIBERATELY_INVALID_CC_ID,
        .num_inputs = 0,
        .texture_edge = false,
        .use_alpha = false,
        .use_fog = false,
        .use_noise = false,
        .used_textures.either = 0,
    };
}

static void rendering_state_init(struct RenderingState* rs, struct RDP* rdp)
{
    *rs = (struct RenderingState) {
        .cc_id = DELIBERATELY_INVALID_CC_ID,
        .texture_settings.u64 = ~0,
        .prim_color.u32 = ~rdp->prim_color.u32,
        .env_color.u32  = ~rdp->env_color.u32,
        .linear_filter = ~0,
        .matrix_set = MATRIX_SET_INVALID,
        .p_mtx_changed = true,
        .mv_mtx_changed = true,
        .stereo_3d_mode = STEREO_MODE_COUNT,
        .iod_mode = IOD_COUNT,
        .current_num_lights = MAX_LIGHTS + 1,
        .enable_lighting = ~0,
        .enable_texgen = ~0,
        .texture_scaling_factor.s = INT32_MAX,
        .texture_scaling_factor.t = INT32_MAX,
        . last_mv_mtx_addr = rs->last_p_mtx_addr = NULL,
    };
}

void gfx_start_frame(void)
{
    gfx_wapi->handle_events();
}

void gfx_end_frame(void) {
    if (!dropped_frame) {
        gfx_rapi_finish_render();
        gfx_wapi->swap_buffers_end();
    }
}

void gfx_cold_pre_run(struct RSP* rsp, struct RenderingState* rs)
{
    gfx_cold_sp_reset(rsp);

    if (!gfx_wapi->start_frame()) {
        dropped_frame = true;
        return;
    }
    dropped_frame = false;

    profiler_3ds_log_time(0);
    gfx_rapi_start_frame();
    profiler_3ds_log_time(4); // GFX Rendering API Start Frame (VSync)
    gfx_rapi_set_backface_culling_mode(rsp->geometry_mode & G_CULL_BOTH);
    rs->last_mv_mtx_addr = rs->last_p_mtx_addr = NULL;
}

void gfx_cold_post_run()
{
    gfx_rapi_end_frame();
    gfx_wapi->swap_buffers_begin();
}

void gfx_cold_init(
    struct GfxWindowManagerAPI *wapi,
    const char *game_name,
    bool start_in_fullscreen,
    UNUSED struct RSP* rsp,
    struct RDP* rdp,
    struct ShaderState* shader_state,
    struct RenderingState* rendering_state)
{
    gfx_wapi = wapi;
    gfx_wapi->init(game_name, start_in_fullscreen);
    gfx_rapi_init();

    // dimensions won't change on 3DS, so just do this once
    gfx_wapi->get_dimensions(&gfx_current_dimensions.width, &gfx_current_dimensions.height);
    if (gfx_current_dimensions.height == 0) {
        // Avoid division by zero
        gfx_current_dimensions.height = 1;
    }
    gfx_current_dimensions.aspect_ratio = (float)gfx_current_dimensions.width / (float)gfx_current_dimensions.height;
    gfx_current_dimensions.aspect_ratio_factor = (4.0f / 3.0f) * (1.0f / gfx_current_dimensions.aspect_ratio);

    shader_state_init(shader_state);
    rendering_state_init(rendering_state, rdp);
}

void gfx_cold_upload_texture_to_rendering_api(
    uint8_t fmt,
    uint8_t siz,
    uint32_t line_size,
    uint32_t tile_size,
    const uint8_t* data,
    const uint8_t* palette)
{

    int width, height;

    switch (siz) {
        case G_IM_SIZ_32b:
            width = line_size / 2;
            height = (tile_size / 2) / line_size;
            break;
        case G_IM_SIZ_16b:
            width = line_size / 2;
            height = tile_size / line_size;
            break;
        case G_IM_SIZ_8b:
            width = line_size;
            height = tile_size / line_size;
            break;
        case G_IM_SIZ_4b:
            width = line_size * 2;
            height = tile_size / line_size;
            break;
        default:
            abort();
    }

    switch (TEX_FORMAT(fmt, siz)) {
        case TEXFMT_RGBA32:
            gfx_rapi_upload_texture_rgba32(data, width, height); // Unused by SM64
            break;
        case TEXFMT_RGBA16:
            gfx_rapi_upload_texture_rgba16(data, width, height);
            break;
        case TEXFMT_IA4:
            gfx_rapi_upload_texture_ia4(data, width, height); // Used by text only
            break;
        case TEXFMT_IA8:
            gfx_rapi_upload_texture_ia8(data, width, height);
            break;
        case TEXFMT_IA16:
            gfx_rapi_upload_texture_ia16(data, width, height);
            break;
        case TEXFMT_I4:
            gfx_rapi_upload_texture_i4(data, width, height); // Unused by SM64
            break;
        case TEXFMT_I8:
            gfx_rapi_upload_texture_i8(data, width, height); // Unused by SM64
            break;
        case TEXFMT_CI4:
            gfx_rapi_upload_texture_ci4(data, palette, width, height); // Unused by SM64
            break;
        case TEXFMT_CI8:
            gfx_rapi_upload_texture_ci8(data, palette, width, height); // Unused by SM64
            break;
        default:
            abort();
    }
}
