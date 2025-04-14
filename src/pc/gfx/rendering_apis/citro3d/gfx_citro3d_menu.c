#include "gfx_citro3d_menu.h"

#include "gfx_citro3d_menu_tex.h"
#include "gfx_citro3d_screens.h"
#include "gfx_citro3d_helpers.h"
#include "src/pc/gfx/windowing_apis/3ds/gfx_3ds.h"
#include "src/pc/gfx/shader_programs/gfx_n3ds_shprog_emu64.h"
#include "src/pc/click_menu.h"
#include "src/pc/pc_macros.h"
#include "src/pc/auto_addr.h"

#define VERTEX_BUFFER_UNIT_SIZE sizeof(float)

typedef struct
{
    int16_t xyzw[4];
    int16_t texcoord[2];
} ButtonVertex;

static const ButtonVertex vertex_list_button[] =
{
    {.xyzw = {0, 0, 1, 1}, .texcoord = { 0, 0 }}, // BL (lower triangle)
    {.xyzw = {1, 1, 1, 1}, .texcoord = { 1, 1 }}, // TR
    {.xyzw = {1, 0, 1, 1}, .texcoord = { 1, 0 }}, // BR
    
    {.xyzw = {0, 0, 1, 1}, .texcoord = { 0, 0 }}, // BL (upper triangle)
    {.xyzw = {0, 1, 1, 1}, .texcoord = { 0, 1 }}, // TL
    {.xyzw = {1, 1, 1, 1}, .texcoord = { 1, 1 }}, // TR
};

static VertexBuffer vertex_buffer;
static ShaderProgram shader_program;

static C3D_Mtx projection_mtx;
static C3D_TexEnv texenv, texenv_inverted;

static void draw_button_internal(Click_Button* button, WrappedTexture* tex)
{
    C3D_Mtx model_view_mtx;

    if (tex == NULL) return;

    Mtx_Identity(&model_view_mtx);
    Mtx_Scale(&model_view_mtx, button->position.w, button->position.h, 1.0f);
    Mtx_Translate(&model_view_mtx, button->position.x, 240 - button->position.h - button->position.y, 0.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_model_view_mtx, &model_view_mtx);

    C3D_FVUnifSet(GPU_VERTEX_SHADER, EMU64_ULOC_tex_settings_1, tex->size_s, tex->size_t, tex->origin_s, tex->origin_t);
    C3D_TexBind(0, &tex->c3d_tex);

    if (button->currently_held)
        C3D_SetTexEnv(0, &texenv_inverted);
    else
        C3D_SetTexEnv(0, &texenv);

    C3D_DrawArrays(GPU_TRIANGLES, 0, ARRAY_COUNT(vertex_list_button)); // 2 triangles
}

void draw_button_basic(Click_Button* button, UNUSED void* common_data)
{
    draw_button_internal(button, (WrappedTexture*) button->graphic);
}

void draw_button_single(Click_Button* button, UNUSED void* common_data)
{
    draw_button_internal(button, autoaddr_resolve_single((AutoAddr_Single*) button->graphic));
}

void draw_button_bool(Click_Button* button, UNUSED void* common_data)
{
    draw_button_internal(button, autoaddr_resolve_bool((AutoAddr_Bool*) button->graphic));
}

void draw_button_enum(Click_Button* button, UNUSED void* common_data)
{
    draw_button_internal(button, autoaddr_resolve_enum((AutoAddr_Enum*) button->graphic));
}

void draw_button_auto(Click_Button* button, UNUSED void* common_data)
{
    draw_button_internal(button, autoaddr_resolve((AutoAddr*) button->graphic));
}

void gfx_citro3d_menu_init(void)
{
    gfx_citro3d_menu_init_tex_all();

    shader_program = citro3d_helpers_init_shader(&emu64_shader_menu, &vertex_buffer, sizeof(vertex_list_button));
    memcpy(shader_program.vertex_buffer->ptr, vertex_list_button, sizeof(vertex_list_button));
    
    C3D_TexEnvInit(&texenv);
    C3D_TexEnvSrc(&texenv, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(&texenv, C3D_Both, GPU_REPLACE);
    
    C3D_TexEnvInit(&texenv_inverted);
    C3D_TexEnvSrc(&texenv_inverted, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvOpRgb(&texenv_inverted, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR, 0, 0);
    C3D_TexEnvOpAlpha(&texenv_inverted, GPU_TEVOP_A_SRC_ALPHA, 0, 0);
    C3D_TexEnvFunc(&texenv_inverted, C3D_Both, GPU_REPLACE);
    
    Mtx_OrthoTilt(&projection_mtx, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    g3dsGfxState.bottom_screen_needs_render = true;
}

void gfx_citro3d_menu_exit(void)
{
    citro3d_helpers_free_shader(&shader_program);
}

void gfx_citro3d_menu_on_draw_start(void)
{
    C3D_FrameDrawOn(gTargetBottom);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, EMU64_ULOC_projection_mtx, &projection_mtx);
    C3D_SetTexEnv(0, &texenv);

    C3D_BindProgram(&shader_program.pica_shader_program);
    C3D_SetAttrInfo(&shader_program.vertex_buffer->attr_info);
    C3D_SetBufInfo(&shader_program.vertex_buffer->buf_info);
}

void gfx_citro3d_menu_on_draw_finish(void)
{
    g3dsGfxState.bottom_screen_needs_render = false;

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_TexEnvInit(C3D_GetTexEnv(0));

    // We don't have to reset uniforms or shader info
}
