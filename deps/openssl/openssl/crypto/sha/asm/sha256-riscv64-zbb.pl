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

my $K256 = "K256";

# Function arguments
my ($INP, $LEN, $ADDR) = ("a1", "a2", "sp");
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6, $T7, $T8) = ("t0", "t1", "t2", "t3", "t4", "t5", "t6", "a3", "a4");
my ($A, $B, $C, $D ,$E ,$F ,$G ,$H) = ("s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9");

sub MSGSCHEDULE0 {
    my (
        $index,
    ) = @_;
    if ($use_zbb) {
        my $code=<<___;
        lw $T1, (4*$index+0)($INP)
        @{[rev8 $T1, $T1]} # rev8 $T1, $T1
        srli $T1, $T1, 32
        sw $T1, 4*$index($ADDR)
___
        return $code;
    } else {
        my $code=<<___;
        lbu $T1, (4*$index+0)($INP)
        lbu $T2, (4*$index+1)($INP)
        lbu $T3, (4*$index+2)($INP)
        lbu $T4, (4*$index+3)($INP)
        slliw $T1, $T1, 24
        slliw $T2, $T2, 16
        or $T1, $T1, $T2
        slliw $T3, $T3, 8
        or $T1, $T1, $T3
        or $T1, $T1, $T4
        sw $T1, 4*$index($ADDR)
___
        return $code;
    }
}

sub MSGSCHEDULE1 {
    my (
        $INDEX,
    ) = @_;
    my $code=<<___;
    lw $T1, (($INDEX-2)&0x0f)*4($ADDR)
    lw $T2, (($INDEX-15)&0x0f)*4($ADDR)
    lw $T3, (($INDEX-7)&0x0f)*4($ADDR)
    lw $T4, ($INDEX&0x0f)*4($ADDR)
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $T5, $T1, 17]}  # roriw $T5, $T1, 17
        @{[roriw $T6, $T1, 19]}  # roriw $T6, $T1, 19
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $T5, $T1, $T7, $T8, 17]}
        @{[roriw_rv64i $T6, $T1, $T7, $T8, 19]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    srliw $T1, $T1, 10
    xor $T1, $T1, $T5
    xor $T1, $T1, $T6
    addw $T1, $T1, $T3
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $T5, $T2, 7]}  # roriw $T5, $T2, 7
        @{[roriw $T6, $T2, 18]}  # roriw $T6, $T2, 18
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $T5, $T2, $T7, $T8, 7]}
        @{[roriw_rv64i $T6, $T2, $T7, $T8, 18]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    srliw $T2, $T2, 3
    xor $T2, $T2, $T5
    xor $T2, $T2, $T6
    addw $T1, $T1, $T2
    addw $T1, $T1, $T4
    sw $T1, 4*($INDEX&0x0f)($ADDR)
___

    return $code;
}

sub sha256_T1 {
    my (
        $INDEX, $e, $f, $g, $h,
    ) = @_;
    my $code=<<___;
    lw $T4, 4*$INDEX($KT)
    addw $h, $h, $T1
    addw $h, $h, $T4
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $T2, $e, 6]}  # roriw $T2, $e, 6
        @{[roriw $T3, $e, 11]}  # roriw $T3, $e, 11
        @{[roriw $T4, $e, 25]}  # roriw $T4, $e, 25
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $T2, $e, $T7, $T8, 6]}
        @{[roriw_rv64i $T3, $e, $T7, $T8, 11]}
        @{[roriw_rv64i $T4, $e, $T7, $T8, 25]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    xor $T2, $T2, $T3
    xor $T1, $f, $g
    xor $T2, $T2, $T4
    and $T1, $T1, $e
    addw $h, $h, $T2
    xor $T1, $T1, $g
    addw $T1, $T1, $h
___

    return $code;
}

sub sha256_T2 {
    my (
        $a, $b, $c,
    ) = @_;
    my $code=<<___;
    # Sum0
___
    if ($use_zbb) {
        my $ror_part = <<___;
        @{[roriw $T2, $a, 2]}  # roriw $T2, $a, 2
        @{[roriw $T3, $a, 13]}  # roriw $T3, $a, 13
        @{[roriw $T4, $a, 22]}  # roriw $T4, $a, 22
___
        $code .= $ror_part;
    } else {
        my $ror_part = <<___;
        @{[roriw_rv64i $T2, $a, $T7, $T8, 2]}
        @{[roriw_rv64i $T3, $a, $T7, $T8, 13]}
        @{[roriw_rv64i $T4, $a, $T7, $T8, 22]}
___
        $code .= $ror_part;
    }
    $code .= <<___;
    xor $T2, $T2, $T3
    xor $T2, $T2, $T4
    # Maj
    xor $T4, $b, $c
    and $T3, $b, $c
    and $T4, $T4, $a
    xor $T4, $T4, $T3
    # T2
    addw $T2, $T2, $T4
___

    return $code;
}

sub SHA256ROUND {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[sha256_T1 $INDEX, $e, $f, $g, $h]}
    @{[sha256_T2 $a, $b, $c]}
    addw $d, $d, $T1
    addw $h, $T2, $T1
___

    return $code;
}

sub SHA256ROUND0 {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE0 $INDEX]}
    @{[SHA256ROUND $INDEX, $a, $b, $c, $d, $e, $f, $g, $h]}
___

    return $code;
}

sub SHA256ROUND1 {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE1 $INDEX]}
    @{[SHA256ROUND $INDEX, $a, $b, $c, $d, $e, $f, $g, $h]}
___

    return $code;
}

################################################################################
# void sha256_block_data_order@{[$isaext]}(void *c, const void *p, size_t len)
$code .= <<___;
.p2align 3
.globl sha256_block_data_order@{[$isaext]}
.type   sha256_block_data_order@{[$isaext]},\@function
sha256_block_data_order@{[$isaext]}:

    addi sp, sp, -96

    sd s0, 0(sp)
    sd s1, 8(sp)
    sd s2, 16(sp)
    sd s3, 24(sp)
    sd s4, 32(sp)
    sd s5, 40(sp)
    sd s6, 48(sp)
    sd s7, 56(sp)
    sd s8, 64(sp)
    sd s9, 72(sp)
    sd s10, 80(sp)
    sd s11, 88(sp)

    addi sp, sp, -64

    la $KT, $K256

    # load ctx
    lw $A, 0(a0)
    lw $B, 4(a0)
    lw $C, 8(a0)
    lw $D, 12(a0)
    lw $E, 16(a0)
    lw $F, 20(a0)
    lw $G, 24(a0)
    lw $H, 28(a0)

L_round_loop:
    # Decrement length by 1
    addi $LEN, $LEN, -1

    @{[SHA256ROUND0 0, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND0 1, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND0 2, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND0 3, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND0 4, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND0 5, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND0 6, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND0 7, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND0 8, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND0 9, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND0 10, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND0 11, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND0 12, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND0 13, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND0 14, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND0 15, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 16, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 17, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 18, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 19, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 20, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 21, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 22, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 23, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 24, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 25, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 26, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 27, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 28, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 29, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 30, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 31, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 32, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 33, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 34, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 35, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 36, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 37, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 38, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 39, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 40, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 41, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 42, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 43, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 44, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 45, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 46, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 47, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 48, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 49, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 50, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 51, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 52, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 53, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 54, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 55, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA256ROUND1 56, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA256ROUND1 57, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA256ROUND1 58, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA256ROUND1 59, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA256ROUND1 60, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA256ROUND1 61, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA256ROUND1 62, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA256ROUND1 63, $B, $C, $D, $E, $F, $G, $H, $A]}

    lw $T1, 0(a0)
    lw $T2, 4(a0)
    lw $T3, 8(a0)
    lw $T4, 12(a0)

    addw $A, $A, $T1
    addw $B, $B, $T2
    addw $C, $C, $T3
    addw $D, $D, $T4

    sw $A, 0(a0)
    sw $B, 4(a0)
    sw $C, 8(a0)
    sw $D, 12(a0)

    lw $T1, 16(a0)
    lw $T2, 20(a0)
    lw $T3, 24(a0)
    lw $T4, 28(a0)

    addw $E, $E, $T1
    addw $F, $F, $T2
    addw $G, $G, $T3
    addw $H, $H, $T4

    sw $E, 16(a0)
    sw $F, 20(a0)
    sw $G, 24(a0)
    sw $H, 28(a0)

    addi $INP, $INP, 64

    bnez $LEN, L_round_loop

    addi sp, sp, 64

    ld s0, 0(sp)
    ld s1, 8(sp)
    ld s2, 16(sp)
    ld s3, 24(sp)
    ld s4, 32(sp)
    ld s5, 40(sp)
    ld s6, 48(sp)
    ld s7, 56(sp)
    ld s8, 64(sp)
    ld s9, 72(sp)
    ld s10, 80(sp)
    ld s11, 88(sp)

    addi sp, sp, 96

    ret
.size sha256_block_data_order@{[$isaext]},.-sha256_block_data_order@{[$isaext]}

.section .rodata
.p2align 3
.type $K256,\@object
$K256:
    .word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
    .word 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
    .word 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
    .word 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
    .word 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
    .word 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
    .word 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
    .word 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
    .word 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
.size $K256,.-$K256
___

print $code;

close STDOUT or die "error closing STDOUT: $!";
