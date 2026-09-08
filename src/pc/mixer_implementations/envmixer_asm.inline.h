
/*
 * This file is for including in a C file as an asm statement, inside of a naked function.
 * Settings:
 * - FUNCNAME: required define. Necessary to disambiguate different permutations.
 * - PROCESS_AUX: If defined, the wet channels will be processed.
 * - POSITIVE_N: If defined, channel N is assumed to be increasing and is clamped only at the upper end.
 *   Else, that channel is clamped at the lower end. There are two of these, for channels 0 and 1.
 * All four of these defines #undef'd at the end of the file for convenience.
 */

#ifndef FUNCNAME
#error EnvMixer: FUNCNAME must be defined
#endif

#ifdef POSITIVE_0
#define VOL_ORDER_0(a, b) #a ", " #b
#else
#define VOL_ORDER_0(a, b) #b ", " #a
#endif

#ifdef POSITIVE_1
#define VOL_ORDER_1(a, b) #a ", " #b
#else
#define VOL_ORDER_1(a, b) #b ", " #a
#endif

asm (
    "push {r4, r5, r6, r7, r8, r9, r10, fp, ip, lr}  @ SP head is 40. IP is overwritten to store the end pointer.   \n\t"
    "ldr r4, [sp, #48]                               @ Load nSamples                                                \n\t"
    "add r4, r0, r4, lsl #1                          @ Compute end pointer                                          \n\t"
    "str r4, [sp, #32]                               @ Store end pointer, overwriting IP on the stack               \n\t"
    "mov r5, #0                                      @ R5 = i                                                       \n\t"
    "ldr r6, [sp, #44]                               @ Load data segment pointer                                    \n\t"
#ifdef PROCESS_AUX
    "ldr r4, [sp, #40]                               @ Load wet1 pointer                                            \n\t"
#endif
FUNCNAME ".loop:                                                                                                    \n\t"
    "@ R7 reserved for volume 0                                                                                     \n\t"
    "@ R8 reserved for volume 1                                                                                     \n\t"

    "@ Uses IP, R9, R10, FP, LR                                                                                     \n\t"
    "@ Calculate new volume 1                                                                                       \n\t"
    "add r9, r6, r5, lsl #2                          @ vol1 Calculate offset                                        \n\t"
    "ldr lr, [r6, #76]                               @ vol1 Load rate                                               \n\t"
    "ldr r8, [r9, #32]                               @ vol1 Load volume 1                                           \n\t"
    "ldr r7, [r6, r5, lsl #2]                        @ vol0 Load volume 0                                           \n\t"


    "smull lr, fp, r8, lr                            @ vol1 volume * rate. LR is lo, FP is hi                       \n\t"
    "ldr ip, [r6, #68]                               @ vol1 Load target                                             \n\t"
    "lsr lr, lr, #16                                 @ vol1 LR = the bottom 32 bits of (vol*rate) >> 16             \n\t"
    "orr lr, lr, fp, lsl #16                         @  |                                                           \n\t"
    "cmp "VOL_ORDER_1(ip, lr)"                       @ vol1 result lo - Target                                      \n\t"
    "asr fp, fp, #16                                 @ vol1 FP = top 16 bits of vol * rate                          \n\t"
    "asr r10, ip, #31                                @ vol1 R10 = sign bit of target                                \n\t"
    "sbcs fp, "VOL_ORDER_1(r10, fp)"                 @ vol1 Top 16 bits of vol*rate - Sign bit of target            \n\t"
    "ldr r10, [r6, #72]                              @ vol0 Load rate                                               \n\t"
    "movlt lr, ip                                    @ vol1 Clamp                                                   \n\t"
    "str lr, [r9, #32]                               @ vol1 Store                                                   \n\t"
    "ldr ip, [r6, #64]                               @ vol0 Load target                                             \n\t"

    "@ Uses IP, R10, FP, R9                                                                                         \n\t"
    "@ Calculate new volume 0                                                                                       \n\t"
    "smull r10, fp, r7, r10                          @ vol0 volume * rate. R10 is lo, FP is hi                      \n\t"
    "lsr r10, r10, #16                               @ vol0 R10 = the bottom 32 bits of (vol*rate) >> 16            \n\t"
    "orr r10, r10, fp, lsl #16                       @  |                                                           \n\t"
    "cmp "VOL_ORDER_0(ip, r10)"                      @ vol0 result lo - Target                                      \n\t"
    "asr fp, fp, #16                                 @ vol0 FP = top 16 bits of vol * rate                          \n\t"
    "asr r9, ip, #31                                 @ vol0 R10 = sign bit of target                                \n\t"
    "sbcs fp, "VOL_ORDER_0(r9, fp)"                  @ vol0 Sign bit of target - Top 16 bits of vol*rate            \n\t"
    "ldrsh fp, [r0], #2                              @ iv   Load input, increment                                   \n\t"
    "movlt r10, ip                                   @ vol0 Clamp                                                   \n\t"
    "str r10, [r6, r5, lsl #2]                       @ vol0 Store                                                   \n\t"

    "@ Calculate IV0 and IV1                                                                                        \n\t"
    "ldr r9, [r6, #80]                               @ Load vol_dry. Up here to help with smulbt early reg          \n\t"
    "smulbt r7, fp, r7                               @ R7 = input * volume 0                                        \n\t"
    "smulbt r8, fp, r8                               @ R8 = input * volume 1                                        \n\t"

    "@ Dry processing. Uses R9, IP, LR                                                                              \n\t"
    "@ dryN = sat16((input * volume N) * vol_dry + dryN)                                                            \n\t"
    "ldrsh ip, [r1]                                  @ dry0 load                                                    \n\t"
    "ldrsh lr, [r2]                                  @ dry1 load                                                    \n\t"
    "ldr r10, [sp, #32]                              @ Load end pointer                                             \n\t"
    "smmlar ip, r7, r9, ip                           @ dry0 multiply                                                \n\t"
    "smmlar lr, r8, r9, lr                           @ dry1 multiply                                                \n\t"
    "cmp r10, r0                                     @ Check exit condition                                         \n\t"
    "add r5, r5, #1                                  @ i++ % 8                                                      \n\t"
    "ssat ip, #16, ip                                @ dry0 sat                                                     \n\t"
    "ssat lr, #16, lr                                @ dry1 sat                                                     \n\t"
    "strh ip, [r1], #2 @ movhi                       @ dry0 store                                                   \n\t"
    "strh lr, [r2], #2 @ movhi                       @ dry1 store                                                   \n\t"

#ifdef PROCESS_AUX
    "@ Wet processing. Uses R10, R9, LR                                                                             \n\t"
    "@ wetN = sat16((input * volume N) * vol_wet + wetN)                                                            \n\t"
    "ldr r9, [r6, #84]                               @ Load vol_wet                                                 \n\t"
    "ldrsh r10, [r3]                                 @ wet0 load                                                    \n\t"
    "ldrsh lr, [r4]                                  @ wet1 load                                                    \n\t"
    "smmlar r10, r7, r9, r10                         @ wet0 multiply                                                \n\t"
    "smmlar lr, r8, r9, lr                           @ wet1 multiply                                                \n\t"
    "and r5, r5, #7                                  @                                                              \n\t"
    "ssat r10, #16, r10                              @ wet0 sat                                                     \n\t"
    "ssat lr, #16, lr                                @ wet1 sat                                                     \n\t"
    "strh r10, [r3], #2 @ movhi                      @ wet0 store                                                   \n\t"
    "strh lr, [r4], #2 @ movhi                       @ wet1 store                                                   \n\t"
#else
    "and r5, r5, #7                                  @                                                              \n\t"
#endif

    "@ Loop or exit                                                                                                 \n\t"
    "bhi " FUNCNAME ".loop                           @                                                              \n\t"
    "pop {r4, r5, r6, r7, r8, r9, r10, fp, ip, pc}   @                                                              \n\t"
);

#undef POSITIVE_0
#undef POSITIVE_1
#undef VOL_ORDER_0
#undef VOL_ORDER_1
#undef PROCESS_AUX

#undef r0
#undef r1
#undef r2
#undef r3
#undef r4
#undef r5
#undef r6
#undef FUNCNAME
