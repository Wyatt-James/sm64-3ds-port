#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "src/pc/n3ds/libctru_inc.h"

// Held in an array.
struct n3ds_emu64_vertex_attribute {
   uint8_t format; // GPU_FORMATS
   uint8_t count;
};

// An array of vertex attributes
struct n3ds_attribute_data {
   const struct n3ds_emu64_vertex_attribute* data;
   uint8_t num_attribs;
};

struct n3ds_shader_vbo_info {
   bool has_position : 1,
        has_texture  : 1,
        has_color    : 1,
        has_normals  : 1;
   uint8_t stride;
   struct n3ds_attribute_data attributes;
};

struct n3ds_shader_binary {
   const uint8_t* data;
   uint32_t size;
   DVLB_s* dvlb;
};

struct n3ds_shader_info {
   struct n3ds_shader_binary* binary;
   uint32_t dvle_index;
   const uint32_t identifier;
   struct n3ds_shader_vbo_info vbo_info;
};

#endif
