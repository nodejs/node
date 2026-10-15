#! /usr/bin/env perl
#
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2026 Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

###############################################################################
# ML-DSA AVX2 Vectorized NTT/INTT Assembly Routines
#
# Description:
#   This file provides optimized x86_64 assembly implementations of the Number
#   Theoretic Transform (NTT) and inverse NTT (INTT) and their modular Montgomery
#   reduction building blocks for the ML-DSA signature scheme.
#
#   The routines are vectorized using AVX2 instructions, performing
#   modular arithmetic and butterfly operations on multiple
#   coefficients in parallel. The mathematical structure and transformations strictly
#   follow those implemented in the corresponding C code (ml_dsa_ntt.c),
#   ensuring that this file provides a drop-in, performant backend using the
#   same algorithms and data layout.
#   There is dedicated zeta table used for INTT only. It is essentially
#   reformatted original table from ml_dsa_ntt.c file to reduce INTT compute cycles.
#
#   This module supports both the forward (NTT) and inverse (INTT)
#   polynomial transforms, as well as element-wise NTT-domain polynomial
#   multiplication, compatible with the ML-DSA cryptographic protocol.
#
#   Step, offset and zeta index details provided for NTT and INTT level operations
#   correspond directly to the original C implementations from ml_dsa_ntt.c file.
#
# Notes:
#   - Uses AVX2 instructions and YMM registers that accommodate 8 32-bit coefficients
#   - Must be kept functionally synchronized with the math and
#     interface of ml_dsa_ntt.c
#   - Data structures, twiddle factors ("zetas"), and constants must match
#     those in the C implementation
###############################################################################

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64 = 0;
$win64 = 1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$avx2 = 0;

$0 =~ m/(.*[\/\\])[^\/\\]+$/;
$dir = $1;
($xlate = "${dir}x86_64-xlate.pl" and -f $xlate)
  or ($xlate = "${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate)
  or die "can't locate x86_64-xlate.pl";

# Check for AVX2 support in assembler
if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1` =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
  $avx2 = ($1 >= 2.22);
}

if (!$avx2
  && $win64
  && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/)
  && `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/)
{
  $avx2 = ($1 >= 2.10);
}

if (!$avx2 && `$ENV{CC} -v 2>&1` =~ /((?:clang|LLVM) version|.*based on LLVM) ([0-9]+\.[0-9]+)/) {
    $avx2 = ($2>=3.3); # minimal tested version for AVX2
}

open OUT, "| \"$^X\" \"$xlate\" $flavour \"$output\""
  or die "can't call $xlate: $!";
*STDOUT = *OUT;

# ML-DSA constants
my $ML_DSA_Q = 8380417;             # Q: 2^23 - 2^13 + 1
my $ML_DSA_Q_NEG_INV = 4236238847;  # -Q^{-1} mod 2^32

#  The multiplicative inverse of 256 mod Q, in Montgomery form is
#  ((256^{-1} mod Q) * ((2^32 * 2^32) mod Q)) mod Q = (8347681 * 2365951) mod 8380417
my $inverse_degree_montgomery = 41978;

if ($avx2>0) {{{

# avx2 feature bit
my $avx2_mask = (1<<5);

$code .= <<___;
.text

.extern OPENSSL_ia32cap_P

.globl  ml_dsa_ntt_avx2_capable
.type   ml_dsa_ntt_avx2_capable,\@abi-omnipotent
.align 32
ml_dsa_ntt_avx2_capable:
    mov     OPENSSL_ia32cap_P+8(%rip), %rcx
    xor     %eax, %eax
    and     \$$avx2_mask, %ecx
    cmovnz  %ecx, %eax
    ret
.size   ml_dsa_ntt_avx2_capable, .-ml_dsa_ntt_avx2_capable
___

###############################################################################
# multiply_mod_Q
#
# Description:
#   The inputs (A and B) are in YMM registers, where each packs 8 32-bit integers.
#   The result, (A * B mod Q), is also in a YMM register.
#   This routine is used as the core modular multiplication step in
#   NTT butterfly operations.
#
# Parameters:
#   inA      - Input YMM containing 8 32-bit unsigned values (A)
#   inB      - Input YMM containing 8 32-bit unsigned values (B)
#   out      - Output YMM for (A * B mod Q)
#   tmp0-tmp2 - Temporary registers for intermediate values
#   q_neg_inv- YMM containing -Q^{-1} mod 2^32 (for Montgomery reduction)
#   q        - YMM containing modulus Q
#   bcast32  – if 1, `inB` is assumed to have each qword formed by
#              repeating its low dword (DW|DW). Multiplication uses only
#              the low dword, avoiding the need for `vmovshdup` when
#              zetas are uniform across all lanes of `inA`.
#
# Output:
#   out      - Resulting 8 packed 32-bit integers, each (A * B) mod Q
#
# Side effects:
#   Clobbers tmp0, tmp1, tmp2
#
# Notes:
#   inA or inB can also be used as out
###############################################################################
sub multiply_mod_Q {
    my ($inA, $inB, $out,
        $tmp0, $tmp1, $tmp2,
        $q_neg_inv, $q, $bcast32) = @_;

    if (!defined($bcast32)) {
        $bcast32 = 0;
    }

    $code .= <<___;
    # Multiply A x B
    vpmuludq $inA, $inB, $tmp0  # multiply even indexes
    vmovshdup $inA, $tmp1
___
    if ($bcast32 == 0) {
        $code .= <<___;
    vmovshdup $inB, $tmp2
    vpmuludq $tmp1, $tmp2, $tmp1  # multiply odd indexes
___
    } else {
        $code .= <<___;
    vpmuludq $tmp1, $inB, $tmp1   # multiply odd indexes of inA
___
    }
        $code .= <<___;
    # Montgomery reduction: t1 = (A x B)[31..0] x Qinv
    vpmuludq $q_neg_inv, $tmp0, $out
    vpmuludq $q_neg_inv, $tmp1, $tmp2

    # t2 = t1[31..0] x Q
    vpmuludq $out, $q, $out
    vpmuludq $tmp2, $q, $tmp2

    # t3 = (A x B) + t2
    vpaddq  $out, $tmp0, $out
    vpaddq  $tmp2, $tmp1, $tmp1

    # out = t3 >> 32
    vmovshdup $out, $out
    vpblendd \$0xAA, $tmp1, $out, $out

    # out can be between Q and 2Q, do final reduction
    vpcmpgtd $out, $q, $tmp0    # $tmp0 = $q > $out ? 0xffffffff : 0
    vpandn $q, $tmp0, $tmp0     # $tmp0 = ~$tmp0 & $q
    vpsubd $tmp0, $out, $out    # $out -= $tmp0
___
}

###############################################################################
###############################################################################
###
### NTT (Number Theoretic Transform)
###
###############################################################################
###############################################################################

###############################################################################
# ntt_butterfly
#
# Description:
#   Performs the butterfly operation for a single NTT stage on two input YMM's,
#   applying a twiddle factor ("zetas"). This is the core step in NTT layers:
#     - Computes t_odd = w_odd * zetas mod Q (Montgomery form)
#     - n_even = w_even + t_odd mod Q
#     - n_odd  = (w_even + Q) - t_odd mod Q
#   Uses AVX2 instructions to process 8 coefficients at a time.
#
# Parameters:
#   w_even  - YMM containing the "even" values
#   w_odd   - YMM containing the "odd" values
#   zetas   - YMM containing the twiddle factor(s)
#   n_even  - Output YMM for the updated even coefficients
#   n_odd   - Output YMM for the updated odd coefficients
#   tmp0, tmp1, tmp2, tmp3 - Temporary registers for intermediate values
#   q_neg_inv - YMM with -Q^{-1} mod 2^32
#   q       - YMM with modulus Q
#   level   - current NTT processing level
#
# Output:
#   n_even  - Updated even coefficients after butterfly and reduction
#   n_odd   - Updated odd coefficients after butterfly and reduction
#
# Side effects:
#   Clobbers tmp0, tmp1, tmp2, tmp3
###############################################################################
sub ntt_butterfly {
    my ($w_even, $w_odd,
        $zetas,
        $n_even, $n_odd,
        $tmp0, $tmp1, $tmp2, $tmp3,
        $q_neg_inv, $q, $level) = @_;

    if ($level >= 7) {
        # level 7: each zeta (B) dword is different
        &multiply_mod_Q($w_odd, # A
                        $zetas, # B
                        $tmp0,  # out (AxB)
                        $tmp1, $tmp2, $tmp3, # tmp
                        $q_neg_inv, $q, 0);  # qinv, q, no-bcast32
    } else {
        # levels 0 to 6: same zeta (B) dwords within each qword
        &multiply_mod_Q($w_odd, # A
                        $zetas, # B
                        $tmp0,  # out (AxB)
                        $tmp1, $tmp2, $tmp3, # tmp
                        $q_neg_inv, $q, 1);  # qinv, q, bcast32
    }
    $code .= <<___;

    # t_odd = $tmp0

    # compute new w_odd (n_odd): (w_even + Q) - t_odd
    vpaddd  $q, $w_even, $tmp1
    vpsubd  $tmp0, $tmp1, $n_odd

    # compute new w_even (n_even): w_even + t_odd
    vpaddd  $w_even, $tmp0, $n_even

    # reduce n_even & n_odd
    # - results can be between Q and 2Q
    vpcmpgtd $n_even, $q, $tmp0 # $tmp0 = $q > $n_even ? 0xffffffff : 0
    vpcmpgtd $n_odd, $q, $tmp1  # $tmp1 = $q > $n_odd ? 0xffffffff : 0
    vpandn $q, $tmp0, $tmp0     # $tmp0 = ~$tmp0 & $q
    vpandn $q, $tmp1, $tmp1     # $tmp1 = ~$tmp1 & $q
    vpsubd $tmp0, $n_even, $n_even  # $n_even -= $tmp0
    vpsubd $tmp1, $n_odd, $n_odd    # $n_odd -= $tmp1
___
}

###############################################################################
# ntt_levels0to2
#
# Description: Performs the first three layers (levels 0, 1, and 2) of the NTT.
#   It works on 8 YMM registers, 8 32-bit coefficients each. Coefficients loaded into
#   YMM's are separated by 32 coefficients (32 x 4 bytes = 128 bytes). All 8 YMM registers
#   undergo consecutive butterfly operations with the appropriate "zetas" (twiddle factors) for
#   each level. This function must be called 4 times with different offsets to process all 256
#   coefficients.
#
# Layer/level details:
#   - Level 0:  offset = 128, step = 1, uses zeta index 1
#   - Level 1:  offset =  64, step = 2, uses zeta indexes 2, 3
#   - Level 2:  offset =  32, step = 4, uses zeta indexes 4, 5, 6, 7
#
# Prerequisites:
#   %rdi    - pointer to the coefficients
#   %r11    - pointer to the zetas (twiddle factors) table
#   %ymm14  - register with q_neg_inv (for Montgomery reduction)
#   %ymm15  - register with modulus Q
#
# Arguments:
#   $off    - offset to the start of a group of 8 coefficients (in bytes, relative to %rdi)
#             valid values: 0*4, 8*4, 16*4 or 24*4
#
# Output:
#   In-place NTT updated coefficients in memory.
#
# Notes:
#   - Must be invoked 4 times for complete polynomial: offsets 0, 8*4, 16*4, 24*4
###############################################################################

sub ntt_levels0to2 {
    my ($off) = @_;
    $code .= <<___;
    vmovdqu $off+0*4(%rdi), %ymm0
    vmovdqu $off+32*4(%rdi), %ymm1
    vmovdqu $off+64*4(%rdi), %ymm2
    vmovdqu $off+96*4(%rdi), %ymm3
    vmovdqu $off+128*4(%rdi), %ymm4
    vmovdqu $off+160*4(%rdi), %ymm5
    vmovdqu $off+192*4(%rdi), %ymm6
    vmovdqu $off+224*4(%rdi), %ymm7

    # ==============================================================
    # level 0: offset = 128, step = 1
    # zeta indexes = 1

    vpbroadcastd 1*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm0", "%ymm4", "%ymm13", "%ymm0", "%ymm4",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 0);                          # qinv, q, level
    &ntt_butterfly("%ymm1", "%ymm5", "%ymm13", "%ymm1", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 0);                          # qinv, q, level
    &ntt_butterfly("%ymm2", "%ymm6", "%ymm13", "%ymm2", "%ymm6",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 0);                          # qinv, q, level
    &ntt_butterfly("%ymm3", "%ymm7", "%ymm13", "%ymm3", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 0);                          # qinv, q, level
    $code .= <<___;

    # ==============================================================
    # level 1: offset = 64, step = 2
    # zeta indexes = 2, 3

    vpbroadcastd 2*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm0", "%ymm2", "%ymm13", "%ymm0", "%ymm2",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 1);                          # qinv, q, level
    &ntt_butterfly("%ymm1", "%ymm3", "%ymm13", "%ymm1", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 1);                          # qinv, q, level
    $code .= <<___;
    vpbroadcastd 3*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm4", "%ymm6", "%ymm13", "%ymm4", "%ymm6",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 1);                          # qinv, q, level
    &ntt_butterfly("%ymm5", "%ymm7", "%ymm13", "%ymm5", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 1);                          # qinv, q, level
$code .= <<___;

    # ==============================================================
    # level 2: offset = 32, step = 4
    # zeta indexes = 4, 5, 6, 7

    vpbroadcastd 4*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm0", "%ymm1", "%ymm13", "%ymm0", "%ymm1",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 2);                          # qinv, q, level
    $code .= <<___;
    vpbroadcastd 5*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm2", "%ymm3", "%ymm13", "%ymm2", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 2);                          # qinv, q, level
    $code .= <<___;
    vpbroadcastd 6*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm4", "%ymm5", "%ymm13", "%ymm4", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 2);                          # qinv, q, level
    $code .= <<___;
    vpbroadcastd 7*4(%r11), %ymm13
___
    &ntt_butterfly("%ymm6", "%ymm7", "%ymm13", "%ymm6", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm8", "%ymm9", "%ymm10", "%ymm11",            # tmp
                   "%ymm14", "%ymm15", 2);                          # qinv, q, level
$code .= <<___;

    vmovdqu %ymm0, $off+0*4(%rdi)
    vmovdqu %ymm1, $off+32*4(%rdi)
    vmovdqu %ymm2, $off+64*4(%rdi)
    vmovdqu %ymm3, $off+96*4(%rdi)
    vmovdqu %ymm4, $off+128*4(%rdi)
    vmovdqu %ymm5, $off+160*4(%rdi)
    vmovdqu %ymm6, $off+192*4(%rdi)
    vmovdqu %ymm7, $off+224*4(%rdi)
___
}

###############################################################################
# ntt_levels3to7
#
# Description:
#   Performs layers 3 through 7 of the NTT on 64 coefficients.
#
#   It operates on 8 YMM's registers, each YMM packs 8 32-bit coefficients (64
#   in total).
#   Contiguous coefficients are loaded into the YMM registers (no gap between them).
#
#   The function must be called 4 times to process 256 coefficients.  At each level, the
#   function executes butterfly operations in the correct coefficient pattern and applies the
#   corresponding twiddle factors ("zetas").
#
# Layer/level details:
#   - Level 3: offset = 16, step = 8;   zeta indexes 8...15
#   - Level 4: offset =  8, step = 16;  zeta indexes 16...31
#   - Level 5: offset =  4, step = 32;  zeta indexes 32...63
#   - Level 6: offset =  2, step = 64;  zeta indexes 64...127
#   - Level 7: offset =  1, step = 128; zeta indexes 128...255
#
# Prerequisites:
#   %rdi    - pointer to the coefficients
#   %r11    - pointer to the zetas (twiddle factors) table
#   %ymm14  - q_neg_inv (Montgomery reduction)
#   %ymm15  - Q (modulus)
#
# Arguments:
#   $off, $l3, $l4, $l5, $l6, $l7
#     $off - offset to the start of a set of 8 coefficients (in bytes, relative to %rdi)
#     $l3, $l4, $l5, $l6, $l7 - offsets to required zetas in the table, for levels 3 to 7
#
# Output:
#   In-place NTT-transformed coefficients for the selected group.
#
# Notes:
#   - Should be called 4 times per complete 256-coefficient transform (offsets 0, 64*4, 128*4, 192*4).
#   - All butterfly operations and twiddle applications handled by subroutine ntt_butterfly.
###############################################################################
sub ntt_levels3to7 {
    my ($off,$l3,$l4,$l5,$l6,$l7) = @_;

    $code .= <<___;
    # ==============================================================
    # level 3: offset = 16, step = 8
    # zeta indexes = 8, 9, 10, 11, 12, 13, 14, 15

    # broadcast zetas
    vpbroadcastd $l3(%r11), %ymm13  # zeta for coefficients 0-15

    # load w_even and w_odd
    vmovdqu $off(%rdi), %ymm0       # w_even[0:7]
    vmovdqu $off+32(%rdi), %ymm1    # w_even[8:15]
    vmovdqu $off+64(%rdi), %ymm2    # w_odd[0:7]
    vmovdqu $off+96(%rdi), %ymm3    # w_odd[8:15]
___
    # Process first 16 coefficients with zeta in ymm13
    &ntt_butterfly("%ymm0", "%ymm2", "%ymm13", "%ymm0", "%ymm2",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 3);                          # qinv, q, level
    &ntt_butterfly("%ymm1", "%ymm3", "%ymm13", "%ymm1", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 3);                          # qinv, q, level
$code .= <<___;
    # broadcast zetas
    vpbroadcastd $l3+4(%r11), %ymm13  # zeta for coefficients 16-31

    # load w_even and w_odd
    vmovdqu $off+128(%rdi), %ymm4   # w_even[16:23]
    vmovdqu $off+160(%rdi), %ymm5   # w_even[24:31]
    vmovdqu $off+192(%rdi), %ymm6  # w_odd[16:23]
    vmovdqu $off+224(%rdi), %ymm7  # w_odd[24:31]
___
    # Process next 16 coefficients with zeta in ymm13
    &ntt_butterfly("%ymm4", "%ymm6", "%ymm13", "%ymm4", "%ymm6",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 3);                          # qinv, q, level
    &ntt_butterfly("%ymm5", "%ymm7", "%ymm13", "%ymm5", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 3);                          # qinv, q, level
$code .= <<___;

    # ==============================================================
    # level 4: offset = 8, step = 16
    # zeta indexes = 16, 17, 18, ..., 30, 31

    # broadcast zetas for first 8 coefficients
    vpbroadcastd $l4(%r11), %ymm13      # zeta for coefficients 0-7

    # Prepare w_even and w_odd
    # Input dword layout:
    #   ymm0 = [ 0  1  2  3 |  4  5  6  7] (even)
    #   ymm1 = [ 8  9 10 11 | 12 13 14 15] (even)
    #   ymm2 = [16 17 18 19 | 20 21 22 23] (odd)
    #   ymm3 = [24 25 26 27 | 28 29 30 31] (odd)
    # Required dword layout is the same:
    #   ymm0 = [ 0  1  2  3 |  4  5  6  7] (even)
    #   ymm1 = [ 8  9 10 11 | 12 13 14 15] (odd)
    #   ymm2 = [16 17 18 19 | 20 21 22 23] (even)
    #   ymm3 = [24 25 26 27 | 28 29 30 31] (odd)

___
    &ntt_butterfly("%ymm0", "%ymm1", "%ymm13", "%ymm0", "%ymm1",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 4);                          # qinv, q, level
$code .= <<___;
    # broadcast zetas for next 8 coefficients
    vpbroadcastd $l4+4(%r11), %ymm13    # zeta for coefficients 8-15
___
    &ntt_butterfly("%ymm2", "%ymm3", "%ymm13", "%ymm2", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 4);                          # qinv, q, level
    $code .= <<___;
    # broadcast zetas for next 8 coefficients
    vpbroadcastd $l4+8(%r11), %ymm13    # zeta for coefficients 16-23
___
    &ntt_butterfly("%ymm4", "%ymm5", "%ymm13", "%ymm4", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 4);                          # qinv, q, level
$code .= <<___;
    # broadcast zetas for next 8 coefficients
    vpbroadcastd $l4+12(%r11), %ymm13   # zeta for coefficients 24-31
___
    &ntt_butterfly("%ymm6", "%ymm7", "%ymm13", "%ymm6", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 4);                          # qinv, q, level
    $code .= <<___;

    # ==============================================================
    # level 5: offset = 4, step = 32
    # zeta indexes = 32, 33, 34, ..., 62, 63

    # prepare w_even and w_odd
    # Input dword layout:
    #   ymm0 = [ 0  1  2  3 |  4  5  6  7] (even)
    #   ymm1 = [ 8  9 10 11 | 12 13 14 15] (odd)
    # Required dword layout:
    #   ymm8 = [ 0  1  2  3 |  8  9 10 11] (even)
    #   ymm1 = [ 4  5  6  7 | 12 13 14 15] (odd)
    vperm2i128 \$0x20, %ymm1, %ymm0, %ymm8
    vperm2i128 \$0x31, %ymm1, %ymm0, %ymm1

    # load zetas for first 8 coefficients
    vpbroadcastd $l5(%r11), %ymm13         # zetas for coefficients 0-3
    vpbroadcastd $l5+4(%r11), %ymm12       # zetas for coefficients 4-7
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13 # blend into ymm13

___
    &ntt_butterfly("%ymm8", "%ymm1", "%ymm13", "%ymm0", "%ymm1",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 5);                          # qinv, q, level
$code .= <<___;
    # prepare w_even and w_odd
    vperm2i128 \$0x20, %ymm3, %ymm2, %ymm8
    vperm2i128 \$0x31, %ymm3, %ymm2, %ymm3

    # load zetas for first 8 coefficients
    vpbroadcastd $l5+8(%r11), %ymm13        # zetas for coefficients 8-11
    vpbroadcastd $l5+12(%r11), %ymm12       # zetas for coefficients 12-15
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13 # blend into ymm13

___
    &ntt_butterfly("%ymm8", "%ymm3", "%ymm13", "%ymm2", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 5);                          # qinv, q, level
    $code .= <<___;
    # prepare w_even and w_odd
    vperm2i128 \$0x20, %ymm5, %ymm4, %ymm8
    vperm2i128 \$0x31, %ymm5, %ymm4, %ymm5

    # load zetas for next 8 coefficients
    vpbroadcastd $l5+16(%r11), %ymm13       # zetas for coefficients 16-19
    vpbroadcastd $l5+20(%r11), %ymm12       # zetas for coefficients 20-23
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13 # blend into ymm13

___
    &ntt_butterfly("%ymm8", "%ymm5", "%ymm13", "%ymm4", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 5);                          # qinv, q, level
$code .= <<___;
    # prepare w_even and w_odd
    vperm2i128 \$0x20, %ymm7, %ymm6, %ymm8
    vperm2i128 \$0x31, %ymm7, %ymm6, %ymm7

    # load zetas for next 8 coefficients
    vpbroadcastd $l5+24(%r11), %ymm13       # zetas for coefficients 24-27
    vpbroadcastd $l5+28(%r11), %ymm12       # zetas for coefficients 28-31
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13 # blend into ymm13

___
    &ntt_butterfly("%ymm8", "%ymm7", "%ymm13", "%ymm6", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 5);                          # qinv, q, level
    $code .= <<___;

    # ==============================================================
    # level 6: offset = 2, step = 64
    # zeta indexes = 64, 65, 66, ..., 62, 127

    # Input DWORD layout in memory:
    #   ymm0 = [ 0  1  2  3] [ 8  9 10 11] (even)
    #   ymm1 = [ 4  5  6  7] [12 13 14 15] (odd)
    #   ymm2 = [16 17 18 19] [24 25 26 27] (even)
    #   ymm3 = [20 21 22 23] [28 29 30 31] (odd)
    # Desired DWORD layout:
    #   ymm8 = [ 0  1  4  5] [ 8  9 12 13] (even)
    #   ymm1 = [ 2  3  6  7] [10 11 14 15] (odd)

    # load & prepare zetas
    # - it is enough that the 1st dword of each qword is populated
    vmovdqu $l6(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13

    # prepare w_even and w_odd
    vpunpcklqdq %ymm1, %ymm0, %ymm8
    vpunpckhqdq %ymm1, %ymm0, %ymm1
___
    &ntt_butterfly("%ymm8", "%ymm1", "%ymm13", "%ymm0", "%ymm1",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 6);                          # qinv, q, level
    $code .= <<___;
    # load & prepare zetas
    vmovdqu $l6+16(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13

    # prepare w_even and w_odd
    vpunpcklqdq %ymm3, %ymm2, %ymm8
    vpunpckhqdq %ymm3, %ymm2, %ymm3
___
    &ntt_butterfly("%ymm8", "%ymm3", "%ymm13", "%ymm2", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 6);                          # qinv, q, level
    $code .= <<___;

    # load & prepare zetas
    vmovdqu $l6+32(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13

    # prepare w_even and w_odd
    vpunpcklqdq %ymm5, %ymm4, %ymm8
    vpunpckhqdq %ymm5, %ymm4, %ymm5
___
    &ntt_butterfly("%ymm8", "%ymm5", "%ymm13", "%ymm4", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 6);                          # qinv, q, level
$code .= <<___;
    # load & prepare zetas
    vmovdqu $l6+48(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13

    # prepare w_even and w_odd
    vpunpcklqdq %ymm7, %ymm6, %ymm8
    vpunpckhqdq %ymm7, %ymm6, %ymm7
___
    &ntt_butterfly("%ymm8", "%ymm7", "%ymm13", "%ymm6", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 6);                          # qinv, q, level
    $code .= <<___;

    # ==============================================================
    # level 7: offset = 1, step = 128
    # zeta indexes = 128, 129, 130, ..., 254, 255

    # Input DWORD layout:
    #   ymm0 = [ 0  1  4  5] [ 8  9 12 13]
    #   ymm1 = [ 2  3  6  7] [10 11 14 15]
    # Required DWORD layout:
    #   ymm0 = [ 0  2  4  6] [ 8 10 12 14] (even)
    #   ymm1 = [ 1  3  5  7] [ 9 11 13 15] (odd)

    vpunpckldq   %ymm1, %ymm0, %ymm8      # ymm8 = [0 2 1 3] [ 8 10  9 11]
    vpunpckhdq   %ymm1, %ymm0, %ymm9      # ymm9 = [4 6 5 7] [12 14 13 15]

    vshufps      \$0xEE, %ymm9, %ymm8, %ymm1   # ymm1 = [1 3 5 7] [ 9 11 13 15]
    vshufps      \$0x44, %ymm9, %ymm8, %ymm0   # ymm0 = [0 2 4 6] [ 8 10 12 14]

    # load zetas
    vmovdqu $l7(%r11), %ymm13          # 8 zetas for coefficients 0-7
___
    &ntt_butterfly("%ymm0", "%ymm1", "%ymm13", "%ymm0", "%ymm1",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 7);                          # qinv, q, level
$code .= <<___;
    # load zetas
    vmovdqu $l7+32(%r11), %ymm13       # 8 zetas for coefficients 8-15

    # format w_even and w_odd
    vpunpckldq   %ymm3, %ymm2, %ymm8
    vpunpckhdq   %ymm3, %ymm2, %ymm9

    vshufps      \$0xEE, %ymm9, %ymm8, %ymm3
    vshufps      \$0x44, %ymm9, %ymm8, %ymm2
___
    &ntt_butterfly("%ymm2", "%ymm3", "%ymm13", "%ymm2", "%ymm3",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 7);                          # qinv, q, level
    $code .= <<___;

    # load zetas
    vmovdqu $l7+64(%r11), %ymm13       # 8 zetas for coefficients 16-23

    # format w_even and w_odd
    vpunpckldq   %ymm5, %ymm4, %ymm8
    vpunpckhdq   %ymm5, %ymm4, %ymm9

    vshufps      \$0xEE, %ymm9, %ymm8, %ymm5
    vshufps      \$0x44, %ymm9, %ymm8, %ymm4
___
    &ntt_butterfly("%ymm4", "%ymm5", "%ymm13", "%ymm4", "%ymm5",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 7);                          # qinv, q, level
$code .= <<___;
    # load zetas
    vmovdqu $l7+96(%r11), %ymm13       # 8 zetas for coefficients 24-31

    # format w_even and w_odd
    vpunpckldq   %ymm7, %ymm6, %ymm8
    vpunpckhdq   %ymm7, %ymm6, %ymm9

    vshufps      \$0xEE, %ymm9, %ymm8, %ymm7
    vshufps      \$0x44, %ymm9, %ymm8, %ymm6
___
    &ntt_butterfly("%ymm6", "%ymm7", "%ymm13", "%ymm6", "%ymm7",    # w_even, w_odd, zetas, n_even, n_odd
                   "%ymm9", "%ymm10", "%ymm11", "%ymm12",           # tmp
                   "%ymm14", "%ymm15", 7);                          # qinv, q, level
    $code .= <<___;

    # Interleave and store first 16
    vpunpckldq %ymm1, %ymm0, %ymm8
    vpunpckhdq %ymm1, %ymm0, %ymm9

    vperm2i128 \$0x20, %ymm9, %ymm8, %ymm10
    vperm2i128 \$0x31, %ymm9, %ymm8, %ymm11
    vmovdqu %ymm10, $off(%rdi)
    vmovdqu %ymm11, $off+32(%rdi)

    vpunpckldq  %ymm3, %ymm2, %ymm8
    vpunpckhdq  %ymm3, %ymm2, %ymm9

    vperm2i128 \$0x20, %ymm9, %ymm8, %ymm10
    vperm2i128 \$0x31, %ymm9, %ymm8, %ymm11
    vmovdqu %ymm10, $off+64(%rdi)
    vmovdqu %ymm11, $off+96(%rdi)

    # Interleave and store second 16
    vpunpckldq %ymm5, %ymm4, %ymm8
    vpunpckhdq %ymm5, %ymm4, %ymm9

    vperm2i128 \$0x20, %ymm9, %ymm8, %ymm10
    vperm2i128 \$0x31, %ymm9, %ymm8, %ymm11
    vmovdqu %ymm10, $off+128(%rdi)
    vmovdqu %ymm11, $off+160(%rdi)

    vpunpckldq  %ymm7, %ymm6, %ymm8
    vpunpckhdq  %ymm7, %ymm6, %ymm9

    vperm2i128 \$0x20, %ymm9, %ymm8, %ymm10
    vperm2i128 \$0x31, %ymm9, %ymm8, %ymm11
    vmovdqu %ymm10, $off+192(%rdi)
    vmovdqu %ymm11, $off+224(%rdi)
___
}

###############################################################################
###############################################################################
###
### INTT (Inverse Number Theoretic Transform)
###
###############################################################################
###############################################################################

###############################################################################
# intt_butterfly
#
# Description:
#   Performs the butterfly operation for a single stage of the INTT
#   on two 8-element vectors of 32-bit integers (YMM).
#
#   Implements the core data-mixing and modular multiplication steps with a
#   zeta (twiddle factor), including the modular reductions. The updated even and
#   odd results are put into the designated registers.
#
#   This butterfly operation computes:
#     n_even = (w_even + w_odd) mod Q
#     n_odd  = ((w_even + Q) - w_odd) * zetas mod Q
#   where Q is the NTT modulus, and zetas is the power of a primitive root used
#   for this stage.
#
# Parameters:
#   w_even    - YMM with "even" input coefficients
#   w_odd     - YMM with "odd" input coefficients
#   zetas     - YMM with modular twiddle factors for this butterfly
#   tmp0-tmp2 - Scratch YMM registers for temporary/intermediate values
#   n_even    - Output YMM for new even coefficients
#   n_odd     - Output YMM for new odd coefficients
#   q_neg_inv - YMM with -Q^{-1} mod 2^32 for Montgomery reduction
#   q         - YMM with modulus Q
#   level     - current INTT processing level
#
# Output:
#   n_even, n_odd
#
# Side effects:
#   Clobbers tmp0, tmp1, tmp2
###############################################################################

sub intt_butterfly {
    my ($w_even, $w_odd, $zetas,
        $tmp0, $tmp1, $tmp2,
        $n_even, $n_odd,
        $q_neg_inv, $q, $level) = @_;

$code .= <<___;
    # n_even = reduce_once(w_even + w_odd)
    # n_odd = (w_even + Q) - w_odd
    vpaddd $q, $w_even, $tmp1           # n_odd: tmp1 = w_even + Q
    vpaddd $w_even, $w_odd, $n_even     # n_even: n_even = w_even + w_odd
    vpsubd $w_odd, $tmp1, $n_odd        # n_odd: n_odd = tmp1 - w_odd

    vpcmpgtd $n_even, $q, $tmp0         # n_even: tmp0 = $q > $n_even ? 0xffffffff : 0
    vpandn $q, $tmp0, $tmp0             # n_even: tmp0 = ~$tmp0 & $q
    vpsubd $tmp0, $n_even, $n_even      # n_even: n_even -= $tmp0

    # Multiply n_odd by zetas (step root)
___
    if ($level < 1) {
        # level 0: each zeta (B) dword is different
        &multiply_mod_Q($n_odd,     # A
                        $zetas,     # B
                        $n_odd,     # out (AxB)
                        $tmp0, $tmp1, $tmp2,    # tmp
                        $q_neg_inv, $q, 0);     # qinv, q, no-bcast32
    } else {
        # levels 1 to 7: same zeta (B) dwords within each qword
        &multiply_mod_Q($n_odd,     # A
                        $zetas,     # B
                        $n_odd,     # out (AxB)
                        $tmp0, $tmp1, $tmp2,    # tmp
                        $q_neg_inv, $q, 1);     # qinv, q, bcast32
    }
}

###############################################################################
# intt_levels0to4
#
# Description:
#   Executes the first five stages (levels 0–4) of the INTT
#   on groups of 32 coefficients (4 YMM registers).
#
#   This function hierarchically mixes and transforms groups of coefficients using
#   butterfly operations and level specific zeta (twiddle) factors, performing all required
#   re-packing and permutations for each layer.
#
#   Each call operates on a block of 64 coefficients, and must be repeated 4 times (with
#   offsets 0, 64*4, 128*4, 192*4) to process all 256 coefficients.
#
# Layer/Level details:
#   - Level 0: offset = 1,   step = 128;  zeta indexes (new) = 0..127
#   - Level 1: offset = 2,   step = 64;   zeta indexes (new) = 128..191
#   - Level 2: offset = 4,   step = 32;   zeta indexes (new) = 192..223
#   - Level 3: offset = 8,   step = 16;   zeta indexes (new) = 224..239
#   - Level 4: offset = 16,  step = 8;    zeta indexes (new) = 240..247
#
# Prerequisites:
#   %rdi    - pointer to the coefficients array
#   %r11    - pointer to the zetas (twiddle factors) table
#   %ymm14  - q_neg_inv (for Montgomery reduction)
#   %ymm15  - Q (modulus)
#
# Arguments:
#   $off    - offset (in bytes) to the start of the 16-coefficient group
#   $l0-$l4 - offsets (in bytes) into the zeta table for each INTT level 0..4
#
# Output:
#   Updated coefficients are written in-place in memory.
#
# Notes:
#   - Function must be called 4 times for a full 256-coefficient INTT layer sweep.
###############################################################################

sub intt_levels0to4 {
    my ($off,$l0,$l1,$l2,$l3,$l4) = @_;
    $code .= <<___;
    # ==============================================================
    # level 0: offset = 1, step = 128
    # zeta indexes (original table) = 255, 254, 253, ... 129, 128
    # zeta indexes (new table) = 0, 1, 2, .. 127

    # dword layout in memory:
    #   ymm0 = [0,1,2,3 | 4,5,6,7]
    #   ymm1 = [8,9,10,11 | 12,13,14,15]
    # required dword layout in registers:
    #   ymm0 = [0,2,4,6 | 8,10,12,14]
    #   ymm1 = [1,3,5,7 | 9,11,13,15]

    # load w_even and w_odd
    vmovdqu $off(%rdi), %ymm8
    vmovdqu $off+32(%rdi), %ymm9

    # compact even words into ymm0 (w_even[0:7])
    vmovdqa idx_even(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10            # [ 0, 2, 4, 6 | 0, 2, 4, 6]
    vpermd %ymm9, %ymm13, %ymm11            # [ 8,10,12,14 | 8,10,12,14]
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm0  # [ 0, 2, 4, 6 | 8,10,12,14]

    # compact odd words into ymm1 (w_odd[0..7])
    vmovdqa idx_odd(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10            # [ 1, 3, 5, 7 | 1, 3, 5, 7]
    vpermd %ymm9, %ymm13, %ymm11            # [ 9,11,13,15 | 9,11,13,15]
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm1  # [ 1, 3, 5, 7 | 9,11,13,15]

    # load 16 zetas
    vmovdqu $l0(%r11), %ymm13

___

    &intt_butterfly("%ymm0", "%ymm1", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm0", "%ymm1",               # n_even, n_odd
                    "%ymm14", "%ymm15", 0);         # qinv, q, level

$code .= <<___;

    # load w_even and w_odd
    vmovdqu $off+64(%rdi), %ymm8
    vmovdqu $off+96(%rdi), %ymm9

    # compact even words into ymm2 (w_even[8..15])
    vmovdqa idx_even(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10
    vpermd %ymm9, %ymm13, %ymm11
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm2

    # compact odd words into ymm3 (w_odd[8..15])
    vmovdqa idx_odd(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10
    vpermd %ymm9, %ymm13, %ymm11
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm3

    # load 16 zetas
    vmovdqu $l0+32(%r11), %ymm13

___

    &intt_butterfly("%ymm2", "%ymm3", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm2", "%ymm3",               # n_even, n_odd
                    "%ymm14", "%ymm15", 0);         # qinv, q, level

$code .= <<___;
    # load w_even and w_odd
    vmovdqu $off+128(%rdi), %ymm8
    vmovdqu $off+160(%rdi), %ymm9

    # compact even words into ymm4 (w_even[16..23])
    vmovdqa idx_even(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10            # [ 0, 2, 4, 6 | 0, 2, 4, 6]
    vpermd %ymm9, %ymm13, %ymm11            # [ 8,10,12,14 | 8,10,12,14]
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm4  # [ 0, 2, 4, 6 | 8,10,12,14]

    # compact odd words into ymm5 (w_odd[16..23])
    vmovdqa idx_odd(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10            # [ 1, 3, 5, 7 | 1, 3, 5, 7]
    vpermd %ymm9, %ymm13, %ymm11            # [ 9,11,13,15 | 9,11,13,15]
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm5  # [ 1, 3, 5, 7 | 9,11,13,15]

    # load 16 zetas
    vmovdqu $l0+64(%r11), %ymm13

___

    &intt_butterfly("%ymm4", "%ymm5", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm4", "%ymm5",               # n_even, n_odd
                    "%ymm14", "%ymm15", 0);         # qinv, q, level

$code .= <<___;

    # load w_even and w_odd
    vmovdqu $off+192(%rdi), %ymm8
    vmovdqu $off+224(%rdi), %ymm9

    # compact even words into ymm6 (w_even[24..31])
    vmovdqa idx_even(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10
    vpermd %ymm9, %ymm13, %ymm11
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm6

    # compact odd words into ymm7 (w_odd[24..31])
    vmovdqa idx_odd(%rip), %ymm13
    vpermd %ymm8, %ymm13, %ymm10
    vpermd %ymm9, %ymm13, %ymm11
    vpblendd \$0xf0, %ymm11, %ymm10, %ymm7

    # load 16 zetas
    vmovdqu $l0+96(%r11), %ymm13

___

    &intt_butterfly("%ymm6", "%ymm7", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm6", "%ymm7",               # n_even, n_odd
                    "%ymm14", "%ymm15", 0);         # qinv, q, level

$code .= <<___;

    # ==============================================================
    # level 1: offset = 2, step = 64
    # zeta indexes = 127, 126, 125, ... 65, 64
    # zeta indexes (new) = 128, 129, .. 191

    # Result DWORD layout:
    #   ymm0 = [0,  2,  4,  6,  8, 10, 12, 14]
    #   ymm1 = [1,  3,  5,  7,  9, 11, 13, 15]
    # Desired layout for this phase:
    #   %ymm0 = [0,1,4,5,8,9,12,13]
    #   %ymm1 = [2,3,6,7,10,11,14,15]

    # Interleave even/odd within each 128-bit lane:
    vpunpckldq %ymm1, %ymm0, %ymm8     # A = [0,1,2,3 | 8,9,10,11]
    vpunpckhdq %ymm1, %ymm0, %ymm9     # B = [4,5,6,7 | 12,13,14,15]

    # [0,1,4,5 | 8,9,12,13]
    vshufps \$0x44, %ymm9, %ymm8, %ymm0

    # [2,3,6,7 | 10,11,14,15]
    vshufps \$0xee, %ymm9, %ymm8, %ymm1

    # load 4 zetas and populate across ymm
    vmovdqu $l1(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13            # [0 - 1 -] [2 - 3 -]
___

    &intt_butterfly("%ymm0", "%ymm1", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm0", "%ymm1",               # n_even, n_odd
                    "%ymm14", "%ymm15", 1);         # qinv, q, level

$code .= <<___;

    # Interleave even/odd within each 128-bit lane:
    vpunpckldq %ymm3, %ymm2, %ymm8     # A = [0,1,2,3 | 8,9,10,11]
    vpunpckhdq %ymm3, %ymm2, %ymm9     # B = [4,5,6,7 | 12,13,14,15]

    # ymm2 = [0,1,4,5 | 8,9,12,13]
    vshufps \$0x44, %ymm9, %ymm8, %ymm2

    # ymm3 = [2,3,6,7 | 10,11,14,15]
    vshufps \$0xee, %ymm9, %ymm8, %ymm3

    # load 4 zetas and populate across YMM
    vmovdqu $l1+16(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13            # [0 - 1 -] [2 - 3 -]
___

    &intt_butterfly("%ymm2", "%ymm3", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm2", "%ymm3",               # n_even, n_odd
                    "%ymm14", "%ymm15", 1);         # qinv, q, level

$code .= <<___;
    # Interleave even/odd within each 128-bit lane:
    vpunpckldq %ymm5, %ymm4, %ymm8     # A = [0,1,2,3 | 8,9,10,11]
    vpunpckhdq %ymm5, %ymm4, %ymm9     # B = [4,5,6,7 | 12,13,14,15]

    # [0,1,4,5 | 8,9,12,13]
    vshufps \$0x44, %ymm9, %ymm8, %ymm4

    # [2,3,6,7 | 10,11,14,15]
    vshufps \$0xee, %ymm9, %ymm8, %ymm5

    # load 4 zetas and populate across ymm
    vmovdqu $l1+32(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13            # [0 - 1 -] [2 - 3 -]
___

    &intt_butterfly("%ymm4", "%ymm5", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm4", "%ymm5",               # n_even, n_odd
                    "%ymm14", "%ymm15", 1);         # qinv, q, level

$code .= <<___;

    # Interleave even/odd within each 128-bit lane:
    vpunpckldq %ymm7, %ymm6, %ymm8     # A = [0,1,2,3 | 8,9,10,11]
    vpunpckhdq %ymm7, %ymm6, %ymm9     # B = [4,5,6,7 | 12,13,14,15]

    # ymm6 = [0,1,4,5 | 8,9,12,13]
    vshufps \$0x44, %ymm9, %ymm8, %ymm6

    # ymm7 = [2,3,6,7 | 10,11,14,15]
    vshufps \$0xee, %ymm9, %ymm8, %ymm7

    # load 4 zetas and populate across YMM
    vmovdqu $l1+48(%r11), %xmm13
    vpmovzxdq %xmm13, %ymm13            # [0 - 1 -] [2 - 3 -]
___

    &intt_butterfly("%ymm6", "%ymm7", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm6", "%ymm7",               # n_even, n_odd
                    "%ymm14", "%ymm15", 1);         # qinv, q, level

$code .= <<___;

    # ==============================================================
    # level 2: offset = 4, step = 32
    # zeta indexes = 63, 62, 61, ... 33, 32
    # zeta indexes (new) = 192, 193, 194, ... 223

    # Result DWORD layout:
    #   ymm0 = [0,1,4,5,8,9,12,13]
    #   ymm1 = [16,17,20,21,24,25,28,29]
    # Desired layout:
    #   ymm8  = [0,1,2,3, 8,9,10,11]
    #   ymm1 = [4,5,6,7, 12,13,14,15]

    # ymm8 = [0,1,2,3 | 8,9,10,11]
    vshufps \$0x44, %ymm1, %ymm0, %ymm8

    # ymm1 = [4,5,6,7 | 12,13,14,15]
    vshufps \$0xee, %ymm1, %ymm0, %ymm1

    # broadcast zetas
    vpbroadcastd $l2(%r11), %ymm13
    vpbroadcastd $l2+4(%r11), %ymm12
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13
___

    &intt_butterfly("%ymm8", "%ymm1", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm0", "%ymm1",               # n_even, n_odd
                    "%ymm14", "%ymm15", 2);         # qinv, q, level

$code .= <<___;
    vshufps \$0x44, %ymm3, %ymm2, %ymm8
    vshufps \$0xee, %ymm3, %ymm2, %ymm3

    # broadcast zetas
    vpbroadcastd $l2+8(%r11), %ymm13
    vpbroadcastd $l2+12(%r11), %ymm12
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13
___

    &intt_butterfly("%ymm8", "%ymm3", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm2", "%ymm3",               # n_even, n_odd
                    "%ymm14", "%ymm15", 2);         # qinv, q, level

$code .= <<___;
    vshufps \$0x44, %ymm5, %ymm4, %ymm8
    vshufps \$0xee, %ymm5, %ymm4, %ymm5

    # broadcast zetas
    vpbroadcastd $l2+16(%r11), %ymm13
    vpbroadcastd $l2+20(%r11), %ymm12
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13
___

    &intt_butterfly("%ymm8", "%ymm5", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm4", "%ymm5",               # n_even, n_odd
                    "%ymm14", "%ymm15", 2);         # qinv, q, level

$code .= <<___;
    vshufps \$0x44, %ymm7, %ymm6, %ymm8
    vshufps \$0xee, %ymm7, %ymm6, %ymm7

    # broadcast zetas
    vpbroadcastd $l2+24(%r11), %ymm13
    vpbroadcastd $l2+28(%r11), %ymm12
    vpblendd \$0xf0, %ymm12, %ymm13, %ymm13
___

    &intt_butterfly("%ymm8", "%ymm7", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm6", "%ymm7",               # n_even, n_odd
                    "%ymm14", "%ymm15", 2);         # qinv, q, level

$code .= <<___;

    # ==============================================================
    # level 3: offset = 8, step = 16
    # zeta indexes = 31, 30, 29, ... 17, 16
    # zeta indexes (new) = 224, 225, 226, ... 239

    #   ymm0 = [0,1,2,3 | 8,9,10,11]
    #   ymm1 = [16,17,18,19 | 24,25,26,27]
    vperm2i128 \$0x20, %ymm1, %ymm0, %ymm8      # [0,1,2,3 | 4,5,6,7]
    vperm2i128 \$0x31, %ymm1, %ymm0, %ymm1      # [8,9,10,11 | 12,13,14,15]

    # broadcast zetas
    vpbroadcastd $l3(%r11), %ymm13
___

    &intt_butterfly("%ymm8", "%ymm1", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm0", "%ymm1",               # n_even, n_odd
                    "%ymm14", "%ymm15", 3);         # qinv, q, level

$code .= <<___;

    vperm2i128 \$0x20, %ymm3, %ymm2, %ymm8
    vperm2i128 \$0x31, %ymm3, %ymm2, %ymm3

    # broadcast zetas
    vpbroadcastd $l3+4(%r11), %ymm13
___

    &intt_butterfly("%ymm8", "%ymm3", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm2", "%ymm3",               # n_even, n_odd
                    "%ymm14", "%ymm15", 3);         # qinv, q, level

$code .= <<___;
    vperm2i128 \$0x20, %ymm5, %ymm4, %ymm8      # [0,1,2,3 | 4,5,6,7]
    vperm2i128 \$0x31, %ymm5, %ymm4, %ymm5      # [8,9,10,11 | 12,13,14,15]

    # broadcast zetas
    vpbroadcastd $l3+8(%r11), %ymm13
___

    &intt_butterfly("%ymm8", "%ymm5", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm4", "%ymm5",               # n_even, n_odd
                    "%ymm14", "%ymm15", 3);         # qinv, q, level

$code .= <<___;

    vperm2i128 \$0x20, %ymm7, %ymm6, %ymm8
    vperm2i128 \$0x31, %ymm7, %ymm6, %ymm7

    # broadcast zetas
    vpbroadcastd $l3+12(%r11), %ymm13
___

    &intt_butterfly("%ymm8", "%ymm7", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm6", "%ymm7",               # n_even, n_odd
                    "%ymm14", "%ymm15", 3);         # qinv, q, level

$code .= <<___;

    # ==============================================================
    # level 4: offset = 16, step = 8
    # zeta indexes = 15, 14, 13, ..., 9, 8
    # zeta indexes (new) = 240, 241, 242, ... 247

    # broadcast zetas
    vpbroadcastd $l4(%r11), %ymm13
___

    &intt_butterfly("%ymm0", "%ymm2", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm0", "%ymm2",               # n_even, n_odd
                    "%ymm14", "%ymm15", 4);         # qinv, q, level

    &intt_butterfly("%ymm1", "%ymm3", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm1", "%ymm3",               # n_even, n_odd
                    "%ymm14", "%ymm15", 4);         # qinv, q, level

$code .= <<___;
    # broadcast zetas
    vpbroadcastd $l4+4(%r11), %ymm13
___

    &intt_butterfly("%ymm4", "%ymm6", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm4", "%ymm6",               # n_even, n_odd
                    "%ymm14", "%ymm15", 4);         # qinv, q, level

    &intt_butterfly("%ymm5", "%ymm7", "%ymm13",     # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12",   # temporary
                    "%ymm5", "%ymm7",               # n_even, n_odd
                    "%ymm14", "%ymm15", 4);         # qinv, q, level

$code .= <<___;

    # Store new w_even and w_odd
    vmovdqu %ymm0, $off(%rdi)
    vmovdqu %ymm1, $off+32(%rdi)
    vmovdqu %ymm2, $off+64(%rdi)
    vmovdqu %ymm3, $off+96(%rdi)
    vmovdqu %ymm4, $off+128(%rdi)
    vmovdqu %ymm5, $off+160(%rdi)
    vmovdqu %ymm6, $off+192(%rdi)
    vmovdqu %ymm7, $off+224(%rdi)
___
}

###############################################################################
# intt_levels5to7
#
# Description:
#   Processes the last three levels (5 to 7) of the INTT on 64 coefficients.
#
#   It completes the hierarchical merging of INTT, applying stage-specific zeta
#   (twiddle) factors and modular butterfly operations for each level, and performs the final
#   post-processing Montgomery multiplication at the end of the transform.
#
#   It must be invoked 4 times with offsets equal to 0*4, 8*4, 16*4 and 24*4 to cover
#   all 256 coefficients.
#
# Layer/Level details:
#   - Level 5: offset =  32, step = 4;   zeta indexes (new) = 248..251
#   - Level 6: offset =  64, step = 2;   zeta indexes (new) = 252, 253
#   - Level 7: offset = 128, step = 1;   zeta index   (new) = 254
#
#   After all INTT levels, multiplies the output by the Montgomery factor
#   corresponding to the inverse transform scaling (usually the modular inverse of the
#   NTT degree in Montgomery form), to obtain the final reduced coefficients.
#
# Prerequisites:
#   %rdi    - pointer to the coefficients array
#   %r11    - pointer to the zetas (twiddle factors) table
#   %ymm14  - q_neg_inv (for Montgomery reduction)
#   %ymm15  - Q (modulus)
#
# Arguments:
#   $off    - offset (in bytes) to the start of the 8-coefficient group
#
# Output:
#   Overwrites memory at the given offset with the INTT-processed coefficients.
#
# Notes:
#   - This subroutine must be called 4 times with appropriate offsets to process
#     all 256 coefficients.
###############################################################################

sub intt_levels5to7 {
    my ($off) = @_;
    $code .= <<___;
    vmovdqu $off+0*4(%rdi), %ymm0
    vmovdqu $off+32*4(%rdi), %ymm1
    vmovdqu $off+64*4(%rdi), %ymm2
    vmovdqu $off+96*4(%rdi), %ymm3
    vmovdqu $off+128*4(%rdi), %ymm4
    vmovdqu $off+160*4(%rdi), %ymm5
    vmovdqu $off+192*4(%rdi), %ymm6
    vmovdqu $off+224*4(%rdi), %ymm7

    # ==============================================================
    # level 5: offset = 32, step = 4

    vpbroadcastd 248*4(%r11), %ymm13
___
    &intt_butterfly("%ymm0", "%ymm1", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm0", "%ymm1",           # n_even, n_odd
                    "%ymm14", "%ymm15", 5);     # qinv, q, level
    $code .= <<___;
    vpbroadcastd 249*4(%r11), %ymm13
___
    &intt_butterfly("%ymm2", "%ymm3", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm2", "%ymm3",           # n_even, n_odd
                    "%ymm14", "%ymm15", 5);     # qinv, q, level

    $code .= <<___;
    vpbroadcastd 250*4(%r11), %ymm13
___
    &intt_butterfly("%ymm4", "%ymm5", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm4", "%ymm5",           # n_even, n_odd
                    "%ymm14", "%ymm15", 5);     # qinv, q, level

    $code .= <<___;
    vpbroadcastd 251*4(%r11), %ymm13
___
    &intt_butterfly("%ymm6", "%ymm7", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm6", "%ymm7",           # n_even, n_odd
                    "%ymm14", "%ymm15", 5);     # qinv, q, level
    $code .= <<___;

    # ==============================================================
    # level 6: offset = 64, step = 2

    vpbroadcastd 252*4(%r11), %ymm13
___
    &intt_butterfly("%ymm0", "%ymm2", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm0", "%ymm2",           # n_even, n_odd
                    "%ymm14", "%ymm15", 6);     # qinv, q, level
    &intt_butterfly("%ymm1", "%ymm3", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm1", "%ymm3",           # n_even, n_odd
                    "%ymm14", "%ymm15", 6);     # qinv, q, level
    $code .= <<___;
    vpbroadcastd 253*4(%r11), %ymm13
___
    &intt_butterfly("%ymm4", "%ymm6", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm4", "%ymm6",           # n_even, n_odd
                    "%ymm14", "%ymm15", 6);     # qinv, q, level
    &intt_butterfly("%ymm5", "%ymm7", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm5", "%ymm7",           # n_even, n_odd
                    "%ymm14", "%ymm15", 6);     # qinv, q, level
$code .= <<___;

    # ==============================================================
    # level 7: offset = 128, step = 1

    vpbroadcastd 254*4(%r11), %ymm13
___
    &intt_butterfly("%ymm0", "%ymm4", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm0", "%ymm4",           # n_even, n_odd
                    "%ymm14", "%ymm15", 7);     # qinv, q, level
    &intt_butterfly("%ymm1", "%ymm5", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm1", "%ymm5",           # n_even, n_odd
                    "%ymm14", "%ymm15", 7);     # qinv, q, level
    &intt_butterfly("%ymm2", "%ymm6", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm2", "%ymm6",           # n_even, n_odd
                    "%ymm14", "%ymm15", 7);     # qinv, q, level
    &intt_butterfly("%ymm3", "%ymm7", "%ymm13", # w_even, w_odd, zetas
                    "%ymm10", "%ymm11", "%ymm12", # temporary
                    "%ymm3", "%ymm7",           # n_even, n_odd
                    "%ymm14", "%ymm15", 7);     # qinv, q, level
$code .= <<___;

    # ==============================================================
    # extra multiply

    vpbroadcastd ml_dsa_inverse_degree_montgomery(%rip), %ymm13
___

    &multiply_mod_Q("%ymm0", "%ymm13", "%ymm0",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm4", "%ymm13", "%ymm4",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm1", "%ymm13", "%ymm1",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm5", "%ymm13", "%ymm5",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm2", "%ymm13", "%ymm2",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm6", "%ymm13", "%ymm6",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm3", "%ymm13", "%ymm3",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32
    &multiply_mod_Q("%ymm7", "%ymm13", "%ymm7",   # A, B, out (AxB)
                    "%ymm10", "%ymm11", "%ymm12", # tmp
                    "%ymm14", "%ymm15", 1);       # qinv, q, bcast32

$code .= <<___;

    vmovdqu %ymm0, $off+0*4(%rdi)
    vmovdqu %ymm1, $off+32*4(%rdi)
    vmovdqu %ymm2, $off+64*4(%rdi)
    vmovdqu %ymm3, $off+96*4(%rdi)
    vmovdqu %ymm4, $off+128*4(%rdi)
    vmovdqu %ymm5, $off+160*4(%rdi)
    vmovdqu %ymm6, $off+192*4(%rdi)
    vmovdqu %ymm7, $off+224*4(%rdi)
___
}

$code .= <<___;
###############################################################################
###############################################################################
### Data section

.section .rodata

###############################################################################
# zetas_inverse:
#
# Description:
#   Table of inverse NTT "zetas" (twiddle factors), precomputed as powers of the
#   primitive root of unity (mod Q) required for each stage of the inverse Number
#   Theoretic Transform (INTT). Entries represent the modular inverses of the
#   forward NTT zetas (ml_dsa_ntt.c), arranged in bit-reversed or stage-order
#   as required by the INTT implementation.
#   Elements of the table are in reversed order vs the original table for the
#   forward NTT. This is reduce permute operations when preparing zetas in the
#   correct format for butterfly operations.
#
#   Each .long entry corresponds to a 32-bit modular value (Q - zeta) for the
#   appropriate stage and index.
#
#   The exact arrangement and computation of these zetas matches the INTT
#   schedule and must be consistent with the NTT forward transform (ml_dsa_ntt.c).
###############################################################################
.align 64
zetas_inverse:
    .long $ML_DSA_Q - 1976782, $ML_DSA_Q - 7534263, $ML_DSA_Q - 1400424, $ML_DSA_Q - 3937738, $ML_DSA_Q - 7018208, $ML_DSA_Q - 8332111, $ML_DSA_Q - 3919660, $ML_DSA_Q - 7826001
    .long $ML_DSA_Q - 4834730, $ML_DSA_Q - 1612842, $ML_DSA_Q - 7403526, $ML_DSA_Q - 183443,  $ML_DSA_Q - 6094090, $ML_DSA_Q - 7959518, $ML_DSA_Q - 6144432, $ML_DSA_Q - 5441381
    .long $ML_DSA_Q - 4546524, $ML_DSA_Q - 8119771, $ML_DSA_Q - 7276084, $ML_DSA_Q - 6712985, $ML_DSA_Q - 1910376, $ML_DSA_Q - 6577327, $ML_DSA_Q - 1723600, $ML_DSA_Q - 7953734
    .long $ML_DSA_Q - 472078,  $ML_DSA_Q - 1717735, $ML_DSA_Q - 7404533, $ML_DSA_Q - 2213111, $ML_DSA_Q - 269760,  $ML_DSA_Q - 3866901, $ML_DSA_Q - 3523897, $ML_DSA_Q - 5341501
    .long $ML_DSA_Q - 6581310, $ML_DSA_Q - 4686184, $ML_DSA_Q - 1652634, $ML_DSA_Q - 810149,  $ML_DSA_Q - 3014001, $ML_DSA_Q - 1616392, $ML_DSA_Q - 162844,  $ML_DSA_Q - 5196991
    .long $ML_DSA_Q - 7173032, $ML_DSA_Q - 185531,  $ML_DSA_Q - 3369112, $ML_DSA_Q - 1957272, $ML_DSA_Q - 8215696, $ML_DSA_Q - 2454455, $ML_DSA_Q - 2432395, $ML_DSA_Q - 6366809
    .long $ML_DSA_Q - 4603424, $ML_DSA_Q - 594136,  $ML_DSA_Q - 4656147, $ML_DSA_Q - 5796124, $ML_DSA_Q - 6533464, $ML_DSA_Q - 6709241, $ML_DSA_Q - 5548557, $ML_DSA_Q - 7838005
    .long $ML_DSA_Q - 3406031, $ML_DSA_Q - 2235880, $ML_DSA_Q - 777191,  $ML_DSA_Q - 1500165, $ML_DSA_Q - 7005614, $ML_DSA_Q - 5834105, $ML_DSA_Q - 1917081, $ML_DSA_Q - 7100756
    .long $ML_DSA_Q - 6417775, $ML_DSA_Q - 3306115, $ML_DSA_Q - 1312455, $ML_DSA_Q - 7929317, $ML_DSA_Q - 6950192, $ML_DSA_Q - 5062207, $ML_DSA_Q - 1237275, $ML_DSA_Q - 7047359
    .long $ML_DSA_Q - 7329447, $ML_DSA_Q - 1903435, $ML_DSA_Q - 1869119, $ML_DSA_Q - 5386378, $ML_DSA_Q - 4832145, $ML_DSA_Q - 2635921, $ML_DSA_Q - 1250494, $ML_DSA_Q - 4613401
    .long $ML_DSA_Q - 1595974, $ML_DSA_Q - 2486353, $ML_DSA_Q - 1247620, $ML_DSA_Q - 4055324, $ML_DSA_Q - 1265009, $ML_DSA_Q - 5790267, $ML_DSA_Q - 2691481, $ML_DSA_Q - 2842341
    .long $ML_DSA_Q - 203044,  $ML_DSA_Q - 1735879, $ML_DSA_Q - 5038140, $ML_DSA_Q - 3437287, $ML_DSA_Q - 4108315, $ML_DSA_Q - 5942594, $ML_DSA_Q - 286988,  $ML_DSA_Q - 342297
    .long $ML_DSA_Q - 4784579, $ML_DSA_Q - 7611795, $ML_DSA_Q - 7855319, $ML_DSA_Q - 4823422, $ML_DSA_Q - 3207046, $ML_DSA_Q - 2031748, $ML_DSA_Q - 5257975, $ML_DSA_Q - 7725090
    .long $ML_DSA_Q - 7857917, $ML_DSA_Q - 8337157, $ML_DSA_Q - 6767243, $ML_DSA_Q - 495491,  $ML_DSA_Q - 819034,  $ML_DSA_Q - 909542,  $ML_DSA_Q - 1859098, $ML_DSA_Q - 900702
    .long $ML_DSA_Q - 5187039, $ML_DSA_Q - 7183191, $ML_DSA_Q - 4621053, $ML_DSA_Q - 4860065, $ML_DSA_Q - 3513181, $ML_DSA_Q - 7144689, $ML_DSA_Q - 2434439, $ML_DSA_Q - 266997
    .long $ML_DSA_Q - 4817955, $ML_DSA_Q - 5933984, $ML_DSA_Q - 2244091, $ML_DSA_Q - 5037939, $ML_DSA_Q - 3817976, $ML_DSA_Q - 2316500, $ML_DSA_Q - 3407706, $ML_DSA_Q - 2091667
    .long $ML_DSA_Q - 3839961, $ML_DSA_Q - 4751448, $ML_DSA_Q - 4499357, $ML_DSA_Q - 5361315, $ML_DSA_Q - 6940675, $ML_DSA_Q - 7567685, $ML_DSA_Q - 6795489, $ML_DSA_Q - 1285669
    .long $ML_DSA_Q - 1341330, $ML_DSA_Q - 1315589, $ML_DSA_Q - 8202977, $ML_DSA_Q - 5971092, $ML_DSA_Q - 6529015, $ML_DSA_Q - 3159746, $ML_DSA_Q - 4827145, $ML_DSA_Q - 189548
    .long $ML_DSA_Q - 7063561, $ML_DSA_Q - 759969,  $ML_DSA_Q - 8169440, $ML_DSA_Q - 2389356, $ML_DSA_Q - 5130689, $ML_DSA_Q - 1653064, $ML_DSA_Q - 8371839, $ML_DSA_Q - 4656075
    .long $ML_DSA_Q - 3958618, $ML_DSA_Q - 904516,  $ML_DSA_Q - 7280319, $ML_DSA_Q - 44288,   $ML_DSA_Q - 3097992, $ML_DSA_Q - 508951,  $ML_DSA_Q - 264944,  $ML_DSA_Q - 5037034
    .long $ML_DSA_Q - 6949987, $ML_DSA_Q - 1852771, $ML_DSA_Q - 1349076, $ML_DSA_Q - 7998430, $ML_DSA_Q - 7072248, $ML_DSA_Q - 8357436, $ML_DSA_Q - 7151892, $ML_DSA_Q - 7709315
    .long $ML_DSA_Q - 5903370, $ML_DSA_Q - 7969390, $ML_DSA_Q - 4686924, $ML_DSA_Q - 5412772, $ML_DSA_Q - 2715295, $ML_DSA_Q - 2147896, $ML_DSA_Q - 7396998, $ML_DSA_Q - 3412210
    .long $ML_DSA_Q - 126922,  $ML_DSA_Q - 4747489, $ML_DSA_Q - 5223087, $ML_DSA_Q - 5190273, $ML_DSA_Q - 7380215, $ML_DSA_Q - 4296819, $ML_DSA_Q - 1939314, $ML_DSA_Q - 7122806
    .long $ML_DSA_Q - 6795196, $ML_DSA_Q - 2176455, $ML_DSA_Q - 3475950, $ML_DSA_Q - 6927966, $ML_DSA_Q - 5339162, $ML_DSA_Q - 4702672, $ML_DSA_Q - 6851714, $ML_DSA_Q - 4450022
    .long $ML_DSA_Q - 5582638, $ML_DSA_Q - 2071892, $ML_DSA_Q - 5823537, $ML_DSA_Q - 3900724, $ML_DSA_Q - 3881043, $ML_DSA_Q - 954230,  $ML_DSA_Q - 531354,  $ML_DSA_Q - 811944
    .long $ML_DSA_Q - 3699596, $ML_DSA_Q - 6779997, $ML_DSA_Q - 6239768, $ML_DSA_Q - 3507263, $ML_DSA_Q - 4558682, $ML_DSA_Q - 3505694, $ML_DSA_Q - 6736599, $ML_DSA_Q - 6681150
    .long $ML_DSA_Q - 7841118, $ML_DSA_Q - 2348700, $ML_DSA_Q - 8079950, $ML_DSA_Q - 3539968, $ML_DSA_Q - 5512770, $ML_DSA_Q - 3574422, $ML_DSA_Q - 5336701, $ML_DSA_Q - 4519302
    .long $ML_DSA_Q - 3915439, $ML_DSA_Q - 5842901, $ML_DSA_Q - 4788269, $ML_DSA_Q - 6718724, $ML_DSA_Q - 3530437, $ML_DSA_Q - 3077325, $ML_DSA_Q - 95776,   $ML_DSA_Q - 2706023
    .long $ML_DSA_Q - 280005,  $ML_DSA_Q - 4010497, $ML_DSA_Q - 8360995, $ML_DSA_Q - 1757237, $ML_DSA_Q - 5102745, $ML_DSA_Q - 6980856, $ML_DSA_Q - 4520680, $ML_DSA_Q - 6262231
    .long $ML_DSA_Q - 6271868, $ML_DSA_Q - 2619752, $ML_DSA_Q - 7260833, $ML_DSA_Q - 7830929, $ML_DSA_Q - 3585928, $ML_DSA_Q - 7300517, $ML_DSA_Q - 1024112, $ML_DSA_Q - 2725464
    .long $ML_DSA_Q - 2680103, $ML_DSA_Q - 3111497, $ML_DSA_Q - 5495562, $ML_DSA_Q - 3119733, $ML_DSA_Q - 6288512, $ML_DSA_Q - 8021166, $ML_DSA_Q - 2353451, $ML_DSA_Q - 1826347
    .long $ML_DSA_Q - 466468,  $ML_DSA_Q - 7504169, $ML_DSA_Q - 7602457, $ML_DSA_Q - 237124,  $ML_DSA_Q - 7861508, $ML_DSA_Q - 5771523, $ML_DSA_Q - 25847,   $ML_DSA_Q - 4193792

.align 32
idx_even:
    .long 0,2,4,6, 0,2,4,6

.align 32
idx_odd:
    .long 1,3,5,7, 1,3,5,7

# Modulus Q for ML-DSA NTT (2^23 - 2^13 + 1)
.align 8
ml_dsa_q:
    .quad $ML_DSA_Q

# -Q^{-1} mod 2^32 (Montgomery parameter for ML-DSA modular reduction)
.align 8
ml_dsa_q_neg_inv:
    .quad $ML_DSA_Q_NEG_INV

# (N^{-1} mod Q) in Montgomery form for scaling after inverse NTT
.align 8
ml_dsa_inverse_degree_montgomery:
    .quad $inverse_degree_montgomery

###############################################################################
###############################################################################
### Code section

.text

###############################################################################
# ml_dsa_poly_ntt_mult_avx2
#
# C Prototype:
#   void ml_dsa_poly_ntt_mult_avx2(
#       const uint32_t *a,       // (rdi) Input polynomial A (in NTT domain)
#       const uint32_t *b,       // (rsi) Input polynomial B (in NTT domain)
#       uint32_t *out,           // (rdx) Output polynomial (result)
#   );
#
# Description:
#   Top-level routine for polynomial multiplication in ML-DSA,
#   using number-theoretic transform (NTT) methods. This function performs
#   multiplication of two polynomials in the NTT domain, making full use of
#   AVX2 vector instructions.
#   It assumes there are 256 coefficients.
#
#   out[i] = a[i] x b[i] mod Q
#
#   The function:
#     - Takes pointers to source polynomials (NTT domain) and destination buffer
#     - Performs element-wise modular (pointwise) multiplication in the NTT domain
#     - Applies Montgomery reduction for efficient modular arithmetic
#
# Inputs:
#   a   - First input polynomial, NTT domain
#   b   - Second input polynomial, NTT domain
#   out - Destination buffer for output coefficients
#
# Output:
#   - Output buffer 'out' contains the coefficient-wise modular product of
#     the input polynomials (still in NTT domain)
#
###############################################################################

.globl  ml_dsa_poly_ntt_mult_avx2
.type   ml_dsa_poly_ntt_mult_avx2,\@function,3
.align 32
ml_dsa_poly_ntt_mult_avx2:
.cfi_startproc
___
$code .= <<___ if ($win64);
    lea     -168(%rax), %rsp
    vmovaps %xmm6,   0(%rsp)
    vmovaps %xmm7,   16(%rsp)
    vmovaps %xmm8,   32(%rsp)
    vmovaps %xmm9,   48(%rsp)
    vmovaps %xmm10,  64(%rsp)
    vmovaps %xmm11,  80(%rsp)
    vmovaps %xmm12,  96(%rsp)
    vmovaps %xmm13,  112(%rsp)
    vmovaps %xmm14,  128(%rsp)
    vmovaps %xmm15,  144(%rsp)
___
$code .= <<___;
.Lntt_mult_body:
    vpbroadcastq ml_dsa_q_neg_inv(%rip), %ymm14
    vpbroadcastd ml_dsa_q(%rip), %ymm15
    xor %r10d, %r10d

.align 32
.Lmult_loop:
    # Load a and b into ymm registers
    vmovdqu (%rdi,%r10), %ymm0   # a[0:7]
    vmovdqu (%rsi,%r10), %ymm1   # b[0:7]

    # multiply this part of input data
___

    &multiply_mod_Q("%ymm0", "%ymm1", "%ymm0",  # A, B, out (AxB)
                    "%ymm8", "%ymm9", "%ymm10", # tmp
                    "%ymm14", "%ymm15", 0);     # qinv, q, bcast32

$code .= <<___;
    # store result to output
    vmovdqu %ymm0, (%rdx,%r10)

    # start new iteration
    add \$8*4, %r10d
    cmp \$256*4, %r10d
    jb .Lmult_loop

    # clear and restore registers
    vzeroall
___
$code .= <<___ if ($win64);
    vmovaps 0(%rsp),   %xmm6
    vmovaps 16(%rsp),  %xmm7
    vmovaps 32(%rsp),  %xmm8
    vmovaps 48(%rsp),  %xmm9
    vmovaps 64(%rsp),  %xmm10
    vmovaps 80(%rsp),  %xmm11
    vmovaps 96(%rsp),  %xmm12
    vmovaps 112(%rsp), %xmm13
    vmovaps 128(%rsp), %xmm14
    vmovaps 144(%rsp), %xmm15
    lea     (%rax), %rsp
___
$code .= <<___;
.Lntt_mult_epilogue:
    ret
.cfi_endproc
.size   ml_dsa_poly_ntt_mult_avx2, .-ml_dsa_poly_ntt_mult_avx2

###############################################################################
# ml_dsa_poly_ntt_avx2
#
# C Prototype:
#   void ml_dsa_poly_ntt_avx2(
#       uint32_t *p_coeffs,     // Pointer to coefficients (input: normal domain, output: NTT domain)
#       const uint32_t *p_zetas // Pointer to zeta (twiddle factor) table for the forward NTT
#   );
#
# Description:
#   Top-level implementation of the forward Number Theoretic
#   Transform (NTT) for ML-DSA polynomials. This function converts a polynomial
#   from its standard coefficient (normal) form to its NTT representation,
#   storing the result in-place in the provided coefficients array. The function
#   uses stage-specific "zeta" (twiddle factor) tables passed from C (ml_dsa_ntt.c).
#
#   The function:
#     - Takes a buffer of polynomial coefficients in normal (standard) order
#     - Uses the provided zeta table for all twiddle-factor multiplications
#     - Processes the NTT in a breadth-first, layered fashion with AVX2 SIMD
#     - Overwrites the input buffer with its NTT-domain representation
#
# Inputs:
#   p_coeffs - Pointer to the coefficient array (will be overwritten in-place by the NTT result)
#   p_zetas  - Pointer to the precomputed table of forward NTT zeta (twiddle) factors (from ml_dsa_ntt.c)
#
# Output:
#   - The 'p_coeffs' array is updated in-place with the corresponding NTT-domain representation.
###############################################################################
.globl  ml_dsa_poly_ntt_avx2
.type   ml_dsa_poly_ntt_avx2,\@function,2
.align 32
ml_dsa_poly_ntt_avx2:
.cfi_startproc
___
$code .= <<___ if ($win64);
    lea     -168(%rax), %rsp
    vmovaps %xmm6,   0(%rsp)
    vmovaps %xmm7,   16(%rsp)
    vmovaps %xmm8,   32(%rsp)
    vmovaps %xmm9,   48(%rsp)
    vmovaps %xmm10,  64(%rsp)
    vmovaps %xmm11,  80(%rsp)
    vmovaps %xmm12,  96(%rsp)
    vmovaps %xmm13,  112(%rsp)
    vmovaps %xmm14,  128(%rsp)
    vmovaps %xmm15,  144(%rsp)
___
$code .= <<___;
.Lntt_body:

    # move p_zetas to r11
    mov %rsi, %r11

    # load constants
    vpbroadcastq ml_dsa_q_neg_inv(%rip), %ymm14
    vpbroadcastd ml_dsa_q(%rip), %ymm15     # 32-bit Q

    # ==============================================================
    # - level 0: offset = 128, step = 1, zeta indexes = 1
    # - level 1: offset = 64, step = 2, zeta indexes = 2, 3
    # - level 2: offset = 32, step = 4, zeta indexes = 4, 5, 6, 7
    # p_coeffs already in rdi
___

    &ntt_levels0to2(0*4);
    &ntt_levels0to2(8*4);
    &ntt_levels0to2(16*4);
    &ntt_levels0to2(24*4);

$code .= <<___;

    # ==============================================================
    # - level 3: offset = 16, step = 8
    #     zeta indexes = 8, 9, 10, 11, 12, 13, 14, 15
    # - level 4: offset = 8, step = 16
    #     zeta indexes = 16, 17, 18, ..., 30, 31
    # - level 5: offset = 4, step = 32
    #     zeta indexes = 32, 33, 34, ..., 62, 63
    # - level 6: offset = 2, step = 64
    #     zeta indexes = 64, 65, 66, ..., 126, 127
    # - level 7: offset = 1, step = 128
    #     zeta indexes = 128, 129, 130, ..., 254, 255
    # p_coeffs already in rdi
___

    # arguments:    coeff,   l3,    l4,    l5,    l6,    l7
    &ntt_levels3to7(  0*4,  8*4,  16*4,  32*4,  64*4, 128*4);
    &ntt_levels3to7( 64*4, 10*4,  20*4,  40*4,  80*4, 160*4);
    &ntt_levels3to7(128*4, 12*4,  24*4,  48*4,  96*4, 192*4);
    &ntt_levels3to7(192*4, 14*4,  28*4,  56*4, 112*4, 224*4);

$code .= <<___;

    # clear and restore registers
    vzeroall
___
$code .= <<___ if ($win64);
    vmovaps 0(%rsp),   %xmm6
    vmovaps 16(%rsp),  %xmm7
    vmovaps 32(%rsp),  %xmm8
    vmovaps 48(%rsp),  %xmm9
    vmovaps 64(%rsp),  %xmm10
    vmovaps 80(%rsp),  %xmm11
    vmovaps 96(%rsp),  %xmm12
    vmovaps 112(%rsp), %xmm13
    vmovaps 128(%rsp), %xmm14
    vmovaps 144(%rsp), %xmm15
    lea     (%rax), %rsp
___
$code .= <<___;
.Lntt_epilogue:
    ret
.cfi_endproc
.size   ml_dsa_poly_ntt_avx2, .-ml_dsa_poly_ntt_avx2

###############################################################################
# ml_dsa_poly_ntt_inverse_avx2
#
# C Prototype:
#     void ml_dsa_poly_ntt_inverse_avx2(
#        uint32_t *p_coeffs // (rdi) Pointer to coefficients
#                           // input: NTT domain, output: normal domain, in-place
#     );
#
# Description:
#   Top-level implementation of the inverse Number Theoretic
#   Transform (INTT) for ML-DSA polynomial. This function converts a polynomial
#   from its NTT domain back to the standard coefficient (normal) domain,
#   storing the result in-place in the provided buffer. The required inverse zeta
#   (twiddle) factors are managed internally.
#
#   The function:
#     - Accepts a buffer of NTT-domain coefficients
#     - Overwrites the input buffer with the result in the normal (coefficient) domain
#
# Inputs:
#   p_coeffs - Pointer to the polynomial coefficient array (in-place transform)
#
# Output:
#   - The 'p_coeffs' array is updated in-place to contain the standard domain polynomial.
#   - Uses 'zetas_inverse' table
###############################################################################
.globl  ml_dsa_poly_ntt_inverse_avx2
.type   ml_dsa_poly_ntt_inverse_avx2,\@function,1
.align 32
ml_dsa_poly_ntt_inverse_avx2:
.cfi_startproc
___
$code .= <<___ if ($win64);
    lea     -168(%rax), %rsp
    vmovaps %xmm6,   0(%rsp)
    vmovaps %xmm7,   16(%rsp)
    vmovaps %xmm8,   32(%rsp)
    vmovaps %xmm9,   48(%rsp)
    vmovaps %xmm10,  64(%rsp)
    vmovaps %xmm11,  80(%rsp)
    vmovaps %xmm12,  96(%rsp)
    vmovaps %xmm13,  112(%rsp)
    vmovaps %xmm14,  128(%rsp)
    vmovaps %xmm15,  144(%rsp)
___
$code .= <<___;
.Lintt_body:
    lea zetas_inverse(%rip), %r11

    vpbroadcastq ml_dsa_q_neg_inv(%rip), %ymm14
    vpbroadcastd ml_dsa_q(%rip), %ymm15

    # ==============================================================
    # - level 0: offset = 1, step = 128
    #     zeta indexes (original table) = 255, 254, 253, ... 129, 128
    #     zeta indexes (new table) = 0, 1, 2, .. 127
    # - level 1: offset = 2, step = 64
    #     zeta indexes (original table) = 127, 126, 125, ... 65, 64
    #     zeta indexes (new table) = 128, 129, .. 191
    # - level 2: offset = 4, step = 32
    #     zeta indexes (original table) = 63, 62, 61, ... 33, 32
    #     zeta indexes (new table) = 192, 193, 194, ... 223
    # - level 3: offset = 8, step = 16
    #     zeta indexes (original table) = 31, 30, 29, ... 17, 16
    #     zeta indexes (new table) = 224, 225, 226, ... 239
    # - level 4: offset = 16, step = 8
    #     zeta indexes (original table) = 15, 14, 13, ..., 9, 8
    #     zeta indexes (new table) = 240, 241, 242, ... 247

___

    #  arguments:    coeff,   l0,    l1,    l2,    l3,    l4
    &intt_levels0to4(0*4,    0*4, 128*4, 192*4, 224*4, 240*4);
    &intt_levels0to4(64*4,  32*4, 144*4, 200*4, 228*4, 242*4);
    &intt_levels0to4(128*4, 64*4, 160*4, 208*4, 232*4, 244*4);
    &intt_levels0to4(192*4, 96*4, 176*4, 216*4, 236*4, 246*4);

$code .= <<___;

    # ==============================================================
    # - level 5: offset = 32, step = 4
    #   zeta indexes (original table) = 7, 6, 5, 4
    #   zeta indexes (new table) = 248, 249, 250, 251
    # - level 6: offset = 64, step = 2
    #   zeta indexes (original table) = 3, 2
    #   zeta indexes (new table) = 252, 253
    # - level 7: offset = 128, step = 1
    #   zeta indexes (original table) = 1
    #   zeta indexes (new table) = 254
___
    &intt_levels5to7(0*4);
    &intt_levels5to7(8*4);
    &intt_levels5to7(16*4);
    &intt_levels5to7(24*4);
    $code .= <<___;

    # clear and restore registers
    vzeroall
___
$code .= <<___ if ($win64);
    vmovaps 0(%rsp),   %xmm6
    vmovaps 16(%rsp),  %xmm7
    vmovaps 32(%rsp),  %xmm8
    vmovaps 48(%rsp),  %xmm9
    vmovaps 64(%rsp),  %xmm10
    vmovaps 80(%rsp),  %xmm11
    vmovaps 96(%rsp),  %xmm12
    vmovaps 112(%rsp), %xmm13
    vmovaps 128(%rsp), %xmm14
    vmovaps 144(%rsp), %xmm15
    lea     (%rax), %rsp
___
$code .= <<___;
.Lintt_epilogue:
    ret
.cfi_endproc
.size   ml_dsa_poly_ntt_inverse_avx2, .-ml_dsa_poly_ntt_inverse_avx2
___

# Windows SEH exception handler and unwind data
if ($win64) {
my $context = "%r8";
my $disp    = "%r9";

$code .= <<___;
.extern __imp_RtlVirtualUnwind
.type   ntt_se_handler,\@abi-omnipotent
.align  16
ntt_se_handler:
    push    %rsi
    push    %rdi
    push    %rbx
    push    %rbp
    push    %r12
    push    %r13
    push    %r14
    push    %r15
    pushfq
    sub     \$64, %rsp

    mov     120($context), %rax     # context->Rax = original %rsp (saved by xlate preamble)
    mov     248($context), %rbx     # context->Rip

    mov     8($disp), %rsi          # disp->ImageBase
    mov     56($disp), %r11         # disp->HandlerData

    mov     0(%r11), %r10d          # HandlerData[0]: body label (rva)
    lea     (%rsi,%r10), %r10
    cmp     %r10, %rbx              # Rip < body?
    jb      .Lntt_in_prologue

    mov     4(%r11), %r10d          # HandlerData[1]: epilogue label (rva)
    lea     (%rsi,%r10), %r10
    cmp     %r10, %rbx              # Rip >= epilogue?
    jae     .Lntt_in_prologue

    # In function body: XMM6-XMM15 are saved at 0..144(new_rsp).
    # context->Rsp = new_rsp = rax - 168
    mov     152($context), %rsi     # context->Rsp = new_rsp (address of XMM saves)
    lea     512($context), %rdi     # &context->Xmm6
    mov     \$20, %ecx              # 10 XMMs * 2 qwords = 20 qwords
    .long   0xa548f3fc              # cld; rep movsq

.Lntt_in_prologue:
    # Restore rdi and rsi saved by xlate preamble in shadow space
    mov     8(%rax), %rcx
    mov     16(%rax), %rdx
    mov     %rcx, 176($context)     # context->Rdi
    mov     %rdx, 168($context)     # context->Rsi
    mov     %rax, 152($context)     # context->Rsp = original %rsp

    mov     40($disp), %rdi         # disp->ContextRecord
    mov     $context, %rsi
    mov     \$154, %ecx             # sizeof(CONTEXT)/8
    .long   0xa548f3fc              # cld; rep movsq

    mov     $disp, %rsi
    xor     %rcx, %rcx              # UNW_FLAG_NHANDLER
    mov     8(%rsi), %rdx           # disp->ImageBase
    mov     0(%rsi), %r8            # disp->ControlPc
    mov     16(%rsi), %r9           # disp->FunctionEntry
    mov     40(%rsi), %r10          # disp->ContextRecord
    lea     56(%rsi), %r11          # &disp->HandlerData
    lea     24(%rsi), %r12          # &disp->EstablisherFrame
    mov     %r10, 32(%rsp)
    mov     %r11, 40(%rsp)
    mov     %r12, 48(%rsp)
    mov     %rcx, 56(%rsp)
    call    *__imp_RtlVirtualUnwind(%rip)

    mov     \$1, %eax               # ExceptionContinueSearch
    add     \$64, %rsp
    popfq
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbp
    pop     %rbx
    pop     %rdi
    pop     %rsi
    ret
.size   ntt_se_handler,.-ntt_se_handler

.section    .pdata
.align  4
    .rva    .LSEH_begin_ml_dsa_poly_ntt_mult_avx2
    .rva    .LSEH_end_ml_dsa_poly_ntt_mult_avx2
    .rva    .LSEH_info_ml_dsa_poly_ntt_mult_avx2
    .rva    .LSEH_begin_ml_dsa_poly_ntt_avx2
    .rva    .LSEH_end_ml_dsa_poly_ntt_avx2
    .rva    .LSEH_info_ml_dsa_poly_ntt_avx2
    .rva    .LSEH_begin_ml_dsa_poly_ntt_inverse_avx2
    .rva    .LSEH_end_ml_dsa_poly_ntt_inverse_avx2
    .rva    .LSEH_info_ml_dsa_poly_ntt_inverse_avx2

.section    .xdata
.align  8
.LSEH_info_ml_dsa_poly_ntt_mult_avx2:
    .byte   9,0,0,0
    .rva    ntt_se_handler
    .rva    .Lntt_mult_body,.Lntt_mult_epilogue
.LSEH_info_ml_dsa_poly_ntt_avx2:
    .byte   9,0,0,0
    .rva    ntt_se_handler
    .rva    .Lntt_body,.Lntt_epilogue
.LSEH_info_ml_dsa_poly_ntt_inverse_avx2:
    .byte   9,0,0,0
    .rva    ntt_se_handler
    .rva    .Lintt_body,.Lintt_epilogue
___
}

}}} else {{{
# When AVX2 is not available, output stub functions
# The capable function returns 0, and the operation functions trap if called
$code .= <<___;
.text

.globl  ml_dsa_ntt_avx2_capable
.type   ml_dsa_ntt_avx2_capable,\@abi-omnipotent
ml_dsa_ntt_avx2_capable:
    xor     %eax, %eax
    ret
.size   ml_dsa_ntt_avx2_capable, .-ml_dsa_ntt_avx2_capable

.globl  ml_dsa_poly_ntt_mult_avx2
.globl  ml_dsa_poly_ntt_avx2
.globl  ml_dsa_poly_ntt_inverse_avx2
.type   ml_dsa_poly_ntt_mult_avx2,\@abi-omnipotent
ml_dsa_poly_ntt_mult_avx2:
ml_dsa_poly_ntt_avx2:
ml_dsa_poly_ntt_inverse_avx2:
    .byte   0x0f,0x0b       # ud2
    ret
.size   ml_dsa_poly_ntt_mult_avx2, .-ml_dsa_poly_ntt_mult_avx2
___
}}}

print $code;
close STDOUT or die "error closing STDOUT: $!";
