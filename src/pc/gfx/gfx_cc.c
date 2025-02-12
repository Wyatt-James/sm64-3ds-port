#include <string.h>

#include "gfx_cc.h"

#define MAX(a_, b_) ((a_ > b_) ? a_ : b_)
#define MIN(a_, b_) ((a_ < b_) ? a_ : b_)

// The layout of an unpacked ShaderId. By pure luck, it's been reversed enough to match the correct order.
struct UnpackedShaderId {
    ShaderInput RGB_a,
                RGB_b,
                RGB_c,
                RGB_d,
                Alpha_a,
                Alpha_b,
                Alpha_c,
                Alpha_d;
};

static const ShaderInput dapper_mapper[CC_COUNT + 1] = {
    [CC_0]       = SHADER_0,
    [CC_TEXEL0]  = SHADER_TEXEL0,
    [CC_TEXEL1]  = SHADER_TEXEL1,
    [CC_TEXEL0A] = SHADER_TEXEL0A,
    [CC_PRIM]    = SHADER_0,       // Next SHADER_INPUT_N
    [CC_SHADE]   = SHADER_0,       // Next SHADER_INPUT_N
    [CC_ENV]     = SHADER_0,       // Next SHADER_INPUT_N
    [CC_LOD]     = SHADER_0,       // Next SHADER_INPUT_N
    [CC_COUNT]   = SHADER_0,       // Shouldn't happen
};

// Populates a CCFeatures struct from the given shader sources and flags.
static void gfx_cc_get_features(
    uint8_t shader_sources[2][4], // ShaderInput enum
    bool opt_noise,
    bool opt_texture_edge,
    bool opt_fog,
    bool opt_alpha,
    struct CCFeatures *cc_features)
{
    memset(cc_features, 0, sizeof(*cc_features));
    // memcpy(cc_features->cc.arr, shader_sources, sizeof(cc_features->cc.arr));
    for (int i = 0; i < 4; i++) {
        cc_features->cc.arr[0][i] = shader_sources[0][3 - i]; // WYATT_TODO we do a little reverse
        cc_features->cc.arr[1][i] = shader_sources[1][3 - i];
    }

    cc_features->opt_noise        = opt_noise;
    cc_features->opt_texture_edge = opt_texture_edge;
    cc_features->opt_fog          = opt_fog;
    cc_features->opt_alpha        = opt_alpha;

    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 4; j++) {
            uint8_t cc_source = cc_features->cc.arr[i][j];
            switch (cc_source) {
                case SHADER_INPUT_1:
                case SHADER_INPUT_2:
                case SHADER_INPUT_3:
                case SHADER_INPUT_4:
                    cc_features->num_inputs = MAX(cc_features->num_inputs, cc_source);
                    break;
                case SHADER_TEXEL0:
                case SHADER_TEXEL0A:
                    cc_features->used_textures[0] = true;
                    break;
                case SHADER_TEXEL1:
                    cc_features->used_textures[1] = true;
                    break;
            }
        }
    }

    for (size_t i = 0; i < 2; i++) {
        cc_features->do_single[i]   = cc_features->cc.arr[i][2] == 0;                                   // only an additive component.
        cc_features->do_multiply[i] = cc_features->cc.arr[i][1] == 0 && cc_features->cc.arr[i][3] == 0; // no subtractive or additive components.
        cc_features->do_mix[i]      = cc_features->cc.arr[i][1] == cc_features->cc.arr[i][3];           // subtractive and additive components are equal.
    }

    cc_features->color_alpha_same = memcmp(shader_sources[0], shader_sources[1], sizeof(shader_sources[0])) == 0;
}

// The layout of an unpacked CCID
struct UnpackedCcid {
    PcPortCombinerSource RGB_d,
                         RGB_c,
                         RGB_b,
                         RGB_a,
                         Alpha_d,
                         Alpha_c,
                         Alpha_b,
                         Alpha_a;
};

// The order of processing
struct ProcessOrder {
    ShaderInput RGB_d,
                RGB_c,
                RGB_b,
                RGB_a,
                Alpha_d,
                Alpha_c,
                Alpha_b,
                Alpha_a;
};

// These are mapped backward with respect to their PcPortCombinerSource originals,
//  due to CCID unpacking being in reverse, but forward with respect to SHADER_INPUT_.
// This will only ever contain SHADER_INPUT_ inputs.
struct OutputShaderInputMappings {
    ShaderInput RGB_input_1,
                RGB_input_2,
                RGB_input_3,
                RGB_input_4,
                Alpha_input_1,
                Alpha_input_2,
                Alpha_input_3,
                Alpha_input_4;
};

void gfx_cc_generate_cc(ColorCombinerId cc_id, union CCInputMapping* out_shader_input_mappings, struct CCFeatures* out_cc_features)
{
    memset(out_shader_input_mappings, 0, sizeof(*out_shader_input_mappings));
    uint8_t shader_sources[2][4] = {{0}};

    ExtractedCcid unpacked_inputs = CCID_UNPACK(cc_id);

    for (size_t i = 0; i < 2; i++) {

        uint8_t custom_inputs[CC_COUNT] = {0}; // PcPortCombinerSource -> SHADER_INPUT_
        ShaderInput next_custom_input = SHADER_INPUT_1;

        for (size_t j = 0; j < 4; j++) {
            const uint8_t cc_input = unpacked_inputs.arr[i][j];
            ShaderInput shader_input;

            switch (MIN(cc_input, CC_COUNT)) {
                default:
                    shader_input = dapper_mapper[cc_input];
                    break;
                case CC_PRIM:
                case CC_SHADE:
                case CC_ENV:
                case CC_LOD:

                    // Allocate custom inputs once per-type
                    if (custom_inputs[cc_input] == 0) {
                        out_shader_input_mappings->arr[i][next_custom_input - 1] = cc_input;
                        custom_inputs[cc_input] = next_custom_input++;
                    }

                    shader_input = custom_inputs[cc_input];
                    break;
            }

            shader_sources[i][j] = shader_input;
        }
    }

    gfx_cc_get_features(
        shader_sources,
        unpacked_inputs.use_noise,
        unpacked_inputs.texture_edge,
        unpacked_inputs.use_fog,
        unpacked_inputs.use_alpha,
        out_cc_features);
}
