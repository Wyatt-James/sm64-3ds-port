#pragma once

/*
 * Various macros that don't live in files predating the 21st century.
 * Notably, these macros will replace other definitions!
 */

#include <stddef.h> // size_t

// I'm making an assumption that these implementations are both correct and
// more desirable than whatever may have happened to be declared prior.
#undef MAX
#undef MIN
#undef CLAMP
#undef ENUM_CLAMP
#undef ARRAY_COUNT
#undef BIT
#undef SET_BITS
#undef BSWAP_32
#undef BSWAP_16
#undef NUM_LEADING_ZEROES
#undef NUM_ONES
#undef VARARGS_COUNT
#undef NTSC_FRAMERATE
#undef ROUND_UP
#undef KIB_TO_BYTE
#undef MIB_TO_BYTE
#undef BYTE_TO_KIB
#undef BYTE_TO_MIB
#undef UNUSED
#undef USED
#undef ALWAYS_INLINE
#undef ALIGNED
#undef HOT
#undef COLD
#undef ASSUME
#undef LIKELY
#undef UNLIKELY
#undef EXPECT

// General macros
#define MAX(a_, b_) ((a_) > (b_) ? (a_) : (b_)) // Returns the higher of a_ and b_.
#define MIN(a_, b_) ((a_) < (b_) ? (a_) : (b_)) // Returns the lower of a_ and b_.
#define CLAMP(v_, min_, max_) MAX(MIN((v_), (max_)), (min_)) // Returns v_ if it is in range [min,max], else returns the nearest in-range value.
#define ENUM_CLAMP(v_, max_) (((v_) <= 0 || (v_) >= (max_)) ? 0 : (v_)) // Returns v_ if it is in range [0, max), else returns 0.
#define ARRAY_COUNT(arr_) (size_t)(sizeof(arr_) / sizeof(arr_[0])) // Returns the number of elements in arr_.
#define BIT(n_) (1U << (n_)) // Returns an integer with its Nth bit set.
#define SET_BITS(num_bits_) ((1U << (num_bits_)) - 1) // Returns an unsigned integer with its N least significant bits set. For example, SET_BITS(2) == 0b11.
#define BSWAP_32(v_) (__builtin_bswap32(v_)) // Reverses the byte order of a 32-bit uint.
#define BSWAP_16(v_) (__builtin_bswap16(v_)) // Reverses the byte order of a 16-bit uint.
#define NUM_LEADING_ZEROES(v_) (__builtin_clz(v_)) // Returns the number of leading zeroes of a 32-bit uint.
#define NUM_ONES(v_) (__builtin_popcount(v_)) // Returns the number of 1-bits in a 32-bit uint.
#define VARARGS_COUNT(type, ...) (sizeof(((type)[]){__VA_ARGS__})/sizeof((type))) // Returns the count of a variadic macro's varargs
#define NTSC_FRAMERATE(fps_) ((float) (fps_) * (1000.0f / 1001.0f)) // Converts a framerate to its NTSC 1000/1001 equivalent.
#define ROUND_UP(round_, val_) (((val_) + (round_)) & ~(round_))

// Size calculations
#define KIB_TO_BYTE(n_) (1024UL * n_)
#define MIB_TO_BYTE(n_) (1024UL * 1024UL * n_)
#define BYTE_TO_KIB(n_) (n_ / 1024UL)
#define BYTE_TO_MIB(n_) (n_ / (1024UL * 1024UL))

// Attributes
#define UNUSED __attribute__((unused))
#define USED __attribute__((used))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define ALIGNED(n_) __attribute__((aligned(n_)))
#define HOT __attribute__ ((hot))
#define COLD __attribute__ ((cold))

// Compiler hints
#define ASSUME(cond_) if (!(cond_)) __builtin_unreachable()
#define LIKELY(cond_) __builtin_expect(!!(cond_), 1)
#define UNLIKELY(cond_)  __builtin_expect(!!(cond_), 0)
#define EXPECT(val_, expected_) __builtin_expect(val_, expected_)
