#include <stddef.h>

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_defines.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_helpers.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"
#include "src/pc/gfx/gfx_cc.h"
#include "src/pc/gfx/color_formats.h"

void c3d_cold_init_color_combiner(struct ColorCombiner* cc, ColorCombinerId cc_id)
{
    union CCInputMapping mapping;

    {
        CCShaderId shader_id;
        gfx_cc_generate_cc(cc_id, &mapping, &shader_id);
        gfx_cc_get_features(shader_id, &cc->cc_features);
    }

    // If num inputs >= 2, we need to reverse the mappings' A and B params (hack for goddard)
    // WYATT_TODO put something better here
    if (cc->cc_features.num_inputs >= 2) {
        union CCInputMapping mapping_temp;
        for (int i = 0; i <= 1; i++) {
            mapping_temp.arr[i][0] = mapping.arr[i][1];
            mapping_temp.arr[i][1] = mapping.arr[i][0];

            mapping.arr[i][0] = mapping_temp.arr[i][0];
            mapping.arr[i][1] = mapping_temp.arr[i][1];
        }
    }

    cc->cc_id = cc_id;

    cc->c3d_shader_input_mapping.c1_rgb = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.rgb[0], false);
    cc->c3d_shader_input_mapping.c2_rgb = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.rgb[1], false);

    cc->c3d_shader_input_mapping.c1_a = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.alpha[0], cc->cc_features.opt_fog);
    cc->c3d_shader_input_mapping.c2_a = citro3d_helpers_convert_cc_mapping_to_emu64_float(mapping.alpha[1], cc->cc_features.opt_fog);

    // WYATT_TODO this is probably incorrect, but it works for now. Fixes the pause tint being too light.
    cc->use_env_color = mapping.rgb[1] == CC_ENV;

    // N3DS only cares about the first two mappings, so we want to make an identifier for specifically this to enhance performance
    // RGBA32 works fine since it's four u8s
    cc->cc_mapping_identifier = (union RGBA32) {
        .r = mapping.rgb[0],
        .g = mapping.rgb[1],
        .b = mapping.alpha[0],
        .a = mapping.alpha[1],
    }.u32;

    // Preconfigure TEV settings
    citro3d_helpers_configure_tex_env_slot_0(&cc->cc_features, &cc->texenv_slot_0);
}
