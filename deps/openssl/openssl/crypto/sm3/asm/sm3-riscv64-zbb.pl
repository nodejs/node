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

$output and open STDOUT,">$output";

my $code=<<___;
.text
___

my $SM3K = "SM3K";

# Function arguments
my ($INP, $LEN, $ADDR) = ("a1", "a2", "sp");
my ($TMP0, $TMP1, $Wi, $Wj) = ("a3", "a4", "t5", "t6");
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6) = ("t0", "t1", "t2", "t3", "t4", "t5", "t6");
my ($A, $B, $C, $D ,$E ,$F ,$G ,$H) = ("s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9");
my ($W9, $W10, $W11, $W12, $W13 ,$W14 ,$W15) = ("s0", "s1", "a5", "a6", "a7", "s10", "s11");
my @W = (undef, undef, undef, undef, undef, undef, undef, undef, undef,
        $W9, $W10, $W11, $W12, $W13, $W14, $W15);

sub load {
    my ($rd, $index, $offset) = @_;
    my $masked = (($index-$offset)& 0x0F);
    if ($masked < 9) {
        return "lw $rd, (($index-$offset)&0x0F)*4($ADDR)";
    } else {
        return "mv $rd, $W[$masked]";
    }
}

sub store {
    my ($rs, $index, $offset) = @_;
    my $masked = (($index-$offset)& 0x0F);
    if ($masked < 9) {
        return "sw $rs, (($index-$offset)&0x0F)*4($ADDR)";
    } else {
        return "mv $W[$masked], $rs";
    }
}

sub FG0 {
    my ($X, $Y, $Z) = @_;
    my $code=<<___;
    xor $TMP1, $Y, $Z
    xor $TMP0, $TMP1, $X
___
    return $code;
}

sub FF1 {
    my ($X, $Y, $Z) = @_;
    my $code=<<___;
    or $TMP0, $X, $Y
    and $TMP0, $TMP0, $Z
    and $TMP1, $X, $Y
    or $TMP0, $TMP0, $TMP1
___
    return $code;
}

sub GG1 {
    my ($X, $Y, $Z) = @_;
    my $code=<<___;
    xor $TMP1, $Y, $Z
    and $TMP0, $TMP1, $X
    xor $TMP0, $TMP0, $Z
___
    return $code;
}

sub P0 {
    my ($X) = @_;
    my $code=<<___;
    @{[roriw $TMP0, $X, 23]}
    @{[roriw $TMP1, $X, 15]}
    xor $TMP0, $TMP0, $TMP1
    xor $X, $X, $TMP0
___
    return $code;
}

sub P1 {
    my ($X) = @_;
    my $code=<<___;
    @{[roriw $TMP0, $X, 17]}
    @{[roriw $TMP1, $X, 9]}
    xor $TMP0, $TMP0, $TMP1
    xor $X, $X, $TMP0
___
    return $code;
}

sub EXPAND {
    my ($index) = @_;
    my $code = <<___;
    @{[load $T1, $index, 0]}
    @{[load $T2, $index, 9]}
    @{[load $T3, $index, 3]}
    @{[load $T4, $index, 13]}
    @{[load $T5, $index, 6]}
    xor $TMP0, $T1, $T2
    @{[roriw $TMP1, $T3, 17]}
    xor $T6, $TMP0, $TMP1
    @{[P1 $T6]}
    @{[roriw $TMP1, $T4, 25]}
    xor $T6, $T6, $TMP1
    xor $T6, $T6, $T5
    @{[store $T6, $index, 0]}
___
    return $code;
}

sub SM3ROUND1 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $code=<<___;
    @{[load $Wi, $index, 0]}
    @{[load $T2, $index, 12]}
    xor $Wj, $Wi, $T2
    lw $T1, 4*$index($KT)
    @{[roriw $T2, $a, 20]} # T2 = A12
    addw $T3, $T2, $e # T3 = A12_SM = A12 + E
    addw $T3, $T3, $T1 # T3 = A12_SM = A12 + E + Tj
    @{[roriw $T3, $T3, 25]} # T3 = SS1
    @{[FG0 $a, $b, $c]}
    addw $T4, $TMP0, $d # T4 = FF + D
    xor $T1, $T3, $T2 # T1 = SS1 ^ A12
    addw $T1, $T1, $T4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw $d, $T1, $Wj # d = T1 + Wj
    @{[FG0 $e, $f, $g]}
    @{[roriw $b, $b, 23]}
    @{[roriw $f, $f, 13]}
    addw $T1, $TMP0, $T3 # T1 = GG + SS1
    addw $T1, $T1, $Wi # T1 = GG + SS1 + Wj
    addw $h, $h, $T1
    @{[P0 $h]}

___
    return $code;
}

sub SM3ROUND2 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $code=<<___;
    @{[load $Wi, $index, 0]}
    @{[load $T2, $index, 12]}
    xor $Wj, $Wi, $T2
    lw $T1, 4*$index($KT)
    @{[roriw $T2, $a, 20]} # T2 = A12
    addw $T3, $T2, $e # T3 = A12_SM = A12 + E
    addw $T3, $T3, $T1 # T3 = A12_SM = A12 + E + Tj
    @{[roriw $T3, $T3, 25]} # T3 = SS1
    @{[FF1 $a, $b, $c]}
    addw $T4, $TMP0, $d # T4 = FF + D
    xor $T1, $T3, $T2 # T1 = SS1 ^ A12
    addw $T1, $T1, $T4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw $d, $T1, $Wj # d = T1 + Wj
    @{[GG1 $e, $f, $g]}
    @{[roriw $b, $b, 23]}
    @{[roriw $f, $f, 13]}
    addw $T1, $TMP0, $T3 # T1 = GG + SS1
    addw $T1, $T1, $Wi # T1 = GG + SS1 + Wj
    addw $h, $h, $T1
    @{[P0 $h]}

___
    return $code;
}

sub loadMsgRev32 {
    my $code=<<___;

    lw $T1, ($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, ($ADDR)

    lw $T1, 4($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 4($ADDR)

    lw $T1, 8($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 8($ADDR)

    lw $T1, 12($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 12($ADDR)

    lw $T1, 16($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 16($ADDR)

    lw $T1, 20($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 20($ADDR)

    lw $T1, 24($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 24($ADDR)

    lw $T1, 28($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 28($ADDR)

    lw $T1, 32($INP)
    @{[rev8 $T1, $T1]}
    srli $T1, $T1, 32
    sw $T1, 32($ADDR)

    lw $T1, 36($INP)
    @{[rev8 $T1, $T1]}
    srli $W9, $T1, 32

    lw $T1, 40($INP)
    @{[rev8 $T1, $T1]}
    srli $W10, $T1, 32

    lw $T1, 44($INP)
    @{[rev8 $T1, $T1]}
    srli $W11, $T1, 32

    lw $T1, 48($INP)
    @{[rev8 $T1, $T1]}
    srli $W12, $T1, 32

    lw $T1, 52($INP)
    @{[rev8 $T1, $T1]}
    srli $W13, $T1, 32

    lw $T1, 56($INP)
    @{[rev8 $T1, $T1]}
    srli $W14, $T1, 32

    lw $T1, 60($INP)
    @{[rev8 $T1, $T1]}
    srli $W15, $T1, 32
___
    return $code;
}

################################################################################
# void ossl_sm3_block_data_order_zbb(SM3_CTX *ctx, const void *p, size_t num)
$code .= <<___;
.p2align 3
.globl ossl_sm3_block_data_order_zbb
.type   ossl_sm3_block_data_order_zbb,\@function
ossl_sm3_block_data_order_zbb:

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

    la $KT, $SM3K

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

    @{[loadMsgRev32]}

    @{[SM3ROUND1 0, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 0]}
    @{[SM3ROUND1 1, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 1]}
    @{[SM3ROUND1 2, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 2]}
    @{[SM3ROUND1 3, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 3]}

    @{[SM3ROUND1 4, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 4]}
    @{[SM3ROUND1 5, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 5]}
    @{[SM3ROUND1 6, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 6]}
    @{[SM3ROUND1 7, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 7]}

    @{[SM3ROUND1 8, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 8]}
    @{[SM3ROUND1 9, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 9]}
    @{[SM3ROUND1 10, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 10]}
    @{[SM3ROUND1 11, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 11]}

    @{[SM3ROUND1 12, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 12]}
    @{[SM3ROUND1 13, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 13]}
    @{[SM3ROUND1 14, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 14]}
    @{[SM3ROUND1 15, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 15]}

    @{[SM3ROUND2 16, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 16]}
    @{[SM3ROUND2 17, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 17]}
    @{[SM3ROUND2 18, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 18]}
    @{[SM3ROUND2 19, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 19]}

    @{[SM3ROUND2 20, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 20]}
    @{[SM3ROUND2 21, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 21]}
    @{[SM3ROUND2 22, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 22]}
    @{[SM3ROUND2 23, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 23]}

    @{[SM3ROUND2 24, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 24]}
    @{[SM3ROUND2 25, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 25]}
    @{[SM3ROUND2 26, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 26]}
    @{[SM3ROUND2 27, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 27]}

    @{[SM3ROUND2 28, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 28]}
    @{[SM3ROUND2 29, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 29]}
    @{[SM3ROUND2 30, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 30]}
    @{[SM3ROUND2 31, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 31]}

    @{[SM3ROUND2 32, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 32]}
    @{[SM3ROUND2 33, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 33]}
    @{[SM3ROUND2 34, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 34]}
    @{[SM3ROUND2 35, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 35]}

    @{[SM3ROUND2 36, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 36]}
    @{[SM3ROUND2 37, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 37]}
    @{[SM3ROUND2 38, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 38]}
    @{[SM3ROUND2 39, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 39]}

    @{[SM3ROUND2 40, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 40]}
    @{[SM3ROUND2 41, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 41]}
    @{[SM3ROUND2 42, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 42]}
    @{[SM3ROUND2 43, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 43]}

    @{[SM3ROUND2 44, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 44]}
    @{[SM3ROUND2 45, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 45]}
    @{[SM3ROUND2 46, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 46]}
    @{[SM3ROUND2 47, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 47]}

    @{[SM3ROUND2 48, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND 48]}
    @{[SM3ROUND2 49, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND 49]}
    @{[SM3ROUND2 50, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND 50]}
    @{[SM3ROUND2 51, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND 51]}

    @{[SM3ROUND2 52, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SM3ROUND2 53, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[SM3ROUND2 54, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[SM3ROUND2 55, $B, $C, $D, $A, $F, $G, $H, $E]}

    @{[SM3ROUND2 56, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SM3ROUND2 57, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[SM3ROUND2 58, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[SM3ROUND2 59, $B, $C, $D, $A, $F, $G, $H, $E]}

    @{[SM3ROUND2 60, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SM3ROUND2 61, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[SM3ROUND2 62, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[SM3ROUND2 63, $B, $C, $D, $A, $F, $G, $H, $E]}

    lw $T1, 0(a0)
    lw $T2, 4(a0)
    lw $T3, 8(a0)
    lw $T4, 12(a0)

    xor $A, $A, $T1
    xor $B, $B, $T2
    xor $C, $C, $T3
    xor $D, $D, $T4

    sw $A, 0(a0)
    sw $B, 4(a0)
    sw $C, 8(a0)
    sw $D, 12(a0)

    lw $T1, 16(a0)
    lw $T2, 20(a0)
    lw $T3, 24(a0)
    lw $T4, 28(a0)

    xor $E, $E, $T1
    xor $F, $F, $T2
    xor $G, $G, $T3
    xor $H, $H, $T4

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
.size ossl_sm3_block_data_order_zbb,.-ossl_sm3_block_data_order_zbb

.section .rodata
.p2align 3
.type $SM3K,\@object
$SM3K:
    .word 0x79CC4519, 0xF3988A32, 0xE7311465, 0xCE6228CB
    .word 0x9CC45197, 0x3988A32F, 0x7311465E, 0xE6228CBC
    .word 0xCC451979, 0x988A32F3, 0x311465E7, 0x6228CBCE
    .word 0xC451979C, 0x88A32F39, 0x11465E73, 0x228CBCE6
    .word 0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C
    .word 0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE
    .word 0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC
    .word 0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5
    .word 0x7A879D8A, 0xF50F3B14, 0xEA1E7629, 0xD43CEC53
    .word 0xA879D8A7, 0x50F3B14F, 0xA1E7629E, 0x43CEC53D
    .word 0x879D8A7A, 0x0F3B14F5, 0x1E7629EA, 0x3CEC53D4
    .word 0x79D8A7A8, 0xF3B14F50, 0xE7629EA1, 0xCEC53D43
    .word 0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C
    .word 0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE
    .word 0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC
    .word 0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5
.size $SM3K,.-$SM3K
___

print $code;

close STDOUT or die "error closing STDOUT: $!";
