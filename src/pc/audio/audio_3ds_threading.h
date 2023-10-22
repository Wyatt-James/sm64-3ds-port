#ifdef TARGET_N3DS

#ifndef GFX_3DS_AUDIO_THREADING_H
#define GFX_3DS_AUDIO_THREADING_H

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

#define N3DS_AUDIO_ENABLE_SLEEP_FUNC 0

// Audio sleep duration of 0.001ms. May sleep for longer.
#define N3DS_AUDIO_SLEEP_DURATION_NANOS 1000

// Allows us to conveniently replace 3DS sleep functions
#if N3DS_AUDIO_ENABLE_SLEEP_FUNC == 1
#define N3DS_AUDIO_SLEEP_FUNC(time) svcSleepThread(time)
#else
#define N3DS_AUDIO_SLEEP_FUNC(time) do {} while (0)
#endif

// Controls when Thread5 is allowed to skip waiting for the audio thread.
extern bool s_thread5_wait_for_audio_to_finish;

// Tells Thread5 whether or not to run audio synchronously
extern bool s_thread5_does_audio;

// Synchronization variables
extern volatile __3ds_s32 s_audio_frames_to_tick;
extern volatile __3ds_s32 s_audio_frames_to_process;

#endif
#endif
