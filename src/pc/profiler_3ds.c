#include "profiler_3ds.h"

// If the profiler is disabled, this translation unit is empty.
#if PROFILER_3DS_ENABLE == 1

#include <string.h>
#include <stdio.h>

#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/pc_macros.h"

// Convenience define. Represents where to place the buffer terminator.
#define PROFILER_3DS_LOG_STRING_TERMINATOR (PROFILER_3DS_LOG_STRING_LENGTH - 1)

#define STR_FREE_SPACE(buf_len_, cur_) ((buf_len_ - cur_) - 1)
#define STR_HAS_SPACE(buf_len_, cur_, size_) (STR_FREE_SPACE(buf_len_, cur_) >= size_)
#define CIRCULAR_ADJUST_FRAME(index_) ((circ_cur_frame + index_ + 1) % PROFILER_3DS_NUM_CIRCULAR_FRAMES)

typedef struct
{
    double time;    // Cumulative sum of all time spent within a given entry
    uint32_t count; // Only currently used in printing
} CircularEntry;

typedef struct
{
    double duration; // Total amount of time
    double average;  // Duration / num_frames
} CircularResult;

typedef struct
{
    uint8_t counter, interval; // When counter reaches 0, trigger a breakpoint and reset to interval
} SnoopCounter;

// Times are stored in milliseconds

static TickCounter tick_counter;

// A circular buffer of the most recent frames.
static volatile CircularEntry  circ_buffer[PROFILER_3DS_NUM_CIRCULAR_FRAMES][PROFILER_3DS_NUM_IDS];
static volatile CircularResult circ_results[PROFILER_3DS_NUM_IDS];
static volatile uint32_t       circ_num_frames = 0; // Number of frames of valid info currently in the buffer
static volatile uint32_t       circ_cur_frame = 0, circ_next_frame = 0; // Circular buffer indices

// Updated per-snoop-ID each profiler_3ds_snoop_impl() call; used for breakpoints.
static volatile SnoopCounter snoop_counters[PROFILER_3DS_NUM_TRACKED_SNOOP_IDS];

// For writing debug strings. Marked as USED to prevent optimize-out with LTO enabled.
static USED char log_string[PROFILER_3DS_LOG_STRING_LENGTH];

// libctru's osTickCounterUpdate measures time between updates. We want time since last reset.
static void update_tick_counters() {
    const __3ds_u64 system_tick = svcGetSystemTick();

    // For the circular average, we want to measure each duration
	tick_counter.elapsed = system_tick - tick_counter.reference;
	tick_counter.reference = system_tick;
}

static void update_circular_log(uint32_t id) {
    const double duration = osTickCounterRead(&tick_counter);

    // Update circular log
    circ_buffer[circ_cur_frame][id].time += duration;
    circ_buffer[circ_cur_frame][id].count++;
}

// --------------- Loggers and Calculators ---------------

// Logs a time with the given ID.
void profiler_3ds_log_time_impl(uint32_t id) {
    update_tick_counters();
    update_circular_log(id);
}

// Advances one frame in the circular log
void profiler_3ds_advance_frame_impl(void) {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        circ_buffer[circ_next_frame][id] = (CircularEntry) {};
    }

    circ_cur_frame = circ_next_frame;
    circ_next_frame = (circ_next_frame + 1) % PROFILER_3DS_NUM_CIRCULAR_FRAMES;
    circ_num_frames = MIN(circ_num_frames + 1, PROFILER_3DS_NUM_CIRCULAR_FRAMES);
    
    tick_counter.reference = svcGetSystemTick();
}

// Calculates the averages for the circular log
void profiler_3ds_calculate_results_impl(void) {
    if (circ_num_frames > 0) {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
            circ_results[id].duration = 0.0;

            for (uint32_t frame = 0; frame < circ_num_frames; frame++)
                circ_results[id].duration += circ_buffer[frame][id].time;

            circ_results[id].average = circ_results[id].duration / circ_num_frames;
        }
    } else {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++)
            circ_results[id].average = circ_results[id].duration = 0.0;
    }
}

// --------------- Resets and Initializers ---------------

// Resets the circular log.
void profiler_3ds_reset_impl(void) {
    // We don't need to zero the circular data beacuse we reset the indices.
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++)
        circ_results[id] = (CircularResult) {};

    circ_cur_frame = circ_next_frame = 0;
    circ_num_frames = 0;
    profiler_3ds_advance_frame_impl();
}

// Initializes the snoop counters and resets all logs.
void profiler_3ds_init_impl(void) {
    for (uint32_t id = 0; id < ARRAY_COUNT(snoop_counters); id++)
        snoop_counters[id] = (SnoopCounter) {PROFILER_3DS_DEFAULT_SNOOP_INTERVAL, PROFILER_3DS_DEFAULT_SNOOP_INTERVAL};

    profiler_3ds_reset_impl();

    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
}

// --------------- Getters, Inspectors, and Snoopers ---------------

// Returns the duration for a given frame and ID from the circular log.
double profiler_3ds_get_frame_duration_impl(uint32_t frame, uint32_t id) {
    if (frame < circ_num_frames)
        return circ_buffer[frame][id].time;
    
    return -1.0;
}

// Returns the total duration for an ID from the circular log. Must be calculated first.
double profiler_3ds_get_total_duration_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return circ_results[id].duration;
    
    return -1.0;
}

// Returns the average time for an ID from the circular log. Must be calculated first.
double profiler_3ds_get_average_time_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return circ_results[id].average;
    
    return -1.0;
}

// Sets the interval for a snoop counter and resets its counter to the interval.
void profiler_3ds_set_snoop_counter_impl(uint32_t snoop_id, uint8_t interval) {
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS)
    {
        volatile SnoopCounter* sc = &snoop_counters[snoop_id];
        sc->counter = sc->interval = interval;
    }
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
    PRINT_NONE,
    PRINT_TIME,
    PRINT_COUNT
};

int profiler_3ds_create_log_string_internal(uint32_t min_id_to_print, uint32_t max_id_to_print, enum PrintMode print_mode) {
    log_string[0] = '\0';
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';

    if (print_mode == PRINT_NONE)
        return 0;

    min_id_to_print = CLAMP(min_id_to_print, 0, PROFILER_3DS_NUM_IDS - 1);
    max_id_to_print = CLAMP(max_id_to_print, 0, PROFILER_3DS_NUM_IDS - 1);

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
                case PRINT_TIME:
                    worker_len = snprintf(worker, WORKER_BUF_LEN, "%lf", frame[id].time);
                    break;
                case PRINT_COUNT:
                    worker_len = snprintf(worker, WORKER_BUF_LEN, "%lu", frame[id].count);
                    break;
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

int profiler_3ds_create_log_string_impl(uint32_t min_id_to_print, uint32_t max_id_to_print)
{
    profiler_3ds_create_log_string_internal(min_id_to_print, max_id_to_print, PRINT_TIME);
}

USED volatile enum PrintMode snoop_print_mode = PRINT_TIME;
USED volatile int breakpoint = 0;

// Computes some useful information for the timestamps. Intended for debugger use.
void profiler_3ds_snoop_impl(UNUSED uint32_t snoop_id) {

    // Useful GDB prints (faster in single-threaded mode; see n3ds_config.c):
    // printf "%s\n", log_string

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

    volatile SnoopCounter* sc = &snoop_counters[snoop_id];

    // Use to break after some number of iterations
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS) {
        if (--sc->counter == 0) {
            sc->counter = sc->interval;

            switch(snoop_id) { 
                case 0: {
                    profiler_3ds_calculate_results_impl();
                    snoop_print_mode = PRINT_TIME;

                    while (snoop_print_mode != PRINT_NONE) {
                        UNUSED volatile int log_len = profiler_3ds_create_log_string_internal(0, 5 /* + 20*/, snoop_print_mode);
                        snoop_print_mode = PRINT_NONE;

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
