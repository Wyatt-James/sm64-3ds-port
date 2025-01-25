#include <stdint.h>
#include <stddef.h>
#include <string.h>

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <c3d/attribs.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_defines.h"
#include "src/pc/gfx/rendering_apis/citro3d/gfx_citro3d_types.h"

static struct VertexBuffer vertex_buffers[MAX_VERTEX_BUFFERS];
static uint8_t num_vertex_buffers = 0;

struct VertexBuffer* internal_citro3d_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo* attr_info)
{
    if (num_vertex_buffers == MAX_VERTEX_BUFFERS) {
        printf("Error: too many vertex buffers! (%d)\n", num_vertex_buffers + 1);
        return &vertex_buffers[0];
    }

    struct VertexBuffer* vb = &vertex_buffers[num_vertex_buffers++];
    vb->vbo_info = vbo_info;

    // Create the vertex buffer
    vb->ptr = linearAlloc(VERTEX_BUFFER_NUM_BYTES);
    vb->num_verts = 0;

    // Configure buffers
    BufInfo_Init(&vb->buf_info);
    BufInfo_Add(&vb->buf_info, vb->ptr, vbo_info->stride * VERTEX_BUFFER_UNIT_SIZE, attr_info->attrCount, attr_info->permutation);
    vb->attr_info = attr_info;

    return vb;
}

struct VertexBuffer* internal_citro3d_lookup_vertex_buffer(C3D_AttrInfo* attr_info)
{
    // Avoid duplicates
    for (size_t i = 0; i < num_vertex_buffers; i++)
    {
        struct VertexBuffer* vb = &vertex_buffers[i];
        if (memcmp(attr_info, vb->attr_info, sizeof(*attr_info)) == 0)
            return vb;
    }

    return NULL;
}

struct VertexBuffer* c3d_cold_lookup_or_create_vertex_buffer(const struct n3ds_shader_vbo_info* vbo_info, C3D_AttrInfo* attr_info)
{
    struct VertexBuffer* vb = internal_citro3d_lookup_vertex_buffer(attr_info);
    
    if (vb == NULL)
        vb = internal_citro3d_create_vertex_buffer(vbo_info, attr_info);

    // not found, create new
    return vb;
}

void c3d_cold_reset_vertex_buffers()
{
    for (int i = 0; i < num_vertex_buffers; i++)
        vertex_buffers[i].num_verts = 0;
}
