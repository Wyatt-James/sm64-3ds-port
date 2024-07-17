#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include <stdbool.h>

struct n3ds_shader_vbo_info {
   bool has_position,
        has_texture,
        has_fog,
        has_color1,
        has_color2;
   uint8_t stride;
}; 

struct n3ds_shader_binary {
   const uint8_t* data;
   uint32_t size;
   DVLB_s* dvlb;
   size_t uniform_locations_size; // sizeof(*uniform_locations)
   void* uniform_locations;
};

struct n3ds_shader_info {
   struct n3ds_shader_binary* binary;
   uint32_t dvle_index;
   const uint32_t identifier;
   struct n3ds_shader_vbo_info vbo_info;
};

extern const struct n3ds_shader_info* const shaders[];

#endif
