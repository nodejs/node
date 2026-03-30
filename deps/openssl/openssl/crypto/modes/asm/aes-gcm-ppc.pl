#! /usr/bin/env perl
# Copyright 2014-2026 The OpenSSL Project Authors. All Rights Reserved.
# Copyright 2025- IBM Corp. All rights reserved
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
#===================================================================================
# Accelerated AES-GCM stitched implementation for ppc64le.
#
# Written by Danny Tsen <dtsen@us.ibm.com>
#
# GHASH is based on the Karatsuba multiplication method.
#
#    Xi xor X1
#
#    X1 * H^4 + X2 * H^3 + x3 * H^2 + X4 * H =
#      (X1.h * H4.h + xX.l * H4.l + X1 * H4) +
#      (X2.h * H3.h + X2.l * H3.l + X2 * H3) +
#      (X3.h * H2.h + X3.l * H2.l + X3 * H2) +
#      (X4.h * H.h + X4.l * H.l + X4 * H)
#
# Xi = v0
# H Poly = v2
# Hash keys = v3 - v14
#     ( H.l, H, H.h)
#     ( H^2.l, H^2, H^2.h)
#     ( H^3.l, H^3, H^3.h)
#     ( H^4.l, H^4, H^4.h)
#
# v30 is IV
# v31 - counter 1
#
# AES used,
#     vs0 - round key 0
#     v15, v16, v17, v18, v19, v20, v21, v22 for 8 blocks (encrypted)
#
# This implementation uses stitched AES-GCM approach to improve overall performance.
# AES is implemented with 8x blocks and GHASH is using 2 4x blocks.
#
# ===================================================================================
#
use strict;
use warnings;

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
my $output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
my $flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$output and open STDOUT,">$output";

my $code.=<<___;
.machine        "any"
.text

.macro SAVE_REGS
	mflr 0
	std 0, 16(1)
	stdu 1,-512(1)

	std	14, 112(1)
	std	15, 120(1)
	std	16, 128(1)
	std	17, 136(1)
	std	18, 144(1)
	std	19, 152(1)
	std	20, 160(1)
	std	21, 168(1)
	std	22, 176(1)
	std	23, 184(1)
	std	24, 192(1)

	stxv	32+20, 256(1)
	stxv	32+21, 256+16(1)
	stxv	32+22, 256+32(1)
	stxv	32+23, 256+48(1)
	stxv	32+24, 256+64(1)
	stxv	32+25, 256+80(1)
	stxv	32+26, 256+96(1)
	stxv	32+27, 256+112(1)
	stxv	32+28, 256+128(1)
	stxv	32+29, 256+144(1)
	stxv	32+30, 256+160(1)
	stxv	32+31, 256+176(1)
.endm # SAVE_REGS

.macro RESTORE_REGS
	lxv	32+20, 256(1)
	lxv	32+21, 256+16(1)
	lxv	32+22, 256+32(1)
	lxv	32+23, 256+48(1)
	lxv	32+24, 256+64(1)
	lxv	32+25, 256+80(1)
	lxv	32+26, 256+96(1)
	lxv	32+27, 256+112(1)
	lxv	32+28, 256+128(1)
	lxv	32+29, 256+144(1)
	lxv	32+30, 256+160(1)
	lxv	32+31, 256+176(1)

	ld	14, 112(1)
	ld	15, 120(1)
	ld	16, 128(1)
	ld	17, 136(1)
	ld	18, 144(1)
	ld	19, 152(1)
	ld	20, 160(1)
	ld	21, 168(1)
	ld	22, 176(1)
	ld	23, 184(1)
	ld	24, 192(1)

	addi    1, 1, 512
	ld 0, 16(1)
	mtlr 0
.endm # RESTORE_REGS

# 4x loops
.macro AES_CIPHER_4x r
	vcipher	15, 15, \\r
	vcipher	16, 16, \\r
	vcipher	17, 17, \\r
	vcipher	18, 18, \\r
.endm

# 8x loops
.macro AES_CIPHER_8x r
	vcipher	15, 15, \\r
	vcipher	16, 16, \\r
	vcipher	17, 17, \\r
	vcipher	18, 18, \\r
	vcipher	19, 19, \\r
	vcipher	20, 20, \\r
	vcipher	21, 21, \\r
	vcipher	22, 22, \\r
.endm

.macro LOOP_8AES_STATE
	AES_CIPHER_8x 23
	AES_CIPHER_8x 24
	AES_CIPHER_8x 25
	AES_CIPHER_8x 26
	AES_CIPHER_8x 27
	AES_CIPHER_8x 28
	AES_CIPHER_8x 29
	AES_CIPHER_8x 1
.endm

#
# PPC_GFMUL128_8x: Compute hash values of 8 blocks based on Karatsuba method.
#
# S1 should xor with the previous digest
#
# Xi = v0
# H Poly = v2
# Hash keys = v3 - v14
# vs10: vpermxor vector
# Scratch: v23 - v29
#
.macro PPC_GFMUL128_8x

	vpmsumd	23, 12, 15		# H4.L * X.L
	vpmsumd	24, 9, 16
	vpmsumd	25, 6, 17
	vpmsumd	26, 3, 18

	vxor	23, 23, 24
	vxor	23, 23, 25
	vxor	23, 23, 26		# L

	vpmsumd	27, 13, 15		# H4.L * X.H + H4.H * X.L
	vpmsumd	28, 10, 16		# H3.L * X1.H + H3.H * X1.L
	vpmsumd	25, 7, 17
	vpmsumd	26, 4, 18

	vxor	24, 27, 28
	vxor	24, 24, 25
	vxor	24, 24, 26		# M

	vpmsumd	26, 14, 15		# H4.H * X.H
	vpmsumd	27, 11, 16
	vpmsumd	28, 8, 17
	vpmsumd	29, 5, 18

	vxor	26, 26, 27
	vxor	26, 26, 28
	vxor	26, 26, 29

	# sum hash and reduction with H Poly
	vpmsumd	28, 23, 2		# reduction

	vxor	1, 1, 1
	vsldoi	25, 24, 1, 8		# mL
	vsldoi	1, 1, 24, 8		# mH
	vxor	23, 23, 25		# mL + L

	# This performs swap and xor like,
	#   vsldoi	23, 23, 23, 8		# swap
	#   vxor	23, 23, 28
	xxlor	32+29, 10, 10
	vpermxor 23, 23, 28, 29

	vxor	24, 26, 1		# H

	# sum hash and reduction with H Poly
	#
	#  vsldoi 25, 23, 23, 8		# swap
	#  vpmsumd 23, 23, 2
	#  vxor	27, 25, 24
	#
	vpermxor 27, 23, 24, 29
	vpmsumd	23, 23, 2
	vxor	0, 23, 27		# Digest of 4 blocks

	vxor	19, 19, 0

	# Compute digest for the next 4 blocks
	vpmsumd	24, 9, 20
	vpmsumd	25, 6, 21
	vpmsumd	26, 3, 22
	vpmsumd	23, 12, 19		# H4.L * X.L

	vxor	23, 23, 24
	vxor	23, 23, 25
	vxor	23, 23, 26		# L

	vpmsumd	27, 13, 19		# H4.L * X.H + H4.H * X.L
	vpmsumd	28, 10, 20		# H3.L * X1.H + H3.H * X1.L
	vpmsumd	25, 7, 21
	vpmsumd	26, 4, 22

	vxor	24, 27, 28
	vxor	24, 24, 25
	vxor	24, 24, 26		# M

	vpmsumd	26, 14, 19		# H4.H * X.H
	vpmsumd	27, 11, 20
	vpmsumd	28, 8, 21
	vpmsumd	29, 5, 22

	vxor	26, 26, 27
	vxor	26, 26, 28
	vxor	26, 26, 29

	# sum hash and reduction with H Poly
	vpmsumd	28, 23, 2		# reduction

	vxor	1, 1, 1
	vsldoi	25, 24, 1, 8		# mL
	vsldoi	1, 1, 24, 8		# mH
	vxor	23, 23, 25		# mL + L

	# This performs swap and xor like,
	#   vsldoi	23, 23, 23, 8		# swap
	#   vxor	23, 23, 28
	xxlor	32+29, 10, 10
	vpermxor 23, 23, 28, 29

	vxor	24, 26, 1		# H

	# sum hash and reduction with H Poly
	#
	#  vsldoi 25, 23, 23, 8		# swap
	#  vpmsumd 23, 23, 2
	#  vxor	27, 25, 24
	#
	vpermxor 27, 23, 24, 29
	vpmsumd	23, 23, 2
	vxor	0, 23, 27		# Digest of 8 blocks
.endm

#
# Compute update single ghash
# vs10: vpermxor vector
# scratch: v1, v22..v27
#
.macro PPC_GHASH1x H S1

	vxor	1, 1, 1

	vpmsumd	22, 3, \\S1		# L
	vpmsumd	23, 4, \\S1		# M
	vpmsumd	24, 5, \\S1		# H

	vpmsumd	27, 22, 2		# reduction

	vsldoi	25, 23, 1, 8		# mL
	vsldoi	26, 1, 23, 8		# mH
	vxor	22, 22, 25		# LL + LL
	vxor	24, 24, 26		# HH + HH

	xxlor	32+25, 10, 10
	vpermxor 22, 22, 27, 25

	#  vsldoi 23, 22, 22, 8		# swap
	#  vpmsumd 22, 22, 2		# reduction
	#  vxor	23, 23, 24
	vpermxor 23, 22, 24, 25
	vpmsumd	22, 22, 2		# reduction

	vxor	\\H, 22, 23
.endm

#
# LOAD_HASH_TABLE
# Xi = v0
# H Poly = v2
# Hash keys = v3 - v14
#
.macro LOAD_HASH_TABLE
	# Load Xi
	lxvb16x	32, 0, 8	# load Xi

	vxor	1, 1, 1

	li	10, 32
	lxvd2x	2+32, 10, 8	# H Poli

	# load Hash - h^4, h^3, h^2, h
	li	10, 64
	lxvd2x	4+32, 10, 8	# H
	vsldoi	3, 1, 4, 8	# l
	vsldoi	5, 4, 1, 8	# h
	li	10, 112
	lxvd2x	7+32, 10, 8	# H^2
	vsldoi	6, 1, 7, 8	# l
	vsldoi	8, 7, 1, 8	# h
	li	10, 160
	lxvd2x	10+32, 10, 8	# H^3
	vsldoi	9, 1, 10, 8	# l
	vsldoi	11, 10, 1, 8	# h
	li	10, 208
	lxvd2x	13+32, 10, 8	# H^4
	vsldoi	12, 1, 13, 8	# l
	vsldoi	14, 13, 1, 8	# h
.endm

.macro PROCESS_8X_AES_STATES
	vcipherlast     15, 15, 1
	vcipherlast     16, 16, 1
	vcipherlast     17, 17, 1
	vcipherlast     18, 18, 1
	vcipherlast     19, 19, 1
	vcipherlast     20, 20, 1
	vcipherlast     21, 21, 1
	vcipherlast     22, 22, 1

	lxvb16x	32+23, 0, 14	# load block
	lxvb16x	32+24, 15, 14	# load block
	lxvb16x	32+25, 16, 14	# load block
	lxvb16x	32+26, 17, 14	# load block
	lxvb16x	32+27, 18, 14	# load block
	lxvb16x	32+28, 19, 14	# load block
	lxvb16x	32+29, 20, 14	# load block
	lxvb16x	32+30, 21, 14	# load block
	addi	14, 14, 128

	vxor	15, 15, 23
	vxor	16, 16, 24
	vxor	17, 17, 25
	vxor	18, 18, 26
	vxor	19, 19, 27
	vxor	20, 20, 28
	vxor	21, 21, 29
	vxor	22, 22, 30

	stxvb16x 47, 0, 9	# store output
	stxvb16x 48, 15, 9	# store output
	stxvb16x 49, 16, 9	# store output
	stxvb16x 50, 17, 9	# store output
	stxvb16x 51, 18, 9	# store output
	stxvb16x 52, 19, 9	# store output
	stxvb16x 53, 20, 9	# store output
	stxvb16x 54, 21, 9	# store output
	addi	9, 9, 128
.endm

.macro COMPUTE_STATES
	xxlor	32+15, 9, 9		# last state
	vadduwm 15, 15, 31		# state + counter
	vadduwm 16, 15, 31
	vadduwm 17, 16, 31
	vadduwm 18, 17, 31
	vadduwm 19, 18, 31
	vadduwm 20, 19, 31
	vadduwm 21, 20, 31
	vadduwm 22, 21, 31
	xxlor	9, 32+22, 32+22		# save last state

        xxlxor	32+15, 32+15, 0		# IV + round key - add round key 0
	xxlxor	32+16, 32+16, 0
	xxlxor	32+17, 32+17, 0
	xxlxor	32+18, 32+18, 0
	xxlxor	32+19, 32+19, 0
	xxlxor	32+20, 32+20, 0
	xxlxor	32+21, 32+21, 0
	xxlxor	32+22, 32+22, 0
.endm

################################################################################
# Compute AES and ghash one block at a time.
# r23: AES rounds
# v30: current IV
# vs0: roundkey 0
#
################################################################################
.align 4
aes_gcm_crypt_1x:
.localentry	aes_gcm_crypt_1x,0

	cmpdi	5, 16
	bge	__More_1x
	blr
__More_1x:
	li      10, 16
	divdu   12, 5, 10

	xxlxor	32+15, 32+30, 0

	# Pre-load 8 AES rounds to scratch vectors.
	lxv	32+16, 16(6)		# round key 1
	lxv	32+17, 32(6)		# round key 2
	lxv	32+18, 48(6)		# round key 3
	lxv	32+19, 64(6)		# round key 4
	lxv	32+20, 80(6)		# round key 5
	lxv	32+21, 96(6)		# round key 6
	lxv	32+28, 112(6)		# round key 7
	lxv	32+29, 128(6)		# round key 8

	lwz	23, 240(6)	# n rounds
	addi	22, 23, -9	# remaining AES rounds

	cmpdi	12, 0
	bgt	__Loop_1x
	blr

__Loop_1x:
	mtctr	22
	addi	10, 6, 144
	vcipher	15, 15, 16
	vcipher	15, 15, 17
	vcipher	15, 15, 18
	vcipher	15, 15, 19
	vcipher	15, 15, 20
	vcipher	15, 15, 21
	vcipher	15, 15, 28
	vcipher	15, 15, 29

__Loop_aes_1state:
	lxv	32+1, 0(10)
	vcipher	15, 15, 1
	addi	10, 10, 16
	bdnz	__Loop_aes_1state
	lxv	32+1, 0(10)		# last round key
	lxvb16x 11, 0, 14		# load input block
	vcipherlast 15, 15, 1

	xxlxor	32+15, 32+15, 11
	stxvb16x 32+15, 0, 9	# store output
	addi	14, 14, 16
	addi	9, 9, 16

	cmpdi	24, 0	# decrypt?
	bne	__Encrypt_1x
	xxlor	15+32, 11, 11
__Encrypt_1x:
	vxor	15, 15, 0
	PPC_GHASH1x 0, 15

	addi	5, 5, -16
	addi	11, 11, 16

	vadduwm 30, 30, 31		# IV + counter
	xxlxor	32+15, 32+30, 0
	addi	12, 12, -1
	cmpdi	12, 0
	bgt	__Loop_1x

	stxvb16x 32+0, 0, 8		# update Xi
	blr
.size   aes_gcm_crypt_1x,.-aes_gcm_crypt_1x

################################################################################
# Process a normal partial block when we come here.
#  Compute partial mask, Load and store partial block to stack.
#  Compute AES state.
#   Compute ghash.
#
################################################################################
.align 4
__Process_partial:
.localentry	__Process_partial,0

	# create partial mask
	vspltisb 16, -1
	li	12, 16
	sub	12, 12, 5
	sldi	12, 12, 3
	mtvsrdd	32+17, 0, 12
	vslo	16, 16, 17		# partial block mask

	lxvb16x 11, 0, 14		# load partial block
	xxland	11, 11, 32+16

	# AES crypt partial
	xxlxor	32+15, 32+30, 0
	lwz	23, 240(6)		# n rounds
	addi	22, 23, -1		# loop - 1
	mtctr	22
	addi	10, 6, 16

__Loop_aes_pstate:
	lxv	32+1, 0(10)
	vcipher	15, 15, 1
	addi	10, 10, 16
	bdnz	__Loop_aes_pstate
	lxv	32+1, 0(10)		# last round key
	vcipherlast 15, 15, 1

	xxlxor	32+15, 32+15, 11
	vand	15, 15, 16

	# AES crypt output v15
	# Write partial
	li	10, 224
	stxvb16x 15+32, 10, 1		# write v15 to stack
	addi	10, 1, 223
	addi	12, 9, -1
        mtctr	5			# partial block len
__Write_partial:
        lbzu	22, 1(10)
	stbu	22, 1(12)
        bdnz	__Write_partial

	cmpdi	24, 0			# decrypt?
	bne	__Encrypt_partial
	xxlor	32+15, 11, 11		# decrypt using the input block
__Encrypt_partial:
	vxor	15, 15, 0		# ^ previous hash
	PPC_GHASH1x 0, 15
	li	5, 0			# done last byte
	stxvb16x 32+0, 0, 8		# Update X1
	blr
.size   __Process_partial,.-__Process_partial

################################################################################
# ppc_aes_gcm_encrypt (const void *inp, void *out, size_t len,
#               const char *rk, unsigned char iv[16], void *Xip);
#
#    r3 - inp
#    r4 - out
#    r5 - len
#    r6 - AES round keys
#    r7 - iv
#    r8 - Xi, HPoli, hash keys
#
#    rounds is at offset 240 in rk
#    Xi is at 0 in gcm_table (Xip).
#
################################################################################
.global ppc_aes_gcm_encrypt
.align 5
ppc_aes_gcm_encrypt:
.localentry     ppc_aes_gcm_encrypt,0

	SAVE_REGS
	LOAD_HASH_TABLE

	# initialize ICB: GHASH( IV ), IV - r7
	lxvb16x	30+32, 0, 7	# load IV  - v30

	mr	14, 3
	mr	9, 4

	# counter 1
	vxor	31, 31, 31
	vspltisb 22, 1
	vsldoi	31, 31, 22,1	# counter 1

	addis	11, 2, permx\@toc\@ha
	addi	11, 11, permx\@toc\@l
	lxv	10, 0(11)	# vs10: vpermxor vector
	li	11, 0

	lxv	0, 0(6)			# round key 0

	#
	# Process different blocks
	#
	cmpdi	5, 128
	blt	__Process_more_enc

	# load 9 round keys
	lxv	32+23, 16(6)		# round key 1
	lxv	32+24, 32(6)		# round key 2
	lxv	32+25, 48(6)		# round key 3
	lxv	32+26, 64(6)		# round key 4
	lxv	32+27, 80(6)		# round key 5
	lxv	32+28, 96(6)		# round key 6
	lxv	32+29, 112(6)		# round key 7
	lxv	32+1, 128(6)		# round key 8

	# load rounds - 10 (128), 12 (192), 14 (256)
	lwz	23, 240(6)		# n rounds

__Process_encrypt:
#
# Process 8x AES/GCM blocks
#
__Process_8x_enc:
	# 8x blocks
	li	10, 128
	divdu	12, 5, 10	# n 128 bytes-blocks

	addi	12, 12, -1	# loop - 1

	vmr	15, 30		# first state: IV
	vadduwm	16, 15, 31	# state + counter
	vadduwm	17, 16, 31
	vadduwm	18, 17, 31
	vadduwm	19, 18, 31
	vadduwm	20, 19, 31
	vadduwm	21, 20, 31
	vadduwm	22, 21, 31
	xxlor	9, 32+22, 32+22	# save last state

	# vxor  state, state, w # addroundkey
	xxlxor	32+15, 32+15, 0      # IV + round key - add round key 0
	xxlxor	32+16, 32+16, 0
	xxlxor	32+17, 32+17, 0
	xxlxor	32+18, 32+18, 0
	xxlxor	32+19, 32+19, 0
	xxlxor	32+20, 32+20, 0
	xxlxor	32+21, 32+21, 0
	xxlxor	32+22, 32+22, 0

	li	15, 16
	li	16, 32
	li	17, 48
	li	18, 64
	li	19, 80
	li	20, 96
	li	21, 112

	#
	# Pre-compute first 8 AES state and leave 1/3/5 more rounds
	# for the loop.
	#
	addi	22, 23, -9		# process 8 keys
	mtctr	22			# AES key loop
	addi	10, 6, 144

	LOOP_8AES_STATE			# process 8 AES keys

__PreLoop_aes_state:
	lxv	32+1, 0(10)		# round key
	AES_CIPHER_8x 1
	addi	10, 10, 16
	bdnz	__PreLoop_aes_state
	lxv	32+1, 0(10)		# last round key (v1)

	cmpdi	12, 0			# Only one loop (8 block)
	beq	__Finish_ghash

#
# Loop 8x blocks and compute ghash
#
__Loop_8x_block_enc:
	PROCESS_8X_AES_STATES

	# Compute ghash here
	vxor	15, 15, 0
	PPC_GFMUL128_8x

	COMPUTE_STATES

	addi    5, 5, -128
	addi    11, 11, 128

	lxv	32+23, 16(6)		# round key 1
	lxv	32+24, 32(6)		# round key 2
	lxv	32+25, 48(6)		# round key 3
	lxv	32+26, 64(6)		# round key 4
	lxv	32+27, 80(6)		# round key 5
	lxv	32+28, 96(6)		# round key 6
	lxv	32+29, 112(6)		# round key 7
	lxv	32+1, 128(6)		# round key 8

	# Compute first 8 AES state and leave 1/3/5 more rounds
	# for the loop.
	LOOP_8AES_STATE			# process 8 AES keys
	mtctr	22			# AES key loop
	addi	10, 6, 144

__LastLoop_aes_state:
	lxv	32+1, 0(10)		# round key
	AES_CIPHER_8x 1
	addi	10, 10, 16
	bdnz	__LastLoop_aes_state

	lxv	32+1, 0(10)		# last round key (v1)

	addi	12, 12, -1
	cmpdi	12, 0
	bne	__Loop_8x_block_enc

	#
	# Remainng blocks
	#
__Finish_ghash:
	PROCESS_8X_AES_STATES

	# Compute ghash here
	vxor	15, 15, 0
	PPC_GFMUL128_8x

	# Update IV and Xi
	xxlor	30+32, 9, 9		# last ctr
	vadduwm	30, 30, 31		# increase ctr
	stxvb16x 32+0, 0, 8		# update Xi

	addi    5, 5, -128
	addi    11, 11, 128

	#
	# Done 8x blocks
	#

	cmpdi   5, 0
	beq     aes_gcm_out

__Process_more_enc:
	li	24, 1			# encrypt
	bl	aes_gcm_crypt_1x
	cmpdi   5, 0
	beq     aes_gcm_out

	bl	__Process_partial
	b	aes_gcm_out

.size   ppc_aes_gcm_encrypt,.-ppc_aes_gcm_encrypt

################################################################################
# ppc_aes_gcm_decrypt (const void *inp, void *out, size_t len,
#               const char *rk, unsigned char iv[16], void *Xip);
# 8x Decrypt
#
################################################################################
.global ppc_aes_gcm_decrypt
.align 5
ppc_aes_gcm_decrypt:
.localentry	ppc_aes_gcm_decrypt, 0

	SAVE_REGS
	LOAD_HASH_TABLE

	# initialize ICB: GHASH( IV ), IV - r7
	lxvb16x	30+32, 0, 7	# load IV  - v30

	mr	14, 3
	mr	9, 4

	# counter 1
	vxor	31, 31, 31
	vspltisb 22, 1
	vsldoi	31, 31, 22,1	# counter 1

	addis	11, 2, permx\@toc\@ha
	addi	11, 11, permx\@toc\@l
	lxv	10, 0(11)	# vs10: vpermxor vector
	li	11, 0

	lxv	0, 0(6)			# round key 0

	#
	# Process different blocks
	#
	cmpdi	5, 128
	blt	__Process_more_dec

	# load 9 round keys
	lxv	32+23, 16(6)		# round key 1
	lxv	32+24, 32(6)		# round key 2
	lxv	32+25, 48(6)		# round key 3
	lxv	32+26, 64(6)		# round key 4
	lxv	32+27, 80(6)		# round key 5
	lxv	32+28, 96(6)		# round key 6
	lxv	32+29, 112(6)		# round key 7
	lxv	32+1, 128(6)		# round key 8

	# load rounds - 10 (128), 12 (192), 14 (256)
	lwz	23, 240(6)		# n rounds

__Process_decrypt:
#
# Process 8x AES/GCM blocks
#
__Process_8x_dec:
	# 8x blocks
	li	10, 128
	divdu	12, 5, 10	# n 128 bytes-blocks

	addi	12, 12, -1	# loop - 1

	vmr	15, 30		# first state: IV
	vadduwm	16, 15, 31	# state + counter
	vadduwm	17, 16, 31
	vadduwm	18, 17, 31
	vadduwm	19, 18, 31
	vadduwm	20, 19, 31
	vadduwm	21, 20, 31
	vadduwm	22, 21, 31
	xxlor	9, 32+22, 32+22	# save last state

	# vxor  state, state, w # addroundkey
	xxlxor	32+15, 32+15, 0      # IV + round key - add round key 0
	xxlxor	32+16, 32+16, 0
	xxlxor	32+17, 32+17, 0
	xxlxor	32+18, 32+18, 0
	xxlxor	32+19, 32+19, 0
	xxlxor	32+20, 32+20, 0
	xxlxor	32+21, 32+21, 0
	xxlxor	32+22, 32+22, 0

	li	15, 16
	li	16, 32
	li	17, 48
	li	18, 64
	li	19, 80
	li	20, 96
	li	21, 112

	#
	# Pre-compute first 8 AES state and leave 1/3/5 more rounds
	# for the loop.
	#
	addi	22, 23, -9		# process 8 keys
	mtctr	22			# AES key loop
	addi	10, 6, 144

	LOOP_8AES_STATE			# process 8 AES keys

__PreLoop_aes_state_dec:
	lxv	32+1, 0(10)		# round key
	AES_CIPHER_8x 1
	addi	10, 10, 16
	bdnz	__PreLoop_aes_state_dec
	lxv	32+1, 0(10)		# last round key (v1)

	cmpdi	12, 0			# Only one loop (8 block)
	beq	__Finish_ghash_dec

#
# Loop 8x blocks and compute ghash
#
__Loop_8x_block_dec:
	vcipherlast     15, 15, 1
	vcipherlast     16, 16, 1
	vcipherlast     17, 17, 1
	vcipherlast     18, 18, 1
	vcipherlast     19, 19, 1
	vcipherlast     20, 20, 1
	vcipherlast     21, 21, 1
	vcipherlast     22, 22, 1

	lxvb16x	32+23, 0, 14	# load block
	lxvb16x	32+24, 15, 14	# load block
	lxvb16x	32+25, 16, 14	# load block
	lxvb16x	32+26, 17, 14	# load block
	lxvb16x	32+27, 18, 14	# load block
	lxvb16x	32+28, 19, 14	# load block
	lxvb16x	32+29, 20, 14	# load block
	lxvb16x	32+30, 21, 14	# load block
	addi	14, 14, 128

	vxor	15, 15, 23
	vxor	16, 16, 24
	vxor	17, 17, 25
	vxor	18, 18, 26
	vxor	19, 19, 27
	vxor	20, 20, 28
	vxor	21, 21, 29
	vxor	22, 22, 30

	stxvb16x 47, 0, 9	# store output
	stxvb16x 48, 15, 9	# store output
	stxvb16x 49, 16, 9	# store output
	stxvb16x 50, 17, 9	# store output
	stxvb16x 51, 18, 9	# store output
	stxvb16x 52, 19, 9	# store output
	stxvb16x 53, 20, 9	# store output
	stxvb16x 54, 21, 9	# store output

	addi	9, 9, 128

	vmr	15, 23
	vmr	16, 24
	vmr	17, 25
	vmr	18, 26
	vmr	19, 27
	vmr	20, 28
	vmr	21, 29
	vmr	22, 30

	# ghash here
	vxor	15, 15, 0
	PPC_GFMUL128_8x

	xxlor	32+15, 9, 9		# last state
	vadduwm 15, 15, 31		# state + counter
	vadduwm 16, 15, 31
	vadduwm 17, 16, 31
	vadduwm 18, 17, 31
	vadduwm 19, 18, 31
	vadduwm 20, 19, 31
	vadduwm 21, 20, 31
	vadduwm 22, 21, 31
	xxlor	9, 32+22, 32+22		# save last state

	xxlor	32+27, 0, 0		# restore roundkey 0
        vxor    15, 15, 27		# IV + round key - add round key 0
	vxor	16, 16, 27
	vxor	17, 17, 27
	vxor	18, 18, 27
	vxor	19, 19, 27
	vxor	20, 20, 27
	vxor	21, 21, 27
	vxor	22, 22, 27

	addi    5, 5, -128
	addi    11, 11, 128

	lxv	32+23, 16(6)		# round key 1
	lxv	32+24, 32(6)		# round key 2
	lxv	32+25, 48(6)		# round key 3
	lxv	32+26, 64(6)		# round key 4
	lxv	32+27, 80(6)		# round key 5
	lxv	32+28, 96(6)		# round key 6
	lxv	32+29, 112(6)		# round key 7
	lxv	32+1, 128(6)		# round key 8

	LOOP_8AES_STATE			# process 8 AES keys
	mtctr	22			# AES key loop
	addi	10, 6, 144
__LastLoop_aes_state_dec:
	lxv	32+1, 0(10)		# round key
	AES_CIPHER_8x 1
	addi	10, 10, 16
	bdnz	__LastLoop_aes_state_dec
	lxv	32+1, 0(10)		# last round key (v1)

	addi	12, 12, -1
	cmpdi	12, 0
	bne	__Loop_8x_block_dec

__Finish_ghash_dec:
	vcipherlast     15, 15, 1
	vcipherlast     16, 16, 1
	vcipherlast     17, 17, 1
	vcipherlast     18, 18, 1
	vcipherlast     19, 19, 1
	vcipherlast     20, 20, 1
	vcipherlast     21, 21, 1
	vcipherlast     22, 22, 1

	lxvb16x	32+23, 0, 14	# load block
	lxvb16x	32+24, 15, 14	# load block
	lxvb16x	32+25, 16, 14	# load block
	lxvb16x	32+26, 17, 14	# load block
	lxvb16x	32+27, 18, 14	# load block
	lxvb16x	32+28, 19, 14	# load block
	lxvb16x	32+29, 20, 14	# load block
	lxvb16x	32+30, 21, 14	# load block
	addi	14, 14, 128

	vxor	15, 15, 23
	vxor	16, 16, 24
	vxor	17, 17, 25
	vxor	18, 18, 26
	vxor	19, 19, 27
	vxor	20, 20, 28
	vxor	21, 21, 29
	vxor	22, 22, 30

	stxvb16x 47, 0, 9	# store output
	stxvb16x 48, 15, 9	# store output
	stxvb16x 49, 16, 9	# store output
	stxvb16x 50, 17, 9	# store output
	stxvb16x 51, 18, 9	# store output
	stxvb16x 52, 19, 9	# store output
	stxvb16x 53, 20, 9	# store output
	stxvb16x 54, 21, 9	# store output
	addi	9, 9, 128

	vxor	15, 23, 0
	vmr	16, 24
	vmr	17, 25
	vmr	18, 26
	vmr	19, 27
	vmr	20, 28
	vmr	21, 29
	vmr	22, 30

	#vxor	15, 15, 0
	PPC_GFMUL128_8x

	xxlor	30+32, 9, 9		# last ctr
	vadduwm	30, 30, 31		# increase ctr
	stxvb16x 32+0, 0, 8		# update Xi

	addi    5, 5, -128
	addi    11, 11, 128

	#
	# Done 8x blocks
	#

	cmpdi   5, 0
	beq     aes_gcm_out

__Process_more_dec:
	li	24, 0			# decrypt
	bl	aes_gcm_crypt_1x
	cmpdi   5, 0
	beq     aes_gcm_out

	bl	__Process_partial
	b	aes_gcm_out
.size   ppc_aes_gcm_decrypt,.-ppc_aes_gcm_decrypt

aes_gcm_out:
.localentry	aes_gcm_out,0

	mr	3, 11			# return count

	RESTORE_REGS
	blr
.size	aes_gcm_out,.-aes_gcm_out

.rodata
.align 4
# for vector permute and xor
permx:
.long 0x4c5d6e7f, 0x08192a3b, 0xc4d5e6f7, 0x8091a2b3
___

print $code;
close STDOUT or die "error closing STDOUT: $!";
