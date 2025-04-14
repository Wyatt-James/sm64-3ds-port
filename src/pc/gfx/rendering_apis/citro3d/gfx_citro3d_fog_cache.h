#ifndef GFX_CITRO3D_FOG_CACHE_H
#define GFX_CITRO3D_FOG_CACHE_H

/*
 * A cache ADT for fog LUTs.
 */

#include <stdint.h>

#include "src/pc/n3ds/c3d_inc.h"

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

#endif
