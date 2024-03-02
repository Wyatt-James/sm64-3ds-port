#include "platform_info.h"
#include "texture_conversion.h"

#if IS_BIG_ENDIAN == 1
#define BIG_ENDIAN_LOAD(v_) (v_)
#else
#define BIG_ENDIAN_LOAD(v_) (__builtin_bswap16(v_))
#endif

void convert_rgba16_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes / 2; i++, output++) {
        uint16_t col16 = (data[2 * i] << 8) | data[2 * i + 1];
        uint8_t a = col16 & 1;
        uint8_t r = col16 >> 11;
        uint8_t g = (col16 >> 6) & 0x1f;
        uint8_t b = (col16 >> 1) & 0x1f;
        output->rgba.r = SCALE_5_8(r);
        output->rgba.g = SCALE_5_8(g);
        output->rgba.b = SCALE_5_8(b);
        output->rgba.a = a ? 255 : 0;
    }
}


void convert_ia4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t part = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint8_t intensity = part >> 1;
        uint8_t alpha = part & 1;
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        output->rgba.r = SCALE_3_8(r);
        output->rgba.g = SCALE_3_8(g);
        output->rgba.b = SCALE_3_8(b);
        output->rgba.a = alpha ? 255 : 0;
    }
}

void convert_ia8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t intensity = data[i] >> 4;
        uint8_t alpha = data[i] & 0xf;
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        output->rgba.r = SCALE_4_8(r);
        output->rgba.g = SCALE_4_8(g);
        output->rgba.b = SCALE_4_8(b);
        output->rgba.a = SCALE_4_8(alpha);
    }
}

void convert_ia16_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes / 2; i++, output++) {
        uint8_t intensity = data[2 * i];
        uint8_t alpha = data[2 * i + 1];
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        output->rgba.r = r;
        output->rgba.g = g;
        output->rgba.b = b;
        output->rgba.a = alpha;
    }
}

// Unused by SM64
void convert_i4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes; i++, output += 2) {
        uint8_t byte = data[i];
        union I4x2 i4x2 = (union I4x2) byte; // Reverse order? Why?
        output[0].rgba.r = SCALE_4_8(i4x2.intensity.i2);
        output[0].rgba.g = SCALE_4_8(i4x2.intensity.i2);
        output[0].rgba.b = SCALE_4_8(i4x2.intensity.i2);
        output[0].rgba.a = 255;
        output[1].rgba.r = SCALE_4_8(i4x2.intensity.i1);
        output[1].rgba.g = SCALE_4_8(i4x2.intensity.i1);
        output[1].rgba.b = SCALE_4_8(i4x2.intensity.i1);
        output[1].rgba.a = 255;
    }
}

// Unused by SM64
void convert_i8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t intensity = data[i]; // Not gonna bother using the struct here
        output->rgba.r = intensity;
        output->rgba.g = intensity;
        output->rgba.b = intensity;
        output->rgba.a = 255;
    }
}

// WYATT_TODO fix up this loop to use CI4x2
// Unused by SM64
void convert_ci4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    const uint16_t* palette_as_u16 = (uint16_t*) palette;
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t idx = (byte >> (4 - (i % 2) * 4)) & 0xf;
        union RGBA16 col = (union RGBA16) BIG_ENDIAN_LOAD(palette_as_u16[idx]);
        output->rgba.r = SCALE_5_8(col.rgba.r);
        output->rgba.g = SCALE_5_8(col.rgba.g);
        output->rgba.b = SCALE_5_8(col.rgba.b);
        output->rgba.a = col.rgba.a ? 255 : 0;
    }
}

// Unused by SM64
void convert_ci8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    const uint16_t* palette_as_u16 = (uint16_t*) palette;
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t idx = data[i];
        union RGBA16 col = (union RGBA16) BIG_ENDIAN_LOAD(palette_as_u16[idx]);
        output->rgba.r = SCALE_5_8(col.rgba.r);
        output->rgba.g = SCALE_5_8(col.rgba.g);
        output->rgba.b = SCALE_5_8(col.rgba.b);
        output->rgba.a = col.rgba.a ? 255 : 0;
    }
}

// WYATT_TODO fix up this loop to use CI4x2
// Unused by SM64
void convert_ci4_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    const uint16_t* palette_as_u16 = (uint16_t*) palette;
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t idx = (byte >> (4 - (i % 2) * 4)) & 0xf;
        *output = (union RGBA16) BIG_ENDIAN_LOAD(palette_as_u16[idx]);
    }
}

// Unused by SM64
void convert_ci8_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    const uint16_t* palette_as_u16 = (uint16_t*) palette;
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t idx = data[i];
        *output = (union RGBA16) BIG_ENDIAN_LOAD(palette_as_u16[idx]);
    }
}

