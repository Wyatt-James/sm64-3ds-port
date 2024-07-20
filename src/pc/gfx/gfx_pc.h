#ifndef GFX_PC_H
#define GFX_PC_H

#include <stdbool.h>

struct GfxRenderingAPI;
struct GfxWindowManagerAPI;

struct GfxDimensions {
    uint32_t width, height;
    float aspect_ratio;
    float aspect_ratio_factor; // Used to save TWO DIVISIONS A FRAME per vertex. Good enough for even PC.
};

extern struct GfxDimensions gfx_current_dimensions;

#ifdef __cplusplus
extern "C" {
#endif

void gfx_init(struct GfxWindowManagerAPI *wapi, const char *game_name, bool start_in_fullscreen);
void gfx_start_frame(void);
void gfx_run(Gfx *commands);
void gfx_end_frame(void);

#ifdef __cplusplus
}
#endif

#endif
