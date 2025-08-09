#pragma once

/*
 * An N3DS-specific profiler
 */

#include <stdint.h>

#define PROFILER_3DS_ENABLE 0


#if PROFILER_3DS_ENABLE == 1

#define PROFILER_3DS_ENABLED

// Maximum ID number for a timestamp.
#define PROFILER_3DS_NUM_IDS 32

// Number of frames to average in the circular buffer.
#define PROFILER_3DS_NUM_CIRCULAR_FRAMES 90

// Maximum number of timestamps that can be stored without a reset.
// Additional timestamps will be dropped.
#define PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH 1024

// Maximum number of snoop IDs to track. If a given ID exceeds this
// value, it will trigger every snoop.
#define PROFILER_3DS_NUM_TRACKED_SNOOP_IDS 64

// Default interval for snoops to trigger
#define PROFILER_3DS_DEFAULT_SNOOP_INTERVAL 180

// Length of the log string.
// Estimate 10 * <max id> * num circular frames, plus some extra for formatting.
#define PROFILER_3DS_LOG_STRING_LENGTH 25000

#else

// Profiler is disabled
#define PROFILER_3DS_NUM_IDS 0
#define PROFILER_3DS_NUM_CIRCULAR_FRAMES 0
#define PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH 0
#define PROFILER_3DS_NUM_TRACKED_SNOOP_IDS 0
#define PROFILER_3DS_DEFAULT_SNOOP_INTERVAL 0
#define PROFILER_3DS_LOG_STRING_LENGTH 0

#endif

// Function definitions and #define relays to them.
#if PROFILER_3DS_ENABLE == 1

// Loggers and Calculators
void profiler_3ds_log_time_impl(uint32_t id);   // Logs a time with the given ID.
void profiler_3ds_advance_frame_impl(void);     // Advances one frame.
void profiler_3ds_calculate_results_impl(void); // Calculates the results. Required to access some data.

// Resets and Initializers
void profiler_3ds_reset_impl(void); // Resets the profiler.
void profiler_3ds_init_impl(void);  // Initializes the snoop counters and resets all logs.

// Getters, Inspectors, and Snoopers
double profiler_3ds_get_frame_duration_impl(uint32_t frame, uint32_t id);                       // Returns the duration for a given frame and ID.
double profiler_3ds_get_total_duration_impl(uint32_t id);                                       // Returns the total duration for an ID. Must be calculated first.
double profiler_3ds_get_average_time_impl(uint32_t id);                                         // Returns the average time for an ID. Must be calculated first.
void   profiler_3ds_set_snoop_counter_impl(uint32_t snoop_id, uint8_t interval);                // Sets the interval for a snoop counter and resets its counter to the interval.
int    profiler_3ds_create_log_string_impl(uint32_t min_id_to_print, uint32_t max_id_to_print); // Creates a log string and stores it in log_string. Returns the string length if successful, or the negative string length if the buffer could not fit the string. The bounds are inclusive.
void   profiler_3ds_snoop_impl(uint32_t snoop_id);                                              // Intended for debugger use.


// Loggers and Calculators
#define profiler_3ds_log_time(id_)        profiler_3ds_log_time_impl(id_)       // Logs a time with the given ID.
#define profiler_3ds_advance_frame()      profiler_3ds_advance_frame_impl()     // Advances one frame.
#define profiler_3ds_calculate_results()  profiler_3ds_calculate_results_impl() // Calculates the results. Required to access some data.

// Resets and Initializers
#define profiler_3ds_reset()              profiler_3ds_reset_impl() // Resets the profiler.
#define profiler_3ds_init()               profiler_3ds_init_impl()  // Initializes the snoop counters and resets all logs.

// Getters, Inspectors, and Snoopers
#define profiler_3ds_get_frame_duration(frame_, id_)          profiler_3ds_get_frame_duration_impl(frame_, id_)         // Returns the duration for a given frame and ID.
#define profiler_3ds_get_total_duration(id_)                  profiler_3ds_get_total_duration_impl(id_)                 // Returns the total duration for an ID. Must be calculated first.
#define profiler_3ds_get_average_time(id_)                    profiler_3ds_get_average_time_impl(id_)                   // Returns the average time for an ID. Must be calculated first.
#define profiler_3ds_set_snoop_counter(snoop_id_, interval_)  profiler_3ds_set_snoop_counter_impl(snoop_id_, interval_) // Sets the interval for a snoop counter and resets its counter to the interval.
#define profiler_3ds_create_log_string(min_, max_)            profiler_3ds_create_log_string_impl(min_, max_)           // Creates a log string and stores it in log_string. Returns the string length if successful, or the negative string length if the buffer could not fit the string. The bounds are inclusive.
#define profiler_3ds_snoop(snoop_id_)                         profiler_3ds_snoop_impl(snoop_id_)                        // Intended for debugger use.

// Stubs used when the profiler is disabled. Note that functions aren't even defined.
#else

#define profiler_3ds_log_time(id)                                    do {} while (0) // Profiler is disabled.
#define profiler_3ds_advance_frame()                                 do {} while (0) // Profiler is disabled.
#define profiler_3ds_calculate_results()                             do {} while (0) // Profiler is disabled.
#define profiler_3ds_reset()                                         do {} while (0) // Profiler is disabled.
#define profiler_3ds_init()                                          do {} while (0) // Profiler is disabled.
#define profiler_3ds_get_frame_duration(frame, id)                               0.0 // Profiler is disabled.
#define profiler_3ds_get_total_duration(id_)                                     0.0 // Profiler is disabled.
#define profiler_3ds_get_average_time(id)                                        0.0 // Profiler is disabled.
#define profiler_3ds_set_snoop_counter(snoop_id, frames_until_snoop) do {} while (0) // Profiler is disabled.
#define profiler_3ds_create_log_string(min, max)                     do {} while (0) // Profiler is disabled.

#define profiler_3ds_snoop(snoop_id)                                 do {} while (0) // Profiler is disabled.

#endif // PROFILER_3DS_ENABLE
