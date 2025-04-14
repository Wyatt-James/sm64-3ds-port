#include "gfx_citro3d_menu_tex.h"

#include "src/minimap/textures/mode_400_t3x.h"
#include "src/minimap/textures/mode_800_t3x.h"
#include "src/minimap/textures/aa_on_t3x.h"
#include "src/minimap/textures/aa_off_t3x.h"
#include "src/minimap/textures/hide_menu_t3x.h"
#include "src/minimap/textures/exit_t3x.h"
#include "src/minimap/textures/menu_cleft_t3x.h"
#include "src/minimap/textures/menu_cright_t3x.h"
#include "src/minimap/textures/menu_cdown_t3x.h"
#include "src/minimap/textures/menu_cup_t3x.h"
#include "src/minimap/textures/fullscreen_test_t3x.h"
#include "src/minimap/textures/fullscreen_test_padded_t3x.h"

#include "gfx_citro3d_alt.h"
#include "src/pc/pc_macros.h"

#define TEX(name_, original_w_, original_h_) (TexLoadGroup) {  \
    .output_tex  = &name_##_tex,                               \
    .t3x_data    = &name_##_t3x,                               \
    .t3x_size    = name_##_t3x_size,                           \
    .original_w  = original_w_,                                \
    .original_h  = original_h_,                                \
}

typedef struct
{
    WrappedTexture* output_tex;
    const uint8_t (*t3x_data)[];
    size_t t3x_size;
    size_t original_w, original_h; // Size before padding to power-of-2 dimensions
} TexLoadGroup;

// From header
WrappedTexture mode_400_tex,
               mode_800_tex,
               aa_on_tex,
               aa_off_tex,
               hide_menu_tex,
               exit_tex,
               menu_cleft_tex,
               menu_cright_tex,
               menu_cdown_tex,
               menu_cup_tex,
               fullscreen_test_tex,
               fullscreen_test_padded_tex;

static const TexLoadGroup load_groups[] = {
    TEX(mode_400,                64,  64),
    TEX(mode_800,                64,  64),
    TEX(aa_on,                   64,  64),
    TEX(aa_off,                  64,  64),
    TEX(hide_menu,               64,  64),
    TEX(exit,                    64,  64),
    TEX(menu_cleft,              64,  64),
    TEX(menu_cright,             64,  64),
    TEX(menu_cdown,              64,  32),
    TEX(menu_cup,                64,  32),
    TEX(fullscreen_test,        320, 240),
    TEX(fullscreen_test_padded, 512, 256),
};

static const TexLoadGroup* load_tex_group(MenuTexId id)
{
    id = CLAMP(id, 0, MENU_TEX_COUNT - 1);
    return &load_groups[id];
}

void gfx_citro3d_menu_init_tex_single(MenuTexId id)
{
    const TexLoadGroup* load_group = load_tex_group(id);
    C3D_Tex* c3d_tex = &load_group->output_tex->c3d_tex;

    citro3d_helpers_load_t3x_texture(c3d_tex, NULL, *load_group->t3x_data, load_group->t3x_size);
    C3D_TexSetFilter(c3d_tex, GPU_LINEAR, GPU_NEAREST);
    C3D_TexFlush(c3d_tex);

    load_group->output_tex->size_s   = load_group->original_w / ((float) c3d_tex->width);
    load_group->output_tex->size_t   = load_group->original_h / ((float) c3d_tex->height);
    load_group->output_tex->origin_s = 0;
    load_group->output_tex->origin_t = (c3d_tex->height - load_group->original_h) / (float) c3d_tex->height;
}

void gfx_citro3d_menu_init_tex_release()
{
    for (MenuTexId id = 0; id < MENU_TEX_FULLSCREEN_TEST; id++)
        gfx_citro3d_menu_init_tex_single(id);
}

void gfx_citro3d_menu_init_tex_all()
{
    for (MenuTexId id = 0; id < MENU_TEX_COUNT; id++)
        gfx_citro3d_menu_init_tex_single(id);
}

