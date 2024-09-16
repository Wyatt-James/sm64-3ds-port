#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Held in an array.
struct n3ds_emu64_vertex_attribute {
   GPU_FORMATS format;
   uint8_t count;
};

// An array of vertex attributes
struct n3ds_attribute_data {
   const struct n3ds_emu64_vertex_attribute* data;
   size_t num_attribs;
};

struct n3ds_shader_vbo_info {
   bool has_position,
        has_texture,
        has_color,
        has_normals;
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

extern const struct n3ds_shader_info* const shaders[];

#endif
