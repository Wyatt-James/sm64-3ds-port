#include <string.h>

#include "gfx_cc.h"

#define MAX(a_, b_) ((a_ > b_) ? a_ : b_)
#define MIN(a_, b_) ((a_ < b_) ? a_ : b_)

void gfx_cc_get_features(uint32_t shader_id, struct CCFeatures *cc_features) {
    for (int i = 0; i < 4; i++) {
        cc_features->c[0][i] = (shader_id >> (i * 3)) & 7;
        cc_features->c[1][i] = (shader_id >> (12 + i * 3)) & 7;
    }

    cc_features->opt_alpha        = (shader_id & SHADER_OPT_ALPHA)        != 0;
    cc_features->opt_fog          = (shader_id & SHADER_OPT_FOG)          != 0;
    cc_features->opt_texture_edge = (shader_id & SHADER_OPT_TEXTURE_EDGE) != 0;
    cc_features->opt_noise        = (shader_id & SHADER_OPT_NOISE)        != 0;

    cc_features->used_textures[0] = false;
    cc_features->used_textures[1] = false;
    cc_features->num_inputs = 0;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            uint8_t cc_source = cc_features->c[i][j];
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

    for (int i = 0; i < 2; i++) {
        cc_features->do_single[i] = cc_features->c[i][2] == 0;
        cc_features->do_multiply[i] = cc_features->c[i][1] == 0 && cc_features->c[i][3] == 0;
        cc_features->do_mix[i] = cc_features->c[i][1] == cc_features->c[i][3];
    }
    cc_features->color_alpha_same = (shader_id & 0xfff) == ((shader_id >> 12) & 0xfff);
}

void gfx_cc_generate_cc(uint32_t cc_id, uint8_t out_shader_input_mappings[4][2], uint32_t* out_shader_id) {
    uint32_t shader_id = (cc_id >> 24) << 24;
    uint8_t c[4][2] = {{0}};

    bzero(out_shader_input_mappings, sizeof(uint8_t) * 4 * 2);

    for (int i = 0; i < 4; i++) {
        c[i][0] = (cc_id >> (i * 3)) & 7;
        c[i][1] = (cc_id >> (12 + i * 3)) & 7;
    }

    for (int i = 0; i < 2; i++) {
        uint8_t input_number[8] = {0};
        int next_input_number = SHADER_INPUT_1;
        for (int j = 0; j < 4; j++) {
            int shader_input = 0;
            uint8_t cc_input = c[j][i];

            switch (cc_input) {
                case CC_0:
                    shader_input = SHADER_0;
                    break;
                case CC_TEXEL0:
                    shader_input = SHADER_TEXEL0;
                    break;
                case CC_TEXEL1:
                    shader_input = SHADER_TEXEL1;
                    break;
                case CC_TEXEL0A:
                    shader_input = SHADER_TEXEL0A;
                    break;
                case CC_PRIM:
                case CC_SHADE:
                case CC_ENV:
                case CC_LOD:
                    if (input_number[cc_input] == 0) {
                        out_shader_input_mappings[next_input_number - 1][i] = cc_input;
                        input_number[cc_input] = next_input_number++;
                    }
                    shader_input = input_number[cc_input];
                    break;
            }
            shader_id |= shader_input << (i * 12 + j * 3);
        }
    }

    *out_shader_id = shader_id;
}
