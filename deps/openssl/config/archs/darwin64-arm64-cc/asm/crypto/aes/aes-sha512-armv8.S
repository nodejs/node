
// Copyright 2023-2026 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arch/arm_arch.h"


#define CIPHER_KEY	0
#define CIPHER_KEY_ROUNDS	8
#define CIPHER_IV	16
#define HMAC_IKEYPAD	24
#define HMAC_OKEYPAD	32

.text

	/*
	*	asm_aescbc_sha512_hmac(
	*	csrc,	x0	(cipher src address)
	*	cdst,	x1	(cipher dst address)
	*	clen	x2	(cipher length)
	*	dsrc,	x3	(digest src address)
	*	ddst,	x4	(digest dst address)
	*	dlen,	x5	(digest length)
	*	arg	x6	:
	*	arg->cipher.key			(round keys)
	*	arg->cipher.key_rounds		(key rounds)
	*	arg->cipher.iv			(initialization vector)
	*	arg->digest.hmac.i_key_pad	(partially hashed i_key_pad)
	*	arg->digest.hmac.o_key_pad	(partially hashed o_key_pad)
	*	)
	*/

.globl	_asm_aescbc_sha512_hmac


.section	__TEXT,__const
.align	6
LK512:
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
_asm_aescbc_sha512_hmac:
	AARCH64_VALID_CALL_TARGET

	stp	d8, d9, [sp,#-64]!
	stp	d10, d11, [sp,#16]
	stp	d12, d13, [sp,#32]
	stp	d14, d15, [sp,#48]


	ldr	x7, [x6, #HMAC_IKEYPAD]
	ld1	{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ldr	x7, [x6, #CIPHER_KEY]
	ldr	x8, [x6, #CIPHER_IV]
	ldr	x9, [x6, #CIPHER_KEY_ROUNDS]
	mov	x12, x7

	adrp	x10, LK512@PAGE
	add	x10, x10, LK512@PAGEOFF

	lsr	x11, x2, #4
	cbz	x11, Lret

	cmp	x11, #16
	b.lt	Lenc_short_case

	ld1	{v0.16b}, [x0], #16
	ld1	{v1.16b}, [x8]

	eor	v0.16b, v0.16b, v1.16b

	ldp	q8, q9, [x7], #32

	aese	v0.16b, v8.16b
	aesmc	v0.16b, v0.16b
	ldp	q10, q11, [x7], #32
	aese	v0.16b, v9.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v10.16b
	aesmc	v0.16b, v0.16b
	ldp	q12, q13, [x7], #32
	aese	v0.16b, v11.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v12.16b
	aesmc	v0.16b, v0.16b
	ldp	q14, q15, [x7], #32
	aese	v0.16b, v13.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v14.16b
	aesmc	v0.16b, v0.16b
	ldp	q16, q17, [x7], #32
	aese	v0.16b, v15.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v16.16b
	aesmc	v0.16b, v0.16b
	ld1	{v18.16b}, [x7]
	cmp	x9, #12
	b.lt	Laes128_enc_prelog_0
Laes192_enc_prelog_0:
	ldp	q18,q19,[x7],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_prelog_0
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_0:
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ldp	q18,q19,[x7],32
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_0:
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	str	q0, [x1], #16
	ld1	{v1.16b}, [x0], #16
	eor	v1.16b, v1.16b, v0.16b

	aese	v1.16b, v8.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v9.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v10.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v11.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v12.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v13.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v14.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v15.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v16.16b
	aesmc	v1.16b, v1.16b
	b.lt	Laes128_enc_prelog_1
Laes192_enc_prelog_1:
	ldp	q18,q19,[x7],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_prelog_1
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_1:
	aese	v1.16b,v19.16b
	aesmc	v1.16b,v1.16b
	ldp	q18,q19,[x7],32
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:

	ld1	{v2.16b}, [x0], #16

	eor	v2.16b, v2.16b, v1.16b
	str	q1, [x1], #16

	aese	v2.16b, v8.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v9.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v10.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v11.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v12.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v13.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v14.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v15.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v16.16b
	aesmc	v2.16b, v2.16b
	b.lt	Laes128_enc_prelog_2
Laes192_enc_prelog_2:
	ldp	q18,q19,[x7],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_prelog_2
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_2:
	aese	v2.16b,v19.16b
	aesmc	v2.16b,v2.16b
	ldp	q18,q19,[x7],32
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:

	ld1	{v3.16b}, [x0], #16

	eor	v3.16b, v3.16b, v2.16b
	str	q2, [x1], #16

	aese	v3.16b, v8.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v9.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v10.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v11.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v12.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v13.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v14.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v15.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v16.16b
	aesmc	v3.16b, v3.16b
	b.lt	Laes128_enc_prelog_3
Laes192_enc_prelog_3:
	ldp	q18,q19,[x7],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_prelog_3
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_3:
	aese	v3.16b,v19.16b
	aesmc	v3.16b,v3.16b
	ldp	q18,q19,[x7],32
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:

	ld1	{v4.16b}, [x0], #16

	eor	v4.16b, v4.16b, v3.16b
	str	q3, [x1], #16

	aese	v4.16b, v8.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v9.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v10.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v11.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v12.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v13.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v14.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v15.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v16.16b
	aesmc	v4.16b, v4.16b
	b.lt	Laes128_enc_prelog_4
Laes192_enc_prelog_4:
	ldp	q18,q19,[x7],32
	aese	v4.16b,v17.16b
	aesmc	v4.16b,v4.16b
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	b.gt	Laes256_enc_prelog_4
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_4:
	aese	v4.16b,v19.16b
	aesmc	v4.16b,v4.16b
	ldp	q18,q19,[x7],32
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_4:
	aese	v4.16b,v17.16b
	eor	v4.16b,v4.16b,v18.16b
1:

	ld1	{v5.16b}, [x0], #16

	eor	v5.16b, v5.16b, v4.16b
	str	q4, [x1], #16

	aese	v5.16b, v8.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v9.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v10.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v11.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v12.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v13.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v14.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v15.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v16.16b
	aesmc	v5.16b, v5.16b
	b.lt	Laes128_enc_prelog_5
Laes192_enc_prelog_5:
	ldp	q18,q19,[x7],32
	aese	v5.16b,v17.16b
	aesmc	v5.16b,v5.16b
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	b.gt	Laes256_enc_prelog_5
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_5:
	aese	v5.16b,v19.16b
	aesmc	v5.16b,v5.16b
	ldp	q18,q19,[x7],32
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_5:
	aese	v5.16b,v17.16b
	eor	v5.16b,v5.16b,v18.16b
1:

	ld1	{v6.16b}, [x0], #16

	eor	v6.16b, v6.16b, v5.16b
	str	q5, [x1], #16

	aese	v6.16b, v8.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v9.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v10.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v11.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v12.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v13.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v14.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v15.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v16.16b
	aesmc	v6.16b, v6.16b
	b.lt	Laes128_enc_prelog_6
Laes192_enc_prelog_6:
	ldp	q18,q19,[x7],32
	aese	v6.16b,v17.16b
	aesmc	v6.16b,v6.16b
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	b.gt	Laes256_enc_prelog_6
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_6:
	aese	v6.16b,v19.16b
	aesmc	v6.16b,v6.16b
	ldp	q18,q19,[x7],32
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_6:
	aese	v6.16b,v17.16b
	eor	v6.16b,v6.16b,v18.16b
1:

	ld1	{v7.16b}, [x0], #16

	eor	v7.16b, v7.16b, v6.16b
	str	q6, [x1], #16

	aese	v7.16b, v8.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v9.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v10.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v11.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v12.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v13.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v14.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v15.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v16.16b
	aesmc	v7.16b, v7.16b
	b.lt	Laes128_enc_prelog_7
Laes192_enc_prelog_7:
	ldp	q18,q19,[x7],32
	aese	v7.16b,v17.16b
	aesmc	v7.16b,v7.16b
	aese	v7.16b,v18.16b
	aesmc	v7.16b,v7.16b
	b.gt	Laes256_enc_prelog_7
	ld1	{v18.16b},[x7]
	aese	v7.16b,v19.16b
	eor	v7.16b,v7.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_prelog_7:
	aese	v7.16b,v19.16b
	aesmc	v7.16b,v7.16b
	ldp	q18,q19,[x7],32
	aese	v7.16b,v18.16b
	aesmc	v7.16b,v7.16b
	ld1	{v18.16b},[x7]
	aese	v7.16b,v19.16b
	eor	v7.16b,v7.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_prelog_7:
	aese	v7.16b,v17.16b
	eor	v7.16b,v7.16b,v18.16b
1:
	str	q7, [x1], #16
	sub	x11, x11, #8

Lenc_main_loop:
	mov	x7, x12
	mov	x14, x1

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v12.16b}, [x0], #16
	eor	v12.16b, v12.16b, v7.16b


	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b
	ld1	{v14.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b

	add	v14.2d, v14.2d, v0.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d

	add	v27.2d, v27.2d, v14.2d
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v15.2d, v15.2d, v1.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d

	add	v26.2d, v26.2d, v15.2d
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v14.2d, v14.2d, v2.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d

	add	v28.2d, v28.2d, v14.2d
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v15.2d, v15.2d, v3.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d

	add	v25.2d, v25.2d, v15.2d
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v14.2d, v14.2d, v4.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d

	add	v24.2d, v24.2d, v14.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	cmp	x9, #12
	b.lt	Lenc_main_loop_aes128_0
Lenc_main_loop_aes192_0:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_0
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_0:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_0:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v15.2d, v15.2d, v5.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8

	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d

	add	v27.2d, v27.2d, v15.2d
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v14.2d, v14.2d, v6.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d

	add	v26.2d, v26.2d, v14.2d
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v15.2d, v15.2d, v7.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8

.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d

	add	v28.2d, v28.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v14.2d, v14.2d, v0.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d

	add	v25.2d, v25.2d, v14.2d
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v15.2d, v15.2d, v1.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d

	add	v24.2d, v24.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	cmp	x9, #12
	b.lt	Lenc_main_loop_aes128_1
Lenc_main_loop_aes192_1:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_1
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_1:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_1:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v14.2d, v14.2d, v2.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8

.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d

	add	v27.2d, v27.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v15.2d, v15.2d, v3.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d

	add	v26.2d, v26.2d, v15.2d
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v14.2d, v14.2d, v4.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d

	add	v28.2d, v28.2d, v14.2d
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v15.2d, v15.2d, v5.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d

	add	v25.2d, v25.2d, v15.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v14.2d, v14.2d, v6.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d

	add	v24.2d, v24.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	cmp	x9, #12
	b.lt	Lenc_main_loop_aes128_2
Lenc_main_loop_aes192_2:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_2
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_2:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_2:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v15.2d, v15.2d, v7.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8

.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d

	add	v27.2d, v27.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v14.2d, v14.2d, v0.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d

	add	v26.2d, v26.2d, v14.2d
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v15.2d, v15.2d, v1.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d

	add	v28.2d, v28.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v14.2d, v14.2d, v2.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d

	add	v25.2d, v25.2d, v14.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v15.2d, v15.2d, v3.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d

	add	v24.2d, v24.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	cmp	x9, #12
	b.lt	Lenc_main_loop_aes128_3
Lenc_main_loop_aes192_3:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_3
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_3:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_3:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v14.2d, v14.2d, v4.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8

.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d

	add	v27.2d, v27.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v15.2d, v15.2d, v5.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d

	add	v26.2d, v26.2d, v15.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v14.2d, v14.2d, v6.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d

	add	v28.2d, v28.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v15.2d, v15.2d, v7.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d

	add	v25.2d, v25.2d, v15.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v14.2d, v14.2d, v0.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d

	add	v24.2d, v24.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	cmp	x9, #12
	b.lt	Lenc_main_loop_aes128_4
Lenc_main_loop_aes192_4:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_4
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_4:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_4:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v15.2d, v15.2d, v1.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8

.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d

	add	v27.2d, v27.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v14.2d, v14.2d, v2.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d

	add	v26.2d, v26.2d, v14.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v15.2d, v15.2d, v3.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d

	add	v28.2d, v28.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v14.2d, v14.2d, v4.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d

	add	v25.2d, v25.2d, v14.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v15.2d, v15.2d, v5.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d

	add	v24.2d, v24.2d, v15.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	cmp	x9, #12

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	b.lt	Lenc_main_loop_aes128_5
Lenc_main_loop_aes192_5:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_5
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_5:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_5:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	add	v14.2d, v14.2d, v6.2d
	ld1	{v15.2d}, [x10], #16
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8

.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d

	add	v27.2d, v27.2d, v14.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v15.2d, v15.2d, v7.2d
	ld1	{v14.2d}, [x10], #16
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d

	add	v26.2d, v26.2d, v15.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v15.2d},[x10],#16
	add	v14.2d, v14.2d, v0.2d
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

	add	v28.2d, v28.2d, v14.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v14.2d},[x10],#16
	add	v15.2d, v15.2d, v1.2d
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

	add	v25.2d, v25.2d, v15.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v15.2d},[x10],#16
	add	v14.2d, v14.2d, v2.2d
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

	add	v24.2d, v24.2d, v14.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	cmp	x9, #12
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	b.lt	Lenc_main_loop_aes128_6
Lenc_main_loop_aes192_6:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_6
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_6:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_6:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	st1	{v12.16b}, [x1], #16

	mov	x7, x12
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ld1	{v13.16b}, [x0], #16
	eor	v12.16b, v12.16b, v13.16b

	ld1	{v14.2d},[x10],#16
	add	v15.2d, v15.2d, v3.2d
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8

	add	v27.2d, v27.2d, v15.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v15.2d},[x10],#16
	add	v14.2d, v14.2d, v4.2d
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

	add	v26.2d, v26.2d, v14.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v14.2d},[x10],#16
	add	v15.2d, v15.2d, v5.2d
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ldp	q10, q11, [x7], #32

	add	v28.2d, v28.2d, v15.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v15.2d},[x10],#16
	add	v14.2d, v14.2d, v6.2d
	ext	v14.16b, v14.16b, v14.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	ldp	q8, q9, [x7], #32

	add	v25.2d, v25.2d, v14.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v15.2d, v15.2d, v7.2d
	ext	v15.16b, v15.16b, v15.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b

	add	v24.2d, v24.2d, v15.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	cmp	x9, #12
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	b.lt	Lenc_main_loop_aes128_7
Lenc_main_loop_aes192_7:
	ldp	q10, q11, [x7], #32
	aese	v12.16b, v9.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v10.16b
	aesmc	v12.16b, v12.16b
	b.gt	Lenc_main_loop_aes256_7
	ld1	{v8.16b},[x7]
	aese	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	b	1f
Lenc_main_loop_aes256_7:
	ldp	q8, q9, [x7], #32
	aese	v12.16b, v11.16b
	aesmc	v12.16b, v12.16b
	ld1	{v10.16b},[x7]
	aese	v12.16b, v8.16b
	aesmc	v12.16b, v12.16b
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	b	1f
Lenc_main_loop_aes128_7:
	ld1	{v10.16b},[x7]
	aese	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
1:
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d

	st1	{v12.16b}, [x1], #16

	ld1	{v0.16b, v1.16b, v2.16b, v3.16b}, [x14], #64
	ld1	{v4.16b, v5.16b, v6.16b, v7.16b}, [x14]

	sub	x11, x11, #8
	cmp	x11, #8
	b.ge	Lenc_main_loop


	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
	mov	x7, x12
	ld1	{v0.16b}, [x0], #16
	ldr	q1, [x14, #48]
	eor	v0.16b, v0.16b, v1.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32
	ldp	q12, q13, [x7], #32
	ldp	q14, q15, [x7], #32
	ldp	q16, q17, [x7], #32
	ld1	{v18.16b}, [x7]

	mov	w12, #0x80
	b	Lenc_less_than_8_block


Lenc_short_case:
	ld1	{v0.16b}, [x0], #16
	ld1	{v1.16b}, [x8]
	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32
	ldp	q12, q13, [x7], #32
	ldp	q14, q15, [x7], #32
	ldp	q16, q17, [x7], #32
	ld1	{v18.16b}, [x7]
	mov	w12, #0x80

	eor	v0.16b, v0.16b, v1.16b

	cmp	x11, #8
	b.lt	Lenc_less_than_8_block

	aese	v0.16b, v8.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v9.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v10.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v11.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v12.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v13.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v14.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v15.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v16.16b
	aesmc	v0.16b, v0.16b
	cmp	x9, #12
	b.lt	Laes128_enc_short_0
Laes192_enc_short_0:
	ldp	q18,q19,[x7],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_short_0
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_0:
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ldp	q18,q19,[x7],32
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_0:
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:

	ld1	{v1.16b}, [x0], #16

	eor	v1.16b, v1.16b, v0.16b

	aese	v1.16b, v8.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v9.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v10.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v11.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v12.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v13.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v14.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v15.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v16.16b
	aesmc	v1.16b, v1.16b
	b.lt	Laes128_enc_short_1
Laes192_enc_short_1:
	ldp	q18,q19,[x7],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_short_1
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_1:
	aese	v1.16b,v19.16b
	aesmc	v1.16b,v1.16b
	ldp	q18,q19,[x7],32
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:

	ld1	{v2.16b}, [x0], #16

	eor	v2.16b, v2.16b, v1.16b

	aese	v2.16b, v8.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v9.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v10.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v11.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v12.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v13.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v14.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v15.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v16.16b
	aesmc	v2.16b, v2.16b
	b.lt	Laes128_enc_short_2
Laes192_enc_short_2:
	ldp	q18,q19,[x7],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_short_2
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_2:
	aese	v2.16b,v19.16b
	aesmc	v2.16b,v2.16b
	ldp	q18,q19,[x7],32
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:

	ld1	{v3.16b}, [x0], #16

	eor	v3.16b, v3.16b, v2.16b

	aese	v3.16b, v8.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v9.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v10.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v11.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v12.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v13.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v14.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v15.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v16.16b
	aesmc	v3.16b, v3.16b
	b.lt	Laes128_enc_short_3
Laes192_enc_short_3:
	ldp	q18,q19,[x7],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_short_3
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_3:
	aese	v3.16b,v19.16b
	aesmc	v3.16b,v3.16b
	ldp	q18,q19,[x7],32
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:

	ld1	{v4.16b}, [x0], #16

	eor	v4.16b, v4.16b, v3.16b

	aese	v4.16b, v8.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v9.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v10.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v11.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v12.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v13.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v14.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v15.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v16.16b
	aesmc	v4.16b, v4.16b
	b.lt	Laes128_enc_short_4
Laes192_enc_short_4:
	ldp	q18,q19,[x7],32
	aese	v4.16b,v17.16b
	aesmc	v4.16b,v4.16b
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	b.gt	Laes256_enc_short_4
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_4:
	aese	v4.16b,v19.16b
	aesmc	v4.16b,v4.16b
	ldp	q18,q19,[x7],32
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_4:
	aese	v4.16b,v17.16b
	eor	v4.16b,v4.16b,v18.16b
1:

	ld1	{v5.16b}, [x0], #16

	eor	v5.16b, v5.16b, v4.16b

	aese	v5.16b, v8.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v9.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v10.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v11.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v12.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v13.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v14.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v15.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v16.16b
	aesmc	v5.16b, v5.16b
	b.lt	Laes128_enc_short_5
Laes192_enc_short_5:
	ldp	q18,q19,[x7],32
	aese	v5.16b,v17.16b
	aesmc	v5.16b,v5.16b
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	b.gt	Laes256_enc_short_5
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_5:
	aese	v5.16b,v19.16b
	aesmc	v5.16b,v5.16b
	ldp	q18,q19,[x7],32
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_5:
	aese	v5.16b,v17.16b
	eor	v5.16b,v5.16b,v18.16b
1:

	ld1	{v6.16b}, [x0], #16

	eor	v6.16b, v6.16b, v5.16b

	aese	v6.16b, v8.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v9.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v10.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v11.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v12.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v13.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v14.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v15.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v16.16b
	aesmc	v6.16b, v6.16b
	b.lt	Laes128_enc_short_6
Laes192_enc_short_6:
	ldp	q18,q19,[x7],32
	aese	v6.16b,v17.16b
	aesmc	v6.16b,v6.16b
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	b.gt	Laes256_enc_short_6
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_6:
	aese	v6.16b,v19.16b
	aesmc	v6.16b,v6.16b
	ldp	q18,q19,[x7],32
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_6:
	aese	v6.16b,v17.16b
	eor	v6.16b,v6.16b,v18.16b
1:

	ld1	{v7.16b}, [x0], #16

	eor	v7.16b, v7.16b, v6.16b

	aese	v7.16b, v8.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v9.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v10.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v11.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v12.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v13.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v14.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v15.16b
	aesmc	v7.16b, v7.16b
	aese	v7.16b, v16.16b
	aesmc	v7.16b, v7.16b
	b.lt	Laes128_enc_short_7
Laes192_enc_short_7:
	ldp	q18,q19,[x7],32
	aese	v7.16b,v17.16b
	aesmc	v7.16b,v7.16b
	aese	v7.16b,v18.16b
	aesmc	v7.16b,v7.16b
	b.gt	Laes256_enc_short_7
	ld1	{v18.16b},[x7]
	aese	v7.16b,v19.16b
	eor	v7.16b,v7.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_7:
	aese	v7.16b,v19.16b
	aesmc	v7.16b,v7.16b
	ldp	q18,q19,[x7],32
	aese	v7.16b,v18.16b
	aesmc	v7.16b,v7.16b
	ld1	{v18.16b},[x7]
	aese	v7.16b,v19.16b
	eor	v7.16b,v7.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_7:
	aese	v7.16b,v17.16b
	eor	v7.16b,v7.16b,v18.16b
1:

	stp	q0, q1, [x1], #32
	stp	q2, q3, [x1], #32
	stp	q4, q5, [x1], #32
	stp	q6, q7, [x1], #32

	sub	x11, x11, #8
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
	ldr	x7, [x6, #CIPHER_KEY]
	ldp	q8, q9, [x7]
	add	x7, x7, #160
	cbz	x11, Lenc_short_no_more_aes_block
	ld1	{v0.16b}, [x0], #16
	ldr	q1, [x1, -16]
	eor	v0.16b, v0.16b, v1.16b
Lenc_less_than_8_block:
	cbz	x11, Lenc_short_no_more_aes_block

	aese	v0.16b, v8.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v9.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v10.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v11.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v12.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v13.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v14.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v15.16b
	aesmc	v0.16b, v0.16b
	aese	v0.16b, v16.16b
	aesmc	v0.16b, v0.16b
	cmp	x9, #12
	b.lt	Laes128_enc_short_partial_0
Laes192_enc_short_partial_0:
	ldp	q18,q19,[x7],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_short_partial_0
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_0:
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ldp	q18,q19,[x7],32
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	ld1	{v18.16b},[x7]
	aese	v0.16b,v19.16b
	eor	v0.16b,v0.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_0:
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	str	q0, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q0

	ld1	{v1.16b}, [x0], #16

	eor	v1.16b, v1.16b, v0.16b

	aese	v1.16b, v8.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v9.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v10.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v11.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v12.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v13.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v14.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v15.16b
	aesmc	v1.16b, v1.16b
	aese	v1.16b, v16.16b
	aesmc	v1.16b, v1.16b
	b.lt	Laes128_enc_short_partial_1
Laes192_enc_short_partial_1:
	ldp	q18,q19,[x7],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_short_partial_1
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_1:
	aese	v1.16b,v19.16b
	aesmc	v1.16b,v1.16b
	ldp	q18,q19,[x7],32
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	ld1	{v18.16b},[x7]
	aese	v1.16b,v19.16b
	eor	v1.16b,v1.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	str	q1, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q1

	ld1	{v2.16b}, [x0], #16

	eor	v2.16b, v2.16b, v1.16b

	aese	v2.16b, v8.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v9.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v10.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v11.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v12.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v13.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v14.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v15.16b
	aesmc	v2.16b, v2.16b
	aese	v2.16b, v16.16b
	aesmc	v2.16b, v2.16b
	b.lt	Laes128_enc_short_partial_2
Laes192_enc_short_partial_2:
	ldp	q18,q19,[x7],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_short_partial_2
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_2:
	aese	v2.16b,v19.16b
	aesmc	v2.16b,v2.16b
	ldp	q18,q19,[x7],32
	aese	v2.16b,v18.16b
	aesmc	v2.16b,v2.16b
	ld1	{v18.16b},[x7]
	aese	v2.16b,v19.16b
	eor	v2.16b,v2.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	str	q2, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q2

	ld1	{v3.16b}, [x0], #16

	eor	v3.16b, v3.16b, v2.16b

	aese	v3.16b, v8.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v9.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v10.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v11.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v12.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v13.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v14.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v15.16b
	aesmc	v3.16b, v3.16b
	aese	v3.16b, v16.16b
	aesmc	v3.16b, v3.16b
	b.lt	Laes128_enc_short_partial_3
Laes192_enc_short_partial_3:
	ldp	q18,q19,[x7],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_short_partial_3
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_3:
	aese	v3.16b,v19.16b
	aesmc	v3.16b,v3.16b
	ldp	q18,q19,[x7],32
	aese	v3.16b,v18.16b
	aesmc	v3.16b,v3.16b
	ld1	{v18.16b},[x7]
	aese	v3.16b,v19.16b
	eor	v3.16b,v3.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	str	q3, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q3

	ld1	{v4.16b}, [x0], #16

	eor	v4.16b, v4.16b, v3.16b

	aese	v4.16b, v8.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v9.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v10.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v11.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v12.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v13.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v14.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v15.16b
	aesmc	v4.16b, v4.16b
	aese	v4.16b, v16.16b
	aesmc	v4.16b, v4.16b
	b.lt	Laes128_enc_short_partial_4
Laes192_enc_short_partial_4:
	ldp	q18,q19,[x7],32
	aese	v4.16b,v17.16b
	aesmc	v4.16b,v4.16b
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	b.gt	Laes256_enc_short_partial_4
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_4:
	aese	v4.16b,v19.16b
	aesmc	v4.16b,v4.16b
	ldp	q18,q19,[x7],32
	aese	v4.16b,v18.16b
	aesmc	v4.16b,v4.16b
	ld1	{v18.16b},[x7]
	aese	v4.16b,v19.16b
	eor	v4.16b,v4.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_4:
	aese	v4.16b,v17.16b
	eor	v4.16b,v4.16b,v18.16b
1:
	str	q4, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q4

	ld1	{v5.16b}, [x0], #16

	eor	v5.16b, v5.16b, v4.16b

	aese	v5.16b, v8.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v9.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v10.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v11.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v12.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v13.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v14.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v15.16b
	aesmc	v5.16b, v5.16b
	aese	v5.16b, v16.16b
	aesmc	v5.16b, v5.16b
	b.lt	Laes128_enc_short_partial_5
Laes192_enc_short_partial_5:
	ldp	q18,q19,[x7],32
	aese	v5.16b,v17.16b
	aesmc	v5.16b,v5.16b
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	b.gt	Laes256_enc_short_partial_5
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_5:
	aese	v5.16b,v19.16b
	aesmc	v5.16b,v5.16b
	ldp	q18,q19,[x7],32
	aese	v5.16b,v18.16b
	aesmc	v5.16b,v5.16b
	ld1	{v18.16b},[x7]
	aese	v5.16b,v19.16b
	eor	v5.16b,v5.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_5:
	aese	v5.16b,v17.16b
	eor	v5.16b,v5.16b,v18.16b
1:
	str	q5, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q5

	ld1	{v6.16b}, [x0], #16

	eor	v6.16b, v6.16b, v5.16b

	aese	v6.16b, v8.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v9.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v10.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v11.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v12.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v13.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v14.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v15.16b
	aesmc	v6.16b, v6.16b
	aese	v6.16b, v16.16b
	aesmc	v6.16b, v6.16b
	b.lt	Laes128_enc_short_partial_6
Laes192_enc_short_partial_6:
	ldp	q18,q19,[x7],32
	aese	v6.16b,v17.16b
	aesmc	v6.16b,v6.16b
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	b.gt	Laes256_enc_short_partial_6
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #32
	b	1f
Laes256_enc_short_partial_6:
	aese	v6.16b,v19.16b
	aesmc	v6.16b,v6.16b
	ldp	q18,q19,[x7],32
	aese	v6.16b,v18.16b
	aesmc	v6.16b,v6.16b
	ld1	{v18.16b},[x7]
	aese	v6.16b,v19.16b
	eor	v6.16b,v6.16b,v18.16b
	sub	x7, x7, #64
	b	1f
Laes128_enc_short_partial_6:
	aese	v6.16b,v17.16b
	eor	v6.16b,v6.16b,v18.16b
1:
	str	q6, [x1], #16
	sub	x11, x11, #1
	cbz	x11, Lenc_short_post_Q6
Lenc_short_no_more_aes_block:
	eor	v0.16b, v0.16b, v0.16b
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v0.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q0:
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v1.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q1:
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v2.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q2:
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v3.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q3:
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v4.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q4:
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v5.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q5:
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v6.b[0], w12
	b	Lenc_short_post_sha
Lenc_short_post_Q6:
	eor	v7.16b, v7.16b, v7.16b
	mov	v7.b[0], w12
	/*	we have one padded sha512 block now, process it and
	then	employ another one to host sha length */
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
	eor	v0.16b, v0.16b, v0.16b
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
Lenc_short_post_sha:

	eor	x13, x13, x13
	eor	x14, x14, x14

	adds	x13, x13, x2, lsl #3
	lsr	x15, x2, #61
	adc	x14, x14, x15

	adds	x13, x13, #1024
	adc	x14, x14, xzr

	mov	v7.d[0], x14
	mov	v7.d[1], x13
	rev64	v7.16b, v7.16b
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d

	mov	v0.16b, v24.16b
	mov	v1.16b, v25.16b
	mov	v2.16b, v26.16b
	mov	v3.16b, v27.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b

	mov	v4.b[7], w12
	mov	x13, #1024+512
	mov	v7.d[1], x13


	ldr	x7, [x6, #HMAC_OKEYPAD]
	ld1	{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
Lret:
	mov	x0, xzr

	rev64	v24.16b, v24.16b
	rev64	v25.16b, v25.16b
	rev64	v26.16b, v26.16b
	rev64	v27.16b, v27.16b


	st1	{v24.2d,v25.2d,v26.2d,v27.2d},[x4]


	ldp	d10, d11, [sp,#16]
	ldp	d12, d13, [sp,#32]
	ldp	d14, d15, [sp,#48]
	ldp	d8, d9, [sp], #64
	ret

	/*
	*	asm_sha512_hmac_aescbc_dec(
	*	csrc,	x0	(cipher src address)
	*	cdst,	x1	(cipher dst address)
	*	clen	x2	(cipher length)
	*	dsrc,	x3	(digest src address)
	*	ddst,	x4	(digest dst address)
	*	dlen,	x5	(digest length)
	*	arg	x6	:
	*	arg->cipher.key			(round keys)
	*	arg->cipher.key_rounds		(key rounds)
	*	arg->cipher.iv			(initialization vector)
	*	arg->digest.hmac.i_key_pad	(partially hashed i_key_pad)
	*	arg->digest.hmac.o_key_pad	(partially hashed o_key_pad)
	*	)
	*/

.globl	_asm_sha512_hmac_aescbc_dec


.align	4
_asm_sha512_hmac_aescbc_dec:
	AARCH64_VALID_CALL_TARGET

	stp	d8, d9, [sp,#-64]!
	stp	d10, d11, [sp,#16]
	stp	d12, d13, [sp,#32]
	stp	d14, d15, [sp,#48]


	ldr	x7, [x6, #HMAC_IKEYPAD]
	ld1	{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ldr	x7, [x6, #CIPHER_KEY]
	ldr	x8, [x6, #CIPHER_IV]
	ldr	x9, [x6, #CIPHER_KEY_ROUNDS]
	mov	x12, x7

	adrp	x10, LK512@PAGE
	add	x10, x10, LK512@PAGEOFF

	lsr	x11, x2, #4
	cbz	x11, Ldec_ret

	ld1	{v20.16b}, [x8]
	cmp	x11, #8
	b.lt	Ldec_short_case
Ldec_main_loop:
	ldp	q12, q13, [x0], #32
	ldp	q14, q15, [x0], #32
	ldp	q16, q17, [x0], #32
	ldp	q18, q19, [x0], #32

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	mov	v0.16b, v12.16b
	mov	v1.16b, v13.16b
	mov	v2.16b, v14.16b
	mov	v3.16b, v15.16b
	mov	v4.16b, v16.16b
	mov	v5.16b, v17.16b
	mov	v6.16b, v18.16b
	mov	v7.16b, v19.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v8.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v8.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v8.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v9.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v9.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v9.16b
	aesimc	v19.16b, v19.16b

	ldp	q8, q9, [x7], #32


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v10.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v10.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v10.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v11.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v11.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v11.16b
	aesimc	v19.16b, v19.16b

	ldp	q10, q11, [x7], #32


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v8.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v8.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v8.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v9.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v9.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v9.16b
	aesimc	v19.16b, v19.16b

	ldp	q8, q9, [x7], #32


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v10.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v10.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v10.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v11.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v11.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v11.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v8.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v8.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v8.16b
	aesimc	v19.16b, v19.16b

	cmp	x9, #12

	b.lt	Laes128_dec_main
Laes192_dec_main:
	ldp	q10,q11,[x7],32

	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v9.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v9.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v9.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v10.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v10.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v10.16b
	aesimc	v19.16b, v19.16b
	b.gt	Laes256_dec_main

	ld1	{v8.16b},[x7]


	aesd	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	aesd	v13.16b, v11.16b
	eor	v13.16b, v13.16b, v8.16b
	aesd	v14.16b, v11.16b
	eor	v14.16b, v14.16b, v8.16b
	aesd	v15.16b, v11.16b
	eor	v15.16b, v15.16b, v8.16b
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	aesd	v17.16b, v11.16b
	eor	v17.16b, v17.16b, v8.16b
	aesd	v18.16b, v11.16b
	eor	v18.16b, v18.16b, v8.16b
	aesd	v19.16b, v11.16b
	eor	v19.16b, v19.16b, v8.16b

	sub	x7, x7, #192
	b	1f
Laes256_dec_main:
	ldp	q8,q9,[x7],32

	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v11.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v11.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v11.16b
	aesimc	v19.16b, v19.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v17.16b, v8.16b
	aesimc	v17.16b, v17.16b
	aesd	v18.16b, v8.16b
	aesimc	v18.16b, v18.16b
	aesd	v19.16b, v8.16b
	aesimc	v19.16b, v19.16b

	ld1	{v10.16b},[x7]


	aesd	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	aesd	v13.16b, v9.16b
	eor	v13.16b, v13.16b, v10.16b
	aesd	v14.16b, v9.16b
	eor	v14.16b, v14.16b, v10.16b
	aesd	v15.16b, v9.16b
	eor	v15.16b, v15.16b, v10.16b
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	aesd	v17.16b, v9.16b
	eor	v17.16b, v17.16b, v10.16b
	aesd	v18.16b, v9.16b
	eor	v18.16b, v18.16b, v10.16b
	aesd	v19.16b, v9.16b
	eor	v19.16b, v19.16b, v10.16b

	sub	x7, x7, #224
	b	1f
Laes128_dec_main:
	ld1	{v10.16b},[x7]
	aesd	v12.16b,v9.16b
	eor	v12.16b, v12.16b, v10.16b
	aesd	v13.16b,v9.16b
	eor	v13.16b, v13.16b, v10.16b
	aesd	v14.16b,v9.16b
	eor	v14.16b, v14.16b, v10.16b
	aesd	v15.16b,v9.16b
	eor	v15.16b, v15.16b, v10.16b
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	aesd	v17.16b,v9.16b
	eor	v17.16b, v17.16b, v10.16b
	aesd	v18.16b,v9.16b
	eor	v18.16b, v18.16b, v10.16b
	aesd	v19.16b,v9.16b
	eor	v19.16b, v19.16b, v10.16b
	sub	x7, x7, #160

1:
	eor	v12.16b, v12.16b, v20.16b
	eor	v13.16b, v13.16b, v0.16b
	eor	v14.16b, v14.16b, v1.16b
	eor	v15.16b, v15.16b, v2.16b
	eor	v16.16b, v16.16b, v3.16b
	eor	v17.16b, v17.16b, v4.16b
	eor	v18.16b, v18.16b, v5.16b
	eor	v19.16b, v19.16b, v6.16b

	stp	q12,q13, [x1], #32
	ldr	q12, [x0, #-16]
	stp	q14,q15, [x1], #32
	stp	q16,q17, [x1], #32
	stp	q18,q19, [x1], #32
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
	mov	v20.16b, v12.16b
	sub	x11, x11, #8
	cmp	x11, #8
	b.ge	Ldec_main_loop


Ldec_short_case:
	mov	w12, #0x80
	cbnz	x11, 1f
	eor	v0.16b, v0.16b, v0.16b
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v0.b[0], w12
	b	Ldec_short_post_sha
1:
	cmp	x11, #4
	b.lt	Ldec_less_than_4_block

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	ldp	q12, q13, [x0], #32
	ldp	q14, q15, [x0], #32

	mov	v0.16b, v12.16b
	mov	v1.16b, v13.16b
	mov	v2.16b, v14.16b
	mov	v3.16b, v15.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b

	ldp	q8, q9, [x7], #32


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b

	ldp	q10, q11, [x7], #32


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b

	ldp	q8, q9, [x7], #32


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b

	cmp	x9, #12

	b.lt	Laes128_dec_short
Laes192_dec_short:
	ldp	q10,q11,[x7],32


	aesd	v12.16b, v9.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v9.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v9.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v9.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v10.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v10.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v10.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v10.16b
	aesimc	v15.16b, v15.16b
	b.gt	Laes256_dec_short

	ld1	{v8.16b},[x7]


	aesd	v12.16b, v11.16b
	eor	v12.16b, v12.16b, v8.16b
	aesd	v13.16b, v11.16b
	eor	v13.16b, v13.16b, v8.16b
	aesd	v14.16b, v11.16b
	eor	v14.16b, v14.16b, v8.16b
	aesd	v15.16b, v11.16b
	eor	v15.16b, v15.16b, v8.16b

	sub	x7, x7, #192
	b	1f
Laes256_dec_short:
	ldp	q8,q9,[x7],32

	aesd	v12.16b, v11.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v11.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v11.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v11.16b
	aesimc	v15.16b, v15.16b


	aesd	v12.16b, v8.16b
	aesimc	v12.16b, v12.16b
	aesd	v13.16b, v8.16b
	aesimc	v13.16b, v13.16b
	aesd	v14.16b, v8.16b
	aesimc	v14.16b, v14.16b
	aesd	v15.16b, v8.16b
	aesimc	v15.16b, v15.16b

	ld1	{v10.16b},[x7]


	aesd	v12.16b, v9.16b
	eor	v12.16b, v12.16b, v10.16b
	aesd	v13.16b, v9.16b
	eor	v13.16b, v13.16b, v10.16b
	aesd	v14.16b, v9.16b
	eor	v14.16b, v14.16b, v10.16b
	aesd	v15.16b, v9.16b
	eor	v15.16b, v15.16b, v10.16b

	sub	x7, x7, #224
	b	1f
Laes128_dec_short:
	ld1	{v10.16b},[x7]
	aesd	v12.16b,v9.16b
	eor	v12.16b, v12.16b, v10.16b
	aesd	v13.16b,v9.16b
	eor	v13.16b, v13.16b, v10.16b
	aesd	v14.16b,v9.16b
	eor	v14.16b, v14.16b, v10.16b
	aesd	v15.16b,v9.16b
	eor	v15.16b, v15.16b, v10.16b
	sub	x7, x7, #160
1:
	eor	v12.16b, v12.16b, v20.16b
	eor	v13.16b, v13.16b, v0.16b
	eor	v14.16b, v14.16b, v1.16b
	eor	v15.16b, v15.16b, v2.16b
	ldr	q20, [x0, #-16]

	sub	x11, x11, #4

	stp	q12,q13, [x1], #32
	stp	q14,q15, [x1], #32
	cbz	x11, Ldec_short_post_Q3
	ld1	{v16.16b}, [x0], #16
	mov	v4.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_4
Laes192_dec_short_4:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_4
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_4:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_4:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q4
	ld1	{v16.16b}, [x0], #16
	mov	v5.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_5
Laes192_dec_short_5:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_5
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_5:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_5:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q5
	ld1	{v16.16b}, [x0], #16
	mov	v6.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_6
Laes192_dec_short_6:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_6
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_6:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_6:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q6
Ldec_short_post_Q3:
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v4.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_Q4:
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v5.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_Q5:
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v6.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_Q6:
	eor	v7.16b, v7.16b, v7.16b
	mov	v7.b[0], w12
	/*	we have one padded sha512 block now, process it and
	then	employ another one to host sha length */
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
	eor	v0.16b, v0.16b, v0.16b
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	b	Ldec_short_post_sha

Ldec_less_than_4_block:
	ld1	{v16.16b}, [x0], #16
	mov	v0.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_less_than_4_0
Laes192_dec_short_less_than_4_0:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_less_than_4_0
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_less_than_4_0:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_less_than_4_0:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q0
	ld1	{v16.16b}, [x0], #16
	mov	v1.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_less_than_4_1
Laes192_dec_short_less_than_4_1:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_less_than_4_1
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_less_than_4_1:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_less_than_4_1:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q1
	ld1	{v16.16b}, [x0], #16
	mov	v2.16b, v16.16b

	ldp	q8, q9, [x7], #32
	ldp	q10, q11, [x7], #32

	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	ldp	q10, q11, [x7], #32
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	ldp	q8, q9, [x7], #32
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	cmp	x9, #12
	b.lt	Laes128_dec_short_less_than_4_2
Laes192_dec_short_less_than_4_2:
	ldp	q10,q11,[x7],32
	aesd	v16.16b, v9.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v10.16b
	aesimc	v16.16b, v16.16b
	b.gt	Laes256_dec_short_less_than_4_2
	ld1	{v8.16b},[x7]
	aesd	v16.16b, v11.16b
	eor	v16.16b, v16.16b, v8.16b
	sub	x7, x7, #192
	b	1f
Laes256_dec_short_less_than_4_2:
	ldp	q8,q9,[x7],32
	aesd	v16.16b, v11.16b
	aesimc	v16.16b, v16.16b
	aesd	v16.16b, v8.16b
	aesimc	v16.16b, v16.16b
	ld1	{v10.16b},[x7]
	aesd	v16.16b, v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #224
	b	1f
Laes128_dec_short_less_than_4_2:
	ld1	{v10.16b},[x7]
	aesd	v16.16b,v9.16b
	eor	v16.16b, v16.16b, v10.16b
	sub	x7, x7, #160
1:
	sub	x11, x11, 1
	eor	v16.16b, v16.16b, v20.16b
	ldr	q20, [x0, #-16]
	st1	{v16.16b}, [x1], #16
	cbz	x11, Ldec_short_post_Q2
Ldec_short_post_Q0:
	eor	v1.16b, v1.16b, v1.16b
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v1.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_Q1:
	eor	v2.16b, v2.16b, v2.16b
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v2.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_Q2:
	eor	v3.16b, v3.16b, v3.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b
	mov	v3.b[0], w12
	b	Ldec_short_post_sha
Ldec_short_post_sha:

	eor	x13, x13, x13
	eor	x14, x14, x14

	adds	x13, x13, x2, lsl #3
	lsr	x15, x2, #61
	adc	x14, x14, x15

	adds	x13, x13, #1024
	adc	x14, x14, xzr

	mov	v7.d[0], x14
	mov	v7.d[1], x13
	rev64	v7.16b, v7.16b
	rev64	v0.16b, v0.16b
	rev64	v1.16b, v1.16b
	rev64	v2.16b, v2.16b
	rev64	v3.16b, v3.16b
	rev64	v4.16b, v4.16b
	rev64	v5.16b, v5.16b
	rev64	v6.16b, v6.16b
	rev64	v7.16b, v7.16b

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d

	mov	v0.16b, v24.16b
	mov	v1.16b, v25.16b
	mov	v2.16b, v26.16b
	mov	v3.16b, v27.16b
	eor	v4.16b, v4.16b, v4.16b
	eor	v5.16b, v5.16b, v5.16b
	eor	v6.16b, v6.16b, v6.16b
	eor	v7.16b, v7.16b, v7.16b

	mov	v4.b[7], w12
	mov	x13, #1024+512
	mov	v7.d[1], x13


	ldr	x7, [x6, #HMAC_OKEYPAD]
	ld1	{v24.2d, v25.2d, v26.2d, v27.2d}, [x7]

	ld1	{v8.2d}, [x10], #16


	mov	v20.16b, v24.16b
	mov	v21.16b, v25.16b
	mov	v22.16b, v26.16b
	mov	v23.16b, v27.16b
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v8.2d, v8.2d, v0.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v4.16b, v5.16b, #8


.long	0xcec08020	//sha512su0 v0.2d, v1.2d

.long	0xce7f88e0	//sha512su1 v0.2d, v7.2d, v31.2d


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v9.2d, v9.2d, v1.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v5.16b, v6.16b, #8


.long	0xcec08041	//sha512su0 v1.2d, v2.2d

.long	0xce7f8801	//sha512su1 v1.2d, v0.2d, v31.2d


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v8.2d, v8.2d, v2.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v6.16b, v7.16b, #8


.long	0xcec08062	//sha512su0 v2.2d, v3.2d

.long	0xce7f8822	//sha512su1 v2.2d, v1.2d, v31.2d


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	add	v9.2d, v9.2d, v3.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8
	ext	v31.16b, v7.16b, v0.16b, #8


.long	0xcec08083	//sha512su0 v3.2d, v4.2d

.long	0xce7f8843	//sha512su1 v3.2d, v2.2d, v31.2d


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	add	v8.2d, v8.2d, v4.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8
	ext	v31.16b, v0.16b, v1.16b, #8


.long	0xcec080a4	//sha512su0 v4.2d, v5.2d

.long	0xce7f8864	//sha512su1 v4.2d, v3.2d, v31.2d


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	add	v9.2d, v9.2d, v5.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8
	ext	v31.16b, v1.16b, v2.16b, #8


.long	0xcec080c5	//sha512su0 v5.2d, v6.2d

.long	0xce7f8885	//sha512su1 v5.2d, v4.2d, v31.2d


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v8.2d, v8.2d, v6.2d
	ld1	{v9.2d}, [x10], #16
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8
	ext	v31.16b, v2.16b, v3.16b, #8


.long	0xcec080e6	//sha512su0 v6.2d, v7.2d

.long	0xce7f88a6	//sha512su1 v6.2d, v5.2d, v31.2d


	add	v27.2d, v27.2d, v8.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	add	v9.2d, v9.2d, v7.2d
	ld1	{v8.2d}, [x10], #16
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8
	ext	v31.16b, v3.16b, v4.16b, #8


.long	0xcec08007	//sha512su0 v7.2d, v0.2d

.long	0xce7f88c7	//sha512su1 v7.2d, v6.2d, v31.2d


	add	v26.2d, v26.2d, v9.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v0.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v8.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v1.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v9.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v2.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v8.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v3.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v26.16b, v27.16b, #8
	ext	v30.16b, v25.16b, v26.16b, #8


	add	v27.2d, v27.2d, v9.2d

.long	0xce7e83bb	//sha512h q27, q29, v30.2d
	add	v28.2d, v25.2d, v27.2d

.long	0xce78873b	//sha512h2 q27, q25, v24.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v4.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v28.16b, v26.16b, #8
	ext	v30.16b, v24.16b, v28.16b, #8


	add	v26.2d, v26.2d, v8.2d

.long	0xce7e83ba	//sha512h q26, q29, v30.2d
	add	v25.2d, v24.2d, v26.2d

.long	0xce7b871a	//sha512h2 q26, q24, v27.2d
	ld1	{v8.2d},[x10],#16
	add	v9.2d, v9.2d, v5.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v25.16b, v28.16b, #8
	ext	v30.16b, v27.16b, v25.16b, #8


	add	v28.2d, v28.2d, v9.2d

.long	0xce7e83bc	//sha512h q28, q29, v30.2d
	add	v24.2d, v27.2d, v28.2d

.long	0xce7a877c	//sha512h2 q28, q27, v26.2d
	ld1	{v9.2d},[x10],#16
	add	v8.2d, v8.2d, v6.2d
	ext	v8.16b, v8.16b, v8.16b, #8
	ext	v29.16b, v24.16b, v25.16b, #8
	ext	v30.16b, v26.16b, v24.16b, #8


	add	v25.2d, v25.2d, v8.2d

.long	0xce7e83b9	//sha512h q25, q29, v30.2d
	add	v27.2d, v26.2d, v25.2d

.long	0xce7c8759	//sha512h2 q25, q26, v28.2d
	sub	x10, x10, #80*8	// rewind
	add	v9.2d, v9.2d, v7.2d
	ext	v9.16b, v9.16b, v9.16b, #8
	ext	v29.16b, v27.16b, v24.16b, #8
	ext	v30.16b, v28.16b, v27.16b, #8


	add	v24.2d, v24.2d, v9.2d

.long	0xce7e83b8	//sha512h q24, q29, v30.2d
	add	v26.2d, v28.2d, v24.2d

.long	0xce798798	//sha512h2 q24, q28, v25.2d
	add	v24.2d, v24.2d, v20.2d
	add	v25.2d, v25.2d, v21.2d
	add	v26.2d, v26.2d, v22.2d
	add	v27.2d, v27.2d, v23.2d
Ldec_ret:
	mov	x0, xzr

	rev64	v24.16b, v24.16b
	rev64	v25.16b, v25.16b
	rev64	v26.16b, v26.16b
	rev64	v27.16b, v27.16b


	st1	{v24.2d,v25.2d,v26.2d,v27.2d},[x4]


	ldp	d10, d11, [sp,#16]
	ldp	d12, d13, [sp,#32]
	ldp	d14, d15, [sp,#48]
	ldp	d8, d9, [sp], #64
	ret

