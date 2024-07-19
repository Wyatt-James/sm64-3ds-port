#ifndef TEXTURE_CONVERSION_H
#define TEXTURE_CONVERSION_H

#include <stdint.h>

#include "color_formats.h"

/*
 * Thanks to the PC port team for most of the conversion functions and macros.
 */

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_5_8(VAL_) (((VAL_) * 0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_) * 0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_) * 0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)
#define SCALE_3_4(VAL_) ((VAL_) * 2)
#define SCALE_4_3(VAL_) ((VAL_) / 2)

// SCALE_M_N_ACCURATE: upscale/downscale M-bit integer to N-bit with higher accuracy (maintains min and max)
#define SCALE_3_8_ACCURATE(VAL_) (((VAL_) * 255) / 0b111)
#define SCALE_3_4_ACCURATE(VAL_) (((VAL_) * 105) / 0b111)

// RGBA32 conversions
void convert_rgba16_to_rgba32(union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_ia4_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_ia8_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_ia16_to_rgba32(  union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_i4_to_rgba32(    union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_i8_to_rgba32(    union RGBA32* output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_ci4_to_rgba32(   union RGBA32* output, const uint8_t* data, const uint8_t* palette, uint32_t width, uint32_t height);
void convert_ci8_to_rgba32(   union RGBA32* output, const uint8_t* data, const uint8_t* palette, uint32_t width, uint32_t height);

// Palletized conversions
// CI4 and CI8 palette entries are RGBA16 (5551)
void convert_ci4_to_rgba16(union RGBA16* output, const uint8_t* data, const uint8_t* palette, uint32_t width, uint32_t height);
void convert_ci8_to_rgba16(union RGBA16* output, const uint8_t* data, const uint8_t* palette, uint32_t width, uint32_t height);

// Special IA4 conversions
void convert_ia4_to_ia8( union IA8*  output, const uint8_t* data, uint32_t width, uint32_t height);
void convert_ia4_to_ia16(union IA16* output, const uint8_t* data, uint32_t width, uint32_t height);

#endif
