#include "n3ds_display.h"

// Converts a GSPGPU_FramebufferFormat to GX_TRANSFER_FORMAT
GX_TRANSFER_FORMAT n3ds_gsp_framebuffer_to_gx(GSPGPU_FramebufferFormat f)
{
    switch (f) {
	    case GSP_RGBA8_OES:   return GX_TRANSFER_FMT_RGBA8;
	    case GSP_BGR8_OES:    return GX_TRANSFER_FMT_RGB8;
	    case GSP_RGB565_OES:  return GX_TRANSFER_FMT_RGB565;
	    case GSP_RGB5_A1_OES: return GX_TRANSFER_FMT_RGB5A1;
	    case GSP_RGBA4_OES:   return GX_TRANSFER_FMT_RGBA4;
        default:              return -1;
    }
}

// Converts a GPU_COLORBUF to GX_TRANSFER_FORMAT
GX_TRANSFER_FORMAT n3ds_gpu_colorbuf_to_gx(GPU_COLORBUF f)
{
    switch (f) {
	    case GPU_RB_RGBA8:      return GX_TRANSFER_FMT_RGBA8;
	    case GPU_RB_RGB8:       return GX_TRANSFER_FMT_RGB8;
	    case GPU_RB_RGBA5551:   return GX_TRANSFER_FMT_RGB5A1;
	    case GPU_RB_RGB565:     return GX_TRANSFER_FMT_RGB565;
	    case GPU_RB_RGBA4:      return GX_TRANSFER_FMT_RGBA4;
        default:                return -1;
    }
}
