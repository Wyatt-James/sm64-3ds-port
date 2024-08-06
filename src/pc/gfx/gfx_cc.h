#ifndef GFX_CC_H
#define GFX_CC_H

#include <stdint.h>
#include <stdbool.h>

enum {
    CC_0,
    CC_TEXEL0,
    CC_TEXEL1,
    CC_PRIM,
    CC_SHADE,
    CC_ENV,
    CC_TEXEL0A,
    CC_LOD
};

enum {
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1
};

#define SHADER_OPT_ALPHA        (1 << 24)
#define SHADER_OPT_FOG          (1 << 25)
#define SHADER_OPT_TEXTURE_EDGE (1 << 26)
#define SHADER_OPT_NOISE        (1 << 27)

struct CCFeatures {
    uint8_t c[2][4];        // CC inputs. Uses format [RGB | A][input].
    bool opt_alpha;         // True if alpha is enabled.
    bool opt_fog;           // True if fog is enabled.
    bool opt_texture_edge;  // True if alpha rejection is enabled.
    bool opt_noise;         // True if noise is enabled.
    bool used_textures[2];  // If both are true, use 2-cycle.
    int num_inputs;         // Number of CC inputs. Max 4.
    bool do_single[2];      // Trtue if there is only an additive component.
    bool do_multiply[2];    // True if there are no subtractive or additive components.
    bool do_mix[2];         // true if Subtractive and additive components are equal.
    bool color_alpha_same;  // True if Color and alpha use identical mixing setups.
};

#ifdef __cplusplus
extern "C" {
#endif

// Populates a CCFeatures struct from the given shader ID.
void gfx_cc_get_features(uint32_t shader_id, struct CCFeatures *cc_features);

// Generates a set of CC shader-input mappings and a shader ID from a CC ID.
// Unused mappings are set to CC_0.
void gfx_cc_generate_cc(uint32_t cc_id, uint8_t out_shader_input_mappings[4][2], uint32_t* out_shader_id);

#ifdef __cplusplus
}
#endif

#endif
