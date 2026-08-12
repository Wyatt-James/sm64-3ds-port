#include "gfx_cc.h"

#include <string.h>

#include "src/pc/pc_macros.h"

void gfx_cc_get_features(const CCShaderId shader_id, struct CCFeatures *cc_features)
{
    for (int i = 0; i < 4; i++) {
        cc_features->cc.arr[i].rgb   = (shader_id >> (i * 3)) & 7;
        cc_features->cc.arr[i].alpha = (shader_id >> (12 + i * 3)) & 7;
    }

    cc_features->used_textures_0 = false;
    cc_features->used_textures_1 = false;
    cc_features->num_inputs = 0;

    #pragma GCC unroll 0
    for (int i = 0; i < 2; i++) {
        #pragma GCC unroll 0
        for (int j = 0; j < 4; j++) {
            uint8_t cc_source = (i == 0) ? cc_features->cc.arr[j].rgb
                                         : cc_features->cc.arr[j].alpha;
            switch (cc_source) {
                case SHADER_INPUT_1:
                case SHADER_INPUT_2:
                case SHADER_INPUT_3:
                case SHADER_INPUT_4:
                    cc_features->num_inputs = MAX(cc_features->num_inputs, cc_source);
                    break;
                case SHADER_TEXEL0:
                case SHADER_TEXEL0A:
                    cc_features->used_textures_0 = true;
                    break;
                case SHADER_TEXEL1:
                    cc_features->used_textures_1 = true;
                    break;
            }
        }
    }

    cc_features->do_single_0   = cc_features->cc.c.rgb == 0;                               // only an additive component.
    cc_features->do_multiply_0 = cc_features->cc.b.rgb == 0 && cc_features->cc.d.rgb == 0; // no subtractive or additive components.
    cc_features->do_mix_0      = cc_features->cc.b.rgb == cc_features->cc.d.rgb;           // subtractive and additive components are equal.
    
    cc_features->do_single_1   = cc_features->cc.c.alpha == 0;                                 // only an additive component.
    cc_features->do_multiply_1 = cc_features->cc.b.alpha == 0 && cc_features->cc.d.alpha == 0; // no subtractive or additive components.
    cc_features->do_mix_1      = cc_features->cc.b.alpha == cc_features->cc.d.alpha;           // subtractive and additive components are equal.

    cc_features->opt_alpha        = (shader_id & SHADER_OPT_ALPHA)        != 0;
    cc_features->opt_fog          = (shader_id & SHADER_OPT_FOG)          != 0;
    cc_features->opt_texture_edge = (shader_id & SHADER_OPT_TEXTURE_EDGE) != 0;
    cc_features->opt_noise        = (shader_id & SHADER_OPT_NOISE)        != 0;
    cc_features->color_alpha_same = (shader_id & 0xfff) == ((shader_id >> 12) & 0xfff);
}

void gfx_cc_generate_cc(ColorCombinerId cc_id, union CCInputMapping* out_shader_input_mappings, CCShaderId* out_shader_id)
{
    CCShaderId shader_id = (cc_id >> 24) << 24;
    union CCInputMapping c = {0};

    bzero(out_shader_input_mappings, sizeof(*out_shader_input_mappings));

    for (int i = 0; i < 4; i++) {
        c.arr[i].rgb   = (cc_id >> (i * 3)) & 7;
        c.arr[i].alpha = (cc_id >> (12 + i * 3)) & 7;
    }

    #pragma GCC unroll 0
    for (int i = 0; i < 2; i++) {
        uint8_t input_number[8] = {0};
        int next_input_number = SHADER_INPUT_1;
        
        #pragma GCC unroll 0
        for (int j = 0; j < 4; j++) {
            int shader_input = 0;
            uint8_t cc_input = (i == 0) ? c.arr[j].rgb
                                        : c.arr[j].alpha;

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
                        if (i == 0) out_shader_input_mappings->arr[next_input_number - 1].rgb = cc_input;
                        else        out_shader_input_mappings->arr[next_input_number - 1].alpha = cc_input;
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
