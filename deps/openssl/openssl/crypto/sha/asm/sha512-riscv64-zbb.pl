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

my $K512 = "K512";

# Function arguments
my ($INP, $LEN, $ADDR) = ("a1", "a2", "sp");
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6) = ("t0", "t1", "t2", "t3", "t4", "t5", "t6");
my ($A, $B, $C, $D ,$E ,$F ,$G ,$H) = ("s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9");

sub MSGSCHEDULE0 {
    my (
        $index,
    ) = @_;
    my $code=<<___;
    ld $T1, (8*$index+0)($INP)
    @{[rev8 $T1, $T1]}
    sd $T1, 8*$index($ADDR)
___

    return $code;
}

sub MSGSCHEDULE1 {
    my (
        $INDEX,
    ) = @_;
    my $code=<<___;
    ld $T1, (($INDEX-2)&0x0f)*8($ADDR)
    ld $T2, (($INDEX-15)&0x0f)*8($ADDR)
    ld $T3, (($INDEX-7)&0x0f)*8($ADDR)
    ld $T4, ($INDEX&0x0f)*8($ADDR)
    @{[rori $T5, $T1, 19]}
    @{[rori $T6, $T1, 61]}
    srli $T1, $T1, 6
    xor $T1, $T1, $T5
    xor $T1, $T1, $T6
    add $T1, $T1, $T3
    @{[rori $T5, $T2, 1]}
    @{[rori $T6, $T2, 8]}
    srli $T2, $T2, 7
    xor $T2, $T2, $T5
    xor $T2, $T2, $T6
    add $T1, $T1, $T2
    add $T1, $T1, $T4
    sd $T1, 8*($INDEX&0x0f)($ADDR)
___

    return $code;
}

sub sha512_T1 {
    my (
        $INDEX, $e, $f, $g, $h,
    ) = @_;
    my $code=<<___;
    ld $T4, 8*$INDEX($KT)
    add $h, $h, $T1
    add $h, $h, $T4
    @{[rori $T2, $e, 14]}
    @{[rori $T3, $e, 18]}
    @{[rori $T4, $e, 41]}
    xor $T2, $T2, $T3
    xor $T1, $f, $g
    xor $T2, $T2, $T4
    and $T1, $T1, $e
    add $h, $h, $T2
    xor $T1, $T1, $g
    add $T1, $T1, $h
___

    return $code;
}

sub sha512_T2 {
    my (
        $a, $b, $c,
    ) = @_;
    my $code=<<___;
    # Sigma0
    @{[rori $T2, $a, 28]}
    @{[rori $T3, $a, 34]}
    @{[rori $T4, $a, 39]}
    xor $T2, $T2, $T3
    # Maj
    xor $T5, $b, $c
    and $T3, $b, $c
    and $T5, $T5, $a
    xor $T2, $T2, $T4
    xor $T3, $T3, $T5
    # T2
    add $T2, $T2, $T3
___

    return $code;
}

sub SHA512ROUND {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[sha512_T1 $INDEX, $e, $f, $g, $h]}
    @{[sha512_T2 $a, $b, $c]}
    add $d, $d, $T1
    add $h, $T2, $T1
___

    return $code;
}

sub SHA512ROUND0 {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE0 $INDEX]}
    @{[SHA512ROUND $INDEX, $a, $b, $c, $d, $e, $f, $g, $h]}
___

    return $code;
}

sub SHA512ROUND1 {
    my (
        $INDEX, $a, $b, $c, $d, $e, $f, $g, $h
    ) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE1 $INDEX]}
    @{[SHA512ROUND $INDEX, $a, $b, $c, $d, $e, $f, $g, $h]}
___

    return $code;
}

################################################################################
# void sha512_block_data_order_zbb(void *c, const void *p, size_t len)
$code .= <<___;
.p2align 3
.globl sha512_block_data_order_zbb
.type   sha512_block_data_order_zbb,\@function
sha512_block_data_order_zbb:

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

    addi sp, sp, -128

    la $KT, $K512

    # load ctx
    ld $A, 0(a0)
    ld $B, 8(a0)
    ld $C, 16(a0)
    ld $D, 24(a0)
    ld $E, 32(a0)
    ld $F, 40(a0)
    ld $G, 48(a0)
    ld $H, 56(a0)

L_round_loop:
    # Decrement length by 1
    addi $LEN, $LEN, -1

    @{[SHA512ROUND0 0, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND0 1, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND0 2, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND0 3, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND0 4, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND0 5, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND0 6, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND0 7, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND0 8, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND0 9, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND0 10, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND0 11, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND0 12, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND0 13, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND0 14, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND0 15, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 16, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 17, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 18, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 19, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 20, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 21, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 22, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 23, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 24, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 25, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 26, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 27, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 28, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 29, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 30, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 31, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 32, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 33, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 34, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 35, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 36, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 37, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 38, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 39, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 40, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 41, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 42, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 43, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 44, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 45, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 46, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 47, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 48, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 49, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 50, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 51, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 52, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 53, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 54, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 55, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 56, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 57, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 58, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 59, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 60, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 61, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 62, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 63, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 64, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 65, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 66, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 67, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 68, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 69, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 70, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 71, $B, $C, $D, $E, $F, $G, $H, $A]}

    @{[SHA512ROUND1 72, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND1 73, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND1 74, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND1 75, $F, $G, $H, $A, $B, $C, $D, $E]}

    @{[SHA512ROUND1 76, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND1 77, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND1 78, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND1 79, $B, $C, $D, $E, $F, $G, $H, $A]}

    ld $T1, 0(a0)
    ld $T2, 8(a0)
    ld $T3, 16(a0)
    ld $T4, 24(a0)

    add $A, $A, $T1
    add $B, $B, $T2
    add $C, $C, $T3
    add $D, $D, $T4

    sd $A, 0(a0)
    sd $B, 8(a0)
    sd $C, 16(a0)
    sd $D, 24(a0)

    ld $T1, 32(a0)
    ld $T2, 40(a0)
    ld $T3, 48(a0)
    ld $T4, 56(a0)

    add $E, $E, $T1
    add $F, $F, $T2
    add $G, $G, $T3
    add $H, $H, $T4

    sd $E, 32(a0)
    sd $F, 40(a0)
    sd $G, 48(a0)
    sd $H, 56(a0)

    addi $INP, $INP, 128

    bnez $LEN, L_round_loop

    addi sp, sp, 128

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
.size sha512_block_data_order_zbb,.-sha512_block_data_order_zbb

.section .rodata
.p2align 3
.type $K512,\@object
$K512:
    .dword 0x428a2f98d728ae22, 0x7137449123ef65cd
    .dword 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc
    .dword 0x3956c25bf348b538, 0x59f111f1b605d019
    .dword 0x923f82a4af194f9b, 0xab1c5ed5da6d8118
    .dword 0xd807aa98a3030242, 0x12835b0145706fbe
    .dword 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2
    .dword 0x72be5d74f27b896f, 0x80deb1fe3b1696b1
    .dword 0x9bdc06a725c71235, 0xc19bf174cf692694
    .dword 0xe49b69c19ef14ad2, 0xefbe4786384f25e3
    .dword 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65
    .dword 0x2de92c6f592b0275, 0x4a7484aa6ea6e483
    .dword 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5
    .dword 0x983e5152ee66dfab, 0xa831c66d2db43210
    .dword 0xb00327c898fb213f, 0xbf597fc7beef0ee4
    .dword 0xc6e00bf33da88fc2, 0xd5a79147930aa725
    .dword 0x06ca6351e003826f, 0x142929670a0e6e70
    .dword 0x27b70a8546d22ffc, 0x2e1b21385c26c926
    .dword 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df
    .dword 0x650a73548baf63de, 0x766a0abb3c77b2a8
    .dword 0x81c2c92e47edaee6, 0x92722c851482353b
    .dword 0xa2bfe8a14cf10364, 0xa81a664bbc423001
    .dword 0xc24b8b70d0f89791, 0xc76c51a30654be30
    .dword 0xd192e819d6ef5218, 0xd69906245565a910
    .dword 0xf40e35855771202a, 0x106aa07032bbd1b8
    .dword 0x19a4c116b8d2d0c8, 0x1e376c085141ab53
    .dword 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8
    .dword 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb
    .dword 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3
    .dword 0x748f82ee5defb2fc, 0x78a5636f43172f60
    .dword 0x84c87814a1f0ab72, 0x8cc702081a6439ec
    .dword 0x90befffa23631e28, 0xa4506cebde82bde9
    .dword 0xbef9a3f7b2c67915, 0xc67178f2e372532b
    .dword 0xca273eceea26619c, 0xd186b8c721c0c207
    .dword 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178
    .dword 0x06f067aa72176fba, 0x0a637dc5a2c898a6
    .dword 0x113f9804bef90dae, 0x1b710b35131c471b
    .dword 0x28db77f523047d84, 0x32caab7b40c72493
    .dword 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c
    .dword 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a
    .dword 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
.size $K512,.-$K512
___

print $code;

close STDOUT or die "error closing STDOUT: $!";
