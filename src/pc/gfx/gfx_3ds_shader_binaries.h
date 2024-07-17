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

extern const uint8_t emu64_shbin[];
extern const uint32_t emu64_shbin_size;

#endif
