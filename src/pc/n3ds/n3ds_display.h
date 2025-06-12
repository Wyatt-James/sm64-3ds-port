#pragma once

#include "libctru_inc.h"

// Converts a GSPGPU_FramebufferFormat to GX_TRANSFER_FORMAT
GX_TRANSFER_FORMAT n3ds_gsp_framebuffer_to_gx(GSPGPU_FramebufferFormat f);

// Converts a GPU_COLORBUF to GX_TRANSFER_FORMAT
GX_TRANSFER_FORMAT n3ds_gpu_colorbuf_to_gx(GPU_COLORBUF f);
