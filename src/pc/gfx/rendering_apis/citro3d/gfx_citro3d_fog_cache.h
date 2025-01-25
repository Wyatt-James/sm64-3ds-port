#pragma once

#include <stdint.h>

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
#include <citro3d.h>
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

#define MAX_FOG_LUTS 32

struct FogCacheHandle {
    uint32_t id;
    C3D_FogLut c3d_lut;
};

struct FogCache {
    struct FogCacheHandle arr[MAX_FOG_LUTS];
    struct FogCacheHandle* current;
    uint8_t next, count;
};

enum FogCacheResult {
    FOGCACHE_CURRENT,
    FOGCACHE_HIT,
    FOGCACHE_MISS
};

// Initializes this ADT.
void fog_cache_init(struct FogCache* cache);

// Loads a LUT into the "current" slot with the given ID.
// Old LUTs are replaced in a round-robin fashion.
// The LUT is NOT modified! You still need to initialize it yourself.
// The returned value dictates how the LUT was found.
enum FogCacheResult fog_cache_load(struct FogCache* cache, uint16_t from, uint16_t to);

// Returns the current fog LUT.
C3D_FogLut* fog_cache_current(struct FogCache* cache);
