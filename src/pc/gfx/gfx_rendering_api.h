#ifndef GFX_RENDERING_API_H
#define GFX_RENDERING_API_H

/*
 * Graphical rendering API. This is platform-dependent!
 * Please ensure in the Makefile that only one implementation is actually compiled.
 * Implementations must also define all incomplete types listed below.
 * 
 * The Rendering API is how the emulated RSP interfaces with the underlying graphics hardware.
 * These functions are defined by the implementation, and the implementation should be selected by the makefile.
 * There also exist optional features; these are NOT yet properly supported externally, but they are codified.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <PR/gbi.h>

#include "src/pc/gfx/color_formats.h"
#include "src/pc/gfx/gfx_cc.h"
#include "multi_viewport/multi_viewport.h"

#define GFX_DISABLE 0
#define GFX_ENABLE 1

#ifdef TARGET_N3DS
#define GFX_RAPI_TEXTURE_FORMATS  GFX_ENABLE
#define GFX_RAPI_STEREOSCOPIC_3D  GFX_ENABLE
#define GFX_RAPI_FOG              GFX_ENABLE
#define GFX_RAPI_VERTEX_MATRICES  GFX_ENABLE
#define GFX_RAPI_VERTEX_LIGHTING  GFX_ENABLE
#define GFX_RAPI_VERTEX_TEXGEN    GFX_ENABLE
#define GFX_RAPI_MULTI_VIEWPORT   GFX_ENABLE
#define GFX_RAPI_GPU_TEXCOORDS    GFX_ENABLE
#define GFX_RAPI_COLOR_COMBINER   GFX_ENABLE
#else
#define GFX_RAPI_TEXTURE_FORMATS  GFX_DISABLE
#define GFX_RAPI_STEREOSCOPIC_3D  GFX_DISABLE
#define GFX_RAPI_FOG              GFX_DISABLE
#define GFX_RAPI_VERTEX_MATRICES  GFX_DISABLE
#define GFX_RAPI_VERTEX_LIGHTING  GFX_DISABLE
#define GFX_RAPI_VERTEX_TEXGEN    GFX_DISABLE
#define GFX_RAPI_MULTI_VIEWPORT   GFX_DISABLE
#define GFX_RAPI_GPU_TEXCOORDS    GFX_DISABLE
#define GFX_RAPI_COLOR_COMBINER   GFX_DISABLE
#endif


#define GRUNK static

// Types to be defined by the implementation
#if GFX_RAPI_COLOR_COMBINER == GFX_DISABLE
struct ShaderProgram;
#endif

// Mandatory functions
bool                    gfx_rapi_z_is_from_0_to_1           ();                                                                                   // Returns true if the API's depth range is from 0 to 1.

#if GFX_RAPI_COLOR_COMBINER == GFX_DISABLE
void                    gfx_rapi_unload_shader              (struct ShaderProgram *old_prg);                                                      // Unloads the given shader.
void                    gfx_rapi_load_shader                (struct ShaderProgram *new_prg);                                                      // Loads the given shader.
struct ShaderProgram*   gfx_rapi_create_and_load_new_shader (uint32_t shader_id);                                                                 // Creates and loads a new shader with the given ID. 
struct ShaderProgram*   gfx_rapi_lookup_shader              (uint32_t shader_id);                                                                 // Looks up an existing shader with the given ID.
void                    gfx_rapi_shader_get_info            (struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]);              // Returns info about the currently loaded shader.
#endif

uint32_t                gfx_rapi_new_texture                ();                                                                                   // Returns the index for a new texture. If no textures are available, it will return a valid texture, but which texture it is is implementation-defined.
// GRUNK void                    gfx_rapi_select_texture             (int tile, uint32_t texture_id);                                                      // Selects a currently loaded texture.
// GRUNK void                    gfx_rapi_set_sampler_parameters     (int texture_slot, bool linear_filter, uint32_t clamp_mode_s, uint32_t clamp_mode_t); // Sets the texture sampling parameters for the given texture slot.
// GRUNK void                    gfx_rapi_set_depth_test             (bool depth_test);                                                                    // Enables or disables GPU depth test.
// GRUNK void                    gfx_rapi_set_depth_mask             (bool z_upd);                                                                         // Enables or disables GPU depth upadtes.
// GRUNK void                    gfx_rapi_set_zmode_decal            (bool zmode_decal);                                                                   // Enables or disables decal mode.
// GRUNK void                    gfx_rapi_set_viewport               (int x, int y, int width, int height);                                                // Sets the GPU viewport settings.
// GRUNK void                    gfx_rapi_set_scissor                (int x, int y, int width, int height);                                                // Sets the GPU scissor settings.
// GRUNK void                    gfx_rapi_set_use_alpha              (bool use_alpha);                                                                     // Enables or disables alpha blending.
// GRUNK void                    gfx_rapi_draw_triangles             (float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris);                       // Draws the given triangles.
void                    gfx_rapi_init                       ();                                                                                   // Initializes the GFX Rendering API.
void                    gfx_rapi_on_resize                  ();                                                                                   // Called when the window is resized.
void                    gfx_rapi_start_frame                ();                                                                                   // Called at the start of a frame.
void                    gfx_rapi_end_frame                  ();                                                                                   // Called at the end of a frame.
void                    gfx_rapi_finish_render              ();                                                                                   // Called after end_frame, but only if the frame was not dropped.

// Optional feature: GPU handles texture formats individually
#if GFX_RAPI_TEXTURE_FORMATS == GFX_ENABLE
void                    gfx_rapi_upload_texture_rgba16      (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_rgba32      (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_ia4         (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_ia8         (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_ia16        (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_i4          (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_i8          (const uint8_t *data, int width, int height);                           // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_ci4         (const uint8_t *data, const uint8_t* palette, int width, int height);   // Uploads a texture with the given N64 format.
void                    gfx_rapi_upload_texture_ci8         (const uint8_t *data, const uint8_t* palette, int width, int height);   // Uploads a texture with the given N64 format.
#else
void                    gfx_rapi_upload_texture             (const uint8_t *rgba32_buf, int width, int height);                     // Uploads a texture with the N64 RGBA8888 format.
#endif

// Optional feature: GPU handles fog rendering
#if GFX_RAPI_FOG == GFX_ENABLE
// GRUNK void                    gfx_rapi_set_fog                    (uint16_t from, uint16_t to);                   // Sets the GPU fog distance factors.
// GRUNK void                    gfx_rapi_set_fog_color              (uint8_t r, uint8_t g, uint8_t b, uint8_t a);   // Sets the GPU fog color.
// GRUNK void                    gfx_rapi_set_fog_color_u32          (uint32_t color);                               // Sets the GPU fog color, RGBA8888 format.
#endif

// Optional feature: stereoscopic 3D rendering
#if GFX_RAPI_STEREOSCOPIC_3D == GFX_ENABLE
// GRUNK void                    gfx_rapi_set_2d_mode                (int mode_2d);          // Used for depth overrides.
// GRUNK void                    gfx_rapi_set_iod                    (float z, float w);     // Adjusts the strength of the depth effect.
#endif

// Optional feature: GPU handles vertex-load matrix-vector multiplication
#if GFX_RAPI_VERTEX_MATRICES == GFX_ENABLE
// GRUNK void                    gfx_rapi_set_model_view_matrix      (float mtx[4][4]);          // Sets the speficied matrix of the current matrix set, but does not send it to the GPU.
// GRUNK void                    gfx_rapi_set_projection_matrix      (float mtx[4][4]);          // Sets the speficied matrix of the current matrix set, but does not send it to the GPU.
// GRUNK void                    gfx_rapi_apply_model_view_matrix    ();                         // Sends the specified matrix from the current matrix set to the GPU.
// GRUNK void                    gfx_rapi_apply_projection_matrix    ();                         // Sends the specified matrix from the current matrix set to the GPU.
// GRUNK void                    gfx_rapi_select_matrix_set          (uint32_t matrix_set_id);   // Selects one of the given matrix sets, but does not send it to the GPU.
void                    gfx_rapi_set_backface_culling_mode  (uint32_t culling_mode);    // Sets the GPU's backface culling mode
#endif

// Optional feature: GPU handles vertex-load vertex lighting calculation
#if GFX_RAPI_VERTEX_LIGHTING == GFX_ENABLE
// GRUNK void                    gfx_rapi_enable_lighting            (bool enable);                   // Enables or disables lights. When enabled, the shader color/normals VBO uses an s8 format, else u8.
// GRUNK void                    gfx_rapi_set_num_lights             (int num_lights);                // Sets the number of lights, including ambient. Set to 1 for ambient-only.
// GRUNK void                    gfx_rapi_configure_light            (int light_id, Light_t* light);  // Configures a single directional light. A light_id of 0 is ambient, for which direction is ignored.
#endif

// Optional feature: GPU handles vertex-load texture generation
#if GFX_RAPI_VERTEX_TEXGEN == GFX_ENABLE
// GRUNK void                    gfx_rapi_enable_texgen              (bool enable);            // Enables or disables texgen.
// GRUNK void                    gfx_rapi_set_texture_scaling_factor (uint32_t s, uint32_t t); // Sets the texture scaling factor for use with texgen. These use a U16.16 format.
#endif

// Optional feature: multiple viewports
#if GFX_RAPI_MULTI_VIEWPORT == GFX_ENABLE
void                    gfx_rapi_set_viewport_clear_color           (uint32_t viewport_id, uint8_t r, uint8_t g, uint8_t b, uint8_t a);     // Sets the clear color of the given viewport.
void                    gfx_rapi_set_viewport_clear_color_u32       (uint32_t viewport_id, uint32_t color);                                 // Sets the clear color of the given viewport. Format RGBA8888.
void                    gfx_rapi_enable_viewport_clear_buffer_flag  (uint32_t viewport_id, enum ViewportClearBuffer mode);                  // Enables a buffer clear flag for the specified viewport. Flags are reset at the start of the frame.
#endif

// Optional feature: GPU texture coordinate calculation
#if GFX_RAPI_GPU_TEXCOORDS == GFX_ENABLE
// GRUNK void                    gfx_rapi_set_uv_offset                    (float offset);                                                               // Sets the global offset for UV coordinates.
// GRUNK void                    gfx_rapi_set_texture_settings             (int16_t upper_left_s, int16_t upper_left_t, int16_t width, int16_t height);  // Sets various texture settings.
#endif

// Optional feature: GPU color combiner
#if GFX_RAPI_COLOR_COMBINER == GFX_ENABLE
// GRUNK void                    gfx_rapi_select_color_combiner            (size_t cc_index);                                              // Selects a color combiner by index. Bounds checks are not enforced. If the current CC is already loaded, does nothing.
// GRUNK size_t                  gfx_rapi_lookup_or_create_color_combiner  (ColorCombinerId cc_id);                                               // Looks up or creates a color combiner from the given CCID.
// GRUNK void                    gfx_rapi_color_combiner_get_info          (size_t cc_index, uint8_t *num_inputs, bool used_textures[2]);  // Returns some info about the current color combiner.

// GRUNK void                    gfx_rapi_set_cc_prim_color                (uint32_t color);                                               // Sets the GPU prim color. This is global across color combiners.
// GRUNK void                    gfx_rapi_set_cc_env_color                 (uint32_t color);                                               // Sets the GPU env color. This is global across color combiners.
#endif

#endif
