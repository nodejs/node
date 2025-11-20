#include "arm_arch.h"

#if __ARM_MAX_ARCH__>=7

.text
.section	__TEXT,__const
.align	5
Lrcon:
.long	0x01,0x01,0x01,0x01
.long	0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d	// rotate-n-splat
.long	0x1b,0x1b,0x1b,0x1b
.previous
.globl	_aes_v8_set_encrypt_key

.align	5
_aes_v8_set_encrypt_key:
Lenc_key:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	mov	x3,#-1
	cmp	x0,#0
	b.eq	Lenc_key_abort
	cmp	x2,#0
	b.eq	Lenc_key_abort
	mov	x3,#-2
	cmp	w1,#128
	b.lt	Lenc_key_abort
	cmp	w1,#256
	b.gt	Lenc_key_abort
	tst	w1,#0x3f
	b.ne	Lenc_key_abort

	adrp	x3,Lrcon@PAGE
	add	x3,x3,Lrcon@PAGEOFF
	cmp	w1,#192

	eor	v0.16b,v0.16b,v0.16b
	ld1	{v3.16b},[x0],#16
	mov	w1,#8		// reuse w1
	ld1	{v1.4s,v2.4s},[x3],#32

	b.lt	Loop128
	b.eq	L192
	b	L256

.align	4
Loop128:
	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b
	b.ne	Loop128

	ld1	{v1.4s},[x3]

	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b

	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	eor	v3.16b,v3.16b,v6.16b
	st1	{v3.4s},[x2]
	add	x2,x2,#0x50

	mov	w12,#10
	b	Ldone

.align	4
L192:
	ld1	{v4.8b},[x0],#8
	movi	v6.16b,#8			// borrow v6.16b
	st1	{v3.4s},[x2],#16
	sub	v2.16b,v2.16b,v6.16b	// adjust the mask

Loop192:
	tbl	v6.16b,{v4.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
#ifdef __AARCH64EB__
	st1	{v4.4s},[x2],#16
	sub	x2,x2,#8
#else
	st1	{v4.8b},[x2],#8
#endif
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b

	dup	v5.4s,v3.s[3]
	eor	v5.16b,v5.16b,v4.16b
	eor	v6.16b,v6.16b,v1.16b
	ext	v4.16b,v0.16b,v4.16b,#12
	shl	v1.16b,v1.16b,#1
	eor	v4.16b,v4.16b,v5.16b
	eor	v3.16b,v3.16b,v6.16b
	eor	v4.16b,v4.16b,v6.16b
	st1	{v3.4s},[x2],#16
	b.ne	Loop192

	mov	w12,#12
	add	x2,x2,#0x20
	b	Ldone

.align	4
L256:
	ld1	{v4.16b},[x0]
	mov	w1,#7
	mov	w12,#14
	st1	{v3.4s},[x2],#16

Loop256:
	tbl	v6.16b,{v4.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v4.4s},[x2],#16
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b
	st1	{v3.4s},[x2],#16
	b.eq	Ldone

	dup	v6.4s,v3.s[3]		// just splat
	ext	v5.16b,v0.16b,v4.16b,#12
	aese	v6.16b,v0.16b

	eor	v4.16b,v4.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v4.16b,v4.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v4.16b,v4.16b,v5.16b

	eor	v4.16b,v4.16b,v6.16b
	b	Loop256

Ldone:
	str	w12,[x2]
	mov	x3,#0

Lenc_key_abort:
	mov	x0,x3			// return value
	ldr	x29,[sp],#16
	ret


.globl	_aes_v8_set_decrypt_key

.align	5
_aes_v8_set_decrypt_key:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	bl	Lenc_key

	cmp	x0,#0
	b.ne	Ldec_key_abort

	sub	x2,x2,#240		// restore original x2
	mov	x4,#-16
	add	x0,x2,x12,lsl#4	// end of key schedule

	ld1	{v0.4s},[x2]
	ld1	{v1.4s},[x0]
	st1	{v0.4s},[x0],x4
	st1	{v1.4s},[x2],#16

Loop_imc:
	ld1	{v0.4s},[x2]
	ld1	{v1.4s},[x0]
	aesimc	v0.16b,v0.16b
	aesimc	v1.16b,v1.16b
	st1	{v0.4s},[x0],x4
	st1	{v1.4s},[x2],#16
	cmp	x0,x2
	b.hi	Loop_imc

	ld1	{v0.4s},[x2]
	aesimc	v0.16b,v0.16b
	st1	{v0.4s},[x0]

	eor	x0,x0,x0		// return value
Ldec_key_abort:
	ldp	x29,x30,[sp],#16
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.globl	_aes_v8_encrypt

.align	5
_aes_v8_encrypt:
	AARCH64_VALID_CALL_TARGET
	ldr	w3,[x2,#240]
	ld1	{v0.4s},[x2],#16
	ld1	{v2.16b},[x0]
	sub	w3,w3,#2
	ld1	{v1.4s},[x2],#16

Loop_enc:
	aese	v2.16b,v0.16b
	aesmc	v2.16b,v2.16b
	ld1	{v0.4s},[x2],#16
	subs	w3,w3,#2
	aese	v2.16b,v1.16b
	aesmc	v2.16b,v2.16b
	ld1	{v1.4s},[x2],#16
	b.gt	Loop_enc

	aese	v2.16b,v0.16b
	aesmc	v2.16b,v2.16b
	ld1	{v0.4s},[x2]
	aese	v2.16b,v1.16b
	eor	v2.16b,v2.16b,v0.16b

	st1	{v2.16b},[x1]
	ret

.globl	_aes_v8_decrypt

.align	5
_aes_v8_decrypt:
	AARCH64_VALID_CALL_TARGET
	ldr	w3,[x2,#240]
	ld1	{v0.4s},[x2],#16
	ld1	{v2.16b},[x0]
	sub	w3,w3,#2
	ld1	{v1.4s},[x2],#16

Loop_dec:
	aesd	v2.16b,v0.16b
	aesimc	v2.16b,v2.16b
	ld1	{v0.4s},[x2],#16
	subs	w3,w3,#2
	aesd	v2.16b,v1.16b
	aesimc	v2.16b,v2.16b
	ld1	{v1.4s},[x2],#16
	b.gt	Loop_dec

	aesd	v2.16b,v0.16b
	aesimc	v2.16b,v2.16b
	ld1	{v0.4s},[x2]
	aesd	v2.16b,v1.16b
	eor	v2.16b,v2.16b,v0.16b

	st1	{v2.16b},[x1]
	ret

.globl	_aes_v8_ecb_encrypt

.align	5
_aes_v8_ecb_encrypt:
	AARCH64_VALID_CALL_TARGET
	subs	x2,x2,#16
	// Original input data size bigger than 16, jump to big size processing.
	b.ne	Lecb_big_size
	ld1	{v0.16b},[x0]
	cmp	w4,#0					// en- or decrypting?
	ldr	w5,[x3,#240]
	ld1	{v5.4s,v6.4s},[x3],#32			// load key schedule...

	b.eq	Lecb_small_dec
	aese	v0.16b,v5.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s,v17.4s},[x3],#32			// load key schedule...
	aese	v0.16b,v6.16b
	aesmc	v0.16b,v0.16b
	subs	w5,w5,#10			// if rounds==10, jump to aes-128-ecb processing
	b.eq	Lecb_128_enc
Lecb_round_loop:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s},[x3],#16				// load key schedule...
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.4s},[x3],#16				// load key schedule...
	subs	w5,w5,#2			// bias
	b.gt	Lecb_round_loop
Lecb_128_enc:
	ld1	{v18.4s,v19.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	ld1	{v20.4s,v21.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ld1	{v22.4s,v23.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	ld1	{v7.4s},[x3]
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v7.16b
	st1	{v0.16b},[x1]
	b	Lecb_Final_abort
Lecb_small_dec:
	aesd	v0.16b,v5.16b
	aesimc	v0.16b,v0.16b
	ld1	{v16.4s,v17.4s},[x3],#32			// load key schedule...
	aesd	v0.16b,v6.16b
	aesimc	v0.16b,v0.16b
	subs	w5,w5,#10			// bias
	b.eq	Lecb_128_dec
Lecb_dec_round_loop:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	ld1	{v16.4s},[x3],#16				// load key schedule...
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	ld1	{v17.4s},[x3],#16				// load key schedule...
	subs	w5,w5,#2			// bias
	b.gt	Lecb_dec_round_loop
Lecb_128_dec:
	ld1	{v18.4s,v19.4s},[x3],#32		// load key schedule...
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	ld1	{v20.4s,v21.4s},[x3],#32		// load key schedule...
	aesd	v0.16b,v18.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v22.4s,v23.4s},[x3],#32		// load key schedule...
	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	ld1	{v7.4s},[x3]
	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v7.16b
	st1	{v0.16b},[x1]
	b	Lecb_Final_abort
Lecb_big_size:
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	mov	x8,#16
	b.lo	Lecb_done
	csel	x8,xzr,x8,eq

	cmp	w4,#0					// en- or decrypting?
	ldr	w5,[x3,#240]
	and	x2,x2,#-16
	ld1	{v0.16b},[x0],x8

	ld1	{v16.4s,v17.4s},[x3]				// load key schedule...
	sub	w5,w5,#6
	add	x7,x3,x5,lsl#4				// pointer to last 7 round keys
	sub	w5,w5,#2
	ld1	{v18.4s,v19.4s},[x7],#32
	ld1	{v20.4s,v21.4s},[x7],#32
	ld1	{v22.4s,v23.4s},[x7],#32
	ld1	{v7.4s},[x7]

	add	x7,x3,#32
	mov	w6,w5
	b.eq	Lecb_dec

	ld1	{v1.16b},[x0],#16
	subs	x2,x2,#32				// bias
	add	w6,w5,#2
	orr	v3.16b,v1.16b,v1.16b
	orr	v24.16b,v1.16b,v1.16b
	orr	v1.16b,v0.16b,v0.16b
	b.lo	Lecb_enc_tail

	orr	v1.16b,v3.16b,v3.16b
	ld1	{v24.16b},[x0],#16
	cmp	x2,#32
	b.lo	Loop3x_ecb_enc

	ld1	{v25.16b},[x0],#16
	ld1	{v26.16b},[x0],#16
	sub	x2,x2,#32				// bias
	mov	w6,w5

Loop5x_ecb_enc:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v16.16b
	aesmc	v26.16b,v26.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v17.16b
	aesmc	v26.16b,v26.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop5x_ecb_enc

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v16.16b
	aesmc	v26.16b,v26.16b
	cmp	x2,#0x40					// because Lecb_enc_tail4x
	sub	x2,x2,#0x50

	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v17.16b
	aesmc	v26.16b,v26.16b
	csel	x6,xzr,x2,gt			// borrow x6, w6, "gt" is not typo
	mov	x7,x3

	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v18.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v18.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v18.16b
	aesmc	v26.16b,v26.16b
	add	x0,x0,x6				// x0 is adjusted in such way that
							// at exit from the loop v1.16b-v26.16b
							// are loaded with last "words"
	add	x6,x2,#0x60		    // because Lecb_enc_tail4x

	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v19.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v19.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v19.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v19.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v20.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v20.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v21.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v21.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v22.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v22.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v23.16b
	ld1	{v2.16b},[x0],#16
	aese	v1.16b,v23.16b
	ld1	{v3.16b},[x0],#16
	aese	v24.16b,v23.16b
	ld1	{v27.16b},[x0],#16
	aese	v25.16b,v23.16b
	ld1	{v28.16b},[x0],#16
	aese	v26.16b,v23.16b
	ld1	{v29.16b},[x0],#16
	cbz	x6,Lecb_enc_tail4x
	ld1	{v16.4s},[x7],#16			// re-pre-load rndkey[0]
	eor	v4.16b,v7.16b,v0.16b
	orr	v0.16b,v2.16b,v2.16b
	eor	v5.16b,v7.16b,v1.16b
	orr	v1.16b,v3.16b,v3.16b
	eor	v17.16b,v7.16b,v24.16b
	orr	v24.16b,v27.16b,v27.16b
	eor	v30.16b,v7.16b,v25.16b
	orr	v25.16b,v28.16b,v28.16b
	eor	v31.16b,v7.16b,v26.16b
	st1	{v4.16b},[x1],#16
	orr	v26.16b,v29.16b,v29.16b
	st1	{v5.16b},[x1],#16
	mov	w6,w5
	st1	{v17.16b},[x1],#16
	ld1	{v17.4s},[x7],#16			// re-pre-load rndkey[1]
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16
	b.hs	Loop5x_ecb_enc

	add	x2,x2,#0x50
	cbz	x2,Lecb_done

	add	w6,w5,#2
	subs	x2,x2,#0x30
	orr	v0.16b,v27.16b,v27.16b
	orr	v1.16b,v28.16b,v28.16b
	orr	v24.16b,v29.16b,v29.16b
	b.lo	Lecb_enc_tail

	b	Loop3x_ecb_enc

.align	4
Lecb_enc_tail4x:
	eor	v5.16b,v7.16b,v1.16b
	eor	v17.16b,v7.16b,v24.16b
	eor	v30.16b,v7.16b,v25.16b
	eor	v31.16b,v7.16b,v26.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16

	b	Lecb_done
.align	4
Loop3x_ecb_enc:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop3x_ecb_enc

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	subs	x2,x2,#0x30
	csel	x6,x2,x6,lo				// x6, w6, is zero at this point
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	add	x0,x0,x6			// x0 is adjusted in such way that
						// at exit from the loop v1.16b-v24.16b
						// are loaded with last "words"
	mov	x7,x3
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	ld1	{v2.16b},[x0],#16
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	ld1	{v3.16b},[x0],#16
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	ld1	{v27.16b},[x0],#16
	aese	v0.16b,v23.16b
	aese	v1.16b,v23.16b
	aese	v24.16b,v23.16b
	ld1	{v16.4s},[x7],#16		// re-pre-load rndkey[0]
	add	w6,w5,#2
	eor	v4.16b,v7.16b,v0.16b
	eor	v5.16b,v7.16b,v1.16b
	eor	v24.16b,v24.16b,v7.16b
	ld1	{v17.4s},[x7],#16		// re-pre-load rndkey[1]
	st1	{v4.16b},[x1],#16
	orr	v0.16b,v2.16b,v2.16b
	st1	{v5.16b},[x1],#16
	orr	v1.16b,v3.16b,v3.16b
	st1	{v24.16b},[x1],#16
	orr	v24.16b,v27.16b,v27.16b
	b.hs	Loop3x_ecb_enc

	cmn	x2,#0x30
	b.eq	Lecb_done
	nop

Lecb_enc_tail:
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lecb_enc_tail

	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	cmn	x2,#0x20
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v23.16b
	aese	v24.16b,v23.16b
	b.eq	Lecb_enc_one
	eor	v5.16b,v7.16b,v1.16b
	eor	v17.16b,v7.16b,v24.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	b	Lecb_done

Lecb_enc_one:
	eor	v5.16b,v7.16b,v24.16b
	st1	{v5.16b},[x1],#16
	b	Lecb_done
.align	5
Lecb_dec:
	ld1	{v1.16b},[x0],#16
	subs	x2,x2,#32			// bias
	add	w6,w5,#2
	orr	v3.16b,v1.16b,v1.16b
	orr	v24.16b,v1.16b,v1.16b
	orr	v1.16b,v0.16b,v0.16b
	b.lo	Lecb_dec_tail

	orr	v1.16b,v3.16b,v3.16b
	ld1	{v24.16b},[x0],#16
	cmp	x2,#32
	b.lo	Loop3x_ecb_dec

	ld1	{v25.16b},[x0],#16
	ld1	{v26.16b},[x0],#16
	sub	x2,x2,#32				// bias
	mov	w6,w5

Loop5x_ecb_dec:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop5x_ecb_dec

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	cmp	x2,#0x40				// because Lecb_tail4x
	sub	x2,x2,#0x50

	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	csel	x6,xzr,x2,gt		// borrow x6, w6, "gt" is not typo
	mov	x7,x3

	aesd	v0.16b,v18.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v18.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v18.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v18.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v18.16b
	aesimc	v26.16b,v26.16b
	add	x0,x0,x6				// x0 is adjusted in such way that
							// at exit from the loop v1.16b-v26.16b
							// are loaded with last "words"
	add	x6,x2,#0x60			// because Lecb_tail4x

	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v19.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v19.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v19.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v20.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v20.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v21.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v21.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v22.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v22.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v23.16b
	ld1	{v2.16b},[x0],#16
	aesd	v1.16b,v23.16b
	ld1	{v3.16b},[x0],#16
	aesd	v24.16b,v23.16b
	ld1	{v27.16b},[x0],#16
	aesd	v25.16b,v23.16b
	ld1	{v28.16b},[x0],#16
	aesd	v26.16b,v23.16b
	ld1	{v29.16b},[x0],#16
	cbz	x6,Lecb_tail4x
	ld1	{v16.4s},[x7],#16			// re-pre-load rndkey[0]
	eor	v4.16b,v7.16b,v0.16b
	orr	v0.16b,v2.16b,v2.16b
	eor	v5.16b,v7.16b,v1.16b
	orr	v1.16b,v3.16b,v3.16b
	eor	v17.16b,v7.16b,v24.16b
	orr	v24.16b,v27.16b,v27.16b
	eor	v30.16b,v7.16b,v25.16b
	orr	v25.16b,v28.16b,v28.16b
	eor	v31.16b,v7.16b,v26.16b
	st1	{v4.16b},[x1],#16
	orr	v26.16b,v29.16b,v29.16b
	st1	{v5.16b},[x1],#16
	mov	w6,w5
	st1	{v17.16b},[x1],#16
	ld1	{v17.4s},[x7],#16			// re-pre-load rndkey[1]
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16
	b.hs	Loop5x_ecb_dec

	add	x2,x2,#0x50
	cbz	x2,Lecb_done

	add	w6,w5,#2
	subs	x2,x2,#0x30
	orr	v0.16b,v27.16b,v27.16b
	orr	v1.16b,v28.16b,v28.16b
	orr	v24.16b,v29.16b,v29.16b
	b.lo	Lecb_dec_tail

	b	Loop3x_ecb_dec

.align	4
Lecb_tail4x:
	eor	v5.16b,v7.16b,v1.16b
	eor	v17.16b,v7.16b,v24.16b
	eor	v30.16b,v7.16b,v25.16b
	eor	v31.16b,v7.16b,v26.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16

	b	Lecb_done
.align	4
Loop3x_ecb_dec:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop3x_ecb_dec

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	subs	x2,x2,#0x30
	csel	x6,x2,x6,lo				// x6, w6, is zero at this point
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	add	x0,x0,x6 			// x0 is adjusted in such way that
						// at exit from the loop v1.16b-v24.16b
						// are loaded with last "words"
	mov	x7,x3
	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	ld1	{v2.16b},[x0],#16
	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	ld1	{v3.16b},[x0],#16
	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	ld1	{v27.16b},[x0],#16
	aesd	v0.16b,v23.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	ld1	{v16.4s},[x7],#16			// re-pre-load rndkey[0]
	add	w6,w5,#2
	eor	v4.16b,v7.16b,v0.16b
	eor	v5.16b,v7.16b,v1.16b
	eor	v24.16b,v24.16b,v7.16b
	ld1	{v17.4s},[x7],#16			// re-pre-load rndkey[1]
	st1	{v4.16b},[x1],#16
	orr	v0.16b,v2.16b,v2.16b
	st1	{v5.16b},[x1],#16
	orr	v1.16b,v3.16b,v3.16b
	st1	{v24.16b},[x1],#16
	orr	v24.16b,v27.16b,v27.16b
	b.hs	Loop3x_ecb_dec

	cmn	x2,#0x30
	b.eq	Lecb_done
	nop

Lecb_dec_tail:
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lecb_dec_tail

	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	cmn	x2,#0x20
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	b.eq	Lecb_dec_one
	eor	v5.16b,v7.16b,v1.16b
	eor	v17.16b,v7.16b,v24.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	b	Lecb_done

Lecb_dec_one:
	eor	v5.16b,v7.16b,v24.16b
	st1	{v5.16b},[x1],#16

Lecb_done:
	ldr	x29,[sp],#16
Lecb_Final_abort:
	ret

.globl	_aes_v8_cbc_encrypt

.align	5
_aes_v8_cbc_encrypt:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	subs	x2,x2,#16
	mov	x8,#16
	b.lo	Lcbc_abort
	csel	x8,xzr,x8,eq

	cmp	w5,#0			// en- or decrypting?
	ldr	w5,[x3,#240]
	and	x2,x2,#-16
	ld1	{v6.16b},[x4]
	ld1	{v0.16b},[x0],x8

	ld1	{v16.4s,v17.4s},[x3]		// load key schedule...
	sub	w5,w5,#6
	add	x7,x3,x5,lsl#4	// pointer to last 7 round keys
	sub	w5,w5,#2
	ld1	{v18.4s,v19.4s},[x7],#32
	ld1	{v20.4s,v21.4s},[x7],#32
	ld1	{v22.4s,v23.4s},[x7],#32
	ld1	{v7.4s},[x7]

	add	x7,x3,#32
	mov	w6,w5
	b.eq	Lcbc_dec

	cmp	w5,#2
	eor	v0.16b,v0.16b,v6.16b
	eor	v5.16b,v16.16b,v7.16b
	b.eq	Lcbc_enc128

	ld1	{v2.4s,v3.4s},[x7]
	add	x7,x3,#16
	add	x6,x3,#16*4
	add	x12,x3,#16*5
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	add	x14,x3,#16*6
	add	x3,x3,#16*7
	b	Lenter_cbc_enc

.align	4
Loop_cbc_enc:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	st1	{v6.16b},[x1],#16
Lenter_cbc_enc:
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v2.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s},[x6]
	cmp	w5,#4
	aese	v0.16b,v3.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.4s},[x12]
	b.eq	Lcbc_enc192

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s},[x14]
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.4s},[x3]
	nop

Lcbc_enc192:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	subs	x2,x2,#16
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	csel	x8,xzr,x8,eq
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.16b},[x0],x8
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	eor	v16.16b,v16.16b,v5.16b
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.4s},[x7]		// re-pre-load rndkey[1]
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v23.16b
	eor	v6.16b,v0.16b,v7.16b
	b.hs	Loop_cbc_enc

	st1	{v6.16b},[x1],#16
	b	Lcbc_done

.align	5
Lcbc_enc128:
	ld1	{v2.4s,v3.4s},[x7]
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	b	Lenter_cbc_enc128
Loop_cbc_enc128:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	st1	{v6.16b},[x1],#16
Lenter_cbc_enc128:
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	subs	x2,x2,#16
	aese	v0.16b,v2.16b
	aesmc	v0.16b,v0.16b
	csel	x8,xzr,x8,eq
	aese	v0.16b,v3.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.16b},[x0],x8
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	eor	v16.16b,v16.16b,v5.16b
	aese	v0.16b,v23.16b
	eor	v6.16b,v0.16b,v7.16b
	b.hs	Loop_cbc_enc128

	st1	{v6.16b},[x1],#16
	b	Lcbc_done
.align	5
Lcbc_dec:
	ld1	{v24.16b},[x0],#16
	subs	x2,x2,#32		// bias
	add	w6,w5,#2
	orr	v3.16b,v0.16b,v0.16b
	orr	v1.16b,v0.16b,v0.16b
	orr	v27.16b,v24.16b,v24.16b
	b.lo	Lcbc_dec_tail

	orr	v1.16b,v24.16b,v24.16b
	ld1	{v24.16b},[x0],#16
	orr	v2.16b,v0.16b,v0.16b
	orr	v3.16b,v1.16b,v1.16b
	orr	v27.16b,v24.16b,v24.16b
	cmp	x2,#32
	b.lo	Loop3x_cbc_dec

	ld1	{v25.16b},[x0],#16
	ld1	{v26.16b},[x0],#16
	sub	x2,x2,#32		// bias
	mov	w6,w5
	orr	v28.16b,v25.16b,v25.16b
	orr	v29.16b,v26.16b,v26.16b

Loop5x_cbc_dec:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop5x_cbc_dec

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	cmp	x2,#0x40		// because Lcbc_tail4x
	sub	x2,x2,#0x50

	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	csel	x6,xzr,x2,gt		// borrow x6, w6, "gt" is not typo
	mov	x7,x3

	aesd	v0.16b,v18.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v18.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v18.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v18.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v18.16b
	aesimc	v26.16b,v26.16b
	add	x0,x0,x6		// x0 is adjusted in such way that
					// at exit from the loop v1.16b-v26.16b
					// are loaded with last "words"
	add	x6,x2,#0x60		// because Lcbc_tail4x

	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v19.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v19.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v19.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v20.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v20.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v21.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v21.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v22.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v22.16b
	aesimc	v26.16b,v26.16b

	eor	v4.16b,v6.16b,v7.16b
	aesd	v0.16b,v23.16b
	eor	v5.16b,v2.16b,v7.16b
	ld1	{v2.16b},[x0],#16
	aesd	v1.16b,v23.16b
	eor	v17.16b,v3.16b,v7.16b
	ld1	{v3.16b},[x0],#16
	aesd	v24.16b,v23.16b
	eor	v30.16b,v27.16b,v7.16b
	ld1	{v27.16b},[x0],#16
	aesd	v25.16b,v23.16b
	eor	v31.16b,v28.16b,v7.16b
	ld1	{v28.16b},[x0],#16
	aesd	v26.16b,v23.16b
	orr	v6.16b,v29.16b,v29.16b
	ld1	{v29.16b},[x0],#16
	cbz	x6,Lcbc_tail4x
	ld1	{v16.4s},[x7],#16	// re-pre-load rndkey[0]
	eor	v4.16b,v4.16b,v0.16b
	orr	v0.16b,v2.16b,v2.16b
	eor	v5.16b,v5.16b,v1.16b
	orr	v1.16b,v3.16b,v3.16b
	eor	v17.16b,v17.16b,v24.16b
	orr	v24.16b,v27.16b,v27.16b
	eor	v30.16b,v30.16b,v25.16b
	orr	v25.16b,v28.16b,v28.16b
	eor	v31.16b,v31.16b,v26.16b
	st1	{v4.16b},[x1],#16
	orr	v26.16b,v29.16b,v29.16b
	st1	{v5.16b},[x1],#16
	mov	w6,w5
	st1	{v17.16b},[x1],#16
	ld1	{v17.4s},[x7],#16	// re-pre-load rndkey[1]
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16
	b.hs	Loop5x_cbc_dec

	add	x2,x2,#0x50
	cbz	x2,Lcbc_done

	add	w6,w5,#2
	subs	x2,x2,#0x30
	orr	v0.16b,v27.16b,v27.16b
	orr	v2.16b,v27.16b,v27.16b
	orr	v1.16b,v28.16b,v28.16b
	orr	v3.16b,v28.16b,v28.16b
	orr	v24.16b,v29.16b,v29.16b
	orr	v27.16b,v29.16b,v29.16b
	b.lo	Lcbc_dec_tail

	b	Loop3x_cbc_dec

.align	4
Lcbc_tail4x:
	eor	v5.16b,v4.16b,v1.16b
	eor	v17.16b,v17.16b,v24.16b
	eor	v30.16b,v30.16b,v25.16b
	eor	v31.16b,v31.16b,v26.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16

	b	Lcbc_done
.align	4
Loop3x_cbc_dec:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop3x_cbc_dec

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	eor	v4.16b,v6.16b,v7.16b
	subs	x2,x2,#0x30
	eor	v5.16b,v2.16b,v7.16b
	csel	x6,x2,x6,lo			// x6, w6, is zero at this point
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	eor	v17.16b,v3.16b,v7.16b
	add	x0,x0,x6		// x0 is adjusted in such way that
					// at exit from the loop v1.16b-v24.16b
					// are loaded with last "words"
	orr	v6.16b,v27.16b,v27.16b
	mov	x7,x3
	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	ld1	{v2.16b},[x0],#16
	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	ld1	{v3.16b},[x0],#16
	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	ld1	{v27.16b},[x0],#16
	aesd	v0.16b,v23.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	ld1	{v16.4s},[x7],#16	// re-pre-load rndkey[0]
	add	w6,w5,#2
	eor	v4.16b,v4.16b,v0.16b
	eor	v5.16b,v5.16b,v1.16b
	eor	v24.16b,v24.16b,v17.16b
	ld1	{v17.4s},[x7],#16	// re-pre-load rndkey[1]
	st1	{v4.16b},[x1],#16
	orr	v0.16b,v2.16b,v2.16b
	st1	{v5.16b},[x1],#16
	orr	v1.16b,v3.16b,v3.16b
	st1	{v24.16b},[x1],#16
	orr	v24.16b,v27.16b,v27.16b
	b.hs	Loop3x_cbc_dec

	cmn	x2,#0x30
	b.eq	Lcbc_done
	nop

Lcbc_dec_tail:
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lcbc_dec_tail

	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	cmn	x2,#0x20
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	eor	v5.16b,v6.16b,v7.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	eor	v17.16b,v3.16b,v7.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	b.eq	Lcbc_dec_one
	eor	v5.16b,v5.16b,v1.16b
	eor	v17.16b,v17.16b,v24.16b
	orr	v6.16b,v27.16b,v27.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	b	Lcbc_done

Lcbc_dec_one:
	eor	v5.16b,v5.16b,v24.16b
	orr	v6.16b,v27.16b,v27.16b
	st1	{v5.16b},[x1],#16

Lcbc_done:
	st1	{v6.16b},[x4]
Lcbc_abort:
	ldr	x29,[sp],#16
	ret

.globl	_aes_v8_ctr32_encrypt_blocks_unroll12_eor3

.align	5
_aes_v8_ctr32_encrypt_blocks_unroll12_eor3:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-80]!
	stp	d8,d9,[sp, #16]
	stp	d10,d11,[sp, #32]
	stp	d12,d13,[sp, #48]
	stp	d14,d15,[sp, #64]
	add	x29,sp,#0

	ldr	w5,[x3,#240]

	ldr	w8, [x4, #12]
#ifdef __AARCH64EB__
	ld1	{v24.16b},[x4]
#else
	ld1	{v24.4s},[x4]
#endif
	ld1	{v2.4s,v3.4s},[x3]		// load key schedule...
	sub	w5,w5,#4
	cmp	x2,#2
	add	x7,x3,x5,lsl#4	// pointer to last round key
	sub	w5,w5,#2
	add	x7, x7, #64
	ld1	{v1.4s},[x7]
	add	x7,x3,#32
	mov	w6,w5
#ifndef __AARCH64EB__
	rev	w8, w8
#endif

	orr	v25.16b,v24.16b,v24.16b
	add	w10, w8, #1
	orr	v26.16b,v24.16b,v24.16b
	add	w8, w8, #2
	orr	v0.16b,v24.16b,v24.16b
	rev	w10, w10
	mov	v25.s[3],w10
	b.ls	Lctr32_tail_unroll
	cmp	x2,#6
	rev	w12, w8
	sub	x2,x2,#3		// bias
	mov	v26.s[3],w12
	b.lo	Loop3x_ctr32_unroll
	cmp	x2,#9
	orr	v27.16b,v24.16b,v24.16b
	add	w11, w8, #1
	orr	v28.16b,v24.16b,v24.16b
	add	w13, w8, #2
	rev	w11, w11
	orr	v29.16b,v24.16b,v24.16b
	add	w8, w8, #3
	rev	w13, w13
	mov	v27.s[3],w11
	rev	w14, w8
	mov	v28.s[3],w13
	mov	v29.s[3],w14
	sub	x2,x2,#3
	b.lo	Loop6x_ctr32_unroll

	// push regs to stack when 12 data chunks are interleaved
	stp	x19,x20,[sp,#-16]!
	stp	x21,x22,[sp,#-16]!
	stp	x23,x24,[sp,#-16]!
	stp	d8,d9,[sp,#-32]!
	stp	d10,d11,[sp,#-32]!

	add	w15,w8,#1
	add	w19,w8,#2
	add	w20,w8,#3
	add	w21,w8,#4
	add	w22,w8,#5
	add	w8,w8,#6
	orr	v30.16b,v24.16b,v24.16b
	rev	w15,w15
	orr	v31.16b,v24.16b,v24.16b
	rev	w19,w19
	orr	v8.16b,v24.16b,v24.16b
	rev	w20,w20
	orr	v9.16b,v24.16b,v24.16b
	rev	w21,w21
	orr	v10.16b,v24.16b,v24.16b
	rev	w22,w22
	orr	v11.16b,v24.16b,v24.16b
	rev	w23,w8

	sub	x2,x2,#6		// bias
	mov	v30.s[3],w15
	mov	v31.s[3],w19
	mov	v8.s[3],w20
	mov	v9.s[3],w21
	mov	v10.s[3],w22
	mov	v11.s[3],w23
	b	Loop12x_ctr32_unroll

.align	4
Loop12x_ctr32_unroll:
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v2.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v2.16b
	aesmc	v31.16b,v31.16b
	aese	v8.16b,v2.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	ld1	{v2.4s},[x7],#16
	subs	w6,w6,#2
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v3.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v3.16b
	aesmc	v31.16b,v31.16b
	aese	v8.16b,v3.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v3.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v3.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v3.16b
	aesmc	v11.16b,v11.16b
	ld1	{v3.4s},[x7],#16
	b.gt	Loop12x_ctr32_unroll

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v2.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v2.16b
	aesmc	v31.16b,v31.16b
	aese	v8.16b,v2.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	ld1	{v2.4s},[x7],#16

	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v3.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v3.16b
	aesmc	v31.16b,v31.16b
	aese	v8.16b,v3.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v3.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v3.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v3.16b
	aesmc	v11.16b,v11.16b
	ld1	{v3.4s},[x7],#16

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	add	w9,w8,#1
	add	w10,w8,#2
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	add	w12,w8,#3
	add	w11,w8,#4
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	add	w13,w8,#5
	add	w14,w8,#6
	rev	w9,w9
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	add	w15,w8,#7
	add	w19,w8,#8
	rev	w10,w10
	rev	w12,w12
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	add	w20,w8,#9
	add	w21,w8,#10
	rev	w11,w11
	rev	w13,w13
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	add	w22,w8,#11
	add	w23,w8,#12
	rev	w14,w14
	rev	w15,w15
	aese	v30.16b,v2.16b
	aesmc	v30.16b,v30.16b
	rev	w19,w19
	rev	w20,w20
	aese	v31.16b,v2.16b
	aesmc	v31.16b,v31.16b
	rev	w21,w21
	rev	w22,w22
	aese	v8.16b,v2.16b
	aesmc	v8.16b,v8.16b
	rev	w23,w23
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	ld1	{v2.4s},[x7],#16

	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x0],#64
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v3.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v3.16b
	aesmc	v31.16b,v31.16b
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x0],#64
	aese	v8.16b,v3.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v3.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v3.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v3.16b
	aesmc	v11.16b,v11.16b
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x0],#64
	ld1	{v3.4s},[x7],#16

	mov	x7, x3
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	aese	v30.16b,v2.16b
	aesmc	v30.16b,v30.16b
	aese	v31.16b,v2.16b
	aesmc	v31.16b,v31.16b
	aese	v8.16b,v2.16b
	aesmc	v8.16b,v8.16b
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	ld1	{v2.4s},[x7],#16	// re-pre-load rndkey[0]

	aese	v24.16b,v3.16b
.long	0xce016084	//eor3 v4.16b,v4.16b,v1.16b,v24.16b
	orr	v24.16b,v0.16b,v0.16b
	aese	v25.16b,v3.16b
.long	0xce0164a5	//eor3 v5.16b,v5.16b,v1.16b,v25.16b
	orr	v25.16b,v0.16b,v0.16b
	aese	v26.16b,v3.16b
.long	0xce0168c6	//eor3 v6.16b,v6.16b,v1.16b,v26.16b
	orr	v26.16b,v0.16b,v0.16b
	aese	v27.16b,v3.16b
.long	0xce016ce7	//eor3 v7.16b,v7.16b,v1.16b,v27.16b
	orr	v27.16b,v0.16b,v0.16b
	aese	v28.16b,v3.16b
.long	0xce017210	//eor3 v16.16b,v16.16b,v1.16b,v28.16b
	orr	v28.16b,v0.16b,v0.16b
	aese	v29.16b,v3.16b
.long	0xce017631	//eor3 v17.16b,v17.16b,v1.16b,v29.16b
	orr	v29.16b,v0.16b,v0.16b
	aese	v30.16b,v3.16b
.long	0xce017a52	//eor3 v18.16b,v18.16b,v1.16b,v30.16b
	orr	v30.16b,v0.16b,v0.16b
	aese	v31.16b,v3.16b
.long	0xce017e73	//eor3 v19.16b,v19.16b,v1.16b,v31.16b
	orr	v31.16b,v0.16b,v0.16b
	aese	v8.16b,v3.16b
.long	0xce012294	//eor3 v20.16b,v20.16b,v1.16b,v8.16b
	orr	v8.16b,v0.16b,v0.16b
	aese	v9.16b,v3.16b
.long	0xce0126b5	//eor3 v21.16b,v21.16b,v1.16b,v9.16b
	orr	v9.16b,v0.16b,v0.16b
	aese	v10.16b,v3.16b
.long	0xce012ad6	//eor3 v22.16b,v22.16b,v1.16b,v10.16b
	orr	v10.16b,v0.16b,v0.16b
	aese	v11.16b,v3.16b
.long	0xce012ef7	//eor3 v23.16b,v23.16b,v1.16b,v11.16b
	orr	v11.16b,v0.16b,v0.16b
	ld1	{v3.4s},[x7],#16	// re-pre-load rndkey[1]

	mov	v24.s[3],w9
	mov	v25.s[3],w10
	mov	v26.s[3],w12
	mov	v27.s[3],w11
	st1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	mov	v28.s[3],w13
	mov	v29.s[3],w14
	mov	v30.s[3],w15
	mov	v31.s[3],w19
	st1	{v16.16b,v17.16b,v18.16b,v19.16b},[x1],#64
	mov	v8.s[3],w20
	mov	v9.s[3],w21
	mov	v10.s[3],w22
	mov	v11.s[3],w23
	st1	{v20.16b,v21.16b,v22.16b,v23.16b},[x1],#64

	mov	w6,w5

	add	w8,w8,#12
	subs	x2,x2,#12
	b.hs	Loop12x_ctr32_unroll

	// pop regs from stack when 12 data chunks are interleaved
	ldp	d10,d11,[sp],#32
	ldp	d8,d9,[sp],#32
	ldp	x23,x24,[sp],#16
	ldp	x21,x22,[sp],#16
	ldp	x19,x20,[sp],#16

	add	x2,x2,#12
	cbz	x2,Lctr32_done_unroll
	sub	w8,w8,#12

	cmp	x2,#2
	b.ls	Lctr32_tail_unroll

	cmp	x2,#6
	sub	x2,x2,#3		// bias
	add	w8,w8,#3
	b.lo	Loop3x_ctr32_unroll

	sub	x2,x2,#3
	add	w8,w8,#3
	b.lo	Loop6x_ctr32_unroll

.align	4
Loop6x_ctr32_unroll:
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	ld1	{v2.4s},[x7],#16
	subs	w6,w6,#2
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	ld1	{v3.4s},[x7],#16
	b.gt	Loop6x_ctr32_unroll

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	ld1	{v2.4s},[x7],#16

	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	ld1	{v3.4s},[x7],#16

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	add	w9,w8,#1
	add	w10,w8,#2
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	add	w12,w8,#3
	add	w11,w8,#4
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	add	w13,w8,#5
	add	w14,w8,#6
	rev	w9,w9
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	rev	w10,w10
	rev	w12,w12
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	rev	w11,w11
	rev	w13,w13
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	rev	w14,w14
	ld1	{v2.4s},[x7],#16

	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x0],#64
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v3.16b
	aesmc	v27.16b,v27.16b
	ld1	{v16.16b,v17.16b},[x0],#32
	aese	v28.16b,v3.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v3.16b
	aesmc	v29.16b,v29.16b
	ld1	{v3.4s},[x7],#16

	mov	x7, x3
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	aese	v27.16b,v2.16b
	aesmc	v27.16b,v27.16b
	aese	v28.16b,v2.16b
	aesmc	v28.16b,v28.16b
	aese	v29.16b,v2.16b
	aesmc	v29.16b,v29.16b
	ld1	{v2.4s},[x7],#16	// re-pre-load rndkey[0]

	aese	v24.16b,v3.16b
.long	0xce016084	//eor3 v4.16b,v4.16b,v1.16b,v24.16b
	aese	v25.16b,v3.16b
.long	0xce0164a5	//eor3 v5.16b,v5.16b,v1.16b,v25.16b
	aese	v26.16b,v3.16b
.long	0xce0168c6	//eor3 v6.16b,v6.16b,v1.16b,v26.16b
	aese	v27.16b,v3.16b
.long	0xce016ce7	//eor3 v7.16b,v7.16b,v1.16b,v27.16b
	aese	v28.16b,v3.16b
.long	0xce017210	//eor3 v16.16b,v16.16b,v1.16b,v28.16b
	aese	v29.16b,v3.16b
.long	0xce017631	//eor3 v17.16b,v17.16b,v1.16b,v29.16b
	ld1	{v3.4s},[x7],#16	// re-pre-load rndkey[1]

	orr	v24.16b,v0.16b,v0.16b
	orr	v25.16b,v0.16b,v0.16b
	orr	v26.16b,v0.16b,v0.16b
	orr	v27.16b,v0.16b,v0.16b
	orr	v28.16b,v0.16b,v0.16b
	orr	v29.16b,v0.16b,v0.16b

	mov	v24.s[3],w9
	mov	v25.s[3],w10
	st1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	mov	v26.s[3],w12
	mov	v27.s[3],w11
	st1	{v16.16b,v17.16b},[x1],#32
	mov	v28.s[3],w13
	mov	v29.s[3],w14

	cbz	x2,Lctr32_done_unroll
	mov	w6,w5

	cmp	x2,#2
	b.ls	Lctr32_tail_unroll

	sub	x2,x2,#3		// bias
	add	w8,w8,#3
	b	Loop3x_ctr32_unroll

.align	4
Loop3x_ctr32_unroll:
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	ld1	{v2.4s},[x7],#16
	subs	w6,w6,#2
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v3.16b
	aesmc	v26.16b,v26.16b
	ld1	{v3.4s},[x7],#16
	b.gt	Loop3x_ctr32_unroll

	aese	v24.16b,v2.16b
	aesmc	v9.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v10.16b,v25.16b
	ld1	{v4.16b,v5.16b,v6.16b},[x0],#48
	orr	v24.16b,v0.16b,v0.16b
	aese	v26.16b,v2.16b
	aesmc	v26.16b,v26.16b
	ld1	{v2.4s},[x7],#16
	orr	v25.16b,v0.16b,v0.16b
	aese	v9.16b,v3.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v3.16b
	aesmc	v10.16b,v10.16b
	aese	v26.16b,v3.16b
	aesmc	v11.16b,v26.16b
	ld1	{v3.4s},[x7],#16
	orr	v26.16b,v0.16b,v0.16b
	add	w9,w8,#1
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	add	w10,w8,#2
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	ld1	{v2.4s},[x7],#16
	add	w8,w8,#3
	aese	v9.16b,v3.16b
	aesmc	v9.16b,v9.16b
	aese	v10.16b,v3.16b
	aesmc	v10.16b,v10.16b

	rev	w9,w9
	aese	v11.16b,v3.16b
	aesmc	v11.16b,v11.16b
	ld1	{v3.4s},[x7],#16
	mov	v24.s[3], w9
	mov	x7,x3
	rev	w10,w10
	aese	v9.16b,v2.16b
	aesmc	v9.16b,v9.16b

	aese	v10.16b,v2.16b
	aesmc	v10.16b,v10.16b
	mov	v25.s[3], w10
	rev	w12,w8
	aese	v11.16b,v2.16b
	aesmc	v11.16b,v11.16b
	mov	v26.s[3], w12

	aese	v9.16b,v3.16b
	aese	v10.16b,v3.16b
	aese	v11.16b,v3.16b

.long	0xce012484	//eor3 v4.16b,v4.16b,v1.16b,v9.16b
	ld1	{v2.4s},[x7],#16	// re-pre-load rndkey[0]
.long	0xce0128a5	//eor3 v5.16b,v5.16b,v1.16b,v10.16b
	mov	w6,w5
.long	0xce012cc6	//eor3 v6.16b,v6.16b,v1.16b,v11.16b
	ld1	{v3.4s},[x7],#16	// re-pre-load rndkey[1]
	st1	{v4.16b,v5.16b,v6.16b},[x1],#48

	cbz	x2,Lctr32_done_unroll

Lctr32_tail_unroll:
	cmp	x2,#1
	b.eq	Lctr32_tail_1_unroll

Lctr32_tail_2_unroll:
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	ld1	{v2.4s},[x7],#16
	subs	w6,w6,#2
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	ld1	{v3.4s},[x7],#16
	b.gt	Lctr32_tail_2_unroll

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	ld1	{v2.4s},[x7],#16
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	ld1	{v3.4s},[x7],#16
	ld1	{v4.16b,v5.16b},[x0],#32
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	ld1	{v2.4s},[x7],#16
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v3.16b
	aesmc	v25.16b,v25.16b
	ld1	{v3.4s},[x7],#16
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v2.16b
	aesmc	v25.16b,v25.16b
	aese	v24.16b,v3.16b
	aese	v25.16b,v3.16b

.long	0xce016084	//eor3 v4.16b,v4.16b,v1.16b,v24.16b
.long	0xce0164a5	//eor3 v5.16b,v5.16b,v1.16b,v25.16b
	st1	{v4.16b,v5.16b},[x1],#32
	b	Lctr32_done_unroll

Lctr32_tail_1_unroll:
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	ld1	{v2.4s},[x7],#16
	subs	w6,w6,#2
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	ld1	{v3.4s},[x7],#16
	b.gt	Lctr32_tail_1_unroll

	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	ld1	{v2.4s},[x7],#16
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	ld1	{v3.4s},[x7],#16
	ld1	{v4.16b},[x0]
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	ld1	{v2.4s},[x7],#16
	aese	v24.16b,v3.16b
	aesmc	v24.16b,v24.16b
	ld1	{v3.4s},[x7],#16
	aese	v24.16b,v2.16b
	aesmc	v24.16b,v24.16b
	aese	v24.16b,v3.16b

.long	0xce016084	//eor3 v4.16b,v4.16b,v1.16b,v24.16b
	st1	{v4.16b},[x1],#16

Lctr32_done_unroll:
	ldp	d8,d9,[sp, #16]
	ldp	d10,d11,[sp, #32]
	ldp	d12,d13,[sp, #48]
	ldp	d14,d15,[sp, #64]
	ldr	x29,[sp],#80
	ret

.globl	_aes_v8_ctr32_encrypt_blocks

.align	5
_aes_v8_ctr32_encrypt_blocks:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	ldr	w5,[x3,#240]

	ldr	w8, [x4, #12]
#ifdef __AARCH64EB__
	ld1	{v0.16b},[x4]
#else
	ld1	{v0.4s},[x4]
#endif
	ld1	{v16.4s,v17.4s},[x3]		// load key schedule...
	sub	w5,w5,#4
	mov	x12,#16
	cmp	x2,#2
	add	x7,x3,x5,lsl#4	// pointer to last 5 round keys
	sub	w5,w5,#2
	ld1	{v20.4s,v21.4s},[x7],#32
	ld1	{v22.4s,v23.4s},[x7],#32
	ld1	{v7.4s},[x7]
	add	x7,x3,#32
	mov	w6,w5
	csel	x12,xzr,x12,lo
#ifndef __AARCH64EB__
	rev	w8, w8
#endif
	orr	v1.16b,v0.16b,v0.16b
	add	w10, w8, #1
	orr	v18.16b,v0.16b,v0.16b
	add	w8, w8, #2
	orr	v6.16b,v0.16b,v0.16b
	rev	w10, w10
	mov	v1.s[3],w10
	b.ls	Lctr32_tail
	rev	w12, w8
	sub	x2,x2,#3		// bias
	mov	v18.s[3],w12
	cmp	x2,#32
	b.lo	Loop3x_ctr32

	add	w13,w8,#1
	add	w14,w8,#2
	orr	v24.16b,v0.16b,v0.16b
	rev	w13,w13
	orr	v25.16b,v0.16b,v0.16b
	rev	w14,w14
	mov	v24.s[3],w13
	sub	x2,x2,#2		// bias
	mov	v25.s[3],w14
	add	w8,w8,#2
	b	Loop5x_ctr32

.align	4
Loop5x_ctr32:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v16.16b
	aesmc	v18.16b,v18.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v17.16b
	aesmc	v18.16b,v18.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop5x_ctr32

	mov	x7,x3
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v16.16b
	aesmc	v18.16b,v18.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	ld1	{v16.4s},[x7],#16	// re-pre-load rndkey[0]

	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v17.16b
	aesmc	v18.16b,v18.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	ld1	{v17.4s},[x7],#16	// re-pre-load rndkey[1]

	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	add	w9,w8,#1
	add	w10,w8,#2
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	add	w12,w8,#3
	add	w13,w8,#4
	aese	v18.16b,v20.16b
	aesmc	v18.16b,v18.16b
	add	w14,w8,#5
	rev	w9,w9
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	rev	w10,w10
	rev	w12,w12
	aese	v25.16b,v20.16b
	aesmc	v25.16b,v25.16b
	rev	w13,w13
	rev	w14,w14

	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v21.16b
	aesmc	v18.16b,v18.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v21.16b
	aesmc	v25.16b,v25.16b

	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	ld1	{v2.16b},[x0],#16
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	ld1	{v3.16b},[x0],#16
	aese	v18.16b,v22.16b
	aesmc	v18.16b,v18.16b
	ld1	{v19.16b},[x0],#16
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	ld1	{v26.16b},[x0],#16
	aese	v25.16b,v22.16b
	aesmc	v25.16b,v25.16b
	ld1	{v27.16b},[x0],#16

	aese	v0.16b,v23.16b
	eor	v2.16b,v2.16b,v7.16b
	aese	v1.16b,v23.16b
	eor	v3.16b,v3.16b,v7.16b
	aese	v18.16b,v23.16b
	eor	v19.16b,v19.16b,v7.16b
	aese	v24.16b,v23.16b
	eor	v26.16b,v26.16b,v7.16b
	aese	v25.16b,v23.16b
	eor	v27.16b,v27.16b,v7.16b

	eor	v2.16b,v2.16b,v0.16b
	orr	v0.16b,v6.16b,v6.16b
	eor	v3.16b,v3.16b,v1.16b
	orr	v1.16b,v6.16b,v6.16b
	eor	v19.16b,v19.16b,v18.16b
	orr	v18.16b,v6.16b,v6.16b
	eor	v26.16b,v26.16b,v24.16b
	orr	v24.16b,v6.16b,v6.16b
	eor	v27.16b,v27.16b,v25.16b
	orr	v25.16b,v6.16b,v6.16b

	st1	{v2.16b},[x1],#16
	mov	v0.s[3],w9
	st1	{v3.16b},[x1],#16
	mov	v1.s[3],w10
	st1	{v19.16b},[x1],#16
	mov	v18.s[3],w12
	st1	{v26.16b},[x1],#16
	mov	v24.s[3],w13
	st1	{v27.16b},[x1],#16
	mov	v25.s[3],w14

	mov	w6,w5
	cbz	x2,Lctr32_done

	add	w8,w8,#5
	subs	x2,x2,#5
	b.hs	Loop5x_ctr32

	add	x2,x2,#5
	sub	w8,w8,#5

	cmp	x2,#2
	mov	x12,#16
	csel	x12,xzr,x12,lo
	b.ls	Lctr32_tail

	sub	x2,x2,#3		// bias
	add	w8,w8,#3
	b	Loop3x_ctr32

.align	4
Loop3x_ctr32:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v16.16b
	aesmc	v18.16b,v18.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v18.16b,v17.16b
	aesmc	v18.16b,v18.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop3x_ctr32

	aese	v0.16b,v16.16b
	aesmc	v4.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v5.16b,v1.16b
	ld1	{v2.16b},[x0],#16
	orr	v0.16b,v6.16b,v6.16b
	aese	v18.16b,v16.16b
	aesmc	v18.16b,v18.16b
	ld1	{v3.16b},[x0],#16
	orr	v1.16b,v6.16b,v6.16b
	aese	v4.16b,v17.16b
	aesmc	v4.16b,v4.16b
	aese	v5.16b,v17.16b
	aesmc	v5.16b,v5.16b
	ld1	{v19.16b},[x0],#16
	mov	x7,x3
	aese	v18.16b,v17.16b
	aesmc	v17.16b,v18.16b
	orr	v18.16b,v6.16b,v6.16b
	add	w9,w8,#1
	aese	v4.16b,v20.16b
	aesmc	v4.16b,v4.16b
	aese	v5.16b,v20.16b
	aesmc	v5.16b,v5.16b
	eor	v2.16b,v2.16b,v7.16b
	add	w10,w8,#2
	aese	v17.16b,v20.16b
	aesmc	v17.16b,v17.16b
	eor	v3.16b,v3.16b,v7.16b
	add	w8,w8,#3
	aese	v4.16b,v21.16b
	aesmc	v4.16b,v4.16b
	aese	v5.16b,v21.16b
	aesmc	v5.16b,v5.16b
	eor	v19.16b,v19.16b,v7.16b
	rev	w9,w9
	aese	v17.16b,v21.16b
	aesmc	v17.16b,v17.16b
	mov	v0.s[3], w9
	rev	w10,w10
	aese	v4.16b,v22.16b
	aesmc	v4.16b,v4.16b
	aese	v5.16b,v22.16b
	aesmc	v5.16b,v5.16b
	mov	v1.s[3], w10
	rev	w12,w8
	aese	v17.16b,v22.16b
	aesmc	v17.16b,v17.16b
	mov	v18.s[3], w12
	subs	x2,x2,#3
	aese	v4.16b,v23.16b
	aese	v5.16b,v23.16b
	aese	v17.16b,v23.16b

	eor	v2.16b,v2.16b,v4.16b
	ld1	{v16.4s},[x7],#16	// re-pre-load rndkey[0]
	st1	{v2.16b},[x1],#16
	eor	v3.16b,v3.16b,v5.16b
	mov	w6,w5
	st1	{v3.16b},[x1],#16
	eor	v19.16b,v19.16b,v17.16b
	ld1	{v17.4s},[x7],#16	// re-pre-load rndkey[1]
	st1	{v19.16b},[x1],#16
	b.hs	Loop3x_ctr32

	adds	x2,x2,#3
	b.eq	Lctr32_done
	cmp	x2,#1
	mov	x12,#16
	csel	x12,xzr,x12,eq

Lctr32_tail:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lctr32_tail

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	ld1	{v2.16b},[x0],x12
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	ld1	{v3.16b},[x0]
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	eor	v2.16b,v2.16b,v7.16b
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	eor	v3.16b,v3.16b,v7.16b
	aese	v0.16b,v23.16b
	aese	v1.16b,v23.16b

	cmp	x2,#1
	eor	v2.16b,v2.16b,v0.16b
	eor	v3.16b,v3.16b,v1.16b
	st1	{v2.16b},[x1],#16
	b.eq	Lctr32_done
	st1	{v3.16b},[x1]

Lctr32_done:
	ldr	x29,[sp],#16
	ret

.globl	_aes_v8_xts_encrypt

.align	5
_aes_v8_xts_encrypt:
	AARCH64_VALID_CALL_TARGET
	cmp	x2,#16
	// Original input data size bigger than 16, jump to big size processing.
	b.ne	Lxts_enc_big_size
	// Encrypt the iv with key2, as the first XEX iv.
	ldr	w6,[x4,#240]
	ld1	{v0.4s},[x4],#16
	ld1	{v6.16b},[x5]
	sub	w6,w6,#2
	ld1	{v1.4s},[x4],#16

Loop_enc_iv_enc:
	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4],#16
	subs	w6,w6,#2
	aese	v6.16b,v1.16b
	aesmc	v6.16b,v6.16b
	ld1	{v1.4s},[x4],#16
	b.gt	Loop_enc_iv_enc

	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4]
	aese	v6.16b,v1.16b
	eor	v6.16b,v6.16b,v0.16b

	ld1	{v0.16b},[x0]
	eor	v0.16b,v6.16b,v0.16b

	ldr	w6,[x3,#240]
	ld1	{v28.4s,v29.4s},[x3],#32		// load key schedule...

	aese	v0.16b,v28.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s,v17.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v29.16b
	aesmc	v0.16b,v0.16b
	subs	w6,w6,#10		// if rounds==10, jump to aes-128-xts processing
	b.eq	Lxts_128_enc
Lxts_enc_round_loop:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	ld1	{v16.4s},[x3],#16		// load key schedule...
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	ld1	{v17.4s},[x3],#16		// load key schedule...
	subs	w6,w6,#2		// bias
	b.gt	Lxts_enc_round_loop
Lxts_128_enc:
	ld1	{v18.4s,v19.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	ld1	{v20.4s,v21.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	ld1	{v22.4s,v23.4s},[x3],#32		// load key schedule...
	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	ld1	{v7.4s},[x3]
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v7.16b
	eor	v0.16b,v0.16b,v6.16b
	st1	{v0.16b},[x1]
	b	Lxts_enc_final_abort

.align	4
Lxts_enc_big_size:
	stp	x19,x20,[sp,#-64]!
	stp	x21,x22,[sp,#48]
	stp	d8,d9,[sp,#32]
	stp	d10,d11,[sp,#16]

	// tailcnt store the tail value of length%16.
	and	x21,x2,#0xf
	and	x2,x2,#-16
	subs	x2,x2,#16
	mov	x8,#16
	b.lo	Lxts_abort
	csel	x8,xzr,x8,eq

	// Firstly, encrypt the iv with key2, as the first iv of XEX.
	ldr	w6,[x4,#240]
	ld1	{v0.4s},[x4],#16
	ld1	{v6.16b},[x5]
	sub	w6,w6,#2
	ld1	{v1.4s},[x4],#16

Loop_iv_enc:
	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4],#16
	subs	w6,w6,#2
	aese	v6.16b,v1.16b
	aesmc	v6.16b,v6.16b
	ld1	{v1.4s},[x4],#16
	b.gt	Loop_iv_enc

	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4]
	aese	v6.16b,v1.16b
	eor	v6.16b,v6.16b,v0.16b

	// The iv for second block
	// x9- iv(low), x10 - iv(high)
	// the five ivs stored into, v6.16b,v8.16b,v9.16b,v10.16b,v11.16b
	fmov	x9,d6
	fmov	x10,v6.d[1]
	mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d8,x9
	fmov	v8.d[1],x10

	ldr	w5,[x3,#240]		// next starting point
	ld1	{v0.16b},[x0],x8

	ld1	{v16.4s,v17.4s},[x3]			// load key schedule...
	sub	w5,w5,#6
	add	x7,x3,x5,lsl#4		// pointer to last 7 round keys
	sub	w5,w5,#2
	ld1	{v18.4s,v19.4s},[x7],#32
	ld1	{v20.4s,v21.4s},[x7],#32
	ld1	{v22.4s,v23.4s},[x7],#32
	ld1	{v7.4s},[x7]

	add	x7,x3,#32
	mov	w6,w5

	// Encryption
Lxts_enc:
	ld1	{v24.16b},[x0],#16
	subs	x2,x2,#32			// bias
	add	w6,w5,#2
	orr	v3.16b,v0.16b,v0.16b
	orr	v1.16b,v0.16b,v0.16b
	orr	v28.16b,v0.16b,v0.16b
	orr	v27.16b,v24.16b,v24.16b
	orr	v29.16b,v24.16b,v24.16b
	b.lo	Lxts_inner_enc_tail
	eor	v0.16b,v0.16b,v6.16b			// before encryption, xor with iv
	eor	v24.16b,v24.16b,v8.16b

	// The iv for third block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d9,x9
	fmov	v9.d[1],x10


	orr	v1.16b,v24.16b,v24.16b
	ld1	{v24.16b},[x0],#16
	orr	v2.16b,v0.16b,v0.16b
	orr	v3.16b,v1.16b,v1.16b
	eor	v27.16b,v24.16b,v9.16b 		// the third block
	eor	v24.16b,v24.16b,v9.16b
	cmp	x2,#32
	b.lo	Lxts_outer_enc_tail

	// The iv for fourth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d10,x9
	fmov	v10.d[1],x10

	ld1	{v25.16b},[x0],#16
	// The iv for fifth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d11,x9
	fmov	v11.d[1],x10

	ld1	{v26.16b},[x0],#16
	eor	v25.16b,v25.16b,v10.16b		// the fourth block
	eor	v26.16b,v26.16b,v11.16b
	sub	x2,x2,#32			// bias
	mov	w6,w5
	b	Loop5x_xts_enc

.align	4
Loop5x_xts_enc:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v16.16b
	aesmc	v26.16b,v26.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v17.16b
	aesmc	v26.16b,v26.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Loop5x_xts_enc

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v16.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v16.16b
	aesmc	v26.16b,v26.16b
	subs	x2,x2,#0x50			// because Lxts_enc_tail4x

	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v17.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v17.16b
	aesmc	v26.16b,v26.16b
	csel	x6,xzr,x2,gt		// borrow x6, w6, "gt" is not typo
	mov	x7,x3

	aese	v0.16b,v18.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v18.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v18.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v18.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v18.16b
	aesmc	v26.16b,v26.16b
	add	x0,x0,x6		// x0 is adjusted in such way that
						// at exit from the loop v1.16b-v26.16b
						// are loaded with last "words"
	add	x6,x2,#0x60		// because Lxts_enc_tail4x

	aese	v0.16b,v19.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v19.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v19.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v19.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v19.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v20.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v20.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v21.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v21.16b
	aesmc	v26.16b,v26.16b

	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	aese	v25.16b,v22.16b
	aesmc	v25.16b,v25.16b
	aese	v26.16b,v22.16b
	aesmc	v26.16b,v26.16b

	eor	v4.16b,v7.16b,v6.16b
	aese	v0.16b,v23.16b
	// The iv for first block of one iteration
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d6,x9
	fmov	v6.d[1],x10
	eor	v5.16b,v7.16b,v8.16b
	ld1	{v2.16b},[x0],#16
	aese	v1.16b,v23.16b
	// The iv for second block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d8,x9
	fmov	v8.d[1],x10
	eor	v17.16b,v7.16b,v9.16b
	ld1	{v3.16b},[x0],#16
	aese	v24.16b,v23.16b
	// The iv for third block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d9,x9
	fmov	v9.d[1],x10
	eor	v30.16b,v7.16b,v10.16b
	ld1	{v27.16b},[x0],#16
	aese	v25.16b,v23.16b
	// The iv for fourth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d10,x9
	fmov	v10.d[1],x10
	eor	v31.16b,v7.16b,v11.16b
	ld1	{v28.16b},[x0],#16
	aese	v26.16b,v23.16b

	// The iv for fifth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d11,x9
	fmov	v11.d[1],x10

	ld1	{v29.16b},[x0],#16
	cbz	x6,Lxts_enc_tail4x
	ld1	{v16.4s},[x7],#16		// re-pre-load rndkey[0]
	eor	v4.16b,v4.16b,v0.16b
	eor	v0.16b,v2.16b,v6.16b
	eor	v5.16b,v5.16b,v1.16b
	eor	v1.16b,v3.16b,v8.16b
	eor	v17.16b,v17.16b,v24.16b
	eor	v24.16b,v27.16b,v9.16b
	eor	v30.16b,v30.16b,v25.16b
	eor	v25.16b,v28.16b,v10.16b
	eor	v31.16b,v31.16b,v26.16b
	st1	{v4.16b},[x1],#16
	eor	v26.16b,v29.16b,v11.16b
	st1	{v5.16b},[x1],#16
	mov	w6,w5
	st1	{v17.16b},[x1],#16
	ld1	{v17.4s},[x7],#16		// re-pre-load rndkey[1]
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16
	b.hs	Loop5x_xts_enc


	// If left 4 blocks, borrow the five block's processing.
	cmn	x2,#0x10
	b.ne	Loop5x_enc_after
	orr	v11.16b,v10.16b,v10.16b
	orr	v10.16b,v9.16b,v9.16b
	orr	v9.16b,v8.16b,v8.16b
	orr	v8.16b,v6.16b,v6.16b
	fmov	x9,d11
	fmov	x10,v11.d[1]
	eor	v0.16b,v6.16b,v2.16b
	eor	v1.16b,v8.16b,v3.16b
	eor	v24.16b,v27.16b,v9.16b
	eor	v25.16b,v28.16b,v10.16b
	eor	v26.16b,v29.16b,v11.16b
	b.eq	Loop5x_xts_enc

Loop5x_enc_after:
	add	x2,x2,#0x50
	cbz	x2,Lxts_enc_done

	add	w6,w5,#2
	subs	x2,x2,#0x30
	b.lo	Lxts_inner_enc_tail

	eor	v0.16b,v6.16b,v27.16b
	eor	v1.16b,v8.16b,v28.16b
	eor	v24.16b,v29.16b,v9.16b
	b	Lxts_outer_enc_tail

.align	4
Lxts_enc_tail4x:
	add	x0,x0,#16
	eor	v5.16b,v1.16b,v5.16b
	st1	{v5.16b},[x1],#16
	eor	v17.16b,v24.16b,v17.16b
	st1	{v17.16b},[x1],#16
	eor	v30.16b,v25.16b,v30.16b
	eor	v31.16b,v26.16b,v31.16b
	st1	{v30.16b,v31.16b},[x1],#32

	b	Lxts_enc_done
.align	4
Lxts_outer_enc_tail:
	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lxts_outer_enc_tail

	aese	v0.16b,v16.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	eor	v4.16b,v6.16b,v7.16b
	subs	x2,x2,#0x30
	// The iv for first block
	fmov	x9,d9
	fmov	x10,v9.d[1]
	//mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr#31
	eor	x9,x11,x9,lsl#1
	fmov	d6,x9
	fmov	v6.d[1],x10
	eor	v5.16b,v8.16b,v7.16b
	csel	x6,x2,x6,lo       // x6, w6, is zero at this point
	aese	v0.16b,v17.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	eor	v17.16b,v9.16b,v7.16b

	add	x6,x6,#0x20
	add	x0,x0,x6
	mov	x7,x3

	aese	v0.16b,v20.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	aese	v0.16b,v21.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	aese	v0.16b,v22.16b
	aesmc	v0.16b,v0.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	aese	v0.16b,v23.16b
	aese	v1.16b,v23.16b
	aese	v24.16b,v23.16b
	ld1	{v27.16b},[x0],#16
	add	w6,w5,#2
	ld1	{v16.4s},[x7],#16                // re-pre-load rndkey[0]
	eor	v4.16b,v4.16b,v0.16b
	eor	v5.16b,v5.16b,v1.16b
	eor	v24.16b,v24.16b,v17.16b
	ld1	{v17.4s},[x7],#16                // re-pre-load rndkey[1]
	st1	{v4.16b},[x1],#16
	st1	{v5.16b},[x1],#16
	st1	{v24.16b},[x1],#16
	cmn	x2,#0x30
	b.eq	Lxts_enc_done
Lxts_encxor_one:
	orr	v28.16b,v3.16b,v3.16b
	orr	v29.16b,v27.16b,v27.16b
	nop

Lxts_inner_enc_tail:
	cmn	x2,#0x10
	eor	v1.16b,v28.16b,v6.16b
	eor	v24.16b,v29.16b,v8.16b
	b.eq	Lxts_enc_tail_loop
	eor	v24.16b,v29.16b,v6.16b
Lxts_enc_tail_loop:
	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lxts_enc_tail_loop

	aese	v1.16b,v16.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v16.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v17.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v17.16b
	aesmc	v24.16b,v24.16b
	aese	v1.16b,v20.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v20.16b
	aesmc	v24.16b,v24.16b
	cmn	x2,#0x20
	aese	v1.16b,v21.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v21.16b
	aesmc	v24.16b,v24.16b
	eor	v5.16b,v6.16b,v7.16b
	aese	v1.16b,v22.16b
	aesmc	v1.16b,v1.16b
	aese	v24.16b,v22.16b
	aesmc	v24.16b,v24.16b
	eor	v17.16b,v8.16b,v7.16b
	aese	v1.16b,v23.16b
	aese	v24.16b,v23.16b
	b.eq	Lxts_enc_one
	eor	v5.16b,v5.16b,v1.16b
	st1	{v5.16b},[x1],#16
	eor	v17.16b,v17.16b,v24.16b
	orr	v6.16b,v8.16b,v8.16b
	st1	{v17.16b},[x1],#16
	fmov	x9,d8
	fmov	x10,v8.d[1]
	mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d6,x9
	fmov	v6.d[1],x10
	b	Lxts_enc_done

Lxts_enc_one:
	eor	v5.16b,v5.16b,v24.16b
	orr	v6.16b,v6.16b,v6.16b
	st1	{v5.16b},[x1],#16
	fmov	x9,d6
	fmov	x10,v6.d[1]
	mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d6,x9
	fmov	v6.d[1],x10
	b	Lxts_enc_done
.align	5
Lxts_enc_done:
	// Process the tail block with cipher stealing.
	tst	x21,#0xf
	b.eq	Lxts_abort

	mov	x20,x0
	mov	x13,x1
	sub	x1,x1,#16
.composite_enc_loop:
	subs	x21,x21,#1
	ldrb	w15,[x1,x21]
	ldrb	w14,[x20,x21]
	strb	w15,[x13,x21]
	strb	w14,[x1,x21]
	b.gt	.composite_enc_loop
Lxts_enc_load_done:
	ld1	{v26.16b},[x1]
	eor	v26.16b,v26.16b,v6.16b

	// Encrypt the composite block to get the last second encrypted text block
	ldr	w6,[x3,#240]		// load key schedule...
	ld1	{v0.4s},[x3],#16
	sub	w6,w6,#2
	ld1	{v1.4s},[x3],#16		// load key schedule...
Loop_final_enc:
	aese	v26.16b,v0.16b
	aesmc	v26.16b,v26.16b
	ld1	{v0.4s},[x3],#16
	subs	w6,w6,#2
	aese	v26.16b,v1.16b
	aesmc	v26.16b,v26.16b
	ld1	{v1.4s},[x3],#16
	b.gt	Loop_final_enc

	aese	v26.16b,v0.16b
	aesmc	v26.16b,v26.16b
	ld1	{v0.4s},[x3]
	aese	v26.16b,v1.16b
	eor	v26.16b,v26.16b,v0.16b
	eor	v26.16b,v26.16b,v6.16b
	st1	{v26.16b},[x1]

Lxts_abort:
	ldp	x21,x22,[sp,#48]
	ldp	d8,d9,[sp,#32]
	ldp	d10,d11,[sp,#16]
	ldp	x19,x20,[sp],#64
Lxts_enc_final_abort:
	ret

.globl	_aes_v8_xts_decrypt

.align	5
_aes_v8_xts_decrypt:
	AARCH64_VALID_CALL_TARGET
	cmp	x2,#16
	// Original input data size bigger than 16, jump to big size processing.
	b.ne	Lxts_dec_big_size
	// Encrypt the iv with key2, as the first XEX iv.
	ldr	w6,[x4,#240]
	ld1	{v0.4s},[x4],#16
	ld1	{v6.16b},[x5]
	sub	w6,w6,#2
	ld1	{v1.4s},[x4],#16

Loop_dec_small_iv_enc:
	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4],#16
	subs	w6,w6,#2
	aese	v6.16b,v1.16b
	aesmc	v6.16b,v6.16b
	ld1	{v1.4s},[x4],#16
	b.gt	Loop_dec_small_iv_enc

	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4]
	aese	v6.16b,v1.16b
	eor	v6.16b,v6.16b,v0.16b

	ld1	{v0.16b},[x0]
	eor	v0.16b,v6.16b,v0.16b

	ldr	w6,[x3,#240]
	ld1	{v28.4s,v29.4s},[x3],#32			// load key schedule...

	aesd	v0.16b,v28.16b
	aesimc	v0.16b,v0.16b
	ld1	{v16.4s,v17.4s},[x3],#32			// load key schedule...
	aesd	v0.16b,v29.16b
	aesimc	v0.16b,v0.16b
	subs	w6,w6,#10			// bias
	b.eq	Lxts_128_dec
Lxts_dec_round_loop:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	ld1	{v16.4s},[x3],#16			// load key schedule...
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	ld1	{v17.4s},[x3],#16			// load key schedule...
	subs	w6,w6,#2			// bias
	b.gt	Lxts_dec_round_loop
Lxts_128_dec:
	ld1	{v18.4s,v19.4s},[x3],#32			// load key schedule...
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	ld1	{v20.4s,v21.4s},[x3],#32			// load key schedule...
	aesd	v0.16b,v18.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	ld1	{v22.4s,v23.4s},[x3],#32			// load key schedule...
	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	ld1	{v7.4s},[x3]
	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v0.16b,v23.16b
	eor	v0.16b,v0.16b,v7.16b
	eor	v0.16b,v6.16b,v0.16b
	st1	{v0.16b},[x1]
	b	Lxts_dec_final_abort
Lxts_dec_big_size:
	stp	x19,x20,[sp,#-64]!
	stp	x21,x22,[sp,#48]
	stp	d8,d9,[sp,#32]
	stp	d10,d11,[sp,#16]

	and	x21,x2,#0xf
	and	x2,x2,#-16
	subs	x2,x2,#16
	mov	x8,#16
	b.lo	Lxts_dec_abort

	// Encrypt the iv with key2, as the first XEX iv
	ldr	w6,[x4,#240]
	ld1	{v0.4s},[x4],#16
	ld1	{v6.16b},[x5]
	sub	w6,w6,#2
	ld1	{v1.4s},[x4],#16

Loop_dec_iv_enc:
	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4],#16
	subs	w6,w6,#2
	aese	v6.16b,v1.16b
	aesmc	v6.16b,v6.16b
	ld1	{v1.4s},[x4],#16
	b.gt	Loop_dec_iv_enc

	aese	v6.16b,v0.16b
	aesmc	v6.16b,v6.16b
	ld1	{v0.4s},[x4]
	aese	v6.16b,v1.16b
	eor	v6.16b,v6.16b,v0.16b

	// The iv for second block
	// x9- iv(low), x10 - iv(high)
	// the five ivs stored into, v6.16b,v8.16b,v9.16b,v10.16b,v11.16b
	fmov	x9,d6
	fmov	x10,v6.d[1]
	mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d8,x9
	fmov	v8.d[1],x10

	ldr	w5,[x3,#240]		// load rounds number

	// The iv for third block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d9,x9
	fmov	v9.d[1],x10

	ld1	{v16.4s,v17.4s},[x3]			// load key schedule...
	sub	w5,w5,#6
	add	x7,x3,x5,lsl#4		// pointer to last 7 round keys
	sub	w5,w5,#2
	ld1	{v18.4s,v19.4s},[x7],#32		// load key schedule...
	ld1	{v20.4s,v21.4s},[x7],#32
	ld1	{v22.4s,v23.4s},[x7],#32
	ld1	{v7.4s},[x7]

	// The iv for fourth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d10,x9
	fmov	v10.d[1],x10

	add	x7,x3,#32
	mov	w6,w5
	b	Lxts_dec

	// Decryption
.align	5
Lxts_dec:
	tst	x21,#0xf
	b.eq	Lxts_dec_begin
	subs	x2,x2,#16
	csel	x8,xzr,x8,eq
	ld1	{v0.16b},[x0],#16
	b.lo	Lxts_done
	sub	x0,x0,#16
Lxts_dec_begin:
	ld1	{v0.16b},[x0],x8
	subs	x2,x2,#32			// bias
	add	w6,w5,#2
	orr	v3.16b,v0.16b,v0.16b
	orr	v1.16b,v0.16b,v0.16b
	orr	v28.16b,v0.16b,v0.16b
	ld1	{v24.16b},[x0],#16
	orr	v27.16b,v24.16b,v24.16b
	orr	v29.16b,v24.16b,v24.16b
	b.lo	Lxts_inner_dec_tail
	eor	v0.16b,v0.16b,v6.16b			// before decryt, xor with iv
	eor	v24.16b,v24.16b,v8.16b

	orr	v1.16b,v24.16b,v24.16b
	ld1	{v24.16b},[x0],#16
	orr	v2.16b,v0.16b,v0.16b
	orr	v3.16b,v1.16b,v1.16b
	eor	v27.16b,v24.16b,v9.16b			// third block xox with third iv
	eor	v24.16b,v24.16b,v9.16b
	cmp	x2,#32
	b.lo	Lxts_outer_dec_tail

	ld1	{v25.16b},[x0],#16

	// The iv for fifth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d11,x9
	fmov	v11.d[1],x10

	ld1	{v26.16b},[x0],#16
	eor	v25.16b,v25.16b,v10.16b		// the fourth block
	eor	v26.16b,v26.16b,v11.16b
	sub	x2,x2,#32			// bias
	mov	w6,w5
	b	Loop5x_xts_dec

.align	4
Loop5x_xts_dec:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	ld1	{v16.4s},[x7],#16		// load key schedule...
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	ld1	{v17.4s},[x7],#16		// load key schedule...
	b.gt	Loop5x_xts_dec

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v16.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v16.16b
	aesimc	v26.16b,v26.16b
	subs	x2,x2,#0x50			// because Lxts_dec_tail4x

	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v17.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v17.16b
	aesimc	v26.16b,v26.16b
	csel	x6,xzr,x2,gt		// borrow x6, w6, "gt" is not typo
	mov	x7,x3

	aesd	v0.16b,v18.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v18.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v18.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v18.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v18.16b
	aesimc	v26.16b,v26.16b
	add	x0,x0,x6		// x0 is adjusted in such way that
						// at exit from the loop v1.16b-v26.16b
						// are loaded with last "words"
	add	x6,x2,#0x60		// because Lxts_dec_tail4x

	aesd	v0.16b,v19.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v19.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v19.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v19.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v19.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v20.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v20.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v21.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v21.16b
	aesimc	v26.16b,v26.16b

	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	aesd	v25.16b,v22.16b
	aesimc	v25.16b,v25.16b
	aesd	v26.16b,v22.16b
	aesimc	v26.16b,v26.16b

	eor	v4.16b,v7.16b,v6.16b
	aesd	v0.16b,v23.16b
	// The iv for first block of next iteration.
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d6,x9
	fmov	v6.d[1],x10
	eor	v5.16b,v7.16b,v8.16b
	ld1	{v2.16b},[x0],#16
	aesd	v1.16b,v23.16b
	// The iv for second block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d8,x9
	fmov	v8.d[1],x10
	eor	v17.16b,v7.16b,v9.16b
	ld1	{v3.16b},[x0],#16
	aesd	v24.16b,v23.16b
	// The iv for third block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d9,x9
	fmov	v9.d[1],x10
	eor	v30.16b,v7.16b,v10.16b
	ld1	{v27.16b},[x0],#16
	aesd	v25.16b,v23.16b
	// The iv for fourth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d10,x9
	fmov	v10.d[1],x10
	eor	v31.16b,v7.16b,v11.16b
	ld1	{v28.16b},[x0],#16
	aesd	v26.16b,v23.16b

	// The iv for fifth block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d11,x9
	fmov	v11.d[1],x10

	ld1	{v29.16b},[x0],#16
	cbz	x6,Lxts_dec_tail4x
	ld1	{v16.4s},[x7],#16		// re-pre-load rndkey[0]
	eor	v4.16b,v4.16b,v0.16b
	eor	v0.16b,v2.16b,v6.16b
	eor	v5.16b,v5.16b,v1.16b
	eor	v1.16b,v3.16b,v8.16b
	eor	v17.16b,v17.16b,v24.16b
	eor	v24.16b,v27.16b,v9.16b
	eor	v30.16b,v30.16b,v25.16b
	eor	v25.16b,v28.16b,v10.16b
	eor	v31.16b,v31.16b,v26.16b
	st1	{v4.16b},[x1],#16
	eor	v26.16b,v29.16b,v11.16b
	st1	{v5.16b},[x1],#16
	mov	w6,w5
	st1	{v17.16b},[x1],#16
	ld1	{v17.4s},[x7],#16		// re-pre-load rndkey[1]
	st1	{v30.16b},[x1],#16
	st1	{v31.16b},[x1],#16
	b.hs	Loop5x_xts_dec

	cmn	x2,#0x10
	b.ne	Loop5x_dec_after
	// If x2(x2) equal to -0x10, the left blocks is 4.
	// After specially processing, utilize the five blocks processing again.
	// It will use the following IVs: v6.16b,v6.16b,v8.16b,v9.16b,v10.16b.
	orr	v11.16b,v10.16b,v10.16b
	orr	v10.16b,v9.16b,v9.16b
	orr	v9.16b,v8.16b,v8.16b
	orr	v8.16b,v6.16b,v6.16b
	fmov	x9,d11
	fmov	x10,v11.d[1]
	eor	v0.16b,v6.16b,v2.16b
	eor	v1.16b,v8.16b,v3.16b
	eor	v24.16b,v27.16b,v9.16b
	eor	v25.16b,v28.16b,v10.16b
	eor	v26.16b,v29.16b,v11.16b
	b.eq	Loop5x_xts_dec

Loop5x_dec_after:
	add	x2,x2,#0x50
	cbz	x2,Lxts_done

	add	w6,w5,#2
	subs	x2,x2,#0x30
	b.lo	Lxts_inner_dec_tail

	eor	v0.16b,v6.16b,v27.16b
	eor	v1.16b,v8.16b,v28.16b
	eor	v24.16b,v29.16b,v9.16b
	b	Lxts_outer_dec_tail

.align	4
Lxts_dec_tail4x:
	add	x0,x0,#16
	tst	x21,#0xf
	eor	v5.16b,v1.16b,v4.16b
	st1	{v5.16b},[x1],#16
	eor	v17.16b,v24.16b,v17.16b
	st1	{v17.16b},[x1],#16
	eor	v30.16b,v25.16b,v30.16b
	eor	v31.16b,v26.16b,v31.16b
	st1	{v30.16b,v31.16b},[x1],#32

	b.eq	Lxts_dec_abort
	ld1	{v0.16b},[x0],#16
	b	Lxts_done
.align	4
Lxts_outer_dec_tail:
	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lxts_outer_dec_tail

	aesd	v0.16b,v16.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	eor	v4.16b,v6.16b,v7.16b
	subs	x2,x2,#0x30
	// The iv for first block
	fmov	x9,d9
	fmov	x10,v9.d[1]
	mov	w19,#0x87
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d6,x9
	fmov	v6.d[1],x10
	eor	v5.16b,v8.16b,v7.16b
	csel	x6,x2,x6,lo	// x6, w6, is zero at this point
	aesd	v0.16b,v17.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	eor	v17.16b,v9.16b,v7.16b
	// The iv for second block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d8,x9
	fmov	v8.d[1],x10

	add	x6,x6,#0x20
	add	x0,x0,x6		// x0 is adjusted to the last data

	mov	x7,x3

	// The iv for third block
	extr	x22,x10,x10,#32
	extr	x10,x10,x9,#63
	and	w11,w19,w22,asr #31
	eor	x9,x11,x9,lsl #1
	fmov	d9,x9
	fmov	v9.d[1],x10

	aesd	v0.16b,v20.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	aesd	v0.16b,v21.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	aesd	v0.16b,v22.16b
	aesimc	v0.16b,v0.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	ld1	{v27.16b},[x0],#16
	aesd	v0.16b,v23.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	ld1	{v16.4s},[x7],#16		// re-pre-load rndkey[0]
	add	w6,w5,#2
	eor	v4.16b,v4.16b,v0.16b
	eor	v5.16b,v5.16b,v1.16b
	eor	v24.16b,v24.16b,v17.16b
	ld1	{v17.4s},[x7],#16		// re-pre-load rndkey[1]
	st1	{v4.16b},[x1],#16
	st1	{v5.16b},[x1],#16
	st1	{v24.16b},[x1],#16

	cmn	x2,#0x30
	add	x2,x2,#0x30
	b.eq	Lxts_done
	sub	x2,x2,#0x30
	orr	v28.16b,v3.16b,v3.16b
	orr	v29.16b,v27.16b,v27.16b
	nop

Lxts_inner_dec_tail:
	// x2 == -0x10 means two blocks left.
	cmn	x2,#0x10
	eor	v1.16b,v28.16b,v6.16b
	eor	v24.16b,v29.16b,v8.16b
	b.eq	Lxts_dec_tail_loop
	eor	v24.16b,v29.16b,v6.16b
Lxts_dec_tail_loop:
	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	ld1	{v16.4s},[x7],#16
	subs	w6,w6,#2
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	ld1	{v17.4s},[x7],#16
	b.gt	Lxts_dec_tail_loop

	aesd	v1.16b,v16.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v16.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v17.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v17.16b
	aesimc	v24.16b,v24.16b
	aesd	v1.16b,v20.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v20.16b
	aesimc	v24.16b,v24.16b
	cmn	x2,#0x20
	aesd	v1.16b,v21.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v21.16b
	aesimc	v24.16b,v24.16b
	eor	v5.16b,v6.16b,v7.16b
	aesd	v1.16b,v22.16b
	aesimc	v1.16b,v1.16b
	aesd	v24.16b,v22.16b
	aesimc	v24.16b,v24.16b
	eor	v17.16b,v8.16b,v7.16b
	aesd	v1.16b,v23.16b
	aesd	v24.16b,v23.16b
	b.eq	Lxts_dec_one
	eor	v5.16b,v5.16b,v1.16b
	eor	v17.16b,v17.16b,v24.16b
	orr	v6.16b,v9.16b,v9.16b
	orr	v8.16b,v10.16b,v10.16b
	st1	{v5.16b},[x1],#16
	st1	{v17.16b},[x1],#16
	add	x2,x2,#16
	b	Lxts_done

Lxts_dec_one:
	eor	v5.16b,v5.16b,v24.16b
	orr	v6.16b,v8.16b,v8.16b
	orr	v8.16b,v9.16b,v9.16b
	st1	{v5.16b},[x1],#16
	add	x2,x2,#32

Lxts_done:
	tst	x21,#0xf
	b.eq	Lxts_dec_abort
	// Processing the last two blocks with cipher stealing.
	mov	x7,x3
	cbnz	x2,Lxts_dec_1st_done
	ld1	{v0.16b},[x0],#16

	// Decrypt the last second block to get the last plain text block
Lxts_dec_1st_done:
	eor	v26.16b,v0.16b,v8.16b
	ldr	w6,[x3,#240]
	ld1	{v0.4s},[x3],#16
	sub	w6,w6,#2
	ld1	{v1.4s},[x3],#16
Loop_final_2nd_dec:
	aesd	v26.16b,v0.16b
	aesimc	v26.16b,v26.16b
	ld1	{v0.4s},[x3],#16		// load key schedule...
	subs	w6,w6,#2
	aesd	v26.16b,v1.16b
	aesimc	v26.16b,v26.16b
	ld1	{v1.4s},[x3],#16		// load key schedule...
	b.gt	Loop_final_2nd_dec

	aesd	v26.16b,v0.16b
	aesimc	v26.16b,v26.16b
	ld1	{v0.4s},[x3]
	aesd	v26.16b,v1.16b
	eor	v26.16b,v26.16b,v0.16b
	eor	v26.16b,v26.16b,v8.16b
	st1	{v26.16b},[x1]

	mov	x20,x0
	add	x13,x1,#16

	// Composite the tailcnt "16 byte not aligned block" into the last second plain blocks
	// to get the last encrypted block.
.composite_dec_loop:
	subs	x21,x21,#1
	ldrb	w15,[x1,x21]
	ldrb	w14,[x20,x21]
	strb	w15,[x13,x21]
	strb	w14,[x1,x21]
	b.gt	.composite_dec_loop
Lxts_dec_load_done:
	ld1	{v26.16b},[x1]
	eor	v26.16b,v26.16b,v6.16b

	// Decrypt the composite block to get the last second plain text block
	ldr	w6,[x7,#240]
	ld1	{v0.4s},[x7],#16
	sub	w6,w6,#2
	ld1	{v1.4s},[x7],#16
Loop_final_dec:
	aesd	v26.16b,v0.16b
	aesimc	v26.16b,v26.16b
	ld1	{v0.4s},[x7],#16		// load key schedule...
	subs	w6,w6,#2
	aesd	v26.16b,v1.16b
	aesimc	v26.16b,v26.16b
	ld1	{v1.4s},[x7],#16		// load key schedule...
	b.gt	Loop_final_dec

	aesd	v26.16b,v0.16b
	aesimc	v26.16b,v26.16b
	ld1	{v0.4s},[x7]
	aesd	v26.16b,v1.16b
	eor	v26.16b,v26.16b,v0.16b
	eor	v26.16b,v26.16b,v6.16b
	st1	{v26.16b},[x1]

Lxts_dec_abort:
	ldp	x21,x22,[sp,#48]
	ldp	d8,d9,[sp,#32]
	ldp	d10,d11,[sp,#16]
	ldp	x19,x20,[sp],#64

Lxts_dec_final_abort:
	ret

#endif
