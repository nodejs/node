#! /usr/bin/env perl
# This file is dual-licensed, meaning that you can use it under your
# choice of either of the following two licenses:
#
# Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License"). You can obtain
# a copy in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# or
#
# Copyright (c) 2023, Christoph Müllner <christoph.muellner@vrull.eu>
# Copyright (c) 2023, Jerry Shih <jerry.shih@sifive.com>
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
# - RISC-V Vector ('V') with VLEN >= 128
# - RISC-V Vector Cryptography Bit-manipulation extension ('Zvkb')
# - RISC-V Vector SM3 Secure Hash extension ('Zvksh')

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

################################################################################
# ossl_hwsm3_block_data_order_zvksh(SM3_CTX *c, const void *p, size_t num);
{
my ($CTX, $INPUT, $NUM, $EVENNUM , $TMPADDR) = ("a0", "a1", "a2", "a6", "t0");
my ($V0, $V1, $V2, $V3, $V4, $V5, $V6, $V7,
    $V8, $V9, $V10, $V11, $V12, $V13, $V14, $V15,
    $V16, $V17, $V18, $V19, $V20, $V21, $V22, $V23,
    $V24, $V25, $V26, $V27, $V28, $V29, $V30, $V31,
) = map("v$_",(0..31));

$code .= <<___;
.text
.p2align 3
.globl ossl_hwsm3_block_data_order_zvksh
.type ossl_hwsm3_block_data_order_zvksh,\@function
ossl_hwsm3_block_data_order_zvksh:
    # Obtain VLEN and select the corresponding branch
    csrr t0, vlenb
    srl t1, t0, 5
    beqz t1, ossl_hwsm3_block_data_order_zvksh_zvl128
    srl t1, t0, 6
    beqz t1, ossl_hwsm3_block_data_order_zvksh_zvl256
ossl_hwsm3_block_data_order_zvksh_zvl512:
    @{[vsetivli "zero", 8, "e32", "m1", "tu", "mu"]}
    @{[vle32_v $V26, $CTX]}
    @{[vrev8_v $V26, $V26]}
    @{[vsetivli "zero", 16, "e32", "m1", "ta", "ma"]}
    la $TMPADDR, ORDER_BY_ZVL512_DATA
    @{[vle32_v $V30, $TMPADDR]}
    addi $TMPADDR, $TMPADDR, 64
    @{[vle32_v $V31, $TMPADDR]}
    la $TMPADDR, ORDER_BY_ZVL512_EXP
    @{[vle32_v $V29, $TMPADDR]}
    addi $TMPADDR, $TMPADDR, 64
    @{[vle32_v $V28, $TMPADDR]}
    srli $EVENNUM , $NUM, 1
    andi $NUM, $NUM, 1
    beqz $EVENNUM , ossl_hwsm3_block_data_order_zvksh_zvl256
L_sm3_loop_zvl512:
    # Use indexed loads (ORDER_BY_RVV512_DATA) to load two blocks in the
    # word order expected by the later vrgather/vsm3c stages.
    @{[vluxei32_v $V0, $INPUT, $V30]}
    @{[vluxei32_v $V1, $INPUT, $V31]}
    @{[vrgather_vv $V9, $V0, $V29]}
    @{[vrgather_vv $V10, $V9, $V29]}
    @{[vrgather_vv $V11, $V1, $V28]}
    @{[vor_vv $V10, $V10, $V11]}
    @{[vrgather_vv $V11, $V10, $V29]}
    @{[vrgather_vv $V12, $V1, $V29]}
    @{[vrgather_vv $V13, $V12, $V29]}
    @{[vsm3me_vv $V2, $V1, $V0]}
    @{[vrgather_vv $V14, $V2, $V28]}
    @{[vor_vv $V13, $V13, $V14]}
    @{[vrgather_vv $V14, $V13, $V29]}
    @{[vrgather_vv $V15, $V2, $V29]}
    @{[vrgather_vv $V16, $V15, $V29]}
    @{[vsm3me_vv $V3, $V2, $V1]}
    @{[vrgather_vv $V17, $V3, $V28]}
    @{[vor_vv $V16, $V16, $V17]}
    @{[vrgather_vv $V17, $V16, $V29]}
    @{[vrgather_vv $V18, $V3, $V29]}
    @{[vrgather_vv $V19, $V18, $V29]}
    @{[vsm3me_vv $V4, $V3, $V2]}
    @{[vrgather_vv $V20, $V4, $V28]}
    @{[vor_vv $V19, $V19, $V20]}
    @{[vrgather_vv $V20, $V19, $V29]}
    @{[vrgather_vv $V21, $V4, $V29]}
    @{[vrgather_vv $V22, $V21, $V29]}
    @{[vsm3me_vv $V5, $V4, $V3]}
    @{[vrgather_vv $V23, $V5, $V28]}
    @{[vor_vv $V22, $V22, $V23]}
    @{[vrgather_vv $V23, $V22, $V29]}
    @{[vrgather_vv $V24, $V5, $V29]}
    @{[vrgather_vv $V25, $V24, $V29]}
    @{[vsm3me_vv $V6, $V5, $V4]}
    @{[vrgather_vv $V27, $V6, $V28]}
    @{[vor_vv $V25, $V25, $V27]}
    @{[vsm3me_vv $V7, $V6, $V5]}
    @{[vsm3me_vv $V8, $V7, $V6]}
    @{[vmv_v_v $V27, $V26]}
    @{[vsetivli "zero", 8, "e32", "m1", "tu", "mu"]}
    @{[vsm3c_vi $V26, $V0, 0]}
    @{[vsm3c_vi $V26, $V9, 1]}
    @{[vsm3c_vi $V26, $V10, 2]}
    @{[vsm3c_vi $V26, $V11, 3]}
    @{[vsm3c_vi $V26, $V1, 4]}
    @{[vsm3c_vi $V26, $V12, 5]}
    @{[vsm3c_vi $V26, $V13, 6]}
    @{[vsm3c_vi $V26, $V14, 7]}
    @{[vsm3c_vi $V26, $V2, 8]}
    @{[vsm3c_vi $V26, $V15, 9]}
    @{[vsm3c_vi $V26, $V16, 10]}
    @{[vsm3c_vi $V26, $V17, 11]}
    @{[vsm3c_vi $V26, $V3, 12]}
    @{[vsm3c_vi $V26, $V18, 13]}
    @{[vsm3c_vi $V26, $V19, 14]}
    @{[vsm3c_vi $V26, $V20, 15]}
    @{[vsm3c_vi $V26, $V4, 16]}
    @{[vsm3c_vi $V26, $V21, 17]}
    @{[vsm3c_vi $V26, $V22, 18]}
    @{[vsm3c_vi $V26, $V23, 19]}
    @{[vsm3c_vi $V26, $V5, 20]}
    @{[vsm3c_vi $V26, $V24, 21]}
    @{[vsm3c_vi $V26, $V25, 22]}
    @{[vrgather_vv $V9, $V25, $V29]}
    @{[vrgather_vv $V10, $V6, $V29]}
    @{[vrgather_vv $V11, $V10, $V29]}
    @{[vrgather_vv $V12, $V7, $V28]}
    @{[vor_vv $V11, $V11, $V12]}
    @{[vrgather_vv $V12, $V11, $V29]}
    @{[vrgather_vv $V13, $V7, $V29]}
    @{[vrgather_vv $V14, $V13, $V29]}
    @{[vrgather_vv $V15, $V8, $V28]}
    @{[vor_vv $V14, $V14, $V15]}
    @{[vrgather_vv $V15, $V14, $V29]}
    @{[vsm3c_vi $V26, $V9, 23]}
    @{[vsm3c_vi $V26, $V6, 24]}
    @{[vsm3c_vi $V26, $V10, 25]}
    @{[vsm3c_vi $V26, $V11, 26]}
    @{[vsm3c_vi $V26, $V12, 27]}
    @{[vsm3c_vi $V26, $V7, 28]}
    @{[vsm3c_vi $V26, $V13, 29]}
    @{[vsm3c_vi $V26, $V14, 30]}
    @{[vsm3c_vi $V26, $V15, 31]}
    @{[vsetivli "zero", 16, "e32", "m1", "ta", "ma"]}
    @{[vxor_vv $V26, $V26, $V27]}
    @{[vslideup_vi $V27, $V26, 8]}
    @{[vmv_v_v $V26, $V27]}
    @{[vsm3c_vi $V26, $V0, 0]}
    @{[vsm3c_vi $V26, $V9, 1]}
    @{[vsm3c_vi $V26, $V10, 2]}
    @{[vsm3c_vi $V26, $V11, 3]}
    @{[vsm3c_vi $V26, $V1, 4]}
    @{[vsm3c_vi $V26, $V12, 5]}
    @{[vsm3c_vi $V26, $V13, 6]}
    @{[vsm3c_vi $V26, $V14, 7]}
    @{[vsm3c_vi $V26, $V2, 8]}
    @{[vsm3c_vi $V26, $V15, 9]}
    @{[vsm3c_vi $V26, $V16, 10]}
    @{[vsm3c_vi $V26, $V17, 11]}
    @{[vsm3c_vi $V26, $V3, 12]}
    @{[vsm3c_vi $V26, $V18, 13]}
    @{[vsm3c_vi $V26, $V19, 14]}
    @{[vsm3c_vi $V26, $V20, 15]}
    @{[vsm3c_vi $V26, $V4, 16]}
    @{[vsm3c_vi $V26, $V21, 17]}
    @{[vsm3c_vi $V26, $V22, 18]}
    @{[vsm3c_vi $V26, $V23, 19]}
    @{[vsm3c_vi $V26, $V5, 20]}
    @{[vsm3c_vi $V26, $V24, 21]}
    @{[vsm3c_vi $V26, $V25, 22]}
    @{[vrgather_vv $V9, $V25, $V29]}
    @{[vrgather_vv $V10, $V6, $V29]}
    @{[vrgather_vv $V11, $V10, $V29]}
    @{[vrgather_vv $V12, $V7, $V28]}
    @{[vor_vv $V11, $V11, $V12]}
    @{[vrgather_vv $V12, $V11, $V29]}
    @{[vrgather_vv $V13, $V7, $V29]}
    @{[vrgather_vv $V14, $V13, $V29]}
    @{[vrgather_vv $V15, $V8, $V28]}
    @{[vor_vv $V14, $V14, $V15]}
    @{[vrgather_vv $V15, $V14, $V29]}
    @{[vsm3c_vi $V26, $V9, 23]}
    @{[vsm3c_vi $V26, $V6, 24]}
    @{[vsm3c_vi $V26, $V10, 25]}
    @{[vsm3c_vi $V26, $V11, 26]}
    @{[vsm3c_vi $V26, $V12, 27]}
    @{[vsm3c_vi $V26, $V7, 28]}
    @{[vsm3c_vi $V26, $V13, 29]}
    @{[vsm3c_vi $V26, $V14, 30]}
    @{[vsm3c_vi $V26, $V15, 31]}
    @{[vxor_vv $V26, $V26, $V27]}
    @{[vslidedown_vi $V27, $V26, 8]}
    @{[vmv_v_v $V26, $V27]}
    addi $EVENNUM , $EVENNUM , -1
    addi $INPUT, $INPUT, 128
    bnez $EVENNUM , L_sm3_loop_zvl512
    @{[vsetivli "zero", 8, "e32", "m1", "ta", "ma"]}
    @{[vrev8_v $V26, $V26]}
    @{[vse32_v $V26, $CTX]}
    bnez $NUM, ossl_hwsm3_block_data_order_zvksh_zvl256
    ret
ossl_hwsm3_block_data_order_zvksh_zvl256:
    @{[vsetivli "zero", 8, "e32", "m1", "ta", "ma"]}
    j ossl_hwsm3_block_data_order_zvksh_single
ossl_hwsm3_block_data_order_zvksh_zvl128:
    @{[vsetivli "zero", 8, "e32", "m2", "ta", "ma"]}
ossl_hwsm3_block_data_order_zvksh_single:
    # Load initial state of hash context (c->A-H).
    @{[vle32_v $V0, $CTX]}
    @{[vrev8_v $V0, $V0]}

L_sm3_loop:
    # Copy the previous state to v2.
    # It will be XOR'ed with the current state at the end of the round.
    @{[vmv_v_v $V2, $V0]}

    # Load the 64B block in 2x32B chunks.
    @{[vle32_v $V6, $INPUT]} # v6 := {w7, ..., w0}
    addi $INPUT, $INPUT, 32

    @{[vle32_v $V8, $INPUT]} # v8 := {w15, ..., w8}
    addi $INPUT, $INPUT, 32

    addi $NUM, $NUM, -1

    # As vsm3c consumes only w0, w1, w4, w5 we need to slide the input
    # 2 elements down so we process elements w2, w3, w6, w7
    # This will be repeated for each odd round.
    @{[vslidedown_vi $V4, $V6, 2]} # v4 := {X, X, w7, ..., w2}

    @{[vsm3c_vi $V0, $V6, 0]}
    @{[vsm3c_vi $V0, $V4, 1]}

    # Prepare a vector with {w11, ..., w4}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w7, ..., w4}
    @{[vslideup_vi $V4, $V8, 4]}   # v4 := {w11, w10, w9, w8, w7, w6, w5, w4}

    @{[vsm3c_vi $V0, $V4, 2]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w11, w10, w9, w8, w7, w6}
    @{[vsm3c_vi $V0, $V4, 3]}

    @{[vsm3c_vi $V0, $V8, 4]}
    @{[vslidedown_vi $V4, $V8, 2]} # v4 := {X, X, w15, w14, w13, w12, w11, w10}
    @{[vsm3c_vi $V0, $V4, 5]}

    @{[vsm3me_vv $V6, $V8, $V6]}   # v6 := {w23, w22, w21, w20, w19, w18, w17, w16}

    # Prepare a register with {w19, w18, w17, w16, w15, w14, w13, w12}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w15, w14, w13, w12}
    @{[vslideup_vi $V4, $V6, 4]}   # v4 := {w19, w18, w17, w16, w15, w14, w13, w12}

    @{[vsm3c_vi $V0, $V4, 6]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w19, w18, w17, w16, w15, w14}
    @{[vsm3c_vi $V0, $V4, 7]}

    @{[vsm3c_vi $V0, $V6, 8]}
    @{[vslidedown_vi $V4, $V6, 2]} # v4 := {X, X, w23, w22, w21, w20, w19, w18}
    @{[vsm3c_vi $V0, $V4, 9]}

    @{[vsm3me_vv $V8, $V6, $V8]}   # v8 := {w31, w30, w29, w28, w27, w26, w25, w24}

    # Prepare a register with {w27, w26, w25, w24, w23, w22, w21, w20}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w23, w22, w21, w20}
    @{[vslideup_vi $V4, $V8, 4]}   # v4 := {w27, w26, w25, w24, w23, w22, w21, w20}

    @{[vsm3c_vi $V0, $V4, 10]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w27, w26, w25, w24, w23, w22}
    @{[vsm3c_vi $V0, $V4, 11]}

    @{[vsm3c_vi $V0, $V8, 12]}
    @{[vslidedown_vi $V4, $V8, 2]} # v4 := {x, X, w31, w30, w29, w28, w27, w26}
    @{[vsm3c_vi $V0, $V4, 13]}

    @{[vsm3me_vv $V6, $V8, $V6]}   # v6 := {w32, w33, w34, w35, w36, w37, w38, w39}

    # Prepare a register with {w35, w34, w33, w32, w31, w30, w29, w28}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w31, w30, w29, w28}
    @{[vslideup_vi $V4, $V6, 4]}   # v4 := {w35, w34, w33, w32, w31, w30, w29, w28}

    @{[vsm3c_vi $V0, $V4, 14]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w35, w34, w33, w32, w31, w30}
    @{[vsm3c_vi $V0, $V4, 15]}

    @{[vsm3c_vi $V0, $V6, 16]}
    @{[vslidedown_vi $V4, $V6, 2]} # v4 := {X, X, w39, w38, w37, w36, w35, w34}
    @{[vsm3c_vi $V0, $V4, 17]}

    @{[vsm3me_vv $V8, $V6, $V8]}   # v8 := {w47, w46, w45, w44, w43, w42, w41, w40}

    # Prepare a register with {w43, w42, w41, w40, w39, w38, w37, w36}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w39, w38, w37, w36}
    @{[vslideup_vi $V4, $V8, 4]}   # v4 := {w43, w42, w41, w40, w39, w38, w37, w36}

    @{[vsm3c_vi $V0, $V4, 18]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w43, w42, w41, w40, w39, w38}
    @{[vsm3c_vi $V0, $V4, 19]}

    @{[vsm3c_vi $V0, $V8, 20]}
    @{[vslidedown_vi $V4, $V8, 2]} # v4 := {X, X, w47, w46, w45, w44, w43, w42}
    @{[vsm3c_vi $V0, $V4, 21]}

    @{[vsm3me_vv $V6, $V8, $V6]}   # v6 := {w55, w54, w53, w52, w51, w50, w49, w48}

    # Prepare a register with {w51, w50, w49, w48, w47, w46, w45, w44}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w47, w46, w45, w44}
    @{[vslideup_vi $V4, $V6, 4]}   # v4 := {w51, w50, w49, w48, w47, w46, w45, w44}

    @{[vsm3c_vi $V0, $V4, 22]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w51, w50, w49, w48, w47, w46}
    @{[vsm3c_vi $V0, $V4, 23]}

    @{[vsm3c_vi $V0, $V6, 24]}
    @{[vslidedown_vi $V4, $V6, 2]} # v4 := {X, X, w55, w54, w53, w52, w51, w50}
    @{[vsm3c_vi $V0, $V4, 25]}

    @{[vsm3me_vv $V8, $V6, $V8]}   # v8 := {w63, w62, w61, w60, w59, w58, w57, w56}

    # Prepare a register with {w59, w58, w57, w56, w55, w54, w53, w52}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w55, w54, w53, w52}
    @{[vslideup_vi $V4, $V8, 4]}   # v4 := {w59, w58, w57, w56, w55, w54, w53, w52}

    @{[vsm3c_vi $V0, $V4, 26]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w59, w58, w57, w56, w55, w54}
    @{[vsm3c_vi $V0, $V4, 27]}

    @{[vsm3c_vi $V0, $V8, 28]}
    @{[vslidedown_vi $V4, $V8, 2]} # v4 := {X, X, w63, w62, w61, w60, w59, w58}
    @{[vsm3c_vi $V0, $V4, 29]}

    @{[vsm3me_vv $V6, $V8, $V6]}   # v6 := {w71, w70, w69, w68, w67, w66, w65, w64}

    # Prepare a register with {w67, w66, w65, w64, w63, w62, w61, w60}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, X, X, w63, w62, w61, w60}
    @{[vslideup_vi $V4, $V6, 4]}   # v4 := {w67, w66, w65, w64, w63, w62, w61, w60}

    @{[vsm3c_vi $V0, $V4, 30]}
    @{[vslidedown_vi $V4, $V4, 2]} # v4 := {X, X, w67, w66, w65, w64, w63, w62}
    @{[vsm3c_vi $V0, $V4, 31]}

    # XOR in the previous state.
    @{[vxor_vv $V0, $V0, $V2]}

    bnez $NUM, L_sm3_loop     # Check if there are any more block to process
L_sm3_end:
    @{[vrev8_v $V0, $V0]}
    @{[vse32_v $V0, $CTX]}
    ret

.size ossl_hwsm3_block_data_order_zvksh,.-ossl_hwsm3_block_data_order_zvksh

.section .rodata
.p2align 3
.type ORDER_BY_ZVL512_DATA,\@object
ORDER_BY_ZVL512_DATA:
    .word 0, 4, 8, 12, 16, 20, 24, 28, 64, 68, 72, 76, 80, 84, 88, 92, 32, 36, 40, 44, 48, 52, 56, 60, 96, 100, 104, 108, 112, 116, 120, 124
.size ORDER_BY_ZVL512_DATA, .-ORDER_BY_ZVL512_DATA

.p2align 3
.type ORDER_BY_ZVL512_EXP,\@object
ORDER_BY_ZVL512_EXP:
    .word 2, 3, 4, 5, 6, 7, 255, 255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 0, 1, 2, 3, 255, 255, 255, 255, 8, 9, 10, 11
.size ORDER_BY_ZVL512_EXP, .-ORDER_BY_ZVL512_EXP
___
}

print $code;

close STDOUT or die "error closing STDOUT: $!";
