#ifndef GFX_CC_H
#define GFX_CC_H

#include <stdint.h>
#include <stdbool.h>

#include "src/pc/bit_flag.h"

typedef enum {
    CC_0,
    CC_TEXEL0,
    CC_TEXEL1,
    CC_PRIM,
    CC_SHADE,
    CC_ENV,
    CC_TEXEL0A,
    CC_LOD,
    CC_COUNT
} PcPortCombinerSource;

typedef enum {
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1
} ShaderInput;

typedef union {
    struct {
        uint8_t C1_RGB_a, // PcPortCombinerSource
                C1_RGB_b,
                C1_RGB_c,
                C1_RGB_d,
                C1_Alpha_a, // If use_alpha is false, Alpha sources are zeroed.
                C1_Alpha_b,
                C1_Alpha_c,
                C1_Alpha_d;
        bool use_noise,
             texture_edge,
             use_fog,
             use_alpha;
    };
    uint8_t arr[2][4]; // [rgb|alpha] [abcd]
} ExtractedCcid;

/*
    Structure of a ColorCombinerId, MSB-first:
    unused       : 4,
    use_noise    : 1,
    texture_edge : 1,
    use_fog      : 1,
    use_alpha    : 1;
    C1_RGB_a     : 3, // PcPortCombinerSource
    C1_RGB_b     : 3,
    C1_RGB_c     : 3,
    C1_RGB_d     : 3,
    C1_Alpha_a   : 3, // If use_alpha is false, Alpha sources are zeroed.
    C1_Alpha_b   : 3,
    C1_Alpha_c   : 3,
    C1_Alpha_d   : 3;
*/
// Contains the entire description of a color combiner, as per gfx_pc.c.
typedef uint32_t ColorCombinerId;

// CCID bit indices
#define CCID_OPTN (27) // Noise bit
#define CCID_OPTT (26) // Texture Edge bit
#define CCID_OPTF (25) // Fog bit
#define CCID_OPTA (24) // Alpha bit
#define CCID_1CA  (21) // 1-cycle Color A
#define CCID_1CB  (18) // 1-cycle Color B
#define CCID_1CC  (15) // 1-cycle Color C
#define CCID_1CD  (12) // 1-cycle Color D
#define CCID_1AA  ( 9) // 1-cycle Alpha A
#define CCID_1AB  ( 6) // 1-cycle Alpha B
#define CCID_1AC  ( 3) // 1-cycle Alpha C
#define CCID_1AD  ( 0) // 1-cycle Alpha D

// CCID parameter extractors
#define CCID_USE_NOISE(cc_id_)    (GET_BITS2(cc_id_, CCID_OPTN, 1))
#define CCID_USE_TEX_EDGE(cc_id_) (GET_BITS2(cc_id_, CCID_OPTT, 1))
#define CCID_USE_FOG(cc_id_)      (GET_BITS2(cc_id_, CCID_OPTF, 1))
#define CCID_USE_ALPHA(cc_id_)    (GET_BITS2(cc_id_, CCID_OPTA, 1))
#define CCID_C1_RGB_A(cc_id_)     (GET_BITS2(cc_id_, CCID_1CA,  3))
#define CCID_C1_RGB_B(cc_id_)     (GET_BITS2(cc_id_, CCID_1CB,  3))
#define CCID_C1_RGB_C(cc_id_)     (GET_BITS2(cc_id_, CCID_1CC,  3))
#define CCID_C1_RGB_D(cc_id_)     (GET_BITS2(cc_id_, CCID_1CD,  3))
#define CCID_C1_ALPHA_A(cc_id_)   (GET_BITS2(cc_id_, CCID_1AA,  3))
#define CCID_C1_ALPHA_B(cc_id_)   (GET_BITS2(cc_id_, CCID_1AB,  3))
#define CCID_C1_ALPHA_C(cc_id_)   (GET_BITS2(cc_id_, CCID_1AC,  3))
#define CCID_C1_ALPHA_D(cc_id_)   (GET_BITS2(cc_id_, CCID_1AD,  3))

// Unpacks a ColorCombinerId into an ExtractedCcid union.
#define CCID_UNPACK(cc_id_) (ExtractedCcid) {   \
    .use_noise    = CCID_USE_NOISE    (cc_id_), \
    .texture_edge = CCID_USE_TEX_EDGE (cc_id_), \
    .use_fog      = CCID_USE_FOG      (cc_id_), \
    .use_alpha    = CCID_USE_ALPHA    (cc_id_), \
    .C1_RGB_a     = CCID_C1_RGB_A     (cc_id_), \
    .C1_RGB_b     = CCID_C1_RGB_B     (cc_id_), \
    .C1_RGB_c     = CCID_C1_RGB_C     (cc_id_), \
    .C1_RGB_d     = CCID_C1_RGB_D     (cc_id_), \
    .C1_Alpha_a   = CCID_C1_ALPHA_A   (cc_id_), \
    .C1_Alpha_b   = CCID_C1_ALPHA_B   (cc_id_), \
    .C1_Alpha_c   = CCID_C1_ALPHA_C   (cc_id_), \
    .C1_Alpha_d   = CCID_C1_ALPHA_D   (cc_id_), \
}

// Bitfields
#define SHADER_OPT_NOISE        (1 << CCID_OPTN)
#define SHADER_OPT_TEXTURE_EDGE (1 << CCID_OPTT)
#define SHADER_OPT_FOG          (1 << CCID_OPTF)
#define SHADER_OPT_ALPHA        (1 << CCID_OPTA)

// WYATT_TODO make CCID_PACK

#define DELIBERATELY_INVALID_CC_ID ~0 // Represents an invalid color combiner, to be used for initial conditions.

// (a - b) * c + d
union CCInputMapping {
    struct {
        uint8_t rgb_a,
                rgb_b,
                rgb_c,
                rgb_d,
                alpha_a,
                alpha_b,
                alpha_c,
                alpha_d;
    };

    struct {
        uint8_t rgb[4], alpha[4]; // a, b, c, d
    };

    uint8_t arr[2][4]; // Uses format [RGB | A][input].
};

struct CCFeatures {
    union CCInputMapping cc;  // CC input mapping.
    bool opt_alpha;           // True if alpha is enabled.
    bool opt_fog;             // True if fog is enabled.
    bool opt_texture_edge;    // True if alpha rejection is enabled.
    bool opt_noise;           // True if noise is enabled.
    bool used_textures[2];    // If both are true, 2-cycle must be enabled.
    int num_inputs;           // Number of CC inputs. Max 4.
    bool do_single[2];        // True if there is only an additive component.
    bool do_multiply[2];      // True if there are no subtractive or additive components.
    bool do_mix[2];           // True if subtractive and additive components are equal.
    bool color_alpha_same;    // True if color and alpha use identical mixing setups.
};

#ifdef __cplusplus
extern "C" {
#endif

// Generates a set of CC shader-input mappings and a CCFeatures from a ColorCombinerId.
void gfx_cc_generate_cc(ColorCombinerId cc_id, union CCInputMapping* out_shader_input_mappings, struct CCFeatures* out_cc_features);

#ifdef __cplusplus
}
#endif

#endif
