#ifndef PROFILER_3DS_H
#define PROFILER_3DS_H

#define PROFILER_3DS_ENABLE 0

// Maximum ID number for a timestamp
#define PROFILER_3DS_TIME_LOG_MAX_ID 255

// Maximum number of timestamps that can be stored without a reset.
// Additional timestamps will be dropped.
#define PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH 1024

// Maximum number of snoop IDs to track. If a given ID exceeds this
// value, it will trigger every snoop.
#define PROFILER_3DS_NUM_TRACKED_SNOOP_IDS 64

#if PROFILER_3DS_ENABLE == 1
#define PROFILER_3DS_DO(a) do{ a; } while(0)
#else
#define PROFILER_3DS_DO(a) do{} while(0)
#endif

void profiler_3ds_log_time_impl(const unsigned int id);
void profiler_3ds_log_start_time_impl();
void profiler_3ds_snoop_impl(volatile unsigned int snoop_id);
double profiler_3ds_elapsed_time_impl();
void profiler_3ds_reset_impl();

// Macros
#define profiler_3ds_log_time(id)     PROFILER_3DS_DO(profiler_3ds_log_time_impl(id)) // Logs a time with the given ID.
#define profiler_3ds_log_start_time() PROFILER_3DS_DO(profiler_3ds_log_start_time_impl()) // Logs the start time, from which deltas and relative times are computed.
#define profiler_3ds_snoop(snoop_id)  PROFILER_3DS_DO(profiler_3ds_snoop_impl(snoop_id)) // Computes some useful information for the timestamps. Intended for debugger use.
#define profiler_3ds_elapsed_time()   PROFILER_3DS_DO(profiler_3ds_elapsed_time_impl()) // Returns the total elapsed time in milliseconds.
#define profiler_3ds_reset()          PROFILER_3DS_DO(profiler_3ds_reset_impl()) // Resets all parameters and logs the start time.

#endif