#ifdef TARGET_N3DS

#ifndef GFX_3DS_AUDIO_THREADING_H
#define GFX_3DS_AUDIO_THREADING_H

#define u64 __u64
#define s64 __s64
#define u32 __u32
#define vu32 __vu32
#define vs32 __vs32
#define s32 __s32
#define u16 __u16
#define s16 __s16
#define u8 __u8
#define s8 __s8
#include <3ds/types.h>
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

#include <3ds.h>

// Currently, the maximum is 1, which allows for one frame to
// be synthesizing and one frame to be ticking on Thread5.
#define MAXIMUM_QUEUED_AUDIO_FRAMES 1

// Controls when Thread5 is allowed to skip waiting for the
// 3DS audio thread.
extern bool s_wait_for_audio_thread_to_finish;

// This tracks how many audio frames are queued.
// Always <= MAXIMUM_QUEUED_AUDIO_FRAMES.
extern volatile s32 s_audio_frames_queued;

#endif
#endif
