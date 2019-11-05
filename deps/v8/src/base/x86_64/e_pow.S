/*
Copyright (c) 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    * this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    * this list of conditions and the following disclaimer in the documentation
    * and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its contributors
    * may be used to endorse or promote products derived from this software
    * without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/******************************************************************************/
//                     ALGORITHM DESCRIPTION
//                     ---------------------
//
//    Let x=2^k * mx, mx in [1,2)
//
//    log2(x) calculation:
//
//    Get B~1/mx based on the output of rcpps instruction (B0)
//    B = int((B0*LH*2^9+0.5))/2^9
//    LH is a short approximation for log2(e)
//
//    Reduced argument, scaled by LH:
//                r=B*mx-LH (computed accurately in high and low parts)
//
//    log2(x) result:  k - log2(B) + p(r)
//             p(r) is a degree 8 polynomial
//             -log2(B) read from data table (high, low parts)
//             log2(x) is formed from high and low parts
//    For |x| in [1-1/32, 1+1/16), a slower but more accurate computation
//    based om the same table design is performed.
//
//   Main path is taken if | floor(log2(|log2(|x|)|) + floor(log2|y|) | < 8,
//   to filter out all potential OF/UF cases.
//   exp2(y*log2(x)) is computed using an 8-bit index table and a degree 5
//   polynomial
//
// Special cases:
//  pow(-0,y) = -INF and raises the divide-by-zero exception for y an odd 
//  integer < 0.
//  pow(-0,y) = +INF and raises the divide-by-zero exception for y < 0 and 
//  not an odd integer.
//  pow(-0,y) = -0 for y an odd integer > 0.
//  pow(-0,y) = +0 for y > 0 and not an odd integer.
//  pow(-1,-INF) = 1.
//  pow(+1,y) = 1 for any y, even a NaN.
//  pow(x,-0) = 1 for any x, even a NaN.
//  pow(x,y) = a NaN and raises the invalid exception for finite x < 0 and
//  finite non-integer y.
//  pow(x,-INF) = +INF for |x|<1.
//  pow(x,-INF) = +0 for |x|>1.
//  pow(x,+INF) = +0 for |x|<1.
//  pow(x,+INF) = +INF for |x|>1.
//  pow(-INF,y) = -0 for y an odd integer < 0.
//  pow(-INF,y) = +0 for y < 0 and not an odd integer.
//  pow(-INF,y) = -INF for y an odd integer > 0.
//  pow(-INF,y) = +INF for y > 0 and not an odd integer.
//  pow(+INF,y) = +0 for y <0.
//  pow(+INF,y) = +INF for y >0.
//
/******************************************************************************/

#include <src/base/bionic/asm.h>
# -- Begin  pow
ENTRY(v8_base_pow)
# parameter 1: %xmm0
# parameter 2: %xmm1
..B1.1:
..___tag_value_pow.1:
        subq      $40, %rsp
..___tag_value_pow.3:
        movsd     %xmm0, 8(%rsp)
        movsd     %xmm1, 16(%rsp)
..B1.2:
        pextrw    $3, %xmm0, %eax
        xorpd     %xmm2, %xmm2
        movq      $0x3ff0000000000000, %r9
        movd      %r9, %xmm2
        movl      $1069088768, %r8d
        movd      %r8, %xmm7
        xorpd     %xmm1, %xmm1
        movq      $0x77f0000000000000, %r10
        movd      %r10, %xmm1
        movq      %xmm0, %xmm3
        movl      $32752, %edx
        andl      %eax, %edx
        subl      $16368, %edx
        movl      %edx, %ecx
        sarl      $31, %edx
        addl      %edx, %ecx
        xorl      %edx, %ecx
        orpd      %xmm2, %xmm0
        movapd    HIGHSIGMASK(%rip), %xmm6
        psrlq     $27, %xmm0
        movq      LOG2_E(%rip), %xmm2
        psrld     $2, %xmm0
        addl      $16, %ecx
        bsr       %ecx, %ecx
        rcpps     %xmm0, %xmm0
        psllq     $12, %xmm3
        movl      $8192, %r11d
        movd      %r11, %xmm4
        psrlq     $12, %xmm3
        subl      $16, %eax
        cmpl      $32736, %eax
        jae       .L_2TAG_PACKET_0.0.2
        movq      $0, %r8
.L_2TAG_PACKET_1.0.2:
        mulss     %xmm7, %xmm0
        movl      $-1, %edx
        subl      $4, %ecx
        shll      %cl, %edx
        shlq      $32, %rdx
        movd      %rdx, %xmm5
        orpd      %xmm1, %xmm3
        subl      $16351, %eax
        cmpl      $1, %eax
        jbe       .L_2TAG_PACKET_2.0.2
        paddd     %xmm4, %xmm0
        andpd     %xmm3, %xmm5
        movd      %xmm0, %edx
        psllq     $29, %xmm0
.L_2TAG_PACKET_3.0.2:
        subsd     %xmm5, %xmm3
        andpd     %xmm6, %xmm0
        subl      $1, %eax
        sarl      $4, %eax
        cvtsi2sd  %eax, %xmm7
        mulpd     %xmm0, %xmm5
.L_2TAG_PACKET_4.0.2:
        mulsd     %xmm0, %xmm3
        movapd    coeff(%rip), %xmm1
        lea       L_tbl(%rip), %r11
        subsd     %xmm2, %xmm5
        movapd    16+coeff(%rip), %xmm4
        movl      %eax, %ecx
        sarl      $31, %eax
        addl      %eax, %ecx
        xorl      %ecx, %eax
        addl      $1, %eax
        bsr       %eax, %eax
        unpcklpd  %xmm3, %xmm5
        movapd    32+coeff(%rip), %xmm6
        addsd     %xmm5, %xmm3
        andl      $16760832, %edx
        shrl      $10, %edx
        addpd     -3648(%r11,%rdx), %xmm5
        movapd    48+coeff(%rip), %xmm0
        pshufd    $68, %xmm3, %xmm2
        mulsd     %xmm3, %xmm3
        mulpd     %xmm2, %xmm1
        mulpd     %xmm2, %xmm4
        addsd     %xmm7, %xmm5
        mulsd     %xmm3, %xmm2
        addpd     %xmm1, %xmm6
        mulsd     %xmm3, %xmm3
        addpd     %xmm4, %xmm0
        movq      16(%rsp), %xmm1
        movw      22(%rsp), %cx
        pshufd    $238, %xmm5, %xmm7
        movq      HIGHMASK_Y(%rip), %xmm4
        mulpd     %xmm2, %xmm6
        pshufd    $68, %xmm3, %xmm3
        mulpd     %xmm2, %xmm0
        shll      $4, %eax
        subl      $15872, %eax
        andl      $32752, %ecx
        addl      %ecx, %eax
        mulpd     %xmm6, %xmm3
        cmpl      $624, %eax
        jae       .L_2TAG_PACKET_5.0.2
        xorpd     %xmm6, %xmm6
        movl      $17080, %edx
        pinsrw    $3, %edx, %xmm6
        movq      %xmm1, %xmm2
        andpd     %xmm1, %xmm4
        subsd     %xmm4, %xmm1
        mulsd     %xmm5, %xmm4
        addsd     %xmm7, %xmm0
        mulsd     %xmm5, %xmm1
        movq      %xmm6, %xmm7
        addsd     %xmm4, %xmm6
        lea       T_exp(%rip), %r11
        addpd     %xmm0, %xmm3
        movd      %xmm6, %edx
        subsd     %xmm7, %xmm6
        pshufd    $238, %xmm3, %xmm0
        subsd     %xmm6, %xmm4
        addsd     %xmm3, %xmm0
        movl      %edx, %ecx
        andl      $255, %edx
        addl      %edx, %edx
        movapd    (%r11,%rdx,8), %xmm5
        addsd     %xmm1, %xmm4
        mulsd     %xmm0, %xmm2
        movapd    e_coeff(%rip), %xmm7
        movapd    16+e_coeff(%rip), %xmm3
        shll      $12, %ecx
        xorl      %r8d, %ecx
        andl      $-1048576, %ecx
        movd      %rcx, %xmm6
        addsd     %xmm4, %xmm2
        movq      $0x3fe62e42fefa39ef, %r9
        movd      %r9, %xmm1
        pshufd    $68, %xmm2, %xmm0
        pshufd    $68, %xmm2, %xmm4
        mulsd     %xmm2, %xmm1
        pshufd    $17, %xmm6, %xmm6
        mulpd     %xmm0, %xmm0
        mulpd     %xmm4, %xmm7
        paddd     %xmm6, %xmm5
        mulsd     %xmm5, %xmm1
        pshufd    $238, %xmm5, %xmm6
        mulsd     %xmm0, %xmm0
        addpd     %xmm7, %xmm3
        addsd     %xmm6, %xmm1
        mulpd     %xmm3, %xmm0
        pshufd    $238, %xmm0, %xmm3
        mulsd     %xmm5, %xmm0
        mulsd     %xmm5, %xmm3
        addsd     %xmm1, %xmm0
        addsd     %xmm3, %xmm0
        addsd     %xmm5, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_0.0.2:
        addl      $16, %eax
        movl      $32752, %edx
        andl      %eax, %edx
        cmpl      $32752, %edx
        je        .L_2TAG_PACKET_6.0.2
        testl     $32768, %eax
        jne       .L_2TAG_PACKET_7.0.2
.L_2TAG_PACKET_8.0.2:
        movq      8(%rsp), %xmm0
        movq      8(%rsp), %xmm3
        movd      %xmm3, %edx
        psrlq     $32, %xmm3
        movd      %xmm3, %ecx
        orl       %ecx, %edx
        cmpl      $0, %edx
        je        .L_2TAG_PACKET_9.0.2
        xorpd     %xmm3, %xmm3
        movl      $18416, %eax
        pinsrw    $3, %eax, %xmm3
        mulsd     %xmm3, %xmm0
        xorpd     %xmm2, %xmm2
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm2
        movq      %xmm0, %xmm3
        pextrw    $3, %xmm0, %eax
        orpd      %xmm2, %xmm0
        movl      $18416, %ecx
        psrlq     $27, %xmm0
        movq      LOG2_E(%rip), %xmm2
        psrld     $2, %xmm0
        rcpps     %xmm0, %xmm0
        psllq     $12, %xmm3
        movapd    HIGHSIGMASK(%rip), %xmm6
        psrlq     $12, %xmm3
        mulss     %xmm7, %xmm0
        movl      $-1024, %edx
        movd      %edx, %xmm5
        orpd      %xmm1, %xmm3
        paddd     %xmm4, %xmm0
        psllq     $32, %xmm5
        movd      %xmm0, %edx
        psllq     $29, %xmm0
        andpd     %xmm3, %xmm5
        movl      $0, %r8d
        andpd     %xmm6, %xmm0
        subsd     %xmm5, %xmm3
        andl      $32752, %eax
        subl      $18416, %eax
        sarl      $4, %eax
        cvtsi2sd  %eax, %xmm7
        mulpd     %xmm0, %xmm5
        jmp       .L_2TAG_PACKET_4.0.2
.L_2TAG_PACKET_10.0.2:
        movq      8(%rsp), %xmm0
        movq      8(%rsp), %xmm3
        movd      %xmm3, %edx
        psrlq     $32, %xmm3
        movd      %xmm3, %ecx
        orl       %ecx, %edx
        cmpl      $0, %edx
        je        .L_2TAG_PACKET_9.0.2
        xorpd     %xmm3, %xmm3
        movl      $18416, %eax
        pinsrw    $3, %eax, %xmm3
        mulsd     %xmm3, %xmm0
        xorpd     %xmm2, %xmm2
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm2
        movq      %xmm0, %xmm3
        pextrw    $3, %xmm0, %eax
        orpd      %xmm2, %xmm0
        movl      $18416, %ecx
        psrlq     $27, %xmm0
        movq      LOG2_E(%rip), %xmm2
        psrld     $2, %xmm0
        rcpps     %xmm0, %xmm0
        psllq     $12, %xmm3
        movapd    HIGHSIGMASK(%rip), %xmm6
        psrlq     $12, %xmm3
        mulss     %xmm7, %xmm0
        movl      $-1024, %edx
        movd      %edx, %xmm5
        orpd      %xmm1, %xmm3
        paddd     %xmm4, %xmm0
        psllq     $32, %xmm5
        movd      %xmm0, %edx
        psllq     $29, %xmm0
        andpd     %xmm3, %xmm5
        movl      $-2147483648, %r8d
        andpd     %xmm6, %xmm0
        subsd     %xmm5, %xmm3
        andl      $32752, %eax
        subl      $18416, %eax
        sarl      $4, %eax
        cvtsi2sd  %eax, %xmm7
        mulpd     %xmm0, %xmm5
        jmp       .L_2TAG_PACKET_4.0.2
.L_2TAG_PACKET_5.0.2:
        cmpl      $0, %eax
        jl        .L_2TAG_PACKET_11.0.2
        cmpl      $736, %eax
        jae       .L_2TAG_PACKET_12.0.2
        addsd     %xmm7, %xmm0
        movq      HALFMASK(%rip), %xmm2
        addpd     %xmm0, %xmm3
        xorpd     %xmm6, %xmm6
        movl      $17080, %eax
        pinsrw    $3, %eax, %xmm6
        pshufd    $238, %xmm3, %xmm0
        addsd     %xmm3, %xmm0
        movq      %xmm5, %xmm3
        addsd     %xmm0, %xmm5
        movq      %xmm2, %xmm4
        subsd     %xmm5, %xmm3
        movq      %xmm5, %xmm7
        andpd     %xmm2, %xmm5
        movq      %xmm1, %xmm2
        andpd     %xmm1, %xmm4
        subsd     %xmm5, %xmm7
        addsd     %xmm3, %xmm0
        subsd     %xmm4, %xmm1
        mulsd     %xmm5, %xmm4
        addsd     %xmm7, %xmm0
        mulsd     %xmm0, %xmm2
        movq      %xmm6, %xmm7
        mulsd     %xmm5, %xmm1
        addsd     %xmm4, %xmm6
        movd      %xmm6, %eax
        subsd     %xmm7, %xmm6
        lea       T_exp(%rip), %r11
        addsd     %xmm1, %xmm2
        movapd    e_coeff(%rip), %xmm7
        movapd    16+e_coeff(%rip), %xmm3
        subsd     %xmm6, %xmm4
        pextrw    $3, %xmm6, %edx
        movl      %eax, %ecx
        andl      $255, %eax
        addl      %eax, %eax
        movapd    (%r11,%rax,8), %xmm5
        addsd     %xmm4, %xmm2
        sarl      $8, %ecx
        movl      %ecx, %eax
        sarl      $1, %ecx
        subl      %ecx, %eax
        shll      $20, %ecx
        xorl      %r8d, %ecx
        movd      %ecx, %xmm6
        movq      32+e_coeff(%rip), %xmm1
        andl      $32767, %edx
        cmpl      $16529, %edx
        ja        .L_2TAG_PACKET_12.0.2
        pshufd    $68, %xmm2, %xmm0
        pshufd    $68, %xmm2, %xmm4
        mulpd     %xmm0, %xmm0
        mulpd     %xmm4, %xmm7
        pshufd    $17, %xmm6, %xmm6
        mulsd     %xmm2, %xmm1
        mulsd     %xmm0, %xmm0
        paddd     %xmm6, %xmm5
        addpd     %xmm7, %xmm3
        mulsd     %xmm5, %xmm1
        pshufd    $238, %xmm5, %xmm6
        mulpd     %xmm3, %xmm0
        addsd     %xmm6, %xmm1
        pshufd    $238, %xmm0, %xmm3
        mulsd     %xmm5, %xmm0
        mulsd     %xmm5, %xmm3
        shll      $4, %eax
        xorpd     %xmm4, %xmm4
        addl      $16368, %eax
        pinsrw    $3, %eax, %xmm4
        addsd     %xmm1, %xmm0
        addsd     %xmm3, %xmm0
        movq      %xmm0, %xmm1
        addsd     %xmm5, %xmm0
        mulsd     %xmm4, %xmm0
        pextrw    $3, %xmm0, %eax
        andl      $32752, %eax
        je        .L_2TAG_PACKET_13.0.2
        cmpl      $32752, %eax
        je        .L_2TAG_PACKET_14.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_6.0.2:
        movq      16(%rsp), %xmm1
        movq      8(%rsp), %xmm0
        movq      %xmm0, %xmm2
        movd      %xmm2, %eax
        psrlq     $20, %xmm2
        movd      %xmm2, %edx
        orl       %edx, %eax
        je        .L_2TAG_PACKET_15.0.2
        movd      %xmm1, %eax
        psrlq     $32, %xmm1
        movd      %xmm1, %edx
        movl      %edx, %ecx
        addl      %edx, %edx
        orl       %edx, %eax
        je        .L_2TAG_PACKET_16.0.2
        addsd     %xmm0, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_16.0.2:
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm0
        movl      $29, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_18.0.2:
        movq      16(%rsp), %xmm0
        addpd     %xmm0, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_15.0.2:
        movd      %xmm1, %eax
        movq      %xmm1, %xmm2
        psrlq     $32, %xmm1
        movd      %xmm1, %edx
        movl      %edx, %ecx
        addl      %edx, %edx
        orl       %edx, %eax
        je        .L_2TAG_PACKET_19.0.2
        pextrw    $3, %xmm2, %eax
        andl      $32752, %eax
        cmpl      $32752, %eax
        jne       .L_2TAG_PACKET_20.0.2
        movd      %xmm2, %eax
        psrlq     $20, %xmm2
        movd      %xmm2, %edx
        orl       %edx, %eax
        jne       .L_2TAG_PACKET_18.0.2
.L_2TAG_PACKET_20.0.2:
        pextrw    $3, %xmm0, %eax
        testl     $32768, %eax
        jne       .L_2TAG_PACKET_21.0.2
        testl     $-2147483648, %ecx
        jne       .L_2TAG_PACKET_22.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_23.0.2:
        movq      16(%rsp), %xmm1
        movd      %xmm1, %eax
        testl     $1, %eax
        jne       .L_2TAG_PACKET_24.0.2
        testl     $2, %eax
        jne       .L_2TAG_PACKET_25.0.2
        jmp       .L_2TAG_PACKET_24.0.2
.L_2TAG_PACKET_21.0.2:
        shrl      $20, %ecx
        andl      $2047, %ecx
        cmpl      $1075, %ecx
        ja        .L_2TAG_PACKET_24.0.2
        je        .L_2TAG_PACKET_26.0.2
        cmpl      $1074, %ecx
        ja        .L_2TAG_PACKET_23.0.2
        cmpl      $1023, %ecx
        jb        .L_2TAG_PACKET_24.0.2
        movq      16(%rsp), %xmm1
        movl      $17208, %eax
        xorpd     %xmm3, %xmm3
        pinsrw    $3, %eax, %xmm3
        movq      %xmm3, %xmm4
        addsd     %xmm1, %xmm3
        subsd     %xmm3, %xmm4
        addsd     %xmm4, %xmm1
        pextrw    $3, %xmm1, %eax
        andl      $32752, %eax
        jne       .L_2TAG_PACKET_24.0.2
        movd      %xmm3, %eax
        andl      $1, %eax
        je        .L_2TAG_PACKET_24.0.2
.L_2TAG_PACKET_25.0.2:
        movq      16(%rsp), %xmm1
        pextrw    $3, %xmm1, %eax
        andl      $32768, %eax
        jne       .L_2TAG_PACKET_27.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_27.0.2:
        xorpd     %xmm0, %xmm0
        movl      $32768, %eax
        pinsrw    $3, %eax, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_24.0.2:
        movq      16(%rsp), %xmm1
        pextrw    $3, %xmm1, %eax
        andl      $32768, %eax
        jne       .L_2TAG_PACKET_22.0.2
        xorpd     %xmm0, %xmm0
        movl      $32752, %eax
        pinsrw    $3, %eax, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_26.0.2:
        movq      16(%rsp), %xmm1
        movd      %xmm1, %eax
        andl      $1, %eax
        je        .L_2TAG_PACKET_24.0.2
        jmp       .L_2TAG_PACKET_25.0.2
.L_2TAG_PACKET_28.0.2:
        movd      %xmm1, %eax
        psrlq     $20, %xmm1
        movd      %xmm1, %edx
        orl       %edx, %eax
        je        .L_2TAG_PACKET_29.0.2
        movq      16(%rsp), %xmm0
        addsd     %xmm0, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_29.0.2:
        movq      8(%rsp), %xmm0
        pextrw    $3, %xmm0, %eax
        cmpl      $49136, %eax
        jne       .L_2TAG_PACKET_30.0.2
        movd      %xmm0, %ecx
        psrlq     $20, %xmm0
        movd      %xmm0, %edx
        orl       %edx, %ecx
        jne       .L_2TAG_PACKET_30.0.2
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_30.0.2:
        movq      16(%rsp), %xmm1
        andl      $32752, %eax
        subl      $16368, %eax
        pextrw    $3, %xmm1, %edx
        xorpd     %xmm0, %xmm0
        xorl      %edx, %eax
        andl      $32768, %eax
        je        .L_2TAG_PACKET_31.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_31.0.2:
        movl      $32752, %ecx
        pinsrw    $3, %ecx, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_32.0.2:
        movd      %xmm1, %eax
        cmpl      $17184, %edx
        ja        .L_2TAG_PACKET_33.0.2
        testl     $1, %eax
        jne       .L_2TAG_PACKET_34.0.2
        testl     $2, %eax
        je        .L_2TAG_PACKET_35.0.2
        jmp       .L_2TAG_PACKET_36.0.2
.L_2TAG_PACKET_33.0.2:
        testl     $1, %eax
        je        .L_2TAG_PACKET_35.0.2
        jmp       .L_2TAG_PACKET_36.0.2
.L_2TAG_PACKET_7.0.2:
        movq      8(%rsp), %xmm2
        movd      %xmm2, %eax
        psrlq     $31, %xmm2
        movd      %xmm2, %ecx
        orl       %ecx, %eax
        je        .L_2TAG_PACKET_9.0.2
        movq      16(%rsp), %xmm1
        pextrw    $3, %xmm1, %edx
        movd      %xmm1, %eax
        movq      %xmm1, %xmm2
        psrlq     $32, %xmm2
        movd      %xmm2, %ecx
        addl      %ecx, %ecx
        orl       %eax, %ecx
        je        .L_2TAG_PACKET_37.0.2
        andl      $32752, %edx
        cmpl      $32752, %edx
        je        .L_2TAG_PACKET_28.0.2
        cmpl      $17200, %edx
        ja        .L_2TAG_PACKET_35.0.2
        cmpl      $17184, %edx
        jae       .L_2TAG_PACKET_32.0.2
        cmpl      $16368, %edx
        jb        .L_2TAG_PACKET_34.0.2
        movl      $17208, %eax
        xorpd     %xmm2, %xmm2
        pinsrw    $3, %eax, %xmm2
        movq      %xmm2, %xmm4
        addsd     %xmm1, %xmm2
        subsd     %xmm2, %xmm4
        addsd     %xmm4, %xmm1
        pextrw    $3, %xmm1, %eax
        andl      $32767, %eax
        jne       .L_2TAG_PACKET_34.0.2
        movd      %xmm2, %eax
        andl      $1, %eax
        je        .L_2TAG_PACKET_35.0.2
.L_2TAG_PACKET_36.0.2:
        xorpd     %xmm1, %xmm1
        movl      $30704, %edx
        pinsrw    $3, %edx, %xmm1
        movq      LOG2_E(%rip), %xmm2
        movq      8(%rsp), %xmm4
        pextrw    $3, %xmm4, %eax
        movl      $8192, %edx
        movd      %edx, %xmm4
        andl      $32767, %eax
        subl      $16, %eax
        jl        .L_2TAG_PACKET_10.0.2
        movl      %eax, %edx
        andl      $32752, %edx
        subl      $16368, %edx
        movl      %edx, %ecx
        sarl      $31, %edx
        addl      %edx, %ecx
        xorl      %edx, %ecx
        addl      $16, %ecx
        bsr       %ecx, %ecx
        movl      $-2147483648, %r8d
        jmp       .L_2TAG_PACKET_1.0.2
.L_2TAG_PACKET_34.0.2:
        xorpd     %xmm1, %xmm1
        movl      $32752, %eax
        pinsrw    $3, %eax, %xmm1
        xorpd     %xmm0, %xmm0
        mulsd     %xmm1, %xmm0
        movl      $28, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_35.0.2:
        xorpd     %xmm1, %xmm1
        movl      $30704, %edx
        pinsrw    $3, %edx, %xmm1
        movq      LOG2_E(%rip), %xmm2
        movq      8(%rsp), %xmm4
        pextrw    $3, %xmm4, %eax
        movl      $8192, %edx
        movd      %edx, %xmm4
        andl      $32767, %eax
        subl      $16, %eax
        jl        .L_2TAG_PACKET_8.0.2
        movl      %eax, %edx
        andl      $32752, %edx
        subl      $16368, %edx
        movl      %edx, %ecx
        sarl      $31, %edx
        addl      %edx, %ecx
        xorl      %edx, %ecx
        addl      $16, %ecx
        bsr       %ecx, %ecx
        movl      $0, %r8d
        jmp       .L_2TAG_PACKET_1.0.2
.L_2TAG_PACKET_19.0.2:
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_22.0.2:
        xorpd     %xmm0, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_11.0.2:
        addl      $384, %eax
        cmpl      $0, %eax
        jl        .L_2TAG_PACKET_38.0.2
        mulsd     %xmm1, %xmm5
        addsd     %xmm7, %xmm0
        shrl      $31, %r8d
        addpd     %xmm0, %xmm3
        pshufd    $238, %xmm3, %xmm0
        addsd     %xmm0, %xmm3
        lea       log2(%rip), %r11
        movq      (%r11,%r8,8), %xmm4
        mulsd     %xmm3, %xmm1
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        shll      $15, %r8d
        orl       %r8d, %eax
        pinsrw    $3, %eax, %xmm0
        addsd     %xmm1, %xmm5
        mulsd     %xmm4, %xmm5
        addsd     %xmm5, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_38.0.2:
.L_2TAG_PACKET_37.0.2:
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_39.0.2:
        xorpd     %xmm0, %xmm0
        movl      $16368, %eax
        pinsrw    $3, %eax, %xmm0
        movl      $26, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_9.0.2:
        movq      16(%rsp), %xmm1
        movq      %xmm1, %xmm2
        pextrw    $3, %xmm1, %eax
        andl      $32752, %eax
        cmpl      $32752, %eax
        jne       .L_2TAG_PACKET_40.0.2
        movd      %xmm2, %eax
        psrlq     $20, %xmm2
        movd      %xmm2, %edx
        orl       %edx, %eax
        jne       .L_2TAG_PACKET_18.0.2
.L_2TAG_PACKET_40.0.2:
        movd      %xmm1, %eax
        psrlq     $32, %xmm1
        movd      %xmm1, %edx
        movl      %edx, %ecx
        addl      %edx, %edx
        orl       %edx, %eax
        je        .L_2TAG_PACKET_39.0.2
        shrl      $21, %edx
        cmpl      $1075, %edx
        ja        .L_2TAG_PACKET_41.0.2
        je        .L_2TAG_PACKET_42.0.2
        cmpl      $1023, %edx
        jb        .L_2TAG_PACKET_41.0.2
        movq      16(%rsp), %xmm1
        movl      $17208, %eax
        xorpd     %xmm3, %xmm3
        pinsrw    $3, %eax, %xmm3
        movq      %xmm3, %xmm4
        addsd     %xmm1, %xmm3
        subsd     %xmm3, %xmm4
        addsd     %xmm4, %xmm1
        pextrw    $3, %xmm1, %eax
        andl      $32752, %eax
        jne       .L_2TAG_PACKET_41.0.2
        movd      %xmm3, %eax
        andl      $1, %eax
        je        .L_2TAG_PACKET_41.0.2
.L_2TAG_PACKET_43.0.2:
        movq      8(%rsp), %xmm0
        testl     $-2147483648, %ecx
        jne       .L_2TAG_PACKET_44.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_42.0.2:
        movq      16(%rsp), %xmm1
        movd      %xmm1, %eax
        testl     $1, %eax
        jne       .L_2TAG_PACKET_43.0.2
.L_2TAG_PACKET_41.0.2:
        testl     $-2147483648, %ecx
        je        .L_2TAG_PACKET_22.0.2
        xorpd     %xmm0, %xmm0
.L_2TAG_PACKET_44.0.2:
        movl      $16368, %eax
        xorpd     %xmm1, %xmm1
        pinsrw    $3, %eax, %xmm1
        divsd     %xmm0, %xmm1
        movq      %xmm1, %xmm0
        movl      $27, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_12.0.2:
        movq      8(%rsp), %xmm2
        movq      16(%rsp), %xmm6
        pextrw    $3, %xmm2, %eax
        pextrw    $3, %xmm6, %edx
        movl      $32752, %ecx
        andl      %edx, %ecx
        cmpl      $32752, %ecx
        je        .L_2TAG_PACKET_45.0.2
        andl      $32752, %eax
        subl      $16368, %eax
        xorl      %eax, %edx
        testl     $32768, %edx
        jne       .L_2TAG_PACKET_46.0.2
.L_2TAG_PACKET_47.0.2:
        movl      $32736, %eax
        pinsrw    $3, %eax, %xmm0
        shrl      $16, %r8d
        orl       %r8d, %eax
        pinsrw    $3, %eax, %xmm1
        mulsd     %xmm1, %xmm0
.L_2TAG_PACKET_14.0.2:
        movl      $24, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_46.0.2:
        movl      $16, %eax
        pinsrw    $3, %eax, %xmm0
        mulsd     %xmm0, %xmm0
        testl     $-2147483648, %r8d
        je        .L_2TAG_PACKET_48.0.2
        movq      $0x8000000000000000, %r9
        movd      %r9, %xmm2
        xorpd     %xmm2, %xmm0
.L_2TAG_PACKET_48.0.2:
        movl      $25, (%rsp)
        jmp       .L_2TAG_PACKET_17.0.2
.L_2TAG_PACKET_13.0.2:
        pextrw    $3, %xmm5, %ecx
        pextrw    $3, %xmm4, %edx
        movl      $-1, %eax
        andl      $32752, %ecx
        subl      $16368, %ecx
        andl      $32752, %edx
        addl      %ecx, %edx
        movl      $-31, %ecx
        sarl      $4, %edx
        subl      %edx, %ecx
        jle       .L_2TAG_PACKET_49.0.2
        cmpl      $20, %ecx
        ja        .L_2TAG_PACKET_50.0.2
        shll      %cl, %eax
.L_2TAG_PACKET_49.0.2:
        movd      %eax, %xmm0
        psllq     $32, %xmm0
        andpd     %xmm5, %xmm0
        subsd     %xmm0, %xmm5
        addsd     %xmm1, %xmm5
        mulsd     %xmm4, %xmm0
        mulsd     %xmm4, %xmm5
        addsd     %xmm5, %xmm0
.L_2TAG_PACKET_50.0.2:
        jmp       .L_2TAG_PACKET_48.0.2
.L_2TAG_PACKET_2.0.2:
        movw      22(%rsp), %cx
        movl      $-2147483648, %edx
        movd      %rdx, %xmm1
        xorpd     %xmm7, %xmm7
        paddd     %xmm4, %xmm0
        movd      %xmm0, %edx
        psllq     $29, %xmm0
        paddq     %xmm3, %xmm1
        andpd     %xmm1, %xmm5
        andw      $32752, %cx
        cmpw      $16560, %cx
        jb        .L_2TAG_PACKET_3.0.2
        andpd     %xmm6, %xmm0
        subsd     %xmm5, %xmm3
        addl      $16351, %eax
        shrl      $4, %eax
        subl      $1022, %eax
        cvtsi2sd  %eax, %xmm7
        mulpd     %xmm0, %xmm5
        lea       L_tbl(%rip), %r11
        movq      coeff_h(%rip), %xmm4
        mulsd     %xmm0, %xmm3
        movq      coeff_h(%rip), %xmm6
        subsd     %xmm2, %xmm5
        movq      8+coeff_h(%rip), %xmm1
        pshufd    $68, %xmm3, %xmm2
        unpcklpd  %xmm3, %xmm5
        addsd     %xmm5, %xmm3
        movq      8+coeff_h(%rip), %xmm0
        andl      $16760832, %edx
        shrl      $10, %edx
        addpd     -3648(%r11,%rdx), %xmm7
        mulsd     %xmm5, %xmm4
        mulsd     %xmm5, %xmm0
        mulsd     %xmm2, %xmm6
        mulsd     %xmm2, %xmm1
        movq      %xmm5, %xmm2
        mulsd     %xmm5, %xmm4
        addsd     %xmm0, %xmm5
        movq      %xmm7, %xmm0
        addsd     %xmm3, %xmm2
        addsd     %xmm5, %xmm7
        mulsd     %xmm2, %xmm6
        subsd     %xmm7, %xmm0
        movq      %xmm7, %xmm2
        addsd     %xmm4, %xmm7
        addsd     %xmm5, %xmm0
        subsd     %xmm7, %xmm2
        addsd     %xmm2, %xmm4
        pshufd    $238, %xmm5, %xmm2
        movq      %xmm7, %xmm5
        addsd     %xmm2, %xmm7
        addsd     %xmm0, %xmm4
        movapd    coeff(%rip), %xmm0
        subsd     %xmm7, %xmm5
        addsd     %xmm4, %xmm6
        movq      %xmm7, %xmm4
        addsd     %xmm2, %xmm5
        addsd     %xmm1, %xmm7
        movapd    64+coeff(%rip), %xmm2
        subsd     %xmm7, %xmm4
        addsd     %xmm5, %xmm6
        addsd     %xmm1, %xmm4
        pshufd    $238, %xmm7, %xmm5
        movapd    %xmm7, %xmm1
        addsd     %xmm5, %xmm7
        subsd     %xmm7, %xmm1
        addsd     %xmm5, %xmm1
        movapd    80+coeff(%rip), %xmm5
        pshufd    $68, %xmm3, %xmm3
        addsd     %xmm4, %xmm6
        addsd     %xmm1, %xmm6
        movapd    32+coeff(%rip), %xmm1
        mulpd     %xmm3, %xmm0
        mulpd     %xmm3, %xmm2
        pshufd    $68, %xmm3, %xmm4
        mulpd     %xmm3, %xmm3
        addpd     %xmm1, %xmm0
        addpd     %xmm2, %xmm5
        mulsd     %xmm3, %xmm4
        movq      HIGHMASK_LOG_X(%rip), %xmm2
        mulpd     %xmm3, %xmm3
        movq      16(%rsp), %xmm1
        movw      22(%rsp), %cx
        mulpd     %xmm4, %xmm0
        pextrw    $3, %xmm7, %eax
        mulpd     %xmm4, %xmm5
        mulpd     %xmm3, %xmm0
        movq      8+HIGHMASK_Y(%rip), %xmm4
        andpd     %xmm7, %xmm2
        addsd     %xmm6, %xmm5
        subsd     %xmm2, %xmm7
        addpd     %xmm0, %xmm5
        andl      $32752, %eax
        subl      $16368, %eax
        andl      $32752, %ecx
        cmpl      $32752, %ecx
        je        .L_2TAG_PACKET_45.0.2
        addl      %eax, %ecx
        cmpl      $16576, %ecx
        jae       .L_2TAG_PACKET_51.0.2
        pshufd    $238, %xmm5, %xmm0
        andpd     %xmm1, %xmm4
        movq      %xmm1, %xmm3
        addsd     %xmm0, %xmm5
        subsd     %xmm4, %xmm1
        xorpd     %xmm6, %xmm6
        movl      $17080, %edx
        pinsrw    $3, %edx, %xmm6
        addsd     %xmm5, %xmm7
        mulsd     %xmm2, %xmm4
        mulsd     %xmm2, %xmm1
        movq      %xmm6, %xmm5
        mulsd     %xmm7, %xmm3
        addsd     %xmm4, %xmm6
        addsd     %xmm3, %xmm1
        movapd    e_coeff(%rip), %xmm7
        movd      %xmm6, %edx
        subsd     %xmm5, %xmm6
        lea       T_exp(%rip), %r11
        movapd    16+e_coeff(%rip), %xmm3
        movq      32+e_coeff(%rip), %xmm2
        subsd     %xmm6, %xmm4
        movl      %edx, %ecx
        andl      $255, %edx
        addl      %edx, %edx
        movapd    (%r11,%rdx,8), %xmm5
        addsd     %xmm1, %xmm4
        pextrw    $3, %xmm6, %edx
        shrl      $8, %ecx
        movl      %ecx, %eax
        shrl      $1, %ecx
        subl      %ecx, %eax
        shll      $20, %ecx
        movd      %ecx, %xmm6
        pshufd    $68, %xmm4, %xmm0
        pshufd    $68, %xmm4, %xmm1
        mulpd     %xmm0, %xmm0
        mulpd     %xmm1, %xmm7
        pshufd    $17, %xmm6, %xmm6
        mulsd     %xmm4, %xmm2
        andl      $32767, %edx
        cmpl      $16529, %edx
        ja        .L_2TAG_PACKET_12.0.2
        mulsd     %xmm0, %xmm0
        paddd     %xmm6, %xmm5
        addpd     %xmm7, %xmm3
        mulsd     %xmm5, %xmm2
        pshufd    $238, %xmm5, %xmm6
        mulpd     %xmm3, %xmm0
        addsd     %xmm6, %xmm2
        pshufd    $238, %xmm0, %xmm3
        addl      $1023, %eax
        shll      $20, %eax
        orl       %r8d, %eax
        movd      %eax, %xmm4
        mulsd     %xmm5, %xmm0
        mulsd     %xmm5, %xmm3
        addsd     %xmm2, %xmm0
        psllq     $32, %xmm4
        addsd     %xmm3, %xmm0
        movq      %xmm0, %xmm1
        addsd     %xmm5, %xmm0
        mulsd     %xmm4, %xmm0
        pextrw    $3, %xmm0, %eax
        andl      $32752, %eax
        je        .L_2TAG_PACKET_13.0.2
        cmpl      $32752, %eax
        je        .L_2TAG_PACKET_14.0.2
.L_2TAG_PACKET_52.0.2:
        jmp       ..B1.5
.L_2TAG_PACKET_45.0.2:
        movq      8(%rsp), %xmm0
        xorpd     %xmm2, %xmm2
        movl      $49136, %eax
        pinsrw    $3, %eax, %xmm2
        addsd     %xmm0, %xmm2
        pextrw    $3, %xmm2, %eax
        cmpl      $0, %eax
        jne       .L_2TAG_PACKET_53.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_53.0.2:
        movq      16(%rsp), %xmm1
        movd      %xmm1, %edx
        movq      %xmm1, %xmm3
        psrlq     $20, %xmm3
        movd      %xmm3, %ecx
        orl       %edx, %ecx
        je        .L_2TAG_PACKET_54.0.2
        addsd     %xmm1, %xmm1
        movq      %xmm1, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_51.0.2:
        pextrw    $3, %xmm1, %eax
        pextrw    $3, %xmm2, %ecx
        xorl      %ecx, %eax
        testl     $32768, %eax
        je        .L_2TAG_PACKET_47.0.2
        jmp       .L_2TAG_PACKET_46.0.2
.L_2TAG_PACKET_54.0.2:
        pextrw    $3, %xmm0, %eax
        andl      $32752, %eax
        pextrw    $3, %xmm1, %edx
        xorpd     %xmm0, %xmm0
        subl      $16368, %eax
        xorl      %edx, %eax
        testl     $32768, %eax
        je        .L_2TAG_PACKET_55.0.2
        jmp       ..B1.5
.L_2TAG_PACKET_55.0.2:
        movl      $32752, %edx
        pinsrw    $3, %edx, %xmm0
        jmp       ..B1.5
.L_2TAG_PACKET_17.0.2:
        movq      %xmm0, 24(%rsp)
..B1.3:
        movq      24(%rsp), %xmm0
.L_2TAG_PACKET_56.0.2:
..B1.5:
        addq      $40, %rsp
..___tag_value_pow.4:
        ret       
..___tag_value_pow.5:
END(v8_base_pow)
# -- End  pow
	.section .rodata, "a"
	.align 16
	.align 16
HIGHSIGMASK:
	.long	0
	.long	4294965248
	.long	0
	.long	4294965248
	.type	HIGHSIGMASK,@object
	.size	HIGHSIGMASK,16
	.align 16
LOG2_E:
	.long	0
	.long	1073160192
	.long	370913857
	.long	3210587105
	.type	LOG2_E,@object
	.size	LOG2_E,16
	.align 16
coeff:
	.long	1841914130
	.long	3213059448
	.long	3995341938
	.long	3214607105
	.long	2677381210
	.long	3216320731
	.long	3011779882
	.long	3218479542
	.long	1367832035
	.long	1066403058
	.long	2894285243
	.long	1067936923
	.long	1215221452
	.long	1069835102
	.long	370913857
	.long	3210587105
	.long	2677381210
	.long	3216320731
	.long	4172642429
	.long	1056068382
	.long	1215221451
	.long	1069835102
	.long	1092638156
	.long	3184925618
	.type	coeff,@object
	.size	coeff,96
	.align 16
L_tbl:
	.long	0
	.long	1072693248
	.long	0
	.long	0
	.long	536870912
	.long	1072689162
	.long	2523013013
	.long	1046157398
	.long	3758096384
	.long	1072685081
	.long	3851513758
	.long	3190968952
	.long	0
	.long	1072681007
	.long	2241466466
	.long	1046044599
	.long	3221225472
	.long	1072676937
	.long	2990928271
	.long	3193084984
	.long	3758096384
	.long	1072672873
	.long	2905112743
	.long	3192918576
	.long	1610612736
	.long	1072668815
	.long	3370591264
	.long	1046051793
	.long	2147483648
	.long	1072664762
	.long	3272361216
	.long	3193793653
	.long	3758096384
	.long	1072660714
	.long	46546755
	.long	1043206936
	.long	3221225472
	.long	1072656672
	.long	3017067724
	.long	3192177962
	.long	0
	.long	1072652636
	.long	3688436631
	.long	3192814956
	.long	2684354560
	.long	1072648604
	.long	1707461992
	.long	3193056712
	.long	2684354560
	.long	1072644578
	.long	1188114540
	.long	3193603086
	.long	3758096384
	.long	1072640557
	.long	3533180564
	.long	1045459375
	.long	2684354560
	.long	1072636542
	.long	2000337630
	.long	3193475557
	.long	2684354560
	.long	1072632532
	.long	3698062443
	.long	3193752766
	.long	3758096384
	.long	1072628527
	.long	3161606138
	.long	3190532995
	.long	2147483648
	.long	1072624528
	.long	3165265478
	.long	3193158459
	.long	1610612736
	.long	1072620534
	.long	1600940077
	.long	3193226777
	.long	2147483648
	.long	1072616545
	.long	1363272552
	.long	3192614278
	.long	3758096384
	.long	1072612561
	.long	3966209910
	.long	3191249654
	.long	2147483648
	.long	1072608583
	.long	1093672789
	.long	3190637330
	.long	1610612736
	.long	1072604610
	.long	1735239357
	.long	3192753616
	.long	1610612736
	.long	1072600642
	.long	1470665156
	.long	1045559697
	.long	2684354560
	.long	1072596679
	.long	3840624926
	.long	1045928953
	.long	536870912
	.long	1072592722
	.long	4259072556
	.long	3191035622
	.long	3221225472
	.long	1072588769
	.long	3613088753
	.long	3192165681
	.long	2147483648
	.long	1072584822
	.long	3175234446
	.long	1039486948
	.long	1610612736
	.long	1072580880
	.long	856576441
	.long	1045702812
	.long	2147483648
	.long	1072576943
	.long	2253498719
	.long	3193285334
	.long	2684354560
	.long	1072573011
	.long	1587070728
	.long	3190801577
	.long	3758096384
	.long	1072569084
	.long	159986317
	.long	1042519436
	.long	1073741824
	.long	1072565163
	.long	3999541949
	.long	3192020440
	.long	2684354560
	.long	1072561246
	.long	3281310262
	.long	1045586786
	.long	536870912
	.long	1072557335
	.long	3775179406
	.long	1045226055
	.long	3221225472
	.long	1072553428
	.long	643472356
	.long	3193681786
	.long	1073741824
	.long	1072549527
	.long	248169775
	.long	1045068977
	.long	3758096384
	.long	1072545630
	.long	307016632
	.long	1042640932
	.long	2147483648
	.long	1072541739
	.long	3872718526
	.long	3189781486
	.long	536870912
	.long	1072537853
	.long	969711630
	.long	3191724732
	.long	3221225472
	.long	1072533971
	.long	4018820394
	.long	3193189264
	.long	1073741824
	.long	1072530095
	.long	3102233092
	.long	1045510224
	.long	3758096384
	.long	1072526223
	.long	1029307912
	.long	3193812776
	.long	1073741824
	.long	1072522357
	.long	984083153
	.long	1045987403
	.long	3221225472
	.long	1072518495
	.long	4171455401
	.long	3193084080
	.long	0
	.long	1072514639
	.long	2592660757
	.long	1046121691
	.long	1073741824
	.long	1072510787
	.long	2964365712
	.long	1046054453
	.long	2147483648
	.long	1072506940
	.long	3792777877
	.long	3193704729
	.long	2147483648
	.long	1072503098
	.long	2948536104
	.long	3192467100
	.long	1610612736
	.long	1072499261
	.long	3836005619
	.long	1041873166
	.long	536870912
	.long	1072495429
	.long	3124543160
	.long	1044409168
	.long	3221225472
	.long	1072491601
	.long	286227933
	.long	1041065990
	.long	1073741824
	.long	1072487779
	.long	2111296776
	.long	3193604419
	.long	2147483648
	.long	1072483961
	.long	2606822001
	.long	3192940394
	.long	2147483648
	.long	1072480148
	.long	194696800
	.long	1046026063
	.long	1610612736
	.long	1072476340
	.long	8535452
	.long	1046200178
	.long	536870912
	.long	1072472537
	.long	950463625
	.long	3192731897
	.long	2147483648
	.long	1072468738
	.long	973831566
	.long	1045683197
	.long	3221225472
	.long	1072464944
	.long	3330435892
	.long	3190277577
	.long	3221225472
	.long	1072461155
	.long	208692097
	.long	3193517651
	.long	1610612736
	.long	1072457371
	.long	2113097415
	.long	1044781749
	.long	3758096384
	.long	1072453591
	.long	1088808936
	.long	3193716142
	.long	0
	.long	1072449817
	.long	1443002127
	.long	3193250205
	.long	3221225472
	.long	1072446046
	.long	3967357419
	.long	1046109477
	.long	1610612736
	.long	1072442281
	.long	3013517861
	.long	3193159691
	.long	2147483648
	.long	1072438520
	.long	2524586286
	.long	1046121951
	.long	1610612736
	.long	1072434764
	.long	1476892861
	.long	1046434731
	.long	0
	.long	1072431013
	.long	3089640950
	.long	3192305780
	.long	536870912
	.long	1072427266
	.long	3812255529
	.long	1045730879
	.long	0
	.long	1072423524
	.long	995354762
	.long	3191528673
	.long	1610612736
	.long	1072419786
	.long	3260567684
	.long	1046273695
	.long	2147483648
	.long	1072416053
	.long	2738210286
	.long	3191471516
	.long	536870912
	.long	1072412325
	.long	1931849805
	.long	1044560405
	.long	1610612736
	.long	1072408601
	.long	358896655
	.long	1044029237
	.long	1073741824
	.long	1072404882
	.long	2214589842
	.long	3193202126
	.long	2684354560
	.long	1072401167
	.long	3118097363
	.long	3192592906
	.long	2147483648
	.long	1072397457
	.long	1835998884
	.long	1045788247
	.long	0
	.long	1072393752
	.long	1585488319
	.long	1045289910
	.long	0
	.long	1072390051
	.long	480160949
	.long	1046030455
	.long	2684354560
	.long	1072386354
	.long	1832959667
	.long	3193013644
	.long	2684354560
	.long	1072382662
	.long	3611346555
	.long	1044544210
	.long	1073741824
	.long	1072378975
	.long	2749418734
	.long	3193712580
	.long	1073741824
	.long	1072375292
	.long	2390043472
	.long	3191710658
	.long	3221225472
	.long	1072371613
	.long	2828199902
	.long	1042265217
	.long	3221225472
	.long	1072367939
	.long	569209321
	.long	3191230982
	.long	536870912
	.long	1072364270
	.long	236159139
	.long	1046240123
	.long	536870912
	.long	1072360605
	.long	1010656270
	.long	3193813968
	.long	1610612736
	.long	1072356944
	.long	2409080597
	.long	1044025029
	.long	536870912
	.long	1072353288
	.long	598419513
	.long	1043327370
	.long	1073741824
	.long	1072349636
	.long	4105950479
	.long	1045747958
	.long	3758096384
	.long	1072345988
	.long	343243853
	.long	3192420172
	.long	3221225472
	.long	1072342345
	.long	2088439530
	.long	1046172091
	.long	536870912
	.long	1072338707
	.long	4117721107
	.long	1043882496
	.long	3758096384
	.long	1072335072
	.long	3192032958
	.long	3192998645
	.long	3758096384
	.long	1072331442
	.long	2366522518
	.long	1045401957
	.long	1610612736
	.long	1072327817
	.long	3685533141
	.long	3193701947
	.long	536870912
	.long	1072324196
	.long	1058658672
	.long	3193572492
	.long	536870912
	.long	1072320579
	.long	166346347
	.long	1045456348
	.long	2147483648
	.long	1072316966
	.long	2027889772
	.long	1046349302
	.long	1073741824
	.long	1072313358
	.long	1079497888
	.long	1044585259
	.long	1073741824
	.long	1072309754
	.long	2189851573
	.long	1045132990
	.long	2684354560
	.long	1072306154
	.long	2486629386
	.long	3193613625
	.long	536870912
	.long	1072302559
	.long	1263686579
	.long	1044789259
	.long	0
	.long	1072298968
	.long	2412061798
	.long	3191369627
	.long	536870912
	.long	1072295381
	.long	584315716
	.long	3193144135
	.long	1610612736
	.long	1072291798
	.long	449000738
	.long	1046330451
	.long	0
	.long	1072288220
	.long	3938320157
	.long	1044446220
	.long	3758096384
	.long	1072284645
	.long	2949844595
	.long	3193462371
	.long	3758096384
	.long	1072281075
	.long	2771329642
	.long	3192121593
	.long	536870912
	.long	1072277510
	.long	3971508621
	.long	3193002806
	.long	2147483648
	.long	1072273948
	.long	4071942301
	.long	1044952619
	.long	536870912
	.long	1072270391
	.long	2090502395
	.long	1044660556
	.long	0
	.long	1072266838
	.long	3657520961
	.long	3193770938
	.long	3758096384
	.long	1072263288
	.long	1608175110
	.long	1045543239
	.long	0
	.long	1072259744
	.long	2506924180
	.long	1045530501
	.long	1073741824
	.long	1072256203
	.long	18238493
	.long	1046305623
	.long	3221225472
	.long	1072252666
	.long	3862640487
	.long	3192882407
	.long	1073741824
	.long	1072249134
	.long	3850158761
	.long	1043656099
	.long	3758096384
	.long	1072245605
	.long	2356524356
	.long	1045915296
	.long	3221225472
	.long	1072242081
	.long	936497287
	.long	3193842353
	.long	2147483648
	.long	1072238561
	.long	2840845344
	.long	1046454771
	.long	2147483648
	.long	1072235045
	.long	3688100713
	.long	1044895451
	.long	2684354560
	.long	1072231533
	.long	479979913
	.long	3193842442
	.long	2684354560
	.long	1072228025
	.long	1016321898
	.long	1046251032
	.long	3758096384
	.long	1072224521
	.long	562232474
	.long	3191974558
	.long	536870912
	.long	1072221022
	.long	3870512029
	.long	3193113881
	.long	1610612736
	.long	1072217526
	.long	1239780547
	.long	3191583604
	.long	2684354560
	.long	1072214034
	.long	2815421327
	.long	1045873682
	.long	0
	.long	1072210547
	.long	2371009561
	.long	1041508792
	.long	1610612736
	.long	1072207063
	.long	1304636524
	.long	3192414284
	.long	3221225472
	.long	1072203583
	.long	210144854
	.long	3193327333
	.long	0
	.long	1072200108
	.long	1454303272
	.long	1046360024
	.long	1610612736
	.long	1072196636
	.long	2095757548
	.long	1044984677
	.long	3221225472
	.long	1072193168
	.long	2027215580
	.long	3192880933
	.long	0
	.long	1072189705
	.long	214794880
	.long	1043457954
	.long	1073741824
	.long	1072186245
	.long	884624917
	.long	1043497079
	.long	2147483648
	.long	1072182789
	.long	2792396634
	.long	3193171685
	.long	2684354560
	.long	1072179337
	.long	4128995250
	.long	3192103434
	.long	2684354560
	.long	1072175889
	.long	333866043
	.long	1046372325
	.long	3221225472
	.long	1072172445
	.long	2194445544
	.long	3193958905
	.long	2684354560
	.long	1072169005
	.long	2316082269
	.long	3192041703
	.long	1610612736
	.long	1072165569
	.long	581005057
	.long	1046322848
	.long	536870912
	.long	1072162137
	.long	3280786513
	.long	1045457251
	.long	3221225472
	.long	1072158708
	.long	2567093361
	.long	1044710359
	.long	1073741824
	.long	1072155284
	.long	3740443584
	.long	1044224237
	.long	2684354560
	.long	1072151863
	.long	3981028272
	.long	1042596351
	.long	3758096384
	.long	1072148446
	.long	3820011120
	.long	3191915623
	.long	0
	.long	1072145034
	.long	2946439484
	.long	3193831276
	.long	3758096384
	.long	1072141624
	.long	3075274422
	.long	3190132432
	.long	2684354560
	.long	1072138219
	.long	496052167
	.long	1043619760
	.long	1073741824
	.long	1072134818
	.long	271106589
	.long	3192265149
	.long	2684354560
	.long	1072131420
	.long	2091955684
	.long	1044443554
	.long	3758096384
	.long	1072128026
	.long	723240109
	.long	3191007419
	.long	3758096384
	.long	1072124636
	.long	1748629070
	.long	1044510075
	.long	3221225472
	.long	1072121250
	.long	3289522046
	.long	3193095178
	.long	1610612736
	.long	1072117868
	.long	3599052146
	.long	3193720427
	.long	3221225472
	.long	1072114489
	.long	2446758135
	.long	3193436303
	.long	3758096384
	.long	1072111114
	.long	1652171097
	.long	3192137173
	.long	3221225472
	.long	1072107743
	.long	1353007155
	.long	1044523902
	.long	1610612736
	.long	1072104376
	.long	990601105
	.long	1046296663
	.long	3758096384
	.long	1072101012
	.long	2228627618
	.long	3193041040
	.long	0
	.long	1072097653
	.long	812484756
	.long	3191950723
	.long	3758096384
	.long	1072094296
	.long	817833130
	.long	3192279242
	.long	2147483648
	.long	1072090944
	.long	3563228521
	.long	3193810951
	.long	3221225472
	.long	1072087595
	.long	2729108859
	.long	3190936185
	.long	3221225472
	.long	1072084250
	.long	2249121662
	.long	3190639690
	.long	2147483648
	.long	1072080909
	.long	4082471745
	.long	3193929368
	.long	3758096384
	.long	1072077571
	.long	2827323806
	.long	3193708561
	.long	3758096384
	.long	1072074237
	.long	735866167
	.long	1042434690
	.long	2684354560
	.long	1072070907
	.long	3240808889
	.long	3191918422
	.long	0
	.long	1072067581
	.long	466482777
	.long	3186962221
	.long	0
	.long	1072064258
	.long	1576076296
	.long	1045849056
	.long	3221225472
	.long	1072060938
	.long	2751923560
	.long	3191910703
	.long	0
	.long	1072057623
	.long	1908755527
	.long	1046437515
	.long	0
	.long	1072054311
	.long	3175841411
	.long	1044572886
	.long	2684354560
	.long	1072051002
	.long	1633258450
	.long	3192670420
	.long	3221225472
	.long	1072047697
	.long	1867746657
	.long	1045726209
	.long	2684354560
	.long	1072044396
	.long	338968864
	.long	3193084662
	.long	0
	.long	1072041099
	.long	1501742471
	.long	3191742031
	.long	0
	.long	1072037805
	.long	4266775786
	.long	3192686970
	.long	2147483648
	.long	1072034514
	.long	4249283553
	.long	1045769728
	.long	2684354560
	.long	1072031227
	.long	2758366873
	.long	1046402161
	.long	1610612736
	.long	1072027944
	.long	2161186990
	.long	1044736865
	.long	2684354560
	.long	1072024664
	.long	810300171
	.long	1045748777
	.long	2147483648
	.long	1072021388
	.long	183688927
	.long	3191515581
	.long	3758096384
	.long	1072018115
	.long	368874072
	.long	3192363575
	.long	3221225472
	.long	1072014846
	.long	2459092970
	.long	1041794640
	.long	536870912
	.long	1072011581
	.long	867488640
	.long	1046310291
	.long	536870912
	.long	1072008319
	.long	50140871
	.long	1043327329
	.long	2684354560
	.long	1072005060
	.long	1241902518
	.long	3192739252
	.long	2684354560
	.long	1072001805
	.long	1027881659
	.long	3193858388
	.long	0
	.long	1071998554
	.long	38457322
	.long	1045489179
	.long	0
	.long	1071995306
	.long	3432963337
	.long	3190969347
	.long	1610612736
	.long	1071992061
	.long	534931792
	.long	1046302734
	.long	1610612736
	.long	1071988820
	.long	1817895268
	.long	3192551860
	.long	3221225472
	.long	1071985582
	.long	357237383
	.long	3191870833
	.long	2684354560
	.long	1071982348
	.long	108262401
	.long	3193365867
	.long	3758096384
	.long	1071979117
	.long	1964729244
	.long	1042502249
	.long	2684354560
	.long	1071975890
	.long	2088446957
	.long	1038010503
	.long	3221225472
	.long	1071972666
	.long	2947239447
	.long	1046377845
	.long	1610612736
	.long	1071969446
	.long	774932072
	.long	1046064854
	.long	2147483648
	.long	1071966229
	.long	4080937590
	.long	3193041284
	.long	3758096384
	.long	1071963015
	.long	2208251454
	.long	1045945089
	.long	3221225472
	.long	1071959805
	.long	2850924475
	.long	1045650959
	.long	0
	.long	1071956599
	.long	714040997
	.long	1046275153
	.long	3221225472
	.long	1071953395
	.long	85533782
	.long	3192816920
	.long	3221225472
	.long	1071950195
	.long	1252511005
	.long	1044805706
	.long	1073741824
	.long	1071946999
	.long	2384659038
	.long	3193391602
	.long	0
	.long	1071943806
	.long	416481813
	.long	1043730233
	.long	536870912
	.long	1071940616
	.long	1675424499
	.long	1046348030
	.long	3221225472
	.long	1071937429
	.long	1175989513
	.long	3193009113
	.long	2684354560
	.long	1071934246
	.long	2400084650
	.long	3192451713
	.long	3758096384
	.long	1071931066
	.long	1467335692
	.long	3193350868
	.long	1610612736
	.long	1071927890
	.long	266493801
	.long	1044954481
	.long	1073741824
	.long	1071924717
	.long	3919093445
	.long	1046023575
	.long	2147483648
	.long	1071921547
	.long	3017408483
	.long	1044880828
	.long	536870912
	.long	1071918381
	.long	948849966
	.long	3193892224
	.long	3758096384
	.long	1071915217
	.long	1870232600
	.long	1045777228
	.long	536870912
	.long	1071912058
	.long	822381492
	.long	3193639186
	.long	2147483648
	.long	1071908901
	.long	788243705
	.long	1044966343
	.long	1073741824
	.long	1071905748
	.long	1344278809
	.long	1044428545
	.long	1073741824
	.long	1071902598
	.long	172864300
	.long	1045765608
	.long	2684354560
	.long	1071899451
	.long	211555467
	.long	3192963574
	.long	536870912
	.long	1071896308
	.long	3373438023
	.long	1045643168
	.long	0
	.long	1071893168
	.long	2867180960
	.long	3189945998
	.long	536870912
	.long	1071890031
	.long	36724362
	.long	3193240584
	.long	1610612736
	.long	1071886897
	.long	2140176984
	.long	1045945349
	.long	0
	.long	1071883767
	.long	436842360
	.long	1040712587
	.long	3758096384
	.long	1071880639
	.long	1225147329
	.long	3193814594
	.long	3758096384
	.long	1071877515
	.long	1586157348
	.long	3191614322
	.long	536870912
	.long	1071874395
	.long	3329332918
	.long	1041699791
	.long	2684354560
	.long	1071871277
	.long	1635968041
	.long	3191783756
	.long	1073741824
	.long	1071868163
	.long	2876158382
	.long	1046097093
	.long	1073741824
	.long	1071865052
	.long	4267556964
	.long	3193723000
	.long	1073741824
	.long	1071861944
	.long	195475940
	.long	1045520795
	.long	2147483648
	.long	1071858839
	.long	2239193514
	.long	1046478675
	.long	0
	.long	1071855738
	.long	4168275596
	.long	1044926285
	.long	2684354560
	.long	1071852639
	.long	142514114
	.long	1045595182
	.long	2147483648
	.long	1071849544
	.long	1943457984
	.long	3192930015
	.long	2147483648
	.long	1071846452
	.long	202659489
	.long	3193926317
	.long	2684354560
	.long	1071843363
	.long	2208408789
	.long	3193857484
	.long	3758096384
	.long	1071840277
	.long	2237297552
	.long	3192939576
	.long	1073741824
	.long	1071837195
	.long	2726920839
	.long	1044193954
	.long	3758096384
	.long	1071834115
	.long	2337732207
	.long	3193611773
	.long	2147483648
	.long	1071831039
	.long	1390088602
	.long	1044000317
	.long	1610612736
	.long	1071827966
	.long	3806188736
	.long	3193463913
	.long	1073741824
	.long	1071824896
	.long	1795276560
	.long	1043671965
	.long	1073741824
	.long	1071821829
	.long	2960792799
	.long	1046240474
	.long	2147483648
	.long	1071818765
	.long	3350591592
	.long	3193333939
	.long	3221225472
	.long	1071815704
	.long	408870754
	.long	3193322854
	.long	0
	.long	1071812647
	.long	4146717132
	.long	1046063520
	.long	2147483648
	.long	1071809592
	.long	1681114919
	.long	3192114313
	.long	0
	.long	1071806541
	.long	1098393137
	.long	3190846732
	.long	2684354560
	.long	1071803492
	.long	2437484983
	.long	3193448718
	.long	1073741824
	.long	1071800447
	.long	1036809185
	.long	3192023501
	.long	0
	.long	1071797405
	.long	659668848
	.long	3193596312
	.long	3221225472
	.long	1071794365
	.long	1112062459
	.long	3192773376
	.long	2147483648
	.long	1071791329
	.long	4082956335
	.long	1045830513
	.long	1610612736
	.long	1071788296
	.long	2387089965
	.long	1045532601
	.long	1610612736
	.long	1071785266
	.long	1522101980
	.long	3193941957
	.long	1073741824
	.long	1071782239
	.long	2157197585
	.long	3188193305
	.long	1073741824
	.long	1071779215
	.long	946810220
	.long	3193223819
	.long	1073741824
	.long	1071776194
	.long	4069942444
	.long	3193878549
	.long	536870912
	.long	1071773176
	.long	1693463440
	.long	1046360588
	.long	536870912
	.long	1071770161
	.long	1954543254
	.long	1046409381
	.long	1073741824
	.long	1071767149
	.long	1050471249
	.long	3193933095
	.long	536870912
	.long	1071764140
	.long	1256240478
	.long	1046456865
	.long	536870912
	.long	1071761134
	.long	676764254
	.long	1046055503
	.long	536870912
	.long	1071758131
	.long	1421032967
	.long	1044779786
	.long	536870912
	.long	1071755131
	.long	38735992
	.long	3192766355
	.long	0
	.long	1071752134
	.long	2960669690
	.long	1044484680
	.long	3758096384
	.long	1071749139
	.long	788707382
	.long	1045299895
	.long	3221225472
	.long	1071746148
	.long	685689300
	.long	1040778831
	.long	2147483648
	.long	1071743160
	.long	1170994182
	.long	1046159174
	.long	1073741824
	.long	1071740175
	.long	64591436
	.long	1046153849
	.long	0
	.long	1071737193
	.long	2338031659
	.long	3189997702
	.long	2684354560
	.long	1071734213
	.long	1941624568
	.long	3186752676
	.long	536870912
	.long	1071731237
	.long	1401255580
	.long	1046383990
	.long	2684354560
	.long	1071728263
	.long	376888427
	.long	1045896456
	.long	536870912
	.long	1071725293
	.long	2831424639
	.long	3193539109
	.long	1610612736
	.long	1071722325
	.long	3303123696
	.long	1044599415
	.long	2684354560
	.long	1071719360
	.long	1077295329
	.long	3189877372
	.long	3221225472
	.long	1071716398
	.long	1434061099
	.long	3184529771
	.long	3221225472
	.long	1071713439
	.long	2104991590
	.long	1045062074
	.long	3221225472
	.long	1071710483
	.long	722060869
	.long	3193788526
	.long	536870912
	.long	1071704580
	.long	3928796486
	.long	1046129020
	.long	536870912
	.long	1071698688
	.long	588844628
	.long	1045492135
	.long	2684354560
	.long	1071692807
	.long	326739366
	.long	3193004445
	.long	1610612736
	.long	1071686938
	.long	2456436042
	.long	1046278169
	.long	2684354560
	.long	1071681080
	.long	2831303512
	.long	1043670046
	.long	536870912
	.long	1071675234
	.long	607223418
	.long	1045507322
	.long	0
	.long	1071669399
	.long	4254921332
	.long	3193290483
	.long	0
	.long	1071663575
	.long	914994333
	.long	3191263853
	.long	1073741824
	.long	1071657762
	.long	4147050180
	.long	3193228552
	.long	2684354560
	.long	1071651960
	.long	594554157
	.long	3193503935
	.long	0
	.long	1071646170
	.long	1062846796
	.long	1045944331
	.long	1073741824
	.long	1071636109
	.long	2909238893
	.long	3193436884
	.long	1073741824
	.long	1071624572
	.long	1682918119
	.long	1042211899
	.long	1073741824
	.long	1071613057
	.long	2419209426
	.long	1045437062
	.long	1073741824
	.long	1071601564
	.long	2951341321
	.long	3190193214
	.long	0
	.long	1071590093
	.long	3084900875
	.long	3192394907
	.long	1073741824
	.long	1071578643
	.long	999567454
	.long	1046433447
	.long	2147483648
	.long	1071567215
	.long	1570101857
	.long	3193291160
	.long	0
	.long	1071555809
	.long	1080647881
	.long	3185154585
	.long	0
	.long	1071544424
	.long	3526309177
	.long	1044843640
	.long	2147483648
	.long	1071533060
	.long	2213463349
	.long	3191738930
	.long	1073741824
	.long	1071521718
	.long	1039925195
	.long	3192618353
	.long	1073741824
	.long	1071510397
	.long	2115757280
	.long	3193671567
	.long	1073741824
	.long	1071499097
	.long	1188751495
	.long	3191145560
	.long	2147483648
	.long	1071487818
	.long	3983461449
	.long	3193897029
	.long	2147483648
	.long	1071476560
	.long	782141500
	.long	1042879962
	.long	2147483648
	.long	1071465323
	.long	4038904626
	.long	1045063881
	.long	2147483648
	.long	1071454107
	.long	2613036921
	.long	3193217642
	.long	0
	.long	1071442912
	.long	2095723435
	.long	1044629175
	.long	1073741824
	.long	1071431737
	.long	3879795974
	.long	1045767874
	.long	1073741824
	.long	1071420583
	.long	2662198042
	.long	3191434637
	.long	3221225472
	.long	1071409449
	.long	4037605722
	.long	3193703090
	.long	2147483648
	.long	1071398336
	.long	1860331835
	.long	1040814822
	.long	3221225472
	.long	1071387243
	.long	1522972033
	.long	3190305974
	.long	1073741824
	.long	1071376171
	.long	2361534207
	.long	1043699366
	.long	0
	.long	1071365119
	.long	4180309179
	.long	1044142099
	.long	0
	.long	1071354087
	.long	1201038528
	.long	3192968772
	.long	0
	.long	1071343075
	.long	1342478171
	.long	3193251215
	.long	0
	.long	1071332083
	.long	3836883348
	.long	3193472007
	.long	3221225472
	.long	1071321110
	.long	3864874250
	.long	1045593126
	.long	2147483648
	.long	1071310158
	.long	2169494998
	.long	1046045346
	.long	1073741824
	.long	1071299226
	.long	3785165075
	.long	3193319246
	.long	2147483648
	.long	1071288313
	.long	1137692678
	.long	3192716779
	.long	1073741824
	.long	1071277420
	.long	1752107598
	.long	1046366120
	.long	3221225472
	.long	1071266546
	.long	1912656912
	.long	1046352281
	.long	3221225472
	.long	1071255692
	.long	2882676334
	.long	1046406353
	.long	1073741824
	.long	1071244858
	.long	963612460
	.long	1045282811
	.long	0
	.long	1071234043
	.long	3811255773
	.long	1046231636
	.long	1073741824
	.long	1071223247
	.long	1126055989
	.long	3192224037
	.long	2147483648
	.long	1071212470
	.long	2079145427
	.long	1044432413
	.long	0
	.long	1071201713
	.long	3611595621
	.long	1043358745
	.long	2147483648
	.long	1071190974
	.long	390522769
	.long	1045888252
	.long	1073741824
	.long	1071180255
	.long	4087939723
	.long	3192930745
	.long	3221225472
	.long	1071169554
	.long	1451494480
	.long	3190219274
	.long	1073741824
	.long	1071158873
	.long	427176194
	.long	3193042022
	.long	2147483648
	.long	1071148210
	.long	1882381948
	.long	3192727946
	.long	2147483648
	.long	1071137566
	.long	3736313771
	.long	3192087019
	.long	1073741824
	.long	1071126941
	.long	1560398816
	.long	3193185715
	.long	2147483648
	.long	1071116334
	.long	1021942441
	.long	1041526696
	.long	2147483648
	.long	1071105746
	.long	3517080249
	.long	3193576041
	.long	3221225472
	.long	1071095176
	.long	2248589878
	.long	1044527624
	.long	2147483648
	.long	1071084625
	.long	2412896695
	.long	1046112867
	.long	3221225472
	.long	1071074092
	.long	3834725738
	.long	1044562378
	.long	1073741824
	.long	1071063578
	.long	1150920407
	.long	1043768986
	.long	0
	.long	1071053082
	.long	1379393428
	.long	3188690690
	.long	0
	.long	1071042604
	.long	3058183278
	.long	3193617655
	.long	0
	.long	1071032144
	.long	421133665
	.long	3193417186
	.long	0
	.long	1071021702
	.long	2860161357
	.long	3191816125
	.long	0
	.long	1071011278
	.long	1742405964
	.long	1043580240
	.long	0
	.long	1071000872
	.long	2821215927
	.long	3188984273
	.long	3221225472
	.long	1070990483
	.long	510275597
	.long	1045813401
	.long	2147483648
	.long	1070980113
	.long	304266588
	.long	3191193536
	.long	3221225472
	.long	1070969760
	.long	1854784211
	.long	1046302073
	.long	0
	.long	1070959426
	.long	3773082854
	.long	3193008899
	.long	2147483648
	.long	1070949108
	.long	3003572392
	.long	1046404879
	.long	3221225472
	.long	1070938808
	.long	1702149204
	.long	1046407257
	.long	2147483648
	.long	1070928526
	.long	3935314439
	.long	1046438280
	.long	3221225472
	.long	1070918261
	.long	2677087609
	.long	1045501749
	.long	2147483648
	.long	1070908014
	.long	4190598039
	.long	3193640515
	.long	1073741824
	.long	1070897784
	.long	368874072
	.long	1044879927
	.long	2147483648
	.long	1070887571
	.long	3584052697
	.long	3192024662
	.long	3221225472
	.long	1070877375
	.long	3762307829
	.long	1045886918
	.long	1073741824
	.long	1070867197
	.long	495710920
	.long	1046317072
	.long	0
	.long	1070857036
	.long	2292768238
	.long	3190887508
	.long	3221225472
	.long	1070846891
	.long	1044078151
	.long	3193772914
	.long	1073741824
	.long	1070836764
	.long	3266010457
	.long	1043443755
	.long	3221225472
	.long	1070826653
	.long	3571665822
	.long	1045547823
	.long	1073741824
	.long	1070816560
	.long	393348347
	.long	3190525143
	.long	2147483648
	.long	1070806483
	.long	4241722498
	.long	3192084193
	.long	2147483648
	.long	1070796423
	.long	1693797068
	.long	3192807972
	.long	0
	.long	1070786380
	.long	2860086745
	.long	1046331646
	.long	2147483648
	.long	1070776353
	.long	1366141759
	.long	3192979363
	.long	1073741824
	.long	1070766343
	.long	737899283
	.long	1045853346
	.long	3221225472
	.long	1070756349
	.long	88734873
	.long	1043881257
	.long	3221225472
	.long	1070746372
	.long	1438003315
	.long	3192917101
	.long	0
	.long	1070736412
	.long	1066505530
	.long	1043896695
	.long	3221225472
	.long	1070726467
	.long	2706653041
	.long	3191113643
	.long	3221225472
	.long	1070716539
	.long	1321764476
	.long	1039573724
	.long	0
	.long	1070706628
	.long	1126753211
	.long	1044502976
	.long	2147483648
	.long	1070696732
	.long	773642884
	.long	1044110727
	.long	1073741824
	.long	1070686853
	.long	1263743406
	.long	3193115278
	.long	0
	.long	1070676990
	.long	3115237732
	.long	3193089176
	.long	3221225472
	.long	1070667142
	.long	3642626838
	.long	3191146032
	.long	2147483648
	.long	1070657311
	.long	2091696428
	.long	1044337177
	.long	1073741824
	.long	1070647496
	.long	3168958391
	.long	1044197568
	.long	0
	.long	1070637697
	.long	711148669
	.long	3193181047
	.long	2147483648
	.long	1070627913
	.long	4207182773
	.long	3193402092
	.long	3221225472
	.long	1070618145
	.long	918070640
	.long	3192902845
	.long	3221225472
	.long	1070608393
	.long	3135571447
	.long	3192193928
	.long	2147483648
	.long	1070598657
	.long	1043705517
	.long	3193188604
	.long	2147483648
	.long	1070581777
	.long	1886680492
	.long	1043890286
	.long	2147483648
	.long	1070562367
	.long	3373799420
	.long	3191917802
	.long	2147483648
	.long	1070542988
	.long	2919618025
	.long	3192461752
	.long	2147483648
	.long	1070523640
	.long	2926365158
	.long	3193113492
	.long	0
	.long	1070504323
	.long	519978638
	.long	1045918846
	.long	0
	.long	1070485037
	.long	3665353151
	.long	3193546248
	.long	0
	.long	1070465781
	.long	2327718958
	.long	1045050797
	.long	0
	.long	1070446556
	.long	345326861
	.long	3188224716
	.long	2147483648
	.long	1070427361
	.long	2263747488
	.long	3192871328
	.long	0
	.long	1070408197
	.long	3894192264
	.long	1045693123
	.long	0
	.long	1070389063
	.long	994321593
	.long	1046347203
	.long	2147483648
	.long	1070369959
	.long	3540366700
	.long	1042296230
	.long	0
	.long	1070350886
	.long	966420752
	.long	3192400412
	.long	2147483648
	.long	1070331842
	.long	1954511160
	.long	3193467762
	.long	2147483648
	.long	1070312828
	.long	1875003040
	.long	1045485629
	.long	0
	.long	1070293845
	.long	4003372005
	.long	3193714109
	.long	2147483648
	.long	1070274890
	.long	2216083644
	.long	1045720399
	.long	0
	.long	1070255966
	.long	1240985743
	.long	1045879414
	.long	0
	.long	1070237071
	.long	1573064162
	.long	1046427916
	.long	0
	.long	1070218206
	.long	2500166582
	.long	3193848169
	.long	2147483648
	.long	1070199369
	.long	862131539
	.long	1045606065
	.long	0
	.long	1070180563
	.long	3733427622
	.long	3193545988
	.long	0
	.long	1070161785
	.long	124515358
	.long	1045504766
	.long	2147483648
	.long	1070143036
	.long	689228007
	.long	1044238436
	.long	0
	.long	1070124317
	.long	976284835
	.long	3189879978
	.long	2147483648
	.long	1070105626
	.long	2997446224
	.long	3193394244
	.long	2147483648
	.long	1070086964
	.long	594985163
	.long	3190453447
	.long	2147483648
	.long	1070068331
	.long	3634411091
	.long	3193012662
	.long	0
	.long	1070049727
	.long	841316482
	.long	3192551604
	.long	0
	.long	1070031151
	.long	518949849
	.long	3189505693
	.long	2147483648
	.long	1070012603
	.long	207633604
	.long	1043791305
	.long	2147483648
	.long	1069994084
	.long	925415631
	.long	3189658670
	.long	2147483648
	.long	1069975593
	.long	3348775015
	.long	1046231055
	.long	0
	.long	1069957131
	.long	4137593961
	.long	1045760644
	.long	2147483648
	.long	1069938696
	.long	3081207972
	.long	1046319652
	.long	2147483648
	.long	1069920290
	.long	2912811806
	.long	3193250863
	.long	0
	.long	1069901912
	.long	1704663230
	.long	3192651171
	.long	2147483648
	.long	1069883561
	.long	1726887473
	.long	3193427817
	.long	2147483648
	.long	1069865238
	.long	516302873
	.long	1042556919
	.long	2147483648
	.long	1069846943
	.long	3737277289
	.long	3192083505
	.long	0
	.long	1069828676
	.long	2829909067
	.long	3191628520
	.long	0
	.long	1069810436
	.long	3474800299
	.long	3187384991
	.long	2147483648
	.long	1069792223
	.long	2041291754
	.long	3186735048
	.long	2147483648
	.long	1069774038
	.long	3100739290
	.long	3192991951
	.long	2147483648
	.long	1069755880
	.long	2641686866
	.long	1042449846
	.long	0
	.long	1069737750
	.long	1353612457
	.long	3192928544
	.long	2147483648
	.long	1069719646
	.long	1823398190
	.long	3193125156
	.long	0
	.long	1069701570
	.long	2629108558
	.long	3192983089
	.long	2147483648
	.long	1069683520
	.long	314889080
	.long	3193178947
	.long	2147483648
	.long	1069665497
	.long	3426846470
	.long	1046055034
	.long	0
	.long	1069647502
	.long	2451521798
	.long	3193081447
	.long	2147483648
	.long	1069629532
	.long	963200030
	.long	1046315089
	.long	0
	.long	1069611590
	.long	3644976987
	.long	1046450297
	.long	2147483648
	.long	1069593674
	.long	1514045874
	.long	3193337489
	.long	0
	.long	1069575785
	.long	2640752615
	.long	3192734715
	.long	0
	.long	1069557922
	.long	177381730
	.long	3193107348
	.long	0
	.long	1069532650
	.long	546871269
	.long	1045601847
	.long	0
	.long	1069497029
	.long	2220408187
	.long	1045964849
	.long	0
	.long	1069461461
	.long	3101209784
	.long	3192417098
	.long	0
	.long	1069425944
	.long	3768825782
	.long	1046196178
	.long	0
	.long	1069390480
	.long	737308942
	.long	1043872555
	.long	0
	.long	1069355068
	.long	1944808119
	.long	3193362317
	.long	0
	.long	1069319707
	.long	852406261
	.long	3191004250
	.long	0
	.long	1069284398
	.long	3202370743
	.long	3192549796
	.long	0
	.long	1069249140
	.long	900633975
	.long	1043862575
	.long	0
	.long	1069213934
	.long	3417168564
	.long	3193213168
	.long	0
	.long	1069178778
	.long	2513309972
	.long	1046051953
	.long	0
	.long	1069143674
	.long	1836846968
	.long	1044036653
	.long	0
	.long	1069108621
	.long	675391362
	.long	3193334972
	.long	0
	.long	1069073618
	.long	1859398086
	.long	3191668729
	.long	0
	.long	1069038666
	.long	3835994043
	.long	3193252196
	.long	0
	.long	1069003764
	.long	563337246
	.long	3192060530
	.long	0
	.long	1068968912
	.long	3715154210
	.long	1045592716
	.long	0
	.long	1068934111
	.long	51415636
	.long	3192193939
	.long	0
	.long	1068899359
	.long	822049108
	.long	1045846080
	.long	0
	.long	1068864658
	.long	3739043340
	.long	3193184949
	.long	0
	.long	1068830006
	.long	2500828997
	.long	3193115638
	.long	0
	.long	1068795403
	.long	1479335089
	.long	1045458233
	.long	0
	.long	1068760850
	.long	1914098598
	.long	1045079833
	.long	0
	.long	1068726346
	.long	1470374909
	.long	1046125471
	.long	0
	.long	1068691892
	.long	2048101185
	.long	3192960024
	.long	0
	.long	1068657486
	.long	801101802
	.long	1042523454
	.long	0
	.long	1068623129
	.long	412171467
	.long	1044799425
	.long	0
	.long	1068588821
	.long	2124566049
	.long	1040459843
	.long	0
	.long	1068554561
	.long	2087558263
	.long	1046083102
	.long	0
	.long	1068520350
	.long	290389316
	.long	1045220023
	.long	0
	.long	1068473430
	.long	393737815
	.long	1045770085
	.long	0
	.long	1068405202
	.long	3273111658
	.long	3193594336
	.long	0
	.long	1068337068
	.long	3076935419
	.long	3191993934
	.long	0
	.long	1068269030
	.long	1564279721
	.long	1040713632
	.long	0
	.long	1068201088
	.long	1950103787
	.long	3191285473
	.long	0
	.long	1068133240
	.long	111301617
	.long	1046140470
	.long	0
	.long	1068065488
	.long	2740933659
	.long	1046091898
	.long	0
	.long	1067997832
	.long	1267131462
	.long	3192947024
	.long	0
	.long	1067930268
	.long	629787343
	.long	1045599114
	.long	0
	.long	1067862800
	.long	2943029746
	.long	3191100621
	.long	0
	.long	1067795426
	.long	2538631151
	.long	3193953989
	.long	0
	.long	1067728144
	.long	3881795033
	.long	3191377363
	.long	0
	.long	1067660956
	.long	2752747058
	.long	3186250103
	.long	0
	.long	1067593862
	.long	892170014
	.long	3193330390
	.long	0
	.long	1067526860
	.long	2000985783
	.long	3192968647
	.long	0
	.long	1067459950
	.long	1954077304
	.long	1044399908
	.long	0
	.long	1067335900
	.long	4120702847
	.long	3193150730
	.long	0
	.long	1067202448
	.long	353489980
	.long	1045676744
	.long	0
	.long	1067069184
	.long	2609643324
	.long	3192108001
	.long	0
	.long	1066936100
	.long	2904433317
	.long	1044836541
	.long	0
	.long	1066803200
	.long	319656790
	.long	1044863904
	.long	0
	.long	1066670484
	.long	2407987331
	.long	3192995083
	.long	0
	.long	1066537948
	.long	2437746120
	.long	3193127733
	.long	0
	.long	1066405592
	.long	762570215
	.long	3189946997
	.long	0
	.long	1066145040
	.long	3317159694
	.long	1046060125
	.long	0
	.long	1065881056
	.long	2317845886
	.long	3191679176
	.long	0
	.long	1065617424
	.long	3665195816
	.long	1045633853
	.long	0
	.long	1065354160
	.long	2008730355
	.long	3193898211
	.long	0
	.long	1064829264
	.long	3746236192
	.long	1046121471
	.long	0
	.long	1064303680
	.long	885296753
	.long	3191852441
	.long	0
	.long	1063253696
	.long	449976495
	.long	3192682663
	.long	0
	.long	0
	.long	0
	.long	2147483648
	.type	L_tbl,@object
	.size	L_tbl,8208
	.space 496, 0x00 	# pad
	.align 16
HIGHMASK_Y:
	.long	0
	.long	4294967288
	.long	0
	.long	4294967295
	.type	HIGHMASK_Y,@object
	.size	HIGHMASK_Y,16
	.align 16
T_exp:
	.long	0
	.long	1072693248
	.long	0
	.long	997195776
	.long	4200250559
	.long	1072696090
	.long	2808127345
	.long	3162830514
	.long	2851812149
	.long	1072698941
	.long	2595802551
	.long	1016815913
	.long	339411585
	.long	1072701800
	.long	264588982
	.long	3162685233
	.long	1048019041
	.long	1072704666
	.long	1398474845
	.long	3161559171
	.long	772914124
	.long	1072707540
	.long	4004372762
	.long	1013278737
	.long	3899555717
	.long	1072710421
	.long	427280750
	.long	3163595548
	.long	1928746161
	.long	1072713311
	.long	983617676
	.long	1015333753
	.long	3541402996
	.long	1072716208
	.long	2759177317
	.long	1015903202
	.long	238821257
	.long	1072719114
	.long	1469694871
	.long	3163933563
	.long	702412510
	.long	1072722027
	.long	3803266087
	.long	3163328991
	.long	728934454
	.long	1072724948
	.long	1413842688
	.long	1015227188
	.long	410360776
	.long	1072727877
	.long	1269990655
	.long	1013024446
	.long	4133881824
	.long	1072730813
	.long	2148155345
	.long	3163979875
	.long	3402036099
	.long	1072733758
	.long	405889334
	.long	1016154232
	.long	2602514713
	.long	1072736711
	.long	2268929336
	.long	1015402860
	.long	1828292879
	.long	1072739672
	.long	1255956747
	.long	1016636974
	.long	1172597893
	.long	1072742641
	.long	114433263
	.long	1016396169
	.long	728909815
	.long	1072745618
	.long	383930225
	.long	1016078044
	.long	590962156
	.long	1072748603
	.long	3829346666
	.long	3164324173
	.long	852742562
	.long	1072751596
	.long	667253586
	.long	1010842135
	.long	1608493509
	.long	1072754597
	.long	3159622171
	.long	3163856313
	.long	2952712987
	.long	1072757606
	.long	3293494651
	.long	3161168877
	.long	685187902
	.long	1072760624
	.long	378731989
	.long	1015891691
	.long	3490863953
	.long	1072763649
	.long	960797498
	.long	3163997456
	.long	2875075254
	.long	1072766683
	.long	4144233330
	.long	3164382292
	.long	3228316108
	.long	1072769725
	.long	3010241991
	.long	3159471380
	.long	351405227
	.long	1072772776
	.long	3125337328
	.long	3160871055
	.long	2930322912
	.long	1072775834
	.long	2599499422
	.long	3163762623
	.long	2471440686
	.long	1072778901
	.long	968836267
	.long	3163263464
	.long	3366293073
	.long	1072781976
	.long	3119426314
	.long	1015169130
	.long	1416741826
	.long	1072785060
	.long	2196380210
	.long	1012462139
	.long	1014845819
	.long	1072788152
	.long	3117910646
	.long	3162607681
	.long	2257959872
	.long	1072791252
	.long	3802946148
	.long	1014013503
	.long	948735466
	.long	1072794361
	.long	3516338028
	.long	3163623459
	.long	1480023343
	.long	1072797478
	.long	2247196168
	.long	1016376029
	.long	3949972341
	.long	1072800603
	.long	2068408548
	.long	1015962444
	.long	4162030108
	.long	1072803737
	.long	2763428480
	.long	1016577925
	.long	2214878420
	.long	1072806880
	.long	892270087
	.long	3164164998
	.long	2502433899
	.long	1072810031
	.long	2148595913
	.long	1016072567
	.long	828946858
	.long	1072813191
	.long	10642492
	.long	1016988014
	.long	1588871207
	.long	1072816359
	.long	143439582
	.long	3164011992
	.long	586995997
	.long	1072819536
	.long	41662348
	.long	3163676568
	.long	2218315341
	.long	1072822721
	.long	2694295388
	.long	3164337444
	.long	2288159958
	.long	1072825915
	.long	2169144469
	.long	1015924597
	.long	897099801
	.long	1072829118
	.long	754756297
	.long	1016289581
	.long	2440944790
	.long	1072832329
	.long	2492769774
	.long	1015196030
	.long	2725843665
	.long	1072835549
	.long	1433917087
	.long	1015887099
	.long	1853186616
	.long	1072838778
	.long	3066496371
	.long	1016705150
	.long	4219606026
	.long	1072842015
	.long	2434574742
	.long	1015730124
	.long	1337108031
	.long	1072845262
	.long	3203724452
	.long	1015726421
	.long	1897844341
	.long	1072848517
	.long	1254300460
	.long	1016324514
	.long	1709341917
	.long	1072851781
	.long	2571168217
	.long	1015201075
	.long	874372905
	.long	1072855054
	.long	100263788
	.long	1016989308
	.long	3790955393
	.long	1072858335
	.long	2352942462
	.long	3164228666
	.long	1972484976
	.long	1072861626
	.long	675290301
	.long	3162688626
	.long	4112506593
	.long	1072864925
	.long	2947355221
	.long	1015419624
	.long	1724976915
	.long	1072868234
	.long	420909223
	.long	3164165955
	.long	3504003472
	.long	1072871551
	.long	3594001060
	.long	3158379228
	.long	964107055
	.long	1072874878
	.long	2800439588
	.long	3163881797
	.long	2799960843
	.long	1072878213
	.long	1423655381
	.long	1016070727
	.long	526652809
	.long	1072881558
	.long	4223459736
	.long	1016927951
	.long	2839424854
	.long	1072884911
	.long	1171596163
	.long	1014090255
	.long	1253935211
	.long	1072888274
	.long	1395382931
	.long	3160751189
	.long	171030293
	.long	1072891646
	.long	3526460132
	.long	1015477354
	.long	3991843581
	.long	1072895026
	.long	4092853457
	.long	1015634339
	.long	4232894513
	.long	1072898416
	.long	2383938684
	.long	1015717095
	.long	1000925746
	.long	1072901816
	.long	1018491672
	.long	3164358120
	.long	2992903935
	.long	1072905224
	.long	2218154406
	.long	1016276769
	.long	1726216749
	.long	1072908642
	.long	2466808228
	.long	3162724981
	.long	1603444721
	.long	1072912069
	.long	1548633640
	.long	3163249902
	.long	2732492859
	.long	1072915505
	.long	2691479646
	.long	3163304260
	.long	926591435
	.long	1072918951
	.long	3208833762
	.long	3163962090
	.long	589198666
	.long	1072922406
	.long	2664346172
	.long	3164206538
	.long	1829099622
	.long	1072925870
	.long	1016661181
	.long	3164509581
	.long	460407023
	.long	1072929344
	.long	4237175092
	.long	3164187045
	.long	887463927
	.long	1072932827
	.long	3596744163
	.long	3161842742
	.long	3219942644
	.long	1072936319
	.long	3798990616
	.long	1016417382
	.long	3272845541
	.long	1072939821
	.long	928852419
	.long	3164536824
	.long	1156440435
	.long	1072943333
	.long	2351451249
	.long	1015015632
	.long	1276261410
	.long	1072946854
	.long	300981948
	.long	1015732745
	.long	3743175029
	.long	1072950384
	.long	2072812490
	.long	3163223651
	.long	78413852
	.long	1072953925
	.long	4183226867
	.long	3164065827
	.long	3278348324
	.long	1072957474
	.long	3069497416
	.long	1015799288
	.long	569847338
	.long	1072961034
	.long	472945272
	.long	3160339305
	.long	654919306
	.long	1072964603
	.long	3232961757
	.long	3164096045
	.long	3645941911
	.long	1072968181
	.long	3814685081
	.long	3162621917
	.long	1065662932
	.long	1072971770
	.long	2533670915
	.long	1015578814
	.long	1617004845
	.long	1072975368
	.long	82804944
	.long	1011391354
	.long	1118294578
	.long	1072978976
	.long	2197495694
	.long	3160957977
	.long	3978100823
	.long	1072982593
	.long	3513027190
	.long	1016894539
	.long	1720398391
	.long	1072986221
	.long	3980678963
	.long	3164348656
	.long	3049340112
	.long	1072989858
	.long	3062915824
	.long	1014219171
	.long	3784486610
	.long	1072993505
	.long	1581883040
	.long	3162747529
	.long	4040676318
	.long	1072997162
	.long	4090609238
	.long	1016712034
	.long	3933059031
	.long	1073000829
	.long	2133366768
	.long	3162580408
	.long	3577096743
	.long	1073004506
	.long	2951496418
	.long	1014842263
	.long	3088564500
	.long	1073008193
	.long	1762311517
	.long	1016094249
	.long	2583551245
	.long	1073011890
	.long	3161094195
	.long	1016655067
	.long	2178460671
	.long	1073015597
	.long	777878098
	.long	3163891069
	.long	1990012071
	.long	1073019314
	.long	3529070563
	.long	3163861769
	.long	2135241198
	.long	1073023041
	.long	1236747871
	.long	1014637723
	.long	2731501122
	.long	1073026778
	.long	1774031855
	.long	3163518597
	.long	3896463087
	.long	1073030525
	.long	1139797873
	.long	3162282381
	.long	1453150082
	.long	1073034283
	.long	498154669
	.long	3162536638
	.long	4109806887
	.long	1073038050
	.long	422403966
	.long	1015517805
	.long	3395129871
	.long	1073041828
	.long	4025345435
	.long	3163383964
	.long	3723038930
	.long	1073045616
	.long	378465264
	.long	3163618158
	.long	917841882
	.long	1073049415
	.long	18715565
	.long	1016707884
	.long	3689071823
	.long	1073053223
	.long	2321004996
	.long	3163601292
	.long	3566716925
	.long	1073057042
	.long	1536826856
	.long	1015191009
	.long	671025100
	.long	1073060872
	.long	3832014351
	.long	3164070606
	.long	3712504873
	.long	1073064711
	.long	88491949
	.long	1016476236
	.long	4222122499
	.long	1073068561
	.long	1277378074
	.long	3164305313
	.long	2321106615
	.long	1073072422
	.long	2171176610
	.long	1010584347
	.long	2425981843
	.long	1073076293
	.long	2830390851
	.long	3164395175
	.long	363667784
	.long	1073080175
	.long	813753950
	.long	1016833785
	.long	551349105
	.long	1073084067
	.long	3821916050
	.long	3163155165
	.long	3111574537
	.long	1073087969
	.long	2606161479
	.long	3163808322
	.long	3872257780
	.long	1073091882
	.long	1253592103
	.long	1017006910
	.long	2956612997
	.long	1073095806
	.long	2118169751
	.long	3163784129
	.long	488188413
	.long	1073099741
	.long	3199821029
	.long	1016612624
	.long	885834528
	.long	1073103686
	.long	1973258547
	.long	3163310140
	.long	4273770423
	.long	1073107641
	.long	3383180809
	.long	3164267477
	.long	2186617381
	.long	1073111608
	.long	2270764084
	.long	3164321289
	.long	3339203574
	.long	1073115585
	.long	1483497780
	.long	3163457330
	.long	3561793907
	.long	1073119573
	.long	1157054053
	.long	1012938926
	.long	2979960120
	.long	1073123572
	.long	2599109725
	.long	1015547069
	.long	1719614413
	.long	1073127582
	.long	330458198
	.long	3164331316
	.long	4201977662
	.long	1073131602
	.long	748330254
	.long	1014642933
	.long	1963711167
	.long	1073135634
	.long	1744767757
	.long	3161622870
	.long	3721688645
	.long	1073139676
	.long	3069276937
	.long	1016887977
	.long	1013258799
	.long	1073143730
	.long	1748797611
	.long	3161177658
	.long	2555984613
	.long	1073147794
	.long	2652555442
	.long	3163601268
	.long	4182873220
	.long	1073151869
	.long	629542646
	.long	3163044879
	.long	1727278727
	.long	1073155956
	.long	3562710623
	.long	1012520516
	.long	3907805044
	.long	1073160053
	.long	2257091225
	.long	3162598983
	.long	2263535754
	.long	1073164162
	.long	752233586
	.long	3163687584
	.long	1218806132
	.long	1073168282
	.long	1818613052
	.long	3163597017
	.long	903334909
	.long	1073172413
	.long	1636462108
	.long	1016088573
	.long	1447192521
	.long	1073176555
	.long	1462857171
	.long	3163563097
	.long	2980802057
	.long	1073180708
	.long	378619896
	.long	1016821879
	.long	1339972927
	.long	1073184873
	.long	167908909
	.long	1016620728
	.long	950803702
	.long	1073189049
	.long	1655364926
	.long	1016285608
	.long	1944781191
	.long	1073193236
	.long	3993278767
	.long	3162772855
	.long	158781403
	.long	1073197435
	.long	2221464712
	.long	3164335029
	.long	19972402
	.long	1073201645
	.long	3507899862
	.long	1017057868
	.long	1660913392
	.long	1073205866
	.long	4218599604
	.long	1016184283
	.long	919555682
	.long	1073210099
	.long	3121969534
	.long	1013996802
	.long	2224145553
	.long	1073214343
	.long	3482522030
	.long	3162537745
	.long	1413356050
	.long	1073218599
	.long	1651349291
	.long	3163716742
	.long	2916157145
	.long	1073222866
	.long	219487565
	.long	1016357943
	.long	2571947539
	.long	1073227145
	.long	3558159064
	.long	3164425245
	.long	515457527
	.long	1073231436
	.long	836709333
	.long	1016699802
	.long	1176749997
	.long	1073235738
	.long	2738998779
	.long	3163084420
	.long	396319521
	.long	1073240052
	.long	4172420816
	.long	3160123208
	.long	2604962541
	.long	1073244377
	.long	2614425274
	.long	3164587768
	.long	3643909174
	.long	1073248714
	.long	3537586109
	.long	1015403223
	.long	3649726105
	.long	1073253063
	.long	4085036346
	.long	1016698050
	.long	2759350287
	.long	1073257424
	.long	1148526634
	.long	1016943509
	.long	1110089947
	.long	1073261797
	.long	1451641639
	.long	1016523249
	.long	3134592888
	.long	1073266181
	.long	4232266862
	.long	1017039710
	.long	380978316
	.long	1073270578
	.long	854188970
	.long	3161511262
	.long	1577608921
	.long	1073274986
	.long	1875489510
	.long	3164016970
	.long	2568320822
	.long	1073279406
	.long	2732824428
	.long	1015401491
	.long	3492293770
	.long	1073283838
	.long	2248032210
	.long	1016435402
	.long	194117574
	.long	1073288283
	.long	777528612
	.long	3164460665
	.long	1403662306
	.long	1073292739
	.long	2788809599
	.long	3162719583
	.long	2966275557
	.long	1073297207
	.long	2176155324
	.long	3160891335
	.long	727685349
	.long	1073301688
	.long	2038246809
	.long	3163407318
	.long	3418903055
	.long	1073306180
	.long	2527457337
	.long	3161869180
	.long	2591453363
	.long	1073310685
	.long	2132396182
	.long	3160122774
	.long	2682146384
	.long	1073315202
	.long	2082178513
	.long	3164411995
	.long	3833209506
	.long	1073319731
	.long	2722920684
	.long	1014803418
	.long	1892288442
	.long	1073324273
	.long	2446255666
	.long	3163648957
	.long	1297350157
	.long	1073328827
	.long	1308022040
	.long	3164461134
	.long	2191782032
	.long	1073333393
	.long	2960257726
	.long	1014791238
	.long	424392917
	.long	1073337972
	.long	2749202995
	.long	3163887294
	.long	434316067
	.long	1073342563
	.long	2028358766
	.long	1014506698
	.long	2366108318
	.long	1073347166
	.long	2867985102
	.long	3162810830
	.long	2069751141
	.long	1073351782
	.long	1562170675
	.long	3163773257
	.long	3985553595
	.long	1073356410
	.long	4002146062
	.long	1016882712
	.long	3964284211
	.long	1073361051
	.long	2111583915
	.long	1016475740
	.long	2152073944
	.long	1073365705
	.long	1486860576
	.long	3164252032
	.long	2990417245
	.long	1073370371
	.long	3683467745
	.long	3164417902
	.long	2331271250
	.long	1073375050
	.long	812057446
	.long	1013256022
	.long	321958744
	.long	1073379742
	.long	3401933767
	.long	1016843134
	.long	1405169241
	.long	1073384446
	.long	2998539689
	.long	3163879527
	.long	1434058175
	.long	1073389163
	.long	251133233
	.long	1016134345
	.long	557149882
	.long	1073393893
	.long	3672720709
	.long	1015585841
	.long	3218338682
	.long	1073398635
	.long	3404164304
	.long	3163525684
	.long	977020788
	.long	1073403391
	.long	3065100517
	.long	1016590139
	.long	2572866477
	.long	1073408159
	.long	878562433
	.long	1016570317
	.long	3861050111
	.long	1073412940
	.long	254893773
	.long	3163861756
	.long	697153126
	.long	1073417735
	.long	1283515429
	.long	3164331765
	.long	1822067026
	.long	1073422542
	.long	1241994956
	.long	1016388866
	.long	3092190715
	.long	1073427362
	.long	814012168
	.long	3160571998
	.long	364333489
	.long	1073432196
	.long	3923737744
	.long	3162469949
	.long	2380618042
	.long	1073437042
	.long	3149557219
	.long	3164369375
	.long	703710506
	.long	1073441902
	.long	1384660846
	.long	1016244467
	.long	4076559943
	.long	1073446774
	.long	2119478331
	.long	3161806927
	.long	4062661092
	.long	1073451660
	.long	1422616006
	.long	3164303894
	.long	815859274
	.long	1073456560
	.long	240396590
	.long	3164536019
	.long	3080351519
	.long	1073461472
	.long	3379126789
	.long	3158266577
	.long	2420883922
	.long	1073466398
	.long	2049810052
	.long	1015168464
	.long	3287523847
	.long	1073471337
	.long	1625971539
	.long	3158058531
	.long	1540824585
	.long	1073476290
	.long	1064017011
	.long	3164536266
	.long	1631695677
	.long	1073481256
	.long	2717633076
	.long	3163392602
	.long	3716502172
	.long	1073486235
	.long	2303740125
	.long	1015091301
	.long	3657065772
	.long	1073491228
	.long	399025623
	.long	3164005654
	.long	1610600570
	.long	1073496235
	.long	3766732298
	.long	1016808759
	.long	2029714210
	.long	1073501255
	.long	613660079
	.long	1016147719
	.long	777507147
	.long	1073506289
	.long	4282924205
	.long	1016236109
	.long	2307442995
	.long	1073511336
	.long	3190117721
	.long	3163453115
	.long	2483480501
	.long	1073516397
	.long	1216371780
	.long	1014082748
	.long	1464976603
	.long	1073521472
	.long	3507292405
	.long	3163026110
	.long	3706687593
	.long	1073526560
	.long	3521726939
	.long	1014301643
	.long	778901109
	.long	1073531663
	.long	2248183954
	.long	3162317327
	.long	1432208378
	.long	1073536779
	.long	1401068914
	.long	3163412539
	.long	1532734324
	.long	1073541909
	.long	3094216535
	.long	3164211433
	.long	1242007932
	.long	1073547053
	.long	1132034716
	.long	3164388407
	.long	721996136
	.long	1073552211
	.long	563754734
	.long	1016419894
	.long	135105010
	.long	1073557383
	.long	1906148728
	.long	3164424315
	.long	3939148246
	.long	1073562568
	.long	3210352148
	.long	1016322899
	.long	3707479175
	.long	1073567768
	.long	3613079303
	.long	1015213314
	.long	3898795731
	.long	1073572982
	.long	1249994144
	.long	1012918394
	.long	382305176
	.long	1073578211
	.long	2347622376
	.long	3163627201
	.long	1912561781
	.long	1073583453
	.long	3147495102
	.long	1016726829
	.long	64696965
	.long	1073588710
	.long	1768797490
	.long	1016865536
	.long	3594158869
	.long	1073593980
	.long	2456521700
	.long	3164305137
	.long	4076975200
	.long	1073599265
	.long	2029000899
	.long	1016257111
	.long	1679558232
	.long	1073604565
	.long	2390342287
	.long	3164382546
	.long	863738719
	.long	1073609879
	.long	1326992220
	.long	3163661773
	.long	1796832535
	.long	1073615207
	.long	3176955716
	.long	3161634089
	.long	351641897
	.long	1073620550
	.long	2172261526
	.long	3164059175
	.long	991358482
	.long	1073625907
	.long	838715019
	.long	3164206244
	.long	3884662774
	.long	1073631278
	.long	2158611599
	.long	1015258761
	.long	610758006
	.long	1073636665
	.long	1965209397
	.long	3162914808
	.long	4224142467
	.long	1073642065
	.long	3389820386
	.long	1016255778
	.long	2009970496
	.long	1073647481
	.long	2159039665
	.long	3163621524
	.long	2728693978
	.long	1073652911
	.long	396109971
	.long	3164511267
	.long	2256325230
	.long	1073658356
	.long	580117746
	.long	1016365871
	.long	764307441
	.long	1073663816
	.long	3021057420
	.long	3164378099
	.long	2719515920
	.long	1073669290
	.long	2760332941
	.long	1016186509
	.long	3999357479
	.long	1073674779
	.long	2258941616
	.long	1016973300
	.long	481706282
	.long	1073680284
	.long	1696079173
	.long	3163759104
	.long	929806999
	.long	1073685803
	.long	3205336643
	.long	1016308133
	.long	1222472308
	.long	1073691337
	.long	1054357470
	.long	3162069594
	.long	1533953344
	.long	1073696886
	.long	769171851
	.long	1016714209
	.long	2038973688
	.long	1073702450
	.long	892941374
	.long	1017095035
	.long	2912730644
	.long	1073708029
	.long	3490067722
	.long	3164453650
	.long	35929225
	.long	1073713624
	.long	2809788041
	.long	3160485544
	.long	2174652632
	.long	1073719233
	.long	4087714590
	.long	1015498835
	.long	915592468
	.long	1073724858
	.long	352947894
	.long	3162072947
	.long	730821105
	.long	1073730498
	.long	2523232743
	.long	1013115764
	.long	1797923801
	.long	1073736153
	.long	1950547427
	.long	1014277635
	.type	T_exp,@object
	.size	T_exp,4096
	.space 512, 0x00 	# pad
	.align 16
e_coeff:
	.long	3884607281
	.long	1062590591
	.long	3607404736
	.long	1068264200
	.long	1874480759
	.long	1065595563
	.long	4286760335
	.long	1070514109
	.long	4277811695
	.long	1072049730
	.long	0
	.long	0
	.type	e_coeff,@object
	.size	e_coeff,48
	.align 16
coeff_h:
	.long	0
	.long	3218479616
	.long	0
	.long	3210587105
	.type	coeff_h,@object
	.size	coeff_h,16
	.align 16
HIGHMASK_LOG_X:
	.long	4160749568
	.long	4294967295
	.long	0
	.long	4294965248
	.type	HIGHMASK_LOG_X,@object
	.size	HIGHMASK_LOG_X,16
	.align 8
HALFMASK:
	.long	4160749568
	.long	4294967295
	.long	4160749568
	.long	4294967295
	.type	HALFMASK,@object
	.size	HALFMASK,16
	.align 8
log2:
	.long	4277811695
	.long	1072049730
	.long	4277811695
	.long	3219533378
	.type	log2,@object
	.size	log2,16
	.data
	.section .note.GNU-stack, ""
// -- Begin DWARF2 SEGMENT .eh_frame
	.section .eh_frame,"a",@progbits
.eh_frame_seg:
	.align 1
	.4byte 0x00000014
	.8byte 0x00527a0100000000
	.8byte 0x08070c1b01107801
	.4byte 0x00000190
	.4byte 0x0000001c
	.4byte 0x0000001c
	.4byte ..___tag_value_pow.1-.
	.4byte ..___tag_value_pow.5-..___tag_value_pow.1
	.2byte 0x0400
	.4byte ..___tag_value_pow.3-..___tag_value_pow.1
	.2byte 0x300e
	.byte 0x04
	.4byte ..___tag_value_pow.4-..___tag_value_pow.3
	.2byte 0x080e
	.byte 0x00
# End
