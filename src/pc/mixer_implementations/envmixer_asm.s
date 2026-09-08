/*
 * This file is for including inside of another assembly file, or for copying its body into different
 * assembly files and setting the following flags:
 * Settings:
 * - FUNCNAME: required define. Necessary to disambiguate different permutations.
 * - PROCESS_AUX: If defined, the wet channels will be processed.
 * - POSITIVE_N: If defined, channel N is assumed to be increasing and is clamped only at the upper end.
 *   Else, that channel is clamped at the lower end. There are two of these, for channels 0 and 1.
 * All four of these defines #undef'd at the end of the file for convenience.
 */

#define rINPT r0
#define rDRY0 r1
#define rDRY1 r2
#define rWET0 r3
#define rWET1 r4
#define rCNT  r5
#define rDATA r6

#ifdef POSITIVE_0
#define VOL_ORDER_0(a, b) a, b
#else
#define VOL_ORDER_0(a, b) b, a
#endif

#ifdef POSITIVE_1
#define VOL_ORDER_1(a, b) a, b
#else
#define VOL_ORDER_1(a, b) b, a
#endif

@ WYATT_TODO if the input segment is overaligned enough, and if we swap the volume array structure,
@ we can just increment and & the pointer. This would also let us use LDM/STM with increments.

.section .text.FUNCNAME,"ax",%progbits
.align    5
.global FUNCNAME
.syntax unified
.arm
.type FUNCNAME, %function
.cfi_startproc
FUNCNAME:
    @ args = 12, pretend = 0, frame = 8
    @ frame_needed = 0, uses_anonymous_args = 0
    push {r4, r5, r6, r7, r8, r9, r10, fp, ip, lr}  @ SP head is 40. IP is overwritten to store the end pointer.
    ldr r4, [sp, #48]                               @ Load nSamples
    add r4, rINPT, r4, lsl #1                       @ Compute end pointer
    str r4, [sp, #32]                               @ Store end pointer, overwriting IP on the stack
    mov rCNT, #0                                    @ R5 = i
    ldr rDATA, [sp, #44]                            @ Load data segment pointer
#ifdef PROCESS_AUX
    ldr rWET1, [sp, #40]                            @ Load wet1 pointer
#endif
.loop:
    @ R7 reserved for volume 0
    @ R8 reserved for volume 1

    @ Uses IP, R9, R10, FP, LR
    @ Calculate new volume 1
    add r9, rDATA, rCNT, lsl #2                     @ vol1 Calculate offset
    ldr lr, [rDATA, #76]                            @ vol1 Load rate
    ldr r8, [r9, #32]                               @ vol1 Load volume 1
    ldr r7, [rDATA, rCNT, lsl #2]                   @ vol0 Load volume 0

    
    smull lr, fp, r8, lr                            @ vol1 volume * rate. LR is lo, FP is hi
    ldr ip, [rDATA, #68]                            @ vol1 Load target
    lsr lr, lr, #16                                 @ vol1 LR = the bottom 32 bits of (vol*rate) >> 16
    orr lr, lr, fp, lsl #16                         @  |
    cmp VOL_ORDER_1(ip, lr)                         @ vol1 result lo - Target
    asr fp, fp, #16                                 @ vol1 FP = top 16 bits of vol * rate
    asr r10, ip, #31                                @ vol1 R10 = sign bit of target
    sbcs fp, VOL_ORDER_1(r10, fp)                   @ vol1 Top 16 bits of vol*rate - Sign bit of target
    ldr r10, [rDATA, #72]                           @ vol0 Load rate
    movlt lr, ip                                    @ vol1 Clamp
    str lr, [r9, #32]                               @ vol1 Store
    ldr ip, [rDATA, #64]                            @ vol0 Load target

    @ Uses IP, R10, FP, R9
    @ Calculate new volume 0
    smull r10, fp, r7, r10                          @ vol0 volume * rate. R10 is lo, FP is hi
    lsr r10, r10, #16                               @ vol0 R10 = the bottom 32 bits of (vol*rate) >> 16
    orr r10, r10, fp, lsl #16                       @  |
    cmp VOL_ORDER_0(ip, r10)                        @ vol0 result lo - Target
    asr fp, fp, #16                                 @ vol0 FP = top 16 bits of vol * rate
    asr r9, ip, #31                                 @ vol0 R10 = sign bit of target
    sbcs fp, VOL_ORDER_0(r9, fp)                    @ vol0 Sign bit of target - Top 16 bits of vol*rate
    ldrsh fp, [rINPT], #2                           @ iv   Load input, increment
    movlt r10, ip                                   @ vol0 Clamp
    str r10, [rDATA, rCNT, lsl #2]                  @ vol0 Store

    @ Calculate IV0 and IV1
    ldr r9, [rDATA, #80]                            @ Load vol_dry. Up here to help with smulbt early reg
    smulbt r7, fp, r7                               @ R7 = input * volume 0
    smulbt r8, fp, r8                               @ R8 = input * volume 1

    @ Dry processing. Uses R9, IP, LR
    @ dryN = sat16((input * volume N) * vol_dry + dryN)
    ldrsh ip, [rDRY0]                               @ dry0 load
    ldrsh lr, [rDRY1]                               @ dry1 load
    ldr r10, [sp, #32]                              @ Load end pointer
    smmlar ip, r7, r9, ip                           @ dry0 multiply
    smmlar lr, r8, r9, lr                           @ dry1 multiply
    cmp r10, rINPT                                  @ Check exit condition
    add rCNT, rCNT, #1                              @ i++ % 8
    ssat ip, #16, ip                                @ dry0 sat
    ssat lr, #16, lr                                @ dry1 sat
    strh ip, [rDRY0], #2 @ movhi                    @ dry0 store
    strh lr, [rDRY1], #2 @ movhi                    @ dry1 store

#ifdef PROCESS_AUX
    @ Wet processing. Uses R10, R9, LR
    @ wetN = sat16((input * volume N) * vol_wet + wetN)
    ldr r9, [rDATA, #84]                            @ Load vol_wet
    ldrsh r10, [rWET0]                              @ wet0 load
    ldrsh lr, [rWET1]                               @ wet1 load
    smmlar r10, r7, r9, r10                         @ wet0 multiply
    smmlar lr, r8, r9, lr                           @ wet1 multiply
    and rCNT, rCNT, #7                              @ 
    ssat r10, #16, r10                              @ wet0 sat
    ssat lr, #16, lr                                @ wet1 sat
    strh r10, [rWET0], #2 @ movhi                   @ wet0 store
    strh lr, [rWET1], #2 @ movhi                    @ wet1 store
#else
    and rCNT, rCNT, #7                              @ 
#endif

    @ Loop or exit
    bhi .loop                                       @ 
    pop {r4, r5, r6, r7, r8, r9, r10, fp, ip, pc}   @ 
.cfi_endproc

#undef POSITIVE_0
#undef POSITIVE_1
#undef VOL_ORDER_0
#undef VOL_ORDER_1

#undef rINPT r0
#undef rDRY0 r1
#undef rDRY1 r2
#undef rWET0 r3
#undef rWET1 r4
#undef rCNT  r5
#undef rDATA r6
#undef FUNCNAME
