/*
 * This file is a stub, and selects from one of the files
 * in the mixer_implementations directory.
 * 
 *               ----- Important -----
 *  These files must not be included in the build path!
 *  If they are present, compilation errors will abound.
 */


// If set to use reference RSPA, force it.
#if defined RSPA_USE_REFERENCE_IMPLEMENTATION
#include "src/pc/mixer_implementations/mixer_reference.c"

// x86 SSE4.1 support
#elif defined __SSE4_1__
#include "src/pc/mixer_implementations/mixer_sse41.c"

// ARM Neon support
#elif defined __ARM_NEON
#include "src/pc/mixer_implementations/mixer_neon.c"

// Optimized for N3DS, supports ENHANCED_RSPA_EMULATION.
#elif defined TARGET_N3DS
#include "src/pc/mixer_implementations/mixer_3ds.c"

// Fall back to reference RSPA if no special versions are available.
#else
#include "src/pc/mixer_implementations/mixer_reference.c"
#endif