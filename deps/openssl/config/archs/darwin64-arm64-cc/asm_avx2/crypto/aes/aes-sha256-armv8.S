#include "arch/arm_arch.h"


#define CIPHER_KEY	0
#define CIPHER_KEY_ROUNDS	8
#define CIPHER_IV	16
#define HMAC_IKEYPAD	24
#define HMAC_OKEYPAD	32

.text

	/*
	*	Description:
	*
	*	Combined Enc/Auth Primitive = aes128cbc/sha256_hmac
	*
	*	Operations:
	*
	*	out = encrypt-AES128CBC(in)
	*	return_hash_ptr = SHA256(o_key_pad | SHA256(i_key_pad | out))
	*
	*	Prototype:
	*	void asm_aescbc_sha256_hmac(uint8_t *csrc, uint8_t *cdst, uint64_t clen,
	*	uint8_t *dsrc, uint8_t *ddst, uint64_t dlen,
	*	CIPH_DIGEST *arg)
	*
	*	Registers used:
	*
	*	asm_aescbc_sha256_hmac(
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
	*
	*	Routine register definitions:
	*
	*	v0  -- v3 -- aes results
	*	v4  -- v7 -- round consts for sha
	*	v8  -- v18 -- round keys
	*	v19 -- v20 -- round keys
	*	v21 -- ABCD tmp
	*	v22 -- sha working state ABCD (q22)
	*	v23 -- sha working state EFGH (q23)
	*	v24 -- sha state ABCD
	*	v25 -- sha state EFGH
	*	v26 -- sha block 0
	*	v27 -- sha block 1
	*	v28 -- sha block 2
	*	v29 -- sha block 3
	*	v30 -- reserved
	*	v31 -- reserved
	*
	*	Constraints:
	*
	*	The variable "clen" must be a multiple of 16, otherwise results
	*	are not defined. For AES partial blocks the user is required
	*	to pad the input to modulus 16 = 0.
	*	The variable "dlen" must be a multiple of 8 and greater or equal
	*	to "clen". This constrain is strictly related to the needs of the IPSec
	*	ESP packet. Encrypted payload is hashed along with the 8 byte ESP header,
	*	forming ICV. Speed gain is achieved by doing both things at the same time,
	*	hence lengths are required to match at least at the cipher level.
	*
	*	Short lengths are not optimized at < 12 AES blocks
	*/

.globl	_asm_aescbc_sha256_hmac


.section	__TEXT,__const
.align	4
Lrcon:
.word	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
.word	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
.word	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
.word	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
.word	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
.word	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
.word	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
.word	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
.word	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
.word	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
.word	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
.word	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
.word	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
.word	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
.word	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
.word	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2

Linit_sha_state:
.word	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a
.word	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
.text

_asm_aescbc_sha256_hmac:
	AARCH64_VALID_CALL_TARGET

	stp	d8,d9,[sp,#-64]!

	ldr	x7, [x6, #HMAC_IKEYPAD]

	ldp	q24,q25,[x7]

	ldr	x7, [x6, #HMAC_OKEYPAD]

	stp	d10,d11,[sp,#16]


	adrp	x12,Linit_sha_state@PAGE
	add	x12,x12,Linit_sha_state@PAGEOFF
	prfm	PLDL1KEEP,[x1,0]
	lsr	x10,x2,4

	stp	d12,d13,[sp,#32]
	stp	d14,d15,[sp,#48]

	ldr	x9, [x6, #CIPHER_KEY]
	ldr	x16, [x6, #CIPHER_KEY_ROUNDS]
	ldr	x6, [x6, #CIPHER_IV]
	add	x17, x9, #160

	/*
	*	Init sha state, prefetch, check for small cases.
	*	Note that the output is prefetched as a load, for the in-place case
	*/
	prfm	PLDL1KEEP,[x0,0]
	cmp	x10,12
	b.lt	Lenc_short_cases


	ld1	{v3.16b},[x6]

	ld1	{v0.16b},[x0],16
	mov	x11,x2
	lsr	x12,x11,6
	/*
	*	now we can do the loop prolog, 1st aes sequence of 4 blocks
	*/
	ld1	{v8.16b},[x9],16
	ld1	{v9.16b},[x9],16
	eor	v0.16b,v0.16b,v3.16b
	ld1	{v10.16b},[x9],16


	aese	v0.16b,v8.16b
	aesmc	v0.16b,v0.16b
	prfm	PLDL1KEEP,[x0,64]
	ld1	{v11.16b},[x9],16
	aese	v0.16b,v9.16b
	aesmc	v0.16b,v0.16b
	prfm	PLDL1KEEP,[x1,64]

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ld1	{v12.16b},[x9],16
	aese	v0.16b,v10.16b
	aesmc	v0.16b,v0.16b

	ld1	{v1.16b},[x0],16
	ld1	{v13.16b},[x9],16
	aese	v0.16b,v11.16b
	aesmc	v0.16b,v0.16b
	ld1	{v14.16b},[x9],16
	aese	v0.16b,v12.16b
	aesmc	v0.16b,v0.16b
	ld1	{v15.16b},[x9],16
	aese	v0.16b,v13.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.16b},[x9],16
	aese	v0.16b,v14.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.16b},[x9],16
	aese	v0.16b,v15.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	cmp	x16,#12
	b.lt	Laes128_enc_prolog_0
Laes192_enc_prolog_0:
	ldp	q30,q31,[x17],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_prolog_0
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_prolog_0:
	aese	v0.16b,v31.16b
	aesmc	v0.16b,v0.16b
	ldp	q30,q31,[x17],32
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_prolog_0:
	ld1	{v18.16b},[x9]
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	eor	v1.16b,v1.16b,v0.16b


	aese	v1.16b,v8.16b
	aesmc	v1.16b,v1.16b

	ld1	{v2.16b},[x0],16
	aese	v1.16b,v9.16b
	aesmc	v1.16b,v1.16b
	prfm	PLDL1KEEP,[x8,0*64]
	aese	v1.16b,v10.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v11.16b
	aesmc	v1.16b,v1.16b

	st1	{v0.16b},[x1],16
	ld1	{v26.16b},[x3],16
	aese	v1.16b,v12.16b
	aesmc	v1.16b,v1.16b
	prfm	PLDL1KEEP,[x8,2*64]
	aese	v1.16b,v13.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v14.16b
	aesmc	v1.16b,v1.16b
	prfm	PLDL1KEEP,[x8,4*64]
	aese	v1.16b,v15.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	prfm	PLDL1KEEP,[x8,6*64]
	b.lt	Laes128_enc_prolog_1
Laes192_enc_prolog_1:
	ldp	q30,q31,[x17],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_prolog_1
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_prolog_1:
	aese	v1.16b,v31.16b
	aesmc	v1.16b,v1.16b
	ldp	q30,q31,[x17],32
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_prolog_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	prfm	PLDL1KEEP,[x8,8*64]
	eor	v2.16b,v2.16b,v1.16b


	aese	v2.16b,v8.16b
	aesmc	v2.16b,v2.16b

	ld1	{v3.16b},[x0],16
	aese	v2.16b,v9.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v10.16b
	aesmc	v2.16b,v2.16b
	prfm	PLDL1KEEP,[x8,10*64]
	aese	v2.16b,v11.16b
	aesmc	v2.16b,v2.16b

	st1	{v1.16b},[x1],16
	ld1	{v27.16b},[x3],16
	aese	v2.16b,v12.16b
	aesmc	v2.16b,v2.16b
	prfm	PLDL1KEEP,[x8,12*64]
	aese	v2.16b,v13.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v14.16b
	aesmc	v2.16b,v2.16b
	prfm	PLDL1KEEP,[x8,14*64]
	aese	v2.16b,v15.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v16.16b
	aesmc	v2.16b,v2.16b
	b.lt	Laes128_enc_prolog_2
Laes192_enc_prolog_2:
	ldp	q30,q31,[x17],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_prolog_2
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_prolog_2:
	aese	v2.16b,v31.16b
	aesmc	v2.16b,v2.16b
	ldp	q30,q31,[x17],32
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_prolog_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	eor	v3.16b,v3.16b,v2.16b


	aese	v3.16b,v8.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v9.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v10.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v11.16b
	aesmc	v3.16b,v3.16b

	st1	{v2.16b},[x1],16
	ld1	{v28.16b},[x3],16
	aese	v3.16b,v12.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v13.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v14.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v15.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v16.16b
	aesmc	v3.16b,v3.16b
	sub	x15,x12,1
	and	x13,x10,3
	b.lt	Laes128_enc_prolog_3
Laes192_enc_prolog_3:
	ldp	q30,q31,[x17],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_prolog_3
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_prolog_3:
	aese	v3.16b,v31.16b
	aesmc	v3.16b,v3.16b
	ldp	q30,q31,[x17],32
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_prolog_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	/*
	*	Note, aes_blocks_left := number after the main (sha)
	*	block is done. Can be 0
	*/

	st1	{v3.16b},[x1],16
	ld1	{v29.16b},[x3],16


	sub	x12,x5,x2

	sub	x5,x5,64

	/*
	*	main combined loop CBC
	*/
Lenc_main_loop:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	/*
	*	Because both mov, rev32 and eor have a busy cycle,this takes longer
	*	than it looks. That's OK since there are 6 cycles before we can use
	*	the load anyway; so this goes as fast as it can without SW
	*	pipelining(too complicated given the code size)
	*/
	rev32	v26.16b,v26.16b

	ld1	{v0.16b},[x0],16
	mov	v22.16b,v24.16b
	prfm	PLDL1KEEP,[x9,64]
	rev32	v27.16b,v27.16b

	prfm	PLDL1KEEP,[x1,64]
	mov	v23.16b,v25.16b
	ld1	{v4.16b},[x8],16
	eor	v0.16b,v0.16b,v3.16b
	ld1	{v5.16b},[x8],16
	/*
	*	aes xform 0, sha quad 0
	*/
	aese	v0.16b,v8.16b
	aesmc	v0.16b,v0.16b
	ld1	{v6.16b},[x8],16
	rev32	v28.16b,v28.16b
	ld1	{v7.16b},[x8],16

	ld1	{v1.16b},[x0],16
	aese	v0.16b,v9.16b
	aesmc	v0.16b,v0.16b
	add	v4.4s,v4.4s,v26.4s
	sha256su0	v26.4s,v27.4s
	aese	v0.16b,v10.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	aese	v0.16b,v11.16b
	aesmc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s

	rev32	v29.16b,v29.16b
	sha256h2	q23, q21, v4.4s
	aese	v0.16b,v12.16b
	aesmc	v0.16b,v0.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	mov	v21.16b, v22.16b
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aese	v0.16b,v13.16b
	aesmc	v0.16b,v0.16b
	sha256h	q22, q23, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v5.4s
	aese	v0.16b,v14.16b
	aesmc	v0.16b,v0.16b
	ld1	{v5.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	sha256su0	v28.4s,v29.4s
	aese	v0.16b,v15.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256su0	v29.4s,v26.4s
	sha256h	q22, q23, v7.4s
	cmp	x16,#12
	b.lt	Laes128_enc_mainloop_0
Laes192_enc_mainloop_0:
	ldp	q30,q31,[x17],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_mainloop_0
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_mainloop_0:
	aese	v0.16b,v31.16b
	aesmc	v0.16b,v0.16b
	ldp	q30,q31,[x17],32
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_mainloop_0:
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s


	sha256su0	v26.4s,v27.4s
	eor	v1.16b,v1.16b,v0.16b
	ld1	{v7.16b},[x8],16
	mov	v21.16b, v22.16b

	st1	{v0.16b},[x1],16
	aese	v1.16b,v8.16b
	aesmc	v1.16b,v1.16b
	sha256h	q22, q23, v4.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v4.4s
	sha256su1	v26.4s,v28.4s,v29.4s
	aese	v1.16b,v9.16b
	aesmc	v1.16b,v1.16b
	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v1.16b,v10.16b
	aesmc	v1.16b,v1.16b

	ld1	{v2.16b},[x0],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	ld1	{v4.16b},[x8],16
	aese	v1.16b,v11.16b
	aesmc	v1.16b,v1.16b
	ld1	{v5.16b},[x8],16
	mov	v21.16b, v22.16b
	sha256su0	v28.4s,v29.4s
	sha256h	q22, q23, v6.4s
	aese	v1.16b,v12.16b
	aesmc	v1.16b,v1.16b
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aese	v1.16b,v13.16b
	aesmc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aese	v1.16b,v14.16b
	aesmc	v1.16b,v1.16b
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	aese	v1.16b,v15.16b
	aesmc	v1.16b,v1.16b
	add	v5.4s,v5.4s,v27.4s
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	add	v6.4s,v6.4s,v28.4s
	b.lt	Laes128_enc_mainloop_1
Laes192_enc_mainloop_1:
	ldp	q30,q31,[x17],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_mainloop_1
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_mainloop_1:
	aese	v1.16b,v31.16b
	aesmc	v1.16b,v1.16b
	ldp	q30,q31,[x17],32
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_mainloop_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	add	v7.4s,v7.4s,v29.4s


	eor	v2.16b,v2.16b,v1.16b


	sha256su0	v26.4s,v27.4s
	aese	v2.16b,v8.16b
	aesmc	v2.16b,v2.16b

	st1	{v1.16b},[x1],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aese	v2.16b,v9.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aese	v2.16b,v10.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v2.16b,v11.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	ld1	{v5.16b},[x8],16
	sha256su0	v28.4s,v29.4s
	aese	v2.16b,v12.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v2.16b,v13.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su0	v29.4s,v26.4s

	ld1	{v3.16b},[x0],16
	aese	v2.16b,v14.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aese	v2.16b,v15.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v29.4s,v27.4s,v28.4s
	add	v5.4s,v5.4s,v27.4s
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	aese	v2.16b,v16.16b
	aesmc	v2.16b,v2.16b
	b.lt	Laes128_enc_mainloop_2
Laes192_enc_mainloop_2:
	ldp	q30,q31,[x17],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_mainloop_2
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_mainloop_2:
	aese	v2.16b,v31.16b
	aesmc	v2.16b,v2.16b
	ldp	q30,q31,[x17],32
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_mainloop_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	add	v6.4s,v6.4s,v28.4s
	add	v7.4s,v7.4s,v29.4s


	eor	v3.16b,v3.16b,v2.16b


	aese	v3.16b,v8.16b
	aesmc	v3.16b,v3.16b

	st1	{v2.16b},[x1],16
	aese	v3.16b,v9.16b
	aesmc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aese	v3.16b,v10.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v11.16b
	aesmc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v3.16b,v12.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v13.16b
	aesmc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v3.16b,v14.16b
	aesmc	v3.16b,v3.16b
	sub	x15,x15,1
	aese	v3.16b,v15.16b
	aesmc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aese	v3.16b,v16.16b
	aesmc	v3.16b,v3.16b
	b.lt	Laes128_enc_mainloop_3
Laes192_enc_mainloop_3:
	ldp	q30,q31,[x17],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_mainloop_3
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_mainloop_3:
	aese	v3.16b,v31.16b
	aesmc	v3.16b,v3.16b
	ldp	q30,q31,[x17],32
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_mainloop_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s

	st1	{v3.16b},[x1],16

	ldp	q26,q27,[x3],32
	ldp	q28,q29,[x3],32
	sub	x5,x5,64

	cbnz	x15,Lenc_main_loop

	mov	w15,0x80
	/*
	*	epilog, process remaining aes blocks and b-2 sha block
	*	do this inline (no loop) to overlap with the sha part
	*	note there are 0-3 aes blocks left.
	*/
	rev32	v26.16b,v26.16b
	rev32	v27.16b,v27.16b
	rev32	v28.16b,v28.16b
	rev32	v29.16b,v29.16b
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	cbz	x13, Lbm2fromQ0

	/*
	*	mode op 0
	*	read next aes block, update aes_ptr_in
	*/
	ld1	{v0.16b},[x0],16

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	eor	v0.16b,v0.16b,v3.16b


	add	v4.4s,v4.4s,v26.4s
	aese	v0.16b,v8.16b
	aesmc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s
	sha256su0	v26.4s,v27.4s
	aese	v0.16b,v9.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aese	v0.16b,v10.16b
	aesmc	v0.16b,v0.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su0	v27.4s,v28.4s
	aese	v0.16b,v11.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v0.16b,v12.16b
	aesmc	v0.16b,v0.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	aese	v0.16b,v13.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v0.16b,v14.16b
	aesmc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aese	v0.16b,v15.16b
	aesmc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	cmp	x16,#12
	b.lt	Laes128_enc_epilog_0
Laes192_enc_epilog_0:
	ldp	q30,q31,[x17],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_epilog_0
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_epilog_0:
	aese	v0.16b,v31.16b
	aesmc	v0.16b,v0.16b
	ldp	q30,q31,[x17],32
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_epilog_0:
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	subs	x14,x13,1

	st1	{v0.16b},[x1],16

	beq	Lbm2fromQ1
	/*
	*	mode op 1
	*	read next aes block, update aes_ptr_in
	*/
	ld1	{v1.16b},[x0],16
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16

	eor	v1.16b,v1.16b,v0.16b


	add	v4.4s,v4.4s,v26.4s
	aese	v1.16b,v8.16b
	aesmc	v1.16b,v1.16b
	add	v5.4s,v5.4s,v27.4s
	sha256su0	v26.4s,v27.4s
	aese	v1.16b,v9.16b
	aesmc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aese	v1.16b,v10.16b
	aesmc	v1.16b,v1.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su0	v27.4s,v28.4s
	aese	v1.16b,v11.16b
	aesmc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v1.16b,v12.16b
	aesmc	v1.16b,v1.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	aese	v1.16b,v13.16b
	aesmc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v1.16b,v14.16b
	aesmc	v1.16b,v1.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aese	v1.16b,v15.16b
	aesmc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	sha256su1	v29.4s,v27.4s,v28.4s
	cmp	x16,#12
	b.lt	Laes128_enc_epilog_1
Laes192_enc_epilog_1:
	ldp	q30,q31,[x17],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_epilog_1
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_epilog_1:
	aese	v1.16b,v31.16b
	aesmc	v1.16b,v1.16b
	ldp	q30,q31,[x17],32
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_epilog_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	subs	x14,x14,1

	st1	{v1.16b},[x1],16

	beq	Lbm2fromQ2
	/*
	*	mode op 2
	*	read next aes block, update aes_ptr_in
	*/
	ld1	{v2.16b},[x0],16
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	eor	v2.16b,v2.16b,v1.16b


	add	v4.4s,v4.4s,v26.4s
	aese	v2.16b,v8.16b
	aesmc	v2.16b,v2.16b
	add	v5.4s,v5.4s,v27.4s
	sha256su0	v26.4s,v27.4s
	aese	v2.16b,v9.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aese	v2.16b,v10.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su0	v27.4s,v28.4s
	aese	v2.16b,v11.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aese	v2.16b,v12.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	aese	v2.16b,v13.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aese	v2.16b,v14.16b
	aesmc	v2.16b,v2.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aese	v2.16b,v15.16b
	aesmc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	aese	v2.16b,v16.16b
	aesmc	v2.16b,v2.16b
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	cmp	x16,#12
	b.lt	Laes128_enc_epilog_2
Laes192_enc_epilog_2:
	ldp	q30,q31,[x17],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_epilog_2
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_epilog_2:
	aese	v2.16b,v31.16b
	aesmc	v2.16b,v2.16b
	ldp	q30,q31,[x17],32
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_epilog_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:

	st1	{v2.16b},[x1],16

	b	Lbm2fromQ3
	/*
	*	Now there is the b-2 sha block before the final one.  Execution takes over
	*	in the appropriate part of this depending on how many aes blocks were left.
	*	If there were none, the whole thing is executed.
	*/

Lbm2fromQ0:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lbm2fromQ1:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lbm2fromQ2:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lbm2fromQ3:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s
	add	v5.4s,v5.4s,v27.4s
	add	v6.4s,v6.4s,v28.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	eor	v26.16b,v26.16b,v26.16b
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	eor	v27.16b,v27.16b,v27.16b
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	eor	v28.16b,v28.16b,v28.16b
	sha256h2	q23, q21, v7.4s

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s


	eor	v29.16b,v29.16b,v29.16b

	cbz	x13,Lpost_long_Q0


	ld1	{v26.16b},[x3],16
	sub	x5,x5,16
	rev32	v26.16b,v26.16b
	subs	x14,x13,1
	b.eq	Lpost_long_Q1


	ld1	{v27.16b},[x3],16
	sub	x5,x5,16
	rev32	v27.16b,v27.16b
	subs	x14,x14,1
	b.eq	Lpost_long_Q2


	ld1	{v28.16b},[x3],16
	sub	x5,x5,16
	rev32	v28.16b,v28.16b

	b	Lpost_long_Q3
	/*
	*	Process remaining 8B blocks of the digest
	*/
Lpost_long_Q0:


	mov	v26.b[3],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v26.d[0],x2

	mov	v26.b[11],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v26.d[1],x2

Lpost_long_Q1:


	mov	v27.b[3],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v27.d[0],x2

	mov	v27.b[11],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v27.d[1],x2

Lpost_long_Q2:


	mov	v28.b[3],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v28.d[0],x2

	mov	v28.b[11],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v28.d[1],x2

Lpost_long_Q3:


	mov	v29.b[3],w15

	cbz	x5,Lpost_long_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v29.d[0],x2

	mov	v29.b[11],w15
	/*
	*	Outstanding 8B blocks left.
	*	Since there has to be another sha block with padding,
	*	we need to calculate hash without padding here.
	*/
	cbz	x5,1f

	ldr	x2,[x3],8
	rev32	x2,x2
	/*
	*	Don't decrease x5 here.
	*	Use it to indicate necessity of constructing "1" padding at the end.
	*/
	mov	v29.d[1],x2

	/*
	*	That is enough of blocks, we allow up to 64 bytes in total.
	*	Now we have the sha256 to do for these 4 16B blocks
	*/
1:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	mov	v21.16b, v22.16b
	add	v7.4s,v7.4s,v29.4s

	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	eor	v26.16b,v26.16b,v26.16b
	add	v24.4s,v24.4s,v22.4s
	eor	v27.16b,v27.16b,v27.16b
	add	v25.4s,v25.4s,v23.4s
	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b


	cbz	x5,Lpost_long_loop
	subs	x5,x5,8

	b.ne	Lpost_long_Q0

	mov	v26.b[3],w15

Lpost_long_loop:

	add	x11,x11,x12

	add	x11,x11, #64
	lsr	x12,x11,32
	and	x13,x11,0xffffffff
	lsl	x12,x12,3
	lsl	x13,x13,3

	mov	v29.s[3],w13
	mov	v29.s[2],w12

	/*
	*	do last sha of pad block
	*/

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF


	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	ldp	q6,q7,[x8],32
	add	v5.4s,v5.4s,v27.4s
	mov	v21.16b, v22.16b
	add	v6.4s,v6.4s,v28.4s

	sha256h	q22, q23, v4.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	add	v26.4s,v24.4s,v22.4s
	add	v27.4s,v25.4s,v23.4s


	eor	v28.16b, v28.16b, v28.16b
	eor	v29.16b, v29.16b, v29.16b

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ldp	q24,q25,[x7]


	mov	w11, #0x80
	mov	v28.b[3], w11

	mov	x11, #64+32
	lsl	x11, x11, 3

	mov	v29.s[3], w11
	ldp	q4,q5,[x8],32
	lsr	x11, x11, 32
	mov	v29.s[2], w11

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	ldp	q6,q7,[x8],32
	add	v5.4s,v5.4s,v27.4s
	mov	v21.16b, v22.16b
	add	v6.4s,v6.4s,v28.4s

	sha256h	q22, q23, v4.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	ldp	d10,d11,[sp,#16]
	ldp	d12,d13,[sp,#32]

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	ldp	d14,d15,[sp,#48]
	ldp	d8,d9,[sp],#64

	mov	x0, xzr

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s

	rev32	v24.16b, v24.16b
	rev32	v25.16b, v25.16b

	stp	q24,q25,[x4]

	ret

	/*
	*	These are the short cases (less efficient), here used for 1-11 aes blocks.
	*	x10 = aes_blocks
	*/
Lenc_short_cases:
	ld1	{v3.16b},[x6]
	ldp	q8,q9,[x9],32
	eor	v26.16b,v26.16b,v26.16b
	ldp	q10,q11,[x9],32
	eor	v27.16b,v27.16b,v27.16b
	ldp	q12,q13,[x9],32
	eor	v28.16b,v28.16b,v28.16b
	ldp	q14,q15,[x9],32
	eor	v29.16b,v29.16b,v29.16b
	ldp	q16,q17,[x9],32
	mov	w15,0x80
	lsl	x11,x10,4
	ld1	{v18.16b},[x9]


	sub	x12,x5,x2
	/*
	*	the idea in the short loop (at least 1) is to break out with the padding
	*	already in place excepting the final word.
	*/
Lenc_short_loop:
	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ld1	{v0.16b},[x0],16
	eor	v0.16b,v0.16b,v3.16b


	aese	v0.16b,v8.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v9.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v10.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v11.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v12.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v13.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v14.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v15.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	cmp	x16,#12
	b.lt	Laes128_enc_short_0
Laes192_enc_short_0:
	ldp	q30,q31,[x17],32
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	b.gt	Laes256_enc_short_0
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_short_0:
	aese	v0.16b,v31.16b
	aesmc	v0.16b,v0.16b
	ldp	q30,q31,[x17],32
	aese	v0.16b,v30.16b
	aesmc	v0.16b,v0.16b
	ld1	{v30.16b},[x17]
	aese	v0.16b,v31.16b
	eor	v0.16b,v0.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_short_0:
	ld1	{v18.16b},[x9]
	aese	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:

	st1	{v0.16b},[x1],16

	ld1	{v26.16b},[x3],16

	sub	x5,x5,16
	rev32	v26.16b,v26.16b
	sub	x10,x10,1
	cbz	x10,Lpost_short_Q1

	ld1	{v1.16b},[x0],16
	eor	v1.16b,v1.16b,v0.16b


	aese	v1.16b,v8.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v9.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v10.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v11.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v12.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v13.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v14.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v15.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	cmp	x16,#12
	b.lt	Laes128_enc_short_1
Laes192_enc_short_1:
	ldp	q30,q31,[x17],32
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	b.gt	Laes256_enc_short_1
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_short_1:
	aese	v1.16b,v31.16b
	aesmc	v1.16b,v1.16b
	ldp	q30,q31,[x17],32
	aese	v1.16b,v30.16b
	aesmc	v1.16b,v1.16b
	ld1	{v30.16b},[x17]
	aese	v1.16b,v31.16b
	eor	v1.16b,v1.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_short_1:
	aese	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:

	st1	{v1.16b},[x1],16

	ld1	{v27.16b},[x3],16

	sub	x5,x5,16
	rev32	v27.16b,v27.16b
	sub	x10,x10,1
	cbz	x10,Lpost_short_Q2

	ld1	{v2.16b},[x0],16
	eor	v2.16b,v2.16b,v1.16b


	aese	v2.16b,v8.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v9.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v10.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v11.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v12.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v13.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v14.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v15.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v16.16b
	aesmc	v2.16b,v2.16b
	cmp	x16,#12
	b.lt	Laes128_enc_short_2
Laes192_enc_short_2:
	ldp	q30,q31,[x17],32
	aese	v2.16b,v17.16b
	aesmc	v2.16b,v2.16b
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	b.gt	Laes256_enc_short_2
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_short_2:
	aese	v2.16b,v31.16b
	aesmc	v2.16b,v2.16b
	ldp	q30,q31,[x17],32
	aese	v2.16b,v30.16b
	aesmc	v2.16b,v2.16b
	ld1	{v30.16b},[x17]
	aese	v2.16b,v31.16b
	eor	v2.16b,v2.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_short_2:
	aese	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:

	st1	{v2.16b},[x1],16

	ld1	{v28.16b},[x3],16

	sub	x5,x5,16
	rev32	v28.16b,v28.16b
	sub	x10,x10,1
	cbz	x10,Lpost_short_Q3

	ld1	{v3.16b},[x0],16
	eor	v3.16b,v3.16b,v2.16b


	aese	v3.16b,v8.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v9.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v10.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v11.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v12.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v13.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v14.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v15.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v16.16b
	aesmc	v3.16b,v3.16b
	cmp	x16,#12
	b.lt	Laes128_enc_short_3
Laes192_enc_short_3:
	ldp	q30,q31,[x17],32
	aese	v3.16b,v17.16b
	aesmc	v3.16b,v3.16b
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	b.gt	Laes256_enc_short_3
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #32
	b	1f
Laes256_enc_short_3:
	aese	v3.16b,v31.16b
	aesmc	v3.16b,v3.16b
	ldp	q30,q31,[x17],32
	aese	v3.16b,v30.16b
	aesmc	v3.16b,v3.16b
	ld1	{v30.16b},[x17]
	aese	v3.16b,v31.16b
	eor	v3.16b,v3.16b,v30.16b
	sub	x17, x17, #64
	b	1f
Laes128_enc_short_3:
	aese	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:

	st1	{v3.16b},[x1],16

	ld1	{v29.16b},[x3],16

	sub	x5,x5,16

	rev32	v29.16b,v29.16b
	/*
	*	now we have the sha256 to do for these 4 aes blocks
	*/


	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	mov	v21.16b, v22.16b
	add	v7.4s,v7.4s,v29.4s

	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	eor	v26.16b,v26.16b,v26.16b
	add	v24.4s,v24.4s,v22.4s
	eor	v27.16b,v27.16b,v27.16b
	add	v25.4s,v25.4s,v23.4s
	eor	v28.16b,v28.16b,v28.16b
	sub	x10,x10,1
	eor	v29.16b,v29.16b,v29.16b

	cbnz	x10,Lenc_short_loop

Lpost_short_Q0:

	mov	v26.b[3],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v26.d[0],x2

	mov	v26.b[11],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v26.d[1],x2
Lpost_short_Q1:

	eor	v27.16b,v27.16b,v27.16b
	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b

	mov	v27.b[3],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v27.d[0],x2

	mov	v27.b[11],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v27.d[1],x2
Lpost_short_Q2:

	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b

	mov	v28.b[3],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v28.d[0],x2

	mov	v28.b[11],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2
	mov	v28.d[1],x2
Lpost_short_Q3:

	eor	v29.16b,v29.16b,v29.16b

	mov	v29.b[3],w15

	cbz	x5,Lpost_short_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v29.d[0],x2

	mov	v29.b[11],w15

	cbz	x5,1f

	ldr	x2,[x3],8
	rev32	x2,x2
	mov	v29.d[1],x2

	/*
	*	That is enough of blocks, we allow up to 64 bytes in total.
	*	Now we have the sha256 to do for these 4 16B blocks
	*/
1:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	mov	v21.16b, v22.16b
	add	v7.4s,v7.4s,v29.4s

	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	eor	v26.16b,v26.16b,v26.16b
	add	v24.4s,v24.4s,v22.4s
	eor	v27.16b,v27.16b,v27.16b
	add	v25.4s,v25.4s,v23.4s
	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b


	cbz	x5,Lpost_short_loop
	subs	x5,x5,8

	b.ne	Lpost_short_Q0

	mov	v26.b[3],w15

	/*
	*	there are between 0 and 3 aes blocks in the final sha256 blocks
	*/
Lpost_short_loop:

	add	x11,x11,x12

	add	x11,x11, #64
	lsr	x12,x11,32
	and	x13,x11,0xffffffff
	lsl	x12,x12,3
	lsl	x13,x13,3

	mov	v29.s[3],w13
	mov	v29.s[2],w12




	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF


	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	ldp	q6,q7,[x8],32
	add	v5.4s,v5.4s,v27.4s
	mov	v21.16b, v22.16b
	add	v6.4s,v6.4s,v28.4s

	sha256h	q22, q23, v4.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	add	v26.4s,v24.4s,v22.4s
	add	v27.4s,v25.4s,v23.4s


	eor	v28.16b, v28.16b, v28.16b
	eor	v29.16b, v29.16b, v29.16b

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ldp	q24,q25,[x7]


	mov	w11, #0x80
	mov	v28.b[3], w11

	mov	x11, #64+32
	lsl	x11, x11, 3

	mov	v29.s[3], w11
	ldp	q4,q5,[x8],32
	lsr	x11, x11, 32
	mov	v29.s[2], w11

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v4.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	ldp	q6,q7,[x8],32
	add	v5.4s,v5.4s,v27.4s
	mov	v21.16b, v22.16b
	add	v6.4s,v6.4s,v28.4s

	sha256h	q22, q23, v4.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	ldp	d10,d11,[sp,#16]
	ldp	d12,d13,[sp,#32]

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	ldp	d14,d15,[sp,#48]
	ldp	d8,d9,[sp],#64

	mov	x0, xzr

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s

	rev32	v24.16b, v24.16b
	rev32	v25.16b, v25.16b

	stp	q24,q25,[x4]

	ret



	/*
	*	Description:
	*
	*	Combined Auth/Dec Primitive = sha256_hmac/aes128cbc
	*
	*	Operations:
	*
	*	out = decrypt-AES128CBC(in)
	*	return_ash_ptr = SHA256(o_key_pad | SHA256(i_key_pad | in))
	*
	*	Prototype:
	*
	*	void asm_sha256_hmac_aescbc_dec(uint8_t *csrc, uint8_t *cdst, uint64_t clen,
	*	uint8_t *dsrc, uint8_t *ddst, uint64_t dlen,
	*	CIPH_DIGEST *arg)
	*
	*	Registers used:
	*
	*	asm_sha256_hmac_aescbc_dec(
	*	csrc,	x0	(cipher src address)
	*	cdst,	x1	(cipher dst address)
	*	clen	x2	(cipher length)
	*	dsrc,	x3	(digest src address)
	*	ddst,	x4	(digest dst address)
	*	dlen,	x5	(digest length)
	*	arg	x6:
	*	arg->cipher.key			(round keys)
	*	arg->cipher.key_rounds		(key rounds)
	*	arg->cipher.iv			(initialization vector)
	*	arg->digest.hmac.i_key_pad	(partially hashed i_key_pad)
	*	arg->digest.hmac.o_key_pad	(partially hashed o_key_pad)
	*	)
	*
	*	Routine register definitions:
	*
	*	v0 - v3 -- aes results
	*	v4 - v7 -- round consts for sha
	*	v8 - v18 -- round keys
	*	v19 - v20 -- round keys
	*	v21 -- ABCD tmp
	*	v22 -- sha working state ABCD (q22)
	*	v23 -- sha working state EFGH (q23)
	*	v24 -- sha state ABCD
	*	v25 -- sha state EFGH
	*	v26 -- sha block 0
	*	v27 -- sha block 1
	*	v28 -- sha block 2
	*	v29 -- sha block 3
	*	v30 -- reserved
	*	v31 -- reserved
	*
	*
	*	Constraints:
	*
	*	The variable "clen" must be a multiple of 16, otherwise results are not
	*	defined For AES partial blocks the user is required to pad the input to
	*	modulus 16 = 0.
	*
	*	The variable "dlen" must be a multiple of 8 and greater or equal to "clen".
	*	The maximum difference between "dlen" and "clen" cannot exceed 64 bytes.
	*	This constrain is strictly related to the needs of the IPSec ESP packet.
	*	Short lengths are less optimized at < 16 AES blocks, however they are
	*	somewhat optimized, and more so than the enc/auth versions.
	*/

.globl	_asm_sha256_hmac_aescbc_dec

.align	4

_asm_sha256_hmac_aescbc_dec:
	AARCH64_VALID_CALL_TARGET

	stp	d8,d9,[sp, #-80]!


	ldr	x7, [x6, #HMAC_IKEYPAD]

	ldp	q24,q25,[x7]

	ldr	x7, [x6, #HMAC_OKEYPAD]

	stp	d10,d11,[sp,#16]

	prfm	PLDL1KEEP,[x0,0]
	stp	d12,d13,[sp,#32]
	prfm	PLDL1KEEP,[x1,0]
	lsr	x10,x2,4
	stp	d14,d15,[sp,#48]

	adrp	x12,Linit_sha_state@PAGE
	add	x12,x12,Linit_sha_state@PAGEOFF
	stp	x19,x20,[sp,#64]

	ldr	x9, [x6, #CIPHER_KEY]
	ldr	x16, [x6, #CIPHER_KEY_ROUNDS]
	ldr	x6, [x6, #CIPHER_IV]
	add	x17, x9, #160
	/*
	*	Init sha state, prefetch, check for small cases.
	*	Note that the output is prefetched as a load, for the in-place case.
	*/
	cmp	x10,16
	blt	Ldec_short_cases


	sub	x20,x5,x2

	mov	x11,x2
	ld1	{v30.16b},[x6]
	lsr	x12,x11,6

	ldp	q26,q27,[x3],32
	rev32	v26.16b,v26.16b
	rev32	v27.16b,v27.16b
	ldp	q28,q29,[x3],32
	rev32	v28.16b,v28.16b
	rev32	v29.16b,v29.16b


	sub	x5,x5,64
	/*
	*	now we can do the loop prolog, 1st sha256 block
	*/
	prfm	PLDL1KEEP,[x0,64]
	prfm	PLDL1KEEP,[x1,64]

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	/*
	*	do the first sha256 block on the plaintext
	*/
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b


	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s
	add	v5.4s,v5.4s,v27.4s
	add	v6.4s,v6.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v8.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	ld1	{v4.16b},[x8],16
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v9.16b},[x9],16

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v10.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	ld1	{v5.16b},[x8],16
	sha256su1	v27.4s,v29.4s,v26.4s

	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v4.4s,v4.4s,v26.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s
	ld1	{v11.16b},[x9],16

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v7.16b},[x8],16
	sha256su1	v29.4s,v27.4s,v28.4s


	add	v6.4s,v6.4s,v28.4s
	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v12.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	ld1	{v4.16b},[x8],16
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v13.16b},[x9],16

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v14.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	ld1	{v5.16b},[x8],16
	sha256su1	v27.4s,v29.4s,v26.4s

	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v4.4s,v4.4s,v26.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s
	ld1	{v15.16b},[x9],16

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v7.16b},[x8],16
	sha256su1	v29.4s,v27.4s,v28.4s


	add	v6.4s,v6.4s,v28.4s
	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v16.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	ld1	{v4.16b},[x8],16
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v17.16b},[x9],16

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v18.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	ld1	{v5.16b},[x8],16
	sha256su1	v27.4s,v29.4s,v26.4s

	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v4.4s,v4.4s,v26.4s
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v7.16b},[x8],16
	sha256su1	v29.4s,v27.4s,v28.4s


	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v4.4s
	ld1	{v26.16b},[x3],16
	ld1	{v27.16b},[x3],16

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v5.4s
	ld1	{v28.16b},[x3],16
	ld1	{v29.16b},[x3],16

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s


	sub	x5,x5,64

	/*
	*	aes_blocks_left := number after the main (sha) block is done.
	*	can be 0 note we account for the extra unwind in main_blocks
	*/
	sub	x15,x12,2

	add	v24.4s,v24.4s,v22.4s
	and	x13,x10,3
	ld1	{v0.16b},[x0]
	add	v25.4s,v25.4s,v23.4s
	add	x9,x0,128

	ld1	{v31.16b},[x0],16


	mov	x19,xzr
	/*
	*	main combined loop CBC, can be used by auth/enc version
	*/
Ldec_main_loop:
	/*
	*	Because both mov, rev32 and eor have a busy cycle, this takes longer
	*	than it looks.
	*/
	rev32	v26.16b,v26.16b
	mov	v22.16b,v24.16b
	prfm	PLDL1KEEP,[x9,64]
	rev32	v27.16b,v27.16b

	prfm	PLDL1KEEP,[x1,64]
	mov	v23.16b,v25.16b

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	/*
	*	aes xform 0, sha quad 0
	*/
	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	ld1	{v4.16b},[x8],16
	rev32	v28.16b,v28.16b

	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	add	v4.4s,v4.4s,v26.4s
	ld1	{v5.16b},[x8],16
	sha256su0	v26.4s,v27.4s
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	ld1	{v7.16b},[x8],16
	rev32	v29.16b,v29.16b

	ld1	{v1.16b},[x0]
	sha256h2	q23, q21, v4.4s
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	mov	v21.16b, v22.16b
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	sha256h	q22, q23, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v5.4s
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	ld1	{v5.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	sha256su0	v28.4s,v29.4s
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256su0	v29.4s,v26.4s
	sha256h	q22, q23, v7.4s
	cmp	x16,#12
	b.lt	Laes128_dec_mainloop_0
Laes192_dec_mainloop_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_mainloop_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_mainloop_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_mainloop_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	ld1	{v6.16b},[x8],16
	eor	v0.16b,v0.16b,v30.16b

	ld1	{v30.16b},[x0],16
	add	v5.4s,v5.4s,v27.4s


	sha256su0	v26.4s,v27.4s
	ld1	{v7.16b},[x8],16
	mov	v21.16b, v22.16b

	st1	{v0.16b},[x1],16
	aesd	v1.16b,v8.16b
	aesimc	v1.16b,v1.16b
	sha256h	q22, q23, v4.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v4.4s
	sha256su1	v26.4s,v28.4s,v29.4s
	aesd	v1.16b,v9.16b
	aesimc	v1.16b,v1.16b
	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v1.16b,v10.16b
	aesimc	v1.16b,v1.16b

	ld1	{v2.16b},[x0]
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	ld1	{v4.16b},[x8],16
	aesd	v1.16b,v11.16b
	aesimc	v1.16b,v1.16b
	ld1	{v5.16b},[x8],16
	mov	v21.16b, v22.16b
	sha256su0	v28.4s,v29.4s
	sha256h	q22, q23, v6.4s
	aesd	v1.16b,v12.16b
	aesimc	v1.16b,v1.16b
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aesd	v1.16b,v13.16b
	aesimc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v1.16b,v14.16b
	aesimc	v1.16b,v1.16b
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	aesd	v1.16b,v15.16b
	aesimc	v1.16b,v1.16b
	add	v5.4s,v5.4s,v27.4s
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	add	v6.4s,v6.4s,v28.4s
	cmp	x16,#12
	b.lt	Laes128_dec_mainloop_1
Laes192_dec_mainloop_1:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	b.gt	Laes256_dec_mainloop_1
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_mainloop_1:
	aesd	v1.16b,v23.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_mainloop_1:
	aesd	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	add	v7.4s,v7.4s,v29.4s
	eor	v1.16b,v1.16b,v31.16b

	ld1	{v31.16b},[x0],16


	sha256su0	v26.4s,v27.4s
	aesd	v2.16b,v8.16b
	aesimc	v2.16b,v2.16b

	st1	{v1.16b},[x1],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aesd	v2.16b,v9.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aesd	v2.16b,v10.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v2.16b,v11.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	ld1	{v5.16b},[x8],16
	sha256su0	v28.4s,v29.4s
	aesd	v2.16b,v12.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v2.16b,v13.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su0	v29.4s,v26.4s

	ld1	{v3.16b},[x0]
	aesd	v2.16b,v14.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v2.16b,v15.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v29.4s,v27.4s,v28.4s
	add	v5.4s,v5.4s,v27.4s
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	aesd	v2.16b,v16.16b
	aesimc	v2.16b,v2.16b
	cmp	x16,#12
	b.lt	Laes128_dec_mainloop_2
Laes192_dec_mainloop_2:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v17.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	b.gt	Laes256_dec_mainloop_2
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_mainloop_2:
	aesd	v2.16b,v23.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_mainloop_2:
	aesd	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	add	v6.4s,v6.4s,v28.4s
	add	v7.4s,v7.4s,v29.4s
	eor	v2.16b,v2.16b,v30.16b

	ld1	{v30.16b},[x0],16


	aesd	v3.16b,v8.16b
	aesimc	v3.16b,v3.16b

	st1	{v2.16b},[x1],16
	aesd	v3.16b,v9.16b
	aesimc	v3.16b,v3.16b
	ld1	{v26.16b},[x3],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aesd	v3.16b,v10.16b
	aesimc	v3.16b,v3.16b
	ld1	{v27.16b},[x3],16
	aesd	v3.16b,v11.16b
	aesimc	v3.16b,v3.16b
	ld1	{v28.16b},[x3],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v3.16b,v12.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v13.16b
	aesimc	v3.16b,v3.16b
	ld1	{v29.16b},[x3],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v3.16b,v14.16b
	aesimc	v3.16b,v3.16b
	sub	x15,x15,1
	aesd	v3.16b,v15.16b
	aesimc	v3.16b,v3.16b
	ld1	{v0.16b},[x0]
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v3.16b,v16.16b
	aesimc	v3.16b,v3.16b
	cmp	x16,#12
	b.lt	Laes128_dec_mainloop_3
Laes192_dec_mainloop_3:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v17.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	b.gt	Laes256_dec_mainloop_3
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_mainloop_3:
	aesd	v3.16b,v23.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_mainloop_3:
	aesd	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	add	v24.4s,v24.4s,v22.4s
	eor	v3.16b,v3.16b,v31.16b

	ld1	{v31.16b},[x0],16
	add	v25.4s,v25.4s,v23.4s

	st1	{v3.16b},[x1],16

	sub	x5,x5,64
	cbnz	x15,Ldec_main_loop
	/*
	*	Now the loop epilog. Since the reads for sha have already been done
	*	in advance, we have to have an extra unwind.
	*	This is why the test for the short cases is 16 and not 12.
	*
	*	The unwind, which is just the main loop without the tests or final reads.
	*/

	rev32	v26.16b,v26.16b
	mov	v22.16b,v24.16b
	rev32	v27.16b,v27.16b

	prfm	PLDL1KEEP,[x1,64]
	mov	v23.16b,v25.16b

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	/*
	*	aes xform 0, sha quad 0
	*/
	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	ld1	{v6.16b},[x8],16
	rev32	v28.16b,v28.16b
	ld1	{v7.16b},[x8],16

	ld1	{v1.16b},[x0]
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	add	v4.4s,v4.4s,v26.4s
	sha256su0	v26.4s,v27.4s
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s
	rev32	v29.16b,v29.16b
	sha256h2	q23, q21, v4.4s
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	mov	v21.16b, v22.16b
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	sha256h	q22, q23, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v5.4s
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	ld1	{v5.16b},[x8],16
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	sha256su0	v28.4s,v29.4s
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256su0	v29.4s,v26.4s
	sha256h	q22, q23, v7.4s
	cmp	x16,#12
	b.lt	Laes128_dec_epilog_0
Laes192_dec_epilog_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_epilog_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_epilog_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_epilog_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	ld1	{v6.16b},[x8],16
	eor	v0.16b,v0.16b,v30.16b

	ld1	{v30.16b},[x0],16
	add	v5.4s,v5.4s,v27.4s


	sha256su0	v26.4s,v27.4s
	ld1	{v7.16b},[x8],16
	mov	v21.16b, v22.16b

	st1	{v0.16b},[x1],16
	aesd	v1.16b,v8.16b
	aesimc	v1.16b,v1.16b
	sha256h	q22, q23, v4.4s
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v4.4s
	sha256su1	v26.4s,v28.4s,v29.4s
	aesd	v1.16b,v9.16b
	aesimc	v1.16b,v1.16b
	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v1.16b,v10.16b
	aesimc	v1.16b,v1.16b

	ld1	{v2.16b},[x0]
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	ld1	{v4.16b},[x8],16
	aesd	v1.16b,v11.16b
	aesimc	v1.16b,v1.16b
	ld1	{v5.16b},[x8],16
	mov	v21.16b, v22.16b
	sha256su0	v28.4s,v29.4s
	sha256h	q22, q23, v6.4s
	aesd	v1.16b,v12.16b
	aesimc	v1.16b,v1.16b
	sha256h2	q23, q21, v6.4s
	ld1	{v6.16b},[x8],16
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aesd	v1.16b,v13.16b
	aesimc	v1.16b,v1.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v1.16b,v14.16b
	aesimc	v1.16b,v1.16b
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	aesd	v1.16b,v15.16b
	aesimc	v1.16b,v1.16b
	add	v5.4s,v5.4s,v27.4s
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	add	v6.4s,v6.4s,v28.4s
	cmp	x16,#12
	b.lt	Laes128_dec_epilog_1
Laes192_dec_epilog_1:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	b.gt	Laes256_dec_epilog_1
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_epilog_1:
	aesd	v1.16b,v23.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_epilog_1:
	aesd	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	add	v7.4s,v7.4s,v29.4s
	eor	v1.16b,v1.16b,v31.16b

	ld1	{v31.16b},[x0],16




	sha256su0	v26.4s,v27.4s
	aesd	v2.16b,v8.16b
	aesimc	v2.16b,v2.16b

	st1	{v1.16b},[x1],16
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aesd	v2.16b,v9.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	ld1	{v4.16b},[x8],16
	sha256su0	v27.4s,v28.4s
	aesd	v2.16b,v10.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v2.16b,v11.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	ld1	{v5.16b},[x8],16
	sha256su0	v28.4s,v29.4s
	aesd	v2.16b,v12.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v2.16b,v13.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su0	v29.4s,v26.4s

	ld1	{v3.16b},[x0]
	aesd	v2.16b,v14.16b
	aesimc	v2.16b,v2.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v2.16b,v15.16b
	aesimc	v2.16b,v2.16b
	sha256su1	v29.4s,v27.4s,v28.4s
	add	v5.4s,v5.4s,v27.4s
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	aesd	v2.16b,v16.16b
	aesimc	v2.16b,v2.16b
	cmp	x16,#12
	b.lt	Laes128_dec_epilog_2
Laes192_dec_epilog_2:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v17.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	b.gt	Laes256_dec_epilog_2
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_epilog_2:
	aesd	v2.16b,v23.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_epilog_2:
	aesd	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	add	v6.4s,v6.4s,v28.4s
	add	v7.4s,v7.4s,v29.4s
	eor	v2.16b,v2.16b,v30.16b

	ld1	{v30.16b},[x0],16




	aesd	v3.16b,v8.16b
	aesimc	v3.16b,v3.16b

	st1	{v2.16b},[x1],16
	aesd	v3.16b,v9.16b
	aesimc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aesd	v3.16b,v10.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v11.16b
	aesimc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v3.16b,v12.16b
	aesimc	v3.16b,v3.16b

	ld1	{v0.16b},[x0]
	aesd	v3.16b,v13.16b
	aesimc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v3.16b,v14.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v15.16b
	aesimc	v3.16b,v3.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	aesd	v3.16b,v16.16b
	aesimc	v3.16b,v3.16b
	cmp	x16,#12
	b.lt	Laes128_dec_epilog_3
Laes192_dec_epilog_3:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v17.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	b.gt	Laes256_dec_epilog_3
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_epilog_3:
	aesd	v3.16b,v23.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_epilog_3:
	aesd	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s
	eor	v3.16b,v3.16b,v31.16b

	ld1	{v31.16b},[x0],16


	/*
	*	now we have to do the 4 aes blocks (b-2) that catch up to where sha is
	*/


	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b

	st1	{v3.16b},[x1],16
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b

	ld1	{v1.16b},[x0]
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	cmp	x16,#12
	b.lt	Laes128_dec_catchup_0
Laes192_dec_catchup_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_catchup_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_catchup_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_catchup_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	eor	v0.16b,v0.16b,v30.16b

	ld1	{v30.16b},[x0],16



	ld1	{v2.16b},[x0]
	aesd	v1.16b,v8.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v9.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v10.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v11.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v12.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v13.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v14.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v15.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	cmp	x16,#12
	b.lt	Laes128_dec_catchup_1
Laes192_dec_catchup_1:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	b.gt	Laes256_dec_catchup_1
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_catchup_1:
	aesd	v1.16b,v23.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_catchup_1:
	aesd	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	eor	v1.16b,v1.16b,v31.16b

	ld1	{v31.16b},[x0],16


	aesd	v2.16b,v8.16b
	aesimc	v2.16b,v2.16b

	ld1	{v3.16b},[x0]
	aesd	v2.16b,v9.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v10.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v11.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v12.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v13.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v14.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v15.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v16.16b
	aesimc	v2.16b,v2.16b
	cmp	x16,#12
	b.lt	Laes128_dec_catchup_2
Laes192_dec_catchup_2:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v17.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	b.gt	Laes256_dec_catchup_2
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_catchup_2:
	aesd	v2.16b,v23.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_catchup_2:
	aesd	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	eor	v2.16b,v2.16b,v30.16b

	ld1	{v30.16b},[x0],16


	aesd	v3.16b,v8.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v9.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v10.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v11.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v12.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v13.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v14.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v15.16b
	aesimc	v3.16b,v3.16b
	eor	v26.16b,v26.16b,v26.16b
	eor	v27.16b,v27.16b,v27.16b
	aesd	v3.16b,v16.16b
	aesimc	v3.16b,v3.16b
	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b
	cmp	x16,#12
	b.lt	Laes128_dec_catchup_3
Laes192_dec_catchup_3:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v17.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	b.gt	Laes256_dec_catchup_3
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_catchup_3:
	aesd	v3.16b,v23.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_catchup_3:
	aesd	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	eor	v3.16b,v3.16b,v31.16b

	add	x19,x19,4
	/*
	*	Now, there is the final b-1 sha256 padded block.
	*	This contains between 0-3 aes blocks. We take some pains to avoid read spill
	*	by only reading the blocks that are actually defined.
	*	This is also the final sha block code for the shortCases.
	*/
Ljoin_common:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	mov	w15,0x80
Lpost_loop_Q0:

	mov	v26.b[0],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8

	mov	v26.d[0],x2

	mov	v26.b[8],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	mov	v26.d[1],x2
Lpost_loop_Q1:

	mov	v27.b[0],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8

	mov	v27.d[0],x2

	mov	v27.b[8],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	mov	v27.d[1],x2
Lpost_loop_Q2:

	mov	v28.b[0],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8

	mov	v28.d[0],x2

	mov	v28.b[8],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	mov	v28.d[1],x2
Lpost_loop_Q3:

	mov	v29.b[3],w15

	cbz	x5,Lpost_loop

	ldr	x2,[x3],8
	sub	x5,x5,8
	rev32	x2,x2

	mov	v29.d[0],x2

	mov	v29.b[11],w15

	cbz	x5,1f

	ldr	x2,[x3],8
	rev32	x2,x2
	mov	v29.d[1],x2

	/*
	*	That is enough of blocks, we allow up to 64 bytes in total.
	*	Now we have the sha256 to do for these 4 16B blocks.
	*/
1:
	mov	x9,x8
	rev32	v26.16b,v26.16b
	ld1	{v4.16b},[x9],16
	rev32	v27.16b,v27.16b
	rev32	v28.16b,v28.16b
	ld1	{v5.16b},[x9],16
	//rev32		v29.16b,v29.16b

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x9],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	add	v5.4s,v5.4s,v27.4s
	eor	v26.16b,v26.16b,v26.16b
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	add	v6.4s,v6.4s,v28.4s
	eor	v27.16b,v27.16b,v27.16b
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v7.4s,v7.4s,v29.4s
	eor	v28.16b,v28.16b,v28.16b
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	eor	v29.16b,v29.16b,v29.16b
	sha256h2	q23, q21, v7.4s

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s


	cbz	x5,Lpost_loop
	subs	x5,x5,8

	b.ne	Lpost_loop_Q0

	mov	v26.b[0],w15

Lpost_loop:

	add	x11,x11,x20

	add	x11,x11,#64
	lsr	x12,x11,32
	and	x14,x11,0xffffffff
	lsl	x12,x12,3
	lsl	x14,x14,3

	mov	v29.s[3],w14
	mov	v29.s[2],w12

	rev32	v26.16b,v26.16b
	mov	v22.16b,v24.16b
	rev32	v27.16b,v27.16b
	mov	v23.16b,v25.16b
	rev32	v28.16b,v28.16b


	cbz	x19,1f
	/*
	*	At this point all data should be fetched for SHA.
	*	Save remaining blocks without danger of overwriting SHA source.
	*/
	stp	q0,q1,[x1],32
	stp	q2,q3,[x1],32
1:
	/*
	*	final sha block
	*	the strategy is to combine the 0-3 aes blocks, which is faster but
	*	a little gourmand on code space.
	*/
	cbz	x13,Lzero_aes_blocks_left

	ld1	{v0.16b},[x0]
	ld1	{v31.16b},[x0],16
	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ld1	{v4.16b},[x8],16
	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s
	add	v6.4s,v6.4s,v28.4s
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	sha256h2	q23, q21, v4.4s
	sha256su1	v26.4s,v28.4s,v29.4s
	sha256su0	v27.4s,v28.4s
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	sha256h2	q23, q21, v6.4s
	sha256su1	v28.4s,v26.4s,v27.4s
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	cmp	x16,#12
	b.lt	Laes128_dec_final1_0
Laes192_dec_final1_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_final1_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_final1_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_final1_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256su1	v29.4s,v27.4s,v28.4s
	eor	v3.16b,v0.16b,v30.16b

	sub	x13,x13,1

	st1	{v3.16b},[x1],16
	cbz	x13,Lfrmquad1




	ld1	{v0.16b},[x0]
	ld1	{v30.16b},[x0],16
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s
	add	v6.4s,v6.4s,v28.4s
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	sha256h	q22, q23, v4.4s
	sha256h2	q23, q21, v4.4s
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v26.4s,v28.4s,v29.4s
	sha256su0	v27.4s,v28.4s
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	sha256h2	q23, q21, v5.4s
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v27.4s,v29.4s,v26.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	sha256su0	v29.4s,v26.4s
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	cmp	x16,#12
	b.lt	Laes128_dec_final2_0
Laes192_dec_final2_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_final2_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_final2_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_final2_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256su1	v29.4s,v27.4s,v28.4s
	eor	v3.16b,v0.16b,v31.16b

	sub	x13,x13,1

	st1	{v3.16b},[x1],16
	cbz	x13,Lfrmquad2




	ld1	{v0.16b},[x0],16
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16
	ld1	{v6.16b},[x8],16
	ld1	{v7.16b},[x8],16
	add	v4.4s,v4.4s,v26.4s
	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	add	v5.4s,v5.4s,v27.4s
	add	v6.4s,v6.4s,v28.4s
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	sha256h2	q23, q21, v4.4s
	sha256su1	v26.4s,v28.4s,v29.4s
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	sha256h2	q23, q21, v5.4s
	sha256su1	v27.4s,v29.4s,v26.4s
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	add	v7.4s,v7.4s,v29.4s
	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	sha256su1	v28.4s,v26.4s,v27.4s
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	cmp	x16,#12
	b.lt	Laes128_dec_final3_0
Laes192_dec_final3_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_final3_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_final3_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_final3_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s
	eor	v3.16b,v0.16b,v30.16b

	st1	{v3.16b},[x1],16
	b	Lfrmquad3
	/*
	*	the final block with no aes component, i.e from here there were zero blocks
	*/

Lzero_aes_blocks_left:

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lfrmquad1:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lfrmquad2:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	sha256su1	v29.4s,v27.4s,v28.4s


Lfrmquad3:
	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s
	eor	v26.16b,v26.16b,v26.16b
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v7.4s,v7.4s,v29.4s
	eor	v27.16b,v27.16b,v27.16b
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	eor	v28.16b,v28.16b,v28.16b
	sha256h2	q23, q21, v7.4s

	add	v26.4s,v24.4s,v22.4s
	eor	v29.16b,v29.16b,v29.16b
	add	v27.4s,v25.4s,v23.4s

	/*
	*	Calculate final HMAC
	*/

	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF

	ld1	{v24.16b},[x7],16
	ld1	{v25.16b},[x7]

	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b


	mov	w11, #0x80
	mov	v28.b[3], w11

	mov	x11, #64+32
	lsl	x11, x11, 3

	mov	v29.s[3], w11
	lsr	x11, x11, 32
	mov	v29.s[2], w11

	ld1	{v4.16b},[x8],16
	ld1	{v5.16b},[x8],16

	add	v4.4s,v4.4s,v26.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x8],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s


	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x8],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s


	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s


	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x8],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x8],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s


	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x8],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s


	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x8],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s

	mov	v21.16b, v22.16b

	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x8],16
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b

	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x8],16
	add	v6.4s,v6.4s,v28.4s

	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b

	sha256h	q22, q23, v6.4s
	add	v7.4s,v7.4s,v29.4s
	ldp	d10,d11,[sp,#16]
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b

	sha256h	q22, q23, v7.4s
	ldp	d12,d13,[sp,#32]
	sha256h2	q23, q21, v7.4s

	add	v24.4s,v24.4s,v22.4s
	ldp	d14,d15,[sp,#48]
	add	v25.4s,v25.4s,v23.4s

	rev32	v24.16b, v24.16b
	ldp	x19,x20,[sp,#64]
	ldp	d8,d9,[sp],#80
	rev32	v25.16b, v25.16b
	st1	{v24.4s},[x4],16
	mov	x0, xzr
	st1	{v25.4s},[x4]

	ret

	/*
	*	These are the short cases (less efficient), here used for 1-11 aes blocks.
	*	x10 = aes_blocks
	*/
Ldec_short_cases:
	ldp	q8,q9,[x9],32
	adrp	x8,Lrcon@PAGE
	add	x8,x8,Lrcon@PAGEOFF
	ldp	q10,q11,[x9],32
	lsl	x11,x10,4

	ldp	q12,q13,[x9],32
	ldp	q14,q15,[x9],32
	ld1	{v30.16b},[x6]
	ldp	q16,q17,[x9],32
	ld1	{v18.16b},[x9]


	sub	x20,x5,x2


	mov	x19,xzr

	mov	x2,x0

	/*
	*	Digest source has to be at least of cipher source length
	*	therefore it is safe to use x10 to indicate whether we can
	*	overtake cipher processing by 4 AES block here.
	*/
	cmp	x10,4

	blt	Llast_sha_block

	sub	x5,x5,64

	mov	x9,x8


	ld1	{v26.16b},[x3],16
	ld1	{v4.16b},[x9],16
	ld1	{v27.16b},[x3],16
	rev32	v26.16b,v26.16b
	ld1	{v28.16b},[x3],16
	rev32	v27.16b,v27.16b
	ld1	{v29.16b},[x3],16
	rev32	v28.16b,v28.16b
	ld1	{v5.16b},[x9],16
	add	v4.4s,v4.4s,v26.4s
	rev32	v29.16b,v29.16b

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	mov	v21.16b, v22.16b

	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s


	b	Lshort_loop_no_store

Ldec_short_loop:
	cmp	x10,4

	blt	Llast_sha_block
	stp	q0,q1,[x1],32
	stp	q2,q3,[x1],32

	sub	x19,x19,4

Lshort_loop_no_store:
	ld1	{v31.16b},[x2]

	ld1	{v0.16b},[x2],16

	add	x0,x0,64


	aesd	v0.16b,v8.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v9.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v10.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v11.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v12.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v13.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v14.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v15.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	cmp	x16,#12
	b.lt	Laes128_dec_short_0
Laes192_dec_short_0:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	b.gt	Laes256_dec_short_0
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_short_0:
	aesd	v0.16b,v23.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v19.16b},[x17]
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_short_0:
	aesd	v0.16b,v17.16b
	eor	v0.16b,v0.16b,v18.16b
1:
	eor	v0.16b,v0.16b,v30.16b

	ld1	{v30.16b},[x2]

	ld1	{v1.16b},[x2],16


	aesd	v1.16b,v8.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v9.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v10.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v11.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v12.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v13.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v14.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v15.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	cmp	x16,#12
	b.lt	Laes128_dec_short_1
Laes192_dec_short_1:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	b.gt	Laes256_dec_short_1
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_short_1:
	aesd	v1.16b,v23.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	ld1	{v19.16b},[x17]
	aesd	v1.16b,v23.16b
	eor	v1.16b,v1.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_short_1:
	aesd	v1.16b,v17.16b
	eor	v1.16b,v1.16b,v18.16b
1:
	eor	v1.16b,v1.16b,v31.16b

	ld1	{v31.16b},[x2]

	ld1	{v2.16b},[x2],16


	aesd	v2.16b,v8.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v9.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v10.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v11.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v12.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v13.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v14.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v15.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v16.16b
	aesimc	v2.16b,v2.16b
	cmp	x16,#12
	b.lt	Laes128_dec_short_2
Laes192_dec_short_2:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v17.16b
	aesimc	v2.16b,v2.16b
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	b.gt	Laes256_dec_short_2
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_short_2:
	aesd	v2.16b,v23.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v2.16b,v19.16b
	aesimc	v2.16b,v2.16b
	ld1	{v19.16b},[x17]
	aesd	v2.16b,v23.16b
	eor	v2.16b,v2.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_short_2:
	aesd	v2.16b,v17.16b
	eor	v2.16b,v2.16b,v18.16b
1:
	eor	v2.16b,v2.16b,v30.16b

	ld1	{v30.16b},[x2]

	ld1	{v3.16b},[x2],16


	aesd	v3.16b,v8.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v9.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v10.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v11.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v12.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v13.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v14.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v15.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v16.16b
	aesimc	v3.16b,v3.16b
	cmp	x16,#12
	b.lt	Laes128_dec_short_3
Laes192_dec_short_3:
	stp	q19,q23,[sp, #-32]!
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v17.16b
	aesimc	v3.16b,v3.16b
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	b.gt	Laes256_dec_short_3
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #32
	ldp	q19,q23,[sp], #32
	b	1f
Laes256_dec_short_3:
	aesd	v3.16b,v23.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17],16
	ld1	{v23.16b},[x17],16
	aesd	v3.16b,v19.16b
	aesimc	v3.16b,v3.16b
	ld1	{v19.16b},[x17]
	aesd	v3.16b,v23.16b
	eor	v3.16b,v3.16b,v19.16b
	sub	x17, x17, #64
	ldp	q19,q23,[sp], #32
	b	1f
Laes128_dec_short_3:
	aesd	v3.16b,v17.16b
	eor	v3.16b,v3.16b,v18.16b
1:
	eor	v3.16b,v3.16b,v31.16b

	add	x19,x19,4

	sub	x10,x10,4
	cmp	x5,64
	b.lt	Ldec_short_loop

	sub	x5,x5,64

	mov	x9,x8


	ld1	{v26.16b},[x3],16
	ld1	{v4.16b},[x9],16
	ld1	{v27.16b},[x3],16
	rev32	v26.16b,v26.16b
	ld1	{v28.16b},[x3],16
	rev32	v27.16b,v27.16b
	ld1	{v29.16b},[x3],16
	rev32	v28.16b,v28.16b
	ld1	{v5.16b},[x9],16
	add	v4.4s,v4.4s,v26.4s
	rev32	v29.16b,v29.16b

	sha256su0	v26.4s,v27.4s
	mov	v22.16b,v24.16b
	mov	v23.16b,v25.16b
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s
	ld1	{v5.16b},[x9],16
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	sha256su0	v26.4s,v27.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	sha256h2	q23, q21, v4.4s
	add	v5.4s,v5.4s,v27.4s
	sha256su1	v26.4s,v28.4s,v29.4s

	sha256su0	v27.4s,v28.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	sha256h2	q23, q21, v5.4s
	add	v6.4s,v6.4s,v28.4s
	sha256su1	v27.4s,v29.4s,v26.4s

	sha256su0	v28.4s,v29.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	ld1	{v4.16b},[x9],16
	sha256h2	q23, q21, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256su1	v28.4s,v26.4s,v27.4s

	sha256su0	v29.4s,v26.4s
	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	ld1	{v5.16b},[x9],16
	sha256h2	q23, q21, v7.4s
	add	v4.4s,v4.4s,v26.4s
	sha256su1	v29.4s,v27.4s,v28.4s


	mov	v21.16b, v22.16b
	sha256h	q22, q23, v4.4s
	ld1	{v6.16b},[x9],16
	add	v5.4s,v5.4s,v27.4s
	sha256h2	q23, q21, v4.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v5.4s
	ld1	{v7.16b},[x9],16
	add	v6.4s,v6.4s,v28.4s
	sha256h2	q23, q21, v5.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v6.4s
	add	v7.4s,v7.4s,v29.4s
	sha256h2	q23, q21, v6.4s

	mov	v21.16b, v22.16b
	sha256h	q22, q23, v7.4s
	sha256h2	q23, q21, v7.4s

	add	v24.4s,v24.4s,v22.4s
	add	v25.4s,v25.4s,v23.4s

	b	Ldec_short_loop
	/*
	*	This is arranged so that we can join the common unwind code that does
	*	the last sha block and the final 0-3 aes blocks.
	*/
Llast_sha_block:
	eor	v26.16b,v26.16b,v26.16b
	eor	v27.16b,v27.16b,v27.16b
	eor	v28.16b,v28.16b,v28.16b
	eor	v29.16b,v29.16b,v29.16b

	mov	x13,x10
	b	Ljoin_common


