#ifndef PROFILER_3DS_H
#define PROFILER_3DS_H

#define PROFILER_3DS_ENABLE 0

// Maximum ID number for a timestamp
#define PROFILER_TIME_LOG_MAX_ID 255

// Maximum number of timestamps that can be stored without a reset.
// Additional timestamps will be dropped.
#define PROFILER_TIMESTAMP_HISTORY_LENGTH 1024

// Maximum number of snoop IDs to track. If a given ID exceeds this
// value, it will trigger every snoop.
#define PROFILER_NUM_TRACKED_SNOOP_IDS 64

#if PROFILER_3DS_ENABLE == 1
#define PROFILER_DO(a) do{ a; } while(0)
#else
#define PROFILER_DO(a) do{} while(0)
#endif

void profiler_log_time_impl(const unsigned int id);
void profiler_log_start_time_impl();
void profiler_snoop_impl(volatile unsigned int snoop_id);
double profiler_elapsed_time_impl();
void profiler_reset_impl();

// Macros
#define profiler_log_time(id)     PROFILER_DO(profiler_log_time_impl(id)) // Logs a time with the given ID.
#define profiler_log_start_time() PROFILER_DO(profiler_log_start_time_impl()) // Logs the start time, from which deltas and relative times are computed.
#define profiler_snoop(snoop_id)  PROFILER_DO(profiler_snoop_impl(snoop_id)) // Computes some useful information for the timestamps. Intended for debugger use.
#define profiler_elapsed_time()   PROFILER_DO(profiler_elapsed_time_impl()) // Returns the total elapsed time in milliseconds.
#define profiler_reset()          PROFILER_DO(profiler_reset_impl()) // Resets all parameters and logs the start time.

#endif