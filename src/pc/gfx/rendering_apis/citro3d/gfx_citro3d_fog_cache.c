#include "gfx_citro3d_fog_cache.h"

#include <strings.h>
#include <stdio.h>

#include "macros.h"

void fog_cache_init(struct FogCache* cache)
{
    cache->current = NULL;
    cache->count = 0;
    cache->next = 0;

    fog_cache_load(cache, 0, 0);
}

enum FogCacheResult fog_cache_load(struct FogCache* cache, uint16_t from, uint16_t to)
{
    uint32_t id = (from << 16) | to;

    // Current already loaded
    if (cache->current != NULL && cache->current->id == id)
        return FOGCACHE_HIT;

    // Load pre-calculated LUT
    for (uint8_t i = 0; i < cache->count; i++)
    {
        if (cache->arr[i].id == id)
        {
            cache->current = &cache->arr[i];
            return FOGCACHE_HIT;
        }
    }

    // new lut required
    cache->count++;
    cache->current = &cache->arr[cache->next];
    cache->next = (cache->next + 1) % MAX_FOG_LUTS;
    cache->current->id = id;
    return FOGCACHE_MISS;
}


C3D_FogLut* fog_cache_current(struct FogCache* cache)
{
    return &cache->current->c3d_lut;
}
