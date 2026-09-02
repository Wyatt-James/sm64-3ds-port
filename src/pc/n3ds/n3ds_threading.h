#pragma once

/*
 * N3DS-specific multithreading API
 */

#include "src/pc/n3ds/libctru_inc.h"

#define N3DS_SECONDS_TO_NANOS(t) (t * 1000000000)     // Calculate a duration in seconds
#define N3DS_MILLIS_TO_NANOS(t)  (t * 1000000)        // Calculate a duration in milliseconds
#define N3DS_MICROS_TO_NANOS(t)  (t * 1000)           // Calculate a duration in microseconds
#define N3DS_NANOS(t)            (t)                  // A duration in nanoseconds
#define N3DS_SLEEP_FUNC(time)    svcSleepThread(time) // Allows us to conveniently replace the sleep func.

typedef enum
{
    OLD_CORE_0  = 0, // Main game core
    OLD_CORE_1  = 1, // System core
    NEW_CORE_2  = 2, // Only available on new 3DS
    NEW_CORE_3  = 3, // Reserved for the kernel and not available for user-mode scehduling.
} N3DS_Processor;

// If true, runtime systems will avoid creating threads and using thread functions.
extern bool n3ds_old_core_1_is_available;

typedef struct N3DS_ThreadInfo_tag N3DS_ThreadInfo; // Required for pointer-to-self

struct N3DS_ThreadInfo_tag
{
    // --- Constants ---
    bool is_disabled;                 // Determines whether this thread will be used or not. Relevant for synchronous engine modes.
    int32_t friendly_id;              // Programmer-assigned thread ID.
    void* misc_data;                  // Miscellaneous data. Put whatever you want in here.
    const char* friendly_name;        // Friendly name for printing.

    N3DS_Processor assigned_cpu;      // Which CPU this thread is assigned to.
    int32_t desired_priority;         // Priority of this thread.
    int32_t actual_priority;          // Real priority of this thread, as obtained via a syscall. If the syscall fails, it is set to desired_priority.
    bool priority_retrieved;          // True if the thread priority syscall worked, false otherwise.
    bool enable_sleep_while_spinning; // If set, this thread will use system sleep while spinning.
    __3ds_s64 spin_sleep_duration;    // Duration in nanos for this thread to sleep while spinning.
    LightEvent* spin_sleep_event;     // If non-null, this thread will wait for this event instead of spinning.

    // --- Internal stuff ---
    size_t internal_stack_size;       // Stack size to allocate.
    size_t internal_detached;         // Whether to create the thread as detached or not.
    Thread thread;                    // Internal ID of this thread.

    // --- Volatile vars ---
    volatile bool running;                 // When set to false, the thread will exit its loop and begin teardown.
    volatile bool is_currently_processing; // Set to false when spinning. Do not use for bidirectional synchronization.
    volatile bool has_settled;             // Set to true once the thread is in a steady state.
    volatile bool attempted_to_start;      // Set to true when attempting to start.

    // --- API Functions ---
    void (*entry_point)  (N3DS_ThreadInfo* thread_info); // Entry-point.
    void (*on_start)     (N3DS_ThreadInfo* thread_info); // Runs once on startup.
    bool (*should_sleep) (void);                         // Determines whether this thread should run its task or spin.
    void (*task)         (void);                         // Does real work.
    void (*teardown)     (N3DS_ThreadInfo* thread_info); // Runs after the thread exits its loop.
};

// Initializes an N3dsThreadInfo object.
extern void n3ds_thread_info_init(N3DS_ThreadInfo* thread_info);

// A provied thread entrypoint.
extern void n3ds_thread_loop_common(N3DS_ThreadInfo* thread_info);

/*
 * Starts a thread with the given information.
 * Returns:
 *   0: success
 *  -1: attempted and failed to create thread
 *  -2: thread was disabled, so no attempt was made
 *  -3: already attempted to start this thread
 */
extern int32_t n3ds_thread_start(N3DS_ThreadInfo* thread_info);

/*
 * Stops the given thread
 * Returns:
 *   0: the thread was joined successfully or timeout was 0
 *   1: the thread was not already running (info->thread == NULL or info->running is false)
 *  -1: timeout elapsed
 * Pass U64_MAX to wait indefinitely for the thread to close.
 * Pass 0 to skip joining threads. Thread->thread will not be set to NULL.
 * Thread->running will be set to false unconditionally.
 * Thread->thread will be set to NULL only if joined successfully.
 */
extern int32_t n3ds_thread_stop(N3DS_ThreadInfo* thread_info, __3ds_u64 timeout);

/* Attempts to enable O3DS core 1.
 * Return:
 *  0: success
 * -1: failure on syscall
 * -2: function was run twice
 */
extern int32_t n3ds_enable_old_core_1(void);
