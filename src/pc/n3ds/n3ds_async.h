#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "src/pc/n3ds/libctru_inc.h"
#include "src/pc/pc_macros.h"
#include "src/pc/n3ds/n3ds_threading.h"

// Asynchronous processing thread
// Only supports one submitting thread

#define ASYNC_QUEUE_SIZE 4

// Required to work around delayed macro expansion
#undef AtomicIncrement
#undef AtomicDecrement
#define AtomicIncrement(ptr) __atomic_add_fetch((__3ds_u32*)(ptr), 1, __ATOMIC_SEQ_CST)
#define AtomicDecrement(ptr) __atomic_sub_fetch((__3ds_u32*)(ptr), 1, __ATOMIC_SEQ_CST)

typedef int (*N3DS_AsyncFunc)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);

typedef struct
{
    bool notFinished;
    int retcode;
} N3DS_AsyncReceipt;

typedef struct
{
    N3DS_AsyncFunc func;
    const char* name;
    volatile N3DS_AsyncReceipt* receipt; // Optional
    uintptr_t params[4];
} N3DS_AsyncCommand;

typedef struct
{
    bool enabled : 1, measure_time : 1;
    volatile N3DS_AsyncCommand commands[ASYNC_QUEUE_SIZE];
    N3DS_AsyncCommand sync_command;
    size_t next, current;
    volatile __3ds_u32 count;
    LightEvent task_finished, task_added;
} N3DS_AsyncThread;

extern N3DS_AsyncThread async;

#define N3DS_ASYNC_CALLFUNC0(name_, rcpt_, func_)                     (N3DS_AsyncCommand) {.func = (N3DS_AsyncFunc)(uintptr_t) func_, .name = name_, .receipt = rcpt_}
#define N3DS_ASYNC_CALLFUNC1(name_, rcpt_, func_, p1_)                (N3DS_AsyncCommand) {.func = (N3DS_AsyncFunc)(uintptr_t) func_, .name = name_, .receipt = rcpt_, .params = {(uintptr_t) p1_}}
#define N3DS_ASYNC_CALLFUNC2(name_, rcpt_, func_, p1_, p2_)           (N3DS_AsyncCommand) {.func = (N3DS_AsyncFunc)(uintptr_t) func_, .name = name_, .receipt = rcpt_, .params = {(uintptr_t) p1_, (uintptr_t) p2_}}
#define N3DS_ASYNC_CALLFUNC3(name_, rcpt_, func_, p1_, p2_, p3_)      (N3DS_AsyncCommand) {.func = (N3DS_AsyncFunc)(uintptr_t) func_, .name = name_, .receipt = rcpt_, .params = {(uintptr_t) p1_, (uintptr_t) p2_, (uintptr_t) p3_}}
#define N3DS_ASYNC_CALLFUNC4(name_, rcpt_, func_, p1_, p2_, p3_, p4_) (N3DS_AsyncCommand) {.func = (N3DS_AsyncFunc)(uintptr_t) func_, .name = name_, .receipt = rcpt_, .params = {(uintptr_t) p1_, (uintptr_t) p2_, (uintptr_t) p3_, (uintptr_t) p4_}}

static inline void N3DS_AsyncWaitForEmpty(__3ds_s64 sleepDuration)
{
    while (async.count > 0)
        LightEvent_WaitTimeout(&async.task_finished, sleepDuration);
}

static inline void N3DS_AsyncWaitForSpace(__3ds_s64 sleepDuration)
{
    while (async.count >= ASYNC_QUEUE_SIZE)
        LightEvent_WaitTimeout(&async.task_finished, sleepDuration);
}

static inline void N3DS_AsyncWaitForCommand(volatile N3DS_AsyncReceipt* receipt, __3ds_s64 sleepDuration)
{
    while (receipt != NULL && receipt->notFinished)
        LightEvent_WaitTimeout(&async.task_finished, sleepDuration);
}

static inline void N3DS_AsyncRun(volatile N3DS_AsyncCommand* cmd)
{
    int retcode = cmd->func(cmd->params[0], cmd->params[1], cmd->params[2], cmd->params[3]);
    if (cmd->receipt != NULL) {
        cmd->receipt->retcode = retcode;
        cmd->receipt->notFinished = false;
    }
}

static inline N3DS_AsyncCommand* N3DS_AsyncRunSync(N3DS_AsyncCommand cmd)
{
    async.sync_command = cmd;
    N3DS_AsyncRun(&async.sync_command);
    return &async.sync_command;
}

static inline void N3DS_AsyncSubmit(N3DS_AsyncCommand cmd)
{
    volatile N3DS_AsyncCommand* out = &async.commands[async.next];
    async.next = (async.next + 1) % ARRAY_COUNT(async.commands);

    *out = cmd;

    if (out->receipt != NULL)
        out->receipt->notFinished = true;

    AtomicIncrement(&async.count);
}

static inline void N3DS_AsyncTry(N3DS_AsyncCommand cmd, bool runIfFailed)
{
    if (async.count < ARRAY_COUNT(async.commands)) // Queue has space, submit
        N3DS_AsyncSubmit(cmd);

    else if (runIfFailed) // No space, run now
        N3DS_AsyncRunSync(cmd);

    // No space, don't try
}

static inline void N3DS_AsyncSubmitBlocking(N3DS_AsyncCommand cmd, __3ds_s64 sleepDuration)
{
    N3DS_AsyncWaitForSpace(sleepDuration);
    N3DS_AsyncSubmit(cmd);
}

int32_t N3DS_AsyncInit(N3DS_Processor desired_cpu);
void N3DS_AsyncExit(void);
