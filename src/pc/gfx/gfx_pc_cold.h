#pragma once

#include <stdbool.h>

#include "gfx_window_manager_api.h"
#include "gfx_pc_shared.h"
#include "gfx_pc.h"

extern float MTX_IDENTITY[4][4];
extern float MTX_NDC_DOWNSCALE[4][4];

void gfx_cold_pre_run(struct RSP* rsp, struct RenderingState* rs);

void gfx_cold_rsp_init(struct RSP* rsp);

void gfx_cold_post_run();

void gfx_cold_init(
    struct GfxWindowManagerAPI *wapi,
    const char *game_name,
    bool start_in_fullscreen,
    struct RSP* rsp,
    struct RDP* rdp,
    struct ShaderState* shader_state,
    struct RenderingState* rendering_state);

void gfx_cold_upload_texture_to_rendering_api(
    uint8_t fmt,
    uint8_t siz,
    uint32_t line_size,
    uint32_t tile_size,
    const uint8_t* data,
    const uint8_t* palette);
