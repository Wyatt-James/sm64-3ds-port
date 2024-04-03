#include <stdio.h>
#include <string.h>
#include <macros.h>
#include "n3ds_threading_common.h"

#define DEFAULT_THREAD_NAME "unnamed"

bool n3ds_enable_multi_threading = true;
bool n3ds_old_core_1_is_available = false;


static void default_init(UNUSED struct N3dsThreadInfo* thread_info) {}
static bool default_should_sleep() { return false; }
static void default_task() {}
static void default_teardown(UNUSED struct N3dsThreadInfo* thread_info) {}

void n3ds_thread_info_init(struct N3dsThreadInfo* thread_info)
{
    thread_info->is_disabled                 = true;
    thread_info->friendly_id                 = 0xBEEF;
    thread_info->misc_data                   = NULL;

    // Fill the name with terminators and then copy the default.
    memset(thread_info->friendly_name,         '\0', sizeof(thread_info->friendly_name));
    memcpy(thread_info->friendly_name,         DEFAULT_THREAD_NAME, sizeof(DEFAULT_THREAD_NAME));
    
    thread_info->assigned_cpu                = OLD_CORE_0;
    thread_info->desired_priority            = N3DS_DESIRED_PRIORITY_MAIN_THREAD;
    thread_info->actual_priority             = -1;
    thread_info->priority_retrieved          = false;
    thread_info->enable_sleep_while_spinning = true;
    thread_info->spin_sleep_duration         = N3DS_MICROS_TO_NANOS(10);

    thread_info->internal_stack_size         = 64 * 1024;
    thread_info->internal_detached           = true;
    thread_info->thread                      = NULL;

    thread_info->running                     = false;
    thread_info->is_currently_processing     = false;
    thread_info->has_settled                 = false;
    thread_info->attempted_to_start          = false;

    thread_info->entry_point  = n3ds_thread_loop_common;
    thread_info->on_start     = default_init;
    thread_info->should_sleep = default_should_sleep;
    thread_info->task         = default_task;
    thread_info->teardown     = default_teardown;
}

void n3ds_thread_loop_common(struct N3dsThreadInfo* thread_info)
{
    thread_info->on_start(thread_info);
    thread_info->has_settled = true;

    while (thread_info->running)
    {
        thread_info->is_currently_processing = !thread_info->should_sleep();

        // If we're processing, run the task.
        if (thread_info->is_currently_processing)
            thread_info->task();
        
        // Else, sleep if we must
        else if (thread_info->enable_sleep_while_spinning)
            N3DS_SLEEP_FUNC(thread_info->spin_sleep_duration);
    }

    thread_info->is_currently_processing = false;
    thread_info->teardown(thread_info);
}

int32_t n3ds_thread_start(struct N3dsThreadInfo* thread_info)
{
    int friendly_id = (int) thread_info->friendly_id; // Only used for printing
    char* friendly_name = thread_info->friendly_name;

    int32_t desired_priority = thread_info->desired_priority;
    enum N3dsCpu assigned_cpu = thread_info->assigned_cpu;
    size_t stack_size = thread_info->internal_stack_size;
    bool detached = thread_info->internal_detached;

    if (thread_info->attempted_to_start) {
        fprintf(stderr, "Attempted to start thread %d twice.\n", friendly_id);
        return -3;
    }

    thread_info->attempted_to_start = true;

    // Disabled
    if (thread_info->is_disabled) {
        fprintf(stderr, "Thread %d is disabled. Not starting.\n", friendly_id);
        return -2;
    }

    printf("Attempting to start thread %d: %s.\n"
           "Desired priority: 0x%x.\n"
           "Assigned CPU: 0x%x.\n"
           "Stack size: 0x%x.\n"
           "Detached: %s.\n",
           friendly_id,
           friendly_name,
           (int) desired_priority,
           assigned_cpu,
           stack_size,
           detached ? "Y" : "N");

    thread_info->running = true;
    thread_info->thread = threadCreate((ThreadFunc) thread_info->entry_point,
                                       (void*) thread_info,
                                       stack_size,
                                       desired_priority,
                                       assigned_cpu,
                                       detached);

    if (thread_info->thread != NULL) {
        printf("Started thread %d on CPU %i.\n", friendly_id, assigned_cpu);

        for (int settle_counter = 0; !thread_info->has_settled; settle_counter++) {
            if ((settle_counter & 15) == 0)
                printf("Thread %d is settling...\n", friendly_id); // Print every ~0.8s

            N3DS_SLEEP_FUNC(N3DS_MILLIS_TO_NANOS(50));
        }

        printf("Thread %d settled.\n", friendly_id);

        // Attempt to grab this thread's actual priority, else set it to -1.
        if (!R_SUCCEEDED(svcGetThreadPriority(&thread_info->actual_priority, threadGetHandle(thread_info->thread)))) {
            fprintf(stderr, "Could not get priority of thread %d. Assuming desired priority.\n", friendly_id);
            thread_info->actual_priority = thread_info->desired_priority;
            thread_info->priority_retrieved = false;
        }
        else
            thread_info->priority_retrieved = true;

    } else {
        fprintf(stderr, "Failed to create thread %d.\n", friendly_id);
        thread_info->is_disabled = true;
        thread_info->running = false;
        return -1;
    }

    return 0;
}

int32_t n3ds_enable_old_core_1()
{
    static bool has_run = false;

    if (has_run)
        return -2;

    n3ds_old_core_1_is_available = R_SUCCEEDED(APT_SetAppCpuTimeLimit(N3DS_CORE_1_LIMIT));
    printf("AppCpuTimeLimit is %d.\nAppCpuIdleLimit is %d.\n", N3DS_CORE_1_LIMIT, N3DS_CORE_1_LIMIT_IDLE);
    return n3ds_old_core_1_is_available ? 0 : -1;
}
