/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "../common/portability_macros.h"

/* Stack marking
 * ref: https://wiki.gentoo.org/wiki/Hardened/GNU_stack_quickstart
 */
#if defined(__ELF__) && defined(__GNUC__)
.section .note.GNU-stack,"",%progbits
#endif

#if ZSTD_ENABLE_ASM_X86_64_BMI2

/* Calling convention:
 *
 * %rdi contains the first argument: HUF_DecompressAsmArgs*.
 * %rbp isn't maintained (no frame pointer).
 * %rsp contains the stack pointer that grows down.
 *      No red-zone is assumed, only addresses >= %rsp are used.
 * All register contents are preserved.
 *
 * TODO: Support Windows calling convention.
 */

ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X1_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X2_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X2_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X1_usingDTable_internal_fast_asm_loop)
.global HUF_decompress4X1_usingDTable_internal_fast_asm_loop
.global HUF_decompress4X2_usingDTable_internal_fast_asm_loop
.global _HUF_decompress4X1_usingDTable_internal_fast_asm_loop
.global _HUF_decompress4X2_usingDTable_internal_fast_asm_loop
.text

/* Sets up register mappings for clarity.
 * op[], bits[], dtable & ip[0] each get their own register.
 * ip[1,2,3] & olimit alias var[].
 * %rax is a scratch register.
 */

#define op0    rsi
#define op1    rbx
#define op2    rcx
#define op3    rdi

#define ip0    r8
#define ip1    r9
#define ip2    r10
#define ip3    r11

#define bits0  rbp
#define bits1  rdx
#define bits2  r12
#define bits3  r13
#define dtable r14
#define olimit r15

/* var[] aliases ip[1,2,3] & olimit
 * ip[1,2,3] are saved every iteration.
 * olimit is only used in compute_olimit.
 */
#define var0   r15
#define var1   r9
#define var2   r10
#define var3   r11

/* 32-bit var registers */
#define vard0  r15d
#define vard1  r9d
#define vard2  r10d
#define vard3  r11d

/* Calls X(N) for each stream 0, 1, 2, 3. */
#define FOR_EACH_STREAM(X) \
    X(0);                  \
    X(1);                  \
    X(2);                  \
    X(3)

/* Calls X(N, idx) for each stream 0, 1, 2, 3. */
#define FOR_EACH_STREAM_WITH_INDEX(X, idx) \
    X(0, idx);                             \
    X(1, idx);                             \
    X(2, idx);                             \
    X(3, idx)

/* Define both _HUF_* & HUF_* symbols because MacOS
 * C symbols are prefixed with '_' & Linux symbols aren't.
 */
_HUF_decompress4X1_usingDTable_internal_fast_asm_loop:
HUF_decompress4X1_usingDTable_internal_fast_asm_loop:
    ZSTD_CET_ENDBRANCH
    /* Save all registers - even if they are callee saved for simplicity. */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* Read HUF_DecompressAsmArgs* args from %rax */
    movq %rdi, %rax
    movq  0(%rax), %ip0
    movq  8(%rax), %ip1
    movq 16(%rax), %ip2
    movq 24(%rax), %ip3
    movq 32(%rax), %op0
    movq 40(%rax), %op1
    movq 48(%rax), %op2
    movq 56(%rax), %op3
    movq 64(%rax), %bits0
    movq 72(%rax), %bits1
    movq 80(%rax), %bits2
    movq 88(%rax), %bits3
    movq 96(%rax), %dtable
    push %rax      /* argument */
    push 104(%rax) /* ilimit */
    push 112(%rax) /* oend */
    push %olimit   /* olimit space */

    subq $24, %rsp

.L_4X1_compute_olimit:
    /* Computes how many iterations we can do safely
     * %r15, %rax may be clobbered
     * rbx, rdx must be saved
     * op3 & ip0 mustn't be clobbered
     */
    movq %rbx, 0(%rsp)
    movq %rdx, 8(%rsp)

    movq 32(%rsp), %rax /* rax = oend */
    subq %op3,    %rax  /* rax = oend - op3 */

    /* r15 = (oend - op3) / 5 */
    movabsq $-3689348814741910323, %rdx
    mulq %rdx
    movq %rdx, %r15
    shrq $2, %r15

    movq %ip0,     %rax /* rax = ip0 */
    movq 40(%rsp), %rdx /* rdx = ilimit */
    subq %rdx,     %rax /* rax = ip0 - ilimit */
    movq %rax,     %rbx /* rbx = ip0 - ilimit */

    /* rdx = (ip0 - ilimit) / 7 */
    movabsq $2635249153387078803, %rdx
    mulq %rdx
    subq %rdx, %rbx
    shrq %rbx
    addq %rbx, %rdx
    shrq $2, %rdx

    /* r15 = min(%rdx, %r15) */
    cmpq %rdx, %r15
    cmova %rdx, %r15

    /* r15 = r15 * 5 */
    leaq (%r15, %r15, 4), %r15

    /* olimit = op3 + r15 */
    addq %op3, %olimit

    movq 8(%rsp), %rdx
    movq 0(%rsp), %rbx

    /* If (op3 + 20 > olimit) */
    movq %op3, %rax    /* rax = op3 */
    addq $20,  %rax    /* rax = op3 + 20 */
    cmpq %rax, %olimit /* op3 + 20 > olimit */
    jb .L_4X1_exit

    /* If (ip1 < ip0) go to exit */
    cmpq %ip0, %ip1
    jb .L_4X1_exit

    /* If (ip2 < ip1) go to exit */
    cmpq %ip1, %ip2
    jb .L_4X1_exit

    /* If (ip3 < ip2) go to exit */
    cmpq %ip2, %ip3
    jb .L_4X1_exit

/* Reads top 11 bits from bits[n]
 * Loads dt[bits[n]] into var[n]
 */
#define GET_NEXT_DELT(n)                \
    movq $53, %var##n;                  \
    shrxq %var##n, %bits##n, %var##n;   \
    movzwl (%dtable,%var##n,2),%vard##n

/* var[n] must contain the DTable entry computed with GET_NEXT_DELT
 * Moves var[n] to %rax
 * bits[n] <<= var[n] & 63
 * op[n][idx] = %rax >> 8
 * %ah is a way to access bits [8, 16) of %rax
 */
#define DECODE_FROM_DELT(n, idx)       \
    movq %var##n, %rax;                \
    shlxq %var##n, %bits##n, %bits##n; \
    movb %ah, idx(%op##n)

/* Assumes GET_NEXT_DELT has been called.
 * Calls DECODE_FROM_DELT then GET_NEXT_DELT
 */
#define DECODE_AND_GET_NEXT(n, idx) \
    DECODE_FROM_DELT(n, idx);       \
    GET_NEXT_DELT(n)                \

/* // ctz & nbBytes is stored in bits[n]
 * // nbBits is stored in %rax
 * ctz  = CTZ[bits[n]]
 * nbBits  = ctz & 7
 * nbBytes = ctz >> 3
 * op[n]  += 5
 * ip[n]  -= nbBytes
 * // Note: x86-64 is little-endian ==> no bswap
 * bits[n] = MEM_readST(ip[n]) | 1
 * bits[n] <<= nbBits
 */
#define RELOAD_BITS(n)             \
    bsfq %bits##n, %bits##n;       \
    movq %bits##n, %rax;           \
    andq $7, %rax;                 \
    shrq $3, %bits##n;             \
    leaq 5(%op##n), %op##n;        \
    subq %bits##n, %ip##n;         \
    movq (%ip##n), %bits##n;       \
    orq $1, %bits##n;              \
    shlx %rax, %bits##n, %bits##n

    /* Store clobbered variables on the stack */
    movq %olimit, 24(%rsp)
    movq %ip1, 0(%rsp)
    movq %ip2, 8(%rsp)
    movq %ip3, 16(%rsp)

    /* Call GET_NEXT_DELT for each stream */
    FOR_EACH_STREAM(GET_NEXT_DELT)

    .p2align 6

.L_4X1_loop_body:
    /* Decode 5 symbols in each of the 4 streams (20 total)
     * Must have called GET_NEXT_DELT for each stream
     */
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 0)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 1)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 2)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 3)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_FROM_DELT, 4)

    /* Load ip[1,2,3] from stack (var[] aliases them)
     * ip[] is needed for RELOAD_BITS
     * Each will be stored back to the stack after RELOAD
     */
    movq 0(%rsp), %ip1
    movq 8(%rsp), %ip2
    movq 16(%rsp), %ip3

    /* Reload each stream & fetch the next table entry
     * to prepare for the next iteration
     */
    RELOAD_BITS(0)
    GET_NEXT_DELT(0)

    RELOAD_BITS(1)
    movq %ip1, 0(%rsp)
    GET_NEXT_DELT(1)

    RELOAD_BITS(2)
    movq %ip2, 8(%rsp)
    GET_NEXT_DELT(2)

    RELOAD_BITS(3)
    movq %ip3, 16(%rsp)
    GET_NEXT_DELT(3)

    /* If op3 < olimit: continue the loop */
    cmp %op3, 24(%rsp)
    ja .L_4X1_loop_body

    /* Reload ip[1,2,3] from stack */
    movq 0(%rsp), %ip1
    movq 8(%rsp), %ip2
    movq 16(%rsp), %ip3

    /* Re-compute olimit */
    jmp .L_4X1_compute_olimit

#undef GET_NEXT_DELT
#undef DECODE_FROM_DELT
#undef DECODE
#undef RELOAD_BITS
.L_4X1_exit:
    addq $24, %rsp

    /* Restore stack (oend & olimit) */
    pop %rax /* olimit */
    pop %rax /* oend */
    pop %rax /* ilimit */
    pop %rax /* arg */

    /* Save ip / op / bits */
    movq %ip0,  0(%rax)
    movq %ip1,  8(%rax)
    movq %ip2, 16(%rax)
    movq %ip3, 24(%rax)
    movq %op0, 32(%rax)
    movq %op1, 40(%rax)
    movq %op2, 48(%rax)
    movq %op3, 56(%rax)
    movq %bits0, 64(%rax)
    movq %bits1, 72(%rax)
    movq %bits2, 80(%rax)
    movq %bits3, 88(%rax)

    /* Restore registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    ret

_HUF_decompress4X2_usingDTable_internal_fast_asm_loop:
HUF_decompress4X2_usingDTable_internal_fast_asm_loop:
    ZSTD_CET_ENDBRANCH
    /* Save all registers - even if they are callee saved for simplicity. */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    movq %rdi, %rax
    movq  0(%rax), %ip0
    movq  8(%rax), %ip1
    movq 16(%rax), %ip2
    movq 24(%rax), %ip3
    movq 32(%rax), %op0
    movq 40(%rax), %op1
    movq 48(%rax), %op2
    movq 56(%rax), %op3
    movq 64(%rax), %bits0
    movq 72(%rax), %bits1
    movq 80(%rax), %bits2
    movq 88(%rax), %bits3
    movq 96(%rax), %dtable
    push %rax      /* argument */
    push %rax      /* olimit */
    push 104(%rax) /* ilimit */

    movq 112(%rax), %rax
    push %rax /* oend3 */

    movq %op3, %rax
    push %rax /* oend2 */

    movq %op2, %rax
    push %rax /* oend1 */

    movq %op1, %rax
    push %rax /* oend0 */

    /* Scratch space */
    subq $8, %rsp

.L_4X2_compute_olimit:
    /* Computes how many iterations we can do safely
     * %r15, %rax may be clobbered
     * rdx must be saved
     * op[1,2,3,4] & ip0 mustn't be clobbered
     */
    movq %rdx, 0(%rsp)

    /* We can consume up to 7 input bytes each iteration. */
    movq %ip0,     %rax  /* rax = ip0 */
    movq 40(%rsp), %rdx  /* rdx = ilimit */
    subq %rdx,     %rax  /* rax = ip0 - ilimit */
    movq %rax,    %r15   /* r15 = ip0 - ilimit */

    /* rdx = rax / 7 */
    movabsq $2635249153387078803, %rdx
    mulq %rdx
    subq %rdx, %r15
    shrq %r15
    addq %r15, %rdx
    shrq $2, %rdx

    /* r15 = (ip0 - ilimit) / 7 */
    movq %rdx, %r15

    /* r15 = min(r15, min(oend0 - op0, oend1 - op1, oend2 - op2, oend3 - op3) / 10) */
    movq 8(%rsp),  %rax /* rax = oend0 */
    subq %op0,     %rax /* rax = oend0 - op0 */
    movq 16(%rsp), %rdx /* rdx = oend1 */
    subq %op1,     %rdx /* rdx = oend1 - op1 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movq 24(%rsp), %rax /* rax = oend2 */
    subq %op2,     %rax /* rax = oend2 - op2 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movq 32(%rsp), %rax /* rax = oend3 */
    subq %op3,     %rax /* rax = oend3 - op3 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movabsq $-3689348814741910323, %rax
    mulq %rdx
    shrq $3,       %rdx /* rdx = rdx / 10 */

    /* r15 = min(%rdx, %r15) */
    cmpq  %rdx, %r15
    cmova %rdx, %r15

    /* olimit = op3 + 5 * r15 */
    movq %r15, %rax
    leaq (%op3, %rax, 4), %olimit
    addq %rax, %olimit

    movq 0(%rsp), %rdx

    /* If (op3 + 10 > olimit) */
    movq %op3, %rax    /* rax = op3 */
    addq $10,  %rax    /* rax = op3 + 10 */
    cmpq %rax, %olimit /* op3 + 10 > olimit */
    jb .L_4X2_exit

    /* If (ip1 < ip0) go to exit */
    cmpq %ip0, %ip1
    jb .L_4X2_exit

    /* If (ip2 < ip1) go to exit */
    cmpq %ip1, %ip2
    jb .L_4X2_exit

    /* If (ip3 < ip2) go to exit */
    cmpq %ip2, %ip3
    jb .L_4X2_exit

#define DECODE(n, idx)              \
    movq %bits##n, %rax;            \
    shrq $53, %rax;                 \
    movzwl 0(%dtable,%rax,4),%r8d;  \
    movzbl 2(%dtable,%rax,4),%r15d; \
    movzbl 3(%dtable,%rax,4),%eax;  \
    movw %r8w, (%op##n);            \
    shlxq %r15, %bits##n, %bits##n; \
    addq %rax, %op##n

#define RELOAD_BITS(n)              \
    bsfq %bits##n, %bits##n;        \
    movq %bits##n, %rax;            \
    shrq $3, %bits##n;              \
    andq $7, %rax;                  \
    subq %bits##n, %ip##n;          \
    movq (%ip##n), %bits##n;        \
    orq $1, %bits##n;               \
    shlxq %rax, %bits##n, %bits##n


    movq %olimit, 48(%rsp)

    .p2align 6

.L_4X2_loop_body:
    /* We clobber r8, so store it on the stack */
    movq %r8, 0(%rsp)

    /* Decode 5 symbols from each of the 4 streams (20 symbols total). */
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 0)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 1)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 2)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 3)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 4)

    /* Reload r8 */
    movq 0(%rsp), %r8

    FOR_EACH_STREAM(RELOAD_BITS)

    cmp %op3, 48(%rsp)
    ja .L_4X2_loop_body
    jmp .L_4X2_compute_olimit

#undef DECODE
#undef RELOAD_BITS
.L_4X2_exit:
    addq $8, %rsp
    /* Restore stack (oend & olimit) */
    pop %rax /* oend0 */
    pop %rax /* oend1 */
    pop %rax /* oend2 */
    pop %rax /* oend3 */
    pop %rax /* ilimit */
    pop %rax /* olimit */
    pop %rax /* arg */

    /* Save ip / op / bits */
    movq %ip0,  0(%rax)
    movq %ip1,  8(%rax)
    movq %ip2, 16(%rax)
    movq %ip3, 24(%rax)
    movq %op0, 32(%rax)
    movq %op1, 40(%rax)
    movq %op2, 48(%rax)
    movq %op3, 56(%rax)
    movq %bits0, 64(%rax)
    movq %bits1, 72(%rax)
    movq %bits2, 80(%rax)
    movq %bits3, 88(%rax)

    /* Restore registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    ret

#endif
