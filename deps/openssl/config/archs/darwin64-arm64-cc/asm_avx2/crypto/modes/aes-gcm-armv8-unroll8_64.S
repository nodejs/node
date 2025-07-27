#include "arm_arch.h"

#if __ARM_MAX_ARCH__>=8

.text
.globl	_unroll8_eor3_aes_gcm_enc_128_kernel

.align	4
_unroll8_eor3_aes_gcm_enc_128_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L128_enc_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	mov	x15, #0x100000000				//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15
	mov	x5, x9
	ld1	{ v0.16b}, [x16]					//CTR block 0

	sub	x5, x5, #1	 	//byte_len - 1

	and	x5, x5, #0xffffffffffffff80		//number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	rev32	v30.16b, v0.16b				//set up reversed counter

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5
	ldp	q26, q27, [x8, #0]				  	//load rk0, rk1

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6

	rev32	v7.16b, v30.16b				//CTR block 7
	add	v30.4s, v30.4s, v31.4s		//CTR block 7

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 1

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7

	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	ldr	q27, [x8, #160]					//load rk10

	aese	v3.16b, v26.16b						//AES block 8k+11 - round 9
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	aese	v2.16b, v26.16b						//AES block 8k+10 - round 9

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v6.16b, v26.16b						//AES block 8k+14 - round 9

	aese	v4.16b, v26.16b						//AES block 8k+12 - round 9
	add	x5, x5, x0
	aese	v0.16b, v26.16b						//AES block 8k+8 - round 9

	aese	v7.16b, v26.16b						//AES block 8k+15 - round 9
	aese	v5.16b, v26.16b						//AES block 8k+13 - round 9
	aese	v1.16b, v26.16b						//AES block 8k+9 - round 9

	add	x4, x0, x1, lsr #3		//end_input_ptr
	cmp	x0, x5				//check if we have <= 8 blocks
	b.ge	L128_enc_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load plaintext

	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load plaintext

	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load plaintext

	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load plaintext
	cmp	x0, x5				//check if we have <= 8 blocks

.long	0xce006d08	//eor3 v8.16b, v8.16b, v0.16b, v27.16b				//AES block 0 - result
	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8

.long	0xce016d29	//eor3 v9.16b, v9.16b, v1.16b, v27.16b				//AES block 1 - result
	stp	q8, q9, [x2], #32			//AES block 0, 1 - store result

	rev32	v1.16b, v30.16b				//CTR block 9
.long	0xce056dad	//eor3 v13.16b, v13.16b, v5.16b, v27.16b				//AES block 5 - result
	add	v30.4s, v30.4s, v31.4s		//CTR block 9

.long	0xce026d4a	//eor3 v10.16b, v10.16b, v2.16b, v27.16b				//AES block 2 - result
.long	0xce066dce	//eor3 v14.16b, v14.16b, v6.16b, v27.16b				//AES block 6 - result
.long	0xce046d8c	//eor3 v12.16b, v12.16b, v4.16b, v27.16b				//AES block 4 - result

	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10

.long	0xce036d6b	//eor3 v11.16b, v11.16b, v3.16b, v27.16b				//AES block 3 - result
.long	0xce076def	//eor3 v15.16b, v15.16b, v7.16b,v27.16b				//AES block 7 - result
	stp	q10, q11, [x2], #32			//AES block 2, 3 - store result

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11
	stp	q12, q13, [x2], #32			//AES block 4, 5 - store result

	stp	q14, q15, [x2], #32			//AES block 6, 7 - store result

	rev32	v4.16b, v30.16b				//CTR block 12
	add	v30.4s, v30.4s, v31.4s		//CTR block 12
	b.ge	L128_enc_prepretail					//do prepretail

L128_enc_main_loop:	//main	loop start
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	rev64	v8.16b, v8.16b						//GHASH block 8k
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k
	rev64	v13.16b, v13.16b						//GHASH block 8k+5 (t0, t1, t2 and t3 free)
	rev64	v11.16b, v11.16b						//GHASH block 8k+3

	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev32	v7.16b, v30.16b				//CTR block 8k+15

	rev64	v15.16b, v15.16b						//GHASH block 8k+7 (t0, t1, t2 and t3 free)

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low

	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high

	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h3l | h3h
	ext	v25.16b, v25.16b, v25.16b, #8
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b,v9.16b			//GHASH block 8k+2, 8k+3 - high
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1

	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid

	rev64	v14.16b, v14.16b						//GHASH block 8k+6 (t0, t1, and t2 free)
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low

	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	rev64	v12.16b, v12.16b						//GHASH block 8k+4 (t0, t1, and t2 free)

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h1l | h1h
	ext	v22.16b, v22.16b, v22.16b, #8
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4

	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5

	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5

	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	ldr	d16, [x10]			//MODULO - load modulo constant
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load plaintext

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	rev32	v20.16b, v30.16b					//CTR block 8k+16
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
	ldp	q28, q26, [x8, #128]				//load rk8, rk9
.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load plaintext

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7

	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7

	rev32	v22.16b, v30.16b					//CTR block 8k+17
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	ldp	q12, q13, [x0], #32			//AES block 8k+12, 8k+13 - load plaintext
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ldr	q27, [x8, #160]					//load rk10

	ext	v29.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment
	rev32	v23.16b, v30.16b					//CTR block 8k+18
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8

	aese	v2.16b, v26.16b						//AES block 8k+10 - round 9
	aese	v4.16b, v26.16b						//AES block 8k+12 - round 9
	aese	v1.16b, v26.16b						//AES block 8k+9 - round 9

	ldp	q14, q15, [x0], #32			//AES block 8k+14, 8k+15 - load plaintext
	rev32	v25.16b, v30.16b					//CTR block 8k+19
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19

	cmp	x0, x5				//LOOP CONTROL
.long	0xce046d8c	//eor3 v12.16b, v12.16b, v4.16b, v27.16b				//AES block 4 - result
	aese	v7.16b, v26.16b						//AES block 8k+15 - round 9

	aese	v6.16b, v26.16b						//AES block 8k+14 - round 9
	aese	v3.16b, v26.16b						//AES block 8k+11 - round 9

.long	0xce026d4a	//eor3 v10.16b, v10.16b, v2.16b, v27.16b				//AES block 8k+10 - result

	mov	v2.16b, v23.16b					//CTR block 8k+18
	aese	v0.16b, v26.16b						//AES block 8k+8 - round 9

	rev32	v4.16b, v30.16b				//CTR block 8k+20
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20

.long	0xce076def	//eor3 v15.16b, v15.16b, v7.16b, v27.16b				//AES block 7 - result
	aese	v5.16b, v26.16b						//AES block 8k+13 - round 9
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low

.long	0xce016d29	//eor3 v9.16b, v9.16b, v1.16b, v27.16b				//AES block 8k+9 - result
.long	0xce036d6b	//eor3 v11.16b, v11.16b, v3.16b, v27.16b				//AES block 8k+11 - result
	mov	v3.16b, v25.16b					//CTR block 8k+19

	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment
.long	0xce056dad	//eor3 v13.16b, v13.16b, v5.16b, v27.16b				//AES block 5 - result
	mov	v1.16b, v22.16b					//CTR block 8k+17

.long	0xce006d08	//eor3 v8.16b, v8.16b, v0.16b, v27.16b				//AES block 8k+8 - result
	mov	v0.16b, v20.16b					//CTR block 8k+16
	stp	q8, q9, [x2], #32			//AES block 8k+8, 8k+9 - store result

	stp	q10, q11, [x2], #32			//AES block 8k+10, 8k+11 - store result
.long	0xce066dce	//eor3 v14.16b, v14.16b, v6.16b, v27.16b				//AES block 6 - result

	stp	q12, q13, [x2], #32			//AES block 8k+12, 8k+13 - store result
.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low

	stp	q14, q15, [x2], #32			//AES block 8k+14, 8k+15 - store result
	b.lt	L128_enc_main_loop

L128_enc_prepretail:	//PREPRETAIL
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev64	v8.16b, v8.16b						//GHASH block 8k
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h6k | h5k
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13
	rev64	v11.16b, v11.16b						//GHASH block 8k+3

	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1

	rev32	v6.16b, v30.16b				//CTR block 8k+14

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	rev64	v13.16b, v13.16b						//GHASH block 8k+5 (t0, t1, t2 and t3 free)
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid

	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid

	rev64	v12.16b, v12.16b						//GHASH block 8k+4 (t0, t1, and t2 free)
	rev64	v15.16b, v15.16b						//GHASH block 8k+7 (t0, t1, t2 and t3 free)

	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	rev32	v7.16b, v30.16b				//CTR block 8k+15

	rev64	v14.16b, v14.16b						//GHASH block 8k+6 (t0, t1, and t2 free)

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0

	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h1l | h1h
	ext	v22.16b, v22.16b, v22.16b, #8
	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3

	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3

	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high

	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high

.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4

	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	ldr	d16, [x10]			//MODULO - load modulo constant

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	ext	v29.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	ext	v18.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
.long	0xce114a73	//eor3 v19.16b, v19.16b, v17.16b, v18.16b		 	//MODULO - fold into low
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8

	ldr	q27, [x8, #160]					//load rk10
	aese	v6.16b, v26.16b						//AES block 8k+14 - round 9
	aese	v2.16b, v26.16b						//AES block 8k+10 - round 9

	aese	v0.16b, v26.16b						//AES block 8k+8 - round 9
	aese	v1.16b, v26.16b						//AES block 8k+9 - round 9

	aese	v3.16b, v26.16b						//AES block 8k+11 - round 9
	aese	v5.16b, v26.16b						//AES block 8k+13 - round 9

	aese	v4.16b, v26.16b						//AES block 8k+12 - round 9
	aese	v7.16b, v26.16b						//AES block 8k+15 - round 9
L128_enc_tail:	//TAIL

	sub	x5, x4, x0 	//main_end_input_ptr is number of bytes left to process
	ldr	q8, [x0], #16				//AES block 8k+8 - load plaintext

	mov	v29.16b, v27.16b
	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8

.long	0xce007509	//eor3 v9.16b, v8.16b, v0.16b, v29.16b			//AES block 8k+8 - result
	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag
	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8

	ldp	q24, q25, [x3, #192]			//load h8k | h7k
	ext	v25.16b, v25.16b, v25.16b, #8
	cmp	x5, #112
	b.gt	L128_enc_blocks_more_than_7

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	movi	v17.8b, #0

	cmp	x5, #96
	sub	v30.4s, v30.4s, v31.4s
	mov	v5.16b, v4.16b

	mov	v4.16b, v3.16b
	mov	v3.16b, v2.16b
	mov	v2.16b, v1.16b

	movi	v19.8b, #0
	movi	v18.8b, #0
	b.gt	L128_enc_blocks_more_than_6

	mov	v7.16b, v6.16b
	cmp	x5, #80

	sub	v30.4s, v30.4s, v31.4s
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v3.16b
	mov	v3.16b, v1.16b
	b.gt	L128_enc_blocks_more_than_5

	cmp	x5, #64
	sub	v30.4s, v30.4s, v31.4s

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v4.16b
	mov	v4.16b, v1.16b
	b.gt	L128_enc_blocks_more_than_4

	mov	v7.16b, v6.16b
	sub	v30.4s, v30.4s, v31.4s
	mov	v6.16b, v5.16b

	mov	v5.16b, v1.16b
	cmp	x5, #48
	b.gt	L128_enc_blocks_more_than_3

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	mov	v6.16b, v1.16b

	cmp	x5, #32
	ldr	q24, [x3, #96]					//load h4k | h3k
	b.gt	L128_enc_blocks_more_than_2

	cmp	x5, #16

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v1.16b
	b.gt	L128_enc_blocks_more_than_1

	ldr	q21, [x3, #48]					//load h2k | h1k
	sub	v30.4s, v30.4s, v31.4s
	b	L128_enc_blocks_less_than_1
L128_enc_blocks_more_than_7:	//blocks	left >  7
	st1	{ v9.16b}, [x2], #16				//AES final-7 block  - store result

	rev64	v8.16b, v9.16b						//GHASH final-7 block
	ldr	q9, [x0], #16				//AES final-6 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high

	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in

.long	0xce017529	//eor3 v9.16b, v9.16b, v1.16b, v29.16b			//AES final-6 block - result

	pmull	v18.1q, v27.1d, v18.1d				//GHASH final-7 block - mid
	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low
L128_enc_blocks_more_than_6:	//blocks	left >  6

	st1	{ v9.16b}, [x2], #16				//AES final-6 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-6 block
	ldr	q9, [x0], #16				//AES final-5 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid

.long	0xce027529	//eor3 v9.16b, v9.16b, v2.16b, v29.16b			//AES final-5 block - result
	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high
L128_enc_blocks_more_than_5:	//blocks	left >  5

	st1	{ v9.16b}, [x2], #16				//AES final-5 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid
	ldr	q9, [x0], #16				//AES final-4 block - load plaintext
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid

.long	0xce037529	//eor3 v9.16b, v9.16b, v3.16b, v29.16b			//AES final-4 block - result
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
L128_enc_blocks_more_than_4:	//blocks	left >  4

	st1	{ v9.16b}, [x2], #16			  	//AES final-4 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	ldr	q9, [x0], #16				//AES final-3 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high
	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low

.long	0xce047529	//eor3 v9.16b, v9.16b, v4.16b, v29.16b			//AES final-3 block - result
	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
L128_enc_blocks_more_than_3:	//blocks	left >  3

	st1	{ v9.16b}, [x2], #16			  	//AES final-3 block - store result

	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8

	rev64	v8.16b, v9.16b						//GHASH final-3 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	movi	v16.8b, #0						//suppress further partial tag feed in

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid
	ldr	q24, [x3, #96]				//load h4k | h3k
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low

	ldr	q9, [x0], #16				//AES final-2 block - load plaintext

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low

.long	0xce057529	//eor3 v9.16b, v9.16b, v5.16b, v29.16b			//AES final-2 block - result

	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid
	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high
L128_enc_blocks_more_than_2:	//blocks	left >  2

	st1	{ v9.16b}, [x2], #16			  	//AES final-2 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-2 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ldr	q9, [x0], #16				//AES final-1 block - load plaintext

	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid
.long	0xce067529	//eor3 v9.16b, v9.16b, v6.16b, v29.16b			//AES final-1 block - result

	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low
	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low
L128_enc_blocks_more_than_1:	//blocks	left >  1

	st1	{ v9.16b}, [x2], #16			  	//AES final-1 block - store result

	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev64	v8.16b, v9.16b						//GHASH final-1 block
	ldr	q9, [x0], #16				//AES final block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	movi	v16.8b, #0						//suppress further partial tag feed in
	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid
.long	0xce077529	//eor3 v9.16b, v9.16b, v7.16b, v29.16b			//AES final block - result

	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid

	ldr	q21, [x3, #48]				//load h2k | h1k

	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid

	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low
	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low
L128_enc_blocks_less_than_1:	//blocks	left <= 1

	rev32	v30.16b, v30.16b
	str	q30, [x16]					//store the updated counter
	and	x1, x1, #127			 	//bit_length %= 128

	sub	x1, x1, #128			 	//bit_length -= 128

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])

	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff
	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored
	and	x1, x1, #127			 	//bit_length %= 128

	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff
	cmp	x1, #64

	csel	x13, x7, x6, lt
	csel	x14, x6, xzr, lt

	mov	v0.d[1], x14
	mov	v0.d[0], x13					//ctr0b is mask for last block

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits

	rev64	v8.16b, v9.16b						//GHASH final block

	bif	v9.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing
	st1	{ v9.16b}, [x2]				//store all 16B

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v16.d[0], v8.d[1]					//GHASH final block - mid

	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid

	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high
	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low

	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high

	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low

	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment
	pmull	v29.1q, v17.1d, v16.1d		  	//MODULO - top 64b align with mid

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		  	//MODULO - karatsuba tidy up

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b		 	//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ext	v21.16b, v18.16b, v18.16b, #8			  	//MODULO - other mid alignment

.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		  	//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]
	mov	x0, x9

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret

L128_enc_ret:
	mov	w0, #0x0
	ret

.globl	_unroll8_eor3_aes_gcm_dec_128_kernel

.align	4
_unroll8_eor3_aes_gcm_dec_128_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L128_dec_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	mov	x5, x9
	ld1	{ v0.16b}, [x16]					//CTR block 0

	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	sub	x5, x5, #1		//byte_len - 1

	mov	x15, #0x100000000				//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15
	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b

	rev32	v30.16b, v0.16b				//set up reversed counter

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	and	x5, x5, #0xffffffffffffff80	//number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0

	rev32	v7.16b, v30.16b				//CTR block 7

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 0
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 0

	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7

	add	x5, x5, x0
	add	v30.4s, v30.4s, v31.4s		//CTR block 7

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 8

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 8

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 8

	aese	v0.16b, v26.16b						//AES block 0 - round 9
	aese	v1.16b, v26.16b						//AES block 1 - round 9
	aese	v6.16b, v26.16b						//AES block 6 - round 9

	ldr	q27, [x8, #160]					//load rk10
	aese	v4.16b, v26.16b						//AES block 4 - round 9
	aese	v3.16b, v26.16b						//AES block 3 - round 9

	aese	v2.16b, v26.16b						//AES block 2 - round 9
	aese	v5.16b, v26.16b						//AES block 5 - round 9
	aese	v7.16b, v26.16b						//AES block 7 - round 9

	add	x4, x0, x1, lsr #3		//end_input_ptr
	cmp	x0, x5				//check if we have <= 8 blocks
	b.ge	L128_dec_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load ciphertext

.long	0xce006d00	//eor3 v0.16b, v8.16b, v0.16b, v27.16b				//AES block 0 - result
.long	0xce016d21	//eor3 v1.16b, v9.16b, v1.16b, v27.16b				//AES block 1 - result
	stp	q0, q1, [x2], #32			//AES block 0, 1 - store result

	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8
	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load ciphertext

	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load ciphertext

	rev32	v1.16b, v30.16b				//CTR block 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 9
	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load ciphertext

.long	0xce036d63	//eor3 v3.16b, v11.16b, v3.16b, v27.16b				//AES block 3 - result
.long	0xce026d42	//eor3 v2.16b, v10.16b, v2.16b, v27.16b				//AES block 2 - result
	stp	q2, q3, [x2], #32			//AES block 2, 3 - store result

	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10

.long	0xce066dc6	//eor3 v6.16b, v14.16b, v6.16b, v27.16b				//AES block 6 - result

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11

.long	0xce046d84	//eor3 v4.16b, v12.16b, v4.16b, v27.16b				//AES block 4 - result
.long	0xce056da5	//eor3 v5.16b, v13.16b, v5.16b, v27.16b				//AES block 5 - result
	stp	q4, q5, [x2], #32			//AES block 4, 5 - store result

.long	0xce076de7	//eor3 v7.16b, v15.16b, v7.16b, v27.16b				//AES block 7 - result
	stp	q6, q7, [x2], #32			//AES block 6, 7 - store result
	rev32	v4.16b, v30.16b				//CTR block 12

	cmp	x0, x5				//check if we have <= 8 blocks
	add	v30.4s, v30.4s, v31.4s		//CTR block 12
	b.ge	L128_dec_prepretail					//do prepretail

L128_dec_main_loop:	//main	loop start
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8

	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	rev64	v8.16b, v8.16b						//GHASH block 8k
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	rev64	v14.16b, v14.16b						//GHASH block 8k+6
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high
	rev64	v11.16b, v11.16b						//GHASH block 8k+3

	rev32	v7.16b, v30.16b				//CTR block 8k+15
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	rev64	v13.16b, v13.16b						//GHASH block 8k+5

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	rev64	v15.16b, v15.16b						//GHASH block 8k+7
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3

	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high

	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4

	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5

	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b 			//GHASH block 8k+4, 8k+5 - mid
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	ldr	d16, [x10]			//MODULO - load modulo constant
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

	rev32	v20.16b, v30.16b					//CTR block 8k+16
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	rev32	v22.16b, v30.16b					//CTR block 8k+17

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment
	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load ciphertext

	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load ciphertext
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	rev32	v23.16b, v30.16b					//CTR block 8k+18

	ldp	q12, q13, [x0], #32			//AES block 8k+12, 8k+13 - load ciphertext
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	ldp	q14, q15, [x0], #32			//AES block 8k+14, 8k+15 - load ciphertext
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8

	aese	v0.16b, v26.16b						//AES block 8k+8 - round 9
	aese	v1.16b, v26.16b						//AES block 8k+9 - round 9
	ldr	q27, [x8, #160]					//load rk10

	aese	v6.16b, v26.16b						//AES block 8k+14 - round 9
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v2.16b, v26.16b						//AES block 8k+10 - round 9

	aese	v7.16b, v26.16b						//AES block 8k+15 - round 9
	aese	v4.16b, v26.16b						//AES block 8k+12 - round 9
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment

	rev32	v25.16b, v30.16b					//CTR block 8k+19
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19

	aese	v3.16b, v26.16b						//AES block 8k+11 - round 9
	aese	v5.16b, v26.16b						//AES block 8k+13 - round 9
.long	0xce016d21	//eor3 v1.16b, v9.16b, v1.16b, v27.16b				//AES block 8k+9 - result

.long	0xce006d00	//eor3 v0.16b, v8.16b, v0.16b, v27.16b				//AES block 8k+8 - result
.long	0xce076de7	//eor3 v7.16b, v15.16b, v7.16b, v27.16b				//AES block 8k+15 - result
.long	0xce066dc6	//eor3 v6.16b, v14.16b, v6.16b, v27.16b				//AES block 8k+14 - result

.long	0xce026d42	//eor3 v2.16b, v10.16b, v2.16b, v27.16b				//AES block 8k+10 - result
	stp	q0, q1, [x2], #32			//AES block 8k+8, 8k+9 - store result
	mov	v1.16b, v22.16b					//CTR block 8k+17

.long	0xce046d84	//eor3 v4.16b, v12.16b, v4.16b, v27.16b				//AES block 8k+12 - result
.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	mov	v0.16b, v20.16b					//CTR block 8k+16

.long	0xce036d63	//eor3 v3.16b, v11.16b, v3.16b, v27.16b				//AES block 8k+11 - result
	cmp	x0, x5				//LOOP CONTROL
	stp	q2, q3, [x2], #32			//AES block 8k+10, 8k+11 - store result

.long	0xce056da5	//eor3 v5.16b, v13.16b, v5.16b, v27.16b				//AES block 8k+13 - result
	mov	v2.16b, v23.16b					//CTR block 8k+18

	stp	q4, q5, [x2], #32			//AES block 8k+12, 8k+13 - store result
	rev32	v4.16b, v30.16b				//CTR block 8k+20
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20

	stp	q6, q7, [x2], #32			//AES block 8k+14, 8k+15 - store result
	mov	v3.16b, v25.16b					//CTR block 8k+19
	b.lt	L128_dec_main_loop

L128_dec_prepretail:	//PREPRETAIL
	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	rev64	v8.16b, v8.16b						//GHASH block 8k

	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1

	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev64	v13.16b, v13.16b						//GHASH block 8k+5

	rev64	v12.16b, v12.16b						//GHASH block 8k+4

	rev64	v14.16b, v14.16b						//GHASH block 8k+6

	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k
	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	rev32	v7.16b, v30.16b				//CTR block 8k+15
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k - mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4

	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5

	ldr	d16, [x10]			//MODULO - load modulo constant
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	ldr	q27, [x8, #160]					//load rk10

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8

	aese	v6.16b, v26.16b						//AES block 8k+14 - round 9
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8

.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15
	aese	v2.16b, v26.16b						//AES block 8k+10 - round 9

	aese	v3.16b, v26.16b						//AES block 8k+11 - round 9
	aese	v5.16b, v26.16b						//AES block 8k+13 - round 9
	aese	v0.16b, v26.16b						//AES block 8k+8 - round 9

	aese	v4.16b, v26.16b						//AES block 8k+12 - round 9
	aese	v1.16b, v26.16b						//AES block 8k+9 - round 9
	aese	v7.16b, v26.16b						//AES block 8k+15 - round 9

L128_dec_tail:	//TAIL

	mov	v29.16b, v27.16b
	sub	x5, x4, x0 	//main_end_input_ptr is number of bytes left to process

	cmp	x5, #112

	ldp	q24, q25, [x3, #192]			//load h8k | h7k
	ext	v25.16b, v25.16b, v25.16b, #8
	ldr	q9, [x0], #16				//AES block 8k+8 - load ciphertext

	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag

	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8

.long	0xce00752c	//eor3 v12.16b, v9.16b, v0.16b, v29.16b				//AES block 8k+8 - result
	b.gt	L128_dec_blocks_more_than_7

	cmp	x5, #96
	mov	v7.16b, v6.16b
	movi	v19.8b, #0

	movi	v17.8b, #0
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v3.16b
	mov	v3.16b, v2.16b
	mov	v2.16b, v1.16b

	movi	v18.8b, #0
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L128_dec_blocks_more_than_6

	cmp	x5, #80
	sub	v30.4s, v30.4s, v31.4s

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v3.16b
	mov	v3.16b, v1.16b
	b.gt	L128_dec_blocks_more_than_5

	cmp	x5, #64

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L128_dec_blocks_more_than_4

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v1.16b
	cmp	x5, #48
	b.gt	L128_dec_blocks_more_than_3

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	cmp	x5, #32

	ldr	q24, [x3, #96]				//load h4k | h3k
	mov	v6.16b, v1.16b
	b.gt	L128_dec_blocks_more_than_2

	cmp	x5, #16

	mov	v7.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L128_dec_blocks_more_than_1

	sub	v30.4s, v30.4s, v31.4s
	ldr	q21, [x3, #48]				//load h2k | h1k
	b	L128_dec_blocks_less_than_1
L128_dec_blocks_more_than_7:	//blocks	left >  7
	rev64	v8.16b, v9.16b						//GHASH final-7 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid

	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low
	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in
	ldr	q9, [x0], #16				//AES final-6 block - load ciphertext

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high
	st1	{ v12.16b}, [x2], #16			 	//AES final-7 block  - store result
.long	0xce01752c	//eor3 v12.16b, v9.16b, v1.16b, v29.16b				//AES final-6 block - result

	pmull	v18.1q, v27.1d, v18.1d			 	//GHASH final-7 block - mid
L128_dec_blocks_more_than_6:	//blocks	left >  6

	rev64	v8.16b, v9.16b						//GHASH final-6 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low
	ldr	q9, [x0], #16				//AES final-5 block - load ciphertext
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid
	st1	{ v12.16b}, [x2], #16			 	//AES final-6 block - store result
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
.long	0xce02752c	//eor3 v12.16b, v9.16b, v2.16b, v29.16b				//AES final-5 block - result
L128_dec_blocks_more_than_5:	//blocks	left >  5

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	ldr	q9, [x0], #16				//AES final-4 block - load ciphertext
	st1	{ v12.16b}, [x2], #16			 	//AES final-5 block - store result

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid

.long	0xce03752c	//eor3 v12.16b, v9.16b, v3.16b, v29.16b				//AES final-4 block - result

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high
L128_dec_blocks_more_than_4:	//blocks	left >  4

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	ldr	q9, [x0], #16				//AES final-3 block - load ciphertext

	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high

	st1	{ v12.16b}, [x2], #16			 	//AES final-4 block - store result
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid

.long	0xce04752c	//eor3 v12.16b, v9.16b, v4.16b, v29.16b				//AES final-3 block - result
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low

	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
L128_dec_blocks_more_than_3:	//blocks	left >  3

	st1	{ v12.16b}, [x2], #16			 	//AES final-3 block - store result
	rev64	v8.16b, v9.16b						//GHASH final-3 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid

	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	ldr	q24, [x3, #96]				//load h4k | h3k

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid

	ldr	q9, [x0], #16				//AES final-2 block - load ciphertext

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low
	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high

	movi	v16.8b, #0						//suppress further partial tag feed in
.long	0xce05752c	//eor3 v12.16b, v9.16b, v5.16b, v29.16b				//AES final-2 block - result
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low

	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high
	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
L128_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v8.16b, v9.16b						//GHASH final-2 block

	st1	{ v12.16b}, [x2], #16			 	//AES final-2 block - store result

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	movi	v16.8b, #0						//suppress further partial tag feed in

	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low

	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high
	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid
	ldr	q9, [x0], #16				//AES final-1 block - load ciphertext

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low

.long	0xce06752c	//eor3 v12.16b, v9.16b, v6.16b, v29.16b				//AES final-1 block - result
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high
L128_dec_blocks_more_than_1:	//blocks	left >  1

	st1	{ v12.16b}, [x2], #16			 	//AES final-1 block - store result
	rev64	v8.16b, v9.16b						//GHASH final-1 block

	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	movi	v16.8b, #0						//suppress further partial tag feed in

	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid

	ldr	q9, [x0], #16				//AES final block - load ciphertext
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high
	ldr	q21, [x3, #48]				//load h2k | h1k

	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid
.long	0xce07752c	//eor3 v12.16b, v9.16b, v7.16b, v29.16b				//AES final block - result

	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
L128_dec_blocks_less_than_1:	//blocks	left <= 1

	and	x1, x1, #127				//bit_length %= 128

	sub	x1, x1, #128				//bit_length -= 128

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])

	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff
	and	x1, x1, #127				//bit_length %= 128

	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	cmp	x1, #64
	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff

	csel	x13, x7, x6, lt
	csel	x14, x6, xzr, lt

	mov	v0.d[1], x14
	mov	v0.d[0], x13					//ctr0b is mask for last block

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits

	rev64	v8.16b, v9.16b						//GHASH final block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high
	ins	v16.d[0], v8.d[1]					//GHASH final block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high
	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid

	bif	v12.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid
	st1	{ v12.16b}, [x2]				//store all 16B

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low

	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low

	eor	v14.16b, v17.16b, v19.16b				//MODULO - karatsuba tidy up

	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	ext	v17.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

	eor	v18.16b, v18.16b, v14.16b				//MODULO - karatsuba tidy up

.long	0xce115652	//eor3 v18.16b, v18.16b, v17.16b, v21.16b			//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ext	v18.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment

.long	0xce124673	//eor3 v19.16b, v19.16b, v18.16b, v17.16b			//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]
	rev32	v30.16b, v30.16b

	str	q30, [x16]					//store the updated counter

	mov	x0, x9

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret
L128_dec_ret:
	mov	w0, #0x0
	ret

.globl	_unroll8_eor3_aes_gcm_enc_192_kernel

.align	4
_unroll8_eor3_aes_gcm_enc_192_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L192_enc_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	mov	x5, x9
	ld1	{ v0.16b}, [x16]					//CTR block 0

	mov	x15, #0x100000000				//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15

	rev32	v30.16b, v0.16b				//set up reversed counter

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4
	sub	x5, x5, #1		//byte_len - 1

	and	x5, x5, #0xffffffffffffff80	//number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1

	add	x5, x5, x0

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6

	rev32	v7.16b, v30.16b				//CTR block 7

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 0

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 1

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 1

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5

	add	v30.4s, v30.4s, v31.4s		//CTR block 7

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 8

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 8

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 8

	add	x4, x0, x1, lsr #3		//end_input_ptr
	cmp	x0, x5				//check if we have <= 8 blocks
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 9

	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	ldp	q27, q28, [x8, #160]				//load rk10, rk11

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 9

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 9
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 9

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 9
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 9

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 14 - round 10
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 11 - round 10

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 9 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 13 - round 10
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 12 - round 10

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 10 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 15 - round 10

	aese	v6.16b, v28.16b						//AES block 14 - round 11
	aese	v3.16b, v28.16b						//AES block 11 - round 11

	aese	v4.16b, v28.16b						//AES block 12 - round 11
	aese	v7.16b, v28.16b						//AES block 15 - round 11
	ldr	q26, [x8, #192]					//load rk12

	aese	v1.16b, v28.16b						//AES block 9 - round 11
	aese	v5.16b, v28.16b						//AES block 13 - round 11

	aese	v2.16b, v28.16b						//AES block 10 - round 11
	aese	v0.16b, v28.16b						//AES block 8 - round 11
	b.ge	L192_enc_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load plaintext

	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load plaintext

	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load plaintext

	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load plaintext

.long	0xce006908	//eor3 v8.16b, v8.16b, v0.16b, v26.16b				//AES block 0 - result
	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8

.long	0xce03696b	//eor3 v11.16b, v11.16b, v3.16b, v26.16b				//AES block 3 - result
.long	0xce016929	//eor3 v9.16b, v9.16b, v1.16b, v26.16b				//AES block 1 - result

	rev32	v1.16b, v30.16b				//CTR block 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 9
.long	0xce04698c	//eor3 v12.16b, v12.16b, v4.16b, v26.16b				//AES block 4 - result

.long	0xce0569ad	//eor3 v13.16b, v13.16b, v5.16b, v26.16b				//AES block 5 - result
.long	0xce0769ef	//eor3 v15.16b, v15.16b, v7.16b, v26.16b				//AES block 7 - result
	stp	q8, q9, [x2], #32			//AES block 0, 1 - store result

.long	0xce02694a	//eor3 v10.16b, v10.16b, v2.16b, v26.16b				//AES block 2 - result
	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10

	stp	q10, q11, [x2], #32			//AES block 2, 3 - store result
	cmp	x0, x5				//check if we have <= 8 blocks

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11
.long	0xce0669ce	//eor3 v14.16b, v14.16b, v6.16b, v26.16b				//AES block 6 - result

	stp	q12, q13, [x2], #32			//AES block 4, 5 - store result

	rev32	v4.16b, v30.16b				//CTR block 12
	stp	q14, q15, [x2], #32			//AES block 6, 7 - store result
	add	v30.4s, v30.4s, v31.4s		//CTR block 12

	b.ge	L192_enc_prepretail					//do prepretail

L192_enc_main_loop:	//main	loop start
	rev64	v12.16b, v12.16b						//GHASH block 8k+4 (t0, t1, and t2 free)
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	rev64	v10.16b, v10.16b						//GHASH block 8k+2

	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8

	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	rev64	v8.16b, v8.16b						//GHASH block 8k
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8

	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	rev64	v13.16b, v13.16b						//GHASH block 8k+5 (t0, t1, t2 and t3 free)

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	rev32	v7.16b, v30.16b				//CTR block 8k+15
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2

	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k - mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3

	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3

	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4

.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7 (t0, t1, t2 and t3 free)

	rev64	v14.16b, v14.16b						//GHASH block 8k+6 (t0, t1, and t2 free)
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5

	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high

	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	ldr	d16, [x10]			//MODULO - load modulo constant
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	ldp	q27, q28, [x8, #160]				//load rk10, rk11

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
	rev32	v20.16b, v30.16b					//CTR block 8k+16
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9
	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load plaintext

	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	rev32	v22.16b, v30.16b					//CTR block 8k+17
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	ldr	q26, [x8, #192]					//load rk12
	ext	v29.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load plaintext

	aese	v4.16b, v28.16b						//AES block 8k+12 - round 11
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	ldp	q12, q13, [x0], #32			//AES block 8k+12, 8k+13 - load plaintext

	ldp	q14, q15, [x0], #32			//AES block 8k+14, 8k+15 - load plaintext
	aese	v2.16b, v28.16b						//AES block 8k+10 - round 11
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10

	rev32	v23.16b, v30.16b					//CTR block 8k+18
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10
	aese	v5.16b, v28.16b						//AES block 8k+13 - round 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18

	aese	v7.16b, v28.16b						//AES block 8k+15 - round 11
	aese	v0.16b, v28.16b						//AES block 8k+8 - round 11
.long	0xce04698c	//eor3 v12.16b, v12.16b, v4.16b, v26.16b				//AES block 4 - result

	aese	v6.16b, v28.16b						//AES block 8k+14 - round 11
	aese	v3.16b, v28.16b						//AES block 8k+11 - round 11
	aese	v1.16b, v28.16b						//AES block 8k+9 - round 11

	rev32	v25.16b, v30.16b					//CTR block 8k+19
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19
.long	0xce0769ef	//eor3 v15.16b, v15.16b, v7.16b, v26.16b				//AES block 7 - result

.long	0xce02694a	//eor3 v10.16b, v10.16b, v2.16b, v26.16b				//AES block 8k+10 - result
.long	0xce006908	//eor3 v8.16b, v8.16b, v0.16b, v26.16b				//AES block 8k+8 - result
	mov	v2.16b, v23.16b					//CTR block 8k+18

.long	0xce016929	//eor3 v9.16b, v9.16b, v1.16b, v26.16b				//AES block 8k+9 - result
	mov	v1.16b, v22.16b					//CTR block 8k+17
	stp	q8, q9, [x2], #32			//AES block 8k+8, 8k+9 - store result
	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment

.long	0xce0669ce	//eor3 v14.16b, v14.16b, v6.16b, v26.16b				//AES block 6 - result
	mov	v0.16b, v20.16b					//CTR block 8k+16
	rev32	v4.16b, v30.16b				//CTR block 8k+20

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20
.long	0xce0569ad	//eor3 v13.16b, v13.16b, v5.16b, v26.16b				//AES block 5 - result
.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low

.long	0xce03696b	//eor3 v11.16b, v11.16b, v3.16b, v26.16b				//AES block 8k+11 - result
	mov	v3.16b, v25.16b					//CTR block 8k+19

	stp	q10, q11, [x2], #32			//AES block 8k+10, 8k+11 - store result

	stp	q12, q13, [x2], #32			//AES block 8k+12, 8k+13 - store result

	cmp	x0, x5				//LOOP CONTROL
	stp	q14, q15, [x2], #32			//AES block 8k+14, 8k+15 - store result
	b.lt	L192_enc_main_loop

L192_enc_prepretail:	//PREPRETAIL
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v8.16b, v8.16b						//GHASH block 8k
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev32	v7.16b, v30.16b				//CTR block 8k+15
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low

	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2
	rev64	v13.16b, v13.16b						//GHASH block 8k+5 (t0, t1, t2 and t3 free)
	rev64	v14.16b, v14.16b						//GHASH block 8k+6 (t0, t1, and t2 free)

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3

	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	rev64	v12.16b, v12.16b						//GHASH block 8k+4 (t0, t1, and t2 free)

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4

.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7 (t0, t1, t2 and t3 free)
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low

	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	ldr	d16, [x10]			//MODULO - load modulo constant
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ext	v29.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9
	ldp	q27, q28, [x8, #160]				//load rk10, rk11
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9

	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ldr	q26, [x8, #192]					//load rk12

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10

.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10

	aese	v1.16b, v28.16b						//AES block 8k+9 - round 11
	aese	v7.16b, v28.16b						//AES block 8k+15 - round 11

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v3.16b, v28.16b						//AES block 8k+11 - round 11

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15
	aese	v2.16b, v28.16b						//AES block 8k+10 - round 11
	aese	v0.16b, v28.16b						//AES block 8k+8 - round 11

	aese	v6.16b, v28.16b						//AES block 8k+14 - round 11
	aese	v4.16b, v28.16b						//AES block 8k+12 - round 11
	aese	v5.16b, v28.16b						//AES block 8k+13 - round 11

L192_enc_tail:	//TAIL

	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	sub	x5, x4, x0 	//main_end_input_ptr is number of bytes left to process

	ldr	q8, [x0], #16				//AES block 8k+8 - l3ad plaintext

	ldp	q24, q25, [x3, #192]			//load h8k | h7k
	ext	v25.16b, v25.16b, v25.16b, #8

	mov	v29.16b, v26.16b

	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8
	cmp	x5, #112

.long	0xce007509	//eor3 v9.16b, v8.16b, v0.16b, v29.16b			//AES block 8k+8 - result
	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag
	b.gt	L192_enc_blocks_more_than_7

	cmp	x5, #96
	mov	v7.16b, v6.16b
	movi	v17.8b, #0

	mov	v6.16b, v5.16b
	movi	v19.8b, #0
	sub	v30.4s, v30.4s, v31.4s

	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b
	mov	v3.16b, v2.16b

	mov	v2.16b, v1.16b
	movi	v18.8b, #0
	b.gt	L192_enc_blocks_more_than_6

	mov	v7.16b, v6.16b
	cmp	x5, #80

	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b

	mov	v3.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L192_enc_blocks_more_than_5

	cmp	x5, #64
	sub	v30.4s, v30.4s, v31.4s

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v1.16b
	b.gt	L192_enc_blocks_more_than_4

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	mov	v5.16b, v1.16b

	sub	v30.4s, v30.4s, v31.4s
	cmp	x5, #48
	b.gt	L192_enc_blocks_more_than_3

	mov	v7.16b, v6.16b
	mov	v6.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s

	ldr	q24, [x3, #96]				//load h4k | h3k
	cmp	x5, #32
	b.gt	L192_enc_blocks_more_than_2

	sub	v30.4s, v30.4s, v31.4s

	cmp	x5, #16
	mov	v7.16b, v1.16b
	b.gt	L192_enc_blocks_more_than_1

	sub	v30.4s, v30.4s, v31.4s
	ldr	q21, [x3, #48]				//load h2k | h1k
	b	L192_enc_blocks_less_than_1
L192_enc_blocks_more_than_7:	//blocks	left >  7
	st1	{ v9.16b}, [x2], #16			 	//AES final-7 block  - store result

	rev64	v8.16b, v9.16b						//GHASH final-7 block
	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid

	ldr	q9, [x0], #16				//AES final-6 block - load plaintext

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high

	pmull	v18.1q, v27.1d, v18.1d			 	//GHASH final-7 block - mid
.long	0xce017529	//eor3 v9.16b, v9.16b, v1.16b, v29.16b			//AES final-6 block - result
L192_enc_blocks_more_than_6:	//blocks	left >  6

	st1	{ v9.16b}, [x2], #16			 	//AES final-6 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-6 block

	ldr	q9, [x0], #16				//AES final-5 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low
.long	0xce027529	//eor3 v9.16b, v9.16b, v2.16b, v29.16b			//AES final-5 block - result

	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
L192_enc_blocks_more_than_5:	//blocks	left >  5

	st1	{ v9.16b}, [x2], #16			 	//AES final-5 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid

	ldr	q9, [x0], #16				//AES final-4 block - load plaintext
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high

	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low
	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid

.long	0xce037529	//eor3 v9.16b, v9.16b, v3.16b, v29.16b			//AES final-4 block - result
	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
L192_enc_blocks_more_than_4:	//blocks	left >  4

	st1	{ v9.16b}, [x2], #16				//AES final-4 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ldr	q9, [x0], #16				//AES final-3 block - load plaintext
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high
	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low

	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
.long	0xce047529	//eor3 v9.16b, v9.16b, v4.16b, v29.16b			//AES final-3 block - result
L192_enc_blocks_more_than_3:	//blocks	left >  3

	ldr	q24, [x3, #96]				//load h4k | h3k
	st1	{ v9.16b}, [x2], #16			 	//AES final-3 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-3 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	movi	v16.8b, #0						//suppress further partial tag feed in

	ldr	q9, [x0], #16				//AES final-2 block - load plaintext
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid

.long	0xce057529	//eor3 v9.16b, v9.16b, v5.16b, v29.16b			//AES final-2 block - result
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low

	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high
	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high
L192_enc_blocks_more_than_2:	//blocks	left >  2

	st1	{ v9.16b}, [x2], #16			 	//AES final-2 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-2 block
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ldr	q9, [x0], #16				//AES final-1 block - load plaintext
	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid
.long	0xce067529	//eor3 v9.16b, v9.16b, v6.16b, v29.16b			//AES final-1 block - result
L192_enc_blocks_more_than_1:	//blocks	left >  1

	ldr	q22, [x3, #64]				//load h1l | h1h
	ext	v22.16b, v22.16b, v22.16b, #8
	st1	{ v9.16b}, [x2], #16			 	//AES final-1 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-1 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid

	ldr	q9, [x0], #16				//AES final block - load plaintext
	ldr	q21, [x3, #48]				//load h2k | h1k

	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid

.long	0xce077529	//eor3 v9.16b, v9.16b, v7.16b, v29.16b			//AES final block - result
	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high
L192_enc_blocks_less_than_1:	//blocks	left <= 1

	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff
	and	x1, x1, #127				//bit_length %= 128

	sub	x1, x1, #128				//bit_length -= 128

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127				//bit_length %= 128

	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	cmp	x1, #64
	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff

	csel	x13, x7, x6, lt
	csel	x14, x6, xzr, lt

	mov	v0.d[1], x14
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8

	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored
	mov	v0.d[0], x13					//ctr0b is mask for last block

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits

	rev64	v8.16b, v9.16b						//GHASH final block
	bif	v9.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing

	st1	{ v9.16b}, [x2]				//store all 16B

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v16.d[0], v8.d[1]					//GHASH final block - mid
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high

	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high
	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low

	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid

	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment

	rev32	v30.16b, v30.16b

	str	q30, [x16]					//store the updated counter
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up

	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment

.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]

	mov	x0, x9					//return sizes

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret

L192_enc_ret:
	mov	w0, #0x0
	ret

.globl	_unroll8_eor3_aes_gcm_dec_192_kernel

.align	4
_unroll8_eor3_aes_gcm_dec_192_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L192_dec_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	mov	x5, x9
	ld1	{ v0.16b}, [x16]					//CTR block 0
	ld1	{ v19.16b}, [x3]

	mov	x15, #0x100000000			//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15

	rev32	v30.16b, v0.16b				//set up reversed counter

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6

	rev32	v7.16b, v30.16b				//CTR block 7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 0

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 1

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 1

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5

	sub	x5, x5, #1		//byte_len - 1

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	add	v30.4s, v30.4s, v31.4s		//CTR block 7

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 8
	and	x5, x5, #0xffffffffffffff80	//number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 8

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 8

	add	x4, x0, x1, lsr #3		//end_input_ptr
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 9

	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b

	ldp	q27, q28, [x8, #160]				//load rk10, rk11

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 9
	add	x5, x5, x0

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 9
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 9

	cmp	x0, x5				//check if we have <= 8 blocks
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 9

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 9
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 9

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 10

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 10

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 10
	ldr	q26, [x8, #192]					//load rk12

	aese	v0.16b, v28.16b						//AES block 0 - round 11
	aese	v1.16b, v28.16b						//AES block 1 - round 11
	aese	v4.16b, v28.16b						//AES block 4 - round 11

	aese	v6.16b, v28.16b						//AES block 6 - round 11
	aese	v5.16b, v28.16b						//AES block 5 - round 11
	aese	v7.16b, v28.16b						//AES block 7 - round 11

	aese	v2.16b, v28.16b						//AES block 2 - round 11
	aese	v3.16b, v28.16b						//AES block 3 - round 11
	b.ge	L192_dec_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load ciphertext

	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load ciphertext

	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load ciphertext

.long	0xce016921	//eor3 v1.16b, v9.16b, v1.16b, v26.16b				//AES block 1 - result
.long	0xce006900	//eor3 v0.16b, v8.16b, v0.16b, v26.16b				//AES block 0 - result
	stp	q0, q1, [x2], #32			//AES block 0, 1 - store result

	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8

	rev32	v1.16b, v30.16b				//CTR block 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 9
.long	0xce036963	//eor3 v3.16b, v11.16b, v3.16b, v26.16b				//AES block 3 - result

.long	0xce026942	//eor3 v2.16b, v10.16b, v2.16b, v26.16b				//AES block 2 - result
	stp	q2, q3, [x2], #32			//AES block 2, 3 - store result
	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load ciphertext

	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10

.long	0xce046984	//eor3 v4.16b, v12.16b, v4.16b, v26.16b				//AES block 4 - result

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11

.long	0xce0569a5	//eor3 v5.16b, v13.16b, v5.16b, v26.16b				//AES block 5 - result
	stp	q4, q5, [x2], #32			//AES block 4, 5 - store result
	cmp	x0, x5				//check if we have <= 8 blocks

.long	0xce0669c6	//eor3 v6.16b, v14.16b, v6.16b, v26.16b				//AES block 6 - result
.long	0xce0769e7	//eor3 v7.16b, v15.16b, v7.16b, v26.16b				//AES block 7 - result
	rev32	v4.16b, v30.16b				//CTR block 12

	add	v30.4s, v30.4s, v31.4s		//CTR block 12
	stp	q6, q7, [x2], #32			//AES block 6, 7 - store result
	b.ge	L192_dec_prepretail					//do prepretail

L192_dec_main_loop:	//main	loop start
	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	rev64	v8.16b, v8.16b						//GHASH block 8k
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	rev64	v11.16b, v11.16b						//GHASH block 8k+3

	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	rev64	v13.16b, v13.16b						//GHASH block 8k+5

	rev32	v7.16b, v30.16b				//CTR block 8k+15
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0

	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high

	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5

	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	rev64	v14.16b, v14.16b						//GHASH block 8k+6

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high

	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	ldr	d16, [x10]			//MODULO - load modulo constant
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
	rev32	v20.16b, v30.16b					//CTR block 8k+16
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9
	ldp	q27, q28, [x8, #160]				//load rk10, rk11

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load ciphertext

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load ciphertext

	rev32	v22.16b, v30.16b					//CTR block 8k+17
	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	ldp	q12, q13, [x0], #32			//AES block 8k+12, 8k+13 - load ciphertext

	rev32	v23.16b, v30.16b					//CTR block 8k+18
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18
.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	ldr	q26, [x8, #192]					//load rk12

	ldp	q14, q15, [x0], #32			//AES block 8k+14, 8k+15 - load ciphertext
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10

	aese	v0.16b, v28.16b						//AES block 8k+8 - round 11
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment
	aese	v1.16b, v28.16b						//AES block 8k+9 - round 11

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	aese	v6.16b, v28.16b						//AES block 8k+14 - round 11
	aese	v3.16b, v28.16b						//AES block 8k+11 - round 11

.long	0xce006900	//eor3 v0.16b, v8.16b, v0.16b, v26.16b				//AES block 8k+8 - result
	rev32	v25.16b, v30.16b					//CTR block 8k+19
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10

	aese	v4.16b, v28.16b						//AES block 8k+12 - round 11
	aese	v2.16b, v28.16b						//AES block 8k+10 - round 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19

	aese	v7.16b, v28.16b						//AES block 8k+15 - round 11
	aese	v5.16b, v28.16b						//AES block 8k+13 - round 11
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low

.long	0xce016921	//eor3 v1.16b, v9.16b, v1.16b, v26.16b				//AES block 8k+9 - result
	stp	q0, q1, [x2], #32			//AES block 8k+8, 8k+9 - store result
.long	0xce036963	//eor3 v3.16b, v11.16b, v3.16b, v26.16b				//AES block 8k+11 - result

.long	0xce026942	//eor3 v2.16b, v10.16b, v2.16b, v26.16b				//AES block 8k+10 - result
.long	0xce0769e7	//eor3 v7.16b, v15.16b, v7.16b, v26.16b				//AES block 8k+15 - result
	stp	q2, q3, [x2], #32			//AES block 8k+10, 8k+11 - store result

.long	0xce0569a5	//eor3 v5.16b, v13.16b, v5.16b, v26.16b				//AES block 8k+13 - result
.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	mov	v3.16b, v25.16b					//CTR block 8k+19

.long	0xce046984	//eor3 v4.16b, v12.16b, v4.16b, v26.16b				//AES block 8k+12 - result
	stp	q4, q5, [x2], #32			//AES block 8k+12, 8k+13 - store result
	cmp	x0, x5				//LOOP CONTROL

.long	0xce0669c6	//eor3 v6.16b, v14.16b, v6.16b, v26.16b				//AES block 8k+14 - result
	stp	q6, q7, [x2], #32			//AES block 8k+14, 8k+15 - store result
	mov	v0.16b, v20.16b					//CTR block 8k+16

	mov	v1.16b, v22.16b					//CTR block 8k+17
	mov	v2.16b, v23.16b					//CTR block 8k+18

	rev32	v4.16b, v30.16b				//CTR block 8k+20
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20
	b.lt	L192_dec_main_loop

L192_dec_prepretail:	//PREPRETAIL
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v8.16b, v8.16b						//GHASH block 8k
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0

	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev32	v7.16b, v30.16b				//CTR block 8k+15

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1

	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	rev64	v13.16b, v13.16b						//GHASH block 8k+5
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2

	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3

	rev64	v15.16b, v15.16b						//GHASH block 8k+7

.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	rev64	v12.16b, v12.16b						//GHASH block 8k+4

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4

	rev64	v14.16b, v14.16b						//GHASH block 8k+6
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5

	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high

	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5

	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7

	ldr	d16, [x10]			//MODULO - load modulo constant
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	ldp	q27, q28, [x8, #160]				//load rk10, rk11

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ldr	q26, [x8, #192]					//load rk12
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10

	aese	v0.16b, v28.16b						//AES block 8k+8 - round 11
.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	aese	v5.16b, v28.16b						//AES block 8k+13 - round 11

	aese	v2.16b, v28.16b						//AES block 8k+10 - round 11
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10

	aese	v6.16b, v28.16b						//AES block 8k+14 - round 11
	aese	v4.16b, v28.16b						//AES block 8k+12 - round 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	aese	v3.16b, v28.16b						//AES block 8k+11 - round 11
	aese	v1.16b, v28.16b						//AES block 8k+9 - round 11
	aese	v7.16b, v28.16b						//AES block 8k+15 - round 11

L192_dec_tail:	//TAIL

	sub	x5, x4, x0 	//main_end_input_ptr is number of bytes left to process

	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q9, [x0], #16				//AES block 8k+8 - load ciphertext

	ldp	q24, q25, [x3, #192]			//load h8k | h7k
	ext	v25.16b, v25.16b, v25.16b, #8

	mov	v29.16b, v26.16b

	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8
	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag

.long	0xce00752c	//eor3 v12.16b, v9.16b, v0.16b, v29.16b				//AES block 8k+8 - result
	cmp	x5, #112
	b.gt	L192_dec_blocks_more_than_7

	mov	v7.16b, v6.16b
	movi	v17.8b, #0
	sub	v30.4s, v30.4s, v31.4s

	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b

	cmp	x5, #96
	movi	v19.8b, #0
	mov	v3.16b, v2.16b

	mov	v2.16b, v1.16b
	movi	v18.8b, #0
	b.gt	L192_dec_blocks_more_than_6

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	mov	v4.16b, v3.16b
	mov	v3.16b, v1.16b

	sub	v30.4s, v30.4s, v31.4s
	cmp	x5, #80
	b.gt	L192_dec_blocks_more_than_5

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v4.16b
	mov	v4.16b, v1.16b
	cmp	x5, #64

	sub	v30.4s, v30.4s, v31.4s
	b.gt	L192_dec_blocks_more_than_4

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v1.16b
	cmp	x5, #48
	b.gt	L192_dec_blocks_more_than_3

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	cmp	x5, #32

	mov	v6.16b, v1.16b
	ldr	q24, [x3, #96]				//load h4k | h3k
	b.gt	L192_dec_blocks_more_than_2

	sub	v30.4s, v30.4s, v31.4s

	mov	v7.16b, v1.16b
	cmp	x5, #16
	b.gt	L192_dec_blocks_more_than_1

	sub	v30.4s, v30.4s, v31.4s
	ldr	q21, [x3, #48]				//load h2k | h1k
	b	L192_dec_blocks_less_than_1
L192_dec_blocks_more_than_7:	//blocks	left >  7
	rev64	v8.16b, v9.16b						//GHASH final-7 block

	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid
	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high
	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid
	ldr	q9, [x0], #16				//AES final-6 block - load ciphertext

	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid
	st1	{ v12.16b}, [x2], #16			 	//AES final-7 block  - store result

.long	0xce01752c	//eor3 v12.16b, v9.16b, v1.16b, v29.16b				//AES final-6 block - result

	pmull	v18.1q, v27.1d, v18.1d			 	//GHASH final-7 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
L192_dec_blocks_more_than_6:	//blocks	left >  6

	rev64	v8.16b, v9.16b						//GHASH final-6 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ldr	q9, [x0], #16				//AES final-5 block - load ciphertext
	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high

	st1	{ v12.16b}, [x2], #16			 	//AES final-6 block - store result
.long	0xce02752c	//eor3 v12.16b, v9.16b, v2.16b, v29.16b				//AES final-5 block - result

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high
	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid
	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low
L192_dec_blocks_more_than_5:	//blocks	left >  5

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high

	ldr	q9, [x0], #16				//AES final-4 block - load ciphertext

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low
	movi	v16.8b, #0						//suppress further partial tag feed in
	st1	{ v12.16b}, [x2], #16			 	//AES final-5 block - store result

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
.long	0xce03752c	//eor3 v12.16b, v9.16b, v3.16b, v29.16b				//AES final-4 block - result
L192_dec_blocks_more_than_4:	//blocks	left >  4

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	movi	v16.8b, #0						//suppress further partial tag feed in

	ldr	q9, [x0], #16				//AES final-3 block - load ciphertext
	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid
	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low

	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid
	st1	{ v12.16b}, [x2], #16			 	//AES final-4 block - store result
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high

.long	0xce04752c	//eor3 v12.16b, v9.16b, v4.16b, v29.16b				//AES final-3 block - result

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high
L192_dec_blocks_more_than_3:	//blocks	left >  3

	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v8.16b, v9.16b						//GHASH final-3 block
	ldr	q9, [x0], #16				//AES final-2 block - load ciphertext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid
	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high
	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low

	st1	{ v12.16b}, [x2], #16			 	//AES final-3 block - store result
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid
.long	0xce05752c	//eor3 v12.16b, v9.16b, v5.16b, v29.16b				//AES final-2 block - result

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low
	ldr	q24, [x3, #96]				//load h4k | h3k

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid

	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
L192_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v8.16b, v9.16b						//GHASH final-2 block
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid
	ldr	q9, [x0], #16				//AES final-1 block - load ciphertext

	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high
	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low
	st1	{ v12.16b}, [x2], #16			 	//AES final-2 block - store result

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid
.long	0xce06752c	//eor3 v12.16b, v9.16b, v6.16b, v29.16b				//AES final-1 block - result
L192_dec_blocks_more_than_1:	//blocks	left >  1

	rev64	v8.16b, v9.16b						//GHASH final-1 block
	ldr	q9, [x0], #16				//AES final block - load ciphertext
	ldr	q22, [x3, #64]				//load h1l | h1h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	movi	v16.8b, #0						//suppress further partial tag feed in
	ldr	q21, [x3, #48]				//load h2k | h1k

	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low
	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid
	st1	{ v12.16b}, [x2], #16			 	//AES final-1 block - store result

	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high

.long	0xce07752c	//eor3 v12.16b, v9.16b, v7.16b, v29.16b				//AES final block - result

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high
L192_dec_blocks_less_than_1:	//blocks	left <= 1

	rev32	v30.16b, v30.16b
	and	x1, x1, #127				//bit_length %= 128

	sub	x1, x1, #128				//bit_length -= 128
	str	q30, [x16]					//store the updated counter

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])
	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff

	and	x1, x1, #127				//bit_length %= 128

	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff
	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	cmp	x1, #64

	csel	x13, x7, x6, lt
	csel	x14, x6, xzr, lt
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8

	mov	v0.d[1], x14
	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored

	mov	v0.d[0], x13					//ctr0b is mask for last block

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits
	bif	v12.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing

	rev64	v8.16b, v9.16b						//GHASH final block

	st1	{ v12.16b}, [x2]				//store all 16B

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v16.d[0], v8.d[1]					//GHASH final block - mid
	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low

	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high
	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high

	eor	v14.16b, v17.16b, v19.16b				//MODULO - karatsuba tidy up
	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

	pmull	v21.1q, v17.1d, v16.1d			//MODULO - top 64b align with mid
	ext	v17.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

	eor	v18.16b, v18.16b, v14.16b				//MODULO - karatsuba tidy up

.long	0xce115652	//eor3 v18.16b, v18.16b, v17.16b, v21.16b			//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ext	v18.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment

.long	0xce124673	//eor3 v19.16b, v19.16b, v18.16b, v17.16b			//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]

	mov	x0, x9

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret

L192_dec_ret:
	mov	w0, #0x0
	ret

.globl	_unroll8_eor3_aes_gcm_enc_256_kernel

.align	4
_unroll8_eor3_aes_gcm_enc_256_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L256_enc_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	ld1	{ v0.16b}, [x16]					//CTR block 0

	mov	x5, x9

	mov	x15, #0x100000000			//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15
	sub	x5, x5, #1		//byte_len - 1

	and	x5, x5, #0xffffffffffffff80	//number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	add	x5, x5, x0

	rev32	v30.16b, v0.16b				//set up reversed counter

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5
	ldp	q26, q27, [x8, #0]				 	//load rk0, rk1

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6

	rev32	v7.16b, v30.16b				//CTR block 7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 0

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 1

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 8

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 8

	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	ldp	q27, q28, [x8, #160]				//load rk10, rk11

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 9
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 9

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 9
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 9
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 9

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 9

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 10
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 10
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 9

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 10

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 10

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 11
	ldp	q26, q27, [x8, #192]				//load rk12, rk13
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 11

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 11
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 11
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 11

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 11
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 11
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 11

	add	v30.4s, v30.4s, v31.4s		//CTR block 7
	ldr	q28, [x8, #224]					//load rk14

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 12
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 12
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 12

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 12
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 12
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 12

	aese	v2.16b, v27.16b						//AES block 2 - round 13
	aese	v1.16b, v27.16b						//AES block 1 - round 13
	aese	v4.16b, v27.16b						//AES block 4 - round 13

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 12
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 12

	aese	v0.16b, v27.16b						//AES block 0 - round 13
	aese	v5.16b, v27.16b						//AES block 5 - round 13

	aese	v6.16b, v27.16b						//AES block 6 - round 13
	aese	v7.16b, v27.16b						//AES block 7 - round 13
	aese	v3.16b, v27.16b						//AES block 3 - round 13

	add	x4, x0, x1, lsr #3		//end_input_ptr
	cmp	x0, x5				//check if we have <= 8 blocks
	b.ge	L256_enc_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load plaintext

	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load plaintext

.long	0xce007108	//eor3 v8.16b, v8.16b, v0.16b, v28.16b				//AES block 0 - result
	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8

.long	0xce017129	//eor3 v9.16b, v9.16b, v1.16b, v28.16b				//AES block 1 - result
.long	0xce03716b	//eor3 v11.16b, v11.16b, v3.16b, v28.16b				//AES block 3 - result

	rev32	v1.16b, v30.16b				//CTR block 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 9
	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load plaintext

	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load plaintext
.long	0xce02714a	//eor3 v10.16b, v10.16b, v2.16b, v28.16b				//AES block 2 - result
	cmp	x0, x5				//check if we have <= 8 blocks

	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10
	stp	q8, q9, [x2], #32			//AES block 0, 1 - store result

	stp	q10, q11, [x2], #32			//AES block 2, 3 - store result

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11

.long	0xce04718c	//eor3 v12.16b, v12.16b, v4.16b, v28.16b				//AES block 4 - result

.long	0xce0771ef	//eor3 v15.16b, v15.16b, v7.16b, v28.16b				//AES block 7 - result
.long	0xce0671ce	//eor3 v14.16b, v14.16b, v6.16b, v28.16b				//AES block 6 - result
.long	0xce0571ad	//eor3 v13.16b, v13.16b, v5.16b, v28.16b				//AES block 5 - result

	stp	q12, q13, [x2], #32			//AES block 4, 5 - store result
	rev32	v4.16b, v30.16b				//CTR block 12

	stp	q14, q15, [x2], #32			//AES block 6, 7 - store result
	add	v30.4s, v30.4s, v31.4s		//CTR block 12
	b.ge	L256_enc_prepretail					//do prepretail

L256_enc_main_loop:	//main	loop start
	ldp	q26, q27, [x8, #0]					//load rk0, rk1

	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14
	rev64	v8.16b, v8.16b						//GHASH block 8k

	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	rev32	v7.16b, v30.16b				//CTR block 8k+15

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	eor	v8.16b, v8.16b, v19.16b				 	//PRE 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	rev64	v14.16b, v14.16b						//GHASH block 8k+6
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	rev64	v10.16b, v10.16b						//GHASH block 8k+2

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3

	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	rev64	v13.16b, v13.16b						//GHASH block 8k+5

	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4

	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4

	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5

	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5

	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6

	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low

	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7

	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8

	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8

.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9

	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9

	ldp	q27, q28, [x8, #160]				//load rk10, rk11
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	ldr	d16, [x10]			//MODULO - load modulo constant
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9

.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high

	ldp	q26, q27, [x8, #192]				//load rk12, rk13
	rev32	v20.16b, v30.16b					//CTR block 8k+16

	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment
	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load plaintext
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 11

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 11

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 11
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 11

	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 11

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 12
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 11

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 12
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 12
	rev32	v22.16b, v30.16b					//CTR block 8k+17

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 11
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 12
	ldr	q28, [x8, #224]					//load rk14
	aese	v7.16b, v27.16b						//AES block 8k+15 - round 13

	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load plaintext
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 12
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 12

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 12
	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load plaintext

	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load plaintext
	aese	v2.16b, v27.16b						//AES block 8k+10 - round 13
	aese	v4.16b, v27.16b						//AES block 8k+12 - round 13

	rev32	v23.16b, v30.16b					//CTR block 8k+18
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18
	aese	v5.16b, v27.16b						//AES block 8k+13 - round 13

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 12
	aese	v3.16b, v27.16b						//AES block 8k+11 - round 13
	cmp	x0, x5				//LOOP CONTROL

.long	0xce02714a	//eor3 v10.16b, v10.16b, v2.16b, v28.16b				//AES block 8k+10 - result
	rev32	v25.16b, v30.16b					//CTR block 8k+19
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19

	aese	v0.16b, v27.16b						//AES block 8k+8 - round 13
	aese	v6.16b, v27.16b						//AES block 8k+14 - round 13
.long	0xce0571ad	//eor3 v13.16b, v13.16b, v5.16b, v28.16b				//AES block 5 - result

	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v1.16b, v27.16b						//AES block 8k+9 - round 13

.long	0xce04718c	//eor3 v12.16b, v12.16b, v4.16b, v28.16b				//AES block 4 - result
	rev32	v4.16b, v30.16b				//CTR block 8k+20
.long	0xce03716b	//eor3 v11.16b, v11.16b, v3.16b, v28.16b				//AES block 8k+11 - result

	mov	v3.16b, v25.16b					//CTR block 8k+19
.long	0xce017129	//eor3 v9.16b, v9.16b, v1.16b, v28.16b				//AES block 8k+9 - result
.long	0xce007108	//eor3 v8.16b, v8.16b, v0.16b, v28.16b				//AES block 8k+8 - result

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20
	stp	q8, q9, [x2], #32			//AES block 8k+8, 8k+9 - store result
	mov	v2.16b, v23.16b					//CTR block 8k+18

.long	0xce0771ef	//eor3 v15.16b, v15.16b, v7.16b, v28.16b				//AES block 7 - result
.long	0xce154673	//eor3 v19.16b, v19.16b, v21.16b, v17.16b		 	//MODULO - fold into low
	stp	q10, q11, [x2], #32			//AES block 8k+10, 8k+11 - store result

.long	0xce0671ce	//eor3 v14.16b, v14.16b, v6.16b, v28.16b				//AES block 6 - result
	mov	v1.16b, v22.16b					//CTR block 8k+17
	stp	q12, q13, [x2], #32			//AES block 4, 5 - store result

	stp	q14, q15, [x2], #32			//AES block 6, 7 - store result
	mov	v0.16b, v20.16b					//CTR block 8k+16
	b.lt	L256_enc_main_loop

L256_enc_prepretail:	//PREPRETAIL
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldp	q26, q27, [x8, #0]					//load rk0, rk1
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	rev64	v10.16b, v10.16b						//GHASH block 8k+2

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	rev64	v13.16b, v13.16b						//GHASH block 8k+5
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	rev32	v7.16b, v30.16b				//CTR block 8k+15

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0

	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	rev64	v8.16b, v8.16b						//GHASH block 8k
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1

	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1

	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1

	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	eor	v8.16b, v8.16b, v19.16b					//PRE 1

	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	rev64	v14.16b, v14.16b						//GHASH block 8k+6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high

	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3

	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5
	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4

	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid

	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	rev64	v15.16b, v15.16b						//GHASH block 8k+7
	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6

	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8

	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7

	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7

	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8

	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high
	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid

	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high

	ldp	q27, q28, [x8, #160]				//load rk10, rk11
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10

	pmull	v29.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 11

	ldp	q26, q27, [x8, #192]				//load rk12, rk13
	ext	v21.16b, v17.16b, v17.16b, #8			 	//MODULO - other top alignment
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 11

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 11
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 11

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 11
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 11
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 11

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 11
	ldr	q28, [x8, #224]					//load rk14

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 12
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 12
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 12

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 12
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 12
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 12
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 12
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 12
	aese	v0.16b, v27.16b						//AES block 8k+8 - round 13

.long	0xce154673	//eor3 v19.16b, v19.16b, v21.16b, v17.16b		 	//MODULO - fold into low
	aese	v5.16b, v27.16b						//AES block 8k+13 - round 13
	aese	v1.16b, v27.16b						//AES block 8k+9 - round 13

	aese	v3.16b, v27.16b						//AES block 8k+11 - round 13
	aese	v4.16b, v27.16b						//AES block 8k+12 - round 13
	aese	v7.16b, v27.16b						//AES block 8k+15 - round 13

	aese	v2.16b, v27.16b						//AES block 8k+10 - round 13
	aese	v6.16b, v27.16b						//AES block 8k+14 - round 13
L256_enc_tail:	//TAIL

	ldp	q24, q25, [x3, #192]			//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	sub	x5, x4, x0		//main_end_input_ptr is number of bytes left to process

	ldr	q8, [x0], #16				//AES block 8k+8 - load plaintext

	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8

	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag
	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8
	mov	v29.16b, v28.16b

	cmp	x5, #112
.long	0xce007509	//eor3 v9.16b, v8.16b, v0.16b, v29.16b				//AES block 8k+8 - result
	b.gt	L256_enc_blocks_more_than_7

	movi	v19.8b, #0
	mov	v7.16b, v6.16b
	movi	v17.8b, #0

	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b

	mov	v3.16b, v2.16b
	sub	v30.4s, v30.4s, v31.4s
	mov	v2.16b, v1.16b

	movi	v18.8b, #0
	cmp	x5, #96
	b.gt	L256_enc_blocks_more_than_6

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b
	cmp	x5, #80

	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b
	mov	v3.16b, v1.16b

	sub	v30.4s, v30.4s, v31.4s
	b.gt	L256_enc_blocks_more_than_5

	mov	v7.16b, v6.16b
	sub	v30.4s, v30.4s, v31.4s

	mov	v6.16b, v5.16b
	mov	v5.16b, v4.16b

	cmp	x5, #64
	mov	v4.16b, v1.16b
	b.gt	L256_enc_blocks_more_than_4

	cmp	x5, #48
	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L256_enc_blocks_more_than_3

	cmp	x5, #32
	mov	v7.16b, v6.16b
	ldr	q24, [x3, #96]				//load h4k | h3k

	mov	v6.16b, v1.16b
	sub	v30.4s, v30.4s, v31.4s
	b.gt	L256_enc_blocks_more_than_2

	mov	v7.16b, v1.16b

	sub	v30.4s, v30.4s, v31.4s
	cmp	x5, #16
	b.gt	L256_enc_blocks_more_than_1

	sub	v30.4s, v30.4s, v31.4s
	ldr	q21, [x3, #48]				//load h2k | h1k
	b	L256_enc_blocks_less_than_1
L256_enc_blocks_more_than_7:	//blocks	left >  7
	st1	{ v9.16b}, [x2], #16				//AES final-7 block  - store result

	rev64	v8.16b, v9.16b						//GHASH final-7 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ldr	q9, [x0], #16				//AES final-6 block - load plaintext

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high
	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid
	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid
.long	0xce017529	//eor3 v9.16b, v9.16b, v1.16b, v29.16b			//AES final-6 block - result

	pmull	v18.1q, v27.1d, v18.1d				//GHASH final-7 block - mid
	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low
L256_enc_blocks_more_than_6:	//blocks	left >  6

	st1	{ v9.16b}, [x2], #16				//AES final-6 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-6 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low
	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high

	ldr	q9, [x0], #16				//AES final-5 block - load plaintext

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid
.long	0xce027529	//eor3 v9.16b, v9.16b, v2.16b, v29.16b			//AES final-5 block - result

	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high
L256_enc_blocks_more_than_5:	//blocks	left >  5

	st1	{ v9.16b}, [x2], #16				//AES final-5 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid

	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid

	ldr	q9, [x0], #16				//AES final-4 block - load plaintext
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
.long	0xce037529	//eor3 v9.16b, v9.16b, v3.16b, v29.16b			//AES final-4 block - result
L256_enc_blocks_more_than_4:	//blocks	left >  4

	st1	{ v9.16b}, [x2], #16				//AES final-4 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	ldr	q9, [x0], #16				//AES final-3 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high

.long	0xce047529	//eor3 v9.16b, v9.16b, v4.16b, v29.16b			//AES final-3 block - result
	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low

	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high
L256_enc_blocks_more_than_3:	//blocks	left >  3

	st1	{ v9.16b}, [x2], #16				//AES final-3 block - store result

	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v8.16b, v9.16b						//GHASH final-3 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid
	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid
	ldr	q24, [x3, #96]				//load h4k | h3k

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid
	ldr	q9, [x0], #16				//AES final-2 block - load plaintext

	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low

.long	0xce057529	//eor3 v9.16b, v9.16b, v5.16b, v29.16b			//AES final-2 block - result
	movi	v16.8b, #0						//suppress further partial tag feed in

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low
L256_enc_blocks_more_than_2:	//blocks	left >  2

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8

	st1	{ v9.16b}, [x2], #16			 	//AES final-2 block - store result

	rev64	v8.16b, v9.16b						//GHASH final-2 block
	ldr	q9, [x0], #16				//AES final-1 block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high
.long	0xce067529	//eor3 v9.16b, v9.16b, v6.16b, v29.16b			//AES final-1 block - result

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid
	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low
L256_enc_blocks_more_than_1:	//blocks	left >  1

	st1	{ v9.16b}, [x2], #16				//AES final-1 block - store result

	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	rev64	v8.16b, v9.16b						//GHASH final-1 block
	ldr	q9, [x0], #16				//AES final block - load plaintext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	movi	v16.8b, #0						//suppress further partial tag feed in

	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high

.long	0xce077529	//eor3 v9.16b, v9.16b, v7.16b, v29.16b			//AES final block - result
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high

	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid

	ldr	q21, [x3, #48]				//load h2k | h1k

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low
	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
L256_enc_blocks_less_than_1:	//blocks	left <= 1

	and	x1, x1, #127				//bit_length %= 128

	sub	x1, x1, #128				//bit_length -= 128

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])

	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff
	and	x1, x1, #127				//bit_length %= 128

	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	cmp	x1, #64
	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff

	csel	x14, x6, xzr, lt
	csel	x13, x7, x6, lt

	mov	v0.d[0], x13					//ctr0b is mask for last block
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8

	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored
	mov	v0.d[1], x14

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits

	rev64	v8.16b, v9.16b						//GHASH final block

	rev32	v30.16b, v30.16b
	bif	v9.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing
	str	q30, [x16]					//store the updated counter

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	st1	{ v9.16b}, [x2]				//store all 16B

	ins	v16.d[0], v8.d[1]					//GHASH final block - mid
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high
	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low

	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high
	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low

	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid

	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant

	ext	v21.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	pmull	v29.1q, v17.1d, v16.1d			//MODULO - top 64b align with mid

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment

.long	0xce115673	//eor3 v19.16b, v19.16b, v17.16b, v21.16b		 	//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]
	mov	x0, x9					//return sizes

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret

L256_enc_ret:
	mov	w0, #0x0
	ret

.globl	_unroll8_eor3_aes_gcm_dec_256_kernel

.align	4
_unroll8_eor3_aes_gcm_dec_256_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L256_dec_ret
	stp	d8, d9, [sp, #-80]!
	lsr	x9, x1, #3
	mov	x16, x4
	mov	x8, x5
	stp	d10, d11, [sp, #16]
	stp	d12, d13, [sp, #32]
	stp	d14, d15, [sp, #48]
	mov	x5, #0xc200000000000000
	stp	x5, xzr, [sp, #64]
	add	x10, sp, #64

	ld1	{ v0.16b}, [x16]					//CTR block 0

	mov	x15, #0x100000000			//set up counter increment
	movi	v31.16b, #0x0
	mov	v31.d[1], x15
	mov	x5, x9

	sub	x5, x5, #1		//byte_len - 1

	rev32	v30.16b, v0.16b				//set up reversed counter

	add	v30.4s, v30.4s, v31.4s		//CTR block 0

	rev32	v1.16b, v30.16b				//CTR block 1
	add	v30.4s, v30.4s, v31.4s		//CTR block 1

	rev32	v2.16b, v30.16b				//CTR block 2
	add	v30.4s, v30.4s, v31.4s		//CTR block 2
	ldp	q26, q27, [x8, #0]				  	//load rk0, rk1

	rev32	v3.16b, v30.16b				//CTR block 3
	add	v30.4s, v30.4s, v31.4s		//CTR block 3

	rev32	v4.16b, v30.16b				//CTR block 4
	add	v30.4s, v30.4s, v31.4s		//CTR block 4

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 0

	rev32	v5.16b, v30.16b				//CTR block 5
	add	v30.4s, v30.4s, v31.4s		//CTR block 5

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 0

	rev32	v6.16b, v30.16b				//CTR block 6
	add	v30.4s, v30.4s, v31.4s		//CTR block 6

	rev32	v7.16b, v30.16b				//CTR block 7
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 0

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b		        //AES block 6 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 0
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b		        //AES block 7 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b		        //AES block 6 - round 1
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b		        //AES block 4 - round 1
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b		        //AES block 0 - round 1

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 1
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 1

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 1
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 1

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 2

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 2
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 2

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 2
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 2
	ldp	q27, q28, [x8, #64]				//load rk4, rk5

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 3
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 3

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 3
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 3

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 3
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 3
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 3

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 3

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 4
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 4

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 4
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 4

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 4
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 4

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 5

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 5

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 5

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 5

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 5

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 6

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 6
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 6
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 7
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 7

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 7
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 7

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 7
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 7

	and	x5, x5, #0xffffffffffffff80 //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 8

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 8
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 8
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 8

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 8
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 8

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 9

	ld1	{ v19.16b}, [x3]
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	ldp	q27, q28, [x8, #160]				//load rk10, rk11
	add	x4, x0, x1, lsr #3 //end_input_ptr
	add	x5, x5, x0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 9
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 9

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 9
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 9

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 9

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 9

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 10

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 10

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 10
	ldp	q26, q27, [x8, #192]				//load rk12, rk13

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 11
	add	v30.4s, v30.4s, v31.4s //CTR block 7

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 11
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 11
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 11

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 11
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 11
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 11

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 11
	ldr	q28, [x8, #224]					//load rk14

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 1 - round 12
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 4 - round 12
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 5 - round 12

	cmp	x0, x5				//check if we have <= 8 blocks
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 3 - round 12
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 2 - round 12

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 6 - round 12
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 0 - round 12
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 7 - round 12

	aese	v5.16b, v27.16b						//AES block 5 - round 13
	aese	v1.16b, v27.16b						//AES block 1 - round 13
	aese	v2.16b, v27.16b						//AES block 2 - round 13

	aese	v0.16b, v27.16b						//AES block 0 - round 13
	aese	v4.16b, v27.16b						//AES block 4 - round 13
	aese	v6.16b, v27.16b						//AES block 6 - round 13

	aese	v3.16b, v27.16b						//AES block 3 - round 13
	aese	v7.16b, v27.16b						//AES block 7 - round 13
	b.ge	L256_dec_tail						//handle tail

	ldp	q8, q9, [x0], #32			//AES block 0, 1 - load ciphertext

	ldp	q10, q11, [x0], #32			//AES block 2, 3 - load ciphertext

	ldp	q12, q13, [x0], #32			//AES block 4, 5 - load ciphertext

	ldp	q14, q15, [x0], #32			//AES block 6, 7 - load ciphertext
	cmp	x0, x5				//check if we have <= 8 blocks

.long	0xce017121	//eor3 v1.16b, v9.16b, v1.16b, v28.16b				//AES block 1 - result
.long	0xce007100	//eor3 v0.16b, v8.16b, v0.16b, v28.16b				//AES block 0 - result
	stp	q0, q1, [x2], #32			//AES block 0, 1 - store result

	rev32	v0.16b, v30.16b				//CTR block 8
	add	v30.4s, v30.4s, v31.4s		//CTR block 8
.long	0xce037163	//eor3 v3.16b, v11.16b, v3.16b, v28.16b				//AES block 3 - result

.long	0xce0571a5	//eor3 v5.16b, v13.16b, v5.16b, v28.16b				//AES block 5 - result

.long	0xce047184	//eor3 v4.16b, v12.16b, v4.16b, v28.16b				//AES block 4 - result
	rev32	v1.16b, v30.16b				//CTR block 9
	add	v30.4s, v30.4s, v31.4s		//CTR block 9

.long	0xce027142	//eor3 v2.16b, v10.16b, v2.16b, v28.16b				//AES block 2 - result
	stp	q2, q3, [x2], #32			//AES block 2, 3 - store result

	rev32	v2.16b, v30.16b				//CTR block 10
	add	v30.4s, v30.4s, v31.4s		//CTR block 10

.long	0xce0671c6	//eor3 v6.16b, v14.16b, v6.16b, v28.16b				//AES block 6 - result

	rev32	v3.16b, v30.16b				//CTR block 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 11
	stp	q4, q5, [x2], #32			//AES block 4, 5 - store result

.long	0xce0771e7	//eor3 v7.16b, v15.16b, v7.16b, v28.16b				//AES block 7 - result
	stp	q6, q7, [x2], #32			//AES block 6, 7 - store result

	rev32	v4.16b, v30.16b				//CTR block 12
	add	v30.4s, v30.4s, v31.4s		//CTR block 12
	b.ge	L256_dec_prepretail					//do prepretail

L256_dec_main_loop:	//main	loop start
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	ldp	q26, q27, [x8, #0]					//load rk0, rk1
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	rev64	v9.16b, v9.16b						//GHASH block 8k+1
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14
	rev64	v8.16b, v8.16b						//GHASH block 8k

	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	rev64	v11.16b, v11.16b						//GHASH block 8k+3

	rev32	v7.16b, v30.16b				//CTR block 8k+15
	rev64	v15.16b, v15.16b						//GHASH block 8k+7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0

	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	ldp	q28, q26, [x8, #32]				//load rk2, rk3

	eor	v8.16b, v8.16b, v19.16b					//PRE 1
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1

	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid

	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high

	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid
	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low

	ldp	q26, q27, [x8, #96]				//load rk6, rk7
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	rev64	v13.16b, v13.16b						//GHASH block 8k+5

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v14.16b, v14.16b						//GHASH block 8k+6
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7
	ldp	q28, q26, [x8, #128]				//load rk8, rk9

	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7

	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7

	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low
	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8

	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high

	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8

	ldp	q27, q28, [x8, #160]				//load rk10, rk11
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9

	ldp	q8, q9, [x0], #32			//AES block 8k+8, 8k+9 - load ciphertext
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9

	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high

	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10

	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid
.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
	rev32	v20.16b, v30.16b					//CTR block 8k+16
	ldr	d16, [x10]			//MODULO - load modulo constant

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+16
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 11
	ldp	q26, q27, [x8, #192]				//load rk12, rk13

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 11
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 11

.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid
	rev32	v22.16b, v30.16b					//CTR block 8k+17
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 11

	ldp	q10, q11, [x0], #32			//AES block 8k+10, 8k+11 - load ciphertext
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 11
	ext	v21.16b, v17.16b, v17.16b, #8				 //MODULO - other top alignment

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 11
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+17
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 11

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 12
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 12
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 12

	rev32	v23.16b, v30.16b					//CTR block 8k+18
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+18
	pmull	v29.1q, v17.1d, v16.1d			//MODULO - top 64b align with mid

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 12
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 11

	ldr	q28, [x8, #224]					//load rk14
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 12
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 12

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid
	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 12
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 12

	ldp	q12, q13, [x0], #32			//AES block 8k+12, 8k+13 - load ciphertext
	aese	v1.16b, v27.16b						//AES block 8k+9 - round 13
	aese	v2.16b, v27.16b						//AES block 8k+10 - round 13

	ldp	q14, q15, [x0], #32			//AES block 8k+14, 8k+15 - load ciphertext
	aese	v0.16b, v27.16b						//AES block 8k+8 - round 13
	aese	v5.16b, v27.16b						//AES block 8k+13 - round 13

	rev32	v25.16b, v30.16b					//CTR block 8k+19
.long	0xce027142	//eor3 v2.16b, v10.16b, v2.16b, v28.16b				//AES block 8k+10 - result
.long	0xce017121	//eor3 v1.16b, v9.16b, v1.16b, v28.16b				//AES block 8k+9 - result

	ext	v21.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment
	aese	v7.16b, v27.16b						//AES block 8k+15 - round 13

	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+19
	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v4.16b, v27.16b						//AES block 8k+12 - round 13

.long	0xce0571a5	//eor3 v5.16b, v13.16b, v5.16b, v28.16b				//AES block 8k+13 - result
.long	0xce007100	//eor3 v0.16b, v8.16b, v0.16b, v28.16b				//AES block 8k+8 - result
	aese	v3.16b, v27.16b						//AES block 8k+11 - round 13

	stp	q0, q1, [x2], #32			//AES block 8k+8, 8k+9 - store result
	mov	v0.16b, v20.16b					//CTR block 8k+16
.long	0xce047184	//eor3 v4.16b, v12.16b, v4.16b, v28.16b				//AES block 8k+12 - result

.long	0xce154673	//eor3 v19.16b, v19.16b, v21.16b, v17.16b		 	//MODULO - fold into low
.long	0xce037163	//eor3 v3.16b, v11.16b, v3.16b, v28.16b				//AES block 8k+11 - result
	stp	q2, q3, [x2], #32			//AES block 8k+10, 8k+11 - store result

	mov	v3.16b, v25.16b					//CTR block 8k+19
	mov	v2.16b, v23.16b					//CTR block 8k+18
	aese	v6.16b, v27.16b						//AES block 8k+14 - round 13

	mov	v1.16b, v22.16b					//CTR block 8k+17
	stp	q4, q5, [x2], #32			//AES block 8k+12, 8k+13 - store result
.long	0xce0771e7	//eor3 v7.16b, v15.16b, v7.16b, v28.16b				//AES block 8k+15 - result

.long	0xce0671c6	//eor3 v6.16b, v14.16b, v6.16b, v28.16b				//AES block 8k+14 - result
	rev32	v4.16b, v30.16b				//CTR block 8k+20
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+20

	cmp	x0, x5				//LOOP CONTROL
	stp	q6, q7, [x2], #32			//AES block 8k+14, 8k+15 - store result
	b.lt	L256_dec_main_loop

L256_dec_prepretail:	//PREPRETAIL
	ldp	q26, q27, [x8, #0]					//load rk0, rk1
	rev32	v5.16b, v30.16b				//CTR block 8k+13
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+13

	rev64	v12.16b, v12.16b						//GHASH block 8k+4
	ldr	q21, [x3, #144]				//load h6k | h5k
	ldr	q24, [x3, #192]				//load h8k | h7k

	rev32	v6.16b, v30.16b				//CTR block 8k+14
	rev64	v8.16b, v8.16b						//GHASH block 8k
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+14

	ext	v19.16b, v19.16b, v19.16b, #8				//PRE 0
	ldr	q23, [x3, #176]				//load h7l | h7h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #208]				//load h8l | h8h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v9.16b, v9.16b						//GHASH block 8k+1

	rev32	v7.16b, v30.16b				//CTR block 8k+15
	rev64	v10.16b, v10.16b						//GHASH block 8k+2
	ldr	q20, [x3, #128]				//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #160]				//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 0
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 0
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 0

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 0
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 0
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 0

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 1
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 0
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 0

	ldp	q28, q26, [x8, #32]				//load rk2, rk3
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 1
	eor	v8.16b, v8.16b, v19.16b					//PRE 1

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 1
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 1
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 1

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 1
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 1
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 1

	pmull2	v16.1q, v9.2d, v23.2d				//GHASH block 8k+1 - high
	trn1	v18.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	pmull	v19.1q, v8.1d, v25.1d				//GHASH block 8k - low

	rev64	v11.16b, v11.16b						//GHASH block 8k+3
	pmull	v23.1q, v9.1d, v23.1d				//GHASH block 8k+1 - low

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 2
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 2
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 2

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 2
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 2
	pmull2	v17.1q, v8.2d, v25.2d				//GHASH block 8k - high

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 2
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 3

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 3
	rev64	v14.16b, v14.16b						//GHASH block 8k+6

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 3
	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 2
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 3

	pmull2	v29.1q, v10.2d, v22.2d				//GHASH block 8k+2 - high
	trn2	v8.2d, v9.2d, v8.2d				//GHASH block 8k, 8k+1 - mid
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 2

	ldp	q27, q28, [x8, #64]				//load rk4, rk5
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 3
	pmull2	v9.1q, v11.2d, v20.2d				//GHASH block 8k+3 - high

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 3
	eor	v17.16b, v17.16b, v16.16b				//GHASH block 8k+1 - high
	eor	v8.16b, v8.16b, v18.16b			//GHASH block 8k, 8k+1 - mid

	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 3
	pmull	v22.1q, v10.1d, v22.1d				//GHASH block 8k+2 - low
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 3

.long	0xce1d2631	//eor3 v17.16b, v17.16b, v29.16b, v9.16b			//GHASH block 8k+2, 8k+3 - high
	trn1	v29.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid
	trn2	v10.2d, v11.2d, v10.2d				//GHASH block 8k+2, 8k+3 - mid

	pmull2	v18.1q, v8.2d, v24.2d				//GHASH block 8k	- mid
	pmull	v20.1q, v11.1d, v20.1d				//GHASH block 8k+3 - low
	eor	v19.16b, v19.16b, v23.16b				//GHASH block 8k+1 - low

	pmull	v24.1q, v8.1d, v24.1d				//GHASH block 8k+1 - mid
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 4
	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 4

.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+2, 8k+3 - low
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8
	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 4

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 4
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 4
	eor	v18.16b, v18.16b, v24.16b				//GHASH block 8k+1 - mid

	eor	v10.16b, v10.16b, v29.16b				//GHASH block 8k+2, 8k+3 - mid
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 5
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 4

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 5
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 4
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 4

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 5
	pmull2	v29.1q, v10.2d, v21.2d				//GHASH block 8k+2 - mid
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 5

	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 5
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 5
	pmull	v21.1q, v10.1d, v21.1d				//GHASH block 8k+3 - mid

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 5
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 5
	ldp	q26, q27, [x8, #96]				//load rk6, rk7

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v15.16b, v15.16b						//GHASH block 8k+7
	rev64	v13.16b, v13.16b						//GHASH block 8k+5

.long	0xce157652	//eor3 v18.16b, v18.16b, v21.16b, v29.16b			//GHASH block 8k+2, 8k+3 - mid

	trn1	v16.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 6
	ldr	q21, [x3, #48]				//load h2k | h1k
	ldr	q24, [x3, #96]				//load h4k | h3k
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 6

	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 6
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 6

	pmull2	v8.1q, v12.2d, v25.2d				//GHASH block 8k+4 - high
	pmull2	v10.1q, v13.2d, v23.2d				//GHASH block 8k+5 - high
	pmull	v25.1q, v12.1d, v25.1d				//GHASH block 8k+4 - low

	trn2	v12.2d, v13.2d, v12.2d				//GHASH block 8k+4, 8k+5 - mid
	pmull	v23.1q, v13.1d, v23.1d				//GHASH block 8k+5 - low
	trn1	v13.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 7
	pmull2	v11.1q, v14.2d, v22.2d				//GHASH block 8k+6 - high
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 6
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 6
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 6

	ldp	q28, q26, [x8, #128]				//load rk8, rk9
	pmull	v22.1q, v14.1d, v22.1d				//GHASH block 8k+6 - low
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 7

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 7
	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 7

	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 7
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 7
.long	0xce082a31	//eor3 v17.16b, v17.16b, v8.16b, v10.16b			//GHASH block 8k+4, 8k+5 - high

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 7
	trn2	v14.2d, v15.2d, v14.2d				//GHASH block 8k+6, 8k+7 - mid
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 7

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 8
	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 8
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 8

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 8
	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 8
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 8
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 9
	eor	v12.16b, v12.16b, v16.16b				//GHASH block 8k+4, 8k+5 - mid

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 9
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 9
	eor	v14.16b, v14.16b, v13.16b				//GHASH block 8k+6, 8k+7 - mid

	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 9
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 9
	pmull2	v16.1q, v12.2d, v24.2d				//GHASH block 8k+4 - mid

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 8
	pmull	v24.1q, v12.1d, v24.1d				//GHASH block 8k+5 - mid
	pmull2	v12.1q, v15.2d, v20.2d				//GHASH block 8k+7 - high

	pmull2	v13.1q, v14.2d, v21.2d				//GHASH block 8k+6 - mid
	pmull	v21.1q, v14.1d, v21.1d				//GHASH block 8k+7 - mid
	pmull	v20.1q, v15.1d, v20.1d				//GHASH block 8k+7 - low

	ldp	q27, q28, [x8, #160]				//load rk10, rk11
.long	0xce195e73	//eor3 v19.16b, v19.16b, v25.16b, v23.16b			//GHASH block 8k+4, 8k+5 - low
.long	0xce184252	//eor3 v18.16b, v18.16b, v24.16b, v16.16b			//GHASH block 8k+4, 8k+5 - mid

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 9
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 9
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 9

.long	0xce0b3231	//eor3 v17.16b, v17.16b, v11.16b, v12.16b			//GHASH block 8k+6, 8k+7 - high
.long	0xce165273	//eor3 v19.16b, v19.16b, v22.16b, v20.16b			//GHASH block 8k+6, 8k+7 - low
	ldr	d16, [x10]			//MODULO - load modulo constant

.long	0xce153652	//eor3 v18.16b, v18.16b, v21.16b, v13.16b			//GHASH block 8k+6, 8k+7 - mid

	aese	v4.16b, v27.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 10
	aese	v6.16b, v27.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 10
	aese	v5.16b, v27.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 10

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 10
	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 10
	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 10

.long	0xce114e52	//eor3 v18.16b, v18.16b, v17.16b, v19.16b		 	//MODULO - karatsuba tidy up

	aese	v7.16b, v27.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 10
	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 10
	ldp	q26, q27, [x8, #192]				//load rk12, rk13

	ext	v21.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 11
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b			//AES block 8k+9 - round 11
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 11

	pmull	v29.1q, v17.1d, v16.1d			//MODULO - top 64b align with mid
	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 11

	aese	v7.16b, v28.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 11
	aese	v6.16b, v28.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 11
	aese	v4.16b, v28.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 11

	aese	v5.16b, v28.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 11
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b			//AES block 8k+11 - round 12

.long	0xce1d5652	//eor3 v18.16b, v18.16b, v29.16b, v21.16b			//MODULO - fold into mid

	aese	v3.16b, v27.16b						//AES block 8k+11 - round 13
	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b			//AES block 8k+10 - round 12
	aese	v6.16b, v26.16b
	aesmc	v6.16b, v6.16b			//AES block 8k+14 - round 12

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low
	aese	v4.16b, v26.16b
	aesmc	v4.16b, v4.16b			//AES block 8k+12 - round 12
	aese	v7.16b, v26.16b
	aesmc	v7.16b, v7.16b			//AES block 8k+15 - round 12

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b			//AES block 8k+8 - round 12
	ldr	q28, [x8, #224]					//load rk14
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b	        	//AES block 8k+9 - round 12

	aese	v4.16b, v27.16b						//AES block 8k+12 - round 13
	ext	v21.16b, v18.16b, v18.16b, #8			 	//MODULO - other mid alignment
	aese	v5.16b, v26.16b
	aesmc	v5.16b, v5.16b			//AES block 8k+13 - round 12

	aese	v6.16b, v27.16b						//AES block 8k+14 - round 13
	aese	v2.16b, v27.16b						//AES block 8k+10 - round 13
	aese	v1.16b, v27.16b						//AES block 8k+9 - round 13

	aese	v5.16b, v27.16b						//AES block 8k+13 - round 13
.long	0xce154673	//eor3 v19.16b, v19.16b, v21.16b, v17.16b		 	//MODULO - fold into low
	add	v30.4s, v30.4s, v31.4s		//CTR block 8k+15

	aese	v7.16b, v27.16b						//AES block 8k+15 - round 13
	aese	v0.16b, v27.16b						//AES block 8k+8 - round 13
L256_dec_tail:	//TAIL

	ext	v16.16b, v19.16b, v19.16b, #8				//prepare final partial tag
	sub	x5, x4, x0		//main_end_input_ptr is number of bytes left to process
	cmp	x5, #112

	ldr	q9, [x0], #16				//AES block 8k+8 - load ciphertext

	ldp	q24, q25, [x3, #192]			//load h8k | h7k
	ext	v25.16b, v25.16b, v25.16b, #8
	mov	v29.16b, v28.16b

	ldp	q20, q21, [x3, #128]			//load h5l | h5h
	ext	v20.16b, v20.16b, v20.16b, #8

.long	0xce00752c	//eor3 v12.16b, v9.16b, v0.16b, v29.16b				//AES block 8k+8 - result
	ldp	q22, q23, [x3, #160]			//load h6l | h6h
	ext	v22.16b, v22.16b, v22.16b, #8
	ext	v23.16b, v23.16b, v23.16b, #8
	b.gt	L256_dec_blocks_more_than_7

	mov	v7.16b, v6.16b
	sub	v30.4s, v30.4s, v31.4s
	mov	v6.16b, v5.16b

	mov	v5.16b, v4.16b
	mov	v4.16b, v3.16b
	movi	v19.8b, #0

	movi	v17.8b, #0
	movi	v18.8b, #0
	mov	v3.16b, v2.16b

	cmp	x5, #96
	mov	v2.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_6

	mov	v7.16b, v6.16b
	mov	v6.16b, v5.16b

	mov	v5.16b, v4.16b
	cmp	x5, #80
	sub	v30.4s, v30.4s, v31.4s

	mov	v4.16b, v3.16b
	mov	v3.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_5

	cmp	x5, #64
	mov	v7.16b, v6.16b
	sub	v30.4s, v30.4s, v31.4s

	mov	v6.16b, v5.16b

	mov	v5.16b, v4.16b
	mov	v4.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_4

	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b
	cmp	x5, #48

	mov	v6.16b, v5.16b
	mov	v5.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_3

	ldr	q24, [x3, #96]				//load h4k | h3k
	sub	v30.4s, v30.4s, v31.4s
	mov	v7.16b, v6.16b

	cmp	x5, #32
	mov	v6.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_2

	sub	v30.4s, v30.4s, v31.4s

	mov	v7.16b, v1.16b
	cmp	x5, #16
	b.gt	L256_dec_blocks_more_than_1

	sub	v30.4s, v30.4s, v31.4s
	ldr	q21, [x3, #48]				//load h2k | h1k
	b	L256_dec_blocks_less_than_1
L256_dec_blocks_more_than_7:	//blocks	left >  7
	rev64	v8.16b, v9.16b						//GHASH final-7 block
	ldr	q9, [x0], #16				//AES final-6 block - load ciphertext
	st1	{ v12.16b}, [x2], #16				//AES final-7 block  - store result

	ins	v18.d[0], v24.d[1]					//GHASH final-7 block - mid

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-7 block - mid
.long	0xce01752c	//eor3 v12.16b, v9.16b, v1.16b, v29.16b				//AES final-6 block - result

	pmull2	v17.1q, v8.2d, v25.2d				//GHASH final-7 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-7 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v19.1q, v8.1d, v25.1d				//GHASH final-7 block - low
	pmull	v18.1q, v27.1d, v18.1d			 	//GHASH final-7 block - mid
L256_dec_blocks_more_than_6:	//blocks	left >  6

	rev64	v8.16b, v9.16b						//GHASH final-6 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	ldr	q9, [x0], #16				//AES final-5 block - load ciphertext
	movi	v16.8b, #0						//suppress further partial tag feed in

	ins	v27.d[0], v8.d[1]					//GHASH final-6 block - mid
	st1	{ v12.16b}, [x2], #16				//AES final-6 block - store result
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-6 block - high

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-6 block - low

.long	0xce02752c	//eor3 v12.16b, v9.16b, v2.16b, v29.16b				//AES final-5 block - result
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-6 block - low
	eor	v27.8b, v27.8b, v8.8b				//GHASH final-6 block - mid

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-6 block - mid

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-6 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-6 block - high
L256_dec_blocks_more_than_5:	//blocks	left >  5

	rev64	v8.16b, v9.16b						//GHASH final-5 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-5 block - high
	ins	v27.d[0], v8.d[1]					//GHASH final-5 block - mid

	ldr	q9, [x0], #16				//AES final-4 block - load ciphertext

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-5 block - mid
	st1	{ v12.16b}, [x2], #16			  	//AES final-5 block - store result

	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-5 block - low
	ins	v27.d[1], v27.d[0]					//GHASH final-5 block - mid

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-5 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-5 block - high
.long	0xce03752c	//eor3 v12.16b, v9.16b, v3.16b, v29.16b				//AES final-4 block - result
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-5 block - low

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-5 block - mid
	movi	v16.8b, #0						//suppress further partial tag feed in
L256_dec_blocks_more_than_4:	//blocks	left >  4

	rev64	v8.16b, v9.16b						//GHASH final-4 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-4 block - mid
	ldr	q9, [x0], #16				//AES final-3 block - load ciphertext

	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final-4 block - low
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final-4 block - high

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-4 block - mid

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-4 block - high

	pmull	v27.1q, v27.1d, v21.1d				//GHASH final-4 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-4 block - low
	st1	{ v12.16b}, [x2], #16			 	//AES final-4 block - store result

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-4 block - mid
.long	0xce04752c	//eor3 v12.16b, v9.16b, v4.16b, v29.16b				//AES final-3 block - result
L256_dec_blocks_more_than_3:	//blocks	left >  3

	ldr	q25, [x3, #112]				//load h4l | h4h
	ext	v25.16b, v25.16b, v25.16b, #8
	rev64	v8.16b, v9.16b						//GHASH final-3 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag
	ldr	q9, [x0], #16				//AES final-2 block - load ciphertext
	ldr	q24, [x3, #96]				//load h4k | h3k

	ins	v27.d[0], v8.d[1]					//GHASH final-3 block - mid
	st1	{ v12.16b}, [x2], #16			 	//AES final-3 block - store result

.long	0xce05752c	//eor3 v12.16b, v9.16b, v5.16b, v29.16b				//AES final-2 block - result

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-3 block - mid

	ins	v27.d[1], v27.d[0]					//GHASH final-3 block - mid
	pmull	v26.1q, v8.1d, v25.1d				//GHASH final-3 block - low
	pmull2	v28.1q, v8.2d, v25.2d				//GHASH final-3 block - high

	movi	v16.8b, #0						//suppress further partial tag feed in
	pmull2	v27.1q, v27.2d, v24.2d				//GHASH final-3 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-3 block - low

	eor	v17.16b, v17.16b, v28.16b					//GHASH final-3 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-3 block - mid
L256_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v8.16b, v9.16b						//GHASH final-2 block

	ldr	q23, [x3, #80]				//load h3l | h3h
	ext	v23.16b, v23.16b, v23.16b, #8
	ldr	q9, [x0], #16				//AES final-1 block - load ciphertext

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-2 block - mid

	pmull	v26.1q, v8.1d, v23.1d				//GHASH final-2 block - low
	st1	{ v12.16b}, [x2], #16			  	//AES final-2 block - store result
.long	0xce06752c	//eor3 v12.16b, v9.16b, v6.16b, v29.16b				//AES final-1 block - result

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-2 block - mid
	eor	v19.16b, v19.16b, v26.16b					//GHASH final-2 block - low
	movi	v16.8b, #0						//suppress further partial tag feed in

	pmull	v27.1q, v27.1d, v24.1d				//GHASH final-2 block - mid
	pmull2	v28.1q, v8.2d, v23.2d				//GHASH final-2 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-2 block - mid
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-2 block - high
L256_dec_blocks_more_than_1:	//blocks	left >  1

	rev64	v8.16b, v9.16b						//GHASH final-1 block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v27.d[0], v8.d[1]					//GHASH final-1 block - mid
	ldr	q22, [x3, #64]				//load h2l | h2h
	ext	v22.16b, v22.16b, v22.16b, #8

	eor	v27.8b, v27.8b, v8.8b				//GHASH final-1 block - mid
	ldr	q9, [x0], #16				//AES final block - load ciphertext
	st1	{ v12.16b}, [x2], #16			 	//AES final-1 block - store result

	ldr	q21, [x3, #48]				//load h2k | h1k
	pmull	v26.1q, v8.1d, v22.1d				//GHASH final-1 block - low

	ins	v27.d[1], v27.d[0]					//GHASH final-1 block - mid

	eor	v19.16b, v19.16b, v26.16b					//GHASH final-1 block - low

.long	0xce07752c	//eor3 v12.16b, v9.16b, v7.16b, v29.16b				//AES final block - result
	pmull2	v28.1q, v8.2d, v22.2d				//GHASH final-1 block - high

	pmull2	v27.1q, v27.2d, v21.2d				//GHASH final-1 block - mid

	movi	v16.8b, #0						//suppress further partial tag feed in
	eor	v17.16b, v17.16b, v28.16b					//GHASH final-1 block - high

	eor	v18.16b, v18.16b, v27.16b				//GHASH final-1 block - mid
L256_dec_blocks_less_than_1:	//blocks	left <= 1

	ld1	{ v26.16b}, [x2]					//load existing bytes where the possibly partial last block is to be stored
	mvn	x6, xzr						//temp0_x = 0xffffffffffffffff
	and	x1, x1, #127				//bit_length %= 128

	sub	x1, x1, #128				//bit_length -= 128
	rev32	v30.16b, v30.16b
	str	q30, [x16]					//store the updated counter

	neg	x1, x1				//bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127			 	//bit_length %= 128

	lsr	x6, x6, x1				//temp0_x is mask for top 64b of last block
	cmp	x1, #64
	mvn	x7, xzr						//temp1_x = 0xffffffffffffffff

	csel	x14, x6, xzr, lt
	csel	x13, x7, x6, lt

	mov	v0.d[0], x13					//ctr0b is mask for last block
	mov	v0.d[1], x14

	and	v9.16b, v9.16b, v0.16b					//possibly partial last block has zeroes in highest bits
	ldr	q20, [x3, #32]				//load h1l | h1h
	ext	v20.16b, v20.16b, v20.16b, #8
	bif	v12.16b, v26.16b, v0.16b					//insert existing bytes in top end of result before storing

	rev64	v8.16b, v9.16b						//GHASH final block

	eor	v8.16b, v8.16b, v16.16b					//feed in partial tag

	ins	v16.d[0], v8.d[1]					//GHASH final block - mid
	pmull2	v28.1q, v8.2d, v20.2d				//GHASH final block - high

	eor	v16.8b, v16.8b, v8.8b				//GHASH final block - mid

	pmull	v26.1q, v8.1d, v20.1d				//GHASH final block - low
	eor	v17.16b, v17.16b, v28.16b					//GHASH final block - high

	pmull	v16.1q, v16.1d, v21.1d				//GHASH final block - mid

	eor	v18.16b, v18.16b, v16.16b				//GHASH final block - mid
	ldr	d16, [x10]			//MODULO - load modulo constant
	eor	v19.16b, v19.16b, v26.16b					//GHASH final block - low

	pmull	v21.1q, v17.1d, v16.1d		 	//MODULO - top 64b align with mid
	eor	v14.16b, v17.16b, v19.16b				//MODULO - karatsuba tidy up

	ext	v17.16b, v17.16b, v17.16b, #8				//MODULO - other top alignment
	st1	{ v12.16b}, [x2]				//store all 16B

	eor	v18.16b, v18.16b, v14.16b				//MODULO - karatsuba tidy up

	eor	v21.16b, v17.16b, v21.16b				//MODULO - fold into mid
	eor	v18.16b, v18.16b, v21.16b				//MODULO - fold into mid

	pmull	v17.1q, v18.1d, v16.1d			//MODULO - mid 64b align with low

	ext	v18.16b, v18.16b, v18.16b, #8				//MODULO - other mid alignment
	eor	v19.16b, v19.16b, v17.16b				//MODULO - fold into low

	eor	v19.16b, v19.16b, v18.16b				//MODULO - fold into low
	ext	v19.16b, v19.16b, v19.16b, #8
	rev64	v19.16b, v19.16b
	st1	{ v19.16b }, [x3]
	mov	x0, x9

	ldp	d10, d11, [sp, #16]
	ldp	d12, d13, [sp, #32]
	ldp	d14, d15, [sp, #48]
	ldp	d8, d9, [sp], #80
	ret

L256_dec_ret:
	mov	w0, #0x0
	ret

.byte	65,69,83,32,71,67,77,32,109,111,100,117,108,101,32,102,111,114,32,65,82,77,118,56,44,32,83,80,68,88,32,66,83,68,45,51,45,67,108,97,117,115,101,32,98,121,32,60,120,105,97,111,107,97,110,103,46,113,105,97,110,64,97,114,109,46,99,111,109,62,0
.align	2
.align	2
#endif
