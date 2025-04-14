#include "gfx_citro3d_alt.h"

#include "src/pc/n3ds/c3d_inc.h"

void gfx_citro3d_alt_reset_api_state(void)
{
    for (size_t i = 0; i < 2; i++)
    {
        C3D_TexEnvInit(C3D_GetTexEnv(i));
    }
    
    // C3D_FrameDrawOn(gTarget); // Also resets viewport and disables scissor
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_DepthMap(true, -1.0f, 0.0f);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
	C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_FogColor(0);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, true);
	C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
}

// API stub functions
void gfx_rapi_on_resize(void) {}
void gfx_rapi_finish_render(void) {}
bool gfx_rapi_z_is_from_0_to_1(void) {return true;}
