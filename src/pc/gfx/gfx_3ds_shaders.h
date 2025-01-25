#ifndef GFX_3DS_SHADERS_H
#define GFX_3DS_SHADERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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
#include <3ds/types.h> // shbin.h fails to include this
#include <3ds/gpu/shbin.h>
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
