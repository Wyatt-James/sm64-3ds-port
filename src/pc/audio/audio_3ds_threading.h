#ifndef AUDIO_3DS_THREADING_H
#define AUDIO_3DS_THREADING_H

#include "src/pc/n3ds/n3ds_threading_common.h"

// Set to 1 to enable sleep, or 0 to disable.
#define N3DS_AUDIO_ENABLE_SLEEP_FUNC true

#define N3DS_DESIRED_PRIORITY_AUDIO_THREAD 0x18 // Priority of audio thread

// Audio sleep duration of 10 microseconds (0.01 millis). May sleep for longer.
#define N3DS_AUDIO_SLEEP_DURATION_NANOS N3DS_MICROS_TO_NANOS(10)
#define N3DS_AUDIO_THREAD_NAME "audio"
#define N3DS_AUDIO_THREAD_FRIENDLY_ID 1

// Allows us to conveniently replace audio-related sleep functions
#if N3DS_AUDIO_ENABLE_SLEEP_FUNC == true
#define N3DS_AUDIO_SLEEP_FUNC(time) N3DS_SLEEP_FUNC(time)
#else
#define N3DS_AUDIO_SLEEP_FUNC(time) do {} while (0)
#endif

extern struct N3dsThreadInfo n3ds_audio_thread_info;
extern enum N3dsCpu n3ds_desired_audio_cpu;

// Controls when Thread5 is allowed to skip waiting for the audio thread.
extern bool s_thread5_wait_for_audio_to_finish;

// Synchronization variables
extern volatile __3ds_s32 s_audio_frames_to_tick;
extern volatile __3ds_s32 s_audio_frames_to_process;

#endif
