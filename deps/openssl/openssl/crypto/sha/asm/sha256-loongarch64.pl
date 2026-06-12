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
# Copyright (c) 2025, Julian Zhu <jz531210@gmail.com>
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

use strict;
use warnings;

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
my $output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
my $flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$output and open STDOUT,">$output";

my $code=<<___;
.text
___

my $K256 = "K256";

# Function arguments
my ($zero,$ra,$tp,$sp,$fp)=map("\$r$_",(0..3,22));
my ($a0,$a1,$a2,$a3,$a4,$a5,$a6,$a7)=map("\$r$_",(4..11));
my ($t0,$t1,$t2,$t3,$t4,$t5,$t6,$t7,$t8,$x)=map("\$r$_",(12..21));
my ($s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7,$s8)=map("\$r$_",(23..31));

my ($INP, $LEN, $ADDR) = ($a1, $a2, $sp);
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6) = ($t0, $t1, $t2, $t3, $t4, $t5, $t6);
my ($A, $B, $C, $D ,$E ,$F ,$G ,$H) = ($s0, $s1, $s2, $s3, $s4, $s5, $s6, $s7);

sub MSGSCHEDULE0 {
    my ($index) = @_;
    my $code=<<___;
    ld.w $T1, $INP, 4*$index
    revb.2w $T1, $T1
    st.w $T1, $ADDR, 4*$index
___
    return $code;
}

sub MSGSCHEDULE1 {
    my ($index) = @_;
    my $code=<<___;
    ld.w $T1, $ADDR, (($index-2)&0x0f)*4
    ld.w $T2, $ADDR, (($index-15)&0x0f)*4
    ld.w $T3, $ADDR, (($index-7)&0x0f)*4
    ld.w $T4, $ADDR, ($index&0x0f)*4
    rotri.w $T5, $T1, 17
    rotri.w $T6, $T1, 19
    srli.w $T1, $T1, 10
    xor $T1, $T1, $T5
    xor $T1, $T1, $T6
    add.w $T1, $T1, $T3
    rotri.w $T5, $T2, 7
    rotri.w $T6, $T2, 18
    srli.w $T2, $T2, 3
    xor $T2, $T2, $T5
    xor $T2, $T2, $T6
    add.w $T1, $T1, $T2
    add.w $T1, $T1, $T4
    st.w $T1, $ADDR, ($index&0x0f)*4
___
    return $code;
}

sub sha256_T1 {
    my ($index, $e, $f, $g, $h) = @_;
    my $code=<<___;
    ld.w $T4, $KT, 4*$index
    add.w $h, $h, $T1
    add.w $h, $h, $T4
    rotri.w $T2, $e, 6
    rotri.w $T3, $e, 11
    rotri.w $T4, $e, 25
    xor $T2, $T2, $T3
    xor $T1, $f, $g
    xor $T2, $T2, $T4
    and $T1, $T1, $e
    add.w $h, $h, $T2
    xor $T1, $T1, $g
    add.w $T1, $T1, $h
___
    return $code;
}

sub sha256_T2 {
    my ($a, $b, $c) = @_;
    my $code=<<___;
    rotri.w $T2, $a, 2
    rotri.w $T3, $a, 13
    rotri.w $T4, $a, 22
    xor $T2, $T2, $T3
    xor $T2, $T2, $T4
    xor $T4, $b, $c
    and $T3, $b, $c
    and $T4, $T4, $a
    xor $T4, $T4, $T3
    add.w $T2, $T2, $T4
___
    return $code;
}

sub SHA256ROUND {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $code=<<___;
    @{[sha256_T1 $index, $e, $f, $g, $h]}
    @{[sha256_T2 $a, $b, $c]}
    add.w $d, $d, $T1
    add.w $h, $T2, $T1
___
    return $code;
}

sub SHA256ROUND0 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE0 $index]}
    @{[SHA256ROUND $index, $a, $b, $c, $d, $e, $f, $g, $h]}
___
    return $code;
}

sub SHA256ROUND1 {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $code=<<___;
    @{[MSGSCHEDULE1 $index]}
    @{[SHA256ROUND $index, $a, $b, $c, $d, $e, $f, $g, $h]}
___
    return $code;
}

################################################################################
# void sha256_block_data_order(void *c, const void *p, size_t len)
$code .= <<___;
.p2align 3
.globl sha256_block_data_order
.type   sha256_block_data_order,\@function
sha256_block_data_order:

    addi.d $sp, $sp, -80

    st.d $s0, $sp, 0
    st.d $s1, $sp, 8
    st.d $s2, $sp, 16
    st.d $s3, $sp, 24
    st.d $s4, $sp, 32
    st.d $s5, $sp, 40
    st.d $s6, $sp, 48
    st.d $s7, $sp, 56
    st.d $s8, $sp, 64
    st.d $fp, $sp, 72

    addi.d $sp, $sp, -64

    la $KT, $K256

    # load ctx
    ld.w $A, $a0, 0
    ld.w $B, $a0, 4
    ld.w $C, $a0, 8
    ld.w $D, $a0, 12
    ld.w $E, $a0, 16
    ld.w $F, $a0, 20
    ld.w $G, $a0, 24
    ld.w $H, $a0, 28

L_round_loop:
    # Decrement length by 1
    addi.d $LEN, $LEN, -1

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

    ld.w $T1, $a0, 0
    ld.w $T2, $a0, 4
    ld.w $T3, $a0, 8
    ld.w $T4, $a0, 12

    add.w $A, $A, $T1
    add.w $B, $B, $T2
    add.w $C, $C, $T3
    add.w $D, $D, $T4

    st.w $A, $a0, 0
    st.w $B, $a0, 4
    st.w $C, $a0, 8
    st.w $D, $a0, 12

    ld.w $T1, $a0, 16
    ld.w $T2, $a0, 20
    ld.w $T3, $a0, 24
    ld.w $T4, $a0, 28

    add.w $E, $E, $T1
    add.w $F, $F, $T2
    add.w $G, $G, $T3
    add.w $H, $H, $T4

    st.w $E, $a0, 16
    st.w $F, $a0, 20
    st.w $G, $a0, 24
    st.w $H, $a0, 28

    addi.d $INP, $INP, 64

    bnez $LEN, L_round_loop

    addi.d $sp, $sp, 64

    ld.d $s0, $sp, 0
    ld.d $s1, $sp, 8
    ld.d $s2, $sp, 16
    ld.d $s3, $sp, 24
    ld.d $s4, $sp, 32
    ld.d $s5, $sp, 40
    ld.d $s6, $sp, 48
    ld.d $s7, $sp, 56
    ld.d $s8, $sp, 64
    ld.d $fp, $sp, 72

    addi.d $sp, $sp, 80

    ret
.size sha256_block_data_order,.-sha256_block_data_order

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
