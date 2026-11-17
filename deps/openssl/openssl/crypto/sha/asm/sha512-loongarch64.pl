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

my $use_lsx = $flavour && $flavour =~ /lsx/i ? 1 : 0;
my $isaext = "_" . ( $use_lsx ? "lsx" : "la64v100" );

$output and open STDOUT,">$output";

my $code=<<___;
.text
___

my $K512 = "K512";

# Function arguments
my ($zero,$ra,$tp,$sp,$fp)=("\$zero", "\$ra", "\$tp", "\$sp", "\$fp");
my ($a0,$a1,$a2,$a3,$a4,$a5,$a6,$a7)=map("\$a$_",(0..7));
my ($t0,$t1,$t2,$t3,$t4,$t5,$t6,$t7,$t8)=map("\$t$_",(0..8));
my ($s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7,$s8)=map("\$s$_",(0..8));
my ($va0, $va1, $va2, $va3, $va4, $va5, $va6, $va7) = map("\$vr$_",(0..7));
my ($vt0, $vt1, $vt2, $vt3, $vt4, $vt5, $vt6, $vt7) = map("\$vr$_",(8..15));

my ($INP, $LEN, $ADDR) = ($a1, $a2, $sp);
my ($KT, $T1, $T2, $T3, $T4, $T5, $T6) = ($t0, $t1, $t2, $t3, $t4, $t5, $t6);
my ($A, $B, $C, $D, $E, $F, $G, $H) = ($s0, $s1, $s2, $s3, $s4, $s5, $s6, $s7);
my @VMSGS = ($va0, $va1, $va2, $va3, $va4, $va5, $va6, $va7);

sub strip {
    my ($str) = @_;
    $str =~ s/^\s+|\s+$//g;
    return $str;
}

sub MSGSCHEDULE0_lsx {
    my ($index) = @_;
    my $msg = $VMSGS[$index / 2];
    my $code;

    if ($index % 2 == 0) {
        $code = <<___;
    vld $msg, $INP, @{[8*$index]}
    vshuf4i.b $msg, $msg, 0b00011011
    vshuf4i.w $msg, $msg, 0b10110001
___
    }

    $code .= <<___;
    vpickve2gr.d $T1, $msg, @{[$index%2]}
___

    return strip($code);
}

sub MSGSCHEDULE0 {
    my ($index) = @_;

    if ($use_lsx) {
        return MSGSCHEDULE0_lsx($index);
    }

    my $code=<<___;
    ld.d $T1, $INP, @{[8*$index]}
    revb.d $T1, $T1
    st.d $T1, $ADDR, @{[8*$index]}
___
    return strip($code);
}

sub MSGSCHEDULE1_lsx {
    my ($index) = @_;
    my $msgidx = ($index / 2) % 8;
    my $m01 = $VMSGS[$msgidx];
    my $m23 = $VMSGS[($msgidx + 1) % 8];
    my $m45 = $VMSGS[($msgidx + 2) % 8];
    my $m67 = $VMSGS[($msgidx + 3) % 8];
    my $m89 = $VMSGS[($msgidx + 4) % 8];
    my $mab = $VMSGS[($msgidx + 5) % 8];
    my $mcd = $VMSGS[($msgidx + 6) % 8];
    my $mef = $VMSGS[($msgidx + 7) % 8];
    my ($m12, $tmp0, $tmp1) = ($vt0, $vt1, $vt2);
    my $code;

    if ($index % 2 == 0) {
        # re-align to get $m12 and "$m9a" ($tmp0)
        $code = <<___;
    # m01 & new = $m01, m23 = $m23, m45 = $m45, m67 = $m67
    # m89 = $m89, mab = $mab, mcd = $mcd, mef = $mef
    vori.b $m12, $m01, 0
    vshuf4i.d $m12, $m23, 0b1001
    vori.b $tmp0, $m89, 0
    vshuf4i.d $tmp0, $mab, 0b1001
    vadd.d $m01, $m01, $tmp0
___

        # $m01 += sigma0($m12)
        $code .= <<___;
    vrotri.d $tmp0, $m12, 1
    vrotri.d $tmp1, $m12, 8
    vsrli.d $m12, $m12, 7
    vxor.v $tmp0, $tmp0, $tmp1
    vxor.v $m12, $m12, $tmp0
    vadd.d $m01, $m01, $m12
___

        # $m01 += sigma1
        # now m12 can be re-used as temporary
        $code .= <<___;
    vrotri.d $tmp0, $mef, 19
    vrotri.d $tmp1, $mef, 61
    vsrli.d $m12, $mef, 6
    vxor.v $tmp0, $tmp0, $tmp1
    vxor.v $m12, $m12, $tmp0
    vadd.d $m01, $m01, $m12
___
    }

    $code .= <<___;
    vpickve2gr.d $T1, $m01, @{[$index%2]}
___

    return strip($code);
}

sub MSGSCHEDULE1 {
    my ($index) = @_;

    if ($use_lsx) {
        return MSGSCHEDULE1_lsx($index);
    }

    my $code=<<___;
    ld.d $T1, $ADDR, @{[(($index-2)&0x0f)*8]}
    ld.d $T2, $ADDR, @{[(($index-15)&0x0f)*8]}
    ld.d $T3, $ADDR, @{[(($index-7)&0x0f)*8]}
    ld.d $T4, $ADDR, @{[($index&0x0f)*8]}
    rotri.d $T5, $T1, 19
    rotri.d $T6, $T1, 61
    srli.d $T1, $T1, 6
    xor $T1, $T1, $T5
    xor $T1, $T1, $T6
    add.d $T1, $T1, $T3
    rotri.d $T5, $T2, 1
    rotri.d $T6, $T2, 8
    srli.d $T2, $T2, 7
    xor $T2, $T2, $T5
    xor $T2, $T2, $T6
    add.d $T1, $T1, $T2
    add.d $T1, $T1, $T4
    st.d $T1, $ADDR, @{[8*($index&0x0f)]}
___
    return strip($code);
}

sub sha512_T1 {
    my ($index, $e, $f, $g, $h) = @_;
    my $code=<<___;
    ld.d $T4, $KT, @{[8*$index]}
    add.d $h, $h, $T1
    add.d $h, $h, $T4
    rotri.d $T2, $e, 14
    rotri.d $T3, $e, 18
    rotri.d $T4, $e, 41
    xor $T2, $T2, $T3
    xor $T1, $f, $g
    xor $T2, $T2, $T4
    and $T1, $T1, $e
    add.d $h, $h, $T2
    xor $T1, $T1, $g
    add.d $T1, $T1, $h
___
    return strip($code);
}

sub sha512_T2 {
    my ($a, $b, $c) = @_;
    my $code=<<___;
    rotri.d $T2, $a, 28
    rotri.d $T3, $a, 34
    rotri.d $T4, $a, 39
    xor $T2, $T2, $T3
    xor $T5, $b, $c
    and $T3, $b, $c
    and $T5, $T5, $a
    xor $T2, $T2, $T4
    xor $T3, $T3, $T5
    add.d $T2, $T2, $T3
___
    return strip($code);
}

sub SHA512ROUND {
    my ($index, $a, $b, $c, $d, $e, $f, $g, $h) = @_;
    my $ms = $index < 16 ? \&MSGSCHEDULE0 : \&MSGSCHEDULE1;
    my $code=<<___;
    @{[$ms->($index)]}
    @{[sha512_T1 $index, $e, $f, $g, $h]}
    @{[sha512_T2 $a, $b, $c]}
    add.d $d, $d, $T1
    add.d $h, $T2, $T1
___
    return strip($code);
}

################################################################################
# void sha512_block_data_order$isaext(void *c, const void *p, size_t len)
$code .= <<___;
.p2align 3
.globl sha512_block_data_order@{[$isaext]}
.type   sha512_block_data_order@{[$isaext]},\@function
sha512_block_data_order@{[$isaext]}:

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
___

# SHA512 LSX needs neither dedicated shuffle control word, nor stack space for
# internal states
if (!$use_lsx) {
    $code .= <<___;
    addi.d $sp, $sp, -128
___
}

$code .= <<___;
    la $KT, $K512

    # load ctx
    ld.d $A, $a0, 0
    ld.d $B, $a0, 8
    ld.d $C, $a0, 16
    ld.d $D, $a0, 24
    ld.d $E, $a0, 32
    ld.d $F, $a0, 40
    ld.d $G, $a0, 48
    ld.d $H, $a0, 56

L_round_loop:
    # Decrement length by 1
    addi.d $LEN, $LEN, -1
___

for (my $i = 0; $i < 80; $i += 8) {
    $code .= <<___;
    @{[SHA512ROUND $i, $A, $B, $C, $D, $E, $F, $G, $H]}
    @{[SHA512ROUND $i+1, $H, $A, $B, $C, $D, $E, $F, $G]}
    @{[SHA512ROUND $i+2, $G, $H, $A, $B, $C, $D, $E, $F]}
    @{[SHA512ROUND $i+3, $F, $G, $H, $A, $B, $C, $D, $E]}
    @{[SHA512ROUND $i+4, $E, $F, $G, $H, $A, $B, $C, $D]}
    @{[SHA512ROUND $i+5, $D, $E, $F, $G, $H, $A, $B, $C]}
    @{[SHA512ROUND $i+6, $C, $D, $E, $F, $G, $H, $A, $B]}
    @{[SHA512ROUND $i+7, $B, $C, $D, $E, $F, $G, $H, $A]}
___
}

$code .= <<___;
    ld.d $T1, $a0, 0
    ld.d $T2, $a0, 8
    ld.d $T3, $a0, 16
    ld.d $T4, $a0, 24

    add.d $A, $A, $T1
    add.d $B, $B, $T2
    add.d $C, $C, $T3
    add.d $D, $D, $T4

    st.d $A, $a0, 0
    st.d $B, $a0, 8
    st.d $C, $a0, 16
    st.d $D, $a0, 24

    ld.d $T1, $a0, 32
    ld.d $T2, $a0, 40
    ld.d $T3, $a0, 48
    ld.d $T4, $a0, 56

    add.d $E, $E, $T1
    add.d $F, $F, $T2
    add.d $G, $G, $T3
    add.d $H, $H, $T4

    st.d $E, $a0, 32
    st.d $F, $a0, 40
    st.d $G, $a0, 48
    st.d $H, $a0, 56

    addi.d $INP, $INP, 128

    bnez $LEN, L_round_loop
___

if (!$use_lsx) {
    $code .= <<___;
    addi.d $sp, $sp, 128
___
}

$code .= <<___;
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
.size sha512_block_data_order@{[$isaext]},.-sha512_block_data_order@{[$isaext]}

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
