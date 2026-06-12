#! /usr/bin/env perl

# Copyright 2023-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}arm-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/arm-xlate.pl" and -f $xlate) or
die "can't locate arm-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

$code=<<___;
#include "arm_arch.h"

/* These are offsets into the CIPH_DIGEST struct */
#define CIPHER_KEY	0
#define CIPHER_KEY_ROUNDS	8
#define CIPHER_IV	16
#define HMAC_IKEYPAD	24
#define HMAC_OKEYPAD	32

.text
.arch armv8-a+crypto
___

sub aes_block_9_rounds() {
	my $i = shift;
$code.=<<___;
	/* aes block $i */
	aese		v$i.16b, v8.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v9.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v10.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v11.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v12.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v13.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v14.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v15.16b
	aesmc		v$i.16b, v$i.16b
	aese		v$i.16b, v16.16b
	aesmc		v$i.16b, v$i.16b
___
}

sub aes_block_last_rounds () {
	my $compare = shift;
	my $label = shift;
	my $i = shift;
	my $load_rk10 = shift;

	if($compare == 1) {
$code.=<<___;
	cmp		x9, #12		/* tell 128,192,256 apart */
___
	}
$code.=<<___;
	b.lt		.Laes128_${label}_$i
.Laes192_${label}_$i:
	ldp		q18,q19,[x7],32		/* rk[10],rk[11] */
	aese		v$i.16b,v17.16b
	aesmc		v$i.16b,v$i.16b
	aese		v$i.16b,v18.16b
	aesmc		v$i.16b,v$i.16b
	b.gt		.Laes256_${label}_$i
	ld1		{v18.16b},[x7]		/* rk[12] */
	aese		v$i.16b,v19.16b
	eor		v$i.16b,v$i.16b,v18.16b
	sub		x7, x7, #32		/* rewind x7 */
	b		1f
.Laes256_${label}_$i:
	aese		v$i.16b,v19.16b
	aesmc		v$i.16b,v$i.16b
	ldp		q18,q19,[x7],32		/* rk[12],rk[13] */
	aese		v$i.16b,v18.16b
	aesmc		v$i.16b,v$i.16b
	ld1		{v18.16b},[x7]		/* rk[14] */
	aese		v$i.16b,v19.16b
	eor		v$i.16b,v$i.16b,v18.16b
	sub		x7, x7, #64		/* rewind x7 */
	b		1f
.Laes128_${label}_$i:
___
	if ($load_rk10 == 1) {
$code.=<<___;
	ld1		{v18.16b},[x7]
___
	}
$code.=<<___;
	aese		v$i.16b,v17.16b
	eor		v$i.16b,v$i.16b,v18.16b	/* res */
1:
___
}

sub aes_block_dec_9_rounds() {
	my $i = shift;
$code.=<<___;
	/* aes block $i */
	aesd		v$i.16b, v8.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v9.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v10.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v11.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v12.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v13.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v14.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v15.16b
	aesimc		v$i.16b, v$i.16b
	aesd		v$i.16b, v16.16b
	aesimc		v$i.16b, v$i.16b
___
}

sub aes_block_dec_last_rounds () {
	my $compare = shift;
	my $label = shift;
	my $i = shift;
	my $load_rk10 = shift;

	if($compare == 1) {
$code.=<<___;
	cmp		x9, #12			/* tell 128,192,256 apart */
___
	}
$code.=<<___;
	b.lt		.Laes128_${label}_$i
.Laes192_${label}_$i:
	ldp		q18,q19,[x7],32			/* rk[10],rk[11] */
	aesd		v$i.16b,v17.16b
	aesimc		v$i.16b,v$i.16b
	aesd		v$i.16b,v18.16b
	aesimc		v$i.16b,v$i.16b
	b.gt		.Laes256_${label}_$i
	ld1		{v18.16b},[x7]			/* rk[12] */
	aesd		v$i.16b,v19.16b
	eor		v$i.16b,v$i.16b,v18.16b
	sub		x7, x7, #32			/* rewind x7 */
	b		1f
.Laes256_${label}_$i:
	aesd		v$i.16b,v19.16b
	aesimc		v$i.16b,v$i.16b
	ldp		q18,q19,[x7],32			/* rk[12],rk[13] */
	aesd		v$i.16b,v18.16b
	aesimc		v$i.16b,v$i.16b
	ld1		{v18.16b},[x7]			/* rk[14] */
	aesd		v$i.16b,v19.16b
	eor		v$i.16b,v$i.16b,v18.16b
	sub		x7, x7, #64			/* rewind x7 */
	b		1f
.Laes128_${label}_$i:
___
	if ($load_rk10 == 1) {
$code.=<<___;
	ld1	{v18.16b},[x7]
___
	}
$code.=<<___;
	aesd		v$i.16b,v17.16b
	eor		v$i.16b,v$i.16b,v18.16b		/* res */
1:
___
}

sub sha512_block() {
	my @H = map("v$_",(24..28));
	my @QH = map("q$_",(24..28));
	my ($FG, $DE) = map("v$_",(29..30));
	my ($QFG, $QDE) = map("q$_",(29..30));
	my $M9_10 = "v31";
	my @MSG = map("v$_", (0..7));
	my ($W0, $W1) = ("v8", "v9");
	my ($AB, $CD, $EF, $GH) = map("v$_",(20..23));
	my $need_revert = shift;

	if($need_revert == 1) {
$code.=<<___;
	rev64		@MSG[0].16b, @MSG[0].16b
	rev64		@MSG[1].16b, @MSG[1].16b
	rev64		@MSG[2].16b, @MSG[2].16b
	rev64		@MSG[3].16b, @MSG[3].16b
	rev64		@MSG[4].16b, @MSG[4].16b
	rev64		@MSG[5].16b, @MSG[5].16b
	rev64		@MSG[6].16b, @MSG[6].16b
	rev64		@MSG[7].16b, @MSG[7].16b
___
	}
$code.=<<___;
	/* load const k */
	ld1		{$W0.2d}, [x10], #16

	/* backup ABCDEFGH */
	mov		$AB.16b, @H[0].16b
	mov		$CD.16b, @H[1].16b
	mov		$EF.16b, @H[2].16b
	mov		$GH.16b, @H[3].16b
___
for($i = 0; $i < 32; $i++) {
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8

	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16)*/
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d

	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
}
for(;$i<40;$i++) {
$code.=<<___	if ($i<39);
	ld1		{$W1.2d},[x10],#16
___
$code.=<<___	if ($i==39);
	sub		x10, x10, #80*8	// rewind
___
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8

	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
}
$code.=<<___;
	add		@H[0].2d, @H[0].2d, $AB.2d
	add		@H[1].2d, @H[1].2d, $CD.2d
	add		@H[2].2d, @H[2].2d, $EF.2d
	add		@H[3].2d, @H[3].2d, $GH.2d
___
}

{
	my @H = map("v$_",(24..28));
	my @QH = map("q$_",(24..28));
	my ($FG, $DE) = map("v$_",(29..30));
	my ($QFG, $QDE) = map("q$_",(29..30));
	my $M9_10 = "v31";
	my @MSG = map("v$_", (0..7));
	my ($W0, $W1) = ("v14", "v15");
	my ($AB, $CD, $EF, $GH) = map("v$_",(20..23));

$code.=<<___;
/*
 * asm_aescbc_sha512_hmac(
 *	csrc,	x0	(cipher src address)
 *	cdst,	x1	(cipher dst address)
 *	clen	x2	(cipher length)
 *	dsrc,	x3	(digest src address)
 *	ddst,	x4	(digest dst address)
 *	dlen,	x5	(digest length)
 *	arg	x6	:
 *		arg->cipher.key			(round keys)
 *		arg->cipher.key_rounds		(key rounds)
 *		arg->cipher.iv			(initialization vector)
 *		arg->digest.hmac.i_key_pad	(partially hashed i_key_pad)
 *		arg->digest.hmac.o_key_pad	(partially hashed o_key_pad)
 *	)
 */

.global asm_aescbc_sha512_hmac
.type	asm_aescbc_sha512_hmac,%function

.rodata
.align 6
.LK512:
	.quad	0x428a2f98d728ae22,0x7137449123ef65cd
	.quad	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	.quad	0x3956c25bf348b538,0x59f111f1b605d019
	.quad	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	.quad	0xd807aa98a3030242,0x12835b0145706fbe
	.quad	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	.quad	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	.quad	0x9bdc06a725c71235,0xc19bf174cf692694
	.quad	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	.quad	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	.quad	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	.quad	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	.quad	0x983e5152ee66dfab,0xa831c66d2db43210
	.quad	0xb00327c898fb213f,0xbf597fc7beef0ee4
	.quad	0xc6e00bf33da88fc2,0xd5a79147930aa725
	.quad	0x06ca6351e003826f,0x142929670a0e6e70
	.quad	0x27b70a8546d22ffc,0x2e1b21385c26c926
	.quad	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	.quad	0x650a73548baf63de,0x766a0abb3c77b2a8
	.quad	0x81c2c92e47edaee6,0x92722c851482353b
	.quad	0xa2bfe8a14cf10364,0xa81a664bbc423001
	.quad	0xc24b8b70d0f89791,0xc76c51a30654be30
	.quad	0xd192e819d6ef5218,0xd69906245565a910
	.quad	0xf40e35855771202a,0x106aa07032bbd1b8
	.quad	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	.quad	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	.quad	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	.quad	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	.quad	0x748f82ee5defb2fc,0x78a5636f43172f60
	.quad	0x84c87814a1f0ab72,0x8cc702081a6439ec
	.quad	0x90befffa23631e28,0xa4506cebde82bde9
	.quad	0xbef9a3f7b2c67915,0xc67178f2e372532b
	.quad	0xca273eceea26619c,0xd186b8c721c0c207
	.quad	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	.quad	0x06f067aa72176fba,0x0a637dc5a2c898a6
	.quad	0x113f9804bef90dae,0x1b710b35131c471b
	.quad	0x28db77f523047d84,0x32caab7b40c72493
	.quad	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	.quad	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	.quad	0x5fcb6fab3ad6faec,0x6c44198c4a475817
	.quad	0	// terminator

.text
	.align	4
asm_aescbc_sha512_hmac:
	AARCH64_VALID_CALL_TARGET
	/* save callee save register */
	stp		d8, d9, [sp,#-64]!
	stp		d10, d11, [sp,#16]
	stp		d12, d13, [sp,#32]
	stp		d14, d15, [sp,#48]

	/* load ABCDEFGH */
	ldr		x7, [x6, #HMAC_IKEYPAD]
	ld1		{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ldr		x7, [x6, #CIPHER_KEY]
	ldr		x8, [x6, #CIPHER_IV]
	ldr		x9, [x6, #CIPHER_KEY_ROUNDS]
	mov		x12, x7				/* backup x7 */

	adrp		x10, .LK512
	add		x10, x10, :lo12:.LK512

	lsr		x11, x2, #4			/* aes_block = len/16 */
	cbz		x11, .Lret			/* return if aes_block = 0 */

	cmp		x11, #16
	b.lt		.Lenc_short_case

	ld1		{v0.16b}, [x0], #16		/* load plaintext */
	ld1		{v1.16b}, [x8]			/* load iv */

	eor		v0.16b, v0.16b, v1.16b	/* iv xor plaintext */

	ldp		q8, q9, [x7], #32		/* rk0, rk1 */
	/* block 0 */
	aese		v0.16b, v8.16b
	aesmc		v0.16b, v0.16b
	ldp		q10, q11, [x7], #32		/* rk2, rk3 */
	aese		v0.16b, v9.16b
	aesmc		v0.16b, v0.16b
	aese		v0.16b, v10.16b
	aesmc		v0.16b, v0.16b
	ldp		q12, q13, [x7], #32		/* rk4, rk5 */
	aese		v0.16b, v11.16b
	aesmc		v0.16b, v0.16b
	aese		v0.16b, v12.16b
	aesmc		v0.16b, v0.16b
	ldp		q14, q15, [x7], #32		/* rk6, rk7 */
	aese		v0.16b, v13.16b
	aesmc		v0.16b, v0.16b
	aese		v0.16b, v14.16b
	aesmc		v0.16b, v0.16b
	ldp		q16, q17, [x7], #32		/* rk8, rk9 */
	aese		v0.16b, v15.16b
	aesmc		v0.16b, v0.16b
	aese		v0.16b, v16.16b
	aesmc		v0.16b, v0.16b
	ld1		{v18.16b}, [x7]			/* rk10 */
___
&aes_block_last_rounds(1, "enc_prelog", 0, 0);
$code.=<<___;
	str		q0, [x1], #16			/* store cipher result */
	ld1		{v1.16b}, [x0], #16		/* load next block */
	eor		v1.16b, v1.16b, v0.16b		/* output xor block */
___
# process aes blocks from 1 to 7
for($i = 1; $i < 8; $i = $i + 1) {
	&aes_block_9_rounds($i);
	&aes_block_last_rounds(0, "enc_prelog", $i, 0);
	if($i != 7) {
		$next = $i + 1;
$code.=<<___;
	/* load next block */
	ld1		{v$next.16b}, [x0], #16
	/* output xor block */
	eor		v$next.16b, v$next.16b, v$i.16b
___
	}
$code.=<<___;
	str		q$i, [x1], #16				/* store cipher result */
___
}
$code.=<<___;
	sub		x11, x11, #8

.Lenc_main_loop:
	mov		x7, x12
	mov		x14, x1
	/* aes block 0 */
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v12.16b}, [x0], #16
	eor		v12.16b, v12.16b, v7.16b

	/* reverse message */
	rev64		@MSG[0].16b, @MSG[0].16b
	rev64		@MSG[1].16b, @MSG[1].16b
	rev64		@MSG[2].16b, @MSG[2].16b
	rev64		@MSG[3].16b, @MSG[3].16b
	rev64		@MSG[4].16b, @MSG[4].16b
	rev64		@MSG[5].16b, @MSG[5].16b
	rev64		@MSG[6].16b, @MSG[6].16b
	rev64		@MSG[7].16b, @MSG[7].16b
	ld1		{$W0.2d}, [x10], #16			/* load const k*/

	/* backup ABCDEFGH */
	mov		$AB.16b, @H[0].16b
	mov		$CD.16b, @H[1].16b
	mov		$EF.16b, @H[2].16b
	mov		$GH.16b, @H[3].16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	cmp		x9, #12
	b.lt		.Lenc_main_loop_aes128_0
.Lenc_main_loop_aes192_0:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_0
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_0:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_0:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 1 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8

	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	cmp		x9, #12
	b.lt		.Lenc_main_loop_aes128_1
.Lenc_main_loop_aes192_1:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_1
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_1:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_1:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 2 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	cmp		x9, #12
	b.lt		.Lenc_main_loop_aes128_2
.Lenc_main_loop_aes192_2:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_2
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_2:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_2:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 3 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	cmp		x9, #12
	b.lt		.Lenc_main_loop_aes128_3
.Lenc_main_loop_aes192_3:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_3
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_3:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_3:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 4 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	cmp		x9, #12
	b.lt		.Lenc_main_loop_aes128_4
.Lenc_main_loop_aes192_4:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_4
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_4:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_4:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 5 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	cmp		x9, #12
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	b.lt		.Lenc_main_loop_aes128_5
.Lenc_main_loop_aes192_5:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_5
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_5:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_5:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 6 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ld1		{$W1.2d}, [x10], #16			/* load const k*/
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	ext		$M9_10.16b, @MSG[4].16b, @MSG[5].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	/* Wt_PART1 = SSIG0(W(t-15)) + W(t-16) */
	sha512su0	@MSG[0].2d, @MSG[1].2d
	/* Wt = SSIG1(W(t-2)) + W(t-7) + Wt_PART1 */
	sha512su1	@MSG[0].2d, @MSG[7].2d, $M9_10.2d
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	cmp		x9, #12
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	b.lt		.Lenc_main_loop_aes128_6
.Lenc_main_loop_aes192_6:
	ldp		q10, q11, [x7], #32			/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_6
	ld1		{v8.16b},[x7]				/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_6:
	ldp		q8, q9, [x7], #32			/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]				/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_6:
	ld1		{v10.16b},[x7]				/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	st1		{v12.16b}, [x1], #16
	/* aes block 7 */
	mov		x7, x12
	ldp		q8, q9, [x7], #32			/* rk0, rk1 */
	ldp		q10, q11, [x7], #32			/* rk2, rk3 */

	ld1		{v13.16b}, [x0], #16
	eor		v12.16b, v12.16b, v13.16b

	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk4, rk5 */
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ldp		q10, q11, [x7], #32			/* rk6, rk7 */
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	ld1		{$W1.2d},[x10],#16
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	ldp		q8, q9, [x7], #32			/* rk8, rk9 */
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	sub		x10, x10, #80*8	// rewind
	add		$W0.2d, $W0.2d, $MSG[0].2d		/* Kt + Wt */
	ext		$W0.16b, $W0.16b, $W0.16b, #8
	ext		$FG.16b, @H[2].16b, @H[3].16b, #8
	ext		$DE.16b, @H[1].16b, @H[2].16b, #8
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	/* T1 = h + Kt + Wt*/
	add		@H[3].2d, @H[3].2d, $W0.2d
	/* T1 = T1 + BSIG1(e) + CH(e,f,g) */
	sha512h		@QH[3], $QFG, $DE.2d
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	cmp		x9, #12
	add		@H[4].2d, @H[1].2d, @H[3].2d		/* d + T1 */
	/* T2 = BSIG0(a) + MAJ(a,b,c), T1 + T2 */
	sha512h2	@QH[3], @QH[1], @H[0].2d
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
	# h=g, g=f,f=e,e=d+T1,d=c,c=b,b=a,a=T1+T2
	@H = (@H[3],@H[0],@H[4],@H[2],@H[1]);
	@QH = (@QH[3],@QH[0],@QH[4],@QH[2],@QH[1]);
$code.=<<___;
	b.lt		.Lenc_main_loop_aes128_7
.Lenc_main_loop_aes192_7:
	ldp		q10, q11, [x7], #32		/* rk10, rk11 */
	aese		v12.16b, v9.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v10.16b
	aesmc		v12.16b, v12.16b
	b.gt		.Lenc_main_loop_aes256_7
	ld1		{v8.16b},[x7]			/* rk12 */
	aese		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	b		1f
.Lenc_main_loop_aes256_7:
	ldp		q8, q9, [x7], #32		/* rk12, rk13 */
	aese		v12.16b, v11.16b
	aesmc		v12.16b, v12.16b
	ld1		{v10.16b},[x7]			/* rk14 */
	aese		v12.16b, v8.16b
	aesmc		v12.16b, v12.16b
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	b		1f
.Lenc_main_loop_aes128_7:
	ld1		{v10.16b},[x7]			/* rk10 */
	aese		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
1:
	add		@H[0].2d, @H[0].2d, $AB.2d
	add		@H[1].2d, @H[1].2d, $CD.2d
	add		@H[2].2d, @H[2].2d, $EF.2d
	add		@H[3].2d, @H[3].2d, $GH.2d

	st1		{v12.16b}, [x1], #16

	ld1		{v0.16b, v1.16b, v2.16b, v3.16b}, [x14], #64
	ld1		{v4.16b, v5.16b, v6.16b, v7.16b}, [x14]

	sub		x11, x11, #8
	cmp		x11, #8
	b.ge	.Lenc_main_loop

	/* epilog - process sha block */
___
	&sha512_block(1);
$code.=<<___;
	mov		x7, x12
	ld1		{v0.16b}, [x0], #16		/* load plaintext */
	ldr		q1, [x14, #48]			/* load the last output of aes block */
	eor		v0.16b, v0.16b, v1.16b

	ldp		q8, q9, [x7], #32		/* rk0, rk1 */
	ldp		q10, q11, [x7], #32		/* rk2, rk3 */
	ldp		q12, q13, [x7], #32		/* rk4, rk5 */
	ldp		q14, q15, [x7], #32		/* rk6, rk7 */
	ldp		q16, q17, [x7], #32		/* rk8, rk9 */
	ld1		{v18.16b}, [x7]			/* rk10 */

	mov		w12, #0x80				/* sha padding 0b10000000 */
	b		.Lenc_less_than_8_block

	/* aes_block < 16 */
.Lenc_short_case:
	ld1		{v0.16b}, [x0], #16		/* load plaintext */
	ld1		{v1.16b}, [x8]			/* load iv */
	ldp		q8, q9, [x7], #32		/* rk0, rk1 */
	ldp		q10, q11, [x7], #32		/* rk2, rk3 */
	ldp		q12, q13, [x7], #32		/* rk4, rk5 */
	ldp		q14, q15, [x7], #32		/* rk6, rk7 */
	ldp		q16, q17, [x7], #32		/* rk8, rk9 */
	ld1		{v18.16b}, [x7]			/* rk10 */
	mov		w12, #0x80				/* sha padding 0b10000000 */

	eor		v0.16b, v0.16b, v1.16b	/* iv xor plaintext */

	cmp		x11, #8
	b.lt		.Lenc_less_than_8_block
___
# process 8 aes blocks
for($i = 0; $i < 8; $i = $i + 1) {
	&aes_block_9_rounds($i);
	# only tell 128/192/256 at the first time
	&aes_block_last_rounds(($i == 0)?1:0, "enc_short", $i, 0);
	if($i != 7) {
		$next = $i + 1;
$code.=<<___;
	/* load next block */
	ld1		{v$next.16b}, [x0], #16
	/* output xor block */
	eor		v$next.16b, v$next.16b, v$i.16b
___
	}
}
$code.=<<___;
	/* store 8 blocks of ciphertext */
	stp		q0, q1, [x1], #32
	stp		q2, q3, [x1], #32
	stp		q4, q5, [x1], #32
	stp		q6, q7, [x1], #32

	sub		x11, x11, #8
___
	# now we have a whole sha512 block
	&sha512_block(1);
$code.=<<___;
	ldr		x7, [x6, #CIPHER_KEY]
	ldp		q8, q9, [x7]			/* restore clobbered rk0, rk1 */
	add		x7, x7, #160			/* x7 point to rk10 */
	cbz		x11, .Lenc_short_no_more_aes_block
	ld1		{v0.16b}, [x0], #16		/* load plaintext */
	ldr		q1, [x1, -16]
	eor		v0.16b, v0.16b, v1.16b
.Lenc_less_than_8_block:
	cbz		x11, .Lenc_short_no_more_aes_block
___
# process remained aes blocks (<= 7)
for($i = 0; $i < 7; $i = $i + 1) {
	&aes_block_9_rounds($i);
	&aes_block_last_rounds(($i == 0)?1:0, "enc_short_partial", $i, 0);
$code.=<<___;
	str		q$i, [x1], #16
	sub		x11, x11, #1
	cbz		x11, .Lenc_short_post_Q$i
___
	if($i != 6) {
		$next = $i + 1;
$code.=<<___;
	/* load next block*/
	ld1		{v$next.16b}, [x0], #16
	/* output xor block */
	eor		v$next.16b, v$next.16b, v$i.16b
___
	}
}
$code.=<<___;
.Lenc_short_no_more_aes_block:
	eor		v0.16b, v0.16b, v0.16b
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v0.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q0:
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v1.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q1:
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v2.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q2:
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v3.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q3:
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v4.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q4:
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v5.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q5:
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v6.b[0], w12
	b		.Lenc_short_post_sha
.Lenc_short_post_Q6:
	eor		v7.16b, v7.16b, v7.16b
	mov		v7.b[0], w12
	/* we have one padded sha512 block now, process it and
	   then employ another one to host sha length */
___
&sha512_block(1);
$code.=<<___;
	eor		v0.16b, v0.16b, v0.16b
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
.Lenc_short_post_sha:
	/* we have last padded sha512 block now */
	eor		x13, x13, x13			/* length_lo */
	eor		x14, x14, x14			/* length_hi */

	adds		x13, x13, x2, lsl #3		/* add len in bits */
	lsr		x15, x2, #61
	adc		x14, x14, x15

	adds		x13, x13, #1024			/* add i_key_pad 1024 bits */
	adc		x14, x14, xzr

	mov		v7.d[0], x14
	mov		v7.d[1], x13
	rev64		v7.16b, v7.16b
___
&sha512_block(1);
$code.=<<___;
	/* Final HMAC - opad part */
	mov		v0.16b, v24.16b
	mov		v1.16b, v25.16b
	mov		v2.16b, v26.16b
	mov		v3.16b, v27.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b

	mov		v4.b[7], w12			/* padding 1 */
	mov		x13, #1024+512			/* length in bits */
	mov		v7.d[1], x13

	/* load ABCDEFGH for opad */
	ldr		x7, [x6, #HMAC_OKEYPAD]
	ld1		{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]
___
&sha512_block(0);
$code.=<<___;
.Lret:
	mov		x0, xzr				/* return 0 */

	rev64		v24.16b, v24.16b
	rev64		v25.16b, v25.16b
	rev64		v26.16b, v26.16b
	rev64		v27.16b, v27.16b

	/* store hash result */
	st1		{v24.2d,v25.2d,v26.2d,v27.2d},[x4]

	/* restore callee save register */
	ldp		d10, d11, [sp,#16]
	ldp		d12, d13, [sp,#32]
	ldp		d14, d15, [sp,#48]
	ldp		d8, d9, [sp], #64
	ret
.size asm_aescbc_sha512_hmac, .-asm_aescbc_sha512_hmac
___
}

{
	my @H = map("v$_",(24..28));
	my @QH = map("q$_",(24..28));
	my ($FG, $DE) = map("v$_",(29..30));
	my ($QFG, $QDE) = map("q$_",(29..30));
	my $M9_10 = "v31";
	my @MSG = map("v$_", (0..7));
	my ($W0, $W1) = ("v14", "v15");
	my ($AB, $CD, $EF, $GH) = map("v$_",(20..23));

$code.=<<___;
/*
 * asm_sha512_hmac_aescbc_dec(
 *	csrc,	x0	(cipher src address)
 *	cdst,	x1	(cipher dst address)
 *	clen	x2	(cipher length)
 *	dsrc,	x3	(digest src address)
 *	ddst,	x4	(digest dst address)
 *	dlen,	x5	(digest length)
 *	arg	x6	:
 *		arg->cipher.key			(round keys)
 *		arg->cipher.key_rounds		(key rounds)
 *		arg->cipher.iv			(initialization vector)
 *		arg->digest.hmac.i_key_pad	(partially hashed i_key_pad)
 *		arg->digest.hmac.o_key_pad	(partially hashed o_key_pad)
 *	)
 */

.global asm_sha512_hmac_aescbc_dec
.type	asm_sha512_hmac_aescbc_dec,%function

.align	4
asm_sha512_hmac_aescbc_dec:
	AARCH64_VALID_CALL_TARGET
	/* save callee save register */
	stp		d8, d9, [sp,#-64]!
	stp		d10, d11, [sp,#16]
	stp		d12, d13, [sp,#32]
	stp		d14, d15, [sp,#48]

	/* load ABCDEFGH */
	ldr		x7, [x6, #HMAC_IKEYPAD]
	ld1		{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ldr		x7, [x6, #CIPHER_KEY]
	ldr		x8, [x6, #CIPHER_IV]
	ldr		x9, [x6, #CIPHER_KEY_ROUNDS]
	mov		x12, x7			/* backup x7 */

	adrp		x10, .LK512
	add		x10, x10, :lo12:.LK512

	lsr		x11, x2, #4		/* aes_block = len/16 */
	cbz		x11, .Ldec_ret		/* return if aes_block = 0 */

	ld1		{v20.16b}, [x8]		/* load iv */
	cmp		x11, #8
	b.lt		.Ldec_short_case
.Ldec_main_loop:
	ldp		q12, q13, [x0], #32
	ldp		q14, q15, [x0], #32
	ldp		q16, q17, [x0], #32
	ldp		q18, q19, [x0], #32

	ldp		q8, q9, [x7], #32	/* rk0, rk1 */
	ldp		q10, q11, [x7], #32	/* rk2, rk3 */

	mov		v0.16b, v12.16b
	mov		v1.16b, v13.16b
	mov		v2.16b, v14.16b
	mov		v3.16b, v15.16b
	mov		v4.16b, v16.16b
	mov		v5.16b, v17.16b
	mov		v6.16b, v18.16b
	mov		v7.16b, v19.16b

	/* 1 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v8.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v8.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v8.16b
	aesimc		v19.16b, v19.16b

	/* 2 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v9.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v9.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v9.16b
	aesimc		v19.16b, v19.16b

	ldp		q8, q9, [x7], #32	/* rk4, rk5 */

	/* 3 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v10.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v10.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v10.16b
	aesimc		v19.16b, v19.16b

	/* 4 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v11.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v11.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v11.16b
	aesimc		v19.16b, v19.16b

	ldp		q10, q11, [x7], #32	/* rk6, rk7 */

	/* 5 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v8.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v8.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v8.16b
	aesimc		v19.16b, v19.16b

	/* 6 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v9.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v9.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v9.16b
	aesimc		v19.16b, v19.16b

	ldp		q8, q9, [x7], #32	/* rk8, rk9 */

	/* 7 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v10.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v10.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v10.16b
	aesimc		v19.16b, v19.16b

	/* 8 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v11.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v11.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v11.16b
	aesimc		v19.16b, v19.16b

	/* 9 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v8.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v8.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v8.16b
	aesimc		v19.16b, v19.16b

	cmp		x9, #12			/* tell 128,192,256 apart */

	b.lt		.Laes128_dec_main
.Laes192_dec_main:
	ldp		q10,q11,[x7],32		/* rk10,rk11 */
	/* 10 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v9.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v9.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v9.16b
	aesimc		v19.16b, v19.16b

	/* 11 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v10.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v10.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v10.16b
	aesimc		v19.16b, v19.16b
	b.gt		.Laes256_dec_main

	ld1		{v8.16b},[x7]		/* rk12 */

	/*12 round */
	aesd		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	aesd		v13.16b, v11.16b
	eor		v13.16b, v13.16b, v8.16b
	aesd		v14.16b, v11.16b
	eor		v14.16b, v14.16b, v8.16b
	aesd		v15.16b, v11.16b
	eor		v15.16b, v15.16b, v8.16b
	aesd		v16.16b, v11.16b
	eor		v16.16b, v16.16b, v8.16b
	aesd		v17.16b, v11.16b
	eor		v17.16b, v17.16b, v8.16b
	aesd		v18.16b, v11.16b
	eor		v18.16b, v18.16b, v8.16b
	aesd		v19.16b, v11.16b
	eor		v19.16b, v19.16b, v8.16b

	sub		x7, x7, #192		/* rewind x7 */
	b		1f
.Laes256_dec_main:
	ldp		q8,q9,[x7],32		/* rk12,rk13 */
	/* 12 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v11.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v11.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v11.16b
	aesimc		v19.16b, v19.16b

	/* 13 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v17.16b, v8.16b
	aesimc		v17.16b, v17.16b
	aesd		v18.16b, v8.16b
	aesimc		v18.16b, v18.16b
	aesd		v19.16b, v8.16b
	aesimc		v19.16b, v19.16b

	ld1		{v10.16b},[x7]			/* rk14 */

	/* 14 round */
	aesd		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	aesd		v13.16b, v9.16b
	eor		v13.16b, v13.16b, v10.16b
	aesd		v14.16b, v9.16b
	eor		v14.16b, v14.16b, v10.16b
	aesd		v15.16b, v9.16b
	eor		v15.16b, v15.16b, v10.16b
	aesd		v16.16b, v9.16b
	eor		v16.16b, v16.16b, v10.16b
	aesd		v17.16b, v9.16b
	eor		v17.16b, v17.16b, v10.16b
	aesd		v18.16b, v9.16b
	eor		v18.16b, v18.16b, v10.16b
	aesd		v19.16b, v9.16b
	eor		v19.16b, v19.16b, v10.16b

	sub		x7, x7, #224
	b		1f
.Laes128_dec_main:
	ld1		{v10.16b},[x7]			/* rk10 */
	aesd		v12.16b,v9.16b
	eor		v12.16b, v12.16b, v10.16b
	aesd		v13.16b,v9.16b
	eor		v13.16b, v13.16b, v10.16b
	aesd		v14.16b,v9.16b
	eor		v14.16b, v14.16b, v10.16b
	aesd		v15.16b,v9.16b
	eor		v15.16b, v15.16b, v10.16b
	aesd		v16.16b,v9.16b
	eor		v16.16b, v16.16b, v10.16b
	aesd		v17.16b,v9.16b
	eor		v17.16b, v17.16b, v10.16b
	aesd		v18.16b,v9.16b
	eor		v18.16b, v18.16b, v10.16b
	aesd		v19.16b,v9.16b
	eor		v19.16b, v19.16b, v10.16b
	sub		x7, x7, #160

1:
	eor		v12.16b, v12.16b, v20.16b
	eor		v13.16b, v13.16b, v0.16b
	eor		v14.16b, v14.16b, v1.16b
	eor		v15.16b, v15.16b, v2.16b
	eor		v16.16b, v16.16b, v3.16b
	eor		v17.16b, v17.16b, v4.16b
	eor		v18.16b, v18.16b, v5.16b
	eor		v19.16b, v19.16b, v6.16b

	stp		q12,q13, [x1], #32
	ldr		q12, [x0, #-16]		/* load last cipher */
	stp		q14,q15, [x1], #32
	stp		q16,q17, [x1], #32
	stp		q18,q19, [x1], #32
___
	&sha512_block(1);
$code.=<<___;
	mov		v20.16b, v12.16b	/* load last cipher */
	sub		x11, x11, #8
	cmp		x11, #8
	b.ge		.Ldec_main_loop

	/* aes_block < 8 */
.Ldec_short_case:
	mov		w12, #0x80		/* sha padding 0b10000000 */
	cbnz		x11, 1f
	eor		v0.16b, v0.16b, v0.16b
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v0.b[0], w12
	b		.Ldec_short_post_sha
1:
	cmp		x11, #4
	b.lt		.Ldec_less_than_4_block

	ldp		q8, q9, [x7], #32	/* rk0, rk1 */
	ldp		q10, q11, [x7], #32	/* rk2, rk3 */

	ldp		q12, q13, [x0], #32
	ldp		q14, q15, [x0], #32

	mov		v0.16b, v12.16b
	mov		v1.16b, v13.16b
	mov		v2.16b, v14.16b
	mov		v3.16b, v15.16b

	/* 1 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b

	/* 2 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b

	ldp		q8, q9, [x7], #32	/* rk4, rk5 */

	/* 3 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b

	/* 4 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b

	ldp		q10, q11, [x7], #32	/* rk6, rk7 */

	/* 5 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b

	/* 6 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b

	ldp		q8, q9, [x7], #32	/* rk8, rk9 */

	/* 7 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b

	/* 8 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b

	/* 9 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b

	cmp		x9, #12			/* tell 128,192,256 apart */

	b.lt		.Laes128_dec_short
.Laes192_dec_short:
	ldp		q10,q11,[x7],32		/* rk10,rk11 */

	/* 10 round */
	aesd		v12.16b, v9.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v9.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v9.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v9.16b
	aesimc		v15.16b, v15.16b

	/* 11 round */
	aesd		v12.16b, v10.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v10.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v10.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v10.16b
	aesimc		v15.16b, v15.16b
	b.gt		.Laes256_dec_short

	ld1		{v8.16b},[x7]			/* rk12 */

	/*12 round */
	aesd		v12.16b, v11.16b
	eor		v12.16b, v12.16b, v8.16b
	aesd		v13.16b, v11.16b
	eor		v13.16b, v13.16b, v8.16b
	aesd		v14.16b, v11.16b
	eor		v14.16b, v14.16b, v8.16b
	aesd		v15.16b, v11.16b
	eor		v15.16b, v15.16b, v8.16b

	sub		x7, x7, #192			/* rewind x7 */
	b		1f
.Laes256_dec_short:
	ldp		q8,q9,[x7],32			/* rk12,rk13 */
	/* 12 round */
	aesd		v12.16b, v11.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v11.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v11.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v11.16b
	aesimc		v15.16b, v15.16b

	/* 13 round */
	aesd		v12.16b, v8.16b
	aesimc		v12.16b, v12.16b
	aesd		v13.16b, v8.16b
	aesimc		v13.16b, v13.16b
	aesd		v14.16b, v8.16b
	aesimc		v14.16b, v14.16b
	aesd		v15.16b, v8.16b
	aesimc		v15.16b, v15.16b

	ld1		{v10.16b},[x7]			/* rk14 */

	/* 14 round */
	aesd		v12.16b, v9.16b
	eor		v12.16b, v12.16b, v10.16b
	aesd		v13.16b, v9.16b
	eor		v13.16b, v13.16b, v10.16b
	aesd		v14.16b, v9.16b
	eor		v14.16b, v14.16b, v10.16b
	aesd		v15.16b, v9.16b
	eor		v15.16b, v15.16b, v10.16b

	sub		x7, x7, #224
	b		1f
.Laes128_dec_short:
	ld1		{v10.16b},[x7]			/* rk10 */
	aesd		v12.16b,v9.16b
	eor		v12.16b, v12.16b, v10.16b
	aesd		v13.16b,v9.16b
	eor		v13.16b, v13.16b, v10.16b
	aesd		v14.16b,v9.16b
	eor		v14.16b, v14.16b, v10.16b
	aesd		v15.16b,v9.16b
	eor		v15.16b, v15.16b, v10.16b
	sub		x7, x7, #160
1:
	eor		v12.16b, v12.16b, v20.16b
	eor		v13.16b, v13.16b, v0.16b
	eor		v14.16b, v14.16b, v1.16b
	eor		v15.16b, v15.16b, v2.16b
	ldr		q20, [x0, #-16]

	sub		x11, x11, #4

	stp		q12,q13, [x1], #32
	stp		q14,q15, [x1], #32
	cbz		x11, .Ldec_short_post_Q3
___
for($i = 0; $i < 3; $i = $i + 1) {
	$block = $i + 4;
$code.=<<___;
	ld1		{v16.16b}, [x0], #16
	mov		v$block.16b, v16.16b

	ldp		q8, q9, [x7], #32		/* rk0, rk1 */
	ldp		q10, q11, [x7], #32		/* rk2, rk3 */

	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	ldp		q8, q9, [x7], #32		/* rk4, rk5 */
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	ldp		q10, q11, [x7], #32		/* rk6, rk7 */
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	ldp		q8, q9, [x7], #32		/* rk8, rk9 */
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	cmp		x9, #12			/* tell 128,192,256 apart */
	b.lt		.Laes128_dec_short_$block
.Laes192_dec_short_$block:
	ldp		q10,q11,[x7],32			/* rk10,rk11 */
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	b.gt		.Laes256_dec_short_$block
	ld1		{v8.16b},[x7]			/* rk12 */
	aesd		v16.16b, v11.16b
	eor		v16.16b, v16.16b, v8.16b
	sub		x7, x7, #192			/* rewind x7 */
	b		1f
.Laes256_dec_short_$block:
	ldp		q8,q9,[x7],32			/* rk12,rk13 */
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	ld1		{v10.16b},[x7]			/* rk14 */
	aesd		v16.16b, v9.16b
	eor		v16.16b, v16.16b, v10.16b
	sub		x7, x7, #224
	b		1f
.Laes128_dec_short_$block:
	ld1		{v10.16b},[x7]			/* rk10 */
	aesd		v16.16b,v9.16b
	eor		v16.16b, v16.16b, v10.16b
	sub		x7, x7, #160
1:
	sub		x11, x11, 1
	eor		v16.16b, v16.16b, v20.16b
	ldr		q20, [x0, #-16]
	st1		{v16.16b}, [x1], #16
	cbz		x11, .Ldec_short_post_Q$block
___
}
$code.=<<___;
.Ldec_short_post_Q3:
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v4.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_Q4:
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v5.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_Q5:
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v6.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_Q6:
	eor		v7.16b, v7.16b, v7.16b
	mov		v7.b[0], w12
	/* we have one padded sha512 block now, process it and
	   then employ another one to host sha length */
___
&sha512_block(1);
$code.=<<___;
	eor		v0.16b, v0.16b, v0.16b
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	b		.Ldec_short_post_sha

.Ldec_less_than_4_block:
___
for($i = 0; $i < 3; $i = $i + 1) {
$code.=<<___;
	ld1		{v16.16b}, [x0], #16
	mov		v$i.16b, v16.16b

	ldp		q8, q9, [x7], #32		/* rk0, rk1 */
	ldp		q10, q11, [x7], #32		/* rk2, rk3 */

	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	ldp		q8, q9, [x7], #32		/* rk4, rk5 */
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	ldp		q10, q11, [x7], #32		/* rk6, rk7 */
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	ldp		q8, q9, [x7], #32		/* rk8, rk9 */
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	cmp		x9, #12				/* tell 128,192,256 apart */
	b.lt		.Laes128_dec_short_less_than_4_$i
.Laes192_dec_short_less_than_4_$i:
	ldp		q10,q11,[x7],32			/* rk10,rk11 */
	aesd		v16.16b, v9.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v10.16b
	aesimc		v16.16b, v16.16b
	b.gt		.Laes256_dec_short_less_than_4_$i
	ld1		{v8.16b},[x7]			/* rk12 */
	aesd		v16.16b, v11.16b
	eor		v16.16b, v16.16b, v8.16b
	sub		x7, x7, #192			/* rewind x7 */
	b		1f
.Laes256_dec_short_less_than_4_$i:
	ldp		q8,q9,[x7],32			/* rk12,rk13 */
	aesd		v16.16b, v11.16b
	aesimc		v16.16b, v16.16b
	aesd		v16.16b, v8.16b
	aesimc		v16.16b, v16.16b
	ld1		{v10.16b},[x7]			/* rk14 */
	aesd		v16.16b, v9.16b
	eor		v16.16b, v16.16b, v10.16b
	sub		x7, x7, #224
	b		1f
.Laes128_dec_short_less_than_4_$i:
	ld1		{v10.16b},[x7]			/* rk10 */
	aesd		v16.16b,v9.16b
	eor		v16.16b, v16.16b, v10.16b
	sub		x7, x7, #160
1:
	sub		x11, x11, 1
	eor		v16.16b, v16.16b, v20.16b
	ldr		q20, [x0, #-16]
	st1		{v16.16b}, [x1], #16
	cbz		x11, .Ldec_short_post_Q$i
___
}
$code.=<<___;
.Ldec_short_post_Q0:
	eor		v1.16b, v1.16b, v1.16b
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v1.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_Q1:
	eor		v2.16b, v2.16b, v2.16b
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v2.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_Q2:
	eor		v3.16b, v3.16b, v3.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b
	mov		v3.b[0], w12
	b		.Ldec_short_post_sha
.Ldec_short_post_sha:
	/* we have last padded sha512 block now */
	eor		x13, x13, x13		/* length_lo */
	eor		x14, x14, x14		/* length_hi */

	adds		x13, x13, x2, lsl #3	/* add len in bits */
	lsr		x15, x2, #61
	adc		x14, x14, x15

	adds		x13, x13, #1024		/* add i_key_pad 1024 bits */
	adc		x14, x14, xzr

	mov		v7.d[0], x14
	mov		v7.d[1], x13
	rev64		v7.16b, v7.16b
___
&sha512_block(1);
$code.=<<___;
	/* Final HMAC - opad part */
	mov		v0.16b, v24.16b
	mov		v1.16b, v25.16b
	mov		v2.16b, v26.16b
	mov		v3.16b, v27.16b
	eor		v4.16b, v4.16b, v4.16b
	eor		v5.16b, v5.16b, v5.16b
	eor		v6.16b, v6.16b, v6.16b
	eor		v7.16b, v7.16b, v7.16b

	mov		v4.b[7], w12		/* padding 1 */
	mov		x13, #1024+512		/* length in bits */
	mov		v7.d[1], x13

	/* load ABCDEFGH for opad */
	ldr		x7, [x6, #HMAC_OKEYPAD]
	ld1		{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]
___
&sha512_block(0);
$code.=<<___;
.Ldec_ret:
	mov		x0, xzr			/* return 0 */

	rev64		v24.16b, v24.16b
	rev64		v25.16b, v25.16b
	rev64		v26.16b, v26.16b
	rev64		v27.16b, v27.16b

	/* store hash result */
	st1		{v24.2d,v25.2d,v26.2d,v27.2d},[x4]

	/* restore callee save register */
	ldp		d10, d11, [sp,#16]
	ldp		d12, d13, [sp,#32]
	ldp		d14, d15, [sp,#48]
	ldp		d8, d9, [sp], #64
	ret
.size asm_sha512_hmac_aescbc_dec, .-asm_sha512_hmac_aescbc_dec
___
}
#########################################
{	my  %opcode = (
	"sha512h"	=> 0xce608000,	"sha512h2"	=> 0xce608400,
	"sha512su0"	=> 0xcec08000,	"sha512su1"	=> 0xce608800	);

	sub unsha512 {
	my ($mnemonic,$arg)=@_;

	$arg =~ m/[qv]([0-9]+)[^,]*,\s*[qv]([0-9]+)[^,]*(?:,\s*[qv]([0-9]+))?/o
	&&
	sprintf ".inst\t0x%08x\t//%s %s",
			$opcode{$mnemonic}|$1|($2<<5)|($3<<16),
			$mnemonic,$arg;
	}
}

open SELF,$0;
while(<SELF>) {
	next if (/^#!/);
	last if (!s/^#/\/\// and !/^$/);
	print;
}
close SELF;

foreach(split("\n",$code)) {
	s/\`([^\`]*)\`/eval($1)/ge;
	s/\b(sha512\w+)\s+([qv].*)/unsha512($1,$2)/ge;
	print $_,"\n";
}

close STDOUT or die "error closing STDOUT: $!";