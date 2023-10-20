#include "profiler_3ds.h"
#include <types.h>

// We want to use the 3DS version of this function
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

#undef osGetTime
#include <3ds/os.h>

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

// Avoid compiler warnings for unused variables
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define TIMESTAMP_SNOOP_INTERVAL 8
#define TIMESTAMP_ARRAY_COUNT(arr) (__3ds_s32)(sizeof(arr) / sizeof(arr[0]))

// Times are stored in milliseconds

static TickCounter tick_counter;

static volatile double all_times[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH + 1]; // Padded by 1

static volatile double* elapsed_times = all_times + 1; // each is time - startTime
static volatile double durations[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // each is time - prev
static volatile double totals_per_id[PROFILER_3DS_TIME_LOG_MAX_ID]; // Cumulative addition of durations per-id
static volatile unsigned int ids[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // ID for each timestamp in elapsed_times

static volatile __3ds_u32 stamp_count = 0;

static volatile __3ds_u8 snoop_interval = 8;
static volatile __3ds_u8 snoop_counters[PROFILER_3DS_NUM_TRACKED_SNOOP_IDS];

// libctru's osTickCounterUpdate measures time between updates. We want time since last reset.
static void tick_counter_update() {
    tick_counter.elapsed = svcGetSystemTick() - tick_counter.reference;
}

// Logs a time with the given ID.
void profiler_3ds_log_time_impl(const unsigned int id) {
    tick_counter_update();
    volatile double curTime = osTickCounterRead(&tick_counter);
    volatile double lastTime = elapsed_times[stamp_count - 1];
    volatile double duration = curTime - lastTime;

    if (stamp_count < PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH && id <= PROFILER_3DS_TIME_LOG_MAX_ID) {
        elapsed_times[stamp_count] = curTime;
        durations[stamp_count] = duration;
        totals_per_id[id] += duration;
        ids[stamp_count] = id;

        stamp_count++;
    }
}

// Logs the start time, from which deltas and relative times are computed.
void profiler_3ds_log_start_time_impl() {
    osTickCounterStart(&tick_counter);
}

// Computes some useful information for the timestamps. Intended for debugger use.
void profiler_3ds_snoop_impl(UNUSED volatile unsigned int snoop_id) {

    // Useful GDB prints:
    // p/f *totals_per_id@11

    // IDs:
    // 0:  mixed
    // 1:  ADPCM Copy
    // 2:  ADPCM Decode
    // 3:  Non-resample note end
    // 4:  High-pitch part 1 end
    // 5:  High-pitch part 2 end
    // 6:  Final Resample
    // 7:  Envelope
    // 8:  Headset Pan
    // 9:  Interleave
    // 10: Output Copy
    // 11: Envelope Stereo Buffer Stuff
    // 12: Envelope volume settings
    // 13: EnvMixer Reverb
    // 14: Mix Reverb
    // 15: EnvMixer Non-Reverb
    // 16: Mix Non-Reverb
    // 17: 3DS export to DSP

    // Use with conditional breakpoints in GDB
    UNUSED volatile int i = 0;
    i++;
    i++;
    i++;
    i++;
    i++;

    // Use to break after some number of iterations
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS) {
        if (--snoop_counters[snoop_id] == 0)
            snoop_counters[snoop_id] = snoop_interval;
    }

    // IDs beyond the limit are still valid, but untracked
    else
        i++;

    return; // Leave this here for breakpoints
}

// Returns the total elapsed time in milliseconds
double profiler_3ds_elapsed_time_impl()
{
    return elapsed_times[stamp_count - 1];
}

// Resets all parameters and logs the start time.
void profiler_3ds_reset_impl() {
    tick_counter.elapsed = 0;
    tick_counter.reference = 0;
    all_times[0] = 0.0;
    stamp_count = 0;

    for (int i = 0; i < TIMESTAMP_ARRAY_COUNT(totals_per_id); i++)
        totals_per_id[i] = 0.0;

    profiler_3ds_log_start_time_impl();
}

void profiler_3ds_init() {
    bzero((void*) snoop_counters, sizeof(snoop_counters));
    profiler_3ds_reset_impl();
}