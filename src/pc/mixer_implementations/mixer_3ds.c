#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "macros.h"

/*
 * 3DS-optimized mixer.c software implementation.
 * Enhanced RSPA emulation is supported.

 * Enhanced RSPA emulation allows us to break the rules of
 * RSPA emulation a little bit for better performance.
 */

#pragma GCC optimize ("unroll-loops")

#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)

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

    int16_t adpcm_table[8][2][8];
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

#define CLAMP16_LOWER_T -0x8000
#define CLAMP16_UPPER_T 0x7fff
#define CLAMP32_LOWER_T -0x7fffffff - 1 // Must remain this way or the compiler goofs
#define CLAMP32_UPPER_T 0x7fffffff

// clamps an int32_t to an int16_t, between CLAMP16_LOWER_T and CLAMP16_UPPER_T.
static inline int16_t clamp16(int32_t v) {
    if (v < CLAMP16_LOWER_T)
        return CLAMP16_LOWER_T;

    else if (v > CLAMP16_UPPER_T)
        return CLAMP16_UPPER_T;

    return (int16_t)v;
}

// clamps an int64_t to an int32_t, between CLAMP32_LOWER_T and CLAMP32_UPPER_T.
static inline int32_t clamp32(int64_t v) {
    if (v < CLAMP32_LOWER_T)
        return CLAMP32_LOWER_T;

    else if (v > CLAMP32_UPPER_T)
        return CLAMP32_UPPER_T;

    return (int32_t)v;
}

// Clamps an int32_t on the positive end, with the provided threshold.
// Do not forget to cast after using both clamping functions!
static inline int32_t clamp16_upper_t(const int32_t v, const int32_t thresh) {
    if (v > thresh)
        return thresh;

    return v;
}

// Clamps an int32_t on the negative end, with the provided threshold.
// Do not forget to cast after using both clamping functions!
static inline int32_t clamp16_lower_t(const int32_t v, const int32_t thresh) {
    if (v < thresh)
        return thresh;

    return v;
}

// Clamps an int64_t on the positive end, with the provided threshold.
// Do not forget to cast after using both clamping functions!
static inline int64_t clamp32_upper_t(const int64_t v, const int64_t thresh) {
    if (v > thresh)
        return thresh;

    return v;
}

// Clamps an int64_t on the negative end, with threshold CLAMP16_LOWER_T.
// Do not forget to cast after using both clamping functions!
static inline int64_t clamp32_lower_t(const int64_t v, const int64_t thresh) {
    if (v < thresh)
        return thresh;

    return v;
}

// Clamps an int32_t on the positive end, with threshold CLAMP16_UPPER_T.
// Do not forget to cast after using both clamping functions!
static inline int32_t clamp16_upper(const int32_t v) {
    return clamp16_upper_t(v, CLAMP16_UPPER_T);
}

// Clamps an int32_t on the positive end, with threshold CLAMP16_LOWER_T.
// Do not forget to cast after using both clamping functions!
static inline int32_t clamp16_lower(const int32_t v) {
    return clamp16_lower_t(v, CLAMP16_LOWER_T);
}


// Clamps an int64_t on the positive end, with threshold CLAMP32_UPPER_T.
// Do not forget to cast after using both clamping functions!
static inline int64_t clamp32_upper(const int64_t v) {
    return clamp32_upper_t(v, CLAMP32_UPPER_T);
}

// Clamps an int64_t on the negative end, with threshold CLAMP32_LOWER_T.
// Do not forget to cast after using both clamping functions!
static inline int64_t clamp32_lower(const int64_t v) {
    return clamp32_lower_t(v, CLAMP32_LOWER_T);
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

void aLoadADPCMImpl(int num_entries_times_16, const int16_t *book_source_addr) {
    memcpy(rspa.adpcm_table, book_source_addr, num_entries_times_16);
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
static void aInterleaveInternal(int16_t* l, int16_t* r, int16_t* dest, const int count) {
    for (int i = count * 8; i != 0; i--) {
        *dest++ = *l++;
        *dest++ = *r++;
    }
}

// Interleaves RSPA NBYTES bytes into RSPA OUT
void aInterleaveImpl(uint16_t left, uint16_t right) {
    const int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);
    int16_t *d = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);

    aInterleaveInternal(l, r, d, count);
}

// Interleaves RSPA NBYTES bytes into the provided buffer
void aInterleaveAndCopyImpl(uint16_t left, uint16_t right, int16_t *dest_addr) {
    const int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);

    aInterleaveInternal(l, r, dest_addr, count);
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(rspa.buf.as_u8 + out_addr, rspa.buf.as_u8 + in_addr, nbytes);
}

void aSetLoopImpl(ADPCM_STATE *adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}

// Internal use only
// ADPCM_STATE is a pointer type! Specifically a short[16].
/*
 * ADPCM packet format:
 *  9 bytes length
 *  Byte 1, upper nibble: shift magnitude, range [0-12]
 *  Byte 1, lower nibble: table index, range [0-7]
 *  Bytes 2-9: data
 * 
 * Each ADPCM packet produces 16 PCM samples.
 * Each PCM sample produced depends on the prior two PCM samples.
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
        const int shift = *in >> 4; // should be in 0..12
        const int table_index = *in++ & 0xf; // should be in 0..7
        int16_t (*tbl)[8] = rspa.adpcm_table[table_index];

        // Decompress 16 PCM samples from 9 bytes ADPCM
        for (int i = 0; i < 2; i++) {
            int16_t ins[8];
            const int16_t prev1 = out[-1];
            const int16_t prev2 = out[-2];

            // Load 4 bytes from *in (total of 8)
            for (int j = 0; j < 4; j++) {
                ins[j * 2] = (((*in >> 4) << 28) >> 28) << shift;
                ins[j * 2 + 1] = (((*in++ & 0xf) << 28) >> 28) << shift;
            }

            for (int j = 0; j < 8; j++, out++) {
                int32_t acc = tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);

                for (int k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }

                const int32_t sample = clamp16_lower(acc >> 11);
                *out = (int16_t) clamp16_upper(sample);
            }
        }
        nbytes -= 16 * sizeof(int16_t);
    }
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
    
    // Round up, and divide by 2 for byte count. If RSPA.nbytes == 0, do 8 samples for do-while compensation.
    for (int nSamples = rspa.nbytes == 0 ? 8 : (ROUND_UP_16(rspa.nbytes) / sizeof(uint16_t)); nSamples != 0; nSamples--, out++) {
        const int16_t* const tbl = resample_table[(pitch_accumulator << 6) >> 16];
        const int32_t sample_temp = clamp16_lower(((in[0] * tbl[0] + 0x4000) >> 15) +
                                                    ((in[1] * tbl[1] + 0x4000) >> 15) +
                                                    ((in[2] * tbl[2] + 0x4000) >> 15) +
                                                    ((in[3] * tbl[3] + 0x4000) >> 15));

        *out = (int16_t) clamp16_upper(sample_temp);

        pitch_accumulator += double_pitch;
        in += pitch_accumulator >> 16;
        pitch_accumulator %= 0x10000;
    }

    state[4] = (int16_t) pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));

    int unknown = (in - in_initial + 4) & 7;
    in -= unknown;

    if (unknown != 0)
        unknown = -8 - unknown;

    state[5] = unknown;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}

static inline int32_t envMixerGetVolume(const int32_t rate, const int32_t volume, const int16_t target) {

    // Increasing volume
    if ((rate >> 16) > 0) {
        if ((volume >> 16) > target) {
            return target << 16;
        }
    }
    
    // Decreasing volume
    else {
        if ((volume >> 16) < target) {
            return target << 16;
        }
    }

    return volume;
}

// Crackpipe optimized version
// Optimize at your own risk!
void aEnvMixerImpl(const uint8_t flags, ENVMIX_STATE state) {
    const bool isInit = flags & A_INIT ? true : false;

    int16_t *in = rspa.buf.as_s16 + rspa.in / sizeof(int16_t);

    int16_t *dry_0 = rspa.buf.as_s16 + rspa.out / sizeof(int16_t),
            *dry_1 = rspa.buf.as_s16 + rspa.dry_right / sizeof(int16_t);

    int16_t *wet_0 = rspa.buf.as_s16 + rspa.wet_left / sizeof(int16_t),
            *wet_1 = rspa.buf.as_s16 + rspa.wet_right / sizeof(int16_t);

    const int16_t target_0 = isInit ? rspa.target[0] : state[32],
                  target_1 = isInit ? rspa.target[1] : state[35];

    const int32_t rate_0 = isInit ? rspa.rate[0] : (state[33] << 16) | (uint16_t)state[34],
                  rate_1 = isInit ? rspa.rate[1] : (state[36] << 16) | (uint16_t)state[37];

    const int16_t vol_dry = isInit ? rspa.vol_dry : state[38],
                  vol_wet = isInit ? rspa.vol_wet : state[39];

    int32_t vols_0[8], vols_1[8];

    if (isInit) {
        const int32_t step_diff[2] = {
            rspa.vol[0] * (rate_0 - 0x10000) / 8,
            rspa.vol[0] * (rate_1 - 0x10000) / 8
        };

        for (int i = 0; i < 8; i++) {
            vols_0[i] = clamp32((int64_t)(rspa.vol[0] << 16) + step_diff[0] * (i + 1));
            vols_1[i] = clamp32((int64_t)(rspa.vol[1] << 16) + step_diff[1] * (i + 1));
        }
    } else {
        memcpy(vols_0, state, 32);
        memcpy(vols_1, state + 16, 32);
    }

    // Round up, and divide by 2 for byte count. If RSPA.nbytes == 0, do 8 samples for do-while compensation.
    const int nSamples = rspa.nbytes == 0 ? 8 : (ROUND_UP_16(rspa.nbytes) / sizeof(uint16_t));

    // If we have the AUX flag set, use both the wet and dry channels.
    if (flags & A_AUX) {
        for (int i = 0, index = 0; i < nSamples; i++, in++, wet_0++, wet_1++, dry_0++, dry_1++, index = i & 7) {
            const int32_t volume_0 = envMixerGetVolume(rate_0, vols_0[index], target_0),
                          volume_1 = envMixerGetVolume(rate_1, vols_1[index], target_1);
            
            {
                // These are done in batches, and clamping in two parts, to reduce branching, which helps performance slightly.
                // Only clamping on the upper end seems to work for volume, but I'll leave both ends on for now.
                const int64_t vol_0_temp = clamp32_lower((((int64_t) volume_0) * rate_0) >> 16),
                              vol_1_temp = clamp32_lower((((int64_t) volume_1) * rate_1) >> 16);

                vols_0[index] = (int32_t) clamp32_upper(vol_0_temp);
                vols_1[index] = (int32_t) clamp32_upper(vol_1_temp);
            }

            const int32_t volume_0_s = volume_0 >> 16,
                          volume_1_s = volume_1 >> 16;

            {
                // These are done in batches, and clamping in two parts, to reduce branching, which helps performance slightly.
                // Thanks to michi for optimizing the underlying math here.
                const int16_t in_val = *in;
                const int32_t dry_temp_0 = clamp16_lower((((*dry_0 << 15) - *dry_0 + in_val * ((volume_0_s * vol_dry) >> 15) + 1) >> 15) + 1),
                              dry_temp_1 = clamp16_lower((((*dry_1 << 15) - *dry_1 + in_val * ((volume_1_s * vol_dry) >> 15) + 1) >> 15) + 1),
                              wet_temp_0 = clamp16_lower((((*wet_0 << 15) - *wet_0 + in_val * ((volume_0_s * vol_wet) >> 15) + 1) >> 15) + 1),
                              wet_temp_1 = clamp16_lower((((*wet_1 << 15) - *wet_1 + in_val * ((volume_1_s * vol_wet) >> 15) + 1) >> 15) + 1);
                
                *dry_0 = (int16_t) clamp16_upper(dry_temp_0);
                *dry_1 = (int16_t) clamp16_upper(dry_temp_1);
                *wet_0 = (int16_t) clamp16_upper(wet_temp_0);
                *wet_1 = (int16_t) clamp16_upper(wet_temp_1);
            }
        }
    }
    
    // Else if we do NOT have the AUX flag set, use only the dry channel
    else {
        for (int i = 0, index = 0; i < nSamples; i++, in++, dry_0++, dry_1++, index = i & 7) {
            const int32_t volume_0 = envMixerGetVolume(rate_0, vols_0[index], target_0),
                          volume_1 = envMixerGetVolume(rate_1, vols_1[index], target_1);
            
            {
                // These are done in batches, and clamping in two parts, to reduce branching, which helps performance slightly.
                // Only clamping on the upper end seems to work for volume, but I'll leave both ends on for now.
                const int64_t vol_0_temp = clamp32_lower((((int64_t) volume_0) * rate_0) >> 16),
                              vol_1_temp = clamp32_lower((((int64_t) volume_1) * rate_1) >> 16);

                vols_0[index] = (int32_t) clamp32_upper(vol_0_temp);
                vols_1[index] = (int32_t) clamp32_upper(vol_1_temp);
            }

            const int32_t volume_0_s = volume_0 >> 16,
                          volume_1_s = volume_1 >> 16;

            {
                // These are done in batches, and clamping in two parts, to reduce branching, which helps performance slightly.
                // Thanks to michi for optimizing the underlying math here.
                const int16_t in_val = *in;
                const int32_t dry_temp_0 = clamp16_lower((((*dry_0 << 15) - *dry_0 + in_val * ((volume_0_s * vol_dry) >> 15) + 1) >> 15) + 1),
                              dry_temp_1 = clamp16_lower((((*dry_1 << 15) - *dry_1 + in_val * ((volume_1_s * vol_dry) >> 15) + 1) >> 15) + 1);
                          
                *dry_0 = (int16_t) clamp16_upper(dry_temp_0);
                *dry_1 = (int16_t) clamp16_upper(dry_temp_1);
            }
        }
    }

    memcpy(state, vols_0, 32);
    memcpy(state + 16, vols_1, 32);
    state[32] = target_0;
    state[35] = target_1;
    state[33] = (int16_t)(rate_0 >> 16);
    state[34] = (int16_t) rate_0;
    state[36] = (int16_t)(rate_1 >> 16);
    state[37] = (int16_t) rate_1;
    state[38] = vol_dry;
    state[39] = vol_wet;
}

void aMixImpl(const int16_t gain, const uint16_t in_addr, const uint16_t out_addr) {
    int16_t *in = rspa.buf.as_s16 + in_addr / sizeof(int16_t);
    int16_t *out = rspa.buf.as_s16 + out_addr / sizeof(int16_t);

    // If gain is a specific value, use simplified logic
    if (gain == -0x8000)
        for (int nsamples = ROUND_UP_32(rspa.nbytes) >> 1; nsamples != 0; nsamples--, in++, out++) {
            const int32_t sample = clamp16_lower(*out - *in);
            *out = (int16_t) clamp16_upper(sample);
        }
    
    // Else, use full logic
    else
        for (int nsamples = ROUND_UP_32(rspa.nbytes) >> 1; nsamples != 0; nsamples--, in++, out++) {
            const int32_t sample = clamp16_lower(((*out * 0x7fff + *in * gain) + 0x4000) >> 15);
            *out = (int16_t) clamp16_upper(sample);
        }
}

// Enables one to inspect the contents of the Emulated RSPA via debugger.
// Use the Snoop Tag to differentiate different calls for breakpoints.
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