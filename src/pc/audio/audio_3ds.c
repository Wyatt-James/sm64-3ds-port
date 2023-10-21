#ifdef TARGET_N3DS

// Must be on top to ensure that 3DS types do not redefine other types.
// Includes 3ds.h and 3ds_types.h.
#include "audio_3ds_threading.h"

#include <stdio.h>
#include <string.h>
#include "macros.h"
#include "audio_3ds.h"
#include "src/audio/external.h"

#include "src/pc/profiler_3ds.h"

#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

#define N3DS_DSP_DMA_BUFFER_COUNT   4
#define N3DS_DSP_DMA_BUFFER_SIZE   4096 * 4
#define N3DS_DSP_N_CHANNELS 2

// Used by Thread5 exclusively
bool s_wait_for_audio_thread_to_finish = true;
bool s_do_audio_on_thread5 = false;

// Incremented by level_script, decremented here
volatile __3ds_s32 s_audio_frames_queued = 0;

// Statically allocate to improve performance
static s16 audio_buffer [2 * SAMPLES_HIGH * N3DS_DSP_N_CHANNELS];

// Used in synthesis.c to avoid intermediate copies when possible
size_t samples_to_copy;
int16_t* copy_buf;
int16_t* direct_buf;

static volatile bool running = true;

static bool is_new_n3ds()
{
    bool is_new_n3ds = false;
    return R_SUCCEEDED(APT_CheckNew3DS(&is_new_n3ds)) ? is_new_n3ds : false;
}

extern void create_next_audio_buffer(s16 *samples, u32 num_samples);

static int sNextBuffer;
static volatile ndspWaveBuf sDspBuffers[N3DS_DSP_DMA_BUFFER_COUNT];
static void* sDspVAddrs[N3DS_DSP_DMA_BUFFER_COUNT];

static int audio_3ds_buffered(void)
{
    int total = 0;
    for (int i = 0; i < N3DS_DSP_DMA_BUFFER_COUNT; i++)
    {
        if (sDspBuffers[i].status == NDSP_WBUF_QUEUED ||
            sDspBuffers[i].status == NDSP_WBUF_PLAYING)
            total += sDspBuffers[i].nsamples;
    }
    return total;
}

static int audio_3ds_get_desired_buffered(void)
{
    return 1100;
}

// Returns true if the next buffer is FREE or DONE. Available in audio_3ds.h.
bool audio_3ds_next_buffer_is_ready()
{
    const u8 status = sDspBuffers[sNextBuffer].status;
    return status == NDSP_WBUF_FREE || status == NDSP_WBUF_DONE;
}

// Copies len_to_copy bytes from src to the audio buffer, then submits len_total to play. 
static void audio_3ds_play_fast(const uint8_t *src, size_t len_total, size_t len_to_copy)
{
    if (len_total > N3DS_DSP_DMA_BUFFER_SIZE)
        return;
    
    // Wait for the next audio buffer to free. This avoids discarding
    // buffers if we outrun the DSP slightly. The DSP should consume
    // buffer at a constant rate, so waiting should be ok. This
    // technically slows down synthesis slightly.
    while (!audio_3ds_next_buffer_is_ready())
        N3DS_AUDIO_SLEEP_FUNC(N3DS_AUDIO_SLEEP_DURATION_NANOS);

    // Copy the data to be played
    s16* dst = (s16*)sDspVAddrs[sNextBuffer];

    if (len_to_copy != 0)
        memcpy(dst, src, len_to_copy);

    DSP_FlushDataCache(dst, len_total);
    
    // Actually play the data
    sDspBuffers[sNextBuffer].nsamples = len_total / 4;
    sDspBuffers[sNextBuffer].status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(0, (ndspWaveBuf*) &sDspBuffers[sNextBuffer]);

    sNextBuffer = (sNextBuffer + 1) % N3DS_DSP_DMA_BUFFER_COUNT;
}

// Plays len bytes from buf in sNextBuffer, if it is available.
static void audio_3ds_play_ext(const uint8_t *buf, size_t len)
{
    if (audio_3ds_next_buffer_is_ready())
        audio_3ds_play_fast(buf, len, len);
}

inline void audio_3ds_run_one_frame() {
            
            profiler_3ds_reset();

            // If we've buffered less than desired, SAMPLES_HIGH; else, SAMPLES_LOW
            u32 num_audio_samples = audio_3ds_buffered() < audio_3ds_get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;

            samples_to_copy = 0;
            s16* direct_buf_t = (s16*) sDspVAddrs[sNextBuffer];
            
            // Update audio state once per Thread5 frame, then let Thread5 continue
            update_game_sound_wrapper_3ds();
            profiler_3ds_log_time(0);

            // Synthesize to our audio buffer
            for (int i = 0; i < 2; i++) {
                copy_buf = audio_buffer + i * (num_audio_samples * N3DS_DSP_N_CHANNELS);
                direct_buf = direct_buf_t + i * (num_audio_samples * N3DS_DSP_N_CHANNELS);

                create_next_audio_buffer(copy_buf, num_audio_samples);
            }

            // Note: this value might have been incremented by Thread5.
            AtomicDecrement(&s_audio_frames_queued);

            // Play our audio buffer. If we outrun the DSP, we wait until the DSP is ready.
            profiler_3ds_log_time(0);
            audio_3ds_play_fast((u8 *)audio_buffer, N3DS_DSP_N_CHANNELS * num_audio_samples * 4, N3DS_DSP_N_CHANNELS * samples_to_copy * 4);
            profiler_3ds_log_time(17); // 3DS export to DSP 

            profiler_3ds_snoop(0);
}

static void audio_3ds_loop()
{
    while (running)
    {
        // Wait for Thread5 to give us a frame of audio
        if (s_audio_frames_queued)
            audio_3ds_run_one_frame();
        else
            N3DS_AUDIO_SLEEP_FUNC(N3DS_AUDIO_SLEEP_DURATION_NANOS);
    }

    // Set to a negative value to ensure that the game loop does not deadlock.
    s_audio_frames_queued = -9999;
}

Thread threadId;

static bool audio_3ds_init()
{
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnWaveBufClear(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 32000);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

    u8* bufferData = linearAlloc(N3DS_DSP_DMA_BUFFER_SIZE * N3DS_DSP_DMA_BUFFER_COUNT);
    for (int i = 0; i < N3DS_DSP_DMA_BUFFER_COUNT; i++)
    {
        sDspVAddrs[i] = &bufferData[i * N3DS_DSP_DMA_BUFFER_SIZE];
        sDspBuffers[i].data_vaddr = &bufferData[i * N3DS_DSP_DMA_BUFFER_SIZE];
        sDspBuffers[i].nsamples = 0;
        sDspBuffers[i].status = NDSP_WBUF_FREE;
    }

    sNextBuffer = 0;
    volatile int cpu;

    
    if (is_new_n3ds())
        cpu = 2; // n3ds 3rd core]

    else if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(80)))
        cpu = 1; // o3ds 2nd core (system)

    else
        cpu = 0; // Run in Thread5

    if (cpu != 0) {
        threadId = threadCreate(audio_3ds_loop, 0, 64 * 1024, 0x18, cpu, true);

        if (threadId != NULL) {
            s_do_audio_on_thread5 = false;
            printf("Created audio thread on core %i\n", cpu);
        } else {
            s_do_audio_on_thread5 = true;
            printf("Failed to create audio thread. Using Thread5.\n");
        }
    } else {
        s_do_audio_on_thread5 = true;
        printf("Using Thread5 for audio, as program CPU time limit failed.\n");
    }

    return true;
}

// Stops the audio thread and waits for it to exit.
static void audio_3ds_stop(void)
{
    running = false;

    if (threadId)
        threadJoin(threadId, U64_MAX);

    ndspExit();
}

struct AudioAPI audio_3ds =
{
    audio_3ds_init,
    audio_3ds_buffered,
    audio_3ds_get_desired_buffered,
    audio_3ds_play_ext,
    audio_3ds_stop
};

#endif
