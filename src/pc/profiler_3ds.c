#include "profiler_3ds.h"

// If the profiler is disabled, functions and their headers do not exist.
#if PROFILER_3DS_ENABLE == 1

#include <string.h>
#include <stdio.h>

// We want to use the 3DS version of osGetTime
#undef osGetTime
#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/pc_macros.h"

#define TIMESTAMP_SNOOP_INTERVAL 8
#define TIMESTAMP_ARRAY_COUNT(arr) (int)(sizeof(arr) / sizeof(arr[0]))
#define STR_FREE_SPACE(buf_len, cur) ((buf_len - cur) - 1)
#define STR_HAS_SPACE(buf_len, cur, size) (STR_FREE_SPACE(buf_len, cur) >= size)
#define CIRCULAR_ADJUST_FRAME(index) ((circ_cur_frame + i + 1) % PROFILER_3DS_NUM_CIRCULAR_FRAMES)

typedef struct
{
    double t;
    uint32_t c;
} CircularEntry;

// Times are stored in milliseconds

static TickCounter tick_counter_average, tick_counter_linear, tick_counter_circular;

// A long-term average over time.
static volatile double   long_durations_per_id[PROFILER_3DS_NUM_IDS];
static volatile double   long_averages_per_id[PROFILER_3DS_NUM_IDS];
static volatile uint32_t long_counts_per_id[PROFILER_3DS_NUM_IDS];

// A linear log of each time recorded.
// Once the cap is reached, it will not be updated.
static volatile double   lin_all_times[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH + 1]; // First index is the start time
static volatile double*  lin_elapsed_times = lin_all_times + 1; // Time since startTime for each stamp
static volatile double   lin_durations[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // Time since the previous stamp for each stamp
static volatile uint32_t lin_ids[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // ID for each timestamp in elapsed_times
static volatile double   lin_averages_per_id[PROFILER_3DS_NUM_IDS]; // Average duration per-id for the linear log
static volatile double   lin_totals_per_id[PROFILER_3DS_NUM_IDS]; // Total duration per-id for the linear log
static volatile uint32_t lin_counts_per_id[PROFILER_3DS_NUM_IDS]; // Count per-id for the linear log
static volatile uint32_t lin_timestamp_count = 0;

// A circular buffer of the most recent frames.
static volatile CircularEntry circ_buffer[PROFILER_3DS_NUM_CIRCULAR_FRAMES][PROFILER_3DS_NUM_IDS]; // Circular buffer of durations
static volatile double        circ_durations_per_id[PROFILER_3DS_NUM_IDS]; // Circular buffer of durations
static volatile double        circ_averages_per_id[PROFILER_3DS_NUM_IDS]; // Average of contents of totals_per_id for each id, computed on-demand
static volatile uint32_t      circ_counts_per_id[PROFILER_3DS_NUM_IDS]; // Count per-ID
static volatile uint32_t      circ_num_frames = 1; // Number of frames encountered in the circular buffer.
static volatile uint32_t      circ_cur_frame = 0, circ_next_frame = 0; // Circular buffer indices

// Updated per-snoop-ID each profiler_3ds_snoop_impl() call; used for breakpoints.
static volatile uint8_t snoop_interval = 180;
static volatile uint8_t snoop_counters[PROFILER_3DS_NUM_TRACKED_SNOOP_IDS];

static USED char log_string[PROFILER_3DS_LOG_STRING_LENGTH];

// libctru's osTickCounterUpdate measures time between updates. We want time since last reset.
static inline void update_tick_counters() {
    const __3ds_u64 system_tick = svcGetSystemTick();

    // For the long average, we want to measure each duration
	tick_counter_average.elapsed = system_tick - tick_counter_average.reference;
	tick_counter_average.reference = system_tick;

    // For the linear log, we want to measure time since the reference
    tick_counter_linear.elapsed = system_tick - tick_counter_linear.reference;

    // For the circular average, we want to measure each duration
	tick_counter_circular.elapsed = system_tick - tick_counter_circular.reference;
	tick_counter_circular.reference = system_tick;
}

static void update_average_log(uint32_t id) {
    const double duration = osTickCounterRead(&tick_counter_average);
    long_counts_per_id[id]++;
    long_durations_per_id[id] += duration;
}

// Update linear log if there is space
static void update_linear_log(uint32_t id) {
    if (lin_timestamp_count < PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH && id < PROFILER_3DS_NUM_IDS) {
        const double curTime = osTickCounterRead(&tick_counter_linear);
        const double lastTime = lin_elapsed_times[lin_timestamp_count - 1];
        const double duration = curTime - lastTime;

        lin_elapsed_times[lin_timestamp_count] = curTime;
        lin_durations[lin_timestamp_count] = duration;
        lin_ids[lin_timestamp_count] = id;
        lin_totals_per_id[id] += duration;
        
        lin_timestamp_count++;
        lin_counts_per_id[id]++;
    }
}

static void update_circular_log(uint32_t id) {
    const double duration = osTickCounterRead(&tick_counter_circular);

    // Update circular log
    circ_buffer[circ_cur_frame][id].t += duration;
    circ_buffer[circ_cur_frame][id].c++;
}


// --------------- Loggers and Calculators ---------------

// Logs a time with the given ID.
void profiler_3ds_log_time_impl(uint32_t id) {
    update_tick_counters();
    update_average_log(id);
    update_linear_log(id);
    update_circular_log(id);
}

void profiler_3ds_average_calculate_average_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        const uint32_t count = long_counts_per_id[id];

        if (count > 0)
            long_averages_per_id[id] = long_durations_per_id[id] / count;
        else
            long_averages_per_id[id] = 0.0;
    }
}

// Calculates the averages over the linear log's history.
void profiler_3ds_linear_calculate_averages_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        const uint32_t count = lin_counts_per_id[id];

        if (count > 0)
            lin_averages_per_id[id] = lin_totals_per_id[id] / count;
        else
            lin_averages_per_id[id] = 0.0;
    }
}

// Advances one frame in the circular log
void profiler_3ds_circular_advance_frame_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        circ_buffer[circ_next_frame][id] = (CircularEntry) {};
    }

    circ_cur_frame = circ_next_frame;

    if (circ_next_frame == PROFILER_3DS_NUM_CIRCULAR_FRAMES - 1)
        circ_next_frame = 0;
    else
        circ_next_frame++;

    if (circ_num_frames < PROFILER_3DS_NUM_CIRCULAR_FRAMES)
        circ_num_frames++;
    
    tick_counter_circular.reference = svcGetSystemTick();
}

// Calculates the averages for the circular log
void profiler_3ds_circular_calculate_averages_impl() {
    if (circ_num_frames > 0) {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
            circ_durations_per_id[id] = 0.0;

            for (uint32_t frame = 0; frame < circ_num_frames; frame++)
                circ_durations_per_id[id] += circ_buffer[frame][id].t;

            circ_averages_per_id[id] = circ_durations_per_id[id] / circ_num_frames;
        }
    } else {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++)
            circ_averages_per_id[id] = 0.0;
    }
}


// --------------- Resets and Initializers ---------------


// Resets the long-term average log.
void profiler_3ds_average_reset_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        long_durations_per_id[id] = 0.0;
        long_averages_per_id[id] = 0.0;
        long_counts_per_id[id] = 0;
    }

    osTickCounterStart(&tick_counter_average);
}

// Resets the linear log.
void profiler_3ds_linear_reset_impl() {
    lin_all_times[0] = 0.0;
    lin_timestamp_count = 0;

    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        lin_totals_per_id[id] = 0.0;
        lin_counts_per_id[id] = 0;
    }

    osTickCounterStart(&tick_counter_linear);
}

// Resets the circular log.
void profiler_3ds_circular_reset_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        for (uint32_t frame = 0; frame < PROFILER_3DS_NUM_CIRCULAR_FRAMES; frame++)
            circ_buffer[frame][id].t = 0.0;

        circ_averages_per_id[id] = 0.0;
    }

    circ_cur_frame = circ_next_frame = 0;
    circ_num_frames = 1;
    profiler_3ds_circular_advance_frame_impl();
}

// Initializes the snoop counters and resets all logs.
void profiler_3ds_init_impl() {
    for (uint32_t i = 0; i < TIMESTAMP_ARRAY_COUNT(snoop_counters); i++)
        snoop_counters[i] = snoop_interval;

    profiler_3ds_average_reset_impl();
    profiler_3ds_linear_reset_impl();
    profiler_3ds_circular_reset_impl();

    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
}

// --------------- Getters, Inspectors, and Snoopers ---------------

// Returns the average time from the long-term average log.
double profiler_3ds_average_get_average_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return long_averages_per_id[id];

    return -1.0;
}

// Returns the total elapsed time of the linear log in milliseconds.
double profiler_3ds_linear_get_elapsed_time_impl() {
    return lin_elapsed_times[lin_timestamp_count - 1];
}

// Returns the average time for the given ID from the linear log in milliseconds. Must be calculated first.
double profiler_3ds_linear_get_average_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return lin_averages_per_id[id];
    
    return -1.0;
}

// Returns the duration for a given frame and ID from the circular log.
double profiler_3ds_circular_get_duration_impl(uint32_t frame, uint32_t id) {
    if (frame < circ_cur_frame - 1 && id < PROFILER_3DS_NUM_IDS)
        return circ_buffer[frame][id].t;
    
    return -1.0;
}

// Returns the average time for an ID from the circular log. Must be calculated first.
double profiler_3ds_circular_get_average_time_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return circ_averages_per_id[id];
    
    return -1.0;
}

// Sets the interval for a snoop counter.
void profiler_3ds_set_snoop_counter_impl(uint32_t snoop_id, uint8_t frames_until_snoop) {
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS)
        snoop_counters[snoop_id] = frames_until_snoop;
}

// Creates a string containing the circular log's data, stored in log_string.
// Returns the size of the log string. It will be positive if it fit within the buffer,
// or negative if it did not fit. If the string does not fit, it will write as much
// as possible.
#define LOG_BUF_SIZE PROFILER_3DS_LOG_STRING_LENGTH
#define WORKER_BUF_LEN 31 // 30 chars + terminator
#define FRAME_SEPARATOR "},\n"
#define VALUE_SEPARATOR ", "
#define FRAME_OPEN "{"
#define FRAME_CLOSE "}"
enum PrintMode
{
    NONE,
    TIME,
    COUNT
};

int profiler_3ds_create_log_string_circular_internal(uint32_t min_id_to_print, uint32_t max_id_to_print, enum PrintMode print_mode) {
    log_string[0] = '\0';
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';

    if (print_mode == NONE)
        return 0;
        
    if (min_id_to_print > PROFILER_3DS_NUM_IDS)
        min_id_to_print = PROFILER_3DS_NUM_IDS;

    if (max_id_to_print > PROFILER_3DS_NUM_IDS)
        max_id_to_print = PROFILER_3DS_NUM_IDS;

    if (min_id_to_print > max_id_to_print)
        return 0;

    size_t log_len = 0;
    char worker[WORKER_BUF_LEN];
    worker[WORKER_BUF_LEN - 1] = '\0';

    static const size_t frame_sep_len = strlen(FRAME_SEPARATOR);
    static const size_t value_sep_len = strlen(VALUE_SEPARATOR);
    static const size_t frame_open_len = strlen(FRAME_OPEN);
    static const size_t frame_close_len = strlen(FRAME_CLOSE);

    // for each frame...
    for (uint32_t i = 0; i < circ_num_frames; i++) {
        int frame_num = CIRCULAR_ADJUST_FRAME(i);
        volatile CircularEntry *frame = circ_buffer[frame_num];

        if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, 1)) goto too_long;
        strcpy(&log_string[log_len], FRAME_OPEN);
        log_len += frame_open_len;

        // print each ID, separated by a comma
        for (uint32_t id = min_id_to_print; id <= max_id_to_print; id++) {
            int worker_len;
            switch (print_mode) {
                case TIME:
                    worker_len = snprintf(worker, WORKER_BUF_LEN, "%lf", frame[id].t);
                    break;
                case COUNT:
                    worker_len = snprintf(worker, WORKER_BUF_LEN, "%lu", frame[id].c);
                default:
                    break;
            }

            if (worker_len >= WORKER_BUF_LEN) {
                // Output was truncated
            } else {
                // Output was not truncated
            }

            // Append value
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, (size_t) worker_len)) goto too_long;
            strcpy(&log_string[log_len], worker);
            log_len += worker_len;

            // Append value separator
            if (id < max_id_to_print) {
                if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, value_sep_len)) goto too_long;
                strcpy(&log_string[log_len], VALUE_SEPARATOR);
                log_len += value_sep_len;
            }
        }

        if (i < circ_num_frames - 1) {
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, frame_sep_len)) goto too_long;
            strcpy(&log_string[log_len], FRAME_SEPARATOR);
            log_len += frame_sep_len;
        } else {
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, 1)) goto too_long;
            strcpy(&log_string[log_len], FRAME_CLOSE);
            log_len += frame_close_len;
        }
    }
    
    // Terminate the end of our buffer for safety
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
    return log_len;

    too_long:
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
    return -1 * log_len;
}
#undef LOG_BUF_SIZE
#undef WORKER_BUF_LEN
#undef FRAME_SEPARATOR
#undef VALUE_SEPARATOR
#undef FRAME_OPEN
#undef FRAME_CLOSE

int profiler_3ds_create_log_string_circular_impl(uint32_t min_id_to_print, uint32_t max_id_to_print)
{
    profiler_3ds_create_log_string_circular_internal(min_id_to_print, max_id_to_print, TIME);
}

USED volatile enum PrintMode snoop_print_mode = TIME;
USED volatile int breakpoint = 0;

// Computes some useful information for the timestamps. Intended for debugger use.
void profiler_3ds_snoop_impl(UNUSED uint32_t snoop_id) {

    // Useful GDB prints:
    // p/f *lin_totals_per_id@8
    // p/d *lin_counts_per_id@8
    // p/f *long_averages_per_id@8
    // p/f *circ_averages_per_id@8
    // p/f *circ_durations_per_id@8
    // printf "%s\n", log_string        // This can be slow. I get 1400 chars/min. Faster in single-core.

    // IDs:
    // 0: Misc
    // 1: Run Level Script
    // 2: Synchronous Audio Synthesis
    // 3: Build Display List
    // 4: GFX Rendering API Start Frame (VSync)
    // 5: GFX Run Display List

    // 60fps IDs:
    
    // 6: GFX Rendering API Start Frame Interpolated (VSync)
    // 7: GFX Run Display List Interpolated

    // Detailed IDs (replaces GFX Run Display List):
    // 5: Vertex Copy
    // 6: gfx_sp_tri_update_state
    // 7: gfx_tri_create_vbo
    // 8: gfx_flush

    // C3D IDs
    // 
    // 6:  C3D_LogSlot_FrameBuf
    // 7:  C3D_LogSlot_Viewport
    // 8:  C3D_LogSlot_Scissor
    // 9:  C3D_LogSlot_Program
    // 10: C3D_LogSlot_AttrInfo
    // 11: C3D_LogSlot_BufInfo
    // 12: C3D_LogSlot_Effect
    // 13: C3D_LogSlot_TexAll
    // 14: C3D_LogSlot_TexStatus
    // 15: C3D_LogSlot_ProcTex
    // 16: C3D_LogSlot_TexEnvBuf
    // 17: C3D_LogSlot_FogLut
    // 18: C3D_LogSlot_Gas
    // 19: C3D_LogSlot_TexEnvAll
    // 20: C3D_LogSlot_LightEnv
    // 21: C3D_LogSlot_FixedAttribDirty
    // 22: C3D_LogSlot_UpdateUniforms
    // 23: C3D_LogSlot_ImmediateDraw
    // 24: C3D_LogSlot_DrawArrays
    // 25: C3D_LogSlot_DrawElements

    // Use with conditional breakpoints in GDB
    breakpoint++;
    breakpoint++;
    breakpoint++;
    breakpoint++;
    breakpoint++;

    // Use to break after some number of iterations
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS) {
        if (--snoop_counters[snoop_id] == 0) {
            snoop_counters[snoop_id] = snoop_interval;

            switch(snoop_id) { 
                case 0: {
                    profiler_3ds_average_calculate_average_impl();
                    profiler_3ds_linear_calculate_averages_impl();
                    profiler_3ds_circular_calculate_averages_impl();
                    snoop_print_mode = TIME;

                    while (snoop_print_mode != NONE) {
                        UNUSED volatile int log_len = profiler_3ds_create_log_string_circular_internal(0, 5 /* + 20*/, snoop_print_mode);
                        snoop_print_mode = NONE;

                        breakpoint += 5; // Place a breakpoint here
                    }
                    
                    breakpoint += 5; // Place a breakpoint here
                    break;
                }
            }
        }
    }

    // IDs beyond the limit are still valid, but untracked
    else
        breakpoint++;

    return; // Leave this here for breakpoints
}

#endif // #if PROFILER_3DS_ENABLE == 1
