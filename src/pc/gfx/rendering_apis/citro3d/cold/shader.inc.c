#include <stdint.h>

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_defines.h"


struct ShaderProgram* c3d_cold_init_shader(struct ShaderProgram* prg, union ShaderProgramFeatureFlags shader_features)
{
    prg->shader_features.u32 = shader_features.u32;

    const struct n3ds_shader_info* shader_info = citro3d_helpers_get_shader_info_from_flags(prg->shader_features);

    citro3d_helpers_init_attr_info(&shader_info->vbo_info.attributes, &prg->attr_info);
    prg->vertex_buffer = c3d_cold_lookup_or_create_vertex_buffer(&shader_info->vbo_info, &prg->attr_info);
    
    shaderProgramInit(&prg->pica_shader_program);
    shaderProgramSetVsh(&prg->pica_shader_program, &shader_info->binary->dvlb->DVLE[shader_info->dvle_index]);
    shaderProgramSetGsh(&prg->pica_shader_program, NULL, 0);

    return prg;
}
