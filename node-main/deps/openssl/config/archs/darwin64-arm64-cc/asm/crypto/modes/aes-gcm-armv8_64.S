#include "arm_arch.h"

#if __ARM_MAX_ARCH__>=8

.text
.globl	_aes_gcm_enc_128_kernel

.align	4
_aes_gcm_enc_128_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L128_enc_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	ldp	x10, x11, [x16]              //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #160]                     //load rk10
#ifdef __AARCH64EB__
	ror	x13, x13, #32
	ror	x14, x14, #32
#endif
	ld1	{v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	lsr	x5, x1, #3              //byte_len
	mov	x15, x5

	ld1	{v18.4s}, [x8], #16								  //load rk0
	add	x4, x0, x1, lsr #3   //end_input_ptr
	sub	x5, x5, #1      //byte_len - 1

	lsr	x12, x11, #32
	ldr	q15, [x3, #112]                        //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif
	fmov	d1, x10                               //CTR block 1
	rev	w12, w12                                //rev_ctr32

	add	w12, w12, #1                            //increment rev_ctr32
	orr	w11, w11, w11
	ld1	{v19.4s}, [x8], #16								  //load rk1

	rev	w9, w12                                 //CTR block 1
	add	w12, w12, #1                            //CTR block 1
	fmov	d3, x10                               //CTR block 3

	orr	x9, x11, x9, lsl #32            //CTR block 1
	ld1	{ v0.16b}, [x16]                             //special case vector load initial counter so we can start first AES block as quickly as possible

	fmov	v1.d[1], x9                               //CTR block 1
	rev	w9, w12                                 //CTR block 2

	fmov	d2, x10                               //CTR block 2
	orr	x9, x11, x9, lsl #32            //CTR block 2
	add	w12, w12, #1                            //CTR block 2

	fmov	v2.d[1], x9                               //CTR block 2
	rev	w9, w12                                 //CTR block 3

	orr	x9, x11, x9, lsl #32            //CTR block 3
	ld1	{v20.4s}, [x8], #16								  //load rk2

	add	w12, w12, #1                            //CTR block 3
	fmov	v3.d[1], x9                               //CTR block 3

	ldr	q14, [x3, #80]                         //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 0
	ld1	{v21.4s}, [x8], #16								  //load rk3

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 0
	ldr	q12, [x3, #32]                         //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 0
	ld1	{v22.4s}, [x8], #16								  //load rk4

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 0
	ld1	{v23.4s}, [x8], #16								  //load rk5

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 1
	trn2	v17.2d,  v14.2d,    v15.2d                      //h4l | h3l

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 1
	ld1	{v24.4s}, [x8], #16								  //load rk6

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 1
	ld1	{v25.4s}, [x8], #16								  //load rk7

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 1
	trn1	v9.2d, v14.2d,    v15.2d                      //h4h | h3h

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 2
	ld1	{v26.4s}, [x8], #16								  //load rk8

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 2
	ldr	q13, [x3, #64]                         //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 2
	eor	v17.16b, v17.16b, v9.16b                  //h4k | h3k

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 3

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 3

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 3
	ld1	{v27.4s}, [x8], #16								  //load rk9

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 3

	and	x5, x5, #0xffffffffffffffc0    //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
	trn2	v16.2d,  v12.2d,    v13.2d                      //h2l | h1l

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 4
	add	x5, x5, x0

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 4
	cmp	x0, x5                   //check if we have <= 4 blocks

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 4

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 5

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 5

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 6

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 4

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 6
	trn1	v8.2d,    v12.2d,    v13.2d                      //h2h | h1h

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 6

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 5

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 7

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 7

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 6

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 8

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 7

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 8

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 8

	aese	v2.16b, v27.16b                                      //AES block 2 - round 9

	aese	v0.16b, v27.16b                                      //AES block 0 - round 9

	eor	v16.16b, v16.16b, v8.16b                     //h2k | h1k

	aese	v1.16b, v27.16b                                      //AES block 1 - round 9

	aese	v3.16b, v27.16b                                      //AES block 3 - round 9
	b.ge	L128_enc_tail                                    //handle tail

	ldp	x6, x7, [x0, #0]            //AES block 0 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	ldp	x21, x22, [x0, #32]           //AES block 2 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	ldp	x19, x20, [x0, #16]           //AES block 1 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	ldp	x23, x24, [x0, #48]           //AES block 3 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	eor	x6, x6, x13                     //AES block 0 - round 10 low
	eor	x7, x7, x14                     //AES block 0 - round 10 high

	eor	x21, x21, x13                     //AES block 2 - round 10 low
	fmov	d4, x6                               //AES block 0 - mov low

	eor	x19, x19, x13                     //AES block 1 - round 10 low
	eor	x22, x22, x14                     //AES block 2 - round 10 high
	fmov	v4.d[1], x7                           //AES block 0 - mov high

	fmov	d5, x19                               //AES block 1 - mov low
	eor	x20, x20, x14                     //AES block 1 - round 10 high

	eor	x23, x23, x13                     //AES block 3 - round 10 low
	fmov	v5.d[1], x20                           //AES block 1 - mov high

	fmov	d6, x21                               //AES block 2 - mov low
	eor	x24, x24, x14                     //AES block 3 - round 10 high
	rev	w9, w12                                 //CTR block 4

	fmov	v6.d[1], x22                           //AES block 2 - mov high
	orr	x9, x11, x9, lsl #32            //CTR block 4

	eor	v4.16b, v4.16b, v0.16b                          //AES block 0 - result
	fmov	d0, x10                               //CTR block 4
	add	w12, w12, #1                            //CTR block 4

	fmov	v0.d[1], x9                               //CTR block 4
	rev	w9, w12                                 //CTR block 5

	eor	v5.16b, v5.16b, v1.16b                          //AES block 1 - result
	fmov	d1, x10                               //CTR block 5
	orr	x9, x11, x9, lsl #32            //CTR block 5

	add	w12, w12, #1                            //CTR block 5
	add	x0, x0, #64                       //AES input_ptr update
	fmov	v1.d[1], x9                               //CTR block 5

	fmov	d7, x23                               //AES block 3 - mov low
	rev	w9, w12                                 //CTR block 6
	st1	{ v4.16b}, [x2], #16                     //AES block 0 - store result

	fmov	v7.d[1], x24                           //AES block 3 - mov high
	orr	x9, x11, x9, lsl #32            //CTR block 6

	add	w12, w12, #1                            //CTR block 6
	eor	v6.16b, v6.16b, v2.16b                          //AES block 2 - result
	st1	{ v5.16b}, [x2], #16                     //AES block 1 - store result

	fmov	d2, x10                               //CTR block 6
	cmp	x0, x5                   //check if we have <= 8 blocks

	fmov	v2.d[1], x9                               //CTR block 6
	rev	w9, w12                                 //CTR block 7
	st1	{ v6.16b}, [x2], #16                     //AES block 2 - store result

	orr	x9, x11, x9, lsl #32            //CTR block 7

	eor	v7.16b, v7.16b, v3.16b                          //AES block 3 - result
	st1	{ v7.16b}, [x2], #16                     //AES block 3 - store result
	b.ge	L128_enc_prepretail                              //do prepretail

L128_enc_main_loop:	//main	loop start
	ldp	x23, x24, [x0, #48]           //AES block 4k+3 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	rev64	v4.16b, v4.16b                                    //GHASH block 4k (only t0 is free)
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2 (t0, t1, and t2 free)

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	fmov	d3, x10                               //CTR block 4k+3

	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	rev64	v5.16b, v5.16b                                    //GHASH block 4k+1 (t0 and t1 free)

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	add	w12, w12, #1                            //CTR block 4k+3
	fmov	v3.d[1], x9                               //CTR block 4k+3

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	mov	d30, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	eor	x24, x24, x14                     //AES block 4k+3 - round 10 high

	pmull2	v28.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high
	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid
	ldp	x6, x7, [x0, #0]            //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	rev	w9, w12                                 //CTR block 4k+8

	eor	v30.8b, v30.8b, v5.8b                          //GHASH block 4k+1 - mid
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid
	orr	x9, x11, x9, lsl #32            //CTR block 4k+8

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	add	w12, w12, #1                            //CTR block 4k+8
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	eor	v9.16b, v9.16b, v28.16b                         //GHASH block 4k+1 - high

	pmull	v28.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3 (t0, t1, t2 and t3 free)

	pmull	v30.1q, v30.1d, v17.1d                          //GHASH block 4k+1 - mid

	pmull	v29.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	pmull2	v8.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	eor	x7, x7, x14                     //AES block 4k+4 - round 10 high

	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+1 - mid
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+1 - low

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	eor	x6, x6, x13                     //AES block 4k+4 - round 10 low

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	pmull2	v4.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3
	eor	v9.16b, v9.16b, v8.16b                         //GHASH block 4k+2 - high

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid

	pmull	v29.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low
	movi	v8.8b, #0xc2

	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v11.16b, v11.16b, v28.16b                         //GHASH block 4k+2 - low

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	shl	d8, d8, #56               //mod_constant

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+3 - high

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5
	ldp	x19, x20, [x0, #16]           //AES block 4k+5 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	ldp	x21, x22, [x0, #32]           //AES block 4k+6 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+3 - low

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	eor	x19, x19, x13                     //AES block 4k+5 - round 10 low

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6
	eor	x23, x23, x13                     //AES block 4k+3 - round 10 low

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5
	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	fmov	d4, x6                               //AES block 4k+4 - mov low
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	fmov	v4.d[1], x7                           //AES block 4k+4 - mov high

	add	x0, x0, #64                       //AES input_ptr update
	fmov	d7, x23                               //AES block 4k+3 - mov low
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	fmov	d5, x19                               //AES block 4k+5 - mov low

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7
	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	eor	x20, x20, x14                     //AES block 4k+5 - round 10 high

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7
	fmov	v5.d[1], x20                           //AES block 4k+5 - mov high

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	fmov	v7.d[1], x24                           //AES block 4k+3 - mov high

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	cmp	x0, x5                   //LOOP CONTROL

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	aese	v0.16b, v27.16b                                      //AES block 4k+4 - round 9
	eor	x21, x21, x13                     //AES block 4k+6 - round 10 low
	eor	x22, x22, x14                     //AES block 4k+6 - round 10 high

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	fmov	d6, x21                               //AES block 4k+6 - mov low

	aese	v1.16b, v27.16b                                      //AES block 4k+5 - round 9
	fmov	v6.d[1], x22                           //AES block 4k+6 - mov high

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	eor	v4.16b, v4.16b, v0.16b                          //AES block 4k+4 - result

	fmov	d0, x10                               //CTR block 4k+8
	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8

	fmov	v0.d[1], x9                               //CTR block 4k+8
	rev	w9, w12                                 //CTR block 4k+9
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	eor	v5.16b, v5.16b, v1.16b                          //AES block 4k+5 - result

	add	w12, w12, #1                            //CTR block 4k+9
	orr	x9, x11, x9, lsl #32            //CTR block 4k+9
	fmov	d1, x10                               //CTR block 4k+9

	pmull	v9.1q, v10.1d, v8.1d            //MODULO - mid 64b align with low
	fmov	v1.d[1], x9                               //CTR block 4k+9
	rev	w9, w12                                 //CTR block 4k+10

	aese	v2.16b, v27.16b                                      //AES block 4k+6 - round 9
	st1	{ v4.16b}, [x2], #16                     //AES block 4k+4 - store result
	eor	v6.16b, v6.16b, v2.16b                          //AES block 4k+6 - result
	orr	x9, x11, x9, lsl #32            //CTR block 4k+10

	aese	v3.16b, v27.16b                                      //AES block 4k+7 - round 9
	add	w12, w12, #1                            //CTR block 4k+10
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment
	fmov	d2, x10                               //CTR block 4k+10

	eor	v11.16b, v11.16b, v9.16b                         //MODULO - fold into low
	st1	{ v5.16b}, [x2], #16                     //AES block 4k+5 - store result

	fmov	v2.d[1], x9                               //CTR block 4k+10
	st1	{ v6.16b}, [x2], #16                     //AES block 4k+6 - store result
	rev	w9, w12                                 //CTR block 4k+11

	orr	x9, x11, x9, lsl #32            //CTR block 4k+11
	eor	v7.16b, v7.16b, v3.16b                          //AES block 4k+3 - result

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	st1	{ v7.16b}, [x2], #16                     //AES block 4k+3 - store result
	b.lt	L128_enc_main_loop

L128_enc_prepretail:	//PREPRETAIL
	rev64	v4.16b, v4.16b                                    //GHASH block 4k (only t0 is free)
	fmov	d3, x10                               //CTR block 4k+3
	rev64	v5.16b, v5.16b                                    //GHASH block 4k+1 (t0 and t1 free)

	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	add	w12, w12, #1                            //CTR block 4k+3
	fmov	v3.d[1], x9                               //CTR block 4k+3

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2 (t0, t1, and t2 free)

	pmull	v29.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low

	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3 (t0, t1, t2 and t3 free)
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1

	pmull2	v28.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	mov	d30, v5.d[1]                                  //GHASH block 4k+1 - mid

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid

	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	eor	v30.8b, v30.8b, v5.8b                          //GHASH block 4k+1 - mid

	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1

	pmull	v30.1q, v30.1d, v17.1d                          //GHASH block 4k+1 - mid
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+1 - low

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0

	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+1 - mid
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	eor	v9.16b, v9.16b, v28.16b                         //GHASH block 4k+1 - high

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid

	pmull2	v8.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	pmull2	v4.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	pmull	v28.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	eor	v9.16b, v9.16b, v8.16b                         //GHASH block 4k+2 - high

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2

	pmull	v29.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low
	movi	v8.8b, #0xc2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	eor	v11.16b, v11.16b, v28.16b                         //GHASH block 4k+2 - low

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2

	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+3 - high

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3

	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid
	shl	d8, d8, #56               //mod_constant

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+3 - low

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4

	pmull	v28.1q, v9.1d, v8.1d
	eor	v10.16b, v10.16b, v9.16b                         //karatsuba tidy up

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	ext	v9.16b, v9.16b, v9.16b, #8

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	eor	v10.16b, v10.16b, v11.16b

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5
	eor	v10.16b, v10.16b, v28.16b

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	eor	v10.16b, v10.16b, v9.16b

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7

	pmull	v28.1q, v10.1d, v8.1d

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7
	ext	v10.16b, v10.16b, v10.16b, #8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	eor	v11.16b, v11.16b, v28.16b

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8

	aese	v3.16b, v27.16b                                      //AES block 4k+7 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8

	aese	v0.16b, v27.16b                                      //AES block 4k+4 - round 9

	aese	v1.16b, v27.16b                                      //AES block 4k+5 - round 9
	eor	v11.16b, v11.16b, v10.16b

	aese	v2.16b, v27.16b                                      //AES block 4k+6 - round 9
L128_enc_tail:	//TAIL

	sub	x5, x4, x0   //main_end_input_ptr is number of bytes left to process
	ldp	x6, x7, [x0], #16           //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	cmp	x5, #48

	ext	v8.16b, v11.16b, v11.16b, #8                     //prepare final partial tag
	eor	x6, x6, x13                     //AES block 4k+4 - round 10 low
	eor	x7, x7, x14                     //AES block 4k+4 - round 10 high

	fmov	d4, x6                               //AES block 4k+4 - mov low

	fmov	v4.d[1], x7                           //AES block 4k+4 - mov high

	eor	v5.16b, v4.16b, v0.16b                          //AES block 4k+4 - result

	b.gt	L128_enc_blocks_more_than_3

	sub	w12, w12, #1
	movi	v11.8b, #0
	mov	v3.16b, v2.16b

	cmp	x5, #32
	mov	v2.16b, v1.16b
	movi	v9.8b, #0

	movi	v10.8b, #0
	b.gt	L128_enc_blocks_more_than_2

	mov	v3.16b, v1.16b
	cmp	x5, #16

	sub	w12, w12, #1
	b.gt	L128_enc_blocks_more_than_1

	sub	w12, w12, #1
	b	L128_enc_blocks_less_than_1
L128_enc_blocks_more_than_3:	//blocks	left >  3
	st1	{ v5.16b}, [x2], #16                     //AES final-3 block  - store result

	ldp	x6, x7, [x0], #16           //AES final-2 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	rev64	v4.16b, v5.16b                                    //GHASH final-3 block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag
	eor	x7, x7, x14                     //AES final-2 block - round 10 high
	eor	x6, x6, x13                     //AES final-2 block - round 10 low

	fmov	d5, x6                                 //AES final-2 block - mov low

	movi	v8.8b, #0                                        //suppress further partial tag feed in
	fmov	v5.d[1], x7                             //AES final-2 block - mov high

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH final-3 block - low
	mov	d22, v4.d[1]                                 //GHASH final-3 block - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH final-3 block - high

	mov	d10, v17.d[1]                               //GHASH final-3 block - mid

	eor	v5.16b, v5.16b, v1.16b                            //AES final-2 block - result
	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-3 block - mid

	pmull	v10.1q, v22.1d, v10.1d                    //GHASH final-3 block - mid
L128_enc_blocks_more_than_2:	//blocks	left >  2

	st1	{ v5.16b}, [x2], #16                     //AES final-2 block - store result

	rev64	v4.16b, v5.16b                                    //GHASH final-2 block
	ldp	x6, x7, [x0], #16           //AES final-1 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	eor	x6, x6, x13                     //AES final-1 block - round 10 low

	fmov	d5, x6                                 //AES final-1 block - mov low
	eor	x7, x7, x14                     //AES final-1 block - round 10 high

	pmull2	v20.1q, v4.2d, v14.2d                          //GHASH final-2 block - high
	fmov	v5.d[1], x7                             //AES final-1 block - mov high

	mov	d22, v4.d[1]                                 //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                          //GHASH final-2 block - low

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-2 block - high

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-2 block - mid

	eor	v5.16b, v5.16b, v2.16b                            //AES final-1 block - result

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-2 block - low

	pmull	v22.1q, v22.1d, v17.1d                      //GHASH final-2 block - mid

	movi	v8.8b, #0                                        //suppress further partial tag feed in

	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-2 block - mid
L128_enc_blocks_more_than_1:	//blocks	left >  1

	st1	{ v5.16b}, [x2], #16                     //AES final-1 block - store result

	rev64	v4.16b, v5.16b                                    //GHASH final-1 block
	ldp	x6, x7, [x0], #16           //AES final block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	eor	x7, x7, x14                     //AES final block - round 10 high
	eor	x6, x6, x13                     //AES final block - round 10 low

	fmov	d5, x6                                 //AES final block - mov low

	pmull2	v20.1q, v4.2d, v13.2d                          //GHASH final-1 block - high
	fmov	v5.d[1], x7                             //AES final block - mov high

	mov	d22, v4.d[1]                                 //GHASH final-1 block - mid

	pmull	v21.1q, v4.1d, v13.1d                          //GHASH final-1 block - low

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-1 block - mid

	eor	v5.16b, v5.16b, v3.16b                            //AES final block - result

	ins	v22.d[1], v22.d[0]                            //GHASH final-1 block - mid

	pmull2	v22.1q, v22.2d, v16.2d                      //GHASH final-1 block - mid

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-1 block - low

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-1 block - high

	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-1 block - mid
	movi	v8.8b, #0                                        //suppress further partial tag feed in
L128_enc_blocks_less_than_1:	//blocks	left <= 1

	and	x1, x1, #127                    //bit_length %= 128
	mvn	x13, xzr                                      //rk10_l = 0xffffffffffffffff

	mvn	x14, xzr                                      //rk10_h = 0xffffffffffffffff
	sub	x1, x1, #128                    //bit_length -= 128

	neg	x1, x1                          //bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127                    //bit_length %= 128

	lsr	x14, x14, x1                     //rk10_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x6, x13, x14, lt
	csel	x7, x14, xzr, lt

	fmov	d0, x6                                 //ctr0b is mask for last block

	fmov	v0.d[1], x7

	and	v5.16b, v5.16b, v0.16b                            //possibly partial last block has zeroes in highest bits

	rev64	v4.16b, v5.16b                                    //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	mov	d8, v4.d[1]                                  //GHASH final block - mid

	pmull	v21.1q, v4.1d, v12.1d                          //GHASH final block - low
	ld1	{ v18.16b}, [x2]                            //load existing bytes where the possibly partial last block is to be stored

	eor	v8.8b, v8.8b, v4.8b                          //GHASH final block - mid
#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif
	pmull2	v20.1q, v4.2d, v12.2d                          //GHASH final block - high

	pmull	v8.1q, v8.1d, v16.1d                          //GHASH final block - mid

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final block - low

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final block - high

	eor	v10.16b, v10.16b, v8.16b                         //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	shl	d8, d8, #56               //mod_constant

	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid

	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	pmull	v9.1q, v10.1d, v8.1d            //MODULO - mid 64b align with low

	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	bif	v5.16b, v18.16b, v0.16b                              //insert existing bytes in top end of result before storing

	eor	v11.16b, v11.16b, v9.16b                         //MODULO - fold into low
	st1	{ v5.16b}, [x2]                          //store all 16B

	str	w9, [x16, #12]                          //store the updated counter

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L128_enc_ret:
	mov	w0, #0x0
	ret

.globl	_aes_gcm_dec_128_kernel

.align	4
_aes_gcm_dec_128_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L128_dec_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	lsr	x5, x1, #3              //byte_len
	mov	x15, x5
	ldp	x10, x11, [x16]              //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #160]                     //load rk10
#ifdef __AARCH64EB__
	ror	x14, x14, 32
	ror	x13, x13, 32
#endif
	sub	x5, x5, #1      //byte_len - 1
	ld1	{v18.4s}, [x8], #16                                //load rk0

	and	x5, x5, #0xffffffffffffffc0 //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)
	ld1	{ v0.16b}, [x16]                             //special case vector load initial counter so we can start first AES block as quickly as possible

	ldr	q13, [x3, #64]                         //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif
	lsr	x12, x11, #32
	fmov	d2, x10                               //CTR block 2

	ld1	{v19.4s}, [x8], #16                                //load rk1
	orr	w11, w11, w11
	rev	w12, w12                                //rev_ctr32

	fmov	d1, x10                               //CTR block 1
	add	w12, w12, #1                            //increment rev_ctr32

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 0
	rev	w9, w12                                 //CTR block 1

	orr	x9, x11, x9, lsl #32            //CTR block 1
	ld1	{v20.4s}, [x8], #16                                //load rk2
	add	w12, w12, #1                            //CTR block 1

	fmov	v1.d[1], x9                               //CTR block 1
	rev	w9, w12                                 //CTR block 2
	add	w12, w12, #1                            //CTR block 2

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 1
	orr	x9, x11, x9, lsl #32            //CTR block 2

	fmov	v2.d[1], x9                               //CTR block 2
	rev	w9, w12                                 //CTR block 3

	fmov	d3, x10                               //CTR block 3
	orr	x9, x11, x9, lsl #32            //CTR block 3
	add	w12, w12, #1                            //CTR block 3

	fmov	v3.d[1], x9                               //CTR block 3
	add	x4, x0, x1, lsr #3   //end_input_ptr

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 0
	ld1	{v21.4s}, [x8], #16                                //load rk3

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 2
	ld1	{v22.4s}, [x8], #16                                //load rk4

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 0
	ld1	{v23.4s}, [x8], #16                                //load rk5

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 1
	ld1	{v24.4s}, [x8], #16                                //load rk6

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 0

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 1

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 2

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 1
	ld1	{ v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 3
	ld1	{v25.4s}, [x8], #16                                //load rk7

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 3

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 2
	ld1	{v26.4s}, [x8], #16                                //load rk8

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 4

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 3

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 3
	ldr	q14, [x3, #80]                         //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif
	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 4
	ld1	{v27.4s}, [x8], #16                                //load rk9

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 5

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 4

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 4

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 5
	ldr	q12, [x3, #32]                         //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif
	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 5

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 6

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 6

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 6

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 6
	trn1	v8.2d,    v12.2d,    v13.2d                      //h2h | h1h

	ldr	q15, [x3, #112]                        //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif
	trn2	v16.2d,  v12.2d,    v13.2d                      //h2l | h1l
	add	x5, x5, x0

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 7

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 7

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 7
	eor	v16.16b, v16.16b, v8.16b                     //h2k | h1k

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 7

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 8
	trn2	v17.2d,  v14.2d,    v15.2d                      //h4l | h3l

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 8

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 8
	trn1	v9.2d, v14.2d,    v15.2d                      //h4h | h3h

	aese	v2.16b, v27.16b                                      //AES block 2 - round 9

	aese	v3.16b, v27.16b                                      //AES block 3 - round 9

	aese	v0.16b, v27.16b                                      //AES block 0 - round 9
	cmp	x0, x5                   //check if we have <= 4 blocks

	aese	v1.16b, v27.16b                                      //AES block 1 - round 9
	eor	v17.16b, v17.16b, v9.16b                  //h4k | h3k
	b.ge	L128_dec_tail                                    //handle tail

	ld1	{v4.16b, v5.16b}, [x0], #32               //AES block 0 - load ciphertext; AES block 1 - load ciphertext

	eor	v1.16b, v5.16b, v1.16b                            //AES block 1 - result
	ld1	{v6.16b}, [x0], #16                       //AES block 2 - load ciphertext

	eor	v0.16b, v4.16b, v0.16b                            //AES block 0 - result
	rev64	v4.16b, v4.16b                                    //GHASH block 0
	rev	w9, w12                                 //CTR block 4

	orr	x9, x11, x9, lsl #32            //CTR block 4
	add	w12, w12, #1                            //CTR block 4
	ld1	{v7.16b}, [x0], #16                       //AES block 3 - load ciphertext

	rev64	v5.16b, v5.16b                                    //GHASH block 1
	mov	x19, v1.d[0]                            //AES block 1 - mov low

	mov	x20, v1.d[1]                            //AES block 1 - mov high

	mov	x6, v0.d[0]                            //AES block 0 - mov low
	cmp	x0, x5                   //check if we have <= 8 blocks

	mov	x7, v0.d[1]                            //AES block 0 - mov high

	fmov	d0, x10                               //CTR block 4

	fmov	v0.d[1], x9                               //CTR block 4
	rev	w9, w12                                 //CTR block 5
	eor	x19, x19, x13                   //AES block 1 - round 10 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	fmov	d1, x10                               //CTR block 5
	add	w12, w12, #1                            //CTR block 5
	orr	x9, x11, x9, lsl #32            //CTR block 5

	fmov	v1.d[1], x9                               //CTR block 5
	rev	w9, w12                                 //CTR block 6
	add	w12, w12, #1                            //CTR block 6

	orr	x9, x11, x9, lsl #32            //CTR block 6

	eor	x20, x20, x14                   //AES block 1 - round 10 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	eor	x6, x6, x13                   //AES block 0 - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v2.16b, v6.16b, v2.16b                            //AES block 2 - result

	eor	x7, x7, x14                   //AES block 0 - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	stp	x6, x7, [x2], #16        //AES block 0 - store result

	stp	x19, x20, [x2], #16        //AES block 1 - store result
	b.ge	L128_dec_prepretail                              //do prepretail

L128_dec_main_loop:	//main	loop start
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low

	pmull2	v28.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high
	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	fmov	d2, x10                               //CTR block 4k+6

	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2
	fmov	v2.d[1], x9                               //CTR block 4k+6
	rev	w9, w12                                 //CTR block 4k+7

	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1
	mov	d30, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3

	pmull	v29.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high
	orr	x9, x11, x9, lsl #32            //CTR block 4k+7

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	fmov	d3, x10                               //CTR block 4k+7
	eor	v30.8b, v30.8b, v5.8b                          //GHASH block 4k+1 - mid

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	fmov	v3.d[1], x9                               //CTR block 4k+7

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+1 - low

	pmull	v29.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	eor	v9.16b, v9.16b, v28.16b                         //GHASH block 4k+1 - high

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0

	pmull	v28.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	eor	x23, x23, x13                   //AES block 4k+3 - round 10 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	pmull	v30.1q, v30.1d, v17.1d                          //GHASH block 4k+1 - mid
	eor	x22, x22, x14                   //AES block 4k+2 - round 10 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	eor	v11.16b, v11.16b, v28.16b                         //GHASH block 4k+2 - low

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+1 - mid

	pmull2	v8.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	pmull2	v4.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4
	eor	v9.16b, v9.16b, v8.16b                         //GHASH block 4k+2 - high

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid
	eor	x24, x24, x14                   //AES block 4k+3 - round 10 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5
	eor	x21, x21, x13                   //AES block 4k+2 - round 10 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif
	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	movi	v8.8b, #0xc2

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+3 - low

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+3 - high
	ld1	{v4.16b}, [x0], #16                       //AES block 4k+3 - load ciphertext

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7
	add	w12, w12, #1                            //CTR block 4k+7

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7
	shl	d8, d8, #56               //mod_constant

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	rev	w9, w12                                 //CTR block 4k+8

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	ld1	{v5.16b}, [x0], #16                       //AES block 4k+4 - load ciphertext
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v0.16b, v27.16b                                      //AES block 4k+4 - round 9
	orr	x9, x11, x9, lsl #32            //CTR block 4k+8

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	aese	v1.16b, v27.16b                                      //AES block 4k+5 - round 9

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	eor	v0.16b, v4.16b, v0.16b                            //AES block 4k+4 - result

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	ld1	{v6.16b}, [x0], #16                       //AES block 4k+5 - load ciphertext

	add	w12, w12, #1                            //CTR block 4k+8
	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid
	eor	v1.16b, v5.16b, v1.16b                            //AES block 4k+5 - result

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	ld1	{v7.16b}, [x0], #16                       //AES block 4k+6 - load ciphertext

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6

	rev64	v5.16b, v5.16b                                    //GHASH block 4k+5
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid
	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	fmov	d0, x10                               //CTR block 4k+8

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	fmov	v0.d[1], x9                               //CTR block 4k+8
	rev	w9, w12                                 //CTR block 4k+9

	aese	v2.16b, v27.16b                                      //AES block 4k+6 - round 9
	orr	x9, x11, x9, lsl #32            //CTR block 4k+9
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8
	eor	x7, x7, x14                   //AES block 4k+4 - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low
	mov	x20, v1.d[1]                            //AES block 4k+5 - mov high
	eor	x6, x6, x13                   //AES block 4k+4 - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v2.16b, v6.16b, v2.16b                            //AES block 4k+6 - result
	mov	x19, v1.d[0]                            //AES block 4k+5 - mov low
	add	w12, w12, #1                            //CTR block 4k+9

	aese	v3.16b, v27.16b                                      //AES block 4k+7 - round 9
	fmov	d1, x10                               //CTR block 4k+9
	cmp	x0, x5                   //LOOP CONTROL

	rev64	v4.16b, v4.16b                                    //GHASH block 4k+4
	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	fmov	v1.d[1], x9                               //CTR block 4k+9

	rev	w9, w12                                 //CTR block 4k+10
	add	w12, w12, #1                            //CTR block 4k+10

	eor	x20, x20, x14                   //AES block 4k+5 - round 10 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	stp	x6, x7, [x2], #16        //AES block 4k+4 - store result

	eor	x19, x19, x13                   //AES block 4k+5 - round 10 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	stp	x19, x20, [x2], #16        //AES block 4k+5 - store result

	orr	x9, x11, x9, lsl #32            //CTR block 4k+10
	b.lt	L128_dec_main_loop

L128_dec_prepretail:	//PREPRETAIL
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low
	mov	d30, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high

	eor	v4.16b, v4.16b, v11.16b                           //PRE 1
	fmov	d2, x10                               //CTR block 4k+6
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	fmov	v2.d[1], x9                               //CTR block 4k+6

	rev	w9, w12                                 //CTR block 4k+7
	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low
	eor	v30.8b, v30.8b, v5.8b                          //GHASH block 4k+1 - mid

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d10, v17.d[1]                               //GHASH block 4k - mid
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2
	orr	x9, x11, x9, lsl #32            //CTR block 4k+7

	pmull	v29.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid
	fmov	d3, x10                               //CTR block 4k+7

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	fmov	v3.d[1], x9                               //CTR block 4k+7

	pmull	v30.1q, v30.1d, v17.1d                          //GHASH block 4k+1 - mid
	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid

	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	pmull2	v28.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+1 - low

	pmull	v29.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid
	eor	v9.16b, v9.16b, v28.16b                         //GHASH block 4k+1 - high

	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+1 - mid

	pmull2	v4.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	pmull2	v8.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	pmull	v28.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	eor	v9.16b, v9.16b, v8.16b                         //GHASH block 4k+2 - high
	movi	v8.8b, #0xc2

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	eor	v11.16b, v11.16b, v28.16b                         //GHASH block 4k+2 - low

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+3 - high

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	eor	x23, x23, x13                   //AES block 4k+3 - round 10 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	x21, x21, x13                   //AES block 4k+2 - round 10 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif
	eor	v11.16b, v11.16b, v29.16b                         //GHASH block 4k+3 - low

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	shl	d8, d8, #56               //mod_constant

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	aese	v1.16b, v27.16b                                      //AES block 4k+5 - round 9

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	eor	x24, x24, x14                   //AES block 4k+3 - round 10 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8
	eor	x22, x22, x14                   //AES block 4k+2 - round 10 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	aese	v0.16b, v27.16b                                      //AES block 4k+4 - round 9
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	aese	v2.16b, v27.16b                                      //AES block 4k+6 - round 9
	add	w12, w12, #1                            //CTR block 4k+7
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result

	aese	v3.16b, v27.16b                                      //AES block 4k+7 - round 9
	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
L128_dec_tail:	//TAIL

	sub	x5, x4, x0   //main_end_input_ptr is number of bytes left to process
	ld1	{ v5.16b}, [x0], #16                      //AES block 4k+4 - load ciphertext

	eor	v0.16b, v5.16b, v0.16b                            //AES block 4k+4 - result

	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high

	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low

	cmp	x5, #48

	eor	x7, x7, x14                   //AES block 4k+4 - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	ext	v8.16b, v11.16b, v11.16b, #8                     //prepare final partial tag
	eor	x6, x6, x13                   //AES block 4k+4 - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	b.gt	L128_dec_blocks_more_than_3

	mov	v3.16b, v2.16b
	sub	w12, w12, #1
	movi	v11.8b, #0

	movi	v9.8b, #0
	mov	v2.16b, v1.16b

	movi	v10.8b, #0
	cmp	x5, #32
	b.gt	L128_dec_blocks_more_than_2

	cmp	x5, #16

	mov	v3.16b, v1.16b
	sub	w12, w12, #1
	b.gt	L128_dec_blocks_more_than_1

	sub	w12, w12, #1
	b	L128_dec_blocks_less_than_1
L128_dec_blocks_more_than_3:	//blocks	left >  3
	rev64	v4.16b, v5.16b                                    //GHASH final-3 block
	ld1	{ v5.16b}, [x0], #16                      //AES final-2 block - load ciphertext

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	mov	d10, v17.d[1]                               //GHASH final-3 block - mid
	stp	x6, x7, [x2], #16        //AES final-3 block  - store result
	eor	v0.16b, v5.16b, v1.16b                            //AES final-2 block - result

	mov	d22, v4.d[1]                                 //GHASH final-3 block - mid
	mov	x7, v0.d[1]                            //AES final-2 block - mov high

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH final-3 block - low
	mov	x6, v0.d[0]                            //AES final-2 block - mov low

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH final-3 block - high

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-3 block - mid

	movi	v8.8b, #0                                        //suppress further partial tag feed in
	eor	x7, x7, x14                   //AES final-2 block - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	pmull	v10.1q, v22.1d, v10.1d                    //GHASH final-3 block - mid
	eor	x6, x6, x13                   //AES final-2 block - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
L128_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v4.16b, v5.16b                                    //GHASH final-2 block
	ld1	{ v5.16b}, [x0], #16                      //AES final-1 block - load ciphertext

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	eor	v0.16b, v5.16b, v2.16b                            //AES final-1 block - result
	stp	x6, x7, [x2], #16        //AES final-2 block  - store result

	mov	d22, v4.d[1]                                 //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                          //GHASH final-2 block - low

	pmull2	v20.1q, v4.2d, v14.2d                          //GHASH final-2 block - high
	mov	x6, v0.d[0]                            //AES final-1 block - mov low

	mov	x7, v0.d[1]                            //AES final-1 block - mov high
	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-2 block - mid

	movi	v8.8b, #0                                        //suppress further partial tag feed in

	pmull	v22.1q, v22.1d, v17.1d                      //GHASH final-2 block - mid

	eor	x6, x6, x13                   //AES final-1 block - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-2 block - low

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-2 block - high

	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-2 block - mid
	eor	x7, x7, x14                   //AES final-1 block - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
L128_dec_blocks_more_than_1:	//blocks	left >  1

	rev64	v4.16b, v5.16b                                    //GHASH final-1 block

	ld1	{ v5.16b}, [x0], #16                      //AES final block - load ciphertext
	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	mov	d22, v4.d[1]                                 //GHASH final-1 block - mid

	eor	v0.16b, v5.16b, v3.16b                            //AES final block - result

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-1 block - mid

	stp	x6, x7, [x2], #16        //AES final-1 block  - store result
	mov	x6, v0.d[0]                            //AES final block - mov low

	mov	x7, v0.d[1]                            //AES final block - mov high
	ins	v22.d[1], v22.d[0]                            //GHASH final-1 block - mid

	pmull	v21.1q, v4.1d, v13.1d                          //GHASH final-1 block - low

	pmull2	v20.1q, v4.2d, v13.2d                          //GHASH final-1 block - high

	pmull2	v22.1q, v22.2d, v16.2d                      //GHASH final-1 block - mid
	movi	v8.8b, #0                                        //suppress further partial tag feed in

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-1 block - low

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-1 block - high
	eor	x7, x7, x14                   //AES final block - round 10 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	x6, x6, x13                   //AES final block - round 10 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-1 block - mid
L128_dec_blocks_less_than_1:	//blocks	left <= 1

	mvn	x14, xzr                                      //rk10_h = 0xffffffffffffffff
	and	x1, x1, #127                    //bit_length %= 128

	mvn	x13, xzr                                      //rk10_l = 0xffffffffffffffff
	sub	x1, x1, #128                    //bit_length -= 128

	neg	x1, x1                          //bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127                    //bit_length %= 128

	lsr	x14, x14, x1                     //rk10_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x10, x14, xzr, lt
	csel	x9, x13, x14, lt

	fmov	d0, x9                                   //ctr0b is mask for last block

	mov	v0.d[1], x10

	and	v5.16b, v5.16b, v0.16b                            //possibly partial last block has zeroes in highest bits

	rev64	v4.16b, v5.16b                                    //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	ldp	x4, x5, [x2] //load existing bytes we need to not overwrite

	and	x7, x7, x10

	pmull2	v20.1q, v4.2d, v12.2d                          //GHASH final block - high
	mov	d8, v4.d[1]                                  //GHASH final block - mid

	eor	v8.8b, v8.8b, v4.8b                          //GHASH final block - mid
	eor	v9.16b, v9.16b, v20.16b                            //GHASH final block - high

	pmull	v8.1q, v8.1d, v16.1d                          //GHASH final block - mid

	pmull	v21.1q, v4.1d, v12.1d                          //GHASH final block - low
	bic	x4, x4, x9           //mask out low existing bytes
	and	x6, x6, x9

#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif

	eor	v10.16b, v10.16b, v8.16b                         //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final block - low

	bic	x5, x5, x10   //mask out high existing bytes
	shl	d8, d8, #56               //mod_constant

	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid

	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	orr	x6, x6, x4
	str	w9, [x16, #12]                          //store the updated counter

	orr	x7, x7, x5
	stp	x6, x7, [x2]
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]

	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L128_dec_ret:
	mov	w0, #0x0
	ret

.globl	_aes_gcm_enc_192_kernel

.align	4
_aes_gcm_enc_192_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L192_enc_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	ldp	x10, x11, [x16]             //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #192]                     //load rk12
#ifdef __AARCH64EB__
	ror	x13, x13, #32
	ror	x14, x14, #32
#endif
	ld1	{v18.4s}, [x8], #16	                             //load rk0

	ld1	{v19.4s}, [x8], #16	                             //load rk1

	ld1	{v20.4s}, [x8], #16	                             //load rk2

	lsr	x12, x11, #32
	ld1	{v21.4s}, [x8], #16	                             //load rk3
	orr	w11, w11, w11

	ld1	{v22.4s}, [x8], #16	                             //load rk4
	rev	w12, w12                               //rev_ctr32

	add	w12, w12, #1                           //increment rev_ctr32
	fmov	d3, x10                              //CTR block 3

	rev	w9, w12                                //CTR block 1
	add	w12, w12, #1                           //CTR block 1
	fmov	d1, x10                              //CTR block 1

	orr	x9, x11, x9, lsl #32           //CTR block 1
	ld1	{ v0.16b}, [x16]                            //special case vector load initial counter so we can start first AES block as quickly as possible

	fmov	v1.d[1], x9                              //CTR block 1
	rev	w9, w12                                //CTR block 2
	add	w12, w12, #1                           //CTR block 2

	fmov	d2, x10                              //CTR block 2
	orr	x9, x11, x9, lsl #32           //CTR block 2

	fmov	v2.d[1], x9                              //CTR block 2
	rev	w9, w12                                //CTR block 3

	orr	x9, x11, x9, lsl #32           //CTR block 3
	ld1	{v23.4s}, [x8], #16	                             //load rk5

	fmov	v3.d[1], x9                              //CTR block 3

	ld1	{v24.4s}, [x8], #16	                             //load rk6

	ld1	{v25.4s}, [x8], #16	                             //load rk7

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 0
	ld1	{ v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 0
	ld1	{v26.4s}, [x8], #16	                             //load rk8

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 0
	ldr	q15, [x3, #112]                       //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 0
	ld1	{v27.4s}, [x8], #16	                             //load rk9

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 1
	ld1	{v28.4s}, [x8], #16	                         //load rk10

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 1
	ldr	q12, [x3, #32]                        //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 1
	ld1	{v29.4s}, [x8], #16	                         //load rk11

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 1
	ldr	q14, [x3, #80]                        //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 2

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 2

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 3
	trn1	v9.2d, v14.2d,    v15.2d                     //h4h | h3h

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 3

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 2
	trn2	v17.2d,  v14.2d,    v15.2d                     //h4l | h3l

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 4

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 3

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 3

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 5

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 4

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 4

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 6

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 4

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 5

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 5

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 5

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 6
	ldr	q13, [x3, #64]                        //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif
	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 6

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 6

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 7

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 7
	trn2	v16.2d,  v12.2d,    v13.2d                     //h2l | h1l

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 8

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 7
	trn1	v8.2d,    v12.2d,    v13.2d                     //h2h | h1h

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 8

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 8

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 9

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 9

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 9

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 9

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b         //AES block 0 - round 10

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b         //AES block 2 - round 10

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b         //AES block 1 - round 10
	lsr	x5, x1, #3             //byte_len
	mov	x15, x5

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b         //AES block 3 - round 10
	sub	x5, x5, #1     //byte_len - 1

	eor	v16.16b, v16.16b, v8.16b                    //h2k | h1k
	and	x5, x5, #0xffffffffffffffc0   //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	eor	v17.16b, v17.16b, v9.16b                 //h4k | h3k

	aese	v2.16b, v29.16b                                    //AES block 2 - round 11
	add	x4, x0, x1, lsr #3  //end_input_ptr
	add	x5, x5, x0

	aese	v1.16b, v29.16b                                    //AES block 1 - round 11
	cmp	x0, x5                  //check if we have <= 4 blocks

	aese	v0.16b, v29.16b                                    //AES block 0 - round 11
	add	w12, w12, #1                           //CTR block 3

	aese	v3.16b, v29.16b                                    //AES block 3 - round 11
	b.ge	L192_enc_tail                                   //handle tail

	rev	w9, w12                                //CTR block 4
	ldp	x6, x7, [x0, #0]           //AES block 0 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	orr	x9, x11, x9, lsl #32           //CTR block 4
	ldp	x21, x22, [x0, #32]          //AES block 2 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	ldp	x23, x24, [x0, #48]          //AES block 3 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	ldp	x19, x20, [x0, #16]          //AES block 1 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	add	x0, x0, #64                      //AES input_ptr update
	cmp	x0, x5                  //check if we have <= 8 blocks

	eor	x6, x6, x13                    //AES block 0 - round 12 low

	eor	x7, x7, x14                    //AES block 0 - round 12 high
	eor	x22, x22, x14                    //AES block 2 - round 12 high
	fmov	d4, x6                              //AES block 0 - mov low

	eor	x24, x24, x14                    //AES block 3 - round 12 high
	fmov	v4.d[1], x7                          //AES block 0 - mov high

	eor	x21, x21, x13                    //AES block 2 - round 12 low
	eor	x19, x19, x13                    //AES block 1 - round 12 low

	fmov	d5, x19                              //AES block 1 - mov low
	eor	x20, x20, x14                    //AES block 1 - round 12 high

	fmov	v5.d[1], x20                          //AES block 1 - mov high

	eor	x23, x23, x13                    //AES block 3 - round 12 low
	fmov	d6, x21                              //AES block 2 - mov low

	add	w12, w12, #1                           //CTR block 4
	eor	v4.16b, v4.16b, v0.16b                         //AES block 0 - result
	fmov	d0, x10                              //CTR block 4

	fmov	v0.d[1], x9                              //CTR block 4
	rev	w9, w12                                //CTR block 5

	orr	x9, x11, x9, lsl #32           //CTR block 5
	add	w12, w12, #1                           //CTR block 5

	fmov	d7, x23                              //AES block 3 - mov low
	st1	{ v4.16b}, [x2], #16                    //AES block 0 - store result

	fmov	v6.d[1], x22                          //AES block 2 - mov high

	eor	v5.16b, v5.16b, v1.16b                         //AES block 1 - result
	fmov	d1, x10                              //CTR block 5
	st1	{ v5.16b}, [x2], #16                    //AES block 1 - store result

	fmov	v7.d[1], x24                          //AES block 3 - mov high

	fmov	v1.d[1], x9                              //CTR block 5
	rev	w9, w12                                //CTR block 6

	orr	x9, x11, x9, lsl #32           //CTR block 6

	add	w12, w12, #1                           //CTR block 6
	eor	v6.16b, v6.16b, v2.16b                         //AES block 2 - result
	fmov	d2, x10                              //CTR block 6

	fmov	v2.d[1], x9                              //CTR block 6
	rev	w9, w12                                //CTR block 7

	orr	x9, x11, x9, lsl #32           //CTR block 7
	st1	{ v6.16b}, [x2], #16                    //AES block 2 - store result

	eor	v7.16b, v7.16b, v3.16b                         //AES block 3 - result
	st1	{ v7.16b}, [x2], #16                    //AES block 3 - store result
	b.ge	L192_enc_prepretail                             //do prepretail

L192_enc_main_loop:	//main	loop start
	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 0
	rev64	v5.16b, v5.16b                                   //GHASH block 4k+1 (t0 and t1 free)

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 0
	ldp	x19, x20, [x0, #16]          //AES block 4k+5 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	ext	v11.16b, v11.16b, v11.16b, #8                    //PRE 0
	fmov	d3, x10                              //CTR block 4k+3
	rev64	v4.16b, v4.16b                                   //GHASH block 4k (only t0 is free)

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 1
	fmov	v3.d[1], x9                              //CTR block 4k+3

	pmull2	v30.1q, v5.2d, v14.2d                         //GHASH block 4k+1 - high
	rev64	v7.16b, v7.16b                                   //GHASH block 4k+3 (t0, t1, t2 and t3 free)
	ldp	x21, x22, [x0, #32]          //AES block 4k+6 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 0
	ldp	x23, x24, [x0, #48]          //AES block 4k+3 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	pmull	v31.1q, v5.1d, v14.1d                         //GHASH block 4k+1 - low
	eor	v4.16b, v4.16b, v11.16b                          //PRE 1

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 1

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 1
	rev64	v6.16b, v6.16b                                   //GHASH block 4k+2 (t0, t1, and t2 free)

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 0
	eor	x24, x24, x14                    //AES block 4k+3 - round 12 high

	pmull	v11.1q, v4.1d, v15.1d                      //GHASH block 4k - low
	mov	d8, v4.d[1]                                 //GHASH block 4k - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 2

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 1
	eor	x21, x21, x13                    //AES block 4k+6 - round 12 low

	eor	v8.8b, v8.8b, v4.8b                         //GHASH block 4k - mid
	eor	v11.16b, v11.16b, v31.16b                        //GHASH block 4k+1 - low

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 3
	eor	x19, x19, x13                    //AES block 4k+5 - round 12 low

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 2
	mov	d31, v6.d[1]                                 //GHASH block 4k+2 - mid

	pmull2	v9.1q, v4.2d, v15.2d                      //GHASH block 4k - high
	mov	d4, v5.d[1]                                 //GHASH block 4k+1 - mid

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 2

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 3

	mov	d10, v17.d[1]                              //GHASH block 4k - mid
	eor	v9.16b, v9.16b, v30.16b                        //GHASH block 4k+1 - high

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 2
	eor	v31.8b, v31.8b, v6.8b                         //GHASH block 4k+2 - mid

	pmull2	v30.1q, v6.2d, v13.2d                         //GHASH block 4k+2 - high

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 4
	eor	v4.8b, v4.8b, v5.8b                         //GHASH block 4k+1 - mid

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 3

	pmull2	v5.1q, v7.2d, v12.2d                         //GHASH block 4k+3 - high
	eor	x20, x20, x14                    //AES block 4k+5 - round 12 high
	ins	v31.d[1], v31.d[0]                               //GHASH block 4k+2 - mid

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 5
	add	w12, w12, #1                           //CTR block 4k+3

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 4
	eor	v9.16b, v9.16b, v30.16b                        //GHASH block 4k+2 - high

	pmull	v4.1q, v4.1d, v17.1d                         //GHASH block 4k+1 - mid
	eor	x22, x22, x14                    //AES block 4k+6 - round 12 high

	pmull2	v31.1q, v31.2d, v16.2d                         //GHASH block 4k+2 - mid
	eor	x23, x23, x13                    //AES block 4k+3 - round 12 low
	mov	d30, v7.d[1]                                 //GHASH block 4k+3 - mid

	pmull	v10.1q, v8.1d, v10.1d                     //GHASH block 4k - mid
	rev	w9, w12                                //CTR block 4k+8

	pmull	v8.1q, v6.1d, v13.1d                         //GHASH block 4k+2 - low
	orr	x9, x11, x9, lsl #32           //CTR block 4k+8

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 3
	eor	v30.8b, v30.8b, v7.8b                         //GHASH block 4k+3 - mid

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 4
	ldp	x6, x7, [x0, #0]           //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 6
	eor	v11.16b, v11.16b, v8.16b                        //GHASH block 4k+2 - low

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 4
	add	x0, x0, #64                      //AES input_ptr update

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 5
	movi	v8.8b, #0xc2

	pmull	v6.1q, v7.1d, v12.1d                         //GHASH block 4k+3 - low
	eor	x7, x7, x14                    //AES block 4k+4 - round 12 high
	eor	v10.16b, v10.16b, v4.16b                        //GHASH block 4k+1 - mid

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 5
	eor	x6, x6, x13                    //AES block 4k+4 - round 12 low

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 6
	shl	d8, d8, #56              //mod_constant

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 5
	eor	v9.16b, v9.16b, v5.16b                        //GHASH block 4k+3 - high

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 7
	fmov	d5, x19                              //AES block 4k+5 - mov low

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 7
	eor	v10.16b, v10.16b, v31.16b                        //GHASH block 4k+2 - mid

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 6
	fmov	v5.d[1], x20                          //AES block 4k+5 - mov high

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 8
	eor	v11.16b, v11.16b, v6.16b                        //GHASH block 4k+3 - low

	pmull	v30.1q, v30.1d, v16.1d                         //GHASH block 4k+3 - mid
	cmp	x0, x5                  //LOOP CONTROL
	fmov	d4, x6                              //AES block 4k+4 - mov low

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 6
	fmov	v4.d[1], x7                          //AES block 4k+4 - mov high

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 8
	fmov	d7, x23                              //AES block 4k+3 - mov low

	eor	v10.16b, v10.16b, v30.16b                        //GHASH block 4k+3 - mid
	eor	v30.16b, v11.16b, v9.16b                        //MODULO - karatsuba tidy up
	add	w12, w12, #1                           //CTR block 4k+8

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 7
	fmov	v7.d[1], x24                          //AES block 4k+3 - mov high

	pmull	v31.1q, v9.1d, v8.1d           //MODULO - top 64b align with mid
	ext	v9.16b, v9.16b, v9.16b, #8                    //MODULO - other top alignment
	fmov	d6, x21                              //AES block 4k+6 - mov low

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 7

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 9
	eor	v10.16b, v10.16b, v30.16b                        //MODULO - karatsuba tidy up

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 8

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 9

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 10
	eor	v10.16b, v10.16b, v31.16b                     //MODULO - fold into mid

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 9

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 9

	aese	v0.16b, v29.16b                                    //AES block 4k+4 - round 11

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 10
	eor	v10.16b, v10.16b, v9.16b                        //MODULO - fold into mid

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 10

	eor	v4.16b, v4.16b, v0.16b                         //AES block 4k+4 - result
	fmov	d0, x10                              //CTR block 4k+8

	aese	v1.16b, v29.16b                                    //AES block 4k+5 - round 11
	fmov	v0.d[1], x9                              //CTR block 4k+8
	rev	w9, w12                                //CTR block 4k+9

	pmull	v9.1q, v10.1d, v8.1d           //MODULO - mid 64b align with low
	fmov	v6.d[1], x22                          //AES block 4k+6 - mov high
	st1	{ v4.16b}, [x2], #16                    //AES block 4k+4 - store result

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 10
	orr	x9, x11, x9, lsl #32           //CTR block 4k+9

	eor	v5.16b, v5.16b, v1.16b                         //AES block 4k+5 - result
	add	w12, w12, #1                           //CTR block 4k+9
	fmov	d1, x10                              //CTR block 4k+9

	aese	v2.16b, v29.16b                                    //AES block 4k+6 - round 11
	fmov	v1.d[1], x9                              //CTR block 4k+9
	rev	w9, w12                                //CTR block 4k+10

	add	w12, w12, #1                           //CTR block 4k+10
	ext	v10.16b, v10.16b, v10.16b, #8                    //MODULO - other mid alignment
	orr	x9, x11, x9, lsl #32           //CTR block 4k+10

	st1	{ v5.16b}, [x2], #16                    //AES block 4k+5 - store result
	eor	v11.16b, v11.16b, v9.16b                        //MODULO - fold into low

	aese	v3.16b, v29.16b                                    //AES block 4k+7 - round 11
	eor	v6.16b, v6.16b, v2.16b                         //AES block 4k+6 - result
	fmov	d2, x10                              //CTR block 4k+10

	st1	{ v6.16b}, [x2], #16                    //AES block 4k+6 - store result
	fmov	v2.d[1], x9                              //CTR block 4k+10
	rev	w9, w12                                //CTR block 4k+11

	eor	v11.16b, v11.16b, v10.16b                        //MODULO - fold into low
	orr	x9, x11, x9, lsl #32           //CTR block 4k+11

	eor	v7.16b, v7.16b, v3.16b                         //AES block 4k+3 - result
	st1	{ v7.16b}, [x2], #16                    //AES block 4k+3 - store result
	b.lt	L192_enc_main_loop

L192_enc_prepretail:	//PREPRETAIL
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 0
	rev64	v4.16b, v4.16b                                   //GHASH block 4k (only t0 is free)

	fmov	d3, x10                              //CTR block 4k+3
	ext	v11.16b, v11.16b, v11.16b, #8                    //PRE 0
	add	w12, w12, #1                           //CTR block 4k+3

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 0
	rev64	v5.16b, v5.16b                                   //GHASH block 4k+1 (t0 and t1 free)

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 0

	fmov	v3.d[1], x9                              //CTR block 4k+3
	eor	v4.16b, v4.16b, v11.16b                          //PRE 1
	mov	d10, v17.d[1]                              //GHASH block 4k - mid

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 1
	rev64	v6.16b, v6.16b                                   //GHASH block 4k+2 (t0, t1, and t2 free)

	pmull2	v30.1q, v5.2d, v14.2d                         //GHASH block 4k+1 - high

	pmull	v11.1q, v4.1d, v15.1d                      //GHASH block 4k - low
	mov	d8, v4.d[1]                                 //GHASH block 4k - mid

	pmull	v31.1q, v5.1d, v14.1d                         //GHASH block 4k+1 - low
	rev64	v7.16b, v7.16b                                   //GHASH block 4k+3 (t0, t1, t2 and t3 free)

	pmull2	v9.1q, v4.2d, v15.2d                      //GHASH block 4k - high

	eor	v8.8b, v8.8b, v4.8b                         //GHASH block 4k - mid
	mov	d4, v5.d[1]                                 //GHASH block 4k+1 - mid

	eor	v11.16b, v11.16b, v31.16b                        //GHASH block 4k+1 - low
	mov	d31, v6.d[1]                                 //GHASH block 4k+2 - mid

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 0
	eor	v9.16b, v9.16b, v30.16b                        //GHASH block 4k+1 - high

	pmull2	v30.1q, v6.2d, v13.2d                         //GHASH block 4k+2 - high

	eor	v4.8b, v4.8b, v5.8b                         //GHASH block 4k+1 - mid
	eor	v31.8b, v31.8b, v6.8b                         //GHASH block 4k+2 - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 1

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 1
	eor	v9.16b, v9.16b, v30.16b                        //GHASH block 4k+2 - high

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 1

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 2
	mov	d30, v7.d[1]                                 //GHASH block 4k+3 - mid

	pmull2	v5.1q, v7.2d, v12.2d                         //GHASH block 4k+3 - high
	ins	v31.d[1], v31.d[0]                               //GHASH block 4k+2 - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 2

	pmull	v10.1q, v8.1d, v10.1d                     //GHASH block 4k - mid
	eor	v30.8b, v30.8b, v7.8b                         //GHASH block 4k+3 - mid

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 3

	pmull2	v31.1q, v31.2d, v16.2d                         //GHASH block 4k+2 - mid

	pmull	v4.1q, v4.1d, v17.1d                         //GHASH block 4k+1 - mid

	pmull	v30.1q, v30.1d, v16.1d                         //GHASH block 4k+3 - mid
	eor	v9.16b, v9.16b, v5.16b                        //GHASH block 4k+3 - high

	pmull	v8.1q, v6.1d, v13.1d                         //GHASH block 4k+2 - low

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 3
	eor	v10.16b, v10.16b, v4.16b                        //GHASH block 4k+1 - mid

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 2
	eor	v11.16b, v11.16b, v8.16b                        //GHASH block 4k+2 - low

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 4

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 3
	eor	v10.16b, v10.16b, v31.16b                        //GHASH block 4k+2 - mid

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 3

	pmull	v6.1q, v7.1d, v12.1d                         //GHASH block 4k+3 - low
	movi	v8.8b, #0xc2

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 4

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 4

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 4
	eor	v10.16b, v10.16b, v30.16b                        //GHASH block 4k+3 - mid

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 5

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 5
	eor	v11.16b, v11.16b, v6.16b                        //GHASH block 4k+3 - low

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 5

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 6
	eor	v10.16b, v10.16b, v9.16b                        //karatsuba tidy up

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 6
	shl	d8, d8, #56              //mod_constant

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 7

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 7
	eor	v10.16b, v10.16b, v11.16b

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 7

	pmull	v30.1q, v9.1d, v8.1d

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 6
	ext	v9.16b, v9.16b, v9.16b, #8

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 8

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v30.16b

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 8

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 8
	eor	v10.16b, v10.16b, v9.16b

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 9

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 9

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 9

	pmull	v30.1q, v10.1d, v8.1d

	ext	v10.16b, v10.16b, v10.16b, #8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b         //AES block 4k+7 - round 10

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b         //AES block 4k+4 - round 10

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b         //AES block 4k+6 - round 10

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b         //AES block 4k+5 - round 10
	eor	v11.16b, v11.16b, v30.16b

	aese	v0.16b, v29.16b                                    //AES block 4k+4 - round 11

	aese	v3.16b, v29.16b                                    //AES block 4k+7 - round 11

	aese	v2.16b, v29.16b                                    //AES block 4k+6 - round 11

	aese	v1.16b, v29.16b                                    //AES block 4k+5 - round 11
	eor	v11.16b, v11.16b, v10.16b
L192_enc_tail:	//TAIL

	sub	x5, x4, x0  //main_end_input_ptr is number of bytes left to process
	ldp	x6, x7, [x0], #16          //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	x6, x6, x13                    //AES block 4k+4 - round 12 low
	eor	x7, x7, x14                    //AES block 4k+4 - round 12 high

	fmov	d4, x6                              //AES block 4k+4 - mov low

	fmov	v4.d[1], x7                          //AES block 4k+4 - mov high
	cmp	x5, #48

	eor	v5.16b, v4.16b, v0.16b                         //AES block 4k+4 - result

	ext	v8.16b, v11.16b, v11.16b, #8                    //prepare final partial tag
	b.gt	L192_enc_blocks_more_than_3

	sub	w12, w12, #1
	movi	v10.8b, #0

	mov	v3.16b, v2.16b
	movi	v9.8b, #0
	cmp	x5, #32

	mov	v2.16b, v1.16b
	movi	v11.8b, #0
	b.gt	L192_enc_blocks_more_than_2

	sub	w12, w12, #1

	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	L192_enc_blocks_more_than_1

	sub	w12, w12, #1
	b	L192_enc_blocks_less_than_1
L192_enc_blocks_more_than_3:	//blocks	left >  3
	st1	{ v5.16b}, [x2], #16                    //AES final-3 block  - store result

	ldp	x6, x7, [x0], #16          //AES final-2 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	rev64	v4.16b, v5.16b                                   //GHASH final-3 block

	eor	x6, x6, x13                    //AES final-2 block - round 12 low
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	eor	x7, x7, x14                    //AES final-2 block - round 12 high
	fmov	d5, x6                                //AES final-2 block - mov low

	fmov	v5.d[1], x7                            //AES final-2 block - mov high

	mov	d22, v4.d[1]                                //GHASH final-3 block - mid

	pmull	v11.1q, v4.1d, v15.1d                      //GHASH final-3 block - low

	mov	d10, v17.d[1]                              //GHASH final-3 block - mid

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-3 block - mid

	movi	v8.8b, #0                                       //suppress further partial tag feed in

	pmull2	v9.1q, v4.2d, v15.2d                      //GHASH final-3 block - high

	pmull	v10.1q, v22.1d, v10.1d                   //GHASH final-3 block - mid
	eor	v5.16b, v5.16b, v1.16b                           //AES final-2 block - result
L192_enc_blocks_more_than_2:	//blocks	left >  2

	st1	{ v5.16b}, [x2], #16                    //AES final-2 block - store result

	rev64	v4.16b, v5.16b                                   //GHASH final-2 block
	ldp	x6, x7, [x0], #16          //AES final-1 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	eor	x7, x7, x14                    //AES final-1 block - round 12 high

	pmull2	v20.1q, v4.2d, v14.2d                         //GHASH final-2 block - high
	mov	d22, v4.d[1]                                //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                         //GHASH final-2 block - low
	eor	x6, x6, x13                    //AES final-1 block - round 12 low

	fmov	d5, x6                                //AES final-1 block - mov low

	fmov	v5.d[1], x7                            //AES final-1 block - mov high
	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-2 block - high
	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-2 block - mid

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-2 block - low

	pmull	v22.1q, v22.1d, v17.1d                     //GHASH final-2 block - mid

	movi	v8.8b, #0                                       //suppress further partial tag feed in

	eor	v5.16b, v5.16b, v2.16b                           //AES final-1 block - result

	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-2 block - mid
L192_enc_blocks_more_than_1:	//blocks	left >  1

	st1	{ v5.16b}, [x2], #16                    //AES final-1 block - store result

	ldp	x6, x7, [x0], #16          //AES final block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	rev64	v4.16b, v5.16b                                   //GHASH final-1 block

	eor	x6, x6, x13                    //AES final block - round 12 low
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag
	movi	v8.8b, #0                                       //suppress further partial tag feed in

	mov	d22, v4.d[1]                                //GHASH final-1 block - mid

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-1 block - mid
	eor	x7, x7, x14                    //AES final block - round 12 high
	fmov	d5, x6                                //AES final block - mov low

	pmull2	v20.1q, v4.2d, v13.2d                         //GHASH final-1 block - high
	fmov	v5.d[1], x7                            //AES final block - mov high

	ins	v22.d[1], v22.d[0]                           //GHASH final-1 block - mid

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-1 block - high

	pmull	v21.1q, v4.1d, v13.1d                         //GHASH final-1 block - low

	pmull2	v22.1q, v22.2d, v16.2d                     //GHASH final-1 block - mid

	eor	v5.16b, v5.16b, v3.16b                           //AES final block - result

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-1 block - low

	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-1 block - mid
L192_enc_blocks_less_than_1:	//blocks	left <= 1

	ld1	{ v18.16b}, [x2]                           //load existing bytes where the possibly partial last block is to be stored
#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif
	and	x1, x1, #127                   //bit_length %= 128

	sub	x1, x1, #128                   //bit_length -= 128
	mvn	x14, xzr                                     //rk12_h = 0xffffffffffffffff

	neg	x1, x1                         //bit_length = 128 - #bits in input (in range [1,128])
	mvn	x13, xzr                                     //rk12_l = 0xffffffffffffffff

	and	x1, x1, #127                   //bit_length %= 128

	lsr	x14, x14, x1                    //rk12_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x6, x13, x14, lt
	csel	x7, x14, xzr, lt

	fmov	d0, x6                                //ctr0b is mask for last block

	fmov	v0.d[1], x7

	and	v5.16b, v5.16b, v0.16b                           //possibly partial last block has zeroes in highest bits

	rev64	v4.16b, v5.16b                                   //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	mov	d8, v4.d[1]                                 //GHASH final block - mid

	pmull	v21.1q, v4.1d, v12.1d                         //GHASH final block - low

	pmull2	v20.1q, v4.2d, v12.2d                         //GHASH final block - high

	eor	v8.8b, v8.8b, v4.8b                         //GHASH final block - mid

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final block - low

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final block - high

	pmull	v8.1q, v8.1d, v16.1d                         //GHASH final block - mid

	eor	v10.16b, v10.16b, v8.16b                        //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v30.16b, v11.16b, v9.16b                        //MODULO - karatsuba tidy up

	shl	d8, d8, #56              //mod_constant

	bif	v5.16b, v18.16b, v0.16b                             //insert existing bytes in top end of result before storing

	eor	v10.16b, v10.16b, v30.16b                        //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d           //MODULO - top 64b align with mid

	ext	v9.16b, v9.16b, v9.16b, #8                    //MODULO - other top alignment

	eor	v10.16b, v10.16b, v31.16b                     //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                        //MODULO - fold into mid

	pmull	v9.1q, v10.1d, v8.1d           //MODULO - mid 64b align with low

	ext	v10.16b, v10.16b, v10.16b, #8                    //MODULO - other mid alignment

	eor	v11.16b, v11.16b, v9.16b                        //MODULO - fold into low
	str	w9, [x16, #12]                         //store the updated counter

	st1	{ v5.16b}, [x2]                         //store all 16B

	eor	v11.16b, v11.16b, v10.16b                        //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]

	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L192_enc_ret:
	mov	w0, #0x0
	ret

.globl	_aes_gcm_dec_192_kernel

.align	4
_aes_gcm_dec_192_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L192_dec_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	add	x4, x0, x1, lsr #3   //end_input_ptr
	ldp	x10, x11, [x16]              //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #192]                     //load rk12
#ifdef __AARCH64EB__
	ror	x13, x13, #32
	ror	x14, x14, #32
#endif
	ld1	{ v0.16b}, [x16]                             //special case vector load initial counter so we can start first AES block as quickly as possible

	ld1	{v18.4s}, [x8], #16                                  //load rk0

	lsr	x5, x1, #3              //byte_len
	mov	x15, x5
	ld1	{v19.4s}, [x8], #16                               //load rk1

	lsr	x12, x11, #32
	orr	w11, w11, w11
	fmov	d3, x10                               //CTR block 3

	rev	w12, w12                                //rev_ctr32
	fmov	d1, x10                               //CTR block 1

	add	w12, w12, #1                            //increment rev_ctr32
	ld1	{v20.4s}, [x8], #16                               //load rk2

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 0
	rev	w9, w12                                 //CTR block 1

	add	w12, w12, #1                            //CTR block 1
	orr	x9, x11, x9, lsl #32            //CTR block 1
	ld1	{v21.4s}, [x8], #16                               //load rk3

	fmov	v1.d[1], x9                               //CTR block 1
	rev	w9, w12                                 //CTR block 2
	add	w12, w12, #1                            //CTR block 2

	fmov	d2, x10                               //CTR block 2
	orr	x9, x11, x9, lsl #32            //CTR block 2

	fmov	v2.d[1], x9                               //CTR block 2
	rev	w9, w12                                 //CTR block 3

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 1
	orr	x9, x11, x9, lsl #32            //CTR block 3

	fmov	v3.d[1], x9                               //CTR block 3

	ld1	{v22.4s}, [x8], #16                               //load rk4

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 2

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 0
	ld1	{v23.4s}, [x8], #16                               //load rk5

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 0
	ldr	q15, [x3, #112]                        //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 0
	ldr	q13, [x3, #64]                         //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 1
	ldr	q14, [x3, #80]                         //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif
	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 1

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 1
	ldr	q12, [x3, #32]                         //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 2
	ld1	{v24.4s}, [x8], #16                               //load rk6

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 3
	ld1	{v25.4s}, [x8], #16                               //load rk7

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 2
	ld1	{v26.4s}, [x8], #16                               //load rk8

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 2
	ld1	{v27.4s}, [x8], #16                               //load rk9

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 3
	ld1	{ v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 3
	add	w12, w12, #1                            //CTR block 3

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 3
	trn1	v9.2d, v14.2d,    v15.2d                      //h4h | h3h

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 4
	ld1	{v28.4s}, [x8], #16                              //load rk10

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 4
	trn2	v17.2d,  v14.2d,    v15.2d                      //h4l | h3l

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 4

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 4
	trn2	v16.2d,  v12.2d,    v13.2d                      //h2l | h1l

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 5
	ld1	{v29.4s}, [x8], #16                              //load rk11

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 5

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 5

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 6

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 6

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 6

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 7

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 7

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 6

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 8

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 7

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 9

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 9

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 8
	sub	x5, x5, #1      //byte_len - 1

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 8
	and	x5, x5, #0xffffffffffffffc0    //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 10
	add	x5, x5, x0

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 9
	cmp	x0, x5                   //check if we have <= 4 blocks

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 9
	trn1	v8.2d,    v12.2d,    v13.2d                      //h2h | h1h

	aese	v3.16b, v29.16b                                     //AES block 3 - round 11

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 10

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 10

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 10
	eor	v16.16b, v16.16b, v8.16b                     //h2k | h1k

	aese	v2.16b, v29.16b                                     //AES block 2 - round 11

	aese	v1.16b, v29.16b                                     //AES block 1 - round 11
	eor	v17.16b, v17.16b, v9.16b                  //h4k | h3k

	aese	v0.16b, v29.16b                                     //AES block 0 - round 11
	b.ge	L192_dec_tail                                    //handle tail

	ld1	{v4.16b, v5.16b}, [x0], #32               //AES block 0,1 - load ciphertext

	eor	v1.16b, v5.16b, v1.16b                            //AES block 1 - result

	eor	v0.16b, v4.16b, v0.16b                            //AES block 0 - result
	rev	w9, w12                                 //CTR block 4
	ld1	{v6.16b, v7.16b}, [x0], #32               //AES block 2,3 - load ciphertext

	mov	x19, v1.d[0]                            //AES block 1 - mov low

	mov	x20, v1.d[1]                            //AES block 1 - mov high

	mov	x6, v0.d[0]                            //AES block 0 - mov low
	orr	x9, x11, x9, lsl #32            //CTR block 4
	add	w12, w12, #1                            //CTR block 4

	mov	x7, v0.d[1]                            //AES block 0 - mov high
	rev64	v4.16b, v4.16b                                    //GHASH block 0

	fmov	d0, x10                               //CTR block 4
	rev64	v5.16b, v5.16b                                    //GHASH block 1
	cmp	x0, x5                   //check if we have <= 8 blocks

	eor	x19, x19, x13                   //AES block 1 - round 12 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	fmov	v0.d[1], x9                               //CTR block 4
	rev	w9, w12                                 //CTR block 5

	orr	x9, x11, x9, lsl #32            //CTR block 5
	fmov	d1, x10                               //CTR block 5
	eor	x20, x20, x14                   //AES block 1 - round 12 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	add	w12, w12, #1                            //CTR block 5
	fmov	v1.d[1], x9                               //CTR block 5
	eor	x6, x6, x13                   //AES block 0 - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	rev	w9, w12                                 //CTR block 6
	eor	x7, x7, x14                   //AES block 0 - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	stp	x6, x7, [x2], #16        //AES block 0 - store result
	orr	x9, x11, x9, lsl #32            //CTR block 6

	stp	x19, x20, [x2], #16        //AES block 1 - store result

	add	w12, w12, #1                            //CTR block 6
	eor	v2.16b, v6.16b, v2.16b                            //AES block 2 - result
	b.ge	L192_dec_prepretail                              //do prepretail

L192_dec_main_loop:	//main	loop start
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0

	pmull	v31.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low

	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	fmov	d2, x10                               //CTR block 4k+6

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1

	pmull2	v30.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high
	fmov	v2.d[1], x9                               //CTR block 4k+6

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	fmov	d3, x10                               //CTR block 4k+7
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d10, v17.d[1]                               //GHASH block 4k - mid
	rev	w9, w12                                 //CTR block 4k+7

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	orr	x9, x11, x9, lsl #32            //CTR block 4k+7

	fmov	v3.d[1], x9                               //CTR block 4k+7
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2
	eor	x22, x22, x14                   //AES block 4k+2 - round 12 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid
	eor	v11.16b, v11.16b, v31.16b                         //GHASH block 4k+1 - low
	eor	x21, x21, x13                   //AES block 4k+2 - round 12 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif
	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3

	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid
	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	eor	v9.16b, v9.16b, v30.16b                         //GHASH block 4k+1 - high

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4

	pmull2	v30.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid

	pmull	v8.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5

	eor	v9.16b, v9.16b, v30.16b                         //GHASH block 4k+2 - high
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3

	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+2 - low

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	movi	v8.8b, #0xc2

	pmull	v6.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9
	eor	v11.16b, v11.16b, v6.16b                         //GHASH block 4k+3 - low

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9
	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	shl	d8, d8, #56               //mod_constant

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	ld1	{v4.16b}, [x0], #16                       //AES block 4k+4 - load ciphertext

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	ld1	{v5.16b}, [x0], #16                       //AES block 4k+5 - load ciphertext
	eor	x23, x23, x13                   //AES block 4k+3 - round 12 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v0.16b, v29.16b                                     //AES block 4k+4 - round 11
	add	w12, w12, #1                            //CTR block 4k+7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	ld1	{v6.16b}, [x0], #16                       //AES block 4k+6 - load ciphertext

	aese	v1.16b, v29.16b                                     //AES block 4k+5 - round 11
	ld1	{v7.16b}, [x0], #16                       //AES block 4k+7 - load ciphertext
	rev	w9, w12                                 //CTR block 4k+8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	cmp	x0, x5                   //LOOP CONTROL

	eor	v0.16b, v4.16b, v0.16b                            //AES block 4k+4 - result
	eor	x24, x24, x14                   //AES block 4k+3 - round 12 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif
	eor	v1.16b, v5.16b, v1.16b                            //AES block 4k+5 - result

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10
	orr	x9, x11, x9, lsl #32            //CTR block 4k+8

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	mov	x19, v1.d[0]                            //AES block 4k+5 - mov low

	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result
	rev64	v5.16b, v5.16b                                    //GHASH block 4k+5

	aese	v2.16b, v29.16b                                     //AES block 4k+6 - round 11
	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10
	mov	x20, v1.d[1]                            //AES block 4k+5 - mov high

	fmov	d0, x10                               //CTR block 4k+8
	add	w12, w12, #1                            //CTR block 4k+8
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	eor	v2.16b, v6.16b, v2.16b                            //AES block 4k+6 - result
	fmov	v0.d[1], x9                               //CTR block 4k+8
	rev	w9, w12                                 //CTR block 4k+9

	eor	x6, x6, x13                   //AES block 4k+4 - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	orr	x9, x11, x9, lsl #32            //CTR block 4k+9
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	fmov	d1, x10                               //CTR block 4k+9
	add	w12, w12, #1                            //CTR block 4k+9
	eor	x19, x19, x13                   //AES block 4k+5 - round 12 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	fmov	v1.d[1], x9                               //CTR block 4k+9
	rev	w9, w12                                 //CTR block 4k+10
	eor	x20, x20, x14                   //AES block 4k+5 - round 12 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	eor	x7, x7, x14                   //AES block 4k+4 - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	stp	x6, x7, [x2], #16        //AES block 4k+4 - store result
	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low

	add	w12, w12, #1                            //CTR block 4k+10
	rev64	v4.16b, v4.16b                                    //GHASH block 4k+4
	orr	x9, x11, x9, lsl #32            //CTR block 4k+10

	aese	v3.16b, v29.16b                                     //AES block 4k+7 - round 11
	stp	x19, x20, [x2], #16        //AES block 4k+5 - store result
	b.lt	L192_dec_main_loop

L192_dec_prepretail:	//PREPRETAIL
	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	eor	v4.16b, v4.16b, v11.16b                           //PRE 1
	fmov	d2, x10                               //CTR block 4k+6

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid
	fmov	d3, x10                               //CTR block 4k+7

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	fmov	v2.d[1], x9                               //CTR block 4k+6
	rev	w9, w12                                 //CTR block 4k+7

	orr	x9, x11, x9, lsl #32            //CTR block 4k+7
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	pmull	v31.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	eor	x24, x24, x14                   //AES block 4k+3 - round 12 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif
	fmov	v3.d[1], x9                               //CTR block 4k+7

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2
	eor	x21, x21, x13                   //AES block 4k+2 - round 12 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif
	pmull2	v30.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high
	eor	x22, x22, x14                   //AES block 4k+2 - round 12 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	eor	x23, x23, x13                   //AES block 4k+3 - round 12 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	eor	v9.16b, v9.16b, v30.16b                         //GHASH block 4k+1 - high

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid
	add	w12, w12, #1                            //CTR block 4k+7

	pmull2	v30.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	eor	v11.16b, v11.16b, v31.16b                         //GHASH block 4k+1 - low

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0

	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid
	mov	d31, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	eor	v9.16b, v9.16b, v30.16b                         //GHASH block 4k+2 - high

	eor	v31.8b, v31.8b, v6.8b                          //GHASH block 4k+2 - mid

	pmull	v8.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	mov	d30, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	ins	v31.d[1], v31.d[0]                                //GHASH block 4k+2 - mid

	pmull	v6.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	eor	v30.8b, v30.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3

	pmull2	v31.1q, v31.2d, v16.2d                          //GHASH block 4k+2 - mid
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+2 - low

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high
	movi	v8.8b, #0xc2

	pmull	v30.1q, v30.1d, v16.1d                          //GHASH block 4k+3 - mid

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	shl	d8, d8, #56               //mod_constant
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	eor	v10.16b, v10.16b, v31.16b                         //GHASH block 4k+2 - mid

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	eor	v11.16b, v11.16b, v6.16b                         //GHASH block 4k+3 - low

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v10.16b, v10.16b, v30.16b                         //GHASH block 4k+3 - mid

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7
	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10

	aese	v0.16b, v29.16b
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	aese	v2.16b, v29.16b

	aese	v1.16b, v29.16b

	aese	v3.16b, v29.16b

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
L192_dec_tail:	//TAIL

	sub	x5, x4, x0   //main_end_input_ptr is number of bytes left to process
	ld1	{ v5.16b}, [x0], #16                      //AES block 4k+4 - load ciphertext

	eor	v0.16b, v5.16b, v0.16b                            //AES block 4k+4 - result

	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high

	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low

	ext	v8.16b, v11.16b, v11.16b, #8                     //prepare final partial tag

	cmp	x5, #48

	eor	x7, x7, x14                   //AES block 4k+4 - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	x6, x6, x13                   //AES block 4k+4 - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	b.gt	L192_dec_blocks_more_than_3

	movi	v11.8b, #0
	movi	v9.8b, #0

	mov	v3.16b, v2.16b
	mov	v2.16b, v1.16b
	sub	w12, w12, #1

	movi	v10.8b, #0
	cmp	x5, #32
	b.gt	L192_dec_blocks_more_than_2

	mov	v3.16b, v1.16b
	cmp	x5, #16
	sub	w12, w12, #1

	b.gt	L192_dec_blocks_more_than_1

	sub	w12, w12, #1
	b	L192_dec_blocks_less_than_1
L192_dec_blocks_more_than_3:	//blocks	left >  3
	rev64	v4.16b, v5.16b                                    //GHASH final-3 block
	ld1	{ v5.16b}, [x0], #16                      //AES final-2 block - load ciphertext

	stp	x6, x7, [x2], #16        //AES final-3 block  - store result

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	eor	v0.16b, v5.16b, v1.16b                            //AES final-2 block - result

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH final-3 block - low
	mov	x6, v0.d[0]                            //AES final-2 block - mov low
	mov	d22, v4.d[1]                                 //GHASH final-3 block - mid

	mov	x7, v0.d[1]                            //AES final-2 block - mov high

	mov	d10, v17.d[1]                               //GHASH final-3 block - mid
	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-3 block - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH final-3 block - high

	eor	x6, x6, x13                   //AES final-2 block - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	movi	v8.8b, #0                                        //suppress further partial tag feed in

	pmull	v10.1q, v22.1d, v10.1d                    //GHASH final-3 block - mid
	eor	x7, x7, x14                   //AES final-2 block - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
L192_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v4.16b, v5.16b                                    //GHASH final-2 block
	ld1	{ v5.16b}, [x0], #16                      //AES final-1 block - load ciphertext

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	movi	v8.8b, #0                                        //suppress further partial tag feed in

	eor	v0.16b, v5.16b, v2.16b                            //AES final-1 block - result

	mov	d22, v4.d[1]                                 //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                          //GHASH final-2 block - low

	stp	x6, x7, [x2], #16        //AES final-2 block  - store result

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-2 block - mid
	mov	x7, v0.d[1]                            //AES final-1 block - mov high

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-2 block - low
	mov	x6, v0.d[0]                            //AES final-1 block - mov low

	pmull2	v20.1q, v4.2d, v14.2d                          //GHASH final-2 block - high

	pmull	v22.1q, v22.1d, v17.1d                      //GHASH final-2 block - mid

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-2 block - high
	eor	x7, x7, x14                   //AES final-1 block - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	x6, x6, x13                   //AES final-1 block - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-2 block - mid
L192_dec_blocks_more_than_1:	//blocks	left >  1

	rev64	v4.16b, v5.16b                                    //GHASH final-1 block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag
	ld1	{ v5.16b}, [x0], #16                      //AES final block - load ciphertext

	mov	d22, v4.d[1]                                 //GHASH final-1 block - mid

	pmull2	v20.1q, v4.2d, v13.2d                          //GHASH final-1 block - high

	eor	v0.16b, v5.16b, v3.16b                            //AES final block - result
	stp	x6, x7, [x2], #16        //AES final-1 block  - store result

	eor	v22.8b, v22.8b, v4.8b                      //GHASH final-1 block - mid

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final-1 block - high

	pmull	v21.1q, v4.1d, v13.1d                          //GHASH final-1 block - low
	mov	x7, v0.d[1]                            //AES final block - mov high

	ins	v22.d[1], v22.d[0]                            //GHASH final-1 block - mid
	mov	x6, v0.d[0]                            //AES final block - mov low

	pmull2	v22.1q, v22.2d, v16.2d                      //GHASH final-1 block - mid

	movi	v8.8b, #0                                        //suppress further partial tag feed in
	eor	v11.16b, v11.16b, v21.16b                            //GHASH final-1 block - low
	eor	x7, x7, x14                   //AES final block - round 12 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	x6, x6, x13                   //AES final block - round 12 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v10.16b, v10.16b, v22.16b                       //GHASH final-1 block - mid
L192_dec_blocks_less_than_1:	//blocks	left <= 1

	mvn	x13, xzr                                      //rk12_l = 0xffffffffffffffff
	ldp	x4, x5, [x2]  //load existing bytes we need to not overwrite
	and	x1, x1, #127                    //bit_length %= 128

	sub	x1, x1, #128                    //bit_length -= 128

	neg	x1, x1                          //bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127                    //bit_length %= 128
	mvn	x14, xzr                                      //rk12_h = 0xffffffffffffffff

	lsr	x14, x14, x1                     //rk12_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x9, x13, x14, lt
	csel	x10, x14, xzr, lt

	fmov	d0, x9                                   //ctr0b is mask for last block
	and	x6, x6, x9
	bic	x4, x4, x9           //mask out low existing bytes

	orr	x6, x6, x4
	mov	v0.d[1], x10
#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif

	and	v5.16b, v5.16b, v0.16b                            //possibly partial last block has zeroes in highest bits
	str	w9, [x16, #12]                          //store the updated counter

	rev64	v4.16b, v5.16b                                    //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag
	bic	x5, x5, x10 //mask out high existing bytes

	and	x7, x7, x10

	pmull2	v20.1q, v4.2d, v12.2d                          //GHASH final block - high
	mov	d8, v4.d[1]                                  //GHASH final block - mid

	pmull	v21.1q, v4.1d, v12.1d                          //GHASH final block - low

	eor	v8.8b, v8.8b, v4.8b                          //GHASH final block - mid

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final block - high

	pmull	v8.1q, v8.1d, v16.1d                          //GHASH final block - mid

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final block - low

	eor	v10.16b, v10.16b, v8.16b                         //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v30.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	shl	d8, d8, #56               //mod_constant

	eor	v10.16b, v10.16b, v30.16b                         //MODULO - karatsuba tidy up

	pmull	v31.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	orr	x7, x7, x5
	stp	x6, x7, [x2]

	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	eor	v10.16b, v10.16b, v31.16b                      //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low

	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]

	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L192_dec_ret:
	mov	w0, #0x0
	ret

.globl	_aes_gcm_enc_256_kernel

.align	4
_aes_gcm_enc_256_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L256_enc_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	add	x4, x0, x1, lsr #3   //end_input_ptr
	lsr	x5, x1, #3              //byte_len
	mov	x15, x5
	ldp	x10, x11, [x16]              //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #224]                     //load rk14
#ifdef __AARCH64EB__
	ror	x13, x13, #32
	ror	x14, x14, #32
#endif
	ld1	{ v0.16b}, [x16]                             //special case vector load initial counter so we can start first AES block as quickly as possible
	sub	x5, x5, #1      //byte_len - 1

	ld1	{v18.4s}, [x8], #16                               //load rk0
	and	x5, x5, #0xffffffffffffffc0 //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	ld1	{v19.4s}, [x8], #16                               //load rk1
	add	x5, x5, x0

	lsr	x12, x11, #32
	fmov	d2, x10                               //CTR block 2
	orr	w11, w11, w11

	rev	w12, w12                                //rev_ctr32
	cmp	x0, x5                   //check if we have <= 4 blocks
	fmov	d1, x10                               //CTR block 1

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 0
	add	w12, w12, #1                            //increment rev_ctr32

	rev	w9, w12                                 //CTR block 1
	fmov	d3, x10                               //CTR block 3

	orr	x9, x11, x9, lsl #32            //CTR block 1
	add	w12, w12, #1                            //CTR block 1
	ld1	{v20.4s}, [x8], #16                               //load rk2

	fmov	v1.d[1], x9                               //CTR block 1
	rev	w9, w12                                 //CTR block 2
	add	w12, w12, #1                            //CTR block 2

	orr	x9, x11, x9, lsl #32            //CTR block 2
	ld1	{v21.4s}, [x8], #16                               //load rk3

	fmov	v2.d[1], x9                               //CTR block 2
	rev	w9, w12                                 //CTR block 3

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 1
	orr	x9, x11, x9, lsl #32            //CTR block 3

	fmov	v3.d[1], x9                               //CTR block 3

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 0
	ld1	{v22.4s}, [x8], #16                               //load rk4

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 2
	ld1	{v23.4s}, [x8], #16                               //load rk5

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 0
	ld1	{v24.4s}, [x8], #16                               //load rk6

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 1
	ldr	q14, [x3, #80]                         //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif
	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 0
	ld1	{v25.4s}, [x8], #16                               //load rk7

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 1
	ld1	{v26.4s}, [x8], #16                               //load rk8

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 2
	ldr	q13, [x3, #64]                         //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif
	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 1
	ld1	{v27.4s}, [x8], #16                               //load rk9

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 2
	ldr	q15, [x3, #112]                        //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif
	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 3
	ld1	{v28.4s}, [x8], #16                              //load rk10

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 2
	ld1	{v29.4s}, [x8], #16                              //load rk11

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 3
	add	w12, w12, #1                            //CTR block 3

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 3

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 3
	ld1	{ v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 4

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 4

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 4

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 4

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 5

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 5

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 5

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 6
	trn2	v17.2d,  v14.2d,    v15.2d                      //h4l | h3l

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 6
	ld1	{v30.4s}, [x8], #16                              //load rk12

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 6
	ldr	q12, [x3, #32]                         //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif
	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 6
	ld1	{v31.4s}, [x8], #16                              //load rk13

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 7
	trn1	v9.2d, v14.2d,    v15.2d                      //h4h | h3h

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 7

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 7
	trn2	v16.2d,  v12.2d,    v13.2d                      //h2l | h1l

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 8

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 8

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 8

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 9

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 9

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 8

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 10

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 9

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 9

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 10

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 10

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 11

	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 11

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 10

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 12

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 12

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 11
	eor	v17.16b, v17.16b, v9.16b                  //h4k | h3k

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 11

	aese	v2.16b, v31.16b                                     //AES block 2 - round 13
	trn1	v8.2d,    v12.2d,    v13.2d                      //h2h | h1h

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 12

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 12

	aese	v1.16b, v31.16b                                     //AES block 1 - round 13

	aese	v0.16b, v31.16b                                     //AES block 0 - round 13

	aese	v3.16b, v31.16b                                     //AES block 3 - round 13
	eor	v16.16b, v16.16b, v8.16b                     //h2k | h1k
	b.ge	L256_enc_tail                                    //handle tail

	ldp	x19, x20, [x0, #16]           //AES block 1 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	rev	w9, w12                                 //CTR block 4
	ldp	x6, x7, [x0, #0]            //AES block 0 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	ldp	x23, x24, [x0, #48]           //AES block 3 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	ldp	x21, x22, [x0, #32]           //AES block 2 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	add	x0, x0, #64                       //AES input_ptr update

	eor	x19, x19, x13                     //AES block 1 - round 14 low
	eor	x20, x20, x14                     //AES block 1 - round 14 high

	fmov	d5, x19                               //AES block 1 - mov low
	eor	x6, x6, x13                     //AES block 0 - round 14 low

	eor	x7, x7, x14                     //AES block 0 - round 14 high
	eor	x24, x24, x14                     //AES block 3 - round 14 high
	fmov	d4, x6                               //AES block 0 - mov low

	cmp	x0, x5                   //check if we have <= 8 blocks
	fmov	v4.d[1], x7                           //AES block 0 - mov high
	eor	x23, x23, x13                     //AES block 3 - round 14 low

	eor	x21, x21, x13                     //AES block 2 - round 14 low
	fmov	v5.d[1], x20                           //AES block 1 - mov high

	fmov	d6, x21                               //AES block 2 - mov low
	add	w12, w12, #1                            //CTR block 4

	orr	x9, x11, x9, lsl #32            //CTR block 4
	fmov	d7, x23                               //AES block 3 - mov low
	eor	x22, x22, x14                     //AES block 2 - round 14 high

	fmov	v6.d[1], x22                           //AES block 2 - mov high

	eor	v4.16b, v4.16b, v0.16b                          //AES block 0 - result
	fmov	d0, x10                               //CTR block 4

	fmov	v0.d[1], x9                               //CTR block 4
	rev	w9, w12                                 //CTR block 5
	add	w12, w12, #1                            //CTR block 5

	eor	v5.16b, v5.16b, v1.16b                          //AES block 1 - result
	fmov	d1, x10                               //CTR block 5
	orr	x9, x11, x9, lsl #32            //CTR block 5

	fmov	v1.d[1], x9                               //CTR block 5
	rev	w9, w12                                 //CTR block 6
	st1	{ v4.16b}, [x2], #16                     //AES block 0 - store result

	fmov	v7.d[1], x24                           //AES block 3 - mov high
	orr	x9, x11, x9, lsl #32            //CTR block 6
	eor	v6.16b, v6.16b, v2.16b                          //AES block 2 - result

	st1	{ v5.16b}, [x2], #16                     //AES block 1 - store result

	add	w12, w12, #1                            //CTR block 6
	fmov	d2, x10                               //CTR block 6

	fmov	v2.d[1], x9                               //CTR block 6
	st1	{ v6.16b}, [x2], #16                     //AES block 2 - store result
	rev	w9, w12                                 //CTR block 7

	orr	x9, x11, x9, lsl #32            //CTR block 7

	eor	v7.16b, v7.16b, v3.16b                          //AES block 3 - result
	st1	{ v7.16b}, [x2], #16                     //AES block 3 - store result
	b.ge	L256_enc_prepretail                               //do prepretail

L256_enc_main_loop:	//main	loop start
	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	rev64	v4.16b, v4.16b                                    //GHASH block 4k (only t0 is free)

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	fmov	d3, x10                               //CTR block 4k+3

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	fmov	v3.d[1], x9                               //CTR block 4k+3

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	ldp	x23, x24, [x0, #48]           //AES block 4k+7 - load plaintext
#ifdef __AARCH64EB__
	rev	x23, x23
	rev	x24, x24
#endif
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	ldp	x21, x22, [x0, #32]           //AES block 4k+6 - load plaintext
#ifdef __AARCH64EB__
	rev	x21, x21
	rev	x22, x22
#endif
	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	eor	x23, x23, x13                     //AES block 4k+7 - round 14 low

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	eor	x22, x22, x14                     //AES block 4k+6 - round 14 high
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	rev64	v5.16b, v5.16b                                    //GHASH block 4k+1 (t0 and t1 free)

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3 (t0, t1, t2 and t3 free)

	pmull2	v4.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2 (t0, t1, and t2 free)

	pmull	v8.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low

	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+1 - high
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+1 - low

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4
	mov	d8, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	eor	v8.8b, v8.8b, v6.8b                          //GHASH block 4k+2 - mid

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	ins	v8.d[1], v8.d[0]                                //GHASH block 4k+2 - mid

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid

	pmull2	v4.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high

	pmull	v5.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7

	pmull	v6.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+2 - high

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	ldp	x19, x20, [x0, #16]           //AES block 4k+5 - load plaintext
#ifdef __AARCH64EB__
	rev	x19, x19
	rev	x20, x20
#endif
	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	mov	d4, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	eor	v11.16b, v11.16b, v5.16b                         //GHASH block 4k+2 - low

	pmull2	v8.1q, v8.2d, v16.2d                          //GHASH block 4k+2 - mid

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high
	eor	v4.8b, v4.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	eor	x19, x19, x13                     //AES block 4k+5 - round 14 low

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9
	eor	v10.16b, v10.16b, v8.16b                         //GHASH block 4k+2 - mid

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	eor	x21, x21, x13                     //AES block 4k+6 - round 14 low

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9
	movi	v8.8b, #0xc2

	pmull	v4.1q, v4.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high
	fmov	d5, x19                               //AES block 4k+5 - mov low

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	ldp	x6, x7, [x0, #0]            //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10
	shl	d8, d8, #56               //mod_constant

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8
	eor	v11.16b, v11.16b, v6.16b                         //GHASH block 4k+3 - low

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+3 - mid

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9
	add	w12, w12, #1                            //CTR block 4k+3

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 11
	eor	v4.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 11
	add	x0, x0, #64                       //AES input_ptr update

	pmull	v7.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	rev	w9, w12                                 //CTR block 4k+8
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10
	eor	x6, x6, x13                     //AES block 4k+4 - round 14 low

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 12
	eor	v10.16b, v10.16b, v4.16b                         //MODULO - karatsuba tidy up

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10
	eor	x7, x7, x14                     //AES block 4k+4 - round 14 high

	fmov	d4, x6                               //AES block 4k+4 - mov low
	orr	x9, x11, x9, lsl #32            //CTR block 4k+8
	eor	v7.16b, v9.16b, v7.16b                   //MODULO - fold into mid

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 12
	eor	x20, x20, x14                     //AES block 4k+5 - round 14 high

	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 11
	eor	x24, x24, x14                     //AES block 4k+7 - round 14 high

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 11
	add	w12, w12, #1                            //CTR block 4k+8

	aese	v0.16b, v31.16b                                     //AES block 4k+4 - round 13
	fmov	v4.d[1], x7                           //AES block 4k+4 - mov high
	eor	v10.16b, v10.16b, v7.16b                      //MODULO - fold into mid

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 12
	fmov	d7, x23                               //AES block 4k+7 - mov low

	aese	v1.16b, v31.16b                                     //AES block 4k+5 - round 13
	fmov	v5.d[1], x20                           //AES block 4k+5 - mov high

	fmov	d6, x21                               //AES block 4k+6 - mov low
	cmp	x0, x5                   //LOOP CONTROL

	fmov	v6.d[1], x22                           //AES block 4k+6 - mov high

	pmull	v9.1q, v10.1d, v8.1d            //MODULO - mid 64b align with low
	eor	v4.16b, v4.16b, v0.16b                          //AES block 4k+4 - result
	fmov	d0, x10                               //CTR block 4k+8

	fmov	v0.d[1], x9                               //CTR block 4k+8
	rev	w9, w12                                 //CTR block 4k+9
	add	w12, w12, #1                            //CTR block 4k+9

	eor	v5.16b, v5.16b, v1.16b                          //AES block 4k+5 - result
	fmov	d1, x10                               //CTR block 4k+9
	orr	x9, x11, x9, lsl #32            //CTR block 4k+9

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 12
	fmov	v1.d[1], x9                               //CTR block 4k+9

	aese	v2.16b, v31.16b                                     //AES block 4k+6 - round 13
	rev	w9, w12                                 //CTR block 4k+10
	st1	{ v4.16b}, [x2], #16                     //AES block 4k+4 - store result

	orr	x9, x11, x9, lsl #32            //CTR block 4k+10
	eor	v11.16b, v11.16b, v9.16b                         //MODULO - fold into low
	fmov	v7.d[1], x24                           //AES block 4k+7 - mov high

	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment
	st1	{ v5.16b}, [x2], #16                     //AES block 4k+5 - store result
	add	w12, w12, #1                            //CTR block 4k+10

	aese	v3.16b, v31.16b                                     //AES block 4k+7 - round 13
	eor	v6.16b, v6.16b, v2.16b                          //AES block 4k+6 - result
	fmov	d2, x10                               //CTR block 4k+10

	st1	{ v6.16b}, [x2], #16                     //AES block 4k+6 - store result
	fmov	v2.d[1], x9                               //CTR block 4k+10
	rev	w9, w12                                 //CTR block 4k+11

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	orr	x9, x11, x9, lsl #32            //CTR block 4k+11

	eor	v7.16b, v7.16b, v3.16b                          //AES block 4k+7 - result
	st1	{ v7.16b}, [x2], #16                     //AES block 4k+7 - store result
	b.lt	L256_enc_main_loop

L256_enc_prepretail:	//PREPRETAIL
	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2 (t0, t1, and t2 free)

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	fmov	d3, x10                               //CTR block 4k+3

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	rev64	v4.16b, v4.16b                                    //GHASH block 4k (only t0 is free)

	fmov	v3.d[1], x9                               //CTR block 4k+3
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1

	eor	v4.16b, v4.16b, v11.16b                           //PRE 1
	rev64	v5.16b, v5.16b                                    //GHASH block 4k+1 (t0 and t1 free)

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid

	pmull2	v4.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	pmull	v8.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2

	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+1 - high
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+1 - low

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3

	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid
	mov	d8, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3 (t0, t1, t2 and t3 free)

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid
	eor	v8.8b, v8.8b, v6.8b                          //GHASH block 4k+2 - mid
	add	w12, w12, #1                            //CTR block 4k+3

	pmull	v5.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid

	pmull2	v4.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high

	eor	v11.16b, v11.16b, v5.16b                         //GHASH block 4k+2 - low
	ins	v8.d[1], v8.d[0]                                //GHASH block 4k+2 - mid

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5

	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+2 - high
	mov	d4, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	pmull2	v8.1q, v8.2d, v16.2d                          //GHASH block 4k+2 - mid

	eor	v4.8b, v4.8b, v7.8b                          //GHASH block 4k+3 - mid

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	pmull	v4.1q, v4.1d, v16.1d                          //GHASH block 4k+3 - mid
	eor	v10.16b, v10.16b, v8.16b                         //GHASH block 4k+2 - mid

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	movi	v8.8b, #0xc2

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	shl	d8, d8, #56               //mod_constant

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+3 - mid

	pmull	v6.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	eor	v11.16b, v11.16b, v6.16b                         //GHASH block 4k+3 - low

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9

	eor	v10.16b, v10.16b, v9.16b                         //karatsuba tidy up

	pmull	v4.1q, v9.1d, v8.1d
	ext	v9.16b, v9.16b, v9.16b, #8

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	eor	v10.16b, v10.16b, v11.16b

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 11
	eor	v10.16b, v10.16b, v4.16b

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 12

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 11
	eor	v10.16b, v10.16b, v9.16b

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 11

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 12

	pmull	v4.1q, v10.1d, v8.1d

	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 11
	ext	v10.16b, v10.16b, v10.16b, #8

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 12

	aese	v1.16b, v31.16b                                     //AES block 4k+5 - round 13
	eor	v11.16b, v11.16b, v4.16b

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 12

	aese	v3.16b, v31.16b                                     //AES block 4k+7 - round 13

	aese	v0.16b, v31.16b                                     //AES block 4k+4 - round 13

	aese	v2.16b, v31.16b                                     //AES block 4k+6 - round 13
	eor	v11.16b, v11.16b, v10.16b
L256_enc_tail:	//TAIL

	ext	v8.16b, v11.16b, v11.16b, #8                     //prepare final partial tag
	sub	x5, x4, x0   //main_end_input_ptr is number of bytes left to process
	ldp	x6, x7, [x0], #16           //AES block 4k+4 - load plaintext
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	x6, x6, x13                     //AES block 4k+4 - round 14 low
	eor	x7, x7, x14                     //AES block 4k+4 - round 14 high

	cmp	x5, #48
	fmov	d4, x6                               //AES block 4k+4 - mov low

	fmov	v4.d[1], x7                           //AES block 4k+4 - mov high

	eor	v5.16b, v4.16b, v0.16b                          //AES block 4k+4 - result
	b.gt	L256_enc_blocks_more_than_3

	cmp	x5, #32
	mov	v3.16b, v2.16b
	movi	v11.8b, #0

	movi	v9.8b, #0
	sub	w12, w12, #1

	mov	v2.16b, v1.16b
	movi	v10.8b, #0
	b.gt	L256_enc_blocks_more_than_2

	mov	v3.16b, v1.16b
	sub	w12, w12, #1
	cmp	x5, #16

	b.gt	L256_enc_blocks_more_than_1

	sub	w12, w12, #1
	b	L256_enc_blocks_less_than_1
L256_enc_blocks_more_than_3:	//blocks	left >  3
	st1	{ v5.16b}, [x2], #16                    //AES final-3 block  - store result

	ldp	x6, x7, [x0], #16          //AES final-2 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	rev64	v4.16b, v5.16b                                   //GHASH final-3 block

	eor	x6, x6, x13                    //AES final-2 block - round 14 low
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	eor	x7, x7, x14                    //AES final-2 block - round 14 high

	mov	d22, v4.d[1]                                //GHASH final-3 block - mid
	fmov	d5, x6                                //AES final-2 block - mov low

	fmov	v5.d[1], x7                            //AES final-2 block - mov high

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-3 block - mid
	movi	v8.8b, #0                                       //suppress further partial tag feed in

	mov	d10, v17.d[1]                              //GHASH final-3 block - mid

	pmull	v11.1q, v4.1d, v15.1d                      //GHASH final-3 block - low

	pmull2	v9.1q, v4.2d, v15.2d                      //GHASH final-3 block - high

	pmull	v10.1q, v22.1d, v10.1d                   //GHASH final-3 block - mid
	eor	v5.16b, v5.16b, v1.16b                           //AES final-2 block - result
L256_enc_blocks_more_than_2:	//blocks	left >  2

	st1	{ v5.16b}, [x2], #16                    //AES final-2 block - store result

	ldp	x6, x7, [x0], #16          //AES final-1 block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	rev64	v4.16b, v5.16b                                   //GHASH final-2 block

	eor	x6, x6, x13                    //AES final-1 block - round 14 low
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	fmov	d5, x6                                //AES final-1 block - mov low
	eor	x7, x7, x14                    //AES final-1 block - round 14 high

	fmov	v5.d[1], x7                            //AES final-1 block - mov high

	movi	v8.8b, #0                                       //suppress further partial tag feed in

	pmull2	v20.1q, v4.2d, v14.2d                         //GHASH final-2 block - high
	mov	d22, v4.d[1]                                //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                         //GHASH final-2 block - low

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-2 block - mid

	eor	v5.16b, v5.16b, v2.16b                           //AES final-1 block - result

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-2 block - high

	pmull	v22.1q, v22.1d, v17.1d                     //GHASH final-2 block - mid

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-2 block - low

	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-2 block - mid
L256_enc_blocks_more_than_1:	//blocks	left >  1

	st1	{ v5.16b}, [x2], #16                    //AES final-1 block - store result

	rev64	v4.16b, v5.16b                                   //GHASH final-1 block

	ldp	x6, x7, [x0], #16          //AES final block - load input low & high
#ifdef __AARCH64EB__
	rev	x6, x6
	rev	x7, x7
#endif
	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	movi	v8.8b, #0                                       //suppress further partial tag feed in

	eor	x6, x6, x13                    //AES final block - round 14 low
	mov	d22, v4.d[1]                                //GHASH final-1 block - mid

	pmull2	v20.1q, v4.2d, v13.2d                         //GHASH final-1 block - high
	eor	x7, x7, x14                    //AES final block - round 14 high

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-1 block - mid

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-1 block - high

	ins	v22.d[1], v22.d[0]                           //GHASH final-1 block - mid
	fmov	d5, x6                                //AES final block - mov low

	fmov	v5.d[1], x7                            //AES final block - mov high

	pmull2	v22.1q, v22.2d, v16.2d                     //GHASH final-1 block - mid

	pmull	v21.1q, v4.1d, v13.1d                         //GHASH final-1 block - low

	eor	v5.16b, v5.16b, v3.16b                           //AES final block - result
	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-1 block - mid

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-1 block - low
L256_enc_blocks_less_than_1:	//blocks	left <= 1

	and	x1, x1, #127                   //bit_length %= 128

	mvn	x13, xzr                                     //rk14_l = 0xffffffffffffffff
	sub	x1, x1, #128                   //bit_length -= 128

	neg	x1, x1                         //bit_length = 128 - #bits in input (in range [1,128])
	ld1	{ v18.16b}, [x2]                           //load existing bytes where the possibly partial last block is to be stored

	mvn	x14, xzr                                     //rk14_h = 0xffffffffffffffff
	and	x1, x1, #127                   //bit_length %= 128

	lsr	x14, x14, x1                    //rk14_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x6, x13, x14, lt
	csel	x7, x14, xzr, lt

	fmov	d0, x6                                //ctr0b is mask for last block

	fmov	v0.d[1], x7

	and	v5.16b, v5.16b, v0.16b                           //possibly partial last block has zeroes in highest bits

	rev64	v4.16b, v5.16b                                   //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	bif	v5.16b, v18.16b, v0.16b                             //insert existing bytes in top end of result before storing

	pmull2	v20.1q, v4.2d, v12.2d                         //GHASH final block - high
	mov	d8, v4.d[1]                                 //GHASH final block - mid
#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif

	pmull	v21.1q, v4.1d, v12.1d                         //GHASH final block - low

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final block - high
	eor	v8.8b, v8.8b, v4.8b                         //GHASH final block - mid

	pmull	v8.1q, v8.1d, v16.1d                         //GHASH final block - mid

	eor	v11.16b, v11.16b, v21.16b                           //GHASH final block - low

	eor	v10.16b, v10.16b, v8.16b                        //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v4.16b, v11.16b, v9.16b                        //MODULO - karatsuba tidy up

	shl	d8, d8, #56              //mod_constant

	eor	v10.16b, v10.16b, v4.16b                        //MODULO - karatsuba tidy up

	pmull	v7.1q, v9.1d, v8.1d           //MODULO - top 64b align with mid

	ext	v9.16b, v9.16b, v9.16b, #8                    //MODULO - other top alignment

	eor	v10.16b, v10.16b, v7.16b                     //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                        //MODULO - fold into mid

	pmull	v9.1q, v10.1d, v8.1d           //MODULO - mid 64b align with low

	ext	v10.16b, v10.16b, v10.16b, #8                    //MODULO - other mid alignment

	str	w9, [x16, #12]                         //store the updated counter

	st1	{ v5.16b}, [x2]                         //store all 16B
	eor	v11.16b, v11.16b, v9.16b                        //MODULO - fold into low

	eor	v11.16b, v11.16b, v10.16b                        //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]

	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L256_enc_ret:
	mov	w0, #0x0
	ret

.globl	_aes_gcm_dec_256_kernel

.align	4
_aes_gcm_dec_256_kernel:
	AARCH64_VALID_CALL_TARGET
	cbz	x1, L256_dec_ret
	stp	x19, x20, [sp, #-112]!
	mov	x16, x4
	mov	x8, x5
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]

	lsr	x5, x1, #3              //byte_len
	mov	x15, x5
	ldp	x10, x11, [x16]              //ctr96_b64, ctr96_t32
#ifdef __AARCH64EB__
	rev	x10, x10
	rev	x11, x11
#endif
	ldp	x13, x14, [x8, #224]                     //load rk14
#ifdef __AARCH64EB__
	ror	x14, x14, #32
	ror	x13, x13, #32
#endif
	ld1	{v18.4s}, [x8], #16                               //load rk0
	sub	x5, x5, #1      //byte_len - 1

	ld1	{v19.4s}, [x8], #16                               //load rk1
	and	x5, x5, #0xffffffffffffffc0 //number of bytes to be processed in main loop (at least 1 byte must be handled by tail)

	add	x4, x0, x1, lsr #3   //end_input_ptr
	ld1	{v20.4s}, [x8], #16                               //load rk2

	lsr	x12, x11, #32
	ld1	{v21.4s}, [x8], #16                               //load rk3
	orr	w11, w11, w11

	ld1	{v22.4s}, [x8], #16                               //load rk4
	add	x5, x5, x0
	rev	w12, w12                                //rev_ctr32

	add	w12, w12, #1                            //increment rev_ctr32
	fmov	d3, x10                               //CTR block 3

	rev	w9, w12                                 //CTR block 1
	add	w12, w12, #1                            //CTR block 1
	fmov	d1, x10                               //CTR block 1

	orr	x9, x11, x9, lsl #32            //CTR block 1
	ld1	{ v0.16b}, [x16]                             //special case vector load initial counter so we can start first AES block as quickly as possible

	fmov	v1.d[1], x9                               //CTR block 1
	rev	w9, w12                                 //CTR block 2
	add	w12, w12, #1                            //CTR block 2

	fmov	d2, x10                               //CTR block 2
	orr	x9, x11, x9, lsl #32            //CTR block 2

	fmov	v2.d[1], x9                               //CTR block 2
	rev	w9, w12                                 //CTR block 3

	orr	x9, x11, x9, lsl #32            //CTR block 3
	ld1	{v23.4s}, [x8], #16                               //load rk5

	fmov	v3.d[1], x9                               //CTR block 3
	add	w12, w12, #1                            //CTR block 3

	ld1	{v24.4s}, [x8], #16                               //load rk6

	ld1	{v25.4s}, [x8], #16                               //load rk7

	ld1	{v26.4s}, [x8], #16                               //load rk8

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 0
	ldr	q14, [x3, #80]                         //load h3l | h3h
#ifndef __AARCH64EB__
	ext	v14.16b, v14.16b, v14.16b, #8
#endif

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 0
	ldr	q15, [x3, #112]                        //load h4l | h4h
#ifndef __AARCH64EB__
	ext	v15.16b, v15.16b, v15.16b, #8
#endif

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 0
	ldr	q13, [x3, #64]                         //load h2l | h2h
#ifndef __AARCH64EB__
	ext	v13.16b, v13.16b, v13.16b, #8
#endif

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 0
	ld1	{v27.4s}, [x8], #16                                 //load rk9

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 1

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 1
	ld1	{ v11.16b}, [x3]
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 1
	ld1	{v28.4s}, [x8], #16                              //load rk10

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 1
	ld1	{v29.4s}, [x8], #16                              //load rk11

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 2
	ldr	q12, [x3, #32]                         //load h1l | h1h
#ifndef __AARCH64EB__
	ext	v12.16b, v12.16b, v12.16b, #8
#endif
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 2
	ld1	{v30.4s}, [x8], #16                              //load rk12

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 2

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 3

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 2

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 3

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 4
	cmp	x0, x5                   //check if we have <= 4 blocks

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 3

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 3

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 4

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 4

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 4

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 5

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 5

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 5

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 5

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 6

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 6

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 6

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 6

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 7

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 7

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 7

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 8

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 7

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 8

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 8

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 9

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 8
	ld1	{v31.4s}, [x8], #16                             //load rk13

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 9

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 10

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 9

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 10

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 9

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 10

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 11

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 10

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 11

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 11

	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 11

	trn1	v9.2d, v14.2d,    v15.2d                      //h4h | h3h

	trn2	v17.2d,  v14.2d,    v15.2d                      //h4l | h3l

	trn1	v8.2d,    v12.2d,    v13.2d                      //h2h | h1h
	trn2	v16.2d,  v12.2d,    v13.2d                      //h2l | h1l

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 1 - round 12

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 0 - round 12

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 2 - round 12

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 3 - round 12
	eor	v17.16b, v17.16b, v9.16b                  //h4k | h3k

	aese	v1.16b, v31.16b                                     //AES block 1 - round 13

	aese	v2.16b, v31.16b                                     //AES block 2 - round 13
	eor	v16.16b, v16.16b, v8.16b                     //h2k | h1k

	aese	v3.16b, v31.16b                                     //AES block 3 - round 13

	aese	v0.16b, v31.16b                                     //AES block 0 - round 13
	b.ge	L256_dec_tail                                    //handle tail

	ld1	{v4.16b, v5.16b}, [x0], #32               //AES block 0,1 - load ciphertext

	rev	w9, w12                                 //CTR block 4

	eor	v0.16b, v4.16b, v0.16b                            //AES block 0 - result

	eor	v1.16b, v5.16b, v1.16b                            //AES block 1 - result
	rev64	v5.16b, v5.16b                                    //GHASH block 1
	ld1	{v6.16b}, [x0], #16                       //AES block 2 - load ciphertext

	mov	x7, v0.d[1]                            //AES block 0 - mov high

	mov	x6, v0.d[0]                            //AES block 0 - mov low
	rev64	v4.16b, v4.16b                                    //GHASH block 0
	add	w12, w12, #1                            //CTR block 4

	fmov	d0, x10                               //CTR block 4
	orr	x9, x11, x9, lsl #32            //CTR block 4

	fmov	v0.d[1], x9                               //CTR block 4
	rev	w9, w12                                 //CTR block 5
	add	w12, w12, #1                            //CTR block 5

	mov	x19, v1.d[0]                            //AES block 1 - mov low

	orr	x9, x11, x9, lsl #32            //CTR block 5
	mov	x20, v1.d[1]                            //AES block 1 - mov high
	eor	x7, x7, x14                   //AES block 0 - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	eor	x6, x6, x13                   //AES block 0 - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	stp	x6, x7, [x2], #16        //AES block 0 - store result
	fmov	d1, x10                               //CTR block 5

	ld1	{v7.16b}, [x0], #16                       //AES block 3 - load ciphertext

	fmov	v1.d[1], x9                               //CTR block 5
	rev	w9, w12                                 //CTR block 6
	add	w12, w12, #1                            //CTR block 6

	eor	x19, x19, x13                   //AES block 1 - round 14 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	orr	x9, x11, x9, lsl #32            //CTR block 6

	eor	x20, x20, x14                   //AES block 1 - round 14 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	stp	x19, x20, [x2], #16        //AES block 1 - store result

	eor	v2.16b, v6.16b, v2.16b                            //AES block 2 - result
	cmp	x0, x5                   //check if we have <= 8 blocks
	b.ge	L256_dec_prepretail                              //do prepretail

L256_dec_main_loop:	//main	loop start
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	fmov	d2, x10                               //CTR block 4k+6

	fmov	v2.d[1], x9                               //CTR block 4k+6
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1
	rev	w9, w12                                 //CTR block 4k+7

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid
	fmov	d3, x10                               //CTR block 4k+7

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2
	orr	x9, x11, x9, lsl #32            //CTR block 4k+7

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	fmov	v3.d[1], x9                               //CTR block 4k+7

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	eor	x22, x22, x14                   //AES block 4k+2 - round 14 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0
	eor	x21, x21, x13                   //AES block 4k+2 - round 14 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif
	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low

	pmull2	v4.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	eor	x23, x23, x13                   //AES block 4k+3 - round 14 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	pmull	v8.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low
	eor	x24, x24, x14                   //AES block 4k+3 - round 14 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+1 - high

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+1 - low

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5
	add	w12, w12, #1                            //CTR block 4k+7

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	mov	d8, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4
	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid

	pmull	v5.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v8.8b, v8.8b, v6.8b                          //GHASH block 4k+2 - mid

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	eor	v11.16b, v11.16b, v5.16b                         //GHASH block 4k+2 - low

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid
	rev	w9, w12                                 //CTR block 4k+8

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6
	ins	v8.d[1], v8.d[0]                                //GHASH block 4k+2 - mid

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	add	w12, w12, #1                            //CTR block 4k+8

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7

	pmull2	v4.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high
	mov	d6, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5

	pmull2	v8.1q, v8.2d, v16.2d                          //GHASH block 4k+2 - mid

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+2 - high

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6

	pmull	v4.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low
	orr	x9, x11, x9, lsl #32            //CTR block 4k+8
	eor	v10.16b, v10.16b, v8.16b                         //GHASH block 4k+2 - mid

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9
	eor	v6.8b, v6.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10

	pmull	v6.1q, v6.1d, v16.1d                          //GHASH block 4k+3 - mid
	movi	v8.8b, #0xc2

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	eor	v11.16b, v11.16b, v4.16b                         //GHASH block 4k+3 - low

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 11

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	shl	d8, d8, #56               //mod_constant

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8
	eor	v10.16b, v10.16b, v6.16b                         //GHASH block 4k+3 - mid

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 12

	pmull	v7.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid
	eor	v6.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9
	ld1	{v4.16b}, [x0], #16                       //AES block 4k+4 - load ciphertext

	aese	v0.16b, v31.16b                                     //AES block 4k+4 - round 13
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10
	eor	v10.16b, v10.16b, v6.16b                         //MODULO - karatsuba tidy up

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9
	ld1	{v5.16b}, [x0], #16                       //AES block 4k+5 - load ciphertext

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8
	eor	v0.16b, v4.16b, v0.16b                            //AES block 4k+4 - result

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 11
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10
	eor	v10.16b, v10.16b, v7.16b                      //MODULO - fold into mid

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9
	ld1	{v6.16b}, [x0], #16                       //AES block 4k+6 - load ciphertext

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 12
	ld1	{v7.16b}, [x0], #16                       //AES block 4k+7 - load ciphertext

	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 11
	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	aese	v1.16b, v31.16b                                     //AES block 4k+5 - round 13
	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 12
	fmov	d0, x10                               //CTR block 4k+8

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 11
	fmov	v0.d[1], x9                               //CTR block 4k+8

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	eor	v1.16b, v5.16b, v1.16b                            //AES block 4k+5 - result
	rev	w9, w12                                 //CTR block 4k+9

	aese	v2.16b, v31.16b                                     //AES block 4k+6 - round 13
	orr	x9, x11, x9, lsl #32            //CTR block 4k+9
	cmp	x0, x5                   //LOOP CONTROL

	add	w12, w12, #1                            //CTR block 4k+9

	eor	x6, x6, x13                   //AES block 4k+4 - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	x7, x7, x14                   //AES block 4k+4 - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	mov	x20, v1.d[1]                            //AES block 4k+5 - mov high
	eor	v2.16b, v6.16b, v2.16b                            //AES block 4k+6 - result
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 12
	mov	x19, v1.d[0]                            //AES block 4k+5 - mov low

	fmov	d1, x10                               //CTR block 4k+9
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	fmov	v1.d[1], x9                               //CTR block 4k+9
	rev	w9, w12                                 //CTR block 4k+10
	add	w12, w12, #1                            //CTR block 4k+10

	aese	v3.16b, v31.16b                                     //AES block 4k+7 - round 13
	orr	x9, x11, x9, lsl #32            //CTR block 4k+10

	rev64	v5.16b, v5.16b                                    //GHASH block 4k+5
	eor	x20, x20, x14                   //AES block 4k+5 - round 14 high
#ifdef __AARCH64EB__
	rev	x20, x20
#endif
	stp	x6, x7, [x2], #16        //AES block 4k+4 - store result

	eor	x19, x19, x13                   //AES block 4k+5 - round 14 low
#ifdef __AARCH64EB__
	rev	x19, x19
#endif
	stp	x19, x20, [x2], #16        //AES block 4k+5 - store result

	rev64	v4.16b, v4.16b                                    //GHASH block 4k+4
	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	b.lt	L256_dec_main_loop


L256_dec_prepretail:	//PREPRETAIL
	ext	v11.16b, v11.16b, v11.16b, #8                     //PRE 0
	mov	x21, v2.d[0]                            //AES block 4k+2 - mov low
	eor	v3.16b, v7.16b, v3.16b                            //AES block 4k+3 - result

	aese	v0.16b, v18.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 0
	mov	x22, v2.d[1]                            //AES block 4k+2 - mov high

	aese	v1.16b, v18.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 0
	fmov	d2, x10                               //CTR block 4k+6

	fmov	v2.d[1], x9                               //CTR block 4k+6
	rev	w9, w12                                 //CTR block 4k+7
	eor	v4.16b, v4.16b, v11.16b                           //PRE 1

	rev64	v6.16b, v6.16b                                    //GHASH block 4k+2
	orr	x9, x11, x9, lsl #32            //CTR block 4k+7
	mov	x23, v3.d[0]                            //AES block 4k+3 - mov low

	aese	v1.16b, v19.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 1
	mov	x24, v3.d[1]                            //AES block 4k+3 - mov high

	pmull	v11.1q, v4.1d, v15.1d                       //GHASH block 4k - low
	mov	d8, v4.d[1]                                  //GHASH block 4k - mid
	fmov	d3, x10                               //CTR block 4k+7

	pmull2	v9.1q, v4.2d, v15.2d                       //GHASH block 4k - high
	fmov	v3.d[1], x9                               //CTR block 4k+7

	aese	v2.16b, v18.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 0
	mov	d10, v17.d[1]                               //GHASH block 4k - mid

	aese	v0.16b, v19.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 1
	eor	v8.8b, v8.8b, v4.8b                          //GHASH block 4k - mid

	pmull2	v4.1q, v5.2d, v14.2d                          //GHASH block 4k+1 - high

	aese	v2.16b, v19.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 1
	rev64	v7.16b, v7.16b                                    //GHASH block 4k+3

	aese	v3.16b, v18.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 0

	pmull	v10.1q, v8.1d, v10.1d                      //GHASH block 4k - mid
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+1 - high

	pmull	v8.1q, v5.1d, v14.1d                          //GHASH block 4k+1 - low

	aese	v3.16b, v19.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 1
	mov	d4, v5.d[1]                                  //GHASH block 4k+1 - mid

	aese	v0.16b, v20.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 2

	aese	v1.16b, v20.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 2
	eor	v11.16b, v11.16b, v8.16b                         //GHASH block 4k+1 - low

	aese	v2.16b, v20.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 2

	aese	v0.16b, v21.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 3
	mov	d8, v6.d[1]                                  //GHASH block 4k+2 - mid

	aese	v3.16b, v20.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 2
	eor	v4.8b, v4.8b, v5.8b                          //GHASH block 4k+1 - mid

	pmull	v5.1q, v6.1d, v13.1d                          //GHASH block 4k+2 - low

	aese	v0.16b, v22.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 4

	aese	v3.16b, v21.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 3
	eor	v8.8b, v8.8b, v6.8b                          //GHASH block 4k+2 - mid

	pmull	v4.1q, v4.1d, v17.1d                          //GHASH block 4k+1 - mid

	aese	v0.16b, v23.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 5
	eor	v11.16b, v11.16b, v5.16b                         //GHASH block 4k+2 - low

	aese	v3.16b, v22.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 4

	pmull2	v5.1q, v7.2d, v12.2d                          //GHASH block 4k+3 - high
	eor	v10.16b, v10.16b, v4.16b                         //GHASH block 4k+1 - mid

	pmull2	v4.1q, v6.2d, v13.2d                          //GHASH block 4k+2 - high

	aese	v3.16b, v23.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 5
	ins	v8.d[1], v8.d[0]                                //GHASH block 4k+2 - mid

	aese	v2.16b, v21.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 3

	aese	v1.16b, v21.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 3
	eor	v9.16b, v9.16b, v4.16b                         //GHASH block 4k+2 - high

	pmull	v4.1q, v7.1d, v12.1d                          //GHASH block 4k+3 - low

	aese	v2.16b, v22.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 4
	mov	d6, v7.d[1]                                  //GHASH block 4k+3 - mid

	aese	v1.16b, v22.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 4

	pmull2	v8.1q, v8.2d, v16.2d                          //GHASH block 4k+2 - mid

	aese	v2.16b, v23.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 5
	eor	v6.8b, v6.8b, v7.8b                          //GHASH block 4k+3 - mid

	aese	v1.16b, v23.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 5

	aese	v3.16b, v24.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 6
	eor	v10.16b, v10.16b, v8.16b                         //GHASH block 4k+2 - mid

	aese	v2.16b, v24.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 6

	aese	v0.16b, v24.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 6
	movi	v8.8b, #0xc2

	aese	v1.16b, v24.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 6
	eor	v11.16b, v11.16b, v4.16b                         //GHASH block 4k+3 - low

	pmull	v6.1q, v6.1d, v16.1d                          //GHASH block 4k+3 - mid

	aese	v3.16b, v25.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 7
	eor	v9.16b, v9.16b, v5.16b                         //GHASH block 4k+3 - high

	aese	v1.16b, v25.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 7

	aese	v0.16b, v25.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 7
	eor	v10.16b, v10.16b, v6.16b                         //GHASH block 4k+3 - mid

	aese	v3.16b, v26.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 8

	aese	v2.16b, v25.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 7
	eor	v6.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	aese	v1.16b, v26.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 8

	aese	v0.16b, v26.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 8
	shl	d8, d8, #56               //mod_constant

	aese	v2.16b, v26.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 8

	aese	v1.16b, v27.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 9
	eor	v10.16b, v10.16b, v6.16b                         //MODULO - karatsuba tidy up

	pmull	v7.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid

	aese	v2.16b, v27.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 9
	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	aese	v3.16b, v27.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 9

	aese	v0.16b, v27.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 9
	eor	v10.16b, v10.16b, v7.16b                      //MODULO - fold into mid

	aese	v2.16b, v28.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 10

	aese	v3.16b, v28.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 10

	aese	v0.16b, v28.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 10
	eor	x22, x22, x14                   //AES block 4k+2 - round 14 high
#ifdef __AARCH64EB__
	rev	x22, x22
#endif
	aese	v1.16b, v28.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 10
	eor	x23, x23, x13                   //AES block 4k+3 - round 14 low
#ifdef __AARCH64EB__
	rev	x23, x23
#endif
	aese	v2.16b, v29.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 11
	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	aese	v0.16b, v29.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 11
	add	w12, w12, #1                            //CTR block 4k+7

	aese	v1.16b, v29.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 11
	eor	x21, x21, x13                   //AES block 4k+2 - round 14 low
#ifdef __AARCH64EB__
	rev	x21, x21
#endif

	aese	v2.16b, v30.16b
	aesmc	v2.16b, v2.16b          //AES block 4k+6 - round 12

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low
	eor	x24, x24, x14                   //AES block 4k+3 - round 14 high
#ifdef __AARCH64EB__
	rev	x24, x24
#endif

	aese	v3.16b, v29.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 11
	stp	x21, x22, [x2], #16        //AES block 4k+2 - store result

	aese	v1.16b, v30.16b
	aesmc	v1.16b, v1.16b          //AES block 4k+5 - round 12
	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	aese	v0.16b, v30.16b
	aesmc	v0.16b, v0.16b          //AES block 4k+4 - round 12
	stp	x23, x24, [x2], #16        //AES block 4k+3 - store result

	aese	v3.16b, v30.16b
	aesmc	v3.16b, v3.16b          //AES block 4k+7 - round 12
	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	aese	v1.16b, v31.16b                                     //AES block 4k+5 - round 13

	aese	v0.16b, v31.16b                                     //AES block 4k+4 - round 13

	aese	v3.16b, v31.16b                                     //AES block 4k+7 - round 13

	aese	v2.16b, v31.16b                                     //AES block 4k+6 - round 13
	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
L256_dec_tail:	//TAIL

	sub	x5, x4, x0   //main_end_input_ptr is number of bytes left to process
	ld1	{ v5.16b}, [x0], #16                      //AES block 4k+4 - load ciphertext

	eor	v0.16b, v5.16b, v0.16b                            //AES block 4k+4 - result

	mov	x6, v0.d[0]                            //AES block 4k+4 - mov low

	mov	x7, v0.d[1]                            //AES block 4k+4 - mov high
	ext	v8.16b, v11.16b, v11.16b, #8                     //prepare final partial tag

	cmp	x5, #48

	eor	x6, x6, x13                   //AES block 4k+4 - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif

	eor	x7, x7, x14                   //AES block 4k+4 - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
	b.gt	L256_dec_blocks_more_than_3

	sub	w12, w12, #1
	mov	v3.16b, v2.16b
	movi	v10.8b, #0

	movi	v11.8b, #0
	cmp	x5, #32

	movi	v9.8b, #0
	mov	v2.16b, v1.16b
	b.gt	L256_dec_blocks_more_than_2

	sub	w12, w12, #1

	mov	v3.16b, v1.16b
	cmp	x5, #16
	b.gt	L256_dec_blocks_more_than_1

	sub	w12, w12, #1
	b	L256_dec_blocks_less_than_1
L256_dec_blocks_more_than_3:	//blocks	left >  3
	rev64	v4.16b, v5.16b                                   //GHASH final-3 block
	ld1	{ v5.16b}, [x0], #16                     //AES final-2 block - load ciphertext

	stp	x6, x7, [x2], #16       //AES final-3 block  - store result

	mov	d10, v17.d[1]                              //GHASH final-3 block - mid

	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag

	eor	v0.16b, v5.16b, v1.16b                           //AES final-2 block - result

	mov	d22, v4.d[1]                                //GHASH final-3 block - mid

	mov	x6, v0.d[0]                           //AES final-2 block - mov low

	mov	x7, v0.d[1]                           //AES final-2 block - mov high

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-3 block - mid

	movi	v8.8b, #0                                       //suppress further partial tag feed in

	pmull2	v9.1q, v4.2d, v15.2d                      //GHASH final-3 block - high

	pmull	v10.1q, v22.1d, v10.1d                   //GHASH final-3 block - mid
	eor	x6, x6, x13                  //AES final-2 block - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif

	pmull	v11.1q, v4.1d, v15.1d                      //GHASH final-3 block - low
	eor	x7, x7, x14                  //AES final-2 block - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
L256_dec_blocks_more_than_2:	//blocks	left >  2

	rev64	v4.16b, v5.16b                                   //GHASH final-2 block
	ld1	{ v5.16b}, [x0], #16                     //AES final-1 block - load ciphertext

	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag
	stp	x6, x7, [x2], #16       //AES final-2 block  - store result

	eor	v0.16b, v5.16b, v2.16b                           //AES final-1 block - result

	mov	d22, v4.d[1]                                //GHASH final-2 block - mid

	pmull	v21.1q, v4.1d, v14.1d                         //GHASH final-2 block - low

	pmull2	v20.1q, v4.2d, v14.2d                         //GHASH final-2 block - high

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-2 block - mid
	mov	x6, v0.d[0]                           //AES final-1 block - mov low

	mov	x7, v0.d[1]                           //AES final-1 block - mov high
	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-2 block - low
	movi	v8.8b, #0                                       //suppress further partial tag feed in

	pmull	v22.1q, v22.1d, v17.1d                     //GHASH final-2 block - mid

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-2 block - high
	eor	x6, x6, x13                  //AES final-1 block - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif

	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-2 block - mid
	eor	x7, x7, x14                  //AES final-1 block - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
L256_dec_blocks_more_than_1:	//blocks	left >  1

	stp	x6, x7, [x2], #16       //AES final-1 block  - store result
	rev64	v4.16b, v5.16b                                   //GHASH final-1 block

	ld1	{ v5.16b}, [x0], #16                     //AES final block - load ciphertext

	eor	v4.16b, v4.16b, v8.16b                          //feed in partial tag
	movi	v8.8b, #0                                       //suppress further partial tag feed in

	mov	d22, v4.d[1]                                //GHASH final-1 block - mid

	eor	v0.16b, v5.16b, v3.16b                           //AES final block - result

	pmull2	v20.1q, v4.2d, v13.2d                         //GHASH final-1 block - high

	eor	v22.8b, v22.8b, v4.8b                     //GHASH final-1 block - mid

	pmull	v21.1q, v4.1d, v13.1d                         //GHASH final-1 block - low
	mov	x6, v0.d[0]                           //AES final block - mov low

	ins	v22.d[1], v22.d[0]                           //GHASH final-1 block - mid

	mov	x7, v0.d[1]                           //AES final block - mov high

	pmull2	v22.1q, v22.2d, v16.2d                     //GHASH final-1 block - mid
	eor	x6, x6, x13                  //AES final block - round 14 low
#ifdef __AARCH64EB__
	rev	x6, x6
#endif
	eor	v11.16b, v11.16b, v21.16b                           //GHASH final-1 block - low

	eor	v9.16b, v9.16b, v20.16b                           //GHASH final-1 block - high

	eor	v10.16b, v10.16b, v22.16b                      //GHASH final-1 block - mid
	eor	x7, x7, x14                  //AES final block - round 14 high
#ifdef __AARCH64EB__
	rev	x7, x7
#endif
L256_dec_blocks_less_than_1:	//blocks	left <= 1

	and	x1, x1, #127                   //bit_length %= 128
	mvn	x14, xzr                                     //rk14_h = 0xffffffffffffffff

	sub	x1, x1, #128                   //bit_length -= 128
	mvn	x13, xzr                                     //rk14_l = 0xffffffffffffffff

	ldp	x4, x5, [x2] //load existing bytes we need to not overwrite
	neg	x1, x1                         //bit_length = 128 - #bits in input (in range [1,128])

	and	x1, x1, #127                   //bit_length %= 128

	lsr	x14, x14, x1                    //rk14_h is mask for top 64b of last block
	cmp	x1, #64

	csel	x9, x13, x14, lt
	csel	x10, x14, xzr, lt

	fmov	d0, x9                                  //ctr0b is mask for last block
	and	x6, x6, x9

	mov	v0.d[1], x10
	bic	x4, x4, x9          //mask out low existing bytes

#ifndef __AARCH64EB__
	rev	w9, w12
#else
	mov	w9, w12
#endif

	bic	x5, x5, x10      //mask out high existing bytes

	orr	x6, x6, x4

	and	x7, x7, x10

	orr	x7, x7, x5

	and	v5.16b, v5.16b, v0.16b                            //possibly partial last block has zeroes in highest bits

	rev64	v4.16b, v5.16b                                    //GHASH final block

	eor	v4.16b, v4.16b, v8.16b                           //feed in partial tag

	pmull	v21.1q, v4.1d, v12.1d                          //GHASH final block - low

	mov	d8, v4.d[1]                                  //GHASH final block - mid

	eor	v8.8b, v8.8b, v4.8b                          //GHASH final block - mid

	pmull2	v20.1q, v4.2d, v12.2d                          //GHASH final block - high

	pmull	v8.1q, v8.1d, v16.1d                          //GHASH final block - mid

	eor	v9.16b, v9.16b, v20.16b                            //GHASH final block - high

	eor	v11.16b, v11.16b, v21.16b                            //GHASH final block - low

	eor	v10.16b, v10.16b, v8.16b                         //GHASH final block - mid
	movi	v8.8b, #0xc2

	eor	v6.16b, v11.16b, v9.16b                         //MODULO - karatsuba tidy up

	shl	d8, d8, #56               //mod_constant

	eor	v10.16b, v10.16b, v6.16b                         //MODULO - karatsuba tidy up

	pmull	v7.1q, v9.1d, v8.1d            //MODULO - top 64b align with mid

	ext	v9.16b, v9.16b, v9.16b, #8                     //MODULO - other top alignment

	eor	v10.16b, v10.16b, v7.16b                      //MODULO - fold into mid

	eor	v10.16b, v10.16b, v9.16b                         //MODULO - fold into mid

	pmull	v8.1q, v10.1d, v8.1d     //MODULO - mid 64b align with low

	ext	v10.16b, v10.16b, v10.16b, #8                     //MODULO - other mid alignment

	eor	v11.16b, v11.16b, v8.16b               //MODULO - fold into low

	stp	x6, x7, [x2]

	str	w9, [x16, #12]                          //store the updated counter

	eor	v11.16b, v11.16b, v10.16b                         //MODULO - fold into low
	ext	v11.16b, v11.16b, v11.16b, #8
	rev64	v11.16b, v11.16b
	mov	x0, x15
	st1	{ v11.16b }, [x3]

	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	d8, d9, [sp, #48]
	ldp	d10, d11, [sp, #64]
	ldp	d12, d13, [sp, #80]
	ldp	d14, d15, [sp, #96]
	ldp	x19, x20, [sp], #112
	ret

L256_dec_ret:
	mov	w0, #0x0
	ret

.section	__TEXT,__const
.byte	71,72,65,83,72,32,102,111,114,32,65,82,77,118,56,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2
#endif
