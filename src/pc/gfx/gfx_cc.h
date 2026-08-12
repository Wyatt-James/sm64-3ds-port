#ifndef GFX_CC_H
#define GFX_CC_H

#include <stdint.h>
#include <stdbool.h>
#include <PR/gbi.h>

#define DELIBERATELY_INVALID_CC_ID ~0 // Represents an invalid color combiner, to be used for initial conditions.
#define DEFAULT_CC_ID calculate_cc_id(COMBINE_MODE(convert_color_combiner(0, 0, 0, G_CCMUX_SHADE), convert_color_combiner(0, 0, 0, G_ACMUX_SHADE)), false, false, false, false) // A real CC ID that produces only black pixels with alpha 0.

#define SHADER_OPT_ALPHA        (1 << 24)
#define SHADER_OPT_FOG          (1 << 25)
#define SHADER_OPT_TEXTURE_EDGE (1 << 26)
#define SHADER_OPT_NOISE        (1 << 27)

#define COMBINE_MODE(rgb, alpha) (((CombineMode) rgb) | (((CombineMode) alpha) << 12))

typedef uint32_t ColorCombinerId; // Contains the entire description of a color combiner, as per gfx_pc.c.
typedef uint32_t CCShaderId;
typedef uint32_t CombineMode; // To be used with the COMBINE_MODE macro.

enum
{
    CC_0,
    CC_TEXEL0,
    CC_TEXEL1,
    CC_PRIM,
    CC_SHADE,
    CC_ENV,
    CC_TEXEL0A,
    CC_LOD
};

typedef enum
{
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1
} ShaderSource;

struct CCInput {
    uint8_t rgb : 4, alpha : 4;
};

// (a - b) * c + d
union CCInputMapping {
    struct {
        struct CCInput a, b, c, d;
    };

    struct CCInput arr[4]; // Uses format [RGB | A][input].
};

struct CCFeatures {
    union CCInputMapping cc;  // CC input mapping.
    bool used_textures_0   : 1;  // If both are true, 2-cycle must be enabled.
    bool do_single_0       : 1;  // True if there is only an additive component.
    bool do_multiply_0     : 1;  // True if there are no subtractive or additive components.
    bool do_mix_0          : 1;  // True if subtractive and additive components are equal.
    bool do_single_1       : 1;  // True if there is only an additive component.
    bool do_multiply_1     : 1;  // True if there are no subtractive or additive components.
    bool do_mix_1          : 1;  // True if subtractive and additive components are equal.
    bool used_textures_1   : 1;  // If both are true, 2-cycle must be enabled.
    // Byte boundary
    bool opt_alpha         : 1;  // True if alpha is enabled.
    bool opt_fog           : 1;  // True if fog is enabled.
    bool opt_texture_edge  : 1;  // True if alpha rejection is enabled.
    bool opt_noise         : 1;  // True if noise is enabled.
    bool color_alpha_same  : 1;  // True if color and alpha use identical mixing setups.
    uint8_t num_inputs     : 3;  // Number of CC inputs. Max 4.
};


#ifdef __cplusplus
extern "C" {
#endif

// Populates a CCFeatures struct from the given shader ID.
void gfx_cc_get_features(CCShaderId shader_id, struct CCFeatures *cc_features);

// Generates a set of CC shader-input mappings and a shader ID from a CC ID.
// Unused mappings are set to CC_0.
void gfx_cc_generate_cc(ColorCombinerId cc_id, union CCInputMapping* out_shader_input_mappings, CCShaderId* out_shader_id);

static uint8_t color_comb_component(uint32_t v)
{
    switch (v) {
        case G_CCMUX_TEXEL0:
            return CC_TEXEL0;
        case G_CCMUX_TEXEL1:
            return CC_TEXEL1;
        case G_CCMUX_PRIMITIVE:
            return CC_PRIM;
        case G_CCMUX_SHADE:
            return CC_SHADE;
        case G_CCMUX_ENVIRONMENT:
            return CC_ENV;
        case G_CCMUX_TEXEL0_ALPHA:
            return CC_TEXEL0A;
        case G_CCMUX_LOD_FRACTION:
            return CC_LOD;
        default:
            return CC_0;
    }
}

static inline ColorCombinerId color_combiner_id(ShaderSource a, ShaderSource b, ShaderSource c, ShaderSource d)
{
    return (d << 9) | (c << 6) | (b << 3) | a;
}

static inline ColorCombinerId convert_color_combiner(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    return color_combiner_id(color_comb_component(a), color_comb_component(b), color_comb_component(c), color_comb_component(d));
}

static inline ColorCombinerId calculate_cc_id(CombineMode combine_mode, bool use_fog, bool texture_edge, bool use_noise, bool use_alpha)
{
    ColorCombinerId id = combine_mode;

    if (use_fog)      id |= SHADER_OPT_FOG;
    if (texture_edge) id |= SHADER_OPT_TEXTURE_EDGE;
    if (use_noise)    id |= SHADER_OPT_NOISE;
    if (use_alpha)
        id |= SHADER_OPT_ALPHA;
    else
        id &= ~0xfff000;

    return id;
}

#ifdef __cplusplus
}
#endif

#endif
