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

void convert_rgba16_to_rgba32(union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_ia4_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_ia8_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_ia16_to_rgba32(  union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_i4_to_rgba32(    union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_i8_to_rgba32(    union RGBA32* output, const uint8_t* data, uint32_t size_bytes);
void convert_ci4_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t size_bytes, const uint8_t* palette);
void convert_ci8_to_rgba32(   union RGBA32* output, const uint8_t* data, uint32_t size_bytes, const uint8_t* palette);

// CI4 and CI8 palette entries are RGBA16 (5551)
void convert_ci4_to_rgba16(union RGBA16* output, const uint8_t* data, uint32_t size_bytes, const uint8_t* palette);
void convert_ci8_to_rgba16(union RGBA16* output, const uint8_t* data, uint32_t size_bytes, const uint8_t* palette);

#endif
