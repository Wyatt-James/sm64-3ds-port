#pragma once

#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"

// #define RFLAG_FOG_ENABLED                  BIT(0)
// #define RFLAG_ALPHA_TEST                   BIT(1)
#define RFLAG_VIEWPORT_CHANGED             BIT(2)
#define RFLAG_SCISSOR_CHANGED              BIT(3)
#define RFLAG_VIEWPORT_OR_SCISSOR_CHANGED (RFLAG_VIEWPORT_CHANGED | RFLAG_SCISSOR_CHANGED)
#define RFLAG_TEX_SETTINGS_CHANGED         BIT(4) // Set explicitly by RDP commands.
#define RFLAG_CC_MAPPING_CHANGED           BIT(5)
#define RFLAG_RECALCULATE_SHADER           BIT(6)
#define RFLAG_VERT_LOAD_FLAGS_CHANGED      BIT(7) // Excludes lighting_enable since it's handled by RFLAG_RECALCULATE_SHADER

#define RFLAG_FRAME_START (RFLAG_VIEWPORT_OR_SCISSOR_CHANGED | RFLAG_TEX_SETTINGS_CHANGED | RFLAG_CC_MAPPING_CHANGED | RFLAG_RECALCULATE_SHADER | RFLAG_VERT_LOAD_FLAGS_CHANGED)

// A static definition of a C3D Identity Matrix
#define C3D_STATIC_IDENTITY_MTX {\
        .r = {\
            {.x = 1.0f},\
            {.y = 1.0f},\
            {.z = 1.0f},\
            {.w = 1.0f}\
        }\
    }

#define VERTEX_BUFFER_UNIT_SIZE sizeof(float)
#define VERTEX_BUFFER_NUM_UNITS (256 * 1024) // 1MB
#define VERTEX_BUFFER_NUM_BYTES (VERTEX_BUFFER_NUM_UNITS * VERTEX_BUFFER_UNIT_SIZE) // 1MB
#define MAX_VERTEX_BUFFERS      EMU64_NUM_VERTEX_FORMATS // Create as many as there are vertex formats.

#define MAX_SHADER_PROGRAMS 32
#define MAX_COLOR_COMBINERS 64

#define TEXTURE_POOL_SIZE 4096
