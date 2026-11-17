#! /usr/bin/env perl
# This file is dual-licensed, meaning that you can use it under your
# choice of either of the following two licenses:
#
# Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License"). You can obtain
# a copy in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# or
#
# Copyright (c) 2025-2026, Julian Zhu <julian.oerv@isrc.iscas.ac.cn>
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
my ($TMP0, $TMP1) = ("a3", "a4");
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6) = ("t0", "t1", "t2", "t3", "t4", "t5", "t6");
my ($A, $B, $C, $D ,$E ,$F ,$G ,$H) = ("s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9");
my ($W9, $W10, $W11, $W12, $W13 ,$W14 ,$W15) = ("s0", "s1", "a5", "a6", "a7", "s10", "s11");
my @W = (undef, undef, undef, undef, undef, undef, undef, undef, undef,
        $W9, $W10, $W11, $W12, $W13, $W14, $W15);
# Misaligned input only; they reuse round temporaries, so are set up per block
my ($BASE, $SHL, $SHR) = ($T3, $T4, $T5);
my ($MISALIGNED_INPUT, $ALIGNED_INPUT) = (0, 1);

# W[9..15] live in registers, the rest on the stack; Wload/Wstore skip registers
sub Wreg {
    my ($index, $offset, $scratch) = @_;
    my $masked = (($index-$offset) & 0x0F);
    return $masked < 9 ? $scratch : $W[$masked];
}

sub Wload {
    my ($scratch, $index, $offset) = @_;
    my $masked = (($index-$offset) & 0x0F);
    if ($masked >= 9) {
        return "";
    }
    return "lw $scratch, (($index-$offset)&0x0F)*4($ADDR)";
}

sub Wstore {
    my ($scratch, $index, $offset) = @_;
    my $masked = (($index-$offset) & 0x0F);
    if ($masked >= 9) {
        return "";
    }
    return "sw $scratch, (($index-$offset)&0x0F)*4($ADDR)";
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
    and $TMP1, $X, $Y
    and $TMP0, $TMP0, $Z
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
    xor $TMP0, $TMP0, $X
    xor $X, $TMP0, $TMP1
___
    return $code;
}

sub P1 {
    my ($X) = @_;
    my $code=<<___;
    @{[roriw $TMP0, $X, 17]}
    @{[roriw $TMP1, $X, 9]}
    xor $TMP0, $TMP0, $X
    xor $X, $TMP0, $TMP1
___
    return $code;
}

# W[j] = P1(W[j-16] ^ W[j-9] ^ ROTL(W[j-3], 15)) ^ ROTL(W[j-13], 7) ^ W[j-6]
sub EXPAND {
    my ($index) = @_;
    my $w16 = Wreg($index, 0, $T1);
    my $w9 = Wreg($index, 9, $T2);
    my $w3 = Wreg($index, 3, $T3);
    my $w13 = Wreg($index, 13, $T4);
    my $w6 = Wreg($index, 6, $T5);
    # W[j] overwrites W[j-16], so W[j-16] is read first
    my $wj = Wreg($index, 0, $T6);
    my $code=<<___;
    @{[Wload $T1, $index, 0]}
    @{[Wload $T2, $index, 9]}
    @{[Wload $T3, $index, 3]}
    @{[Wload $T4, $index, 13]}
    @{[Wload $T5, $index, 6]}
    xor $TMP0, $w16, $w9
    @{[roriw $TMP1, $w3, 17]}
    @{[roriw $T1, $w13, 25]}
    xor $wj, $TMP0, $TMP1
    xor $T1, $T1, $w6
    @{[P1 $wj]}
    xor $wj, $wj, $T1
    @{[Wstore $T6, $index, 0]}
___
    return $code;
}

sub SM3ROUND1 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $wj = Wreg($index, 0, $T5);   # W[j]
    my $w4 = Wreg($index, 12, $T2);  # W[j+4]
    my $code=<<___;
    @{[Wload $T5, $index, 0]}
    @{[Wload $T2, $index, 12]}
    lw $T1, 4*$index($KT) # T1 = Tj
    xor $T6, $wj, $w4 # T6 = W'[j] = W[j] ^ W[j+4]
    @{[roriw $T2, $a, 20]} # T2 = A12
    @{[FG0 $a, $b, $c]}
    addw $T3, $T2, $T1 # T3 = A12 + Tj
    addw $T4, $TMP0, $d # T4 = FF + D
    addw $T3, $T3, $e # T3 = A12 + Tj + E
    addw $T4, $T4, $T6 # T4 = FF + D + W'
    @{[roriw $T3, $T3, 25]} # T3 = SS1
    addw $h, $h, $wj # h = H + W
    xor $T1, $T3, $T2 # T1 = SS2 = SS1 ^ A12
    @{[FG0 $e, $f, $g]}
    addw $d, $T1, $T4 # d = TT1
    @{[roriw $b, $b, 23]}
    addw $T1, $TMP0, $T3 # T1 = GG + SS1
    @{[roriw $f, $f, 13]}
    addw $h, $h, $T1 # h = TT2
    @{[P0 $h]}

___
    return $code;
}

sub SM3ROUND2 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $wj = Wreg($index, 0, $T5);   # W[j]
    my $w4 = Wreg($index, 12, $T2);  # W[j+4]
    my $code=<<___;
    @{[Wload $T5, $index, 0]}
    @{[Wload $T2, $index, 12]}
    lw $T1, 4*$index($KT) # T1 = Tj
    xor $T6, $wj, $w4 # T6 = W'[j] = W[j] ^ W[j+4]
    @{[roriw $T2, $a, 20]} # T2 = A12
    @{[FF1 $a, $b, $c]}
    addw $T3, $T2, $T1 # T3 = A12 + Tj
    addw $T4, $TMP0, $d # T4 = FF + D
    addw $T3, $T3, $e # T3 = A12 + Tj + E
    addw $T4, $T4, $T6 # T4 = FF + D + W'
    @{[roriw $T3, $T3, 25]} # T3 = SS1
    addw $h, $h, $wj # h = H + W
    xor $T1, $T3, $T2 # T1 = SS2 = SS1 ^ A12
    @{[GG1 $e, $f, $g]}
    addw $d, $T1, $T4 # d = TT1
    @{[roriw $b, $b, 23]}
    addw $T1, $TMP0, $T3 # T1 = GG + SS1
    @{[roriw $f, $f, 13]}
    addw $h, $h, $T1 # h = TT2
    @{[P0 $h]}

___
    return $code;
}

# Misaligned: $dst = (lo >> SHL) | (hi << SHR) from two aligned loads
sub loadDword {
    my ($ALIGNED, $dst, $off) = @_;
    if ($ALIGNED) {
        return "ld $dst, $off($INP)";
    }
    my $code=<<___;
    ld $TMP0, $off($BASE)
    ld $TMP1, ($off+8)($BASE)
    srl $dst, $TMP0, $SHL
    sll $TMP1, $TMP1, $SHR
    or $dst, $dst, $TMP1
___
    return $code;
}

# One ld plus rev8 yields two message words, the first in the top half
sub loadMsgRev32 {
    my ($ALIGNED) = @_;
    my $code=<<___;
    @{[loadDword $ALIGNED, $T1, 0]}
    @{[rev8 $T1, $T1]}
    srli $T2, $T1, 32
    sw $T2, 0($ADDR)
    sw $T1, 4($ADDR)

    @{[loadDword $ALIGNED, $T1, 8]}
    @{[rev8 $T1, $T1]}
    srli $T2, $T1, 32
    sw $T2, 8($ADDR)
    sw $T1, 12($ADDR)

    @{[loadDword $ALIGNED, $T1, 16]}
    @{[rev8 $T1, $T1]}
    srli $T2, $T1, 32
    sw $T2, 16($ADDR)
    sw $T1, 20($ADDR)

    @{[loadDword $ALIGNED, $T1, 24]}
    @{[rev8 $T1, $T1]}
    srli $T2, $T1, 32
    sw $T2, 24($ADDR)
    sw $T1, 28($ADDR)

    @{[loadDword $ALIGNED, $W9, 32]}
    @{[rev8 $W9, $W9]}
    srli $T2, $W9, 32
    sw $T2, 32($ADDR)

    @{[loadDword $ALIGNED, $W11, 40]}
    @{[rev8 $W11, $W11]}
    srli $W10, $W11, 32

    @{[loadDword $ALIGNED, $W13, 48]}
    @{[rev8 $W13, $W13]}
    srli $W12, $W13, 32

    @{[loadDword $ALIGNED, $W15, 56]}
    @{[rev8 $W15, $W15]}
    srli $W14, $W15, 32
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

    andi $T1, $INP, 7
    bnez $T1, L_load_misaligned
    @{[loadMsgRev32 $ALIGNED_INPUT]}
    j L_rounds

L_load_misaligned:
    andi $BASE, $INP, -8
    andi $SHL, $INP, 7
    slli $SHL, $SHL, 3
    li $SHR, 64
    sub $SHR, $SHR, $SHL
    @{[loadMsgRev32 $MISALIGNED_INPUT]}

L_rounds:
___

for (my $i = 0; $i < 16; $i += 4) {
    $code .= <<___;
    @{[SM3ROUND1 $i, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND $i]}
    @{[SM3ROUND1 $i+1, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND $i+1]}
    @{[SM3ROUND1 $i+2, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND $i+2]}
    @{[SM3ROUND1 $i+3, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND $i+3]}
___
}

for (my $i = 16; $i < 52; $i += 4) {
    $code .= <<___;
    @{[SM3ROUND2 $i, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[EXPAND $i]}
    @{[SM3ROUND2 $i+1, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[EXPAND $i+1]}
    @{[SM3ROUND2 $i+2, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[EXPAND $i+2]}
    @{[SM3ROUND2 $i+3, $B, $C, $D, $A, $F, $G, $H, $E]}
    @{[EXPAND $i+3]}
___
}

for (my $i = 52; $i < 64; $i += 4) {
    $code .= <<___;
    @{[SM3ROUND2 $i, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SM3ROUND2 $i+1, $D, $A, $B, $C, $H, $E, $F, $G]}
    @{[SM3ROUND2 $i+2, $C, $D, $A, $B, $G, $H, $E, $F]}
    @{[SM3ROUND2 $i+3, $B, $C, $D, $A, $F, $G, $H, $E]}
___
}

$code .= <<___;
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
