#ifndef COLOR_FORMATS_H
#define COLOR_FORMATS_H

#include <stdint.h>

/*
 * Thanks to n64squid.com for the color format specifications.
 */

// One 32-bit RGBA color
union RGBA32 {
    struct {
        uint8_t r, g, b, a;
    };
    uint32_t u32;
};

// One 16-bit RGBA color. Uses u16 bitfields because u8s bloat the struct size to 4U.
union RGBA16 {
    struct {
        uint16_t r : 5;
        uint16_t g : 5;
        uint16_t b : 5;
        uint16_t a : 1;
    };
    uint16_t u16;
};

// Two 16-bit YUV colors
union YUV16x2 {
    struct {
        uint8_t y1;
        uint8_t y2;
        uint8_t u;
        uint8_t v;
    };
    uint32_t u32;
};

// One 8-bit-indexed color
union CI8 {
    struct {
        uint8_t i;
    };
    uint8_t u8;
};

// Two 4-bit-indexed colors
union CI4x2 {
    struct {
        uint8_t i1 : 4;
        uint8_t i2 : 4;
    };
    uint8_t u8;
};

// One 16-bit intensity/alpha color
union IA16 {
    struct {
        uint8_t i;
        uint8_t a;
    };
    uint16_t u16;
};

// One 8-bit intensity color
union I8 {
    struct {
        uint8_t i;
    };
    uint8_t u8;
};

// One 8-bit intensity/alpha color
union IA8 {
    struct {
        uint8_t i : 4;
        uint8_t a : 4;
    };
    uint8_t u8;
};

// Two 4-bit intensity colors
union I4x2 {
    struct {
        uint8_t i1 : 4;
        uint8_t i2 : 4;
    };
    uint8_t u8;
};

// Two 4-bit intensity/alpha colors
union IA4x2 {
    struct {
        uint8_t i1 : 3;
        uint8_t a1 : 1;
        uint8_t i2 : 3;
        uint8_t a2 : 1;
    };
    uint8_t u8;
};

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
