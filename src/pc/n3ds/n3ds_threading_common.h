#ifndef N3DS_THREADING_COMMON_H
#define N3DS_THREADING_COMMON_H

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
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
#include <3ds/types.h>
#include <3ds.h>
#include <3ds/svc.h>
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

#define N3DS_CORE_1_LIMIT 80              // Limit during gameplay. Can be [10-80].
#define N3DS_CORE_1_LIMIT_IDLE 10         // Limit when in the home menu, sleeping, etc. Can be [10-80].
#define N3DS_DESIRED_PRIORITY_MAIN_THREAD 0x19  // Priority of thread5

#define N3DS_SECONDS_TO_NANOS(t) (t * 1000000000)     // Calculate a duration in seconds
#define N3DS_MILLIS_TO_NANOS(t)  (t * 1000000)        // Calculate a duration in milliseconds
#define N3DS_MICROS_TO_NANOS(t)  (t * 1000)           // Calculate a duration in microseconds
#define N3DS_NANOS(t)            (t)                  // A duration in nanoseconds
#define N3DS_SLEEP_FUNC(time)    svcSleepThread(time) // Allows us to conveniently replace the sleep func.

enum N3dsCpu {
    OLD_CORE_0  = 0, // Main game core
    OLD_CORE_1  = 1, // System core
    NEW_CORE_2  = 2
};

// If true, runtime systems will avoid creating threads and using thread functions.
extern bool n3ds_enable_multi_threading;
extern bool n3ds_old_core_1_is_available;

struct N3dsThreadInfo {

    // --- Constants ---
    bool is_disabled;                 // Determines whether this thread will be used or not. Relevant for synchronous engine modes.
    int32_t friendly_id;              // Programmer-assigned thread ID.
    void* misc_data;                  // Miscellaneous data. Put whatever you want in here.
    char friendly_name[16];           // Friendly name for printing.

    enum N3dsCpu assigned_cpu;        // Which CPU this thread is assigned to.
    int32_t desired_priority;         // Priority of this thread.
    int32_t actual_priority;          // Real priority of this thread, as obtained via a syscall. If the syscall fails, it is set to desired_priority.
    bool priority_retrieved;          // True if the thread priority syscall worked, false otherwise.
    bool enable_sleep_while_spinning; // If set, this thread will use system sleep while spinning.
    __3ds_s64 spin_sleep_duration;    // Duration in nanos for this thread to sleep while spinning.

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
    void (*entry_point)  (struct N3dsThreadInfo* thread_info); // Entry-point.
    void (*on_start)     (struct N3dsThreadInfo* thread_info); // Runs once on startup.
    bool (*should_sleep) (void);                               // Determines whether this thread should run its task or spin.
    void (*task)         (void);                               // Does real work.
    void (*teardown)     (struct N3dsThreadInfo* thread_info); // Runs after the thread exits its loop.
};

// Initializes an N3dsThreadInfo object.
extern void n3ds_thread_info_init(struct N3dsThreadInfo* thread_info);

// A provied thread entrypoint.
extern void n3ds_thread_loop_common(struct N3dsThreadInfo* thread_info);

/*
 * Starts a thread with the given information.
 * Returns:
 *   0: success
 *  -1: attempted and failed to create thread
 *  -2: thread was disabled, so no attempt was made
 *  -3: already attempted to start this thread
 */
extern int32_t n3ds_thread_start(struct N3dsThreadInfo* thread_info);

/* Attempts to enable O3DS core 1.
 * Return:
 *  0: success
 * -1: failure on syscall
 * -2: function was run twice
 */
extern int32_t n3ds_enable_old_core_1();

#endif
