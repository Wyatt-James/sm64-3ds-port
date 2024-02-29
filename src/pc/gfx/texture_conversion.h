#ifndef TEXTURE_CONVERSION_H
#define TEXTURE_CONVERSION_H

#include <stdint.h>

/*
 * Thanks to n64squid.com for the color format specifications.
 * Thanks to the PC port team for most of the conversion functions and macros.
 */

// One 32-bit RGBA color
union RGBA32 {
    struct {
        uint8_t r, g, b, a;
    } rgba;
    uint32_t u32;
};

// One 16-bit RGBA color
union RGBA16 {
    struct {
        uint8_t r : 5;
        uint8_t g : 5;
        uint8_t b : 5;
        uint8_t a : 1;
    } rgba;
    uint16_t u16;
};

// Two 16-bit YUV colors
union YUV16x2 {
    struct {
        uint8_t y1;
        uint8_t y2;
        uint8_t u;
        uint8_t v;
    } yuv;
    uint32_t u32;
};

// One 8-bit-indexed color
union CI8 {
    struct {
        uint8_t i;
    } index;
    uint8_t u8;
};

// Two 4-bit-indexed colors
union CI4x2 {
    struct {
        uint8_t i1 : 4;
        uint8_t i2 : 4;
    } index;
    uint8_t u8;
};

// One 16-bit intensity/alpha color
union IA16 {
    struct {
        uint8_t i;
        uint8_t a;
    } intensity;
    uint16_t u16;
};

// One 8-bit intensity color
union I8 {
    struct {
        uint8_t i;
    } intensity;
    uint8_t u8;
};

// One 8-bit intensity/alpha color
union IA8 {
    struct {
        uint8_t i : 4;
        uint8_t a : 4;
    } intensity;
    uint8_t u8;
};

// Two 4-bit intensity colors
union I4x2 {
    struct {
        uint8_t i1 : 4;
        uint8_t i2 : 4;
    } intensity;
    uint8_t u8;
};

// Two 4-bit intensity/alpha colors
union IA4x2 {
    struct {
        uint8_t i1 : 3;
        uint8_t a1 : 1;
        uint8_t i2 : 3;
        uint8_t a2 : 1;
    } intensity;
    uint8_t u8;
};

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_5_8(VAL_) (((VAL_) * 0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_) * 0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_) * 0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

void convert_rgba16_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_ia4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_ia8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_ia16_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_i4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_i8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes);
void convert_ci4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette);
void convert_ci8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette);

// CI4 and CI8 palette entries are RGBA16 (5551)
void convert_ci4_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette);
void convert_ci8_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette);

/*
 *  Color formats, for easy copy-pasta:
 *  RGBA16
 *  RGBA32
 *  IA4
 *  IA8
 *  IA16
 *  I4
 *  I8
 *  CI4
 *  CI8
 
 *  rgba16
 *  rgba32
 *  ia4
 *  ia8
 *  ia16
 *  i4
 *  i8
 *  ci4
 *  ci8
 */

#endif
