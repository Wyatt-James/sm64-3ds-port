#include "src/pc/gfx/texture_conversion.h"

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

void convert_i4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t part = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint8_t intensity = part;
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        output->rgba.r = SCALE_4_8(r);
        output->rgba.g = SCALE_4_8(g);
        output->rgba.b = SCALE_4_8(b);
        output->rgba.a = 255;
    }
}

void convert_i8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t intensity = data[i];
        uint8_t r = intensity;
        uint8_t g = intensity;
        uint8_t b = intensity;
        output->rgba.r = r;
        output->rgba.g = g;
        output->rgba.b = b;
        output->rgba.a = 255;
    }
}

void convert_ci4_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t idx = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
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

void convert_ci8_to_rgba32(union RGBA32* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    for (uint32_t i = 0; i < size_bytes; i++, output++) {
        uint8_t idx = data[i];
        uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
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

void convert_ci4_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t idx = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
        uint8_t a = col16 & 1;
        uint8_t r = col16 >> 11;
        uint8_t g = (col16 >> 6) & 0x1f;
        uint8_t b = (col16 >> 1) & 0x1f;
        output->rgba.r = r;
        output->rgba.g = g;
        output->rgba.b = b;
        output->rgba.a = a;
    }
}

void convert_ci8_to_rgba16(union RGBA16* output, uint8_t* data, uint32_t size_bytes, const uint8_t* palette)
{
    for (uint32_t i = 0; i < size_bytes * 2; i++, output++) {
        uint8_t byte = data[i / 2];
        uint8_t idx = (byte >> (4 - (i % 2) * 4)) & 0xf;
        uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
        uint8_t a = col16 & 1;
        uint8_t r = col16 >> 11;
        uint8_t g = (col16 >> 6) & 0x1f;
        uint8_t b = (col16 >> 1) & 0x1f;
        output->rgba.r = r;
        output->rgba.g = g;
        output->rgba.b = b;
        output->rgba.a = a;
    }
}

