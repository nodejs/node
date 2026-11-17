#! /usr/bin/env perl
# This file is dual-licensed, meaning that you can use it under your
# choice of either of the following two licenses:
#
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License"). You can obtain
# a copy in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# or
#
# Copyright (c) 2025, Julian Zhu <julian.oerv@isrc.iscas.ac.cn>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# The generated code of this file depends on the following RISC-V extensions:
# - RV64I
# Optional:
# - RISC-V Basic Bit-manipulation extension ('Zbb')

use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin";
use lib "$Bin/../../perlasm";
use riscv;

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
my $output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
my $flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

my $use_zbb = $flavour && $flavour =~ /zbb/i ? 1 : 0;
my $isaext = "_" . ( $use_zbb ? "zbb" : "riscv64" );

$output and open STDOUT,">$output";

my $code=<<___;
.text
___

# Function arguments
my ($CTX, $INP, $LEN, $A, $B, $C, $D) = ("a0", "a1", "a2", "a4", "a5", "a6", "a7");
my ($KT, $T0, $T1, $T2, $lA, $lB, $lC, $lD) = ("a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6");
my ($C1, $C2, $C3, $C4, $C5, $C6, $C7, $C8) = ("s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7");

sub ROUND0 {
    my ($a, $b, $c, $d, $x, $const, $shift, $is_odd) = @_;
    my $code=<<___;
    li $T0, $const
___
    if ($is_odd) {
        $code .= <<___;
        addw $a, $a, $T0
        srli $T0, $x, 32
___
    } else {
        $code .= <<___;
        addw $a, $a, $x
___
    }
    $code .= <<___;
    addw $a, $a, $T0
    xor $T0, $c, $d
    and $T0, $T0, $b
    xor $T0, $T0, $d
    addw $a, $a, $T0
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $a, $a, 32 - $shift]}
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $a, $a, $T1, $T2, 32 - $shift]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    addw $a, $a, $b
___
    return $code;
}

sub ROUND1 {
    my ($a, $b, $c, $d, $x, $const, $shift, $is_odd) = @_;
    my $code=<<___;
    li $T0, $const
___
    if ($is_odd) {
        $code .= <<___;
        addw $a, $a, $T0
        srli $T0, $x, 32
___
    } else {
        $code .= <<___;
        addw $a, $a, $x
___
    }
    $code .= <<___;
    addw $a, $a, $T0
    xor $T0, $b, $c
    and $T0, $T0, $d
    xor $T0, $T0, $c
    addw $a, $a, $T0
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $a, $a, 32 - $shift]}
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $a, $a, $T1, $T2, 32 - $shift]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    addw $a, $a, $b
___
    return $code;
}

sub ROUND2 {
    my ($a, $b, $c, $d, $x, $const, $shift, $is_odd) = @_;
    my $code=<<___;
    li $T0, $const
___
    if ($is_odd) {
        $code .= <<___;
        addw $a, $a, $T0
        srli $T0, $x, 32
___
    } else {
        $code .= <<___;
        addw $a, $a, $x
___
    }
    $code .= <<___;
    addw $a, $a, $T0
    xor $T0, $c, $d
    xor $T0, $T0, $b
    addw $a, $a, $T0
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $a, $a, 32 - $shift]}
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $a, $a, $T1, $T2, 32 - $shift]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    addw $a, $a, $b
___
    return $code;
}

sub ROUND3 {
    my ($a, $b, $c, $d, $x, $const, $shift, $is_odd) = @_;
    my $code=<<___;
    li $T0, $const
___
    if ($is_odd) {
        $code .= <<___;
        addw $a, $a, $T0
        srli $T0, $x, 32
___
    } else {
        $code .= <<___;
        addw $a, $a, $x
___
    }
    $code .= <<___;
    addw $a, $a, $T0
___
    if ($use_zbb) {
        my $orn_part = <<___;
        @{[orn $T0, $b, $d]}
___
        $code .= $orn_part;
    } else { 
        my $orn_part = <<___;
        @{[orn_rv64i $T0, $b, $d]}
___
        $code .= $orn_part;
    }
    $code .= <<___;
    xor $T0, $T0, $c
    addw $a, $a, $T0
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $a, $a, 32 - $shift]}
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $a, $a, $T1, $T2, 32 - $shift]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    addw $a, $a, $b
___
    return $code;
}

################################################################################
# void ossl_md5_block_asm_data_order@{[$isaext]}(MD5_CTX *c, const void *p, size_t num)
$code .= <<___;
.p2align 3
.globl ossl_md5_block_asm_data_order@{[$isaext]}
.type ossl_md5_block_asm_data_order@{[$isaext]},\@function
ossl_md5_block_asm_data_order@{[$isaext]}:

    addi sp, sp, -64

    sd s0, 0(sp)
    sd s1, 8(sp)
    sd s2, 16(sp)
    sd s3, 24(sp)
    sd s4, 32(sp)
    sd s5, 40(sp)
    sd s6, 48(sp)
    sd s7, 56(sp)

    # load ctx
    lw $A, 0($CTX)
    lw $B, 4($CTX)
    lw $C, 8($CTX)
    lw $D, 12($CTX)

L_round_loop:

    ld $C1, 0($INP)
    ld $C2, 8($INP)
    ld $C3, 16($INP)
    ld $C4, 24($INP)
    ld $C5, 32($INP)
    ld $C6, 40($INP)
    ld $C7, 48($INP)
    ld $C8, 56($INP)

    mv $lA, $A
    mv $lB, $B
    mv $lC, $C
    mv $lD, $D

    @{[ROUND0 $A, $B, $C, $D, $C1, 0xd76aa478,  7, 0]}
    @{[ROUND0 $D, $A, $B, $C, $C1, 0xe8c7b756, 12, 1]}
    @{[ROUND0 $C, $D, $A, $B, $C2, 0x242070db, 17, 0]}
    @{[ROUND0 $B, $C, $D, $A, $C2, 0xc1bdceee, 22, 1]}
    @{[ROUND0 $A, $B, $C, $D, $C3, 0xf57c0faf,  7, 0]}
    @{[ROUND0 $D, $A, $B, $C, $C3, 0x4787c62a, 12, 1]}
    @{[ROUND0 $C, $D, $A, $B, $C4, 0xa8304613, 17, 0]}
    @{[ROUND0 $B, $C, $D, $A, $C4, 0xfd469501, 22, 1]}
    @{[ROUND0 $A, $B, $C, $D, $C5, 0x698098d8,  7, 0]}
    @{[ROUND0 $D, $A, $B, $C, $C5, 0x8b44f7af, 12, 1]}
    @{[ROUND0 $C, $D, $A, $B, $C6, 0xffff5bb1, 17, 0]}
    @{[ROUND0 $B, $C, $D, $A, $C6, 0x895cd7be, 22, 1]}
    @{[ROUND0 $A, $B, $C, $D, $C7, 0x6b901122,  7, 0]}
    @{[ROUND0 $D, $A, $B, $C, $C7, 0xfd987193, 12, 1]}
    @{[ROUND0 $C, $D, $A, $B, $C8, 0xa679438e, 17, 0]}
    @{[ROUND0 $B, $C, $D, $A, $C8, 0x49b40821, 22, 1]}

    @{[ROUND1 $A, $B, $C, $D, $C1, 0xf61e2562,  5, 1]}
    @{[ROUND1 $D, $A, $B, $C, $C4, 0xc040b340,  9, 0]}
    @{[ROUND1 $C, $D, $A, $B, $C6, 0x265e5a51, 14, 1]}
    @{[ROUND1 $B, $C, $D, $A, $C1, 0xe9b6c7aa, 20, 0]}
    @{[ROUND1 $A, $B, $C, $D, $C3, 0xd62f105d,  5, 1]}
    @{[ROUND1 $D, $A, $B, $C, $C6, 0x2441453,   9, 0]}
    @{[ROUND1 $C, $D, $A, $B, $C8, 0xd8a1e681, 14, 1]}
    @{[ROUND1 $B, $C, $D, $A, $C3, 0xe7d3fbc8, 20, 0]}
    @{[ROUND1 $A, $B, $C, $D, $C5, 0x21e1cde6,  5, 1]}
    @{[ROUND1 $D, $A, $B, $C, $C8, 0xc33707d6,  9, 0]}
    @{[ROUND1 $C, $D, $A, $B, $C2, 0xf4d50d87, 14, 1]}
    @{[ROUND1 $B, $C, $D, $A, $C5, 0x455a14ed, 20, 0]}
    @{[ROUND1 $A, $B, $C, $D, $C7, 0xa9e3e905,  5, 1]}
    @{[ROUND1 $D, $A, $B, $C, $C2, 0xfcefa3f8,  9, 0]}
    @{[ROUND1 $C, $D, $A, $B, $C4, 0x676f02d9, 14, 1]}
    @{[ROUND1 $B, $C, $D, $A, $C7, 0x8d2a4c8a, 20, 0]}

    @{[ROUND2 $A, $B, $C, $D, $C3, 0xfffa3942,  4, 1]}
    @{[ROUND2 $D, $A, $B, $C, $C5, 0x8771f681, 11, 0]}
    @{[ROUND2 $C, $D, $A, $B, $C6, 0x6d9d6122, 16, 1]}
    @{[ROUND2 $B, $C, $D, $A, $C8, 0xfde5380c, 23, 0]}
    @{[ROUND2 $A, $B, $C, $D, $C1, 0xa4beea44,  4, 1]}
    @{[ROUND2 $D, $A, $B, $C, $C3, 0x4bdecfa9, 11, 0]}
    @{[ROUND2 $C, $D, $A, $B, $C4, 0xf6bb4b60, 16, 1]}
    @{[ROUND2 $B, $C, $D, $A, $C6, 0xbebfbc70, 23, 0]}
    @{[ROUND2 $A, $B, $C, $D, $C7, 0x289b7ec6,  4, 1]}
    @{[ROUND2 $D, $A, $B, $C, $C1, 0xeaa127fa, 11, 0]}
    @{[ROUND2 $C, $D, $A, $B, $C2, 0xd4ef3085, 16, 1]}
    @{[ROUND2 $B, $C, $D, $A, $C4, 0x4881d05,  23, 0]}
    @{[ROUND2 $A, $B, $C, $D, $C5, 0xd9d4d039,  4, 1]}
    @{[ROUND2 $D, $A, $B, $C, $C7, 0xe6db99e5, 11, 0]}
    @{[ROUND2 $C, $D, $A, $B, $C8, 0x1fa27cf8, 16, 1]}
    @{[ROUND2 $B, $C, $D, $A, $C2, 0xc4ac5665, 23, 0]}

    @{[ROUND3 $A, $B, $C, $D, $C1, 0xf4292244,  6, 0]}
    @{[ROUND3 $D, $A, $B, $C, $C4, 0x432aff97, 10, 1]}
    @{[ROUND3 $C, $D, $A, $B, $C8, 0xab9423a7, 15, 0]}
    @{[ROUND3 $B, $C, $D, $A, $C3, 0xfc93a039, 21, 1]}
    @{[ROUND3 $A, $B, $C, $D, $C7, 0x655b59c3,  6, 0]}
    @{[ROUND3 $D, $A, $B, $C, $C2, 0x8f0ccc92, 10, 1]}
    @{[ROUND3 $C, $D, $A, $B, $C6, 0xffeff47d, 15, 0]}
    @{[ROUND3 $B, $C, $D, $A, $C1, 0x85845dd1, 21, 1]}
    @{[ROUND3 $A, $B, $C, $D, $C5, 0x6fa87e4f,  6, 0]}
    @{[ROUND3 $D, $A, $B, $C, $C8, 0xfe2ce6e0, 10, 1]}
    @{[ROUND3 $C, $D, $A, $B, $C4, 0xa3014314, 15, 0]}
    @{[ROUND3 $B, $C, $D, $A, $C7, 0x4e0811a1, 21, 1]}
    @{[ROUND3 $A, $B, $C, $D, $C3, 0xf7537e82,  6, 0]}
    @{[ROUND3 $D, $A, $B, $C, $C6, 0xbd3af235, 10, 1]}
    @{[ROUND3 $C, $D, $A, $B, $C2, 0x2ad7d2bb, 15, 0]}
    @{[ROUND3 $B, $C, $D, $A, $C5, 0xeb86d391, 21, 1]}

    addw $A, $A, $lA
    addw $B, $B, $lB
    addw $C, $C, $lC
    addw $D, $D, $lD

    addi $INP, $INP, 64

    addi $LEN, $LEN, -1

    bnez $LEN, L_round_loop

    sw $A, 0($CTX)
    sw $B, 4($CTX)
    sw $C, 8($CTX)
    sw $D, 12($CTX)

    ld s0, 0(sp)
    ld s1, 8(sp)
    ld s2, 16(sp)
    ld s3, 24(sp)
    ld s4, 32(sp)
    ld s5, 40(sp)
    ld s6, 48(sp)
    ld s7, 56(sp)

    addi sp, sp, 64

    ret
.size ossl_md5_block_asm_data_order@{[$isaext]},.-ossl_md5_block_asm_data_order@{[$isaext]}
___

print $code;

close STDOUT or die "error closing STDOUT: $!";
