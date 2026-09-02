#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lib/src/libultra_internal.h"
#include "macros.h"
#include "src/pc/n3ds/n3ds_async.h"

#ifdef TARGET_WEB
#include <emscripten.h>
#endif

extern OSMgrArgs piMgrArgs;

u64 osClockRate = 62500000;

s32 osPiStartDma(UNUSED OSIoMesg *mb, UNUSED s32 priority, UNUSED s32 direction,
                 uintptr_t devAddr, void *vAddr, size_t nbytes,
                 UNUSED OSMesgQueue *mq) {
    memcpy(vAddr, (const void *) devAddr, nbytes);
    return 0;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}

void osSetEventMesg(UNUSED OSEvent e, UNUSED OSMesgQueue *mq, UNUSED OSMesg msg) {
}
s32 osJamMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}
s32 osSendMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
#ifdef VERSION_EU
    s32 index;
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }
    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;
#endif
    return 0;
}
s32 osRecvMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg *msg, UNUSED s32 flag) {
#if VERSION_EU
    if (mq->validCount == 0) {
        return -1;
    }
    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
#endif
    return 0;
}

uintptr_t osVirtualToPhysical(void *addr) {
    return (uintptr_t) addr;
}

void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode *mode) {
}
void osViSetEvent(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViBlack(UNUSED u8 active) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void *vaddr) {
}

OSTime osGetTime(void) {
    return 0;
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void *a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void *a, UNUSED size_t b) {
}

u32 osGetCount(void) {
    static u32 counter;
    return counter++;
}

s32 osAiSetFrequency(u32 freq) {
    u32 a1;
    s32 a2;
    u32 D_8033491C;

#ifdef VERSION_EU
    D_8033491C = 0x02E6025C;
#else
    D_8033491C = 0x02E6D354;
#endif

    a1 = D_8033491C / (float) freq + .5f;

    if (a1 < 0x84) {
        return -1;
    }

    a2 = (a1 / 66) & 0xff;
    if (a2 > 16) {
        a2 = 16;
    }

    return D_8033491C / (s32) a1;
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return 1;
}

s32 osEepromLongReadSync(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    u8 content[512];
    s32 ret = -1;

#ifdef TARGET_WEB
    if (EM_ASM_INT({
        var s = localStorage.sm64_save_file;
        if (s && s.length === 684) {
            try {
                var binary = atob(s);
                if (binary.length === 512) {
                    for (var i = 0; i < 512; i++) {
                        HEAPU8[$0 + i] = binary.charCodeAt(i);
                    }
                    return 1;
                }
            } catch (e) {
            }
        }
        return 0;
    }, content)) {
        memcpy(buffer, content + address * 8, nbytes);
        ret = 0;
    }
#else
    int file = open("sm64_save_file.bin", O_RDONLY | O_CREAT);
    if (file == -1) {
        ret = -1;
    } else if (read(file, content, 512) == 512) {
        memcpy(buffer, content + address * 8, nbytes);
        close(file);
    } else {
        ret = -1;
    }
#endif
    return ret;
}

s32 osEepromLongWriteSync(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    s32 ret = 0;
    u8 content[512] = {0};
    if (address != 0 || nbytes != 512) {
        osEepromLongReadSync(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, nbytes);

#ifdef TARGET_WEB
    EM_ASM({
        var str = "";
        for (var i = 0; i < 512; i++) {
            str += String.fromCharCode(HEAPU8[$0 + i]);
        }
        localStorage.sm64_save_file = btoa(str);
    }, content);
#else
    int file = open("sm64_save_file.bin", O_WRONLY | O_CREAT);
    if (file == -1) {
        ret = -1;
    } else {
        ret = write(file, content, 512) == 512 ? 0 : -1;
        close(file);
    }
#endif
    return ret;
}

volatile N3DS_AsyncReceipt receipt;
u8 asyncBuffer[512];

// All reads are synchronous but must wait for writes
s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    if (async.enabled) {
        printf("EEPREAD: waiting for current save\n");
        N3DS_AsyncWaitForCommand(&receipt, N3DS_MICROS_TO_NANOS(100));
    }
    s32 res = osEepromLongReadSync(mq, address, buffer, nbytes);
    printf("EEPREAD: finished with %d\n", (int) res);
    return res;
}

// All writes are asynchronous
s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    if (!async.enabled) {
        printf("SAVE: sync begin\n");
        s32 res = osEepromLongWriteSync(mq, address, buffer, nbytes);
        printf("EEPWRITE: sync end with %d\n", (int) res);
        return res;
    }
    
    printf("EEPWRITE: waiting for current save\n");
    N3DS_AsyncWaitForCommand(&receipt, N3DS_MICROS_TO_NANOS(100));
    memcpy(asyncBuffer, buffer, nbytes);
    
    printf("EEPWRITE: waiting for queue\n");
    N3DS_AsyncSubmitBlocking(N3DS_ASYNC_CALLFUNC4("save game", &receipt, osEepromLongWriteSync, mq, address, asyncBuffer, nbytes), N3DS_MICROS_TO_NANOS(100));
    printf("EEPWRITE: submitted\n");
    return 0; //! WYATT_TODO fixing this would require engine changes. We could do error checking in the save func.
}
