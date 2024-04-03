#ifdef TARGET_N3DS

// Must be on top to ensure that 3DS types do not redefine other types.
// Includes 3ds.h and 3ds_types.h.
#include "audio_3ds_threading.h"
#include "src/pc/n3ds/n3ds_system_info.h"

#include <stdio.h>
#include <string.h>
#include "macros.h"
#include "audio_3ds.h"
#include "src/audio/external.h"

#define PLAYBACK_RATE 32000

// We synthesize 2 * SAMPLES_HIGH or LOW each frame
#ifdef VERSION_EU
#define SAMPLES_HIGH 656 // ROUND_UP_16(32000/50) + 16
#define SAMPLES_LOW 640
#define SAMPLES_DESIRED 1320
#else
#define SAMPLES_HIGH 544 // ROUND_UP_16(32000/60)
#define SAMPLES_LOW 528
#define SAMPLES_DESIRED 1100
#endif

#define N3DS_DSP_DMA_BUFFER_COUNT 4
#define N3DS_DSP_DMA_BUFFER_SIZE 4096 * 4
#define N3DS_DSP_N_CHANNELS 2

// Definitions taken from libctru comments
union NdspMix {
    float raw[12];
    struct {
        struct {
            float volume_left;
            float volume_right;
            float volume_back_left;
            float volume_back_right;
        } main;

        struct {
            float volume_left;
            float volume_right;
            float volume_back_left;
            float volume_back_right;
        } aux_0;

        struct {
            float volume_left;
            float volume_right;
            float volume_back_left;
            float volume_back_right;
        } aux_1;
    } mix;
};

struct N3dsThreadInfo n3ds_audio_thread_info;
enum N3dsCpu n3ds_desired_audio_cpu = OLD_CORE_0;

bool s_thread5_wait_for_audio_to_finish = true;

// Synchronization Variables
volatile __3ds_s32 s_audio_frames_to_tick = 0;
volatile __3ds_s32 s_audio_frames_to_process = 0;

// Statically allocate to improve performance
static s16 audio_buffer [2 * SAMPLES_HIGH * N3DS_DSP_N_CHANNELS];

// Used in synthesis.c to avoid intermediate copies when possible
size_t samples_to_copy;
int16_t* copy_buf;
int16_t* direct_buf;

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
    return SAMPLES_DESIRED;
}

// Returns true if the next buffer is FREE or DONE. Available in audio_3ds.h.
bool audio_3ds_next_buffer_is_ready()
{
    const u8 status = sDspBuffers[sNextBuffer].status;
    return status == NDSP_WBUF_FREE || status == NDSP_WBUF_DONE;
}

// Copies len_to_copy bytes from src to the audio buffer, then submits len_total to play. 
static void audio_3ds_play_internal(const uint8_t *src, size_t len_total, size_t len_to_copy)
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

    // DSP_FlushDataCache is slow if AppCpuLimit is set high for some reason.
    // svcFlushProcessDataCache is much faster and still works perfectly.
    // DSP_FlushDataCache(dst, len_total);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (Handle) dst, len_total);
    
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
        audio_3ds_play_internal(buf, len, len);
}

inline void audio_3ds_run_one_frame() {

    // If we've buffered less than desired, SAMPLES_HIGH; else, SAMPLES_LOW
    u32 num_audio_samples = audio_3ds_buffered() < audio_3ds_get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    s16* const direct_buf_t = (s16*) sDspVAddrs[sNextBuffer];
    samples_to_copy = 0;
    
    // Update audio state once per Thread5 frame, then let Thread5 continue
    update_game_sound_wrapper_3ds();
    AtomicDecrement(&s_audio_frames_to_tick);

    // Synthesize to our audio buffer
    for (int i = 0; i < 2; i++) {
        copy_buf = audio_buffer + i * (num_audio_samples * N3DS_DSP_N_CHANNELS);
        direct_buf = direct_buf_t + i * (num_audio_samples * N3DS_DSP_N_CHANNELS);

        create_next_audio_buffer(copy_buf, num_audio_samples);
    }

    AtomicDecrement(&s_audio_frames_to_process);

    // Play our audio buffer. If we outrun the DSP, we wait until the DSP is ready.
    audio_3ds_play_internal((u8 *)audio_buffer, N3DS_DSP_N_CHANNELS * num_audio_samples * 4, N3DS_DSP_N_CHANNELS * samples_to_copy * 4);
}

// In an ideal world, we would just continually run audio synthesis.
// This would give an N64-like behavior, with no audio choppiness on slowdown.
// However, due to race conditions, that is currently non-viable.
// If the audio thread's DMA were ripped out, it would likely work.
static bool audio_3ds_thread_should_sleep()
{
    return !(s_audio_frames_to_process > 0);
}

static void audio_3ds_thread_teardown()
{
    // Set to a negative value to ensure that the game loop does not deadlock.
    s_audio_frames_to_process = -9999;
    s_audio_frames_to_tick = -9999;
}

static void initialize_thread_info()
{
    n3ds_thread_info_init(&n3ds_audio_thread_info);

    n3ds_audio_thread_info.is_disabled                 = false;
    n3ds_audio_thread_info.friendly_id                 = N3DS_AUDIO_THREAD_FRIENDLY_ID;
    n3ds_audio_thread_info.enable_sleep_while_spinning = N3DS_AUDIO_ENABLE_SLEEP_FUNC;
    n3ds_audio_thread_info.assigned_cpu                = n3ds_desired_audio_cpu;

    // Fill the name with terminators and then copy the default.
    memcpy(n3ds_audio_thread_info.friendly_name, N3DS_AUDIO_THREAD_NAME, sizeof(N3DS_AUDIO_THREAD_NAME));
    
    n3ds_audio_thread_info.desired_priority = N3DS_DESIRED_PRIORITY_AUDIO_THREAD;
    n3ds_audio_thread_info.should_sleep     = audio_3ds_thread_should_sleep;
    n3ds_audio_thread_info.task             = audio_3ds_run_one_frame;
    n3ds_audio_thread_info.teardown         = audio_3ds_thread_teardown;
}

// Fully initializes the audio thread
static void audio_3ds_initialize_thread()
{
    // Start audio thread in a consistent state
    s_audio_frames_to_tick = s_audio_frames_to_process = 0;
    initialize_thread_info();

    // Create a thread if applicable
    if (n3ds_audio_thread_info.assigned_cpu != OLD_CORE_0)
        n3ds_thread_start(&n3ds_audio_thread_info);
    
    // If thread creation failed, or was never attempted, use thread5.
    if (n3ds_audio_thread_info.thread == NULL) {
        n3ds_audio_thread_info.is_disabled = true;
        n3ds_audio_thread_info.assigned_cpu = OLD_CORE_0;
        printf("Using synchronous audio.\n");
    }
}

union NdspMix ndsp_mix;

static void audio_3ds_initialize_dsp()
{
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnWaveBufClear(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, PLAYBACK_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    memset(ndsp_mix.raw, 0, sizeof(ndsp_mix));
    ndsp_mix.mix.main.volume_left = 1.0;
    ndsp_mix.mix.main.volume_right = 1.0;
    ndspChnSetMix(0, ndsp_mix.raw);

    u8* bufferData = linearAlloc(N3DS_DSP_DMA_BUFFER_SIZE * N3DS_DSP_DMA_BUFFER_COUNT);
    for (int i = 0; i < N3DS_DSP_DMA_BUFFER_COUNT; i++)
    {
        sDspVAddrs[i] = &bufferData[i * N3DS_DSP_DMA_BUFFER_SIZE];
        sDspBuffers[i].data_vaddr = &bufferData[i * N3DS_DSP_DMA_BUFFER_SIZE];
        sDspBuffers[i].nsamples = 0;
        sDspBuffers[i].status = NDSP_WBUF_FREE;
    }

    sNextBuffer = 0;
}

static bool audio_3ds_init()
{
    audio_3ds_initialize_dsp();
    audio_3ds_initialize_thread();
    return true;
}

// Stops the audio thread and waits for it to exit.
static void audio_3ds_stop(void)
{
    if (n3ds_audio_thread_info.thread) {
        n3ds_audio_thread_info.running = false;
        threadJoin(n3ds_audio_thread_info.thread, U64_MAX);
        n3ds_audio_thread_info.thread = NULL;
    }

    ndspExit();
}

void audio_3ds_set_dsp_volume(float left, float right)
{
    ndsp_mix.mix.main.volume_left = left;
    ndsp_mix.mix.main.volume_right = right;
    ndspChnSetMix(0, ndsp_mix.raw);
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
