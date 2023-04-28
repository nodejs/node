# Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2020, Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
#
# Originally written by Ilya Albrekht, Sergey Kirillov and Andrey Matyukov
# Intel Corporation
#
# December 2020
#
# Initial release.
#
# Implementation utilizes 256-bit (ymm) registers to avoid frequency scaling issues.
#
# IceLake-Client @ 1.3GHz
# |---------+----------------------+--------------+-------------|
# |         | OpenSSL 3.0.0-alpha9 | this         | Unit        |
# |---------+----------------------+--------------+-------------|
# | rsa2048 | 2 127 659            | 1 015 625    | cycles/sign |
# |         | 611                  | 1280 / +109% | sign/s      |
# |---------+----------------------+--------------+-------------|
#

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);
$avx512ifma=0;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
        =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
    $avx512ifma = ($1>=2.26);
}

if (!$avx512 && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
       `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/) {
    $avx512ifma = ($1==2.11 && $2>=8) + ($1>=2.12);
}

if (!$avx512 && `$ENV{CC} -v 2>&1`
    =~ /(Apple)?\s*((?:clang|LLVM) version|.*based on LLVM) ([0-9]+)\.([0-9]+)\.([0-9]+)?/) {
    my $ver = $3 + $4/100.0 + $5/10000.0; # 3.1.0->3.01, 3.10.1->3.1001
    if ($1) {
        # Apple conditions, they use a different version series, see
        # https://en.wikipedia.org/wiki/Xcode#Xcode_7.0_-_10.x_(since_Free_On-Device_Development)_2
        # clang 7.0.0 is Apple clang 10.0.1
        $avx512ifma = ($ver>=10.0001)
    } else {
        $avx512ifma = ($3>=7.0);
    }
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

if ($avx512ifma>0) {{{
@_6_args_universal_ABI = ("%rdi","%rsi","%rdx","%rcx","%r8","%r9");

$code.=<<___;
.extern OPENSSL_ia32cap_P
.globl  ossl_rsaz_avx512ifma_eligible
.type   ossl_rsaz_avx512ifma_eligible,\@abi-omnipotent
.align  32
ossl_rsaz_avx512ifma_eligible:
    mov OPENSSL_ia32cap_P+8(%rip), %ecx
    xor %eax,%eax
    and \$`1<<31|1<<21|1<<17|1<<16`, %ecx     # avx512vl + avx512ifma + avx512dq + avx512f
    cmp \$`1<<31|1<<21|1<<17|1<<16`, %ecx
    cmove %ecx,%eax
    ret
.size   ossl_rsaz_avx512ifma_eligible, .-ossl_rsaz_avx512ifma_eligible
___

###############################################################################
# Almost Montgomery Multiplication (AMM) for 20-digit number in radix 2^52.
#
# AMM is defined as presented in the paper
# "Efficient Software Implementations of Modular Exponentiation" by Shay Gueron.
#
# The input and output are presented in 2^52 radix domain, i.e.
#   |res|, |a|, |b|, |m| are arrays of 20 64-bit qwords with 12 high bits zeroed.
#   |k0| is a Montgomery coefficient, which is here k0 = -1/m mod 2^64
#        (note, the implementation counts only 52 bits from it).
#
# NB: the AMM implementation does not perform "conditional" subtraction step as
# specified in the original algorithm as according to the paper "Enhanced Montgomery
# Multiplication" by Shay Gueron (see Lemma 1), the result will be always < 2*2^1024
# and can be used as a direct input to the next AMM iteration.
# This post-condition is true, provided the correct parameter |s| is choosen, i.e.
# s >= n + 2 * k, which matches our case: 1040 > 1024 + 2 * 1.
#
# void ossl_rsaz_amm52x20_x1_256(BN_ULONG *res,
#                           const BN_ULONG *a,
#                           const BN_ULONG *b,
#                           const BN_ULONG *m,
#                           BN_ULONG k0);
###############################################################################
{
# input parameters ("%rdi","%rsi","%rdx","%rcx","%r8")
my ($res,$a,$b,$m,$k0) = @_6_args_universal_ABI;

my $mask52     = "%rax";
my $acc0_0     = "%r9";
my $acc0_0_low = "%r9d";
my $acc0_1     = "%r15";
my $acc0_1_low = "%r15d";
my $b_ptr      = "%r11";

my $iter = "%ebx";

my $zero = "%ymm0";
my ($R0_0,$R0_0h,$R1_0,$R1_0h,$R2_0) = ("%ymm1", map("%ymm$_",(16..19)));
my ($R0_1,$R0_1h,$R1_1,$R1_1h,$R2_1) = ("%ymm2", map("%ymm$_",(20..23)));
my $Bi = "%ymm3";
my $Yi = "%ymm4";

# Registers mapping for normalization.
# We can reuse Bi, Yi registers here.
my $TMP = $Bi;
my $mask52x4 = $Yi;
my ($T0,$T0h,$T1,$T1h,$T2) = map("%ymm$_", (24..28));

sub amm52x20_x1() {
# _data_offset - offset in the |a| or |m| arrays pointing to the beginning
#                of data for corresponding AMM operation;
# _b_offset    - offset in the |b| array pointing to the next qword digit;
my ($_data_offset,$_b_offset,$_acc,$_R0,$_R0h,$_R1,$_R1h,$_R2,$_k0) = @_;
my $_R0_xmm = $_R0;
$_R0_xmm =~ s/%y/%x/;
$code.=<<___;
    movq    $_b_offset($b_ptr), %r13             # b[i]

    vpbroadcastq    %r13, $Bi                    # broadcast b[i]
    movq    $_data_offset($a), %rdx
    mulx    %r13, %r13, %r12                     # a[0]*b[i] = (t0,t2)
    addq    %r13, $_acc                          # acc += t0
    movq    %r12, %r10
    adcq    \$0, %r10                            # t2 += CF

    movq    $_k0, %r13
    imulq   $_acc, %r13                          # acc * k0
    andq    $mask52, %r13                        # yi = (acc * k0) & mask52

    vpbroadcastq    %r13, $Yi                    # broadcast y[i]
    movq    $_data_offset($m), %rdx
    mulx    %r13, %r13, %r12                     # yi * m[0] = (t0,t1)
    addq    %r13, $_acc                          # acc += t0
    adcq    %r12, %r10                           # t2 += (t1 + CF)

    shrq    \$52, $_acc
    salq    \$12, %r10
    or      %r10, $_acc                          # acc = ((acc >> 52) | (t2 << 12))

    vpmadd52luq `$_data_offset+64*0`($a), $Bi, $_R0
    vpmadd52luq `$_data_offset+64*0+32`($a), $Bi, $_R0h
    vpmadd52luq `$_data_offset+64*1`($a), $Bi, $_R1
    vpmadd52luq `$_data_offset+64*1+32`($a), $Bi, $_R1h
    vpmadd52luq `$_data_offset+64*2`($a), $Bi, $_R2

    vpmadd52luq `$_data_offset+64*0`($m), $Yi, $_R0
    vpmadd52luq `$_data_offset+64*0+32`($m), $Yi, $_R0h
    vpmadd52luq `$_data_offset+64*1`($m), $Yi, $_R1
    vpmadd52luq `$_data_offset+64*1+32`($m), $Yi, $_R1h
    vpmadd52luq `$_data_offset+64*2`($m), $Yi, $_R2

    # Shift accumulators right by 1 qword, zero extending the highest one
    valignq     \$1, $_R0, $_R0h, $_R0
    valignq     \$1, $_R0h, $_R1, $_R0h
    valignq     \$1, $_R1, $_R1h, $_R1
    valignq     \$1, $_R1h, $_R2, $_R1h
    valignq     \$1, $_R2, $zero, $_R2

    vmovq   $_R0_xmm, %r13
    addq    %r13, $_acc    # acc += R0[0]

    vpmadd52huq `$_data_offset+64*0`($a), $Bi, $_R0
    vpmadd52huq `$_data_offset+64*0+32`($a), $Bi, $_R0h
    vpmadd52huq `$_data_offset+64*1`($a), $Bi, $_R1
    vpmadd52huq `$_data_offset+64*1+32`($a), $Bi, $_R1h
    vpmadd52huq `$_data_offset+64*2`($a), $Bi, $_R2

    vpmadd52huq `$_data_offset+64*0`($m), $Yi, $_R0
    vpmadd52huq `$_data_offset+64*0+32`($m), $Yi, $_R0h
    vpmadd52huq `$_data_offset+64*1`($m), $Yi, $_R1
    vpmadd52huq `$_data_offset+64*1+32`($m), $Yi, $_R1h
    vpmadd52huq `$_data_offset+64*2`($m), $Yi, $_R2
___
}

# Normalization routine: handles carry bits in R0..R2 QWs and
# gets R0..R2 back to normalized 2^52 representation.
#
# Uses %r8-14,%e[bcd]x
sub amm52x20_x1_norm {
my ($_acc,$_R0,$_R0h,$_R1,$_R1h,$_R2) = @_;
$code.=<<___;
    # Put accumulator to low qword in R0
    vpbroadcastq    $_acc, $TMP
    vpblendd \$3, $TMP, $_R0, $_R0

    # Extract "carries" (12 high bits) from each QW of R0..R2
    # Save them to LSB of QWs in T0..T2
    vpsrlq    \$52, $_R0,   $T0
    vpsrlq    \$52, $_R0h,  $T0h
    vpsrlq    \$52, $_R1,   $T1
    vpsrlq    \$52, $_R1h,  $T1h
    vpsrlq    \$52, $_R2,   $T2

    # "Shift left" T0..T2 by 1 QW
    valignq \$3, $T1h,  $T2,  $T2
    valignq \$3, $T1,   $T1h, $T1h
    valignq \$3, $T0h,  $T1,  $T1
    valignq \$3, $T0,   $T0h, $T0h
    valignq \$3, $zero, $T0,  $T0

    # Drop "carries" from R0..R2 QWs
    vpandq    $mask52x4, $_R0,  $_R0
    vpandq    $mask52x4, $_R0h, $_R0h
    vpandq    $mask52x4, $_R1,  $_R1
    vpandq    $mask52x4, $_R1h, $_R1h
    vpandq    $mask52x4, $_R2,  $_R2

    # Sum R0..R2 with corresponding adjusted carries
    vpaddq  $T0,  $_R0,  $_R0
    vpaddq  $T0h, $_R0h, $_R0h
    vpaddq  $T1,  $_R1,  $_R1
    vpaddq  $T1h, $_R1h, $_R1h
    vpaddq  $T2,  $_R2,  $_R2

    # Now handle carry bits from this addition
    # Get mask of QWs which 52-bit parts overflow...
    vpcmpuq   \$1, $_R0,  $mask52x4, %k1 # OP=lt
    vpcmpuq   \$1, $_R0h, $mask52x4, %k2
    vpcmpuq   \$1, $_R1,  $mask52x4, %k3
    vpcmpuq   \$1, $_R1h, $mask52x4, %k4
    vpcmpuq   \$1, $_R2,  $mask52x4, %k5
    kmovb   %k1, %r14d                   # k1
    kmovb   %k2, %r13d                   # k1h
    kmovb   %k3, %r12d                   # k2
    kmovb   %k4, %r11d                   # k2h
    kmovb   %k5, %r10d                   # k3

    # ...or saturated
    vpcmpuq   \$0, $_R0,  $mask52x4, %k1 # OP=eq
    vpcmpuq   \$0, $_R0h, $mask52x4, %k2
    vpcmpuq   \$0, $_R1,  $mask52x4, %k3
    vpcmpuq   \$0, $_R1h, $mask52x4, %k4
    vpcmpuq   \$0, $_R2,  $mask52x4, %k5
    kmovb   %k1, %r9d                    # k4
    kmovb   %k2, %r8d                    # k4h
    kmovb   %k3, %ebx                    # k5
    kmovb   %k4, %ecx                    # k5h
    kmovb   %k5, %edx                    # k6

    # Get mask of QWs where carries shall be propagated to.
    # Merge 4-bit masks to 8-bit values to use add with carry.
    shl   \$4, %r13b
    or    %r13b, %r14b
    shl   \$4, %r11b
    or    %r11b, %r12b

    add   %r14b, %r14b
    adc   %r12b, %r12b
    adc   %r10b, %r10b

    shl   \$4, %r8b
    or    %r8b,%r9b
    shl   \$4, %cl
    or    %cl, %bl

    add   %r9b, %r14b
    adc   %bl, %r12b
    adc   %dl, %r10b

    xor   %r9b, %r14b
    xor   %bl, %r12b
    xor   %dl, %r10b

    kmovb   %r14d, %k1
    shr     \$4, %r14b
    kmovb   %r14d, %k2
    kmovb   %r12d, %k3
    shr     \$4, %r12b
    kmovb   %r12d, %k4
    kmovb   %r10d, %k5

    # Add carries according to the obtained mask
    vpsubq  $mask52x4, $_R0,  ${_R0}{%k1}
    vpsubq  $mask52x4, $_R0h, ${_R0h}{%k2}
    vpsubq  $mask52x4, $_R1,  ${_R1}{%k3}
    vpsubq  $mask52x4, $_R1h, ${_R1h}{%k4}
    vpsubq  $mask52x4, $_R2,  ${_R2}{%k5}

    vpandq   $mask52x4, $_R0,  $_R0
    vpandq   $mask52x4, $_R0h, $_R0h
    vpandq   $mask52x4, $_R1,  $_R1
    vpandq   $mask52x4, $_R1h, $_R1h
    vpandq   $mask52x4, $_R2,  $_R2
___
}

$code.=<<___;
.text

.globl  ossl_rsaz_amm52x20_x1_256
.type   ossl_rsaz_amm52x20_x1_256,\@function,5
.align 32
ossl_rsaz_amm52x20_x1_256:
.cfi_startproc
    endbranch
    push    %rbx
.cfi_push   %rbx
    push    %rbp
.cfi_push   %rbp
    push    %r12
.cfi_push   %r12
    push    %r13
.cfi_push   %r13
    push    %r14
.cfi_push   %r14
    push    %r15
.cfi_push   %r15
.Lrsaz_amm52x20_x1_256_body:

    # Zeroing accumulators
    vpxord   $zero, $zero, $zero
    vmovdqa64   $zero, $R0_0
    vmovdqa64   $zero, $R0_0h
    vmovdqa64   $zero, $R1_0
    vmovdqa64   $zero, $R1_0h
    vmovdqa64   $zero, $R2_0

    xorl    $acc0_0_low, $acc0_0_low

    movq    $b, $b_ptr                       # backup address of b
    movq    \$0xfffffffffffff, $mask52       # 52-bit mask

    # Loop over 20 digits unrolled by 4
    mov     \$5, $iter

.align 32
.Lloop5:
___
    foreach my $idx (0..3) {
        &amm52x20_x1(0,8*$idx,$acc0_0,$R0_0,$R0_0h,$R1_0,$R1_0h,$R2_0,$k0);
    }
$code.=<<___;
    lea    `4*8`($b_ptr), $b_ptr
    dec    $iter
    jne    .Lloop5

    vmovdqa64   .Lmask52x4(%rip), $mask52x4
___
    &amm52x20_x1_norm($acc0_0,$R0_0,$R0_0h,$R1_0,$R1_0h,$R2_0);
$code.=<<___;

    vmovdqu64   $R0_0, ($res)
    vmovdqu64   $R0_0h, 32($res)
    vmovdqu64   $R1_0, 64($res)
    vmovdqu64   $R1_0h, 96($res)
    vmovdqu64   $R2_0, 128($res)

    vzeroupper
    mov  0(%rsp),%r15
.cfi_restore    %r15
    mov  8(%rsp),%r14
.cfi_restore    %r14
    mov  16(%rsp),%r13
.cfi_restore    %r13
    mov  24(%rsp),%r12
.cfi_restore    %r12
    mov  32(%rsp),%rbp
.cfi_restore    %rbp
    mov  40(%rsp),%rbx
.cfi_restore    %rbx
    lea  48(%rsp),%rsp
.cfi_adjust_cfa_offset  -48
.Lrsaz_amm52x20_x1_256_epilogue:
    ret
.cfi_endproc
.size   ossl_rsaz_amm52x20_x1_256, .-ossl_rsaz_amm52x20_x1_256
___

$code.=<<___;
.data
.align 32
.Lmask52x4:
    .quad   0xfffffffffffff
    .quad   0xfffffffffffff
    .quad   0xfffffffffffff
    .quad   0xfffffffffffff
___

###############################################################################
# Dual Almost Montgomery Multiplication for 20-digit number in radix 2^52
#
# See description of ossl_rsaz_amm52x20_x1_256() above for details about Almost
# Montgomery Multiplication algorithm and function input parameters description.
#
# This function does two AMMs for two independent inputs, hence dual.
#
# void ossl_rsaz_amm52x20_x2_256(BN_ULONG out[2][20],
#                           const BN_ULONG a[2][20],
#                           const BN_ULONG b[2][20],
#                           const BN_ULONG m[2][20],
#                           const BN_ULONG k0[2]);
###############################################################################

$code.=<<___;
.text

.globl  ossl_rsaz_amm52x20_x2_256
.type   ossl_rsaz_amm52x20_x2_256,\@function,5
.align 32
ossl_rsaz_amm52x20_x2_256:
.cfi_startproc
    endbranch
    push    %rbx
.cfi_push   %rbx
    push    %rbp
.cfi_push   %rbp
    push    %r12
.cfi_push   %r12
    push    %r13
.cfi_push   %r13
    push    %r14
.cfi_push   %r14
    push    %r15
.cfi_push   %r15
.Lrsaz_amm52x20_x2_256_body:

    # Zeroing accumulators
    vpxord   $zero, $zero, $zero
    vmovdqa64   $zero, $R0_0
    vmovdqa64   $zero, $R0_0h
    vmovdqa64   $zero, $R1_0
    vmovdqa64   $zero, $R1_0h
    vmovdqa64   $zero, $R2_0
    vmovdqa64   $zero, $R0_1
    vmovdqa64   $zero, $R0_1h
    vmovdqa64   $zero, $R1_1
    vmovdqa64   $zero, $R1_1h
    vmovdqa64   $zero, $R2_1

    xorl    $acc0_0_low, $acc0_0_low
    xorl    $acc0_1_low, $acc0_1_low

    movq    $b, $b_ptr                       # backup address of b
    movq    \$0xfffffffffffff, $mask52       # 52-bit mask

    mov    \$20, $iter

.align 32
.Lloop20:
___
    &amm52x20_x1(   0,   0,$acc0_0,$R0_0,$R0_0h,$R1_0,$R1_0h,$R2_0,"($k0)");
    # 20*8 = offset of the next dimension in two-dimension array
    &amm52x20_x1(20*8,20*8,$acc0_1,$R0_1,$R0_1h,$R1_1,$R1_1h,$R2_1,"8($k0)");
$code.=<<___;
    lea    8($b_ptr), $b_ptr
    dec    $iter
    jne    .Lloop20

    vmovdqa64   .Lmask52x4(%rip), $mask52x4
___
    &amm52x20_x1_norm($acc0_0,$R0_0,$R0_0h,$R1_0,$R1_0h,$R2_0);
    &amm52x20_x1_norm($acc0_1,$R0_1,$R0_1h,$R1_1,$R1_1h,$R2_1);
$code.=<<___;

    vmovdqu64   $R0_0, ($res)
    vmovdqu64   $R0_0h, 32($res)
    vmovdqu64   $R1_0, 64($res)
    vmovdqu64   $R1_0h, 96($res)
    vmovdqu64   $R2_0, 128($res)

    vmovdqu64   $R0_1, 160($res)
    vmovdqu64   $R0_1h, 192($res)
    vmovdqu64   $R1_1, 224($res)
    vmovdqu64   $R1_1h, 256($res)
    vmovdqu64   $R2_1, 288($res)

    vzeroupper
    mov  0(%rsp),%r15
.cfi_restore    %r15
    mov  8(%rsp),%r14
.cfi_restore    %r14
    mov  16(%rsp),%r13
.cfi_restore    %r13
    mov  24(%rsp),%r12
.cfi_restore    %r12
    mov  32(%rsp),%rbp
.cfi_restore    %rbp
    mov  40(%rsp),%rbx
.cfi_restore    %rbx
    lea  48(%rsp),%rsp
.cfi_adjust_cfa_offset  -48
.Lrsaz_amm52x20_x2_256_epilogue:
    ret
.cfi_endproc
.size   ossl_rsaz_amm52x20_x2_256, .-ossl_rsaz_amm52x20_x2_256
___
}

###############################################################################
# Constant time extraction from the precomputed table of powers base^i, where
#    i = 0..2^EXP_WIN_SIZE-1
#
# The input |red_table| contains precomputations for two independent base values,
# so the |tbl_idx| indicates for which base shall we extract the value.
# |red_table_idx| is a power index.
#
# Extracted value (output) is 20 digit number in 2^52 radix.
#
# void ossl_extract_multiplier_2x20_win5(BN_ULONG *red_Y,
#                                        const BN_ULONG red_table[1 << EXP_WIN_SIZE][2][20],
#                                        int red_table_idx,
#                                        int tbl_idx);           # 0 or 1
#
# EXP_WIN_SIZE = 5
###############################################################################
{
# input parameters
my ($out,$red_tbl,$red_tbl_idx,$tbl_idx) = @_6_args_universal_ABI;

my ($t0,$t1,$t2,$t3,$t4) = map("%ymm$_", (0..4));
my $t4xmm = $t4;
$t4xmm =~ s/%y/%x/;
my ($tmp0,$tmp1,$tmp2,$tmp3,$tmp4) = map("%ymm$_", (16..20));
my ($cur_idx,$idx,$ones) = map("%ymm$_", (21..23));

$code.=<<___;
.text

.align 32
.globl  ossl_extract_multiplier_2x20_win5
.type   ossl_extract_multiplier_2x20_win5,\@function,4
ossl_extract_multiplier_2x20_win5:
.cfi_startproc
    endbranch
    leaq    ($tbl_idx,$tbl_idx,4), %rax
    salq    \$5, %rax
    addq    %rax, $red_tbl

    vmovdqa64   .Lones(%rip), $ones         # broadcast ones
    vpbroadcastq    $red_tbl_idx, $idx
    leaq   `(1<<5)*2*20*8`($red_tbl), %rax  # holds end of the tbl

    vpxor   $t4xmm, $t4xmm, $t4xmm
    vmovdqa64   $t4, $t3                    # zeroing t0..4, cur_idx
    vmovdqa64   $t4, $t2
    vmovdqa64   $t4, $t1
    vmovdqa64   $t4, $t0
    vmovdqa64   $t4, $cur_idx

.align 32
.Lloop:
    vpcmpq  \$0, $cur_idx, $idx, %k1        # mask of (idx == cur_idx)
    addq    \$320, $red_tbl                 # 320 = 2 * 20 digits * 8 bytes
    vpaddq  $ones, $cur_idx, $cur_idx       # increment cur_idx
    vmovdqu64  -320($red_tbl), $tmp0        # load data from red_tbl
    vmovdqu64  -288($red_tbl), $tmp1
    vmovdqu64  -256($red_tbl), $tmp2
    vmovdqu64  -224($red_tbl), $tmp3
    vmovdqu64  -192($red_tbl), $tmp4
    vpblendmq  $tmp0, $t0, ${t0}{%k1}       # extract data when mask is not zero
    vpblendmq  $tmp1, $t1, ${t1}{%k1}
    vpblendmq  $tmp2, $t2, ${t2}{%k1}
    vpblendmq  $tmp3, $t3, ${t3}{%k1}
    vpblendmq  $tmp4, $t4, ${t4}{%k1}
    cmpq    $red_tbl, %rax
    jne .Lloop

    vmovdqu64   $t0, ($out)                 # store t0..4
    vmovdqu64   $t1, 32($out)
    vmovdqu64   $t2, 64($out)
    vmovdqu64   $t3, 96($out)
    vmovdqu64   $t4, 128($out)

    ret
.cfi_endproc
.size   ossl_extract_multiplier_2x20_win5, .-ossl_extract_multiplier_2x20_win5
___
$code.=<<___;
.data
.align 32
.Lones:
    .quad   1,1,1,1
___
}

if ($win64) {
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";

$code.=<<___
.extern     __imp_RtlVirtualUnwind
.type   rsaz_def_handler,\@abi-omnipotent
.align  16
rsaz_def_handler:
    push    %rsi
    push    %rdi
    push    %rbx
    push    %rbp
    push    %r12
    push    %r13
    push    %r14
    push    %r15
    pushfq
    sub     \$64,%rsp

    mov     120($context),%rax # pull context->Rax
    mov     248($context),%rbx # pull context->Rip

    mov     8($disp),%rsi      # disp->ImageBase
    mov     56($disp),%r11     # disp->HandlerData

    mov     0(%r11),%r10d      # HandlerData[0]
    lea     (%rsi,%r10),%r10   # prologue label
    cmp     %r10,%rbx          # context->Rip<.Lprologue
    jb  .Lcommon_seh_tail

    mov     152($context),%rax # pull context->Rsp

    mov     4(%r11),%r10d      # HandlerData[1]
    lea     (%rsi,%r10),%r10   # epilogue label
    cmp     %r10,%rbx          # context->Rip>=.Lepilogue
    jae     .Lcommon_seh_tail

    lea     48(%rax),%rax

    mov     -8(%rax),%rbx
    mov     -16(%rax),%rbp
    mov     -24(%rax),%r12
    mov     -32(%rax),%r13
    mov     -40(%rax),%r14
    mov     -48(%rax),%r15
    mov     %rbx,144($context) # restore context->Rbx
    mov     %rbp,160($context) # restore context->Rbp
    mov     %r12,216($context) # restore context->R12
    mov     %r13,224($context) # restore context->R13
    mov     %r14,232($context) # restore context->R14
    mov     %r15,240($context) # restore context->R14

.Lcommon_seh_tail:
    mov     8(%rax),%rdi
    mov     16(%rax),%rsi
    mov     %rax,152($context) # restore context->Rsp
    mov     %rsi,168($context) # restore context->Rsi
    mov     %rdi,176($context) # restore context->Rdi

    mov     40($disp),%rdi     # disp->ContextRecord
    mov     $context,%rsi      # context
    mov     \$154,%ecx         # sizeof(CONTEXT)
    .long   0xa548f3fc         # cld; rep movsq

    mov     $disp,%rsi
    xor     %rcx,%rcx          # arg1, UNW_FLAG_NHANDLER
    mov     8(%rsi),%rdx       # arg2, disp->ImageBase
    mov     0(%rsi),%r8        # arg3, disp->ControlPc
    mov     16(%rsi),%r9       # arg4, disp->FunctionEntry
    mov     40(%rsi),%r10      # disp->ContextRecord
    lea     56(%rsi),%r11      # &disp->HandlerData
    lea     24(%rsi),%r12      # &disp->EstablisherFrame
    mov     %r10,32(%rsp)      # arg5
    mov     %r11,40(%rsp)      # arg6
    mov     %r12,48(%rsp)      # arg7
    mov     %rcx,56(%rsp)      # arg8, (NULL)
    call    *__imp_RtlVirtualUnwind(%rip)

    mov     \$1,%eax           # ExceptionContinueSearch
    add     \$64,%rsp
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
.size   rsaz_def_handler,.-rsaz_def_handler

.section    .pdata
.align  4
    .rva    .LSEH_begin_ossl_rsaz_amm52x20_x1_256
    .rva    .LSEH_end_ossl_rsaz_amm52x20_x1_256
    .rva    .LSEH_info_ossl_rsaz_amm52x20_x1_256

    .rva    .LSEH_begin_ossl_rsaz_amm52x20_x2_256
    .rva    .LSEH_end_ossl_rsaz_amm52x20_x2_256
    .rva    .LSEH_info_ossl_rsaz_amm52x20_x2_256

    .rva    .LSEH_begin_ossl_extract_multiplier_2x20_win5
    .rva    .LSEH_end_ossl_extract_multiplier_2x20_win5
    .rva    .LSEH_info_ossl_extract_multiplier_2x20_win5

.section    .xdata
.align  8
.LSEH_info_ossl_rsaz_amm52x20_x1_256:
    .byte   9,0,0,0
    .rva    rsaz_def_handler
    .rva    .Lrsaz_amm52x20_x1_256_body,.Lrsaz_amm52x20_x1_256_epilogue
.LSEH_info_ossl_rsaz_amm52x20_x2_256:
    .byte   9,0,0,0
    .rva    rsaz_def_handler
    .rva    .Lrsaz_amm52x20_x2_256_body,.Lrsaz_amm52x20_x2_256_epilogue
.LSEH_info_ossl_extract_multiplier_2x20_win5:
    .byte   9,0,0,0
    .rva    rsaz_def_handler
    .rva    .LSEH_begin_ossl_extract_multiplier_2x20_win5,.LSEH_begin_ossl_extract_multiplier_2x20_win5
___
}
}}} else {{{                # fallback for old assembler
$code.=<<___;
.text

.globl  ossl_rsaz_avx512ifma_eligible
.type   ossl_rsaz_avx512ifma_eligible,\@abi-omnipotent
ossl_rsaz_avx512ifma_eligible:
    xor     %eax,%eax
    ret
.size   ossl_rsaz_avx512ifma_eligible, .-ossl_rsaz_avx512ifma_eligible

.globl  ossl_rsaz_amm52x20_x1_256
.globl  ossl_rsaz_amm52x20_x2_256
.globl  ossl_extract_multiplier_2x20_win5
.type   ossl_rsaz_amm52x20_x1_256,\@abi-omnipotent
ossl_rsaz_amm52x20_x1_256:
ossl_rsaz_amm52x20_x2_256:
ossl_extract_multiplier_2x20_win5:
    .byte   0x0f,0x0b    # ud2
    ret
.size   ossl_rsaz_amm52x20_x1_256, .-ossl_rsaz_amm52x20_x1_256
___
}}}

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT or die "error closing STDOUT: $!";
