#if __ARM_FEATURE_SIMD32 == 1 && __ARM_FEATURE_SAT == 1 // Useful for debugging

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include <arm_acle.h>
#include "src/pc/pc_macros.h"
#include "src/pc/n3ds/n3ds_intrinsics.h"

/*
 * 3DS-optimized mixer.c software implementation, using SIMD32 extensions.
 * Enhanced RSPA emulation is supported.

 * Enhanced RSPA emulation allows us to break the rules of
 * RSPA emulation a little bit for better performance.
 * 
 * 3DS cache line length is 8 words (32 bytes)
 * 
 * Thanks to michi and Wuerfel_21 for help in optimizing
 * some math and compiler nonsense.
 */

#pragma GCC optimize ("unroll-loops")


#define MIX_AUX true
#define MIX_NORMAL false
#define NUM_CHANNELS 2

#ifdef AUDIO_USE_ACCURATE_MATH
#define MIX_VOLUME_SHIFT 0
#else
#define MIX_VOLUME_SHIFT 2
#endif

#define ROUND_UP_8(v)  (((v) + 7)  & ~7)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_32(v) (((v) + 31) & ~31)

// By all rights PKHBT should be faster but it's not turning out.
#define INT16x2_LOAD(upper, lower) ((int16x2_t) (((upper) << 16) | ((uint16_t) (lower)))) // 0x2a38
// #define INT16x2_LOAD(upper, lower) ((int16x2_t) __pkhbt(lower, upper, 16)) // 0x2a28

typedef struct
{
    union {
        int16x2_t cross[8]; // table 1 high, table 0 low
        struct {
            int16_t t0, t1;
        } separate[8];
    };
    int16x2_t simd[4]; // tbl1 as read through the simd pointer
} AdpcmTableRow;

static struct {
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;

    int16_t vol[2];

    uint16_t dry_right;
    uint16_t wet_left;
    uint16_t wet_right;

    int16_t target[2];
    int32_t rate[2];

    int16_t vol_dry;
    int16_t vol_wet;

    ADPCM_STATE *adpcm_loop_state;

    AdpcmTableRow adpcm_table[8] ALIGNED(64);
    union {
        int16_t as_s16[2512 / sizeof(int16_t)];
        uint8_t as_u8[2512];
    } buf;
} rspa;

static int16_t resample_table[64][4] = {
    {0x0c39, 0x66ad, 0x0d46, 0xffdf}, {0x0b39, 0x6696, 0x0e5f, 0xffd8},
    {0x0a44, 0x6669, 0x0f83, 0xffd0}, {0x095a, 0x6626, 0x10b4, 0xffc8},
    {0x087d, 0x65cd, 0x11f0, 0xffbf}, {0x07ab, 0x655e, 0x1338, 0xffb6},
    {0x06e4, 0x64d9, 0x148c, 0xffac}, {0x0628, 0x643f, 0x15eb, 0xffa1},
    {0x0577, 0x638f, 0x1756, 0xff96}, {0x04d1, 0x62cb, 0x18cb, 0xff8a},
    {0x0435, 0x61f3, 0x1a4c, 0xff7e}, {0x03a4, 0x6106, 0x1bd7, 0xff71},
    {0x031c, 0x6007, 0x1d6c, 0xff64}, {0x029f, 0x5ef5, 0x1f0b, 0xff56},
    {0x022a, 0x5dd0, 0x20b3, 0xff48}, {0x01be, 0x5c9a, 0x2264, 0xff3a},
    {0x015b, 0x5b53, 0x241e, 0xff2c}, {0x0101, 0x59fc, 0x25e0, 0xff1e},
    {0x00ae, 0x5896, 0x27a9, 0xff10}, {0x0063, 0x5720, 0x297a, 0xff02},
    {0x001f, 0x559d, 0x2b50, 0xfef4}, {0xffe2, 0x540d, 0x2d2c, 0xfee8},
    {0xffac, 0x5270, 0x2f0d, 0xfedb}, {0xff7c, 0x50c7, 0x30f3, 0xfed0},
    {0xff53, 0x4f14, 0x32dc, 0xfec6}, {0xff2e, 0x4d57, 0x34c8, 0xfebd},
    {0xff0f, 0x4b91, 0x36b6, 0xfeb6}, {0xfef5, 0x49c2, 0x38a5, 0xfeb0},
    {0xfedf, 0x47ed, 0x3a95, 0xfeac}, {0xfece, 0x4611, 0x3c85, 0xfeab},
    {0xfec0, 0x4430, 0x3e74, 0xfeac}, {0xfeb6, 0x424a, 0x4060, 0xfeaf},
    {0xfeaf, 0x4060, 0x424a, 0xfeb6}, {0xfeac, 0x3e74, 0x4430, 0xfec0},
    {0xfeab, 0x3c85, 0x4611, 0xfece}, {0xfeac, 0x3a95, 0x47ed, 0xfedf},
    {0xfeb0, 0x38a5, 0x49c2, 0xfef5}, {0xfeb6, 0x36b6, 0x4b91, 0xff0f},
    {0xfebd, 0x34c8, 0x4d57, 0xff2e}, {0xfec6, 0x32dc, 0x4f14, 0xff53},
    {0xfed0, 0x30f3, 0x50c7, 0xff7c}, {0xfedb, 0x2f0d, 0x5270, 0xffac},
    {0xfee8, 0x2d2c, 0x540d, 0xffe2}, {0xfef4, 0x2b50, 0x559d, 0x001f},
    {0xff02, 0x297a, 0x5720, 0x0063}, {0xff10, 0x27a9, 0x5896, 0x00ae},
    {0xff1e, 0x25e0, 0x59fc, 0x0101}, {0xff2c, 0x241e, 0x5b53, 0x015b},
    {0xff3a, 0x2264, 0x5c9a, 0x01be}, {0xff48, 0x20b3, 0x5dd0, 0x022a},
    {0xff56, 0x1f0b, 0x5ef5, 0x029f}, {0xff64, 0x1d6c, 0x6007, 0x031c},
    {0xff71, 0x1bd7, 0x6106, 0x03a4}, {0xff7e, 0x1a4c, 0x61f3, 0x0435},
    {0xff8a, 0x18cb, 0x62cb, 0x04d1}, {0xff96, 0x1756, 0x638f, 0x0577},
    {0xffa1, 0x15eb, 0x643f, 0x0628}, {0xffac, 0x148c, 0x64d9, 0x06e4},
    {0xffb6, 0x1338, 0x655e, 0x07ab}, {0xffbf, 0x11f0, 0x65cd, 0x087d},
    {0xffc8, 0x10b4, 0x6626, 0x095a}, {0xffd0, 0x0f83, 0x6669, 0x0a44},
    {0xffd8, 0x0e5f, 0x6696, 0x0b39}, {0xffdf, 0x0d46, 0x66ad, 0x0c39}
};

#define SAT32_LOWER_T -0x7fffffff - 1 // Must remain this way or the compiler goofs
#define SAT32_UPPER_T 0x7fffffff

// clamps an int32_t to an int16_t
#define saturate16(v) ((int16_t) __ssat(v, 16))

// Clamps an int64_t on the positive end, with threshold SAT32_UPPER_T.
// Unfortunately, 3DS lacks a 64-bit saturation instruction.
// Do not forget to cast after using!
static inline int64_t saturate32_upper(const int64_t v) {
    return CLAMP_UPPER(v, SAT32_UPPER_T);
}

// Clamps an int64_t on the negative end, with threshold SAT32_LOWER_T.
// Unfortunately, 3DS lacks a 64-bit saturation instruction.
// Do not forget to cast after using!
static inline int64_t saturate32_lower(const int64_t v) {
    return CLAMP_LOWER(v, SAT32_LOWER_T);
}

// clamps an int64_t to an int32_t. Unfortunately, 3DS lacks a 64-bit saturation instruction.
// Doing it this way tricks the compiler into using conditional logic, which is faster than conditional branches.
static inline int32_t saturate32(const int64_t v) {
    return (int32_t) saturate32_upper(saturate32_lower(v));
}

void aClearBufferImpl(uint16_t addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memset(rspa.buf.as_u8 + addr, 0, nbytes);
}

void aLoadBufferImpl(const void *source_addr) {
    memcpy(rspa.buf.as_u8 + rspa.in, source_addr, ROUND_UP_8(rspa.nbytes));
}

void aSaveBufferImpl(int16_t *dest_addr) {
    memcpy(dest_addr, rspa.buf.as_s16 + rspa.out / sizeof(int16_t), ROUND_UP_8(rspa.nbytes));
}

// nbytes is a multiple of 16 as per the ABI, which is one row of 8 s16s.
// We don't support copying individual bytes. Womp womp! If you need that
// it'd be reasonably simple to add because data is read linearly.
void aLoadADPCMImpl(int nbytes, const int16_t *book_source_addr) {
    #define OLD(i, tbl, j) book_source_addr[i*16 + tbl*8 + j]

    for (int i = 0; i < nbytes / 16; i++) {
        int row = i >> 1;
        int tbl = i & 1;

        // Copy data of one row
        for (int j = 0; j < 8; j++) {
            int16_t entry = OLD(row, tbl, j);

            // Update the cross table
            int16_t* cross = (int16_t*) &rspa.adpcm_table[row].cross[j] + (tbl);
            *cross = entry;

            // Update the simd table
            int16_t* simd_tbl_16 = (int16_t*) &rspa.adpcm_table[row].simd;
            simd_tbl_16[j] = entry;
        }
    }
    #undef OLD
}

void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    if (flags & A_AUX) {
        rspa.dry_right = in;
        rspa.wet_left = out;
        rspa.wet_right = nbytes;
    } else {
        rspa.in = in;
        rspa.out = out;
        rspa.nbytes = nbytes;
    }
}

void aSetVolumeImpl(uint8_t flags, int16_t v, int16_t t, int16_t r) {
    if (flags & A_AUX) {
        rspa.vol_dry = v;
        rspa.vol_wet = r;
    } else if (flags & A_VOL) {
        if (flags & A_LEFT) {
            rspa.vol[0] = v;
        } else {
            rspa.vol[1] = v;
        }
    } else {
        if (flags & A_LEFT) {
            rspa.target[0] = v;
            rspa.rate[0] = (int32_t)((uint16_t)t << 16 | ((uint16_t)r));
        } else {
            rspa.target[1] = v;
            rspa.rate[1] = (int32_t)((uint16_t)t << 16 | ((uint16_t)r));
        }
    }
}

// Interleaves into dest
COLD static void aInterleaveInternal(int16_t* l, int16_t* r, int16_t* dest, const int count) {
    typedef struct
    {
        uint32_t samples[8];
    } sample_batch;

    sample_batch* dest_batch = (sample_batch*) dest;
    for (int i = count; i != 0; i--) {
        __builtin_prefetch(((void*)l) + 32);
        __builtin_prefetch(((void*)r) + 32);
        *dest_batch++ = (sample_batch) {{
            INT16x2_LOAD(l[0], r[0]),
            INT16x2_LOAD(l[1], r[1]),
            INT16x2_LOAD(l[2], r[2]),
            INT16x2_LOAD(l[3], r[3]),
            INT16x2_LOAD(l[4], r[4]),
            INT16x2_LOAD(l[5], r[5]),
            INT16x2_LOAD(l[6], r[6]),
            INT16x2_LOAD(l[7], r[7]),
        }};
        l += 8;
        r += 8;
    }
}

// Interleaves into dest. This version requires 32-bit alignment, prefers 64-bit.
static NAKED void aInterleaveInternalASM(int16_t* l, int16_t* r, int16_t* dest, int count) {
    (void) l; (void) r; (void) dest; (void) count;

// Packs and stores two sources
// in:  low is first sample, high is second
// out: low is left, high is right
#define PACKSTR(rDst, rD1, rD2, rL, rR)                                        \
    "pkhbt " #rD1", "#rL", "#rR", lsl #16   \n\t" /* L low + R low << 16   */  \
    "pkhtb " #rD2", "#rR", "#rL", asr #16   \n\t" /* L high >> 16 + R high */  \
    "stm   " #rDst", {"#rD1", "#rD2"}       \n\t"

    // r0 is left, r1 is right, r2 is dest, r3 is nIterations
    asm (
    "    cmp r3, #0                         \n\t"
    "    bxeq lr                            \n\t"
    "    push {r4-r12, lr}                  \n\t"
    "aInterleaveInternalASM_loop:           \n\t"
    "    ldm r1!, {r8-r9}                   \n\t"
    "    ldm r0!, {r4-r7}                   \n\t"
    "    ldm r1!, {r10-r11}                 \n\t"
    "    subs r3, r3, #1                    \n\t"
         PACKSTR(r2!, r12, lr, r4, r8)
         PACKSTR(r2!, r4, r8, r5, r9)
         PACKSTR(r2!, r5, r9, r6, r10)
         PACKSTR(r2!, r6, r10, r7, r11)
    "    bne aInterleaveInternalASM_loop    \n\t"
    "    pop {r4-r12, pc}                   \n\t"
    );
}

// Interleaves RSPA NBYTES bytes into RSPA OUT
void aInterleaveImpl(uint16_t left, uint16_t right) {
    const int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);
    int16_t *d = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);

    // In the real world, this seems to always be taken
    if (LIKELY((((uintptr_t)l | (uintptr_t) r | (uintptr_t) d) & 0b11) == 0))
        aInterleaveInternalASM(l, r, d, count);
    else
        aInterleaveInternal(l, r, d, count);
}

// Interleaves RSPA NBYTES bytes into the provided buffer
void aInterleaveAndCopyImpl(uint16_t left, uint16_t right, int16_t *restrict dest_addr) {
    const int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);
    
    // In the real world, this seems to always be taken
    if (LIKELY((((uintptr_t)l | (uintptr_t) r | (uintptr_t) dest_addr) & 0b11) == 0))
        aInterleaveInternalASM(l, r, dest_addr, count);
    else
        aInterleaveInternal(l, r, dest_addr, count);
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(rspa.buf.as_u8 + out_addr, rspa.buf.as_u8 + in_addr, nbytes);
}

void aSetLoopImpl(ADPCM_STATE *adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}

/*
 * Internal use only
 * ADPCM_STATE is a pointer type! Specifically a short[16].
 * 
 * ADPCM packet format:
 *  9 bytes length
 *  Byte 1, upper nibble: shift magnitude, range [0-12]
 *  Byte 1, lower nibble: table index, range [0-7]
 *  Bytes 2-9: data, 1 nibble per output sample
 * 
 * Each ADPCM packet produces 16 PCM samples.
 * Each ADPCM packet depends on the prior two PCM samples.
 * Data is decoded one ADPCM packet at a time.
*/
static void aADPCMdecInternal(uint8_t flags, ADPCM_STATE state, uint8_t* in, int16_t* out, int nbytes) {

    // Write the initial state
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    // Main decode: write data in chunks of 16 samples (32 bytes)
    while (nbytes > 0) {
        const uint8_t shift = 28 - (*in >> 4); // range 28 - 0..12
        const uint8_t table_index = *in++ & 0xf; // range 0..7
        const AdpcmTableRow tbl = rspa.adpcm_table[table_index];

        // Output 8 PCM samples per-loop
        for (int i = 0; i < 2; i++) {
            const int16x2_t prev = *((int16x2_t*) (out - 2)); // Reverse order due to endianness
            int16_t ins[8];
            int32_t acc_tbl[8];
            
            #define TCROSS(n) tbl.cross[n]
            #define T0(n) tbl.separate[n].t0
            #define T1(n) tbl.separate[n].t1
            #define TSIMD(n) tbl.simd[n]

            // Load, extend, and shift 8 nibbles from in, and calculate initial accumulators
            #pragma GCC unroll 4
            for (int j = 0; j < 8; j += 2, in++) {
                ins[j]     = ((*in >> 4)  << 28) >> shift;
                ins[j + 1] = ((*in & 0xf) << 28) >> shift;

                acc_tbl[j]     = __smlad(TCROSS(j),     prev, ins[j]     << 11);
                acc_tbl[j + 1] = __smlad(TCROSS(j + 1), prev, ins[j + 1] << 11);
            }

            int16x2_t inputs;

            // Meat and potatoes
            // Touch the funny numbers and you shall surely perish
            // acc_tbl += tbl_simd * inputs
            acc_tbl[2] = __smlad(TSIMD(0), (inputs = INT16x2_LOAD(ins[0], ins[1])), acc_tbl[2]); // tbl = 1,0
            acc_tbl[4] = __smlad(TSIMD(1),  inputs,                                 acc_tbl[4]); // tbl = 3,2
            acc_tbl[6] = __smlad(TSIMD(2),  inputs,                                 acc_tbl[6]); // tbl = 5,4
            acc_tbl[3] = __smlad(TSIMD(0), (inputs = INT16x2_LOAD(inputs, ins[2])), acc_tbl[3]); // tbl = 1,0
            acc_tbl[5] = __smlad(TSIMD(1),  inputs,                                 acc_tbl[5]); // tbl = 3,2
            acc_tbl[7] = __smlad(TSIMD(2),  inputs,                                 acc_tbl[7]); // tbl = 5,4
            acc_tbl[4] = __smlad(TSIMD(0), (inputs = INT16x2_LOAD(inputs, ins[3])), acc_tbl[4]); // tbl = 1,0
            acc_tbl[6] = __smlad(TSIMD(1),  inputs,                                 acc_tbl[6]); // tbl = 3,2
            acc_tbl[5] = __smlad(TSIMD(0), (inputs = INT16x2_LOAD(inputs, ins[4])), acc_tbl[5]); // tbl = 1,0
            acc_tbl[7] = __smlad(TSIMD(1),  inputs,                                 acc_tbl[7]); // tbl = 3,2
            acc_tbl[6] = __smlad(TSIMD(0), (inputs = INT16x2_LOAD(inputs, ins[5])), acc_tbl[6]); // tbl = 1,0
            acc_tbl[7] = __smlad(TSIMD(0),           INT16x2_LOAD(inputs, ins[6]),  acc_tbl[7]); // tbl = 1,0
            
            // Add the stragglers that we can't SIMD
            acc_tbl[1] += T1(0) * ins[0];
            acc_tbl[3] += T1(2) * ins[0];
            acc_tbl[5] += T1(4) * ins[0];
            acc_tbl[7] += T1(6) * ins[0];
            
            // Output
            #pragma GCC unroll 8
            for (int j = 0; j < 8; j++, out++)
                *out = saturate16(acc_tbl[j] >> 11);
        }
        nbytes -= 16 * sizeof(int16_t);
    }

    // Save the last 16 samples for decoding the next chunk
    memcpy(state, out - 16, 16 * sizeof(int16_t));
}

// Decodes ADPCM data directly from a given source.
void aADPCMdecDirectImpl(uint8_t flags, ADPCM_STATE state, uint8_t* source) {
    int16_t *out = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    int nbytes = ROUND_UP_32(rspa.nbytes);

    aADPCMdecInternal(flags, state, source, out, nbytes);
}

// Decompresses ADPCM data
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t *in = rspa.buf.as_u8 + rspa.in;
    int16_t *out = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    int nbytes = ROUND_UP_32(rspa.nbytes);

    aADPCMdecInternal(flags, state, in, out, nbytes);
}

void aResampleImpl(const uint8_t flags, const uint16_t pitch, RESAMPLE_STATE state) {
    int16_t tmp[16];
    int16_t *const in_initial = rspa.buf.as_s16 + rspa.in / sizeof(int16_t);
    int16_t *in = in_initial;
    int16_t *out = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    
    const uint32_t double_pitch = pitch << 1;

    if (flags & A_INIT)
        memset(tmp, 0, 5 * sizeof(int16_t));
    else
        memcpy(tmp, state, 16 * sizeof(int16_t));

    if (flags & 2) {
        memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= tmp[5] / sizeof(int16_t);
    }

    in -= 4;
    memcpy(in, tmp, 4 * sizeof(int16_t));
    uint32_t pitch_accumulator = (uint16_t) tmp[4];
    int nSamples = ROUND_UP_16(rspa.nbytes) / 2;
    
    do {
#ifdef AUDIO_USE_ACCURATE_MATH
        const int16_t* const tbl = resample_table[(pitch_accumulator << 6) >> 16];
        *out = saturate16(((in[0] * tbl[0] + 0x4000) >> 15) +
                    ((in[1] * tbl[1] + 0x4000) >> 15) +
                    ((in[2] * tbl[2] + 0x4000) >> 15) +
                    ((in[3] * tbl[3] + 0x4000) >> 15));
#else
        // Inaccurate rounding
        const int32_t* const tbl = (const int32_t* const) resample_table[(pitch_accumulator << 6) >> 16];
        const int32_t* const in_tmp = (const int32_t* const) in;
        *out++ = saturate16((__smlad(tbl[1], in_tmp[1], __smlad(tbl[0], in_tmp[0], 0x10000))) >> 15);
#endif

        pitch_accumulator += double_pitch;
        in += pitch_accumulator >> 16;
        pitch_accumulator %= 0x10000;
    } while (--nSamples > 0);

    state[4] = (int16_t) pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));

    int unknown = (in - in_initial + 4) & 7;
    in -= unknown;

    if (unknown != 0)
        unknown = -8 - unknown;

    state[5] = unknown;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}

// EnvMixes a single sample. If AUX is set, writes both dry and wet buffers, else only dry.
// Thanks to michi and Wuerfel_21 for help in optimizing the underlying math here.
static ALWAYS_INLINE void envMixerProcessOneSample(
    const int16_t input,
    int16_t* dry[2],
    int16_t* wet[2],
    const int32_t volume[2],
    const int32_t vol_dry, // In inaccurate math, these are 18-bit due to a left-shift.
    const int32_t vol_wet,
    const bool aux)
{
#ifdef AUDIO_USE_ACCURATE_MATH
    *dry[0] = saturate16(((*dry[0] << 15) - *dry[0] + input * ((volume[0] * vol_dry + 0x4000) >> 15) + 0x4000) >> 15);
    *dry[1] = saturate16(((*dry[1] << 15) - *dry[0] + input * ((volume[1] * vol_dry + 0x4000) >> 15) + 0x4000) >> 15);
    if (aux) {
        *wet[0] = saturate16(((*wet[0] << 15) - *dry[0] + input * ((volume[0] * vol_wet + 0x4000) >> 15) + 0x4000) >> 15);
        *wet[1] = saturate16(((*wet[1] << 15) - *dry[0] + input * ((volume[1] * vol_wet + 0x4000) >> 15) + 0x4000) >> 15);
    }
#else
    int32_t iv0 = __smulbt(input, volume[0]);
    int32_t iv1 = __smulbt(input, volume[1]);

    *dry[0] = saturate16(__smmlar(iv0, vol_dry, *dry[0]));
    *dry[1] = saturate16(__smmlar(iv1, vol_dry, *dry[1]));
    if (aux) {
        *wet[0] = saturate16(__smmlar(iv0, vol_wet, *wet[0]));
        *wet[1] = saturate16(__smmlar(iv1, vol_wet, *wet[1]));
    }
#endif
}

static ALWAYS_INLINE void envMixerLoop(
    int16_t *in,
    int16_t *dry[2],
    int16_t *wet[2],
    int32_t vols[2][8],
    const int32_t target_s[2],
    const int32_t rate[2],
    const int32_t vol_dry,
    const int32_t vol_wet,
    int nLoops,
    const bool aux
)
{
    do {
        // WYATT_TODO Unroll 8 is about 300us faster in single-threaded mode. Check if this holds up in MT.
        #pragma GCC unroll 0
        for (int j = 0; j < 8; j++, in++, dry[0]++, dry[1]++, wet[0] += aux, wet[1] += aux) {
            const int32_t volume[] = {vols[0][j], vols[1][j]};
            
            #pragma GCC unroll 2
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                if (rate[ch] >= 0x10000)
                    vols[ch][j] = CLAMP_UPPER((((int64_t) vols[ch][j] * rate[ch]) >> 16), target_s[ch]);
                else
                    vols[ch][j] = CLAMP_LOWER((((int64_t) vols[ch][j] * rate[ch]) >> 16), target_s[ch]);
            }

            envMixerProcessOneSample(*in, dry, wet, volume, vol_dry, vol_wet, aux);
        }
    } while (--nLoops > 0);
}

// Crackpipe optimized version
// Optimize at your own risk!
// Channel 0 is left and 1 is right
void aEnvMixerImpl(const uint8_t flags, ENVMIX_STATE state) {
    const bool isInit = flags & A_INIT ? true : false;

    int16_t *in = rspa.buf.as_s16 + rspa.in / sizeof(int16_t);

    int16_t *dry[2] = {rspa.buf.as_s16 + rspa.out / sizeof(int16_t),
                       rspa.buf.as_s16 + rspa.dry_right / sizeof(int16_t)};

    int16_t *wet[2] = {rspa.buf.as_s16 + rspa.wet_left / sizeof(int16_t),
                       rspa.buf.as_s16 + rspa.wet_right / sizeof(int16_t)};

    const int16_t target[2] = {isInit ? rspa.target[0] : state[32],
                               isInit ? rspa.target[1] : state[35]};

    const int32_t target_s[2] = {target[0] << 16,
                                 target[1] << 16};

    const int32_t rate[2] = {isInit ? rspa.rate[0] : (state[33] << 16) | (uint16_t)state[34],
                             isInit ? rspa.rate[1] : (state[36] << 16) | (uint16_t)state[37]};

    const int32_t vol_dry = (isInit ? rspa.vol_dry : state[38]) << MIX_VOLUME_SHIFT,
                  vol_wet = (isInit ? rspa.vol_wet : state[39]) << MIX_VOLUME_SHIFT;

    int32_t vols[2][8];

    if (isInit) {
        const int32_t step_diff[2] = {rspa.vol[0] * (rate[0] - 0x10000) / 8,
                                      rspa.vol[1] * (rate[1] - 0x10000) / 8};

        #pragma GCC unroll 0
        for (int i = 0; i < 8; i++) {
            vols[0][i] = saturate32((int64_t)(rspa.vol[0] << 16) + step_diff[0] * (i + 1));
            vols[1][i] = saturate32((int64_t)(rspa.vol[1] << 16) + step_diff[1] * (i + 1));
        }
    } else {
        memcpy(vols[0], state, 32);
        memcpy(vols[1], state + 16, 32);
    }

    int nLoops = ROUND_UP_16(rspa.nbytes) / (8 * sizeof(int16_t));

    // If Aux is set, we output wet and dry, else only dry.
    // We outline rate to reduce logic within the loop.
    if (flags & A_AUX)
        if (rate[0] >= 0x10000)
            if (rate[1] >= 0x10000)
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_AUX); // ++A
            else
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_AUX); // +-A
        else
            if (rate[1] >= 0x10000)
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_AUX); // -+A
            else
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_AUX); // --A
    else
        if (rate[0] >= 0x10000)
            if (rate[1] >= 0x10000)
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_NORMAL); // ++N
            else
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_NORMAL); // +-N
        else
            if (rate[1] >= 0x10000)
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_NORMAL); // -+N
            else
                envMixerLoop(in, dry, wet, vols, target_s, rate, vol_dry, vol_wet, nLoops, MIX_NORMAL); // --N

    memcpy(state,      vols[0], 32);
    memcpy(state + 16, vols[1], 32);
    state[32] = target[0];
    state[35] = target[1];
    state[33] = (int16_t)(rate[0] >> 16);
    state[34] = (int16_t) rate[0];
    state[36] = (int16_t)(rate[1] >> 16);
    state[37] = (int16_t) rate[1];
    state[38] = vol_dry >> MIX_VOLUME_SHIFT;
    state[39] = vol_wet >> MIX_VOLUME_SHIFT;
}

void aMixImpl(const int16_t gain, const uint16_t in_addr, const uint16_t out_addr) {
    int16_t *in = rspa.buf.as_s16 + in_addr / sizeof(int16_t);
    int16_t *out = rspa.buf.as_s16 + out_addr / sizeof(int16_t);

    // If gain is a specific value, use simplified logic
    if (gain == -0x8000)
        for (int nsamples = ROUND_UP_32(rspa.nbytes) >> 1; nsamples != 0; nsamples--, in++, out++)
            *out = __qsub16(*out, *in);
    
    // Else, use full logic
    else
        for (int nsamples = ROUND_UP_32(rspa.nbytes) >> 1; nsamples != 0; nsamples--, in++, out++)
            *out = saturate16(*out + ((*in * gain) >> 15));
}

// Enables one to inspect the contents of the Emulated RSPA via debugger.
// Use the Snoop Tag to differentiate different calls for breakpoints.
/*
void aSnoop(volatile int snoopTag) {
    UNUSED volatile uint16_t vInOffset = rspa.in;
    UNUSED volatile uint16_t vOutOffset = rspa.out;
    UNUSED volatile int16_t* vIn = rspa.buf.as_s16 + rspa.in;
    UNUSED volatile int16_t* vOut = rspa.buf.as_s16 + rspa.out;
    UNUSED volatile int16_t* vData = rspa.buf.as_s16 + 0x180 / sizeof(uint16_t); // DMEM_ADDR_UNCOMPRESSED_NOTE
    UNUSED volatile uint16_t vNbytes = rspa.nbytes;
    UNUSED volatile uint16_t vDataSize = vOut - vData + (vNbytes / sizeof(uint16_t)); // Total output data size
    UNUSED volatile int16_t vInFirst = vIn[0];
    UNUSED volatile int16_t vOutFirst = vOut[0];

    UNUSED volatile int i = 0;

    if (snoopTag == 0)
        i++;
    
    else if (snoopTag == 1)
        i++;
    
    else if (snoopTag == 2)
        i++;

    else if (snoopTag == 0 || snoopTag == 4) // Multi-chunk sample, pre-copy and post-copy
        i++;

    else if (snoopTag == 0 || snoopTag == 2) // End of processing step and end of note
        i++;

    else if (snoopTag == 0 && vOutFirst != 0) // End of note and non-empty (first valid note)
        i++;
        
    // GDB: p/z *vOut@vNbytes/sizeof(uint16_t)

    // Wildcards (use with conditional breaks in GDB)
    i++;
    i++;
    i++;
    i++;
    i++;
}
*/

#endif
