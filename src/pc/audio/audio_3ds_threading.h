#ifdef TARGET_N3DS

#ifndef GFX_3DS_AUDIO_THREADING_H
#define GFX_3DS_AUDIO_THREADING_H

#define N3DS_AUDIO_ENABLE_SLEEP_FUNC 1

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

// Currently, the maximum is 1, which allows for one frame to
// be synthesizing and one frame to be ticking on Thread5.
#define N3DS_AUDIO_MAXIMUM_QUEUED_FRAMES 1

// Audio sleep duration of 0.001ms. May sleep for longer.
#define N3DS_AUDIO_SLEEP_DURATION_NANOS 1000

// Allows us to conveniently replace 3DS sleep functions
#if PROFILER_3DS_ENABLE == 1
#define N3DS_AUDIO_SLEEP_FUNC(time) svcSleepThread(time)
#else
#define N3DS_AUDIO_SLEEP_FUNC(time) do {} while (0)
#endif

// Controls when Thread5 is allowed to skip waiting for the
// 3DS audio thread.
extern bool s_wait_for_audio_thread_to_finish;

// This tracks how many audio frames are queued.
// Always <= N3DS_AUDIO_MAXIMUM_QUEUED_FRAMES.
extern volatile __3ds_s32 s_audio_frames_queued;

// Set to 1 when update_game_sound() is called
extern volatile bool s_audio_has_updated_game_sound;

#endif
#endif
