#ifndef GFX_3DS_SHADER_BINARIES_H
#define GFX_3DS_SHADER_BINARIES_H

/*
 * This file should ONLY contain SHBIN references, as provided by bin2s.
 * You should not use this file directly under most circumstances.
 * 
 * In case anybody gets confused, bin2s outputs .o files directly into the
 * build/src directory, and these .o files simply contain binary representations
 * of their files, with variable names adapted from the input file's name as
 * seen below.
 */

#include <stdint.h>

extern const uint8_t shader_shbin[];
extern const uint32_t shader_shbin_size;

extern const uint8_t shader_1_shbin[];
extern const uint32_t shader_1_shbin_size;
// extern const uint8_t shader_3_shbin[]; // Disabled for fog
// extern const uint32_t shader_3_shbin_size;
extern const uint8_t shader_4_shbin[];
extern const uint32_t shader_4_shbin_size;
extern const uint8_t shader_5_shbin[];
extern const uint32_t shader_5_shbin_size;
// extern const uint8_t shader_6_shbin[]; // Disabled for fog
// extern const uint32_t shader_6_shbin_size;
// extern const uint8_t shader_7_shbin[]; // Disabled for fog
// extern const uint32_t shader_7_shbin_size;
extern const uint8_t shader_8_shbin[];
extern const uint32_t shader_8_shbin_size;
extern const uint8_t shader_9_shbin[];
extern const uint32_t shader_9_shbin_size;
extern const uint8_t shader_20_shbin[];
extern const uint32_t shader_20_shbin_size;
extern const uint8_t shader_41_shbin[];
extern const uint32_t shader_41_shbin_size;

#endif
