#include "n3ds_async.h"

#include <string.h>
#include <stdio.h>

#include "src/pc/n3ds/n3ds_threading.h"
#include "src/pc/n3ds/n3ds_config.h"

N3DS_ThreadInfo n3ds_async_thread_info;
N3DS_AsyncThread async;

static void run_one_command(void)
{
    if (async.count != 0) {
        volatile N3DS_AsyncCommand* cmd = &async.commands[async.current];
        TickCounter tc;

        if (UNLIKELY(async.measure_time))
            osTickCounterStart(&tc);

        async.current = (async.current + 1) % ARRAY_COUNT(async.commands);

        N3DS_AsyncRun(cmd);
        int count = AtomicDecrement(&async.count);
        LightEvent_Pulse(&async.task_finished);
        
        if (UNLIKELY(async.measure_time)) {
            osTickCounterUpdate(&tc);
            printf("[%d] Async finished %s in %.2fs\n", (int) count, cmd->name, osTickCounterRead(&tc) / 1000);
        } else
            printf("[%d] Async finished %s\n", (int) count, cmd->name);
    }
}

static void initialize_thread_info(N3DS_Processor desired_cpu)
{
    n3ds_thread_info_init(&n3ds_async_thread_info);

    n3ds_async_thread_info.is_disabled                 = false;
    n3ds_async_thread_info.friendly_id                 = 2;
    n3ds_async_thread_info.assigned_cpu                = desired_cpu;
    n3ds_async_thread_info.spin_sleep_duration         = N3DS_MICROS_TO_NANOS(100);
    n3ds_async_thread_info.internal_detached           = true;
    n3ds_async_thread_info.spin_sleep_event            = &async.task_added;

    // Fill the name with terminators and then copy the default.
    n3ds_async_thread_info.friendly_name = "async";
    
    n3ds_async_thread_info.desired_priority = g3dsConfig.desired_async_thread_priority;
    n3ds_async_thread_info.task             = run_one_command;
}

int32_t N3DS_AsyncInit(N3DS_Processor desired_cpu)
{
    if (async.enabled)
        return -1;

    initialize_thread_info(desired_cpu);
    int32_t ret = 0;

    ret = n3ds_thread_start(&n3ds_async_thread_info);
    
    // If thread creation failed, or was never attempted, use thread5.
    if (n3ds_async_thread_info.thread == NULL) {
        n3ds_async_thread_info.is_disabled = true;
        n3ds_async_thread_info.assigned_cpu = OLD_CORE_0;
        async.enabled = false;
        printf("Async thread is disabled.\n");
    } else {
        async.enabled = true;
        LightEvent_Init(&async.task_finished, RESET_STICKY);
        LightEvent_Init(&async.task_added, RESET_STICKY);
    }

    async.measure_time = g3dsConfig.console_screen != N3DS_SCREEN_NONE;
    printf("Async measure time: %c\n", async.measure_time ? 'Y' : 'N');

    return ret;
}

void N3DS_AsyncExit(void)
{
    if (!n3ds_async_thread_info.running)
        return;

    N3DS_AsyncWaitForEmpty(N3DS_MILLIS_TO_NANOS(1));
    n3ds_async_thread_info.running = false;
    while (R_FAILED(threadJoin(n3ds_async_thread_info.thread, N3DS_MILLIS_TO_NANOS(100))))
        printf("Waiting for async thread to exit...\n");
    n3ds_async_thread_info.thread = NULL;
}
