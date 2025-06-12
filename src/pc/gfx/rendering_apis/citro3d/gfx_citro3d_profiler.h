#pragma once

/*
 * C3D-profiler-relevant functions.
 */

static inline void gfx_citro3d_init_profiler(void)
{
#ifdef PROFILER_3DS_ENABLED
    C3D_ProfilerFunc(profiler_3ds_log_time_impl);

    uint32_t profiler_id = 6; // Next available profiler slot

    C3D_ProfilerCategoryMapAll(profiler_id - 1); // We'll overwrite default after
    C3D_ProfilerCategoryEnableAll(true);
    C3D_ProfilerCategoryMap(C3D_ProfilerSlot_Misc, 0);
#endif
}
