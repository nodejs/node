#! /usr/bin/env perl
# This file is dual-licensed, meaning that you can use it under your
# choice of either of the following two licenses:
#
# Copyright 2023-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License"). You can obtain
# a copy in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# or
#
# Copyright (c) 2023, Christoph Müllner <christoph.muellner@vrull.eu>
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
# - RISC-V Vector SM4 Block Cipher extension ('Zvksed')

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

my $BLOCK_SIZE = 16;
my $STRIDE = -4;  # Used for reversing word order
my $FOUR_BLOCKS = 64;
my $EIGHT_BLOCKS = 128;
my ($vk0,$vk1,$vk2,$vk3,$vk4,$vk5,$vk6,$vk7)=("v16","v17","v18","v19","v20","v21","v22","v23");
my ($tmp_stride,$tmp_base)=("t1","t2");
# Loading with word order reversed
sub reverse_order_L {
    my $vreg = shift;
    my $base_reg = shift;

    return <<___;
        addi $tmp_base, $base_reg, 12
        @{[vlse32_v $vreg, $tmp_base, $tmp_stride]}
___
}

# Storing with word order reversed
sub reverse_order_S {
    my $vreg = shift;
    my $base_reg = shift;

    return <<___;
        addi $tmp_base, $base_reg, 12
        @{[vsse32_v $vreg, $tmp_base, $tmp_stride]}
___
}

# Load 32 round keys
sub enc_load_key {
    my $keys = shift;

    my $code=<<___;
    # Order of elements was adjusted in set_encrypt_key()
    @{[vle32_v $vk0, $keys]} # rk[0:3]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk1, $keys]} # rk[4:7]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk2, $keys]} # rk[8:11]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk3, $keys]} # rk[12:15]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk4, $keys]} # rk[16:19]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk5, $keys]} # rk[20:23]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk6, $keys]} # rk[24:27]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk7, $keys]} # rk[28:31]
___

    return $code;
}

sub dec_load_key {
    my $keys = shift;

    my $code=<<___;
    # Order of elements was adjusted in set_decrypt_key()
    @{[vle32_v $vk7, $keys]} # rk[31:28]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk6, $keys]} # rk[27:24]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk5, $keys]} # rk[23:20]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk4, $keys]} # rk[19:16]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk3, $keys]} # rk[15:12]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk2, $keys]} # rk[11:8]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk1, $keys]} # rk[7:4]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vle32_v $vk0, $keys]} # rk[3:0]
___

    return $code;
}

# Encrypt with all keys
sub enc_blk {
    my $data = shift;

    my $code=<<___;
    @{[vsm4r_vs $data, $vk0]}
    @{[vsm4r_vs $data, $vk1]}
    @{[vsm4r_vs $data, $vk2]}
    @{[vsm4r_vs $data, $vk3]}
    @{[vsm4r_vs $data, $vk4]}
    @{[vsm4r_vs $data, $vk5]}
    @{[vsm4r_vs $data, $vk6]}
    @{[vsm4r_vs $data, $vk7]}
___

    return $code;
}

# Decrypt with all keys
sub dec_blk {
    my $data = shift;

    my $code=<<___;
    @{[vsm4r_vs $data, $vk7]}
    @{[vsm4r_vs $data, $vk6]}
    @{[vsm4r_vs $data, $vk5]}
    @{[vsm4r_vs $data, $vk4]}
    @{[vsm4r_vs $data, $vk3]}
    @{[vsm4r_vs $data, $vk2]}
    @{[vsm4r_vs $data, $vk1]}
    @{[vsm4r_vs $data, $vk0]}
___

    return $code;
}

# Decrypt 4 blocks with all keys
sub dec_4blks {
    my $data0 = shift;
    my $data1 = shift;
    my $data2 = shift;
    my $data3 = shift;

    my $code=<<___;
    @{[vsm4r_vs $data0, $vk7]}
    @{[vsm4r_vs $data1, $vk7]}
    @{[vsm4r_vs $data2, $vk7]}
    @{[vsm4r_vs $data3, $vk7]}

    @{[vsm4r_vs $data0, $vk6]}
    @{[vsm4r_vs $data1, $vk6]}
    @{[vsm4r_vs $data2, $vk6]}
    @{[vsm4r_vs $data3, $vk6]}

    @{[vsm4r_vs $data0, $vk5]}
    @{[vsm4r_vs $data1, $vk5]}
    @{[vsm4r_vs $data2, $vk5]}
    @{[vsm4r_vs $data3, $vk5]}

    @{[vsm4r_vs $data0, $vk4]}
    @{[vsm4r_vs $data1, $vk4]}
    @{[vsm4r_vs $data2, $vk4]}
    @{[vsm4r_vs $data3, $vk4]}

    @{[vsm4r_vs $data0, $vk3]}
    @{[vsm4r_vs $data1, $vk3]}
    @{[vsm4r_vs $data2, $vk3]}
    @{[vsm4r_vs $data3, $vk3]}

    @{[vsm4r_vs $data0, $vk2]}
    @{[vsm4r_vs $data1, $vk2]}
    @{[vsm4r_vs $data2, $vk2]}
    @{[vsm4r_vs $data3, $vk2]}

    @{[vsm4r_vs $data0, $vk1]}
    @{[vsm4r_vs $data1, $vk1]}
    @{[vsm4r_vs $data2, $vk1]}
    @{[vsm4r_vs $data3, $vk1]}

    @{[vsm4r_vs $data0, $vk0]}
    @{[vsm4r_vs $data1, $vk0]}
    @{[vsm4r_vs $data2, $vk0]}
    @{[vsm4r_vs $data3, $vk0]}
___

    return $code;
}

####
# void rv64i_zvksed_sm4_cbc_encrypt(const unsigned char *in, unsigned char *out,
#                                   size_t len, const SM4_KEY *key,
#                                   unsigned char *iv, int enc);
#
{
my ($in,$out,$len,$keys,$ivp)=("a0","a1","a2","a3","a4");
my ($tmp,$base)=("t0","t2");
my ($vdata0,$vdata1,$vdata2,$vdata3,$vdata4,$vdata5,$vdata6,$vdata7)=("v1","v2","v3","v4","v5","v6","v7","v24");
my ($vivec)=("v8");
my ($vindex)=("v0");

$code .= <<___;
.section .rodata
.align 4
.Lreverse_index:
    .word 3, 2, 1, 0
.text
.p2align 3
.globl rv64i_zvksed_sm4_cbc_encrypt
.type rv64i_zvksed_sm4_cbc_encrypt,\@function
rv64i_zvksed_sm4_cbc_encrypt:
    # check whether the length is a multiple of 16 and >= 16
    li $tmp, $BLOCK_SIZE
    bltu $len, $tmp, .Lcbc_enc_end
    andi $tmp, $len, 15
    bnez $tmp, .Lcbc_enc_end

    @{[vsetivli "zero", 4, "e32", "m1", "ta", "ma"]}
    # Load 32 round keys
    @{[enc_load_key $keys]}

    # Load IV
    @{[vle32_v $vivec, $ivp]}

    # Load the reverse index (for IV updates)
    la $tmp, .Lreverse_index
    @{[vle32_v $vindex, $tmp]}
# =====================================================
# If data length ≥ 64 bytes, process 4 blocks in batch:
# 4-block CBC encryption pipeline:
#   1. Load 4 plaintext blocks
#   2. Reverse bytes for SM4 endianness
#   3. Perform XOR operation with IV or previous ciphertext block (CBC chain)
#   4. Encrypt each data block using the enc_blk function
#   5. Adjust the byte order and store the ciphertext block
#   6. Update the initialization vector (IV)
# If data length < 64 bytes, process it block by block using the Lcbc_enc_single function
# =====================================================
.Lcbc_enc_loop:
    li $tmp, $FOUR_BLOCKS
    bltu $len, $tmp, .Lcbc_enc_single
    # Load input data0-data3
    @{[vle32_v $vdata0, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata1, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata2, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata3, $in]}
    addi $in, $in, $BLOCK_SIZE
    #XOR with IV
    @{[vxor_vv $vdata0, $vdata0, $vivec]}

    @{[vrev8_v $vdata0, $vdata0]}
    # Encrypt with all keys
    @{[enc_blk $vdata0]}
    @{[vrev8_v $vdata0, $vdata0]}

    #Update IV to ciphertext block 0
    @{[vrgather_vv $vivec, $vdata0, $vindex]}

    @{[vxor_vv $vdata1, $vdata1, $vivec]}

    @{[vrev8_v $vdata1, $vdata1]}
    @{[enc_blk $vdata1]}
    @{[vrev8_v $vdata1, $vdata1]}

    #Update IV to ciphertext block 1
    @{[vrgather_vv $vivec, $vdata1, $vindex]}

    @{[vxor_vv $vdata2, $vdata2, $vivec]}

    @{[vrev8_v $vdata2, $vdata2]}
    @{[enc_blk $vdata2]}
    @{[vrev8_v $vdata2, $vdata2]}

    #Update IV to ciphertext block 2
    @{[vrgather_vv $vivec, $vdata2, $vindex]}

    @{[vxor_vv $vdata3, $vdata3, $vivec]}

    @{[vrev8_v $vdata3, $vdata3]}
    @{[enc_blk $vdata3]}
    @{[vrev8_v $vdata3, $vdata3]}

    #Update IV to ciphertext block 3
    @{[vrgather_vv $vivec, $vdata3, $vindex]}

    # Save the ciphertext (in reverse element order)
    li $tmp_stride, $STRIDE
    @{[reverse_order_S $vdata0, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata1, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata2, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata3, $out]}
    addi $out, $out, $BLOCK_SIZE

    addi $len, $len, -$FOUR_BLOCKS
    bnez $len, .Lcbc_enc_loop
    #Save the final IV
    @{[vse32_v $vivec, $ivp]}
    ret

.Lcbc_enc_single:
    # Load input data0
    @{[vle32_v $vdata0, $in]}
    addi $in, $in, $BLOCK_SIZE
    #XOR with IV
    @{[vxor_vv $vdata0, $vdata0, $vivec]}

    @{[vrev8_v $vdata0, $vdata0]}
    # Encrypt with all keys
    @{[enc_blk $vdata0]}
    @{[vrev8_v $vdata0, $vdata0]}

    # Update IV to ciphertext block 0
    @{[vrgather_vv $vivec, $vdata0, $vindex]}

    # Save the ciphertext (in reverse element order)
    li $tmp_stride, $STRIDE
    @{[reverse_order_S $vdata0, $out]}
    addi $out, $out, $BLOCK_SIZE
    addi $len, $len, -$BLOCK_SIZE

    li $tmp, $BLOCK_SIZE
    bgeu $len, $tmp, .Lcbc_enc_single
    # Save the final IV
    @{[vse32_v $vivec, $ivp]}
.Lcbc_enc_end:
    ret
.size rv64i_zvksed_sm4_cbc_encrypt,.-rv64i_zvksed_sm4_cbc_encrypt
___

####
# void rv64i_zvksed_sm4_cbc_decrypt(const unsigned char *in, unsigned char *out,
#                                   size_t len, const SM4_KEY *key,
#                                   unsigned char *iv, int enc);
#
$code .= <<___;
.p2align 3
.globl rv64i_zvksed_sm4_cbc_decrypt
.type rv64i_zvksed_sm4_cbc_decrypt,\@function
rv64i_zvksed_sm4_cbc_decrypt:
    # check whether the length is a multiple of 16 and >= 16
    li $tmp, $BLOCK_SIZE
    bltu $len, $tmp, .Lcbc_dec_end
    andi $tmp, $len, 15
    bnez $tmp, .Lcbc_dec_end

    @{[vsetivli "zero", 4, "e32", "m1", "ta", "ma"]}
    # Load IV (in reverse element order)
    li $tmp_stride, $STRIDE
    @{[reverse_order_L $vivec, $ivp]}

    # Load 32 round keys
    @{[dec_load_key $keys]}
# =====================================================
# If data length ≥ 128 bytes, process 8 blocks in batch:
# 8-block CBC decryption pipeline:
#   1. Load 8 ciphertext blocks
#   2. Reverse bytes for SM4 endianness
#   3. Use two calls to dec_4blks for decrypting each data block
#   4. XOR with previous ciphertext block (CBC chain)
#   5. Update IV and store plaintext with byte reversal
# =====================================================
.Lcbc_dec_loop:
    li $tmp, $EIGHT_BLOCKS
    bltu $len, $tmp, .Lcbc_check_64
    # Load input data0-data7
    @{[vle32_v $vdata0, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata1, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata2, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata3, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata4, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata5, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata6, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata7, $in]}
    addi $in, $in, $BLOCK_SIZE

    @{[vrev8_v $vdata0, $vdata0]}
    @{[vrev8_v $vdata1, $vdata1]}
    @{[vrev8_v $vdata2, $vdata2]}
    @{[vrev8_v $vdata3, $vdata3]}
    @{[vrev8_v $vdata4, $vdata4]}
    @{[vrev8_v $vdata5, $vdata5]}
    @{[vrev8_v $vdata6, $vdata6]}
    @{[vrev8_v $vdata7, $vdata7]}
    # Decrypt 8 data blocks
    @{[dec_4blks $vdata0,$vdata1,$vdata2,$vdata3]}
    @{[dec_4blks $vdata4,$vdata5,$vdata6,$vdata7]}
    @{[vrev8_v $vdata0, $vdata0]}
    @{[vrev8_v $vdata1, $vdata1]}
    @{[vrev8_v $vdata2, $vdata2]}
    @{[vrev8_v $vdata3, $vdata3]}
    @{[vrev8_v $vdata4, $vdata4]}
    @{[vrev8_v $vdata5, $vdata5]}
    @{[vrev8_v $vdata6, $vdata6]}
    @{[vrev8_v $vdata7, $vdata7]}

    @{[vxor_vv $vdata0, $vdata0, $vivec]}

    # Update ciphertext to IV (in reverse element order)
    addi $base, $in, -128
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata1, $vdata1, $vivec]}

    addi $base, $in, -112
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata2, $vdata2, $vivec]}

    addi $base, $in, -96
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata3, $vdata3, $vivec]}

    addi $base, $in, -80
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata4, $vdata4, $vivec]}

    addi $base, $in, -64
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata5, $vdata5, $vivec]}

    addi $base, $in, -48
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata6, $vdata6, $vivec]}

    addi $base, $in, -32
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata7, $vdata7, $vivec]}

    addi $base, $in, -16
    @{[reverse_order_L $vivec, $base]}

    # Save the plaintext (in reverse element order)
    @{[reverse_order_S $vdata0, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata1, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata2, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata3, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata4, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata5, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata6, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata7, $out]}
    addi $out, $out, $BLOCK_SIZE

    addi $len, $len, -$EIGHT_BLOCKS
    bnez $len, .Lcbc_dec_loop
    #Save the final IV (in reverse element order)
    @{[reverse_order_S $vivec, $ivp]}
    ret
# =====================================================
# If data length ≥ 64 bytes, process in batches of 4 blocks:
# 4-block CBC decryption process:
#   1. Load 4 ciphertext blocks
#   2. Reverse byte order to fit SM4 byte order
#   3. Decrypt each data block using the dec_4blks function
#   4. XOR with previous ciphertext block (CBC chain)
#   5. Update IV and store plaintext with byte reversal
# If the data length is less than 64 bytes, process it block by block using the Lcbc_dec_single function
# =====================================================
.Lcbc_check_64:
    li $tmp, $FOUR_BLOCKS
    bltu $len, $tmp, .Lcbc_dec_single
    # Load input data0-data3
    @{[vle32_v $vdata0, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata1, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata2, $in]}
    addi $in, $in, $BLOCK_SIZE
    @{[vle32_v $vdata3, $in]}
    addi $in, $in, $BLOCK_SIZE

    @{[vrev8_v $vdata0, $vdata0]}
    @{[vrev8_v $vdata1, $vdata1]}
    @{[vrev8_v $vdata2, $vdata2]}
    @{[vrev8_v $vdata3, $vdata3]}
    # Decrypt 4 data blocks
    @{[dec_4blks $vdata0,$vdata1,$vdata2,$vdata3]}
    @{[vrev8_v $vdata0, $vdata0]}
    @{[vrev8_v $vdata1, $vdata1]}
    @{[vrev8_v $vdata2, $vdata2]}
    @{[vrev8_v $vdata3, $vdata3]}

    @{[vxor_vv $vdata0, $vdata0, $vivec]}

    # Update ciphertext to IV (in reverse element order)
    addi $base, $in, -64
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata1, $vdata1, $vivec]}

    addi $base, $in, -48
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata2, $vdata2, $vivec]}

    addi $base, $in, -32
    @{[reverse_order_L $vivec, $base]}

    @{[vxor_vv $vdata3, $vdata3, $vivec]}

    addi $base, $in, -16
    @{[reverse_order_L $vivec, $base]}

    # Save the plaintext (in reverse element order)
    @{[reverse_order_S $vdata0, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata1, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata2, $out]}
    addi $out, $out, $BLOCK_SIZE
    @{[reverse_order_S $vdata3, $out]}
    addi $out, $out, $BLOCK_SIZE

    addi $len, $len, -$FOUR_BLOCKS
    bnez $len, .Lcbc_check_64
    #Save the final IV (in reverse element order)
    @{[reverse_order_S $vivec, $ivp]}
    ret

.Lcbc_dec_single:
    # Load input data0
    @{[vle32_v $vdata0, $in]}
    addi $in, $in, $BLOCK_SIZE

    @{[vrev8_v $vdata0, $vdata0]}
    # Decrypt with all keys
    @{[dec_blk $vdata0]}
    @{[vrev8_v $vdata0, $vdata0]}

    #XOR with IV
    @{[vxor_vv $vdata0, $vdata0, $vivec]}

    # Update ciphertext to IV (in reverse element order)
    li $tmp_stride, $STRIDE
    addi $base, $in, -$BLOCK_SIZE
    @{[reverse_order_L $vivec, $base]}
    # Save the plaintext (in reverse element order)
    @{[reverse_order_S $vdata0, $out]}
    addi $out, $out, $BLOCK_SIZE
    addi $len, $len, -$BLOCK_SIZE

    li $tmp, $BLOCK_SIZE
    bgeu $len, $tmp, .Lcbc_dec_single
    #Save the final IV (in reverse element order)
    @{[reverse_order_S $vivec, $ivp]}
.Lcbc_dec_end:
    ret
.size rv64i_zvksed_sm4_cbc_decrypt,.-rv64i_zvksed_sm4_cbc_decrypt
___
}

####
# int rv64i_zvksed_sm4_set_encrypt_key(const unsigned char *userKey,
#                                      SM4_KEY *key);
#
{
my ($ukey,$keys,$fk)=("a0","a1","t0");
my ($vukey,$vfk,$vk0,$vk1,$vk2,$vk3,$vk4,$vk5,$vk6,$vk7)=("v1","v2","v3","v4","v5","v6","v7","v8","v9","v10");
$code .= <<___;
.p2align 3
.globl rv64i_zvksed_sm4_set_encrypt_key
.type rv64i_zvksed_sm4_set_encrypt_key,\@function
rv64i_zvksed_sm4_set_encrypt_key:
    @{[vsetivli__x0_4_e32_m1_tu_mu]}

    # Load the user key
    @{[vle32_v $vukey, $ukey]}
    @{[vrev8_v $vukey, $vukey]}

    # Load the FK.
    la $fk, FK
    @{[vle32_v $vfk, $fk]}

    # Generate round keys.
    @{[vxor_vv $vukey, $vukey, $vfk]}
    @{[vsm4k_vi $vk0, $vukey, 0]} # rk[0:3]
    @{[vsm4k_vi $vk1, $vk0, 1]} # rk[4:7]
    @{[vsm4k_vi $vk2, $vk1, 2]} # rk[8:11]
    @{[vsm4k_vi $vk3, $vk2, 3]} # rk[12:15]
    @{[vsm4k_vi $vk4, $vk3, 4]} # rk[16:19]
    @{[vsm4k_vi $vk5, $vk4, 5]} # rk[20:23]
    @{[vsm4k_vi $vk6, $vk5, 6]} # rk[24:27]
    @{[vsm4k_vi $vk7, $vk6, 7]} # rk[28:31]

    # Store round keys
    @{[vse32_v $vk0, $keys]} # rk[0:3]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk1, $keys]} # rk[4:7]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk2, $keys]} # rk[8:11]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk3, $keys]} # rk[12:15]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk4, $keys]} # rk[16:19]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk5, $keys]} # rk[20:23]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk6, $keys]} # rk[24:27]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vse32_v $vk7, $keys]} # rk[28:31]

    li a0, 1
    ret
.size rv64i_zvksed_sm4_set_encrypt_key,.-rv64i_zvksed_sm4_set_encrypt_key
___
}

####
# int rv64i_zvksed_sm4_set_decrypt_key(const unsigned char *userKey,
#                                      SM4_KEY *key);
#
{
my ($ukey,$keys,$fk,$stride)=("a0","a1","t0","t1");
my ($vukey,$vfk,$vk0,$vk1,$vk2,$vk3,$vk4,$vk5,$vk6,$vk7)=("v1","v2","v3","v4","v5","v6","v7","v8","v9","v10");
$code .= <<___;
.p2align 3
.globl rv64i_zvksed_sm4_set_decrypt_key
.type rv64i_zvksed_sm4_set_decrypt_key,\@function
rv64i_zvksed_sm4_set_decrypt_key:
    @{[vsetivli__x0_4_e32_m1_tu_mu]}

    # Load the user key
    @{[vle32_v $vukey, $ukey]}
    @{[vrev8_v $vukey, $vukey]}

    # Load the FK.
    la $fk, FK
    @{[vle32_v $vfk, $fk]}

    # Generate round keys.
    @{[vxor_vv $vukey, $vukey, $vfk]}
    @{[vsm4k_vi $vk0, $vukey, 0]} # rk[0:3]
    @{[vsm4k_vi $vk1, $vk0, 1]} # rk[4:7]
    @{[vsm4k_vi $vk2, $vk1, 2]} # rk[8:11]
    @{[vsm4k_vi $vk3, $vk2, 3]} # rk[12:15]
    @{[vsm4k_vi $vk4, $vk3, 4]} # rk[16:19]
    @{[vsm4k_vi $vk5, $vk4, 5]} # rk[20:23]
    @{[vsm4k_vi $vk6, $vk5, 6]} # rk[24:27]
    @{[vsm4k_vi $vk7, $vk6, 7]} # rk[28:31]

    # Store round keys in reverse order
    addi $keys, $keys, 12
    li $stride, $STRIDE
    @{[vsse32_v $vk7, $keys, $stride]} # rk[31:28]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk6, $keys, $stride]} # rk[27:24]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk5, $keys, $stride]} # rk[23:20]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk4, $keys, $stride]} # rk[19:16]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk3, $keys, $stride]} # rk[15:12]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk2, $keys, $stride]} # rk[11:8]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk1, $keys, $stride]} # rk[7:4]
    addi $keys, $keys, $BLOCK_SIZE
    @{[vsse32_v $vk0, $keys, $stride]} # rk[3:0]

    li a0, 1
    ret
.size rv64i_zvksed_sm4_set_decrypt_key,.-rv64i_zvksed_sm4_set_decrypt_key
___
}

####
# void rv64i_zvksed_sm4_encrypt(const unsigned char *in, unsigned char *out,
#                               const SM4_KEY *key);
#
{
my ($in,$out,$keys)=("a0","a1","a2");
my ($vdata)=("v1");
$code .= <<___;
.p2align 3
.globl rv64i_zvksed_sm4_encrypt
.type rv64i_zvksed_sm4_encrypt,\@function
rv64i_zvksed_sm4_encrypt:
    @{[vsetivli__x0_4_e32_m1_tu_mu]}

    @{[enc_load_key $keys]}

    # Load input data
    @{[vle32_v $vdata, $in]}
    @{[vrev8_v $vdata, $vdata]}

    # Encrypt with all keys
    @{[enc_blk $vdata]}

    # Save the ciphertext (in reverse element order)
    @{[vrev8_v $vdata, $vdata]}
    li $tmp_stride, $STRIDE
    @{[reverse_order_S $vdata, $out]}

    ret
.size rv64i_zvksed_sm4_encrypt,.-rv64i_zvksed_sm4_encrypt
___
}

####
# void rv64i_zvksed_sm4_decrypt(const unsigned char *in, unsigned char *out,
#                               const SM4_KEY *key);
#
{
my ($in,$out,$keys)=("a0","a1","a2");
my ($vdata)=("v1");
$code .= <<___;
.p2align 3
.globl rv64i_zvksed_sm4_decrypt
.type rv64i_zvksed_sm4_decrypt,\@function
rv64i_zvksed_sm4_decrypt:
    @{[vsetivli__x0_4_e32_m1_tu_mu]}

    @{[dec_load_key $keys]}

    # Load input data
    @{[vle32_v $vdata, $in]}
    @{[vrev8_v $vdata, $vdata]}

    # Decrypt with all keys
    @{[dec_blk $vdata]}

    # Save the plaintext (in reverse element order)
    @{[vrev8_v $vdata, $vdata]}
    li $tmp_stride, $STRIDE
    @{[reverse_order_S $vdata, $out]}

    ret
.size rv64i_zvksed_sm4_decrypt,.-rv64i_zvksed_sm4_decrypt
___
}

$code .= <<___;
# Family Key (little-endian 32-bit chunks)
.p2align 3
FK:
    .word 0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
.size FK,.-FK
___

print $code;

close STDOUT or die "error closing STDOUT: $!";
