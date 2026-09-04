#pragma once

#include "src/pc/pc_macros.h"

// Additional ARMv6k intrinsics that GCC/arm_acle.h don't have

// 32-bit multiply, round top 32 bits and accumulate ((round(a * b) >> 32) + acc)
static ALWAYS_INLINE int32_t __smmlar(int32_t a, int32_t b, int32_t acc)
{
    int32_t ret;
    asm (
        "smmlar %0, %1, %2, %3\n\t"
        : "=r" (ret)
        : "r" (a), "r" (b), "r" (acc)
    );
    return ret;
}

// 16-bit a * b, 32-bit result
static ALWAYS_INLINE int32_t __smulbb(int32_t a, int32_t b)
{
    int32_t ret;
    asm (
        "smulbb %0, %1, %2\n\t"
        : "=r" (ret)
        : "r" (a), "r" (b)
    );
    return ret;
}

// 16-bit a * (b >> 16), 32-bit result
static ALWAYS_INLINE int32_t __smulbt(int32_t a, int32_t b)
{
    int32_t ret;
    asm (
        "smulbt %0, %1, %2\n\t"
        : "=r" (ret)
        : "r" (a), "r" (b)
    );
    return ret;
}

// top half of A | bottom half of rightshifted B
static ALWAYS_INLINE int32_t __pkhtb(int32_t a, int32_t b, int32_t rshift)
{
    int32_t ret;
    asm (
        "pkhtb %0, %1, %2, asr %3\n\t"
        : "=r" (ret)
        : "r" (a), "r" (b), "i" (rshift)
    );
    return ret;
}

// bottom half of A | top half of leftshifted B
static ALWAYS_INLINE int32_t __pkhbt(int32_t a, int32_t b, int32_t lshift)
{
    int32_t ret;
    asm (
        "pkhbt %0, %1, %2, lsl %3\n\t"
        : "=r" (ret)
        : "r" (a), "r" (b), "i" (lshift)
    );
    return ret;
}
