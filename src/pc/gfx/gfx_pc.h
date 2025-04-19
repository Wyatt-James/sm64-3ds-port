#ifndef GFX_PC_H
#define GFX_PC_H

#include <stdbool.h>

#define MAX_DIRECTIONAL_LIGHTS 2
#define MAX_LIGHTS (MAX_DIRECTIONAL_LIGHTS + 1)

struct GfxRenderingAPI;
struct GfxWindowManagerAPI;

struct GfxDimensions {
    uint32_t width, height;
    float ratio_x, ratio_y;
    float aspect_ratio;
};

extern struct GfxDimensions gfx_current_dimensions;

#ifdef __cplusplus
extern "C" {
#endif

void gfx_init(struct GfxWindowManagerAPI *wapi, const char *game_name, bool start_in_fullscreen);
void gfx_exit(void);
void gfx_start_frame(void);
void gfx_run(Gfx *commands);
void gfx_end_frame(void);

#ifdef __cplusplus
}
#endif

#endif
