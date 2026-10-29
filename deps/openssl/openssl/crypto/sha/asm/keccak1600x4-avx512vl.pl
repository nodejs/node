#!/usr/bin/env perl
#
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2026 Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

###############################################################################
# Keccak x4 AVX512VL SHA3/SHAKE Assembly Routines
#
# Description:
#   This file emits x86_64 assembly for AVX512VL accelerated Keccak-f[1600]
#   processing of 4 independent states in parallel ("x4").
#
#   It provides the core 24-round Keccak permutation and x4 helper routines
#   used by SHA3 and SHAKE absorb/finalize/squeeze paths. Data from four
#   input/output lanes is packed across YMM registers so lane-local operations
#   execute in SIMD.
#
###############################################################################

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$avx512vl = 0;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

# Check for AVX512VL support in assembler
if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1` =~ /GNU assembler version (\d+)\.(\d+)/) {
  my ($gas_major, $gas_minor) = ($1, $2);
  $avx512vl = ($gas_major > 2 || ($gas_major == 2 && $gas_minor >= 26));
}

if (!$avx512vl
  && $win64
  && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/)
  && `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/)
{
  $avx512vl = ($1 >= 2.12);
}

if (!$avx512vl && `$ENV{CC} -v 2>&1` =~ /((?:clang|LLVM) version|.*based on LLVM) ([0-9]+\.[0-9]+)/) {
    $avx512vl = ($2>=3.9);
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

$arg1="%rdi";
$arg2="%rsi";
$arg3="%rdx";
$arg4="%rcx";
$arg5="%r8";
$arg6="%r9";
$roundn="%r13d";
$tblptr="%r14";

# Define SHAKE rates
$SHAKE128_RATE="\$168";
$SHAKE256_RATE="\$136";

# Stack frame offsets for SHAKE x4 wrapper functions
$STATE_SIZE="808";    # (25 * 8 * 4) + 8 = 808 bytes
$sf_arg1="0";
$sf_arg2="8";
$sf_arg3="16";
$sf_arg4="24";
$sf_arg5="32";
$sf_state_ptr="40";
$sf_state_x4="48";
$sf_size="856";       # 48 + 808 = 856 bytes

# Emit an internal helper call used by one-shot wrappers.
# - Win64: call the provided *_internal shim and bracket it with 32-byte
#   shadow space so shim entry can use xlate-compatible [rsp+8]/[rsp+16].
# - non-Win64: call the local function entry label (.L_<base name>), which
#   sits at the same address as the public symbol.  Calling the public
#   global symbol by name here would break Mach-O builds: the call textually
#   precedes the symbol's .globl declaration, so x86_64-xlate.pl never gets
#   a chance to prepend the platform's leading-underscore, leaving an
#   undefined reference to the un-decorated name.
# The argument must be the shim/internal symbol name, e.g.
#   SHA3_shake128_x4_inc_squeeze_avx512vl_internal
sub call_internal {
    my ($shim_name) = @_;
    my $external_name = $shim_name;

    $external_name =~ s/_internal$//;

    return <<___ if ($win64);
    sub     \$32, %rsp
    call    $shim_name
    add     \$32, %rsp
___

    return <<___;
    call    .L_$external_name
___
}

if ($avx512vl>0) {{{

my $avx512_mask = (1<<31)|(1<<30)|(1<<17)|(1<<16);  # AVX512VL|BW|DQ|F

$code .= <<___;
.text

.extern OPENSSL_ia32cap_P

.globl  SHA3_avx512vl_capable
.type   SHA3_avx512vl_capable,\@abi-omnipotent
.align 32
SHA3_avx512vl_capable:
    mov     OPENSSL_ia32cap_P+8(%rip), %ecx
    xor     %eax, %eax
    # 1<<31|1<<30|1<<17|1<<16: AVX512VL|AVX512BW|AVX512DQ|AVX512F
    and     \$$avx512_mask, %ecx
    cmp     \$$avx512_mask, %ecx
    cmove   %ecx, %eax
    ret
.size   SHA3_avx512vl_capable, .-SHA3_avx512vl_capable
___

$code.=<<___;
.text

# Perform Keccak permutation
#
# YMM registers 0 to 24 are used as Keccak state registers.
# This function, as is, can work on 1 to 4 independent states at the same time.
#
# There is no clear boundary between Theta, Rho, Pi, Chi and Iota steps.
# Instructions corresponding to these steps overlap for better efficiency.
#
# Arguments:
# ymm0-ymm24    [in/out]    Keccak state registers (one SIMD per one state register)
# ymm25-ymm31   [clobbered] temporary SIMD registers
# $roundn       [clobbered] used for round tracking
# $tblptr       [clobbered] used for access to SHA3 constant table
.type keccak_1600_permute,\@abi-omnipotent
.align  32
keccak_1600_permute:
.cfi_startproc
    mov     \$24, $roundn        # 24 rounds
    lea     iotas(%rip), $tblptr # Load the address of the SHA3 round constants

.align  32
.Lkeccak_rnd_loop:
    # Theta step

    # Compute column parities
    # C[5] = [0, 0, 0, 0, 0]
    # for x in 0 to 4:
    #     C[x] = state[x][0] XOR state[x][1] XOR state[x][2] XOR state[x][3] XOR state[x][4]

    vmovdqa64   %ymm0, %ymm25
    vpternlogq  \$0x96, %ymm5, %ymm10, %ymm25
    vmovdqa64   %ymm1, %ymm26
    vpternlogq  \$0x96, %ymm11, %ymm6, %ymm26
    vmovdqa64   %ymm2, %ymm27
    vpternlogq  \$0x96, %ymm12, %ymm7, %ymm27

    vmovdqa64   %ymm3, %ymm28
    vpternlogq  \$0x96, %ymm13, %ymm8, %ymm28
    vmovdqa64   %ymm4, %ymm29
    vpternlogq  \$0x96, %ymm14, %ymm9, %ymm29
    vpternlogq  \$0x96, %ymm20, %ymm15, %ymm25

    vpternlogq  \$0x96, %ymm21, %ymm16, %ymm26
    vpternlogq  \$0x96, %ymm22, %ymm17, %ymm27
    vpternlogq  \$0x96, %ymm23, %ymm18, %ymm28

    # Start computing D values and keep computing column parity
    # D[5] = [0, 0, 0, 0, 0]
    # for x in 0 to 4:
    #     D[x] = C[(x+4) mod 5] XOR ROTATE_LEFT(C[(x+1) mod 5], 1)

    vprolq      \$1, %ymm26, %ymm30
    vprolq      \$1, %ymm27, %ymm31
    vpternlogq  \$0x96, %ymm24, %ymm19, %ymm29

    # Continue computing D values and apply Theta
    # for x in 0 to 4:
    #     for y in 0 to 4:
    #         state[x][y] = state[x][y] XOR D[x]

    vpternlogq  \$0x96, %ymm30, %ymm29, %ymm0
    vpternlogq  \$0x96, %ymm30, %ymm29, %ymm10
    vpternlogq  \$0x96, %ymm30, %ymm29, %ymm20

    vpternlogq  \$0x96, %ymm30, %ymm29, %ymm5
    vpternlogq  \$0x96, %ymm30, %ymm29, %ymm15
    vprolq      \$1, %ymm28, %ymm30

    vpternlogq  \$0x96, %ymm31, %ymm25, %ymm6
    vpternlogq  \$0x96, %ymm31, %ymm25, %ymm16
    vpternlogq  \$0x96, %ymm31, %ymm25, %ymm1

    vpternlogq  \$0x96, %ymm31, %ymm25, %ymm11
    vpternlogq  \$0x96, %ymm31, %ymm25, %ymm21
    vprolq      \$1, %ymm29, %ymm31

    vpbroadcastq    ($tblptr), %ymm29 # Load the round constant into ymm29 (Iota)
    add         \$8, $tblptr          # Increment the pointer to the next round constant

    vpternlogq  \$0x96, %ymm30, %ymm26, %ymm12
    vpternlogq  \$0x96, %ymm30, %ymm26, %ymm7
    vpternlogq  \$0x96, %ymm30, %ymm26, %ymm22

    vpternlogq  \$0x96, %ymm30, %ymm26, %ymm17
    vpternlogq  \$0x96, %ymm30, %ymm26, %ymm2
    vprolq      \$1, %ymm25, %ymm30

    # Rho step
    # Keep applying Theta and start Rho step
    #
    # ROTATION_OFFSETS[5][5] = [
    #     [0, 1, 62, 28, 27],
    #     [36, 44, 6, 55, 20],
    #     [3, 10, 43, 25, 39],
    #     [41, 45, 15, 21, 8],
    #     [18, 2, 61, 56, 14] ]
    #
    # for x in 0 to 4:
    #     for y in 0 to 4:
    #         state[x][y] = ROTATE_LEFT(state[x][y], ROTATION_OFFSETS[x][y])

    vpternlogq  \$0x96, %ymm31, %ymm27, %ymm3
    vpternlogq  \$0x96, %ymm31, %ymm27, %ymm13
    vpternlogq  \$0x96, %ymm31, %ymm27, %ymm23

    vprolq      \$44, %ymm6, %ymm6
    vpternlogq  \$0x96, %ymm31, %ymm27, %ymm18
    vpternlogq  \$0x96, %ymm31, %ymm27, %ymm8

    vprolq      \$43, %ymm12, %ymm12
    vprolq      \$21, %ymm18, %ymm18
    vpternlogq  \$0x96, %ymm30, %ymm28, %ymm24

    vprolq      \$14, %ymm24, %ymm24
    vprolq      \$28, %ymm3, %ymm3
    vpternlogq  \$0x96, %ymm30, %ymm28, %ymm9

    vprolq      \$20, %ymm9, %ymm9
    vprolq      \$3, %ymm10, %ymm10
    vpternlogq  \$0x96, %ymm30, %ymm28, %ymm19

    vprolq      \$45, %ymm16, %ymm16
    vprolq      \$61, %ymm22, %ymm22
    vpternlogq  \$0x96, %ymm30, %ymm28, %ymm4

    vprolq      \$1, %ymm1, %ymm1
    vprolq      \$6, %ymm7, %ymm7
    vpternlogq  \$0x96, %ymm30, %ymm28, %ymm14

    # Continue with Rho and start Pi and Chi steps at the same time
    # Ternary logic 0xD2 is used for Chi step
    #
    # for x in 0 to 4:
    #     for y in 0 to 4:
    #         state[x][y] = state[x][y] XOR ((NOT state[(x+1) mod 5][y]) AND state[(x+2) mod 5][y])

    vprolq      \$25, %ymm13, %ymm13
    vprolq      \$8, %ymm19, %ymm19
    vmovdqa64   %ymm0, %ymm30
    vpternlogq  \$0xD2, %ymm12, %ymm6, %ymm30

    vprolq      \$18, %ymm20, %ymm20
    vprolq      \$27, %ymm4, %ymm4
    vpxorq      %ymm29, %ymm30, %ymm30 # Iota step

    vprolq      \$36, %ymm5, %ymm5
    vprolq      \$10, %ymm11, %ymm11
    vmovdqa64   %ymm6, %ymm31
    vpternlogq  \$0xD2, %ymm18, %ymm12, %ymm31

    vprolq      \$15, %ymm17, %ymm17
    vprolq      \$56, %ymm23, %ymm23
    vpternlogq  \$0xD2, %ymm24, %ymm18, %ymm12

    vprolq      \$62, %ymm2, %ymm2
    vprolq      \$55, %ymm8, %ymm8
    vpternlogq  \$0xD2, %ymm0, %ymm24, %ymm18

    vprolq      \$39, %ymm14, %ymm14
    vprolq      \$41, %ymm15, %ymm15
    vpternlogq  \$0xD2, %ymm6, %ymm0, %ymm24
    vmovdqa64   %ymm30, %ymm0
    vmovdqa64   %ymm31, %ymm6

    vprolq      \$2, %ymm21, %ymm21
    vmovdqa64   %ymm3, %ymm30
    vpternlogq  \$0xD2, %ymm10, %ymm9, %ymm30
    vmovdqa64   %ymm9, %ymm31
    vpternlogq  \$0xD2, %ymm16, %ymm10, %ymm31

    vpternlogq  \$0xD2, %ymm22, %ymm16, %ymm10
    vpternlogq  \$0xD2, %ymm3, %ymm22, %ymm16
    vpternlogq  \$0xD2, %ymm9, %ymm3, %ymm22
    vmovdqa64   %ymm30, %ymm3
    vmovdqa64   %ymm31, %ymm9

    vmovdqa64   %ymm1, %ymm30
    vpternlogq  \$0xD2, %ymm13, %ymm7, %ymm30
    vmovdqa64   %ymm7, %ymm31
    vpternlogq  \$0xD2, %ymm19, %ymm13, %ymm31
    vpternlogq  \$0xD2, %ymm20, %ymm19, %ymm13

    vpternlogq  \$0xD2, %ymm1, %ymm20, %ymm19
    vpternlogq  \$0xD2, %ymm7, %ymm1, %ymm20
    vmovdqa64   %ymm30, %ymm1
    vmovdqa64   %ymm31, %ymm7
    vmovdqa64   %ymm4, %ymm30
    vpternlogq  \$0xD2, %ymm11, %ymm5, %ymm30

    vmovdqa64   %ymm5, %ymm31
    vpternlogq  \$0xD2, %ymm17, %ymm11, %ymm31
    vpternlogq  \$0xD2, %ymm23, %ymm17, %ymm11
    vpternlogq  \$0xD2, %ymm4, %ymm23, %ymm17

    vpternlogq  \$0xD2, %ymm5, %ymm4, %ymm23
    vmovdqa64   %ymm30, %ymm4
    vmovdqa64   %ymm31, %ymm5
    vmovdqa64   %ymm2, %ymm30
    vpternlogq  \$0xD2, %ymm14, %ymm8, %ymm30
    vmovdqa64   %ymm8, %ymm31
    vpternlogq  \$0xD2, %ymm15, %ymm14, %ymm31

    vpternlogq  \$0xD2, %ymm21, %ymm15, %ymm14
    vpternlogq  \$0xD2, %ymm2, %ymm21, %ymm15
    vpternlogq  \$0xD2, %ymm8, %ymm2, %ymm21
    vmovdqa64   %ymm30, %ymm2
    vmovdqa64   %ymm31, %ymm8

    # Complete the steps and get updated state registers in ymm0 to ymm24
    vmovdqa64   %ymm3,  %ymm30
    vmovdqa64   %ymm18, %ymm3
    vmovdqa64   %ymm17, %ymm18
    vmovdqa64   %ymm11, %ymm17
    vmovdqa64   %ymm7,  %ymm11
    vmovdqa64   %ymm10, %ymm7
    vmovdqa64   %ymm1,  %ymm10
    vmovdqa64   %ymm6,  %ymm1
    vmovdqa64   %ymm9,  %ymm6
    vmovdqa64   %ymm22, %ymm9
    vmovdqa64   %ymm14, %ymm22
    vmovdqa64   %ymm20, %ymm14
    vmovdqa64   %ymm2,  %ymm20
    vmovdqa64   %ymm12, %ymm2
    vmovdqa64   %ymm13, %ymm12
    vmovdqa64   %ymm19, %ymm13
    vmovdqa64   %ymm23, %ymm19
    vmovdqa64   %ymm15, %ymm23
    vmovdqa64   %ymm4,  %ymm15
    vmovdqa64   %ymm24, %ymm4
    vmovdqa64   %ymm21, %ymm24
    vmovdqa64   %ymm8,  %ymm21
    vmovdqa64   %ymm16, %ymm8
    vmovdqa64   %ymm5,  %ymm16
    vmovdqa64   %ymm30, %ymm5

    dec         $roundn           # Decrement the round counter
    jnz         .Lkeccak_rnd_loop # Jump to the start of the loop if r13d is not zero
    ret
.cfi_endproc
.size   keccak_1600_permute,.-keccak_1600_permute

# Initialize YMM registers 0-24 to zero
.globl  keccak_1600_init_state
.type   keccak_1600_init_state,\@abi-omnipotent
.align  32
keccak_1600_init_state:
.cfi_startproc
    vpxorq      %ymm0, %ymm0, %ymm0
    vmovdqa64   %ymm0, %ymm1
    vmovdqa64   %ymm0, %ymm2
    vmovdqa64   %ymm0, %ymm3
    vmovdqa64   %ymm0, %ymm4
    vmovdqa64   %ymm0, %ymm5
    vmovdqa64   %ymm0, %ymm6
    vmovdqa64   %ymm0, %ymm7
    vmovdqa64   %ymm0, %ymm8
    vmovdqa64   %ymm0, %ymm9
    vmovdqa64   %ymm0, %ymm10
    vmovdqa64   %ymm0, %ymm11
    vmovdqa64   %ymm0, %ymm12
    vmovdqa64   %ymm0, %ymm13
    vmovdqa64   %ymm0, %ymm14
    vmovdqa64   %ymm0, %ymm15
    vmovdqa64   %ymm0, %ymm16
    vmovdqa64   %ymm0, %ymm17
    vmovdqa64   %ymm0, %ymm18
    vmovdqa64   %ymm0, %ymm19
    vmovdqa64   %ymm0, %ymm20
    vmovdqa64   %ymm0, %ymm21
    vmovdqa64   %ymm0, %ymm22
    vmovdqa64   %ymm0, %ymm23
    vmovdqa64   %ymm0, %ymm24
    ret
.cfi_endproc
.size   keccak_1600_init_state,.-keccak_1600_init_state

.globl  keccak_1600_load_state_x4
.type   keccak_1600_load_state_x4,\@abi-omnipotent
.align  32
keccak_1600_load_state_x4:
.cfi_startproc
    vmovdqu64   32*0($arg1),  %ymm0
    vmovdqu64   32*1($arg1),  %ymm1
    vmovdqu64   32*2($arg1),  %ymm2
    vmovdqu64   32*3($arg1),  %ymm3
    vmovdqu64   32*4($arg1),  %ymm4
    vmovdqu64   32*5($arg1),  %ymm5
    vmovdqu64   32*6($arg1),  %ymm6
    vmovdqu64   32*7($arg1),  %ymm7
    vmovdqu64   32*8($arg1),  %ymm8
    vmovdqu64   32*9($arg1),  %ymm9
    vmovdqu64   32*10($arg1), %ymm10
    vmovdqu64   32*11($arg1), %ymm11
    vmovdqu64   32*12($arg1), %ymm12
    vmovdqu64   32*13($arg1), %ymm13
    vmovdqu64   32*14($arg1), %ymm14
    vmovdqu64   32*15($arg1), %ymm15
    vmovdqu64   32*16($arg1), %ymm16
    vmovdqu64   32*17($arg1), %ymm17
    vmovdqu64   32*18($arg1), %ymm18
    vmovdqu64   32*19($arg1), %ymm19
    vmovdqu64   32*20($arg1), %ymm20
    vmovdqu64   32*21($arg1), %ymm21
    vmovdqu64   32*22($arg1), %ymm22
    vmovdqu64   32*23($arg1), %ymm23
    vmovdqu64   32*24($arg1), %ymm24
    ret
.cfi_endproc
.size   keccak_1600_load_state_x4,.-keccak_1600_load_state_x4


.globl  keccak_1600_save_state_x4
.type   keccak_1600_save_state_x4,\@abi-omnipotent
.align  32
keccak_1600_save_state_x4:
.cfi_startproc
    vmovdqu64   %ymm0,  32*0($arg1)
    vmovdqu64   %ymm1,  32*1($arg1)
    vmovdqu64   %ymm2,  32*2($arg1)
    vmovdqu64   %ymm3,  32*3($arg1)
    vmovdqu64   %ymm4,  32*4($arg1)
    vmovdqu64   %ymm5,  32*5($arg1)
    vmovdqu64   %ymm6,  32*6($arg1)
    vmovdqu64   %ymm7,  32*7($arg1)
    vmovdqu64   %ymm8,  32*8($arg1)
    vmovdqu64   %ymm9,  32*9($arg1)
    vmovdqu64   %ymm10, 32*10($arg1)
    vmovdqu64   %ymm11, 32*11($arg1)
    vmovdqu64   %ymm12, 32*12($arg1)
    vmovdqu64   %ymm13, 32*13($arg1)
    vmovdqu64   %ymm14, 32*14($arg1)
    vmovdqu64   %ymm15, 32*15($arg1)
    vmovdqu64   %ymm16, 32*16($arg1)
    vmovdqu64   %ymm17, 32*17($arg1)
    vmovdqu64   %ymm18, 32*18($arg1)
    vmovdqu64   %ymm19, 32*19($arg1)
    vmovdqu64   %ymm20, 32*20($arg1)
    vmovdqu64   %ymm21, 32*21($arg1)
    vmovdqu64   %ymm22, 32*22($arg1)
    vmovdqu64   %ymm23, 32*23($arg1)
    vmovdqu64   %ymm24, 32*24($arg1)
    ret
.cfi_endproc
.size   keccak_1600_save_state_x4,.-keccak_1600_save_state_x4


# Add input data to state when message length is less than rate
# Arguments:
#   r10:        state pointer to absorb into (clobbered)
#   arg2 (rsi): message pointer lane 0 (updated on output)
#   arg3 (rdx): message pointer lane 1 (updated on output)
#   arg4 (rcx): message pointer lane 2 (updated on output)
#   arg5 (r8):  message pointer lane 3 (updated on output)
#   r12:        length in bytes (clobbered on output)
# Clobbers: r9, rbx, r15, k1, ymm31-ymm29
.globl  keccak_1600_partial_add_x4
.type   keccak_1600_partial_add_x4,\@abi-omnipotent
.align  32
keccak_1600_partial_add_x4:
.cfi_startproc
    mov     8*100(%r10), %r9
    test    \$7, %r9d
    jz      .Lstart_aligned_to_4x8

    # Start offset is not aligned to register size
    mov     %r9, %r15 # %r15 = s[100]

    and     \$7, %r9d
    neg     %r9d
    add     \$8, %r9d     # register capacity = 8 - (offset % 8)
    cmp     %r9d, %r12d
    cmovnae   %r12d, %r9d # %r9d = min(register capacity, length)

    lea     byte_kmask_0_to_7(%rip), %rbx
    kmovb   (%rbx,%r9), %k1 # message load mask

    mov     %r15, %rbx
    and     \$~7, %ebx
    lea     (%r10,%rbx,4), %r10 # get to state starting register

    mov     %r15, %rbx
    and     \$7, %ebx

    vmovdqu8    (%r10), %ymm31 # load & store / allocate SB for the register
    vmovdqu8    %ymm31, (%r10)

    vmovdqu8    ($arg2), %xmm31{%k1}{z}        # Read 1 to 7 bytes from lane 0
    vmovdqu8    8*0(%r10,%rbx), %xmm30{%k1}{z} # Read 1 to 7 bytes from state reg lane 0
    vpxorq      %xmm30, %xmm31, %xmm31
    vmovdqu8    %xmm31, 8*0(%r10,%rbx){%k1}    # Write 1 to 7 bytes to state reg lane 0

    vmovdqu8    ($arg3), %xmm31{%k1}{z}        # Read 1 to 7 bytes from lane 1
    vmovdqu8    8*1(%r10,%rbx), %xmm30{%k1}{z} # Read 1 to 7 bytes from state reg lane 1
    vpxorq      %xmm30, %xmm31, %xmm31
    vmovdqu8    %xmm31, 8*1(%r10,%rbx){%k1}    # Write 1 to 7 bytes to state reg lane 1

    vmovdqu8    ($arg4), %xmm31{%k1}{z}        # Read 1 to 7 bytes from lane 2
    vmovdqu8    8*2(%r10,%rbx), %xmm30{%k1}{z} # Read 1 to 7 bytes from state reg lane 2
    vpxorq      %xmm30, %xmm31, %xmm31
    vmovdqu8    %xmm31, 8*2(%r10,%rbx){%k1}    # Write 1 to 7 bytes to state reg lane 2

    vmovdqu8    ($arg5), %xmm31{%k1}{z}        # Read 1 to 7 bytes from lane 3
    vmovdqu8    8*3(%r10,%rbx), %xmm30{%k1}{z} # Read 1 to 7 bytes from state reg lane 3
    vpxorq      %xmm30, %xmm31, %xmm31
    vmovdqu8    %xmm31, 8*3(%r10,%rbx){%k1}    # Write 1 to 7 bytes to state reg lane 3

    sub     %r9, %r12
    jz      .Lzero_bytes

    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4
    add     %r9, $arg5
    add     \$32, %r10
    xor     %r9, %r9
    jmp     .Lymm_loop

.Lstart_aligned_to_4x8:
    lea     (%r10,%r9,4), %r10
    xor     %r9, %r9

.align  32
.Lymm_loop:
    cmp     \$8, %r12d
    jb      .Llt_8_bytes

    vmovq       ($arg2,%r9), %xmm31              # Read 8 bytes from lane 0
    vpinsrq     \$1, ($arg3,%r9), %xmm31, %xmm31 # Read 8 bytes from lane 1
    vmovq       ($arg4,%r9), %xmm30              # Read 8 bytes from lane 2
    vpinsrq     \$1, ($arg5,%r9),%xmm30, %xmm30  # Read 8 bytes from lane 3
    vinserti32x4 \$1, %xmm30, %ymm31, %ymm31
    vpxorq      (%r10,%r9,4), %ymm31, %ymm31     # Add data with the state
    vmovdqu64   %ymm31, (%r10,%r9,4)
    add     \$8, %r9
    sub     \$8, %r12
    jz      .Lzero_bytes

    jmp     .Lymm_loop

.align  32
.Lzero_bytes:
    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4
    add     %r9, $arg5
    ret

.align  32
.Llt_8_bytes:
    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4
    add     %r9, $arg5
    lea     (%r10,%r9,4), %r10

    lea     byte_kmask_0_to_7(%rip), %rbx
    kmovb   (%rbx,%r12), %k1 # message load mask

    vmovdqu8    ($arg2), %xmm31{%k1}{z} # Read 1 to 7 bytes from lane 0
    vmovdqu8    ($arg3), %xmm30{%k1}{z} # Read 1 to 7 bytes from lane 1
    vpunpcklqdq %xmm30, %xmm31, %xmm31  # Interleave data from lane 0 and lane 1
    vmovdqu8    ($arg4), %xmm30{%k1}{z} # Read 1 to 7 bytes from lane 2
    vmovdqu8    ($arg5), %xmm29{%k1}{z} # Read 1 to 7 bytes from lane 3
    vpunpcklqdq %xmm29, %xmm30, %xmm30  # Interleave data from lane 2 and lane 3
    vinserti32x4 \$1, %xmm30, %ymm31, %ymm31

    vpxorq      (%r10), %ymm31, %ymm31 # Add data to the state
    vmovdqu64   %ymm31, (%r10)         # Update state in memory

    add     %r12, $arg2 # increment message pointer lane 0
    add     %r12, $arg3 # increment message pointer lane 1
    add     %r12, $arg4 # increment message pointer lane 2
    add     %r12, $arg5 # increment message pointer lane 3
    ret
.cfi_endproc
.size   keccak_1600_partial_add_x4,.-keccak_1600_partial_add_x4


# Extract bytes from state and write to outputs
# Arguments:
#   r10:        state pointer to start extracting from (clobbered)
#   arg1 (rdi): output pointer lane 0 (updated on output)
#   arg2 (rsi): output pointer lane 1 (updated on output)
#   arg3 (rdx): output pointer lane 2 (updated on output)
#   arg4 (rcx): output pointer lane 3 (updated on output)
#   r12:        length in bytes (clobbered on output)
#   r11:        state offset to start extract from
.globl  keccak_1600_extract_bytes_x4
.type   keccak_1600_extract_bytes_x4,\@abi-omnipotent
.align  32
keccak_1600_extract_bytes_x4:
.cfi_startproc
    or      %r12, %r12
    jz      .Lextract_zero_bytes

    test    \$7, %r11d
    jz      .Lextract_start_aligned_to_4x8

    # Extract offset is not aligned to the register size (8 bytes)
    mov     %r11, %r9

    and     \$7, %r9d
    neg     %r9d
    add     \$8, %r9d     # register capacity = 8 - (offset % 8)
    cmp     %r9d, %r12d
    cmovnae   %r12d, %r9d # %r9d = min(register capacity, length)

    lea     byte_kmask_0_to_7(%rip), %rbx
    kmovb   (%rbx,%r9), %k1 # message store mask

    mov     %r11, %rbx
    and     \$~7, %ebx
    lea     (%r10,%rbx,4), %r10 # get to state starting register

    mov     %r11, %rbx
    and     \$7, %ebx

    vmovdqu8    8*0(%r10,%rbx), %xmm31{%k1}{z} # Read 1-7 bytes from state reg lane 0
    vmovdqu8    %xmm31, ($arg1){%k1}           # Write 1-7 bytes to lane 0 output

    vmovdqu8    8*1(%r10,%rbx), %xmm31{%k1}{z} # Read 1-7 bytes from state reg lane 1
    vmovdqu8    %xmm31, ($arg2){%k1}           # Write 1-7 bytes to lane 1 output

    vmovdqu8    8*2(%r10,%rbx), %xmm31{%k1}{z} # Read 1-7 bytes from state reg lane 2
    vmovdqu8    %xmm31, ($arg3){%k1}           # Write 1-7 bytes to lane 2 output

    vmovdqu8    8*3(%r10,%rbx), %xmm31{%k1}{z} # Read 1-7 bytes from state reg lane 3
    vmovdqu8    %xmm31, ($arg4){%k1}           # Write 1-7 bytes to lane 3 output

    # Increment output registers
    add     %r9, $arg1
    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4

    # Decrement length to extract
    sub     %r9, %r12
    jz      .Lextract_zero_bytes

    # More data to extract, update state register pointer
    add     \$32, %r10
    xor     %r9, %r9
    jmp     .Lextract_ymm_loop

.Lextract_start_aligned_to_4x8:
        lea     (%r10,%r11,4), %r10
        xor     %r9, %r9

.align  32
.Lextract_ymm_loop:
    cmp     \$8, %r12
    jb      .Lextract_lt_8_bytes

    vmovdqu64   (%r10), %xmm31
    vmovdqu64   16(%r10), %xmm30
    vmovq       %xmm31, ($arg1,%r9)
    vpextrq     \$1, %xmm31, ($arg2,%r9)
    vmovq       %xmm30, ($arg3,%r9)
    vpextrq     \$1, %xmm30, ($arg4,%r9)
    add     \$8, %r9
    sub     \$8, %r12
    jz      .Lzero_bytes_left

    add     \$32, %r10
    jmp     .Lextract_ymm_loop

.align  32
.Lzero_bytes_left:
    # Increment output pointers
    add     %r9, $arg1
    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4
.Lextract_zero_bytes:
    ret

.align  32
.Lextract_lt_8_bytes:
    add     %r9, $arg1
    add     %r9, $arg2
    add     %r9, $arg3
    add     %r9, $arg4

    lea     byte_kmask_0_to_7(%rip), %r9
    kmovb   (%r9,%r12), %k1 # k1 is the mask of message bytes to read

    vmovq       0*8(%r10), %xmm31    # Read 8 bytes from state lane 0
    vmovdqu8    %xmm31, ($arg1){%k1} # Extract 1-7 bytes into output 0
    vmovq       1*8(%r10), %xmm31    # Read 8 bytes from state lane 1
    vmovdqu8    %xmm31, ($arg2){%k1} # Extract 1-7 bytes into output 1
    vmovq       2*8(%r10), %xmm31    # Read 8 bytes from state lane 2
    vmovdqu8    %xmm31, ($arg3){%k1} # Extract 1-7 bytes into output 2
    vmovq       3*8(%r10), %xmm31    # Read 8 bytes from state lane 3
    vmovdqu8    %xmm31, ($arg4){%k1} # Extract 1-7 bytes into output 3

    # Increment output pointers
    add     %r12, $arg1
    add     %r12, $arg2
    add     %r12, $arg3
    add     %r12, $arg4
    ret
.cfi_endproc
.size   keccak_1600_extract_bytes_x4,.-keccak_1600_extract_bytes_x4


# SHAKE128 x4 multi-buffer functions
# These functions process 4 independent SHAKE128 streams in parallel using AVX-512VL
# State layout: 25 ymm registers (200 bytes each) + 1 qword = 808 bytes per context
# Rate: 168 bytes for SHAKE128

# SHA3_shake128_x4_avx512vl
# One-shot SHAKE-128 x4 function: init + absorb + finalize + squeeze
# Arguments:
#   arg1 (rdi): pointer to output lane 0
#   arg2 (rsi): pointer to output lane 1
#   arg3 (rdx): pointer to output lane 2
#   arg4 (rcx): pointer to output lane 3
#   arg5 (r8):  output length in bytes (must be same for all lanes)
#   arg6 (r9):  pointer to input lane 0
#   [stack+0]:  pointer to input lane 1
#   [stack+8]:  pointer to input lane 2
#   [stack+16]: pointer to input lane 3
#   [stack+24]: input length in bytes (must be same for all lanes)
# Returns: void
.globl  SHA3_shake128_x4_avx512vl
.type   SHA3_shake128_x4_avx512vl,\@function,10
.align  32
SHA3_shake128_x4_avx512vl:
.cfi_startproc
    push    %rbp
.cfi_push       %rbp
    mov     %rsp, %rbp
    push    %rbx
.cfi_push       %rbx
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

    sub     \$$sf_size, %rsp
    mov     %rsp, %rbx

.Lshake128_x4_body:
    mov     $arg1, $sf_arg1(%rbx)
    mov     $arg2, $sf_arg2(%rbx)
    mov     $arg3, $sf_arg3(%rbx)
    mov     $arg4, $sf_arg4(%rbx)
    mov     $arg5, $sf_arg5(%rbx)

    lea     $sf_state_x4(%rbx), $arg1 # start of x4 state on the stack frame
    mov     $arg1, $sf_state_ptr(%rbx)

    # Initialize the state array to zero
    call    keccak_1600_init_state

    call    keccak_1600_save_state_x4

    movq    \$0, 8*100($arg1) # clear s[100]

    mov     $sf_state_ptr(%rbx), $arg1
    mov     $arg6, $arg2
___
$code .= <<___ if ($win64);
    # xlate prologue handles up to six arguments. For one-shot x4 wrappers
    # (10 args), the remaining four stay in Win64 stack slots.
    mov     64(%rbp), $arg3 # arg7 from stack
    mov     72(%rbp), $arg4 # arg8 from stack
    mov     80(%rbp), $arg5 # arg9 from stack
    mov     88(%rbp), $arg6 # arg10 from stack
___
$code .= <<___ if (!$win64);
    mov     16(%rbp), $arg3 # arg7 from stack
    mov     24(%rbp), $arg4 # arg8 from stack
    mov     32(%rbp), $arg5 # arg9 from stack
    mov     40(%rbp), $arg6 # arg10 from stack
___
$code.=<<___;
    # Internal entry avoids Win64 xlate prologue argument remapping.
___
$code .= call_internal("SHA3_shake128_x4_inc_absorb_avx512vl_internal");
$code.=<<___;

    mov     $sf_state_ptr(%rbx), $arg1
    call    .L_SHA3_shake128_x4_inc_finalize_avx512vl

    # squeeze
    mov     $sf_arg1(%rbx), $arg1
    mov     $sf_arg2(%rbx), $arg2
    mov     $sf_arg3(%rbx), $arg3
    mov     $sf_arg4(%rbx), $arg4
    mov     $sf_arg5(%rbx), $arg5
    mov     $sf_state_ptr(%rbx), $arg6
___
$code .= call_internal("SHA3_shake128_x4_inc_squeeze_avx512vl_internal");
$code.=<<___;

    # Clear the temporary buffer
    lea     $sf_state_x4(%rbx), %r9
    vpxorq      %ymm31, %ymm31, %ymm31
    vmovdqu64   %ymm31, 32*0(%r9)
    vmovdqu64   %ymm31, 32*1(%r9)
    vmovdqu64   %ymm31, 32*2(%r9)
    vmovdqu64   %ymm31, 32*3(%r9)
    vmovdqu64   %ymm31, 32*4(%r9)
    vmovdqu64   %ymm31, 32*5(%r9)
    vmovdqu64   %ymm31, 32*6(%r9)
    vmovdqu64   %ymm31, 32*7(%r9)
    vmovdqu64   %ymm31, 32*8(%r9)
    vmovdqu64   %ymm31, 32*9(%r9)
    vmovdqu64   %ymm31, 32*10(%r9)
    vmovdqu64   %ymm31, 32*11(%r9)
    vmovdqu64   %ymm31, 32*12(%r9)
    vmovdqu64   %ymm31, 32*13(%r9)
    vmovdqu64   %ymm31, 32*14(%r9)
    vmovdqu64   %ymm31, 32*15(%r9)
    vmovdqu64   %ymm31, 32*16(%r9)
    vmovdqu64   %ymm31, 32*17(%r9)
    vmovdqu64   %ymm31, 32*18(%r9)
    vmovdqu64   %ymm31, 32*19(%r9)
    vmovdqu64   %ymm31, 32*20(%r9)
    vmovdqu64   %ymm31, 32*21(%r9)
    vmovdqu64   %ymm31, 32*22(%r9)
    vmovdqu64   %ymm31, 32*23(%r9)
    vmovdqu64   %ymm31, 32*24(%r9)
    vmovq       %xmm31, 32*25(%r9)

.Lshake128_x4_epilogue:
___
$code .= <<___ if ($win64);
    vmovups $sf_size+0(%rsp),   %xmm6
    vmovups $sf_size+16(%rsp),  %xmm7
    vmovups $sf_size+32(%rsp),  %xmm8
    vmovups $sf_size+48(%rsp),  %xmm9
    vmovups $sf_size+64(%rsp),  %xmm10
    vmovups $sf_size+80(%rsp),  %xmm11
    vmovups $sf_size+96(%rsp),  %xmm12
    vmovups $sf_size+112(%rsp), %xmm13
    vmovups $sf_size+128(%rsp), %xmm14
    vmovups $sf_size+144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;
    add     \$$sf_size, %rsp
    pop     %rbx
.cfi_pop        %rbx
    pop     %rbp
.cfi_pop        %rbp
    ret
.cfi_endproc
.size   SHA3_shake128_x4_avx512vl,.-SHA3_shake128_x4_avx512vl

___

$code .= <<___ if ($win64);
# Internal Win64 shim for absorb entry. It establishes xlate-compatible
# unwind state and then jumps to the function entry after the prologue.
# This is required for internal calls since the xlate ABI conversion
# is already done in the caller function.
.type   SHA3_shake128_x4_inc_absorb_avx512vl_internal,\@abi-omnipotent
.align  32
.LSEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl_internal:
SHA3_shake128_x4_inc_absorb_avx512vl_internal:
    mov     %rsp, %rax
    mov     $arg1, 8(%rsp)
    mov     $arg2, 16(%rsp)
    jmp     .L_SHA3_shake128_x4_inc_absorb_avx512vl
.LSEH_end_SHA3_shake128_x4_inc_absorb_avx512vl_internal:
.size   SHA3_shake128_x4_inc_absorb_avx512vl_internal,.-SHA3_shake128_x4_inc_absorb_avx512vl_internal
___
$code.=<<___;

# SHA3_shake128_x4_inc_absorb_avx512vl
# Absorb input data into 4 parallel SHAKE128 states
# Arguments:
#   arg1 (rdi): pointer to state context (808 bytes)
#   arg2 (rsi): pointer to lane 0 input data
#   arg3 (rdx): pointer to lane 1 input data
#   arg4 (rcx): pointer to lane 2 input data
#   arg5 (r8):  pointer to lane 3 input data
#   arg6 (r9):  input length in bytes (must be same for all lanes)
# Returns: void
# Note: Input is XORed into state and Keccak permutation is applied for each rate-sized block
.globl  SHA3_shake128_x4_inc_absorb_avx512vl
.type   SHA3_shake128_x4_inc_absorb_avx512vl,\@function,6
.align  32
SHA3_shake128_x4_inc_absorb_avx512vl:
.L_SHA3_shake128_x4_inc_absorb_avx512vl:
.cfi_startproc
        push    %rbp
.cfi_push       %rbp
        push    %rbx
.cfi_push       %rbx
        push    %r12
.cfi_push       %r12
        push    %r13
.cfi_push       %r13
        push    %r14
.cfi_push       %r14
        push    %r15
.cfi_push       %r15
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

.Lshake128_absorb_body:
    # check for partially processed block
    mov     8*100($arg1), %r14
    or      %r14, %r14 # s[100] == 0?
    je      .Lshake128_absorb_main_loop_start

    # process remaining bytes if message long enough
    mov     \$168, %r12 # SHAKE128_RATE = 168
    sub     %r14, %r12  # %r12 = capacity

    cmp     %r12, $arg6 # if mlen <= capacity then no permute
    jbe     .Lshake128_absorb_skip_permute

    sub     %r12, $arg6
    mov     $arg6, %r11 # preserve remaining length across helper calls

    # r10/state, arg2-arg5/inputs, r12/length
    mov     $arg1, %r10                # %r10 = state
    call    keccak_1600_partial_add_x4 # arg2-arg5 are updated

    call    keccak_1600_load_state_x4

    call    keccak_1600_permute

    movq    \$0, 8*100($arg1) # clear s[100]
    jmp     .Lshake128_absorb_partial_block_done

.Lshake128_absorb_skip_permute:
    # r10/state, arg2-arg5/inputs, r12/length
    mov     $arg1, %r10
    mov     $arg6, %r12
    mov     $arg6, %r11 # preserve input length across helper call
    call    keccak_1600_partial_add_x4

    lea     (%r11,%r14), %r15
    mov     %r15, 8*100($arg1) # s[100] += inlen

    cmp     \$168, %r15 # check s[100] below SHAKE128_RATE
    jb      .Lshake128_absorb_exit

    call    keccak_1600_load_state_x4

    call    keccak_1600_permute

    call    keccak_1600_save_state_x4

    movq    \$0, 8*100($arg1) # clear s[100]
    jmp     .Lshake128_absorb_exit

.Lshake128_absorb_main_loop_start:
    call    keccak_1600_load_state_x4
    mov     $arg6, %r11 # full input length when no prior partial block

.Lshake128_absorb_partial_block_done:
    xor     %r12, %r12  # zero message offset

    # Process the input message in blocks
.align  32
.Lshake128_absorb_while_loop:
    cmp     \$168, %r11 # compare mlen to SHAKE128_RATE
    jb      .Lshake128_absorb_while_loop_done

    # Inline absorb_bytes_x4 for SHAKE128_RATE (168 bytes = 21 ymm registers)
___

# Generate absorb code for SHAKE128 rate (168 bytes)
for (my $i = 0; $i < 21; $i++) {
    my $offset = $i * 8;
    $code.=<<___;
        vmovq       $offset($arg2,%r12), %xmm31
        vpinsrq     \$1, $offset($arg3,%r12), %xmm31, %xmm31
        vmovq       $offset($arg4,%r12), %xmm30
        vpinsrq     \$1, $offset($arg5,%r12), %xmm30, %xmm30
        vinserti32x4 \$1, %xmm30, %ymm31, %ymm31
        vpxorq      %ymm31, %ymm$i, %ymm$i
___
}

$code.=<<___;
    sub     \$168, %r11         # Subtract the rate from the remaining length
    add     \$168, %r12         # Adjust offset to next block
    call    keccak_1600_permute # Perform the Keccak permutation

    jmp     .Lshake128_absorb_while_loop

.align  32
.Lshake128_absorb_while_loop_done:
    call    keccak_1600_save_state_x4

    mov     %r11, 8*100($arg1) # update s[100]
    or      %r11, %r11
    jz      .Lshake128_absorb_exit

    movq    \$0, 8*100($arg1) # clear s[100]

    # r10/state, arg2-arg5/input, r12/length
    mov     $arg1, %r10
    add     %r12, $arg2
    add     %r12, $arg3
    add     %r12, $arg4
    add     %r12, $arg5
    mov     %r11, %r12
    call    keccak_1600_partial_add_x4

    mov     %r11, 8*100($arg1) # update s[100]

.Lshake128_absorb_exit:
    # Clear sensitive registers
    vpxorq      %xmm16, %xmm16, %xmm16
    vmovdqa64   %ymm16, %ymm17
    vmovdqa64   %ymm16, %ymm18
    vmovdqa64   %ymm16, %ymm19
    vmovdqa64   %ymm16, %ymm20
    vmovdqa64   %ymm16, %ymm21
    vmovdqa64   %ymm16, %ymm22
    vmovdqa64   %ymm16, %ymm23
    vmovdqa64   %ymm16, %ymm24
    vmovdqa64   %ymm16, %ymm25
    vmovdqa64   %ymm16, %ymm26
    vmovdqa64   %ymm16, %ymm27
    vmovdqa64   %ymm16, %ymm28
    vmovdqa64   %ymm16, %ymm29
    vmovdqa64   %ymm16, %ymm30
    vmovdqa64   %ymm16, %ymm31
.Lshake128_absorb_epilogue:
    vzeroall
___
$code .= <<___ if ($win64);
    vmovups 0(%rsp),   %xmm6
    vmovups 16(%rsp),  %xmm7
    vmovups 32(%rsp),  %xmm8
    vmovups 48(%rsp),  %xmm9
    vmovups 64(%rsp),  %xmm10
    vmovups 80(%rsp),  %xmm11
    vmovups 96(%rsp),  %xmm12
    vmovups 112(%rsp), %xmm13
    vmovups 128(%rsp), %xmm14
    vmovups 144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;

    pop     %r15
.cfi_pop        %r15
    pop     %r14
.cfi_pop        %r14
    pop     %r13
.cfi_pop        %r13
    pop     %r12
.cfi_pop        %r12
    pop     %rbx
.cfi_pop        %rbx
    pop     %rbp
.cfi_pop        %rbp
    ret
.cfi_endproc
.size   SHA3_shake128_x4_inc_absorb_avx512vl,.-SHA3_shake128_x4_inc_absorb_avx512vl


# SHA3_shake128_x4_inc_finalize_avx512vl
# Finalize absorption phase for 4 parallel SHAKE-128 states
# Adds padding and terminator bytes and clears the absorb offset
# Arguments:
#   arg1 (rdi): pointer to state context (808 bytes)
# Returns: void
# Note: After this call, state is ready for squeezing output
.globl  SHA3_shake128_x4_inc_finalize_avx512vl
.type   SHA3_shake128_x4_inc_finalize_avx512vl,\@function,1
.align  32
SHA3_shake128_x4_inc_finalize_avx512vl:
.L_SHA3_shake128_x4_inc_finalize_avx512vl:
.cfi_startproc
    mov         8*100($arg1), %r11 # load state offset from s[100]
    mov         %r11, %r10
    and         \$~7, %r10d        # offset to the state register
    and         \$7, %r11d         # offset within the register

    # add EOM byte right after the message
    vmovdqu32   ($arg1,%r10,4), %ymm31
    lea         shake_msg_pad_x4(%rip), %r9
    sub         %r11, %r9
    vmovdqu32   (%r9), %ymm30
    vpxorq      %ymm30, %ymm31, %ymm31
    vmovdqu32   %ymm31, ($arg1,%r10,4)

    # add terminating byte at offset equal to rate - 1 (SHAKE128_RATE = 168)
    vmovdqu32   640($arg1), %ymm31 # 168*4 - 32 = 672 - 32 = 640
    vmovdqa32   shake_terminator_byte_x4(%rip), %ymm30
    vpxorq      %ymm30, %ymm31, %ymm31
    vmovdqu32   %ymm31, 640($arg1)

    movq        \$0, 8*100($arg1) # clear s[100]
    vpxorq      %ymm31, %ymm31, %ymm31
    ret
.cfi_endproc
.size   SHA3_shake128_x4_inc_finalize_avx512vl,.-SHA3_shake128_x4_inc_finalize_avx512vl

___

$code .= <<___ if ($win64);
# Internal Win64 shim for squeeze entry. It establishes xlate-compatible
# unwind state and then jumps to the function entry after the prologue.
# This is required for internal calls since the xlate ABI conversion
# is already done in the caller function.
.type   SHA3_shake128_x4_inc_squeeze_avx512vl_internal,\@abi-omnipotent
.align  32
.LSEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
    mov     %rsp, %rax
    mov     $arg1, 8(%rsp)
    mov     $arg2, 16(%rsp)
    jmp     .L_SHA3_shake128_x4_inc_squeeze_avx512vl
.LSEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
.size   SHA3_shake128_x4_inc_squeeze_avx512vl_internal,.-SHA3_shake128_x4_inc_squeeze_avx512vl_internal
___
$code.=<<___;

# SHA3_shake128_x4_inc_squeeze_avx512vl
# Squeeze output from 4 parallel SHAKE128 states
# Arguments:
#   arg1 (rdi): pointer to lane 0 output buffer
#   arg2 (rsi): pointer to lane 1 output buffer
#   arg3 (rdx): pointer to lane 2 output buffer
#   arg4 (rcx): pointer to lane 3 output buffer
#   arg5 (r8):  output length in bytes (must be same for all lanes)
#   arg6 (r9):  pointer to state context (808 bytes)
# Returns: void
# Note: Can be called multiple times to generate arbitrary-length output
.globl  SHA3_shake128_x4_inc_squeeze_avx512vl
.type   SHA3_shake128_x4_inc_squeeze_avx512vl,\@function,6
.align  32
SHA3_shake128_x4_inc_squeeze_avx512vl:
.L_SHA3_shake128_x4_inc_squeeze_avx512vl:
.cfi_startproc
    push    %rbp
.cfi_push       %rbp
    push    %rbx
.cfi_push       %rbx
    push    %r12
.cfi_push       %r12
    push    %r13
.cfi_push       %r13
    push    %r14
.cfi_push       %r14
    push    %r15
.cfi_push       %r15
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

.Lshake128_squeeze_body:
    or      $arg5, $arg5
    jz      .Lshake128_squeeze_done

    # check for partially processed block
    mov     8*100($arg6), %r15 # s[100] - capacity
    or      %r15, %r15
    jnz     .Lshake128_squeeze_no_init_permute

    mov     $arg1, %r14
    mov     $arg6, $arg1
    call    keccak_1600_load_state_x4

    mov     %r14, $arg1

    xor     %rbp, %rbp
    jmp     .Lshake128_squeeze_loop

.align  32
.Lshake128_squeeze_no_init_permute:
    # extract bytes: r10 - state/src, arg1-arg4 - output/dst, r12 - length = min(capacity, outlen), r11 - offset
    mov     $arg6, %r10
    mov     $arg6, %r14 # preserve state pointer across extract helper

    mov     %r15, %r12
    cmp     %r15, $arg5
    cmovnae $arg5, %r12 # %r12 = min(capacity, outlen)

    sub     %r12, $arg5 # outlen -= length

    mov     \$168, %r11d # SHAKE128_RATE
    sub     %r15, %r11   # state offset

    sub     %r12, %r15         # capacity -= length
    mov     %r15, 8*100($arg6) # update s[100]

    call    keccak_1600_extract_bytes_x4
    mov     %r14, $arg6        # restore state pointer after helper clobbers

    or      %r15, %r15
    jnz     .Lshake128_squeeze_done # check s[100] not zero

    mov     $arg1, %r13 # preserve arg1
    mov     %r14, $arg1
    call    keccak_1600_load_state_x4

    mov     %r13, $arg1
    xor     %rbp, %rbp

.align  32
.Lshake128_squeeze_loop:
    cmp     \$168, $arg5 # outlen > SHAKE128_RATE
    jb      .Lshake128_squeeze_final_extract

    call    keccak_1600_permute

    # Extract SHAKE128 rate bytes (168 bytes = 21 x 8 bytes) inline
___

# Generate extract code for SHAKE128 rate (168 bytes = 21 ymm registers)
for (my $i = 0; $i < 21; $i++) {
    my $offset = $i * 8;
    $code.=<<___;
        vextracti64x2 \$1, %ymm$i, %xmm31
        vmovq       %xmm$i, $offset($arg1,%rbp)
        vpextrq     \$1, %xmm$i, $offset($arg2,%rbp)
        vmovq       %xmm31, $offset($arg3,%rbp)
        vpextrq     \$1, %xmm31, $offset($arg4,%rbp)
___
}

$code.=<<___;
    add     \$168, %rbp  # dst offset += SHAKE128_RATE
    sub     \$168, $arg5 # outlen -= SHAKE128_RATE
    jmp     .Lshake128_squeeze_loop

.align  32
.Lshake128_squeeze_final_extract:
    or      $arg5, $arg5
    jz      .Lshake128_squeeze_no_end_permute

    # update output pointers
    add     %rbp, $arg1
    add     %rbp, $arg2
    add     %rbp, $arg3
    add     %rbp, $arg4

    mov     \$168, %r15d       # SHAKE128_RATE
    sub     $arg5, %r15
    mov     %r15, 8*100($arg6) # s[100] = capacity

    call    keccak_1600_permute

    mov     $arg1, %r14
    mov     $arg6, $arg1
    call    keccak_1600_save_state_x4

    mov     %r14, $arg1

    # extract bytes: r10 - state/src, arg1-arg4 - output/dst, r12 - length, r11 - offset = 0
    mov     $arg6, %r10
    mov     $arg5, %r12
    xor     %r11, %r11
    call    keccak_1600_extract_bytes_x4

    jmp     .Lshake128_squeeze_done

.Lshake128_squeeze_no_end_permute:
    movq    \$0, 8*100($arg6) # s[100] = 0
    mov     $arg6, $arg1
    call    keccak_1600_save_state_x4

.Lshake128_squeeze_done:
    # Clear sensitive registers
    vpxorq      %xmm16, %xmm16, %xmm16
    vmovdqa64   %ymm16, %ymm17
    vmovdqa64   %ymm16, %ymm18
    vmovdqa64   %ymm16, %ymm19
    vmovdqa64   %ymm16, %ymm20
    vmovdqa64   %ymm16, %ymm21
    vmovdqa64   %ymm16, %ymm22
    vmovdqa64   %ymm16, %ymm23
    vmovdqa64   %ymm16, %ymm24
    vmovdqa64   %ymm16, %ymm25
    vmovdqa64   %ymm16, %ymm26
    vmovdqa64   %ymm16, %ymm27
    vmovdqa64   %ymm16, %ymm28
    vmovdqa64   %ymm16, %ymm29
    vmovdqa64   %ymm16, %ymm30
    vmovdqa64   %ymm16, %ymm31
.Lshake128_squeeze_epilogue:
    vzeroall
___
$code .= <<___ if ($win64);
    vmovups 0(%rsp),   %xmm6
    vmovups 16(%rsp),  %xmm7
    vmovups 32(%rsp),  %xmm8
    vmovups 48(%rsp),  %xmm9
    vmovups 64(%rsp),  %xmm10
    vmovups 80(%rsp),  %xmm11
    vmovups 96(%rsp),  %xmm12
    vmovups 112(%rsp), %xmm13
    vmovups 128(%rsp), %xmm14
    vmovups 144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;

    pop %r15
.cfi_pop    %r15
    pop %r14
.cfi_pop    %r14
    pop %r13
.cfi_pop    %r13
    pop %r12
.cfi_pop    %r12
    pop %rbx
.cfi_pop    %rbx
    pop %rbp
.cfi_pop    %rbp
    ret
.cfi_endproc
.size   SHA3_shake128_x4_inc_squeeze_avx512vl,.-SHA3_shake128_x4_inc_squeeze_avx512vl


# SHAKE256 x4 multi-buffer functions
# These functions process 4 independent SHAKE256 streams in parallel using AVX-512VL
# State layout: 25 ymm registers (200 bytes each) + 1 qword = 808 bytes per context
# Rate: 136 bytes for SHAKE256

# SHA3_shake256_x4_avx512vl
# One-shot SHAKE-256 x4 function: init + absorb + finalize + squeeze
# Arguments:
#   arg1 (rdi): pointer to output lane 0
#   arg2 (rsi): pointer to output lane 1
#   arg3 (rdx): pointer to output lane 2
#   arg4 (rcx): pointer to output lane 3
#   arg5 (r8):  output length in bytes (must be same for all lanes)
#   arg6 (r9):  pointer to input lane 0
#   [stack+0]:  pointer to input lane 1
#   [stack+8]:  pointer to input lane 2
#   [stack+16]: pointer to input lane 3
#   [stack+24]: input length in bytes (must be same for all lanes)
# Returns: void
.globl  SHA3_shake256_x4_avx512vl
.type   SHA3_shake256_x4_avx512vl,\@function,10
.align  32
SHA3_shake256_x4_avx512vl:
.cfi_startproc
    push    %rbp
.cfi_push       %rbp
    mov     %rsp, %rbp
    push    %rbx
.cfi_push       %rbx
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

    sub     \$$sf_size, %rsp
    mov     %rsp, %rbx

.Lshake256_x4_body:
    mov     $arg1, $sf_arg1(%rbx)
    mov     $arg2, $sf_arg2(%rbx)
    mov     $arg3, $sf_arg3(%rbx)
    mov     $arg4, $sf_arg4(%rbx)
    mov     $arg5, $sf_arg5(%rbx)

    lea     $sf_state_x4(%rbx), $arg1 # start of x4 state on the stack frame
    mov     $arg1, $sf_state_ptr(%rbx)

    # Initialize the state array to zero
    call    keccak_1600_init_state

    call    keccak_1600_save_state_x4

    movq    \$0, 8*100($arg1) # clear s[100]

    mov     $sf_state_ptr(%rbx), $arg1
    mov     $arg6, $arg2
___
$code .= <<___ if ($win64);
    # xlate prologue handles up to six arguments. For one-shot x4 wrappers
    # (10 args), the remaining four stay in Win64 stack slots.
    mov     64(%rbp), $arg3 # arg7 from stack
    mov     72(%rbp), $arg4 # arg8 from stack
    mov     80(%rbp), $arg5 # arg9 from stack
    mov     88(%rbp), $arg6 # arg10 from stack
___
$code .= <<___ if (!$win64);
    mov     16(%rbp), $arg3 # arg7 from stack
    mov     24(%rbp), $arg4 # arg8 from stack
    mov     32(%rbp), $arg5 # arg9 from stack
    mov     40(%rbp), $arg6 # arg10 from stack
___
$code.=<<___;
    # Internal entry avoids Win64 xlate prologue argument remapping.
___
$code .= call_internal("SHA3_shake256_x4_inc_absorb_avx512vl_internal");
$code.=<<___;

    mov     $sf_state_ptr(%rbx), $arg1
    call    .L_SHA3_shake256_x4_inc_finalize_avx512vl

    # squeeze
    mov     $sf_arg1(%rbx), $arg1
    mov     $sf_arg2(%rbx), $arg2
    mov     $sf_arg3(%rbx), $arg3
    mov     $sf_arg4(%rbx), $arg4
    mov     $sf_arg5(%rbx), $arg5
    mov     $sf_state_ptr(%rbx), $arg6
___
$code .= call_internal("SHA3_shake256_x4_inc_squeeze_avx512vl_internal");
$code.=<<___;

    # Clear the temporary buffer
    lea     $sf_state_x4(%rbx), %r9
    vpxorq      %ymm31, %ymm31, %ymm31
    vmovdqu64   %ymm31, 32*0(%r9)
    vmovdqu64   %ymm31, 32*1(%r9)
    vmovdqu64   %ymm31, 32*2(%r9)
    vmovdqu64   %ymm31, 32*3(%r9)
    vmovdqu64   %ymm31, 32*4(%r9)
    vmovdqu64   %ymm31, 32*5(%r9)
    vmovdqu64   %ymm31, 32*6(%r9)
    vmovdqu64   %ymm31, 32*7(%r9)
    vmovdqu64   %ymm31, 32*8(%r9)
    vmovdqu64   %ymm31, 32*9(%r9)
    vmovdqu64   %ymm31, 32*10(%r9)
    vmovdqu64   %ymm31, 32*11(%r9)
    vmovdqu64   %ymm31, 32*12(%r9)
    vmovdqu64   %ymm31, 32*13(%r9)
    vmovdqu64   %ymm31, 32*14(%r9)
    vmovdqu64   %ymm31, 32*15(%r9)
    vmovdqu64   %ymm31, 32*16(%r9)
    vmovdqu64   %ymm31, 32*17(%r9)
    vmovdqu64   %ymm31, 32*18(%r9)
    vmovdqu64   %ymm31, 32*19(%r9)
    vmovdqu64   %ymm31, 32*20(%r9)
    vmovdqu64   %ymm31, 32*21(%r9)
    vmovdqu64   %ymm31, 32*22(%r9)
    vmovdqu64   %ymm31, 32*23(%r9)
    vmovdqu64   %ymm31, 32*24(%r9)
    vmovq       %xmm31, 32*25(%r9)

.Lshake256_x4_epilogue:
___
$code .= <<___ if ($win64);
    vmovups $sf_size+0(%rsp),   %xmm6
    vmovups $sf_size+16(%rsp),  %xmm7
    vmovups $sf_size+32(%rsp),  %xmm8
    vmovups $sf_size+48(%rsp),  %xmm9
    vmovups $sf_size+64(%rsp),  %xmm10
    vmovups $sf_size+80(%rsp),  %xmm11
    vmovups $sf_size+96(%rsp),  %xmm12
    vmovups $sf_size+112(%rsp), %xmm13
    vmovups $sf_size+128(%rsp), %xmm14
    vmovups $sf_size+144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;
    add     \$$sf_size, %rsp
    pop     %rbx
.cfi_pop        %rbx
    pop     %rbp
.cfi_pop        %rbp
    ret
.cfi_endproc
.size   SHA3_shake256_x4_avx512vl,.-SHA3_shake256_x4_avx512vl

___

$code .= <<___ if ($win64);
# Internal Win64 shim for absorb entry. It establishes xlate-compatible
# unwind state and then jumps to the function entry after the prologue.
# This is required for internal calls since the xlate ABI conversion
# is already done in the caller function.
.type   SHA3_shake256_x4_inc_absorb_avx512vl_internal,\@abi-omnipotent
.align  32
.LSEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl_internal:
SHA3_shake256_x4_inc_absorb_avx512vl_internal:
    mov     %rsp, %rax
    mov     $arg1, 8(%rsp)
    mov     $arg2, 16(%rsp)
    jmp     .L_SHA3_shake256_x4_inc_absorb_avx512vl
.LSEH_end_SHA3_shake256_x4_inc_absorb_avx512vl_internal:
.size   SHA3_shake256_x4_inc_absorb_avx512vl_internal,.-SHA3_shake256_x4_inc_absorb_avx512vl_internal
___
$code.=<<___;

# SHA3_shake256_x4_inc_absorb_avx512vl
# Absorb input data into 4 parallel SHAKE256 states
# Arguments:
#   arg1 (rdi): pointer to state context (808 bytes)
#   arg2 (rsi): pointer to lane 0 input data
#   arg3 (rdx): pointer to lane 1 input data
#   arg4 (rcx): pointer to lane 2 input data
#   arg5 (r8):  pointer to lane 3 input data
#   arg6 (r9):  input length in bytes (must be same for all lanes)
# Returns: void
# Note: Input is XORed into state and Keccak permutation is applied for each rate-sized block
.globl  SHA3_shake256_x4_inc_absorb_avx512vl
.type   SHA3_shake256_x4_inc_absorb_avx512vl,\@function,6
.align  32
SHA3_shake256_x4_inc_absorb_avx512vl:
.L_SHA3_shake256_x4_inc_absorb_avx512vl:
.cfi_startproc
    push    %rbp
.cfi_push       %rbp
    push    %rbx
.cfi_push       %rbx
    push    %r12
.cfi_push       %r12
    push    %r13
.cfi_push       %r13
    push    %r14
.cfi_push       %r14
    push    %r15
.cfi_push       %r15
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

.Lshake256_absorb_body:
    # check for partially processed block
    mov     8*100($arg1), %r14
    or      %r14, %r14 # s[100] == 0?
    je      .Lshake256_absorb_main_loop_start

    # process remaining bytes if message long enough
    mov     \$136, %r12 # SHAKE256_RATE = 136
    sub     %r14, %r12  # %r12 = capacity

    cmp     %r12, $arg6 # if mlen <= capacity then no permute
    jbe     .Lshake256_absorb_skip_permute

    sub     %r12, $arg6
    mov     $arg6, %r11 # preserve remaining length across helper calls

    # r10/state, arg2-arg5/inputs, r12/length
    mov     $arg1, %r10                # %r10 = state
    call    keccak_1600_partial_add_x4 # arg2-arg5 are updated

    call    keccak_1600_load_state_x4

    call    keccak_1600_permute

    movq    \$0, 8*100($arg1) # clear s[100]
    jmp     .Lshake256_absorb_partial_block_done

.Lshake256_absorb_skip_permute:
    # r10/state, arg2-arg5/inputs, r12/length
    mov     $arg1, %r10
    mov     $arg6, %r12
    mov     $arg6, %r11 # preserve input length across helper call
    call    keccak_1600_partial_add_x4

    lea     (%r11,%r14), %r15
    mov     %r15, 8*100($arg1) # s[100] += inlen

    cmp     \$136, %r15 # check s[100] below SHAKE256_RATE
    jb      .Lshake256_absorb_exit

    call    keccak_1600_load_state_x4

    call    keccak_1600_permute

    call    keccak_1600_save_state_x4

    movq    \$0, 8*100($arg1) # clear s[100]
    jmp     .Lshake256_absorb_exit

.Lshake256_absorb_main_loop_start:
    call    keccak_1600_load_state_x4
    mov     $arg6, %r11 # full input length when no prior partial block

.Lshake256_absorb_partial_block_done:
    xor     %r12, %r12  # zero message offset

    # Process the input message in blocks
.align  32
.Lshake256_absorb_while_loop:
    cmp     \$136, %r11 # compare mlen to SHAKE256_RATE
    jb      .Lshake256_absorb_while_loop_done

    # Inline absorb_bytes_x4 for SHAKE256_RATE (136 bytes = 17 ymm registers)
___

# Generate absorb code for SHAKE256 rate (136 bytes)
for (my $i = 0; $i < 17; $i++) {
    my $offset = $i * 8;
    $code.=<<___;
        vmovq       $offset($arg2,%r12), %xmm31
        vpinsrq     \$1, $offset($arg3,%r12), %xmm31, %xmm31
        vmovq       $offset($arg4,%r12), %xmm30
        vpinsrq     \$1, $offset($arg5,%r12), %xmm30, %xmm30
        vinserti32x4 \$1, %xmm30, %ymm31, %ymm31
        vpxorq      %ymm31, %ymm$i, %ymm$i
___
}

$code.=<<___;
    sub     \$136, %r11         # Subtract the rate from the remaining length
    add     \$136, %r12         # Adjust offset to next block
    call    keccak_1600_permute # Perform the Keccak permutation

    jmp     .Lshake256_absorb_while_loop

.align  32
.Lshake256_absorb_while_loop_done:
    call    keccak_1600_save_state_x4

    mov     %r11, 8*100($arg1) # update s[100]
    or      %r11, %r11
    jz      .Lshake256_absorb_exit

    movq    \$0, 8*100($arg1) # clear s[100]

    # r10/state, arg2-arg5/input, r12/length
    mov     $arg1, %r10
    add     %r12, $arg2
    add     %r12, $arg3
    add     %r12, $arg4
    add     %r12, $arg5
    mov     %r11, %r12
    call    keccak_1600_partial_add_x4

    mov     %r11, 8*100($arg1) # update s[100]

.Lshake256_absorb_exit:
    # Clear sensitive registers
    vpxorq      %xmm16, %xmm16, %xmm16
    vmovdqa64   %ymm16, %ymm17
    vmovdqa64   %ymm16, %ymm18
    vmovdqa64   %ymm16, %ymm19
    vmovdqa64   %ymm16, %ymm20
    vmovdqa64   %ymm16, %ymm21
    vmovdqa64   %ymm16, %ymm22
    vmovdqa64   %ymm16, %ymm23
    vmovdqa64   %ymm16, %ymm24
    vmovdqa64   %ymm16, %ymm25
    vmovdqa64   %ymm16, %ymm26
    vmovdqa64   %ymm16, %ymm27
    vmovdqa64   %ymm16, %ymm28
    vmovdqa64   %ymm16, %ymm29
    vmovdqa64   %ymm16, %ymm30
    vmovdqa64   %ymm16, %ymm31
.Lshake256_absorb_epilogue:
    vzeroall
___
$code .= <<___ if ($win64);
    vmovups 0(%rsp),   %xmm6
    vmovups 16(%rsp),  %xmm7
    vmovups 32(%rsp),  %xmm8
    vmovups 48(%rsp),  %xmm9
    vmovups 64(%rsp),  %xmm10
    vmovups 80(%rsp),  %xmm11
    vmovups 96(%rsp),  %xmm12
    vmovups 112(%rsp), %xmm13
    vmovups 128(%rsp), %xmm14
    vmovups 144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;

    pop %r15
.cfi_pop    %r15
    pop %r14
.cfi_pop    %r14
    pop %r13
.cfi_pop    %r13
    pop %r12
.cfi_pop    %r12
    pop %rbx
.cfi_pop    %rbx
    pop %rbp
.cfi_pop    %rbp
    ret
.cfi_endproc
.size   SHA3_shake256_x4_inc_absorb_avx512vl,.-SHA3_shake256_x4_inc_absorb_avx512vl


# SHA3_shake256_x4_inc_finalize_avx512vl
# Finalize absorption phase for 4 parallel SHAKE-256 states
# Adds padding and terminator bytes and clears the absorb offset
# Arguments:
#   arg1 (rdi): pointer to state context (808 bytes)
# Returns: void
# Note: After this call, state is ready for squeezing output
.globl  SHA3_shake256_x4_inc_finalize_avx512vl
.type   SHA3_shake256_x4_inc_finalize_avx512vl,\@function,1
.align  32
SHA3_shake256_x4_inc_finalize_avx512vl:
.L_SHA3_shake256_x4_inc_finalize_avx512vl:
.cfi_startproc
    mov     8*100($arg1), %r11 # load state offset from s[100]
    mov     %r11, %r10
    and     \$~7, %r10d        # offset to the state register
    and     \$7, %r11d         # offset within the register

    # add EOM byte right after the message
    vmovdqu32   ($arg1,%r10,4), %ymm31
    lea         shake_msg_pad_x4(%rip), %r9
    sub         %r11, %r9
    vmovdqu32   (%r9), %ymm30
    vpxorq      %ymm30, %ymm31, %ymm31
    vmovdqu32   %ymm31, ($arg1,%r10,4)

    # add terminating byte at offset equal to rate - 1 (SHAKE256_RATE = 136)
    vmovdqu32   512($arg1), %ymm31 # 136*4 - 32 = 544 - 32 = 512
    vmovdqa32   shake_terminator_byte_x4(%rip), %ymm30
    vpxorq      %ymm30, %ymm31, %ymm31
    vmovdqu32   %ymm31, 512($arg1)

    movq        \$0, 8*100($arg1) # clear s[100]
    vpxorq      %ymm31, %ymm31, %ymm31
    ret
.cfi_endproc
.size   SHA3_shake256_x4_inc_finalize_avx512vl,.-SHA3_shake256_x4_inc_finalize_avx512vl

___

$code .= <<___ if ($win64);
# Internal Win64 shim for squeeze entry. It establishes xlate-compatible
# unwind state and then jumps to the function entry after the prologue.
# This is required for internal calls since the xlate ABI conversion
# is already done in the caller function.
.type   SHA3_shake256_x4_inc_squeeze_avx512vl_internal,\@abi-omnipotent
.align  32
.LSEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
    mov     %rsp, %rax
    mov     $arg1, 8(%rsp)
    mov     $arg2, 16(%rsp)
    jmp     .L_SHA3_shake256_x4_inc_squeeze_avx512vl
.LSEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
.size   SHA3_shake256_x4_inc_squeeze_avx512vl_internal,.-SHA3_shake256_x4_inc_squeeze_avx512vl_internal
___
$code.=<<___;

# SHA3_shake256_x4_inc_squeeze_avx512vl
# Squeeze output from 4 parallel SHAKE256 states
# Arguments:
#   arg1 (rdi): pointer to lane 0 output buffer
#   arg2 (rsi): pointer to lane 1 output buffer
#   arg3 (rdx): pointer to lane 2 output buffer
#   arg4 (rcx): pointer to lane 3 output buffer
#   arg5 (r8):  output length in bytes (must be same for all lanes)
#   arg6 (r9):  pointer to state context (808 bytes)
# Returns: void
# Note: Can be called multiple times to generate arbitrary-length output
.globl  SHA3_shake256_x4_inc_squeeze_avx512vl
.type   SHA3_shake256_x4_inc_squeeze_avx512vl,\@function,6
.align  32
SHA3_shake256_x4_inc_squeeze_avx512vl:
.L_SHA3_shake256_x4_inc_squeeze_avx512vl:
.cfi_startproc
    push    %rbp
.cfi_push       %rbp
    push    %rbx
.cfi_push       %rbx
    push    %r12
.cfi_push       %r12
    push    %r13
.cfi_push       %r13
    push    %r14
.cfi_push       %r14
    push    %r15
.cfi_push       %r15
___
$code .= <<___ if ($win64);
    sub     \$160, %rsp
    vmovups %xmm6,   0(%rsp)
    vmovups %xmm7,   16(%rsp)
    vmovups %xmm8,   32(%rsp)
    vmovups %xmm9,   48(%rsp)
    vmovups %xmm10,  64(%rsp)
    vmovups %xmm11,  80(%rsp)
    vmovups %xmm12,  96(%rsp)
    vmovups %xmm13,  112(%rsp)
    vmovups %xmm14,  128(%rsp)
    vmovups %xmm15,  144(%rsp)
___
$code.=<<___;

.Lshake256_squeeze_body:
    or      $arg5, $arg5
    jz      .Lshake256_squeeze_done

    # check for partially processed block
    mov     8*100($arg6), %r15 # s[100] - capacity
    or      %r15, %r15
    jnz     .Lshake256_squeeze_no_init_permute

    mov     $arg1, %r14
    mov     $arg6, $arg1
    call    keccak_1600_load_state_x4

    mov     %r14, $arg1

    xor     %rbp, %rbp
    jmp     .Lshake256_squeeze_loop

.align  32
.Lshake256_squeeze_no_init_permute:
    # extract bytes: r10 - state/src, arg1-arg4 - output/dst, r12 - length = min(capacity, outlen), r11 - offset
    mov     $arg6, %r10
    mov     $arg6, %r14 # preserve state pointer across extract helper

    mov     %r15, %r12
    cmp     %r15, $arg5
    cmovnae $arg5, %r12 # %r12 = min(capacity, outlen)

    sub     %r12, $arg5 # outlen -= length

    mov     \$136, %r11d # SHAKE256_RATE
    sub     %r15, %r11   # state offset

    sub     %r12, %r15         # capacity -= length
    mov     %r15, 8*100($arg6) # update s[100]

    call    keccak_1600_extract_bytes_x4
    mov     %r14, $arg6        # restore state pointer after helper clobbers

    or      %r15, %r15
    jnz     .Lshake256_squeeze_done # check s[100] not zero

    mov     $arg1, %r13 # preserve arg1
    mov     %r14, $arg1
    call    keccak_1600_load_state_x4

    mov     %r13, $arg1
    xor     %rbp, %rbp

.align  32
.Lshake256_squeeze_loop:
    cmp     \$136, $arg5 # outlen > SHAKE256_RATE
    jb      .Lshake256_squeeze_final_extract

    call    keccak_1600_permute

    # Extract SHAKE256 rate bytes (136 bytes = 17 x 8 bytes) inline
___

# Generate extract code for SHAKE256 rate (136 bytes = 17 ymm registers)
for (my $i = 0; $i < 17; $i++) {
    my $offset = $i * 8;
    $code.=<<___;
        vextracti64x2 \$1, %ymm$i, %xmm31
        vmovq       %xmm$i, $offset($arg1,%rbp)
        vpextrq     \$1, %xmm$i, $offset($arg2,%rbp)
        vmovq       %xmm31, $offset($arg3,%rbp)
        vpextrq     \$1, %xmm31, $offset($arg4,%rbp)
___
}

$code.=<<___;
    add     \$136, %rbp  # dst offset += SHAKE256_RATE
    sub     \$136, $arg5 # outlen -= SHAKE256_RATE
    jmp     .Lshake256_squeeze_loop

.align  32
.Lshake256_squeeze_final_extract:
    or      $arg5, $arg5
    jz      .Lshake256_squeeze_no_end_permute

    # update output pointers
    add     %rbp, $arg1
    add     %rbp, $arg2
    add     %rbp, $arg3
    add     %rbp, $arg4

    mov     \$136, %r15d       # SHAKE256_RATE
    sub     $arg5, %r15
    mov     %r15, 8*100($arg6) # s[100] = capacity

    call    keccak_1600_permute

    mov     $arg1, %r14
    mov     $arg6, $arg1
    call    keccak_1600_save_state_x4

    mov     %r14, $arg1

    # extract bytes: r10 - state/src, arg1-arg4 - output/dst, r12 - length, r11 - offset = 0
    mov     $arg6, %r10
    mov     $arg5, %r12
    xor     %r11, %r11
    call    keccak_1600_extract_bytes_x4

    jmp     .Lshake256_squeeze_done

.Lshake256_squeeze_no_end_permute:
    movq    \$0, 8*100($arg6) # s[100] = 0
    mov     $arg6, $arg1
    call    keccak_1600_save_state_x4

.Lshake256_squeeze_done:
    # Clear sensitive registers
    vpxorq      %xmm16, %xmm16, %xmm16
    vmovdqa64   %ymm16, %ymm17
    vmovdqa64   %ymm16, %ymm18
    vmovdqa64   %ymm16, %ymm19
    vmovdqa64   %ymm16, %ymm20
    vmovdqa64   %ymm16, %ymm21
    vmovdqa64   %ymm16, %ymm22
    vmovdqa64   %ymm16, %ymm23
    vmovdqa64   %ymm16, %ymm24
    vmovdqa64   %ymm16, %ymm25
    vmovdqa64   %ymm16, %ymm26
    vmovdqa64   %ymm16, %ymm27
    vmovdqa64   %ymm16, %ymm28
    vmovdqa64   %ymm16, %ymm29
    vmovdqa64   %ymm16, %ymm30
    vmovdqa64   %ymm16, %ymm31
.Lshake256_squeeze_epilogue:
    vzeroall
___
$code .= <<___ if ($win64);
    vmovups 0(%rsp),   %xmm6
    vmovups 16(%rsp),  %xmm7
    vmovups 32(%rsp),  %xmm8
    vmovups 48(%rsp),  %xmm9
    vmovups 64(%rsp),  %xmm10
    vmovups 80(%rsp),  %xmm11
    vmovups 96(%rsp),  %xmm12
    vmovups 112(%rsp), %xmm13
    vmovups 128(%rsp), %xmm14
    vmovups 144(%rsp), %xmm15
    add     \$160, %rsp
___
$code.=<<___;

    pop %r15
.cfi_pop    %r15
    pop %r14
.cfi_pop    %r14
    pop %r13
.cfi_pop    %r13
    pop %r12
.cfi_pop    %r12
    pop %rbx
.cfi_pop    %rbx
    pop %rbp
.cfi_pop    %rbp
    ret
.cfi_endproc
.size   SHA3_shake256_x4_inc_squeeze_avx512vl,.-SHA3_shake256_x4_inc_squeeze_avx512vl
___

if ($win64) {
my $context = "%r8";
my $disp    = "%r9";

$code.=<<___;
.extern __imp_RtlVirtualUnwind
.type   keccak_se_handler,\@abi-omnipotent
.align  16
keccak_se_handler:
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

    mov     120($context), %rax # context->Rax = original %rsp from xlate prologue
    mov     248($context), %rbx # context->Rip

    mov     8($disp), %rsi  # disp->ImageBase
    mov     56($disp), %r11 # disp->HandlerData

    mov     0(%r11), %r10d # HandlerData[0]: body label (rva)
    lea     (%rsi,%r10), %r10
    cmp     %r10, %rbx     # Rip < body?
    jb      .Lkeccak_in_prologue

    mov     4(%r11), %r10d # HandlerData[1]: epilogue label (rva)
    lea     (%rsi,%r10), %r10
    cmp     %r10, %rbx     # Rip >= epilogue?
    jae     .Lkeccak_in_epilogue

    # In function body:
    # HandlerData[2]: delta from context->Rsp(body) to original %rsp
    # HandlerData[3]: offset of XMM6 save area from context->Rsp(body), -1 if none
    # HandlerData[4]: number of saved non-volatiles in stack frame layout (2 or 6)
    # HandlerData[5]: delta from context->Rsp(epilogue) to original %rsp
    mov     152($context), %rdx # body rsp
    mov     8(%r11), %r10d
    lea     (%rdx,%r10), %rax   # original rsp
    jmp     .Lkeccak_restore_body_or_epilogue

.Lkeccak_in_epilogue:
    mov     152($context), %rdx # epilogue rsp
    mov     20(%r11), %r10d
    lea     (%rdx,%r10), %rax   # original rsp

.Lkeccak_restore_body_or_epilogue:
    mov     8(%rax), %rcx       # xlate shadow save of original rdi
    mov     16(%rax), %rsi      # xlate shadow save of original rsi
    mov     %rax, 152($context) # context->Rsp = original rsp
    mov     %rsi, 168($context) # context->Rsi
    mov     %rcx, 176($context) # context->Rdi

    mov     16(%r11), %r10d # gpr save count
    cmp     \$6, %r10d
    jne     .Lkeccak_restore_two

    mov     -24(%rax), %r12
    mov     -32(%rax), %r13
    mov     -40(%rax), %r14
    mov     -48(%rax), %r15
    mov     %r12, 216($context) # context->R12
    mov     %r13, 224($context) # context->R13
    mov     %r14, 232($context) # context->R14
    mov     %r15, 240($context) # context->R15

.Lkeccak_restore_two:
    mov     -8(%rax), %rbp
    mov     -16(%rax), %rbx
    mov     %rbp, 160($context) # context->Rbp
    mov     %rbx, 144($context) # context->Rbx

    mov     12(%r11), %r10d # xmm save offset from body rsp
    cmp     \$-1, %r10d
    je      .Lkeccak_in_prologue

    lea     (%rdx,%r10), %rsi   # source = xmm save area
    lea     512($context), %rdi # &context->Xmm6
    mov     \$20, %ecx          # 10 XMM * 2 qwords
    .long   0xa548f3fc          # cld; rep movsq

.Lkeccak_in_prologue:
    mov     8(%rax), %rcx
    mov     16(%rax), %rdx
    mov     %rcx, 176($context) # context->Rdi
    mov     %rdx, 168($context) # context->Rsi
    mov     %rax, 152($context) # context->Rsp = original rsp

    mov     40($disp), %rdi # disp->ContextRecord
    mov     $context, %rsi
    mov     \$154, %ecx     # sizeof(CONTEXT)/8
    .long   0xa548f3fc      # cld; rep movsq

    mov     $disp, %rsi
    xor     %rcx, %rcx     # UNW_FLAG_NHANDLER
    mov     8(%rsi), %rdx  # disp->ImageBase
    mov     0(%rsi), %r8   # disp->ControlPc
    mov     16(%rsi), %r9  # disp->FunctionEntry
    mov     40(%rsi), %r10 # disp->ContextRecord
    lea     56(%rsi), %r11 # &disp->HandlerData
    lea     24(%rsi), %r12 # &disp->EstablisherFrame
    mov     %r10, 32(%rsp)
    mov     %r11, 40(%rsp)
    mov     %r12, 48(%rsp)
    mov     %rcx, 56(%rsp)
    call    *__imp_RtlVirtualUnwind(%rip)

    mov     \$1, %eax # ExceptionContinueSearch
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
.size   keccak_se_handler,.-keccak_se_handler

.section    .pdata
.align  4
    .rva    .LSEH_begin_SHA3_shake128_x4_avx512vl
    .rva    .LSEH_end_SHA3_shake128_x4_avx512vl
    .rva    .LSEH_info_SHA3_shake128_x4_avx512vl
    .rva    .LSEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_end_SHA3_shake128_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_info_SHA3_shake128_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl
    .rva    .LSEH_end_SHA3_shake128_x4_inc_absorb_avx512vl
    .rva    .LSEH_info_SHA3_shake128_x4_inc_absorb_avx512vl
    .rva    .LSEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl
    .rva    .LSEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl
    .rva    .LSEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl
    .rva    .LSEH_begin_SHA3_shake256_x4_avx512vl
    .rva    .LSEH_end_SHA3_shake256_x4_avx512vl
    .rva    .LSEH_info_SHA3_shake256_x4_avx512vl
    .rva    .LSEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_end_SHA3_shake256_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_info_SHA3_shake256_x4_inc_absorb_avx512vl_internal
    .rva    .LSEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl
    .rva    .LSEH_end_SHA3_shake256_x4_inc_absorb_avx512vl
    .rva    .LSEH_info_SHA3_shake256_x4_inc_absorb_avx512vl
    .rva    .LSEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl_internal
    .rva    .LSEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl
    .rva    .LSEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl
    .rva    .LSEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl

.section    .xdata
.align  8
.LSEH_info_SHA3_shake128_x4_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake128_x4_body,.Lshake128_x4_epilogue
    .long   1032,856,2,1032
.LSEH_info_SHA3_shake128_x4_inc_absorb_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake128_absorb_body,.Lshake128_absorb_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake128_x4_inc_absorb_avx512vl_internal:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake128_absorb_body,.Lshake128_absorb_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake128_squeeze_body,.Lshake128_squeeze_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake128_squeeze_body,.Lshake128_squeeze_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake256_x4_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake256_x4_body,.Lshake256_x4_epilogue
    .long   1032,856,2,1032
.LSEH_info_SHA3_shake256_x4_inc_absorb_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake256_absorb_body,.Lshake256_absorb_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake256_x4_inc_absorb_avx512vl_internal:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake256_absorb_body,.Lshake256_absorb_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake256_squeeze_body,.Lshake256_squeeze_epilogue
    .long   208,0,6,208
.LSEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
    .byte   9,0,0,0
    .rva    keccak_se_handler
    .rva    .Lshake256_squeeze_body,.Lshake256_squeeze_epilogue
    .long   208,0,6,208
___
}

$code.=<<___;

.section .rodata align=128
.align  128
.type   iotas,\@object
iotas:
    .quad   0x0000000000000001
    .quad   0x0000000000008082
    .quad   0x800000000000808a
    .quad   0x8000000080008000
    .quad   0x000000000000808b
    .quad   0x0000000080000001
    .quad   0x8000000080008081
    .quad   0x8000000000008009
    .quad   0x000000000000008a
    .quad   0x0000000000000088
    .quad   0x0000000080008009
    .quad   0x000000008000000a
    .quad   0x000000008000808b
    .quad   0x800000000000008b
    .quad   0x8000000000008089
    .quad   0x8000000000008003
    .quad   0x8000000000008002
    .quad   0x8000000000000080
    .quad   0x000000000000800a
    .quad   0x800000008000000a
    .quad   0x8000000080008081
    .quad   0x8000000000008080
    .quad   0x0000000080000001
    .quad   0x8000000080008008
.size   iotas,.-iotas

.align  8
byte_kmask_0_to_7:
    .byte   0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f

.align  32
shake_terminator_byte_x4:
    .byte   0, 0, 0, 0, 0, 0, 0, 0x80
    .byte   0, 0, 0, 0, 0, 0, 0, 0x80
    .byte   0, 0, 0, 0, 0, 0, 0, 0x80
    .byte   0, 0, 0, 0, 0, 0, 0, 0x80

.align  8
    .byte   0, 0, 0, 0, 0, 0, 0, 0
shake_msg_pad_x4:
    .byte   0x1F, 0, 0, 0, 0, 0, 0, 0
    .byte   0x1F, 0, 0, 0, 0, 0, 0, 0
    .byte   0x1F, 0, 0, 0, 0, 0, 0, 0
    .byte   0x1F, 0, 0, 0, 0, 0, 0, 0

.asciz  "Keccak-1600 absorb and squeeze for AVX512VL, CRYPTOGAMS by <appro\@openssl.org>"
___

}}} else {{{

# When AVX512VL is not available, output stub functions
# The capable function returns 0, and the operation functions are not defined (will use C fallback)

$code .= <<___;
.text

.globl  SHA3_avx512vl_capable
.type   SHA3_avx512vl_capable,\@abi-omnipotent
SHA3_avx512vl_capable:
    xor     %eax, %eax
    ret
.size   SHA3_avx512vl_capable, .-SHA3_avx512vl_capable

.globl  SHA3_shake128_x4_inc_absorb_avx512vl
.globl  SHA3_shake256_x4_inc_absorb_avx512vl
.globl  SHA3_shake128_x4_inc_finalize_avx512vl
.globl  SHA3_shake256_x4_inc_finalize_avx512vl
.globl  SHA3_shake128_x4_inc_squeeze_avx512vl
.globl  SHA3_shake256_x4_inc_squeeze_avx512vl
.globl  SHA3_shake128_x4_avx512vl
.globl  SHA3_shake256_x4_avx512vl
.type   SHA3_shake128_x4_inc_absorb_avx512vl,\@abi-omnipotent
SHA3_shake128_x4_inc_absorb_avx512vl:
SHA3_shake256_x4_inc_absorb_avx512vl:
SHA3_shake128_x4_inc_finalize_avx512vl:
SHA3_shake256_x4_inc_finalize_avx512vl:
SHA3_shake128_x4_inc_squeeze_avx512vl:
SHA3_shake256_x4_inc_squeeze_avx512vl:
SHA3_shake128_x4_avx512vl:
SHA3_shake256_x4_avx512vl:
    .byte   0x0f,0x0b # ud2
    ret
.size   SHA3_shake128_x4_inc_absorb_avx512vl, .-SHA3_shake128_x4_inc_absorb_avx512vl
___
}}}

print $code;
close STDOUT or die "error closing STDOUT: $!";
