#include "arm_arch.h"
#ifndef	__KERNEL__

.hidden	OPENSSL_armcap_P


#endif

.section	.rodata

.align	5
.Lsigma:
.quad	0x3320646e61707865,0x6b20657479622d32		// endian-neutral
.Lone:
.long	1,2,3,4
.Lrot24:
.long	0x02010003,0x06050407,0x0a09080b,0x0e0d0c0f
.byte	67,104,97,67,104,97,50,48,32,102,111,114,32,65,82,77,118,56,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,64,100,111,116,45,97,115,109,0
.align	2

.text

.globl	ChaCha20_ctr32_dflt
.type	ChaCha20_ctr32_dflt,%function
.align	5
ChaCha20_ctr32_dflt:
	AARCH64_SIGN_LINK_REGISTER
	cmp	x2,#192
	b.lo	.Lshort
#ifndef	__KERNEL__
	adrp	x17,OPENSSL_armcap_P
	ldr	w17,[x17,#:lo12:OPENSSL_armcap_P]
.Lcheck_neon:
	tst	w17,#ARMV7_NEON
	b.ne	.LChaCha20_neon
#endif

.Lshort:
	stp	x29,x30,[sp,#-96]!
	add	x29,sp,#0

	adrp	x5,.Lsigma
	add	x5,x5,#:lo12:.Lsigma
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]
	stp	x27,x28,[sp,#80]
	sub	sp,sp,#64

	ldp	x22,x23,[x5]		// load sigma
	ldp	x24,x25,[x3]		// load key
	ldp	x26,x27,[x3,#16]
	ldp	x28,x30,[x4]		// load counter
#ifdef	__AARCH64EB__
	ror	x24,x24,#32
	ror	x25,x25,#32
	ror	x26,x26,#32
	ror	x27,x27,#32
	ror	x28,x28,#32
	ror	x30,x30,#32
#endif

.Loop_outer:
	mov	w5,w22			// unpack key block
	lsr	x6,x22,#32
	mov	w7,w23
	lsr	x8,x23,#32
	mov	w9,w24
	lsr	x10,x24,#32
	mov	w11,w25
	lsr	x12,x25,#32
	mov	w13,w26
	lsr	x14,x26,#32
	mov	w15,w27
	lsr	x16,x27,#32
	mov	w17,w28
	lsr	x19,x28,#32
	mov	w20,w30
	lsr	x21,x30,#32

	mov	x4,#10
	subs	x2,x2,#64
.Loop:
	sub	x4,x4,#1
	add	w5,w5,w9
	add	w6,w6,w10
	add	w7,w7,w11
	add	w8,w8,w12
	eor	w17,w17,w5
	eor	w19,w19,w6
	eor	w20,w20,w7
	eor	w21,w21,w8
	ror	w17,w17,#16
	ror	w19,w19,#16
	ror	w20,w20,#16
	ror	w21,w21,#16
	add	w13,w13,w17
	add	w14,w14,w19
	add	w15,w15,w20
	add	w16,w16,w21
	eor	w9,w9,w13
	eor	w10,w10,w14
	eor	w11,w11,w15
	eor	w12,w12,w16
	ror	w9,w9,#20
	ror	w10,w10,#20
	ror	w11,w11,#20
	ror	w12,w12,#20
	add	w5,w5,w9
	add	w6,w6,w10
	add	w7,w7,w11
	add	w8,w8,w12
	eor	w17,w17,w5
	eor	w19,w19,w6
	eor	w20,w20,w7
	eor	w21,w21,w8
	ror	w17,w17,#24
	ror	w19,w19,#24
	ror	w20,w20,#24
	ror	w21,w21,#24
	add	w13,w13,w17
	add	w14,w14,w19
	add	w15,w15,w20
	add	w16,w16,w21
	eor	w9,w9,w13
	eor	w10,w10,w14
	eor	w11,w11,w15
	eor	w12,w12,w16
	ror	w9,w9,#25
	ror	w10,w10,#25
	ror	w11,w11,#25
	ror	w12,w12,#25
	add	w5,w5,w10
	add	w6,w6,w11
	add	w7,w7,w12
	add	w8,w8,w9
	eor	w21,w21,w5
	eor	w17,w17,w6
	eor	w19,w19,w7
	eor	w20,w20,w8
	ror	w21,w21,#16
	ror	w17,w17,#16
	ror	w19,w19,#16
	ror	w20,w20,#16
	add	w15,w15,w21
	add	w16,w16,w17
	add	w13,w13,w19
	add	w14,w14,w20
	eor	w10,w10,w15
	eor	w11,w11,w16
	eor	w12,w12,w13
	eor	w9,w9,w14
	ror	w10,w10,#20
	ror	w11,w11,#20
	ror	w12,w12,#20
	ror	w9,w9,#20
	add	w5,w5,w10
	add	w6,w6,w11
	add	w7,w7,w12
	add	w8,w8,w9
	eor	w21,w21,w5
	eor	w17,w17,w6
	eor	w19,w19,w7
	eor	w20,w20,w8
	ror	w21,w21,#24
	ror	w17,w17,#24
	ror	w19,w19,#24
	ror	w20,w20,#24
	add	w15,w15,w21
	add	w16,w16,w17
	add	w13,w13,w19
	add	w14,w14,w20
	eor	w10,w10,w15
	eor	w11,w11,w16
	eor	w12,w12,w13
	eor	w9,w9,w14
	ror	w10,w10,#25
	ror	w11,w11,#25
	ror	w12,w12,#25
	ror	w9,w9,#25
	cbnz	x4,.Loop

	add	w5,w5,w22		// accumulate key block
	add	x6,x6,x22,lsr#32
	add	w7,w7,w23
	add	x8,x8,x23,lsr#32
	add	w9,w9,w24
	add	x10,x10,x24,lsr#32
	add	w11,w11,w25
	add	x12,x12,x25,lsr#32
	add	w13,w13,w26
	add	x14,x14,x26,lsr#32
	add	w15,w15,w27
	add	x16,x16,x27,lsr#32
	add	w17,w17,w28
	add	x19,x19,x28,lsr#32
	add	w20,w20,w30
	add	x21,x21,x30,lsr#32

	b.lo	.Ltail

	add	x5,x5,x6,lsl#32	// pack
	add	x7,x7,x8,lsl#32
	ldp	x6,x8,[x1,#0]		// load input
	add	x9,x9,x10,lsl#32
	add	x11,x11,x12,lsl#32
	ldp	x10,x12,[x1,#16]
	add	x13,x13,x14,lsl#32
	add	x15,x15,x16,lsl#32
	ldp	x14,x16,[x1,#32]
	add	x17,x17,x19,lsl#32
	add	x20,x20,x21,lsl#32
	ldp	x19,x21,[x1,#48]
	add	x1,x1,#64
#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	eor	x5,x5,x6
	eor	x7,x7,x8
	eor	x9,x9,x10
	eor	x11,x11,x12
	eor	x13,x13,x14
	eor	x15,x15,x16
	eor	x17,x17,x19
	eor	x20,x20,x21

	stp	x5,x7,[x0,#0]		// store output
	add	x28,x28,#1			// increment counter
	stp	x9,x11,[x0,#16]
	stp	x13,x15,[x0,#32]
	stp	x17,x20,[x0,#48]
	add	x0,x0,#64

	b.hi	.Loop_outer

	ldp	x19,x20,[x29,#16]
	add	sp,sp,#64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#96
.Labort:
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.align	4
.Ltail:
	add	x2,x2,#64
.Less_than_64:
	sub	x0,x0,#1
	add	x1,x1,x2
	add	x0,x0,x2
	add	x4,sp,x2
	neg	x2,x2

	add	x5,x5,x6,lsl#32	// pack
	add	x7,x7,x8,lsl#32
	add	x9,x9,x10,lsl#32
	add	x11,x11,x12,lsl#32
	add	x13,x13,x14,lsl#32
	add	x15,x15,x16,lsl#32
	add	x17,x17,x19,lsl#32
	add	x20,x20,x21,lsl#32
#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	stp	x5,x7,[sp,#0]
	stp	x9,x11,[sp,#16]
	stp	x13,x15,[sp,#32]
	stp	x17,x20,[sp,#48]

.Loop_tail:
	ldrb	w10,[x1,x2]
	ldrb	w11,[x4,x2]
	add	x2,x2,#1
	eor	w10,w10,w11
	strb	w10,[x0,x2]
	cbnz	x2,.Loop_tail

	stp	xzr,xzr,[sp,#0]
	stp	xzr,xzr,[sp,#16]
	stp	xzr,xzr,[sp,#32]
	stp	xzr,xzr,[sp,#48]

	ldp	x19,x20,[x29,#16]
	add	sp,sp,#64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#96
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ChaCha20_ctr32_dflt,.-ChaCha20_ctr32_dflt

.globl	ChaCha20_ctr32
.type	ChaCha20_ctr32,%function
.align	5
ChaCha20_ctr32:
	AARCH64_SIGN_LINK_REGISTER
	cbz	x2,.Labort
	cmp	x2,#192
	b.lo	.Lshort
#ifndef	__KERNEL__
	adrp	x17,OPENSSL_armcap_P
	ldr	w17,[x17,#:lo12:OPENSSL_armcap_P]
	tst	w17,#ARMV8_SVE
	b.eq	.Lcheck_neon
	stp	x29,x30,[sp,#-16]!
	sub	sp,sp,#16
	// SVE handling will inevitably increment the counter
	// Neon/Scalar code that follows to process tail data needs to
	// use new counter, unfortunately the input counter buffer
	// pointed to by ctr is meant to be read-only per API contract
	// we have to copy the buffer to stack to be writable by SVE
	ldp	x5,x6,[x4]
	stp	x5,x6,[sp]
	mov	x4,sp
	bl	ChaCha20_ctr32_sve
	cbz	x2,1f
	bl	ChaCha20_ctr32_dflt
1:
	add	sp,sp,#16
	ldp	x29,x30,[sp],#16
	AARCH64_VALIDATE_LINK_REGISTER
	ret
#endif
	b	.Lshort
.size	ChaCha20_ctr32,.-ChaCha20_ctr32

#ifdef	__KERNEL__
.globl	ChaCha20_neon
#endif
.type	ChaCha20_neon,%function
.align	5
ChaCha20_neon:
	AARCH64_SIGN_LINK_REGISTER
.LChaCha20_neon:
	stp	x29,x30,[sp,#-96]!
	add	x29,sp,#0

	adrp	x5,.Lsigma
	add	x5,x5,#:lo12:.Lsigma
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]
	stp	x27,x28,[sp,#80]
	cmp	x2,#512
	b.hs	.L512_or_more_neon

	sub	sp,sp,#64

	ldp	x22,x23,[x5]		// load sigma
	ld1	{v0.4s},[x5],#16
	ldp	x24,x25,[x3]		// load key
	ldp	x26,x27,[x3,#16]
	ld1	{v1.4s,v2.4s},[x3]
	ldp	x28,x30,[x4]		// load counter
	ld1	{v3.4s},[x4]
	stp	d8,d9,[sp]			// meet ABI requirements
	ld1	{v8.4s,v9.4s},[x5]
#ifdef	__AARCH64EB__
	rev64	v0.4s,v0.4s
	ror	x24,x24,#32
	ror	x25,x25,#32
	ror	x26,x26,#32
	ror	x27,x27,#32
	ror	x28,x28,#32
	ror	x30,x30,#32
#endif

.Loop_outer_neon:
	dup	v16.4s,v0.s[0]			// unpack key block
	mov	w5,w22
	dup	v20.4s,v0.s[1]
	lsr	x6,x22,#32
	dup	v24.4s,v0.s[2]
	mov	w7,w23
	dup	v28.4s,v0.s[3]
	lsr	x8,x23,#32
	dup	v17.4s,v1.s[0]
	mov	w9,w24
	dup	v21.4s,v1.s[1]
	lsr	x10,x24,#32
	dup	v25.4s,v1.s[2]
	mov	w11,w25
	dup	v29.4s,v1.s[3]
	lsr	x12,x25,#32
	dup	v19.4s,v3.s[0]
	mov	w13,w26
	dup	v23.4s,v3.s[1]
	lsr	x14,x26,#32
	dup	v27.4s,v3.s[2]
	mov	w15,w27
	dup	v31.4s,v3.s[3]
	lsr	x16,x27,#32
	add	v19.4s,v19.4s,v8.4s
	mov	w17,w28
	dup	v18.4s,v2.s[0]
	lsr	x19,x28,#32
	dup	v22.4s,v2.s[1]
	mov	w20,w30
	dup	v26.4s,v2.s[2]
	lsr	x21,x30,#32
	dup	v30.4s,v2.s[3]

	mov	x4,#10
	subs	x2,x2,#320
.Loop_neon:
	sub	x4,x4,#1
	add	v16.4s,v16.4s,v17.4s
	add	w5,w5,w9
	add	v20.4s,v20.4s,v21.4s
	add	w6,w6,w10
	add	v24.4s,v24.4s,v25.4s
	add	w7,w7,w11
	add	v28.4s,v28.4s,v29.4s
	add	w8,w8,w12
	eor	v19.16b,v19.16b,v16.16b
	eor	w17,w17,w5
	eor	v23.16b,v23.16b,v20.16b
	eor	w19,w19,w6
	eor	v27.16b,v27.16b,v24.16b
	eor	w20,w20,w7
	eor	v31.16b,v31.16b,v28.16b
	eor	w21,w21,w8
	rev32	v19.8h,v19.8h
	ror	w17,w17,#16
	rev32	v23.8h,v23.8h
	ror	w19,w19,#16
	rev32	v27.8h,v27.8h
	ror	w20,w20,#16
	rev32	v31.8h,v31.8h
	ror	w21,w21,#16
	add	v18.4s,v18.4s,v19.4s
	add	w13,w13,w17
	add	v22.4s,v22.4s,v23.4s
	add	w14,w14,w19
	add	v26.4s,v26.4s,v27.4s
	add	w15,w15,w20
	add	v30.4s,v30.4s,v31.4s
	add	w16,w16,w21
	eor	v4.16b,v17.16b,v18.16b
	eor	w9,w9,w13
	eor	v5.16b,v21.16b,v22.16b
	eor	w10,w10,w14
	eor	v6.16b,v25.16b,v26.16b
	eor	w11,w11,w15
	eor	v7.16b,v29.16b,v30.16b
	eor	w12,w12,w16
	ushr	v17.4s,v4.4s,#20
	ror	w9,w9,#20
	ushr	v21.4s,v5.4s,#20
	ror	w10,w10,#20
	ushr	v25.4s,v6.4s,#20
	ror	w11,w11,#20
	ushr	v29.4s,v7.4s,#20
	ror	w12,w12,#20
	sli	v17.4s,v4.4s,#12
	add	w5,w5,w9
	sli	v21.4s,v5.4s,#12
	add	w6,w6,w10
	sli	v25.4s,v6.4s,#12
	add	w7,w7,w11
	sli	v29.4s,v7.4s,#12
	add	w8,w8,w12
	add	v16.4s,v16.4s,v17.4s
	eor	w17,w17,w5
	add	v20.4s,v20.4s,v21.4s
	eor	w19,w19,w6
	add	v24.4s,v24.4s,v25.4s
	eor	w20,w20,w7
	add	v28.4s,v28.4s,v29.4s
	eor	w21,w21,w8
	eor	v4.16b,v19.16b,v16.16b
	ror	w17,w17,#24
	eor	v5.16b,v23.16b,v20.16b
	ror	w19,w19,#24
	eor	v6.16b,v27.16b,v24.16b
	ror	w20,w20,#24
	eor	v7.16b,v31.16b,v28.16b
	ror	w21,w21,#24
	tbl	v19.16b,{v4.16b},v9.16b
	add	w13,w13,w17
	tbl	v23.16b,{v5.16b},v9.16b
	add	w14,w14,w19
	tbl	v27.16b,{v6.16b},v9.16b
	add	w15,w15,w20
	tbl	v31.16b,{v7.16b},v9.16b
	add	w16,w16,w21
	add	v18.4s,v18.4s,v19.4s
	eor	w9,w9,w13
	add	v22.4s,v22.4s,v23.4s
	eor	w10,w10,w14
	add	v26.4s,v26.4s,v27.4s
	eor	w11,w11,w15
	add	v30.4s,v30.4s,v31.4s
	eor	w12,w12,w16
	eor	v4.16b,v17.16b,v18.16b
	ror	w9,w9,#25
	eor	v5.16b,v21.16b,v22.16b
	ror	w10,w10,#25
	eor	v6.16b,v25.16b,v26.16b
	ror	w11,w11,#25
	eor	v7.16b,v29.16b,v30.16b
	ror	w12,w12,#25
	ushr	v17.4s,v4.4s,#25
	ushr	v21.4s,v5.4s,#25
	ushr	v25.4s,v6.4s,#25
	ushr	v29.4s,v7.4s,#25
	sli	v17.4s,v4.4s,#7
	sli	v21.4s,v5.4s,#7
	sli	v25.4s,v6.4s,#7
	sli	v29.4s,v7.4s,#7
	add	v16.4s,v16.4s,v21.4s
	add	w5,w5,w10
	add	v20.4s,v20.4s,v25.4s
	add	w6,w6,w11
	add	v24.4s,v24.4s,v29.4s
	add	w7,w7,w12
	add	v28.4s,v28.4s,v17.4s
	add	w8,w8,w9
	eor	v31.16b,v31.16b,v16.16b
	eor	w21,w21,w5
	eor	v19.16b,v19.16b,v20.16b
	eor	w17,w17,w6
	eor	v23.16b,v23.16b,v24.16b
	eor	w19,w19,w7
	eor	v27.16b,v27.16b,v28.16b
	eor	w20,w20,w8
	rev32	v31.8h,v31.8h
	ror	w21,w21,#16
	rev32	v19.8h,v19.8h
	ror	w17,w17,#16
	rev32	v23.8h,v23.8h
	ror	w19,w19,#16
	rev32	v27.8h,v27.8h
	ror	w20,w20,#16
	add	v26.4s,v26.4s,v31.4s
	add	w15,w15,w21
	add	v30.4s,v30.4s,v19.4s
	add	w16,w16,w17
	add	v18.4s,v18.4s,v23.4s
	add	w13,w13,w19
	add	v22.4s,v22.4s,v27.4s
	add	w14,w14,w20
	eor	v4.16b,v21.16b,v26.16b
	eor	w10,w10,w15
	eor	v5.16b,v25.16b,v30.16b
	eor	w11,w11,w16
	eor	v6.16b,v29.16b,v18.16b
	eor	w12,w12,w13
	eor	v7.16b,v17.16b,v22.16b
	eor	w9,w9,w14
	ushr	v21.4s,v4.4s,#20
	ror	w10,w10,#20
	ushr	v25.4s,v5.4s,#20
	ror	w11,w11,#20
	ushr	v29.4s,v6.4s,#20
	ror	w12,w12,#20
	ushr	v17.4s,v7.4s,#20
	ror	w9,w9,#20
	sli	v21.4s,v4.4s,#12
	add	w5,w5,w10
	sli	v25.4s,v5.4s,#12
	add	w6,w6,w11
	sli	v29.4s,v6.4s,#12
	add	w7,w7,w12
	sli	v17.4s,v7.4s,#12
	add	w8,w8,w9
	add	v16.4s,v16.4s,v21.4s
	eor	w21,w21,w5
	add	v20.4s,v20.4s,v25.4s
	eor	w17,w17,w6
	add	v24.4s,v24.4s,v29.4s
	eor	w19,w19,w7
	add	v28.4s,v28.4s,v17.4s
	eor	w20,w20,w8
	eor	v4.16b,v31.16b,v16.16b
	ror	w21,w21,#24
	eor	v5.16b,v19.16b,v20.16b
	ror	w17,w17,#24
	eor	v6.16b,v23.16b,v24.16b
	ror	w19,w19,#24
	eor	v7.16b,v27.16b,v28.16b
	ror	w20,w20,#24
	tbl	v31.16b,{v4.16b},v9.16b
	add	w15,w15,w21
	tbl	v19.16b,{v5.16b},v9.16b
	add	w16,w16,w17
	tbl	v23.16b,{v6.16b},v9.16b
	add	w13,w13,w19
	tbl	v27.16b,{v7.16b},v9.16b
	add	w14,w14,w20
	add	v26.4s,v26.4s,v31.4s
	eor	w10,w10,w15
	add	v30.4s,v30.4s,v19.4s
	eor	w11,w11,w16
	add	v18.4s,v18.4s,v23.4s
	eor	w12,w12,w13
	add	v22.4s,v22.4s,v27.4s
	eor	w9,w9,w14
	eor	v4.16b,v21.16b,v26.16b
	ror	w10,w10,#25
	eor	v5.16b,v25.16b,v30.16b
	ror	w11,w11,#25
	eor	v6.16b,v29.16b,v18.16b
	ror	w12,w12,#25
	eor	v7.16b,v17.16b,v22.16b
	ror	w9,w9,#25
	ushr	v21.4s,v4.4s,#25
	ushr	v25.4s,v5.4s,#25
	ushr	v29.4s,v6.4s,#25
	ushr	v17.4s,v7.4s,#25
	sli	v21.4s,v4.4s,#7
	sli	v25.4s,v5.4s,#7
	sli	v29.4s,v6.4s,#7
	sli	v17.4s,v7.4s,#7
	cbnz	x4,.Loop_neon

	add	v19.4s,v19.4s,v8.4s

	zip1	v4.4s,v16.4s,v20.4s			// transpose data
	zip1	v5.4s,v24.4s,v28.4s
	zip2	v6.4s,v16.4s,v20.4s
	zip2	v7.4s,v24.4s,v28.4s
	zip1	v16.2d,v4.2d,v5.2d
	zip2	v20.2d,v4.2d,v5.2d
	zip1	v24.2d,v6.2d,v7.2d
	zip2	v28.2d,v6.2d,v7.2d

	zip1	v4.4s,v17.4s,v21.4s
	zip1	v5.4s,v25.4s,v29.4s
	zip2	v6.4s,v17.4s,v21.4s
	zip2	v7.4s,v25.4s,v29.4s
	zip1	v17.2d,v4.2d,v5.2d
	zip2	v21.2d,v4.2d,v5.2d
	zip1	v25.2d,v6.2d,v7.2d
	zip2	v29.2d,v6.2d,v7.2d

	zip1	v4.4s,v18.4s,v22.4s
	add	w5,w5,w22		// accumulate key block
	zip1	v5.4s,v26.4s,v30.4s
	add	x6,x6,x22,lsr#32
	zip2	v6.4s,v18.4s,v22.4s
	add	w7,w7,w23
	zip2	v7.4s,v26.4s,v30.4s
	add	x8,x8,x23,lsr#32
	zip1	v18.2d,v4.2d,v5.2d
	add	w9,w9,w24
	zip2	v22.2d,v4.2d,v5.2d
	add	x10,x10,x24,lsr#32
	zip1	v26.2d,v6.2d,v7.2d
	add	w11,w11,w25
	zip2	v30.2d,v6.2d,v7.2d
	add	x12,x12,x25,lsr#32

	zip1	v4.4s,v19.4s,v23.4s
	add	w13,w13,w26
	zip1	v5.4s,v27.4s,v31.4s
	add	x14,x14,x26,lsr#32
	zip2	v6.4s,v19.4s,v23.4s
	add	w15,w15,w27
	zip2	v7.4s,v27.4s,v31.4s
	add	x16,x16,x27,lsr#32
	zip1	v19.2d,v4.2d,v5.2d
	add	w17,w17,w28
	zip2	v23.2d,v4.2d,v5.2d
	add	x19,x19,x28,lsr#32
	zip1	v27.2d,v6.2d,v7.2d
	add	w20,w20,w30
	zip2	v31.2d,v6.2d,v7.2d
	add	x21,x21,x30,lsr#32

	b.lo	.Ltail_neon

	add	x5,x5,x6,lsl#32	// pack
	add	x7,x7,x8,lsl#32
	ldp	x6,x8,[x1,#0]		// load input
	add	v16.4s,v16.4s,v0.4s			// accumulate key block
	add	x9,x9,x10,lsl#32
	add	x11,x11,x12,lsl#32
	ldp	x10,x12,[x1,#16]
	add	v17.4s,v17.4s,v1.4s
	add	x13,x13,x14,lsl#32
	add	x15,x15,x16,lsl#32
	ldp	x14,x16,[x1,#32]
	add	v18.4s,v18.4s,v2.4s
	add	x17,x17,x19,lsl#32
	add	x20,x20,x21,lsl#32
	ldp	x19,x21,[x1,#48]
	add	v19.4s,v19.4s,v3.4s
	add	x1,x1,#64
#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	eor	x5,x5,x6
	add	v20.4s,v20.4s,v0.4s
	eor	x7,x7,x8
	add	v21.4s,v21.4s,v1.4s
	eor	x9,x9,x10
	add	v22.4s,v22.4s,v2.4s
	eor	x11,x11,x12
	add	v23.4s,v23.4s,v3.4s
	eor	x13,x13,x14
	eor	v16.16b,v16.16b,v4.16b
	movi	v4.4s,#5
	eor	x15,x15,x16
	eor	v17.16b,v17.16b,v5.16b
	eor	x17,x17,x19
	eor	v18.16b,v18.16b,v6.16b
	eor	x20,x20,x21
	eor	v19.16b,v19.16b,v7.16b
	add	v8.4s,v8.4s,v4.4s			// += 5
	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64

	stp	x5,x7,[x0,#0]		// store output
	add	x28,x28,#5			// increment counter
	stp	x9,x11,[x0,#16]
	stp	x13,x15,[x0,#32]
	stp	x17,x20,[x0,#48]
	add	x0,x0,#64

	st1	{v16.16b,v17.16b,v18.16b,v19.16b},[x0],#64
	add	v24.4s,v24.4s,v0.4s
	add	v25.4s,v25.4s,v1.4s
	add	v26.4s,v26.4s,v2.4s
	add	v27.4s,v27.4s,v3.4s
	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x1],#64

	eor	v20.16b,v20.16b,v4.16b
	eor	v21.16b,v21.16b,v5.16b
	eor	v22.16b,v22.16b,v6.16b
	eor	v23.16b,v23.16b,v7.16b
	st1	{v20.16b,v21.16b,v22.16b,v23.16b},[x0],#64
	add	v28.4s,v28.4s,v0.4s
	add	v29.4s,v29.4s,v1.4s
	add	v30.4s,v30.4s,v2.4s
	add	v31.4s,v31.4s,v3.4s
	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x1],#64

	eor	v24.16b,v24.16b,v16.16b
	eor	v25.16b,v25.16b,v17.16b
	eor	v26.16b,v26.16b,v18.16b
	eor	v27.16b,v27.16b,v19.16b
	st1	{v24.16b,v25.16b,v26.16b,v27.16b},[x0],#64

	eor	v28.16b,v28.16b,v20.16b
	eor	v29.16b,v29.16b,v21.16b
	eor	v30.16b,v30.16b,v22.16b
	eor	v31.16b,v31.16b,v23.16b
	st1	{v28.16b,v29.16b,v30.16b,v31.16b},[x0],#64

	b.hi	.Loop_outer_neon

	ldp	d8,d9,[sp]			// meet ABI requirements

	ldp	x19,x20,[x29,#16]
	add	sp,sp,#64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#96
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.align	4
.Ltail_neon:
	add	x2,x2,#320
	ldp	d8,d9,[sp]			// meet ABI requirements
	cmp	x2,#64
	b.lo	.Less_than_64

	add	x5,x5,x6,lsl#32	// pack
	add	x7,x7,x8,lsl#32
	ldp	x6,x8,[x1,#0]		// load input
	add	x9,x9,x10,lsl#32
	add	x11,x11,x12,lsl#32
	ldp	x10,x12,[x1,#16]
	add	x13,x13,x14,lsl#32
	add	x15,x15,x16,lsl#32
	ldp	x14,x16,[x1,#32]
	add	x17,x17,x19,lsl#32
	add	x20,x20,x21,lsl#32
	ldp	x19,x21,[x1,#48]
	add	x1,x1,#64
#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	eor	x5,x5,x6
	eor	x7,x7,x8
	eor	x9,x9,x10
	eor	x11,x11,x12
	eor	x13,x13,x14
	eor	x15,x15,x16
	eor	x17,x17,x19
	eor	x20,x20,x21

	stp	x5,x7,[x0,#0]		// store output
	add	v16.4s,v16.4s,v0.4s			// accumulate key block
	stp	x9,x11,[x0,#16]
	add	v17.4s,v17.4s,v1.4s
	stp	x13,x15,[x0,#32]
	add	v18.4s,v18.4s,v2.4s
	stp	x17,x20,[x0,#48]
	add	v19.4s,v19.4s,v3.4s
	add	x0,x0,#64
	b.eq	.Ldone_neon
	sub	x2,x2,#64
	cmp	x2,#64
	b.lo	.Last_neon

	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	eor	v16.16b,v16.16b,v4.16b
	eor	v17.16b,v17.16b,v5.16b
	eor	v18.16b,v18.16b,v6.16b
	eor	v19.16b,v19.16b,v7.16b
	st1	{v16.16b,v17.16b,v18.16b,v19.16b},[x0],#64
	b.eq	.Ldone_neon

	add	v16.4s,v20.4s,v0.4s
	add	v17.4s,v21.4s,v1.4s
	sub	x2,x2,#64
	add	v18.4s,v22.4s,v2.4s
	cmp	x2,#64
	add	v19.4s,v23.4s,v3.4s
	b.lo	.Last_neon

	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	eor	v20.16b,v16.16b,v4.16b
	eor	v21.16b,v17.16b,v5.16b
	eor	v22.16b,v18.16b,v6.16b
	eor	v23.16b,v19.16b,v7.16b
	st1	{v20.16b,v21.16b,v22.16b,v23.16b},[x0],#64
	b.eq	.Ldone_neon

	add	v16.4s,v24.4s,v0.4s
	add	v17.4s,v25.4s,v1.4s
	sub	x2,x2,#64
	add	v18.4s,v26.4s,v2.4s
	cmp	x2,#64
	add	v19.4s,v27.4s,v3.4s
	b.lo	.Last_neon

	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	eor	v24.16b,v16.16b,v4.16b
	eor	v25.16b,v17.16b,v5.16b
	eor	v26.16b,v18.16b,v6.16b
	eor	v27.16b,v19.16b,v7.16b
	st1	{v24.16b,v25.16b,v26.16b,v27.16b},[x0],#64
	b.eq	.Ldone_neon

	add	v16.4s,v28.4s,v0.4s
	add	v17.4s,v29.4s,v1.4s
	add	v18.4s,v30.4s,v2.4s
	add	v19.4s,v31.4s,v3.4s
	sub	x2,x2,#64

.Last_neon:
	st1	{v16.16b,v17.16b,v18.16b,v19.16b},[sp]

	sub	x0,x0,#1
	add	x1,x1,x2
	add	x0,x0,x2
	add	x4,sp,x2
	neg	x2,x2

.Loop_tail_neon:
	ldrb	w10,[x1,x2]
	ldrb	w11,[x4,x2]
	add	x2,x2,#1
	eor	w10,w10,w11
	strb	w10,[x0,x2]
	cbnz	x2,.Loop_tail_neon

	stp	xzr,xzr,[sp,#0]
	stp	xzr,xzr,[sp,#16]
	stp	xzr,xzr,[sp,#32]
	stp	xzr,xzr,[sp,#48]

.Ldone_neon:
	ldp	x19,x20,[x29,#16]
	add	sp,sp,#64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#96
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ChaCha20_neon,.-ChaCha20_neon
.type	ChaCha20_512_neon,%function
.align	5
ChaCha20_512_neon:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-96]!
	add	x29,sp,#0

	adrp	x5,.Lsigma
	add	x5,x5,#:lo12:.Lsigma
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]
	stp	x27,x28,[sp,#80]

.L512_or_more_neon:
	sub	sp,sp,#128+64

	eor	v7.16b,v7.16b,v7.16b
	ldp	x22,x23,[x5]		// load sigma
	ld1	{v0.4s},[x5],#16
	ldp	x24,x25,[x3]		// load key
	ldp	x26,x27,[x3,#16]
	ld1	{v1.4s,v2.4s},[x3]
	ldp	x28,x30,[x4]		// load counter
	ld1	{v3.4s},[x4]
	ld1	{v7.s}[0],[x5]
	add	x3,x5,#16			// .Lrot24
#ifdef	__AARCH64EB__
	rev64	v0.4s,v0.4s
	ror	x24,x24,#32
	ror	x25,x25,#32
	ror	x26,x26,#32
	ror	x27,x27,#32
	ror	x28,x28,#32
	ror	x30,x30,#32
#endif
	add	v3.4s,v3.4s,v7.4s		// += 1
	stp	q0,q1,[sp,#0]		// off-load key block, invariant part
	add	v3.4s,v3.4s,v7.4s		// not typo
	str	q2,[sp,#32]
	add	v4.4s,v3.4s,v7.4s
	add	v5.4s,v4.4s,v7.4s
	add	v6.4s,v5.4s,v7.4s
	shl	v7.4s,v7.4s,#2			// 1 -> 4

	stp	d8,d9,[sp,#128+0]		// meet ABI requirements
	stp	d10,d11,[sp,#128+16]
	stp	d12,d13,[sp,#128+32]
	stp	d14,d15,[sp,#128+48]

	sub	x2,x2,#512			// not typo

.Loop_outer_512_neon:
	mov	v8.16b,v0.16b
	mov	v12.16b,v0.16b
	mov	v16.16b,v0.16b
	mov	v20.16b,v0.16b
	mov	v24.16b,v0.16b
	mov	v28.16b,v0.16b
	mov	v9.16b,v1.16b
	mov	w5,w22			// unpack key block
	mov	v13.16b,v1.16b
	lsr	x6,x22,#32
	mov	v17.16b,v1.16b
	mov	w7,w23
	mov	v21.16b,v1.16b
	lsr	x8,x23,#32
	mov	v25.16b,v1.16b
	mov	w9,w24
	mov	v29.16b,v1.16b
	lsr	x10,x24,#32
	mov	v11.16b,v3.16b
	mov	w11,w25
	mov	v15.16b,v4.16b
	lsr	x12,x25,#32
	mov	v19.16b,v5.16b
	mov	w13,w26
	mov	v23.16b,v6.16b
	lsr	x14,x26,#32
	mov	v10.16b,v2.16b
	mov	w15,w27
	mov	v14.16b,v2.16b
	lsr	x16,x27,#32
	add	v27.4s,v11.4s,v7.4s			// +4
	mov	w17,w28
	add	v31.4s,v15.4s,v7.4s			// +4
	lsr	x19,x28,#32
	mov	v18.16b,v2.16b
	mov	w20,w30
	mov	v22.16b,v2.16b
	lsr	x21,x30,#32
	mov	v26.16b,v2.16b
	stp	q3,q4,[sp,#48]		// off-load key block, variable part
	mov	v30.16b,v2.16b
	stp	q5,q6,[sp,#80]

	mov	x4,#5
	ld1	{v6.4s},[x3]
	subs	x2,x2,#512
.Loop_upper_neon:
	sub	x4,x4,#1
	add	v8.4s,v8.4s,v9.4s
	add	w5,w5,w9
	add	v12.4s,v12.4s,v13.4s
	add	w6,w6,w10
	add	v16.4s,v16.4s,v17.4s
	add	w7,w7,w11
	add	v20.4s,v20.4s,v21.4s
	add	w8,w8,w12
	add	v24.4s,v24.4s,v25.4s
	eor	w17,w17,w5
	add	v28.4s,v28.4s,v29.4s
	eor	w19,w19,w6
	eor	v11.16b,v11.16b,v8.16b
	eor	w20,w20,w7
	eor	v15.16b,v15.16b,v12.16b
	eor	w21,w21,w8
	eor	v19.16b,v19.16b,v16.16b
	ror	w17,w17,#16
	eor	v23.16b,v23.16b,v20.16b
	ror	w19,w19,#16
	eor	v27.16b,v27.16b,v24.16b
	ror	w20,w20,#16
	eor	v31.16b,v31.16b,v28.16b
	ror	w21,w21,#16
	rev32	v11.8h,v11.8h
	add	w13,w13,w17
	rev32	v15.8h,v15.8h
	add	w14,w14,w19
	rev32	v19.8h,v19.8h
	add	w15,w15,w20
	rev32	v23.8h,v23.8h
	add	w16,w16,w21
	rev32	v27.8h,v27.8h
	eor	w9,w9,w13
	rev32	v31.8h,v31.8h
	eor	w10,w10,w14
	add	v10.4s,v10.4s,v11.4s
	eor	w11,w11,w15
	add	v14.4s,v14.4s,v15.4s
	eor	w12,w12,w16
	add	v18.4s,v18.4s,v19.4s
	ror	w9,w9,#20
	add	v22.4s,v22.4s,v23.4s
	ror	w10,w10,#20
	add	v26.4s,v26.4s,v27.4s
	ror	w11,w11,#20
	add	v30.4s,v30.4s,v31.4s
	ror	w12,w12,#20
	eor	v0.16b,v9.16b,v10.16b
	add	w5,w5,w9
	eor	v1.16b,v13.16b,v14.16b
	add	w6,w6,w10
	eor	v2.16b,v17.16b,v18.16b
	add	w7,w7,w11
	eor	v3.16b,v21.16b,v22.16b
	add	w8,w8,w12
	eor	v4.16b,v25.16b,v26.16b
	eor	w17,w17,w5
	eor	v5.16b,v29.16b,v30.16b
	eor	w19,w19,w6
	ushr	v9.4s,v0.4s,#20
	eor	w20,w20,w7
	ushr	v13.4s,v1.4s,#20
	eor	w21,w21,w8
	ushr	v17.4s,v2.4s,#20
	ror	w17,w17,#24
	ushr	v21.4s,v3.4s,#20
	ror	w19,w19,#24
	ushr	v25.4s,v4.4s,#20
	ror	w20,w20,#24
	ushr	v29.4s,v5.4s,#20
	ror	w21,w21,#24
	sli	v9.4s,v0.4s,#12
	add	w13,w13,w17
	sli	v13.4s,v1.4s,#12
	add	w14,w14,w19
	sli	v17.4s,v2.4s,#12
	add	w15,w15,w20
	sli	v21.4s,v3.4s,#12
	add	w16,w16,w21
	sli	v25.4s,v4.4s,#12
	eor	w9,w9,w13
	sli	v29.4s,v5.4s,#12
	eor	w10,w10,w14
	add	v8.4s,v8.4s,v9.4s
	eor	w11,w11,w15
	add	v12.4s,v12.4s,v13.4s
	eor	w12,w12,w16
	add	v16.4s,v16.4s,v17.4s
	ror	w9,w9,#25
	add	v20.4s,v20.4s,v21.4s
	ror	w10,w10,#25
	add	v24.4s,v24.4s,v25.4s
	ror	w11,w11,#25
	add	v28.4s,v28.4s,v29.4s
	ror	w12,w12,#25
	eor	v11.16b,v11.16b,v8.16b
	add	w5,w5,w10
	eor	v15.16b,v15.16b,v12.16b
	add	w6,w6,w11
	eor	v19.16b,v19.16b,v16.16b
	add	w7,w7,w12
	eor	v23.16b,v23.16b,v20.16b
	add	w8,w8,w9
	eor	v27.16b,v27.16b,v24.16b
	eor	w21,w21,w5
	eor	v31.16b,v31.16b,v28.16b
	eor	w17,w17,w6
	tbl	v11.16b,{v11.16b},v6.16b
	eor	w19,w19,w7
	tbl	v15.16b,{v15.16b},v6.16b
	eor	w20,w20,w8
	tbl	v19.16b,{v19.16b},v6.16b
	ror	w21,w21,#16
	tbl	v23.16b,{v23.16b},v6.16b
	ror	w17,w17,#16
	tbl	v27.16b,{v27.16b},v6.16b
	ror	w19,w19,#16
	tbl	v31.16b,{v31.16b},v6.16b
	ror	w20,w20,#16
	add	v10.4s,v10.4s,v11.4s
	add	w15,w15,w21
	add	v14.4s,v14.4s,v15.4s
	add	w16,w16,w17
	add	v18.4s,v18.4s,v19.4s
	add	w13,w13,w19
	add	v22.4s,v22.4s,v23.4s
	add	w14,w14,w20
	add	v26.4s,v26.4s,v27.4s
	eor	w10,w10,w15
	add	v30.4s,v30.4s,v31.4s
	eor	w11,w11,w16
	eor	v0.16b,v9.16b,v10.16b
	eor	w12,w12,w13
	eor	v1.16b,v13.16b,v14.16b
	eor	w9,w9,w14
	eor	v2.16b,v17.16b,v18.16b
	ror	w10,w10,#20
	eor	v3.16b,v21.16b,v22.16b
	ror	w11,w11,#20
	eor	v4.16b,v25.16b,v26.16b
	ror	w12,w12,#20
	eor	v5.16b,v29.16b,v30.16b
	ror	w9,w9,#20
	ushr	v9.4s,v0.4s,#25
	add	w5,w5,w10
	ushr	v13.4s,v1.4s,#25
	add	w6,w6,w11
	ushr	v17.4s,v2.4s,#25
	add	w7,w7,w12
	ushr	v21.4s,v3.4s,#25
	add	w8,w8,w9
	ushr	v25.4s,v4.4s,#25
	eor	w21,w21,w5
	ushr	v29.4s,v5.4s,#25
	eor	w17,w17,w6
	sli	v9.4s,v0.4s,#7
	eor	w19,w19,w7
	sli	v13.4s,v1.4s,#7
	eor	w20,w20,w8
	sli	v17.4s,v2.4s,#7
	ror	w21,w21,#24
	sli	v21.4s,v3.4s,#7
	ror	w17,w17,#24
	sli	v25.4s,v4.4s,#7
	ror	w19,w19,#24
	sli	v29.4s,v5.4s,#7
	ror	w20,w20,#24
	ext	v10.16b,v10.16b,v10.16b,#8
	add	w15,w15,w21
	ext	v14.16b,v14.16b,v14.16b,#8
	add	w16,w16,w17
	ext	v18.16b,v18.16b,v18.16b,#8
	add	w13,w13,w19
	ext	v22.16b,v22.16b,v22.16b,#8
	add	w14,w14,w20
	ext	v26.16b,v26.16b,v26.16b,#8
	eor	w10,w10,w15
	ext	v30.16b,v30.16b,v30.16b,#8
	eor	w11,w11,w16
	ext	v11.16b,v11.16b,v11.16b,#12
	eor	w12,w12,w13
	ext	v15.16b,v15.16b,v15.16b,#12
	eor	w9,w9,w14
	ext	v19.16b,v19.16b,v19.16b,#12
	ror	w10,w10,#25
	ext	v23.16b,v23.16b,v23.16b,#12
	ror	w11,w11,#25
	ext	v27.16b,v27.16b,v27.16b,#12
	ror	w12,w12,#25
	ext	v31.16b,v31.16b,v31.16b,#12
	ror	w9,w9,#25
	ext	v9.16b,v9.16b,v9.16b,#4
	ext	v13.16b,v13.16b,v13.16b,#4
	ext	v17.16b,v17.16b,v17.16b,#4
	ext	v21.16b,v21.16b,v21.16b,#4
	ext	v25.16b,v25.16b,v25.16b,#4
	ext	v29.16b,v29.16b,v29.16b,#4
	add	v8.4s,v8.4s,v9.4s
	add	w5,w5,w9
	add	v12.4s,v12.4s,v13.4s
	add	w6,w6,w10
	add	v16.4s,v16.4s,v17.4s
	add	w7,w7,w11
	add	v20.4s,v20.4s,v21.4s
	add	w8,w8,w12
	add	v24.4s,v24.4s,v25.4s
	eor	w17,w17,w5
	add	v28.4s,v28.4s,v29.4s
	eor	w19,w19,w6
	eor	v11.16b,v11.16b,v8.16b
	eor	w20,w20,w7
	eor	v15.16b,v15.16b,v12.16b
	eor	w21,w21,w8
	eor	v19.16b,v19.16b,v16.16b
	ror	w17,w17,#16
	eor	v23.16b,v23.16b,v20.16b
	ror	w19,w19,#16
	eor	v27.16b,v27.16b,v24.16b
	ror	w20,w20,#16
	eor	v31.16b,v31.16b,v28.16b
	ror	w21,w21,#16
	rev32	v11.8h,v11.8h
	add	w13,w13,w17
	rev32	v15.8h,v15.8h
	add	w14,w14,w19
	rev32	v19.8h,v19.8h
	add	w15,w15,w20
	rev32	v23.8h,v23.8h
	add	w16,w16,w21
	rev32	v27.8h,v27.8h
	eor	w9,w9,w13
	rev32	v31.8h,v31.8h
	eor	w10,w10,w14
	add	v10.4s,v10.4s,v11.4s
	eor	w11,w11,w15
	add	v14.4s,v14.4s,v15.4s
	eor	w12,w12,w16
	add	v18.4s,v18.4s,v19.4s
	ror	w9,w9,#20
	add	v22.4s,v22.4s,v23.4s
	ror	w10,w10,#20
	add	v26.4s,v26.4s,v27.4s
	ror	w11,w11,#20
	add	v30.4s,v30.4s,v31.4s
	ror	w12,w12,#20
	eor	v0.16b,v9.16b,v10.16b
	add	w5,w5,w9
	eor	v1.16b,v13.16b,v14.16b
	add	w6,w6,w10
	eor	v2.16b,v17.16b,v18.16b
	add	w7,w7,w11
	eor	v3.16b,v21.16b,v22.16b
	add	w8,w8,w12
	eor	v4.16b,v25.16b,v26.16b
	eor	w17,w17,w5
	eor	v5.16b,v29.16b,v30.16b
	eor	w19,w19,w6
	ushr	v9.4s,v0.4s,#20
	eor	w20,w20,w7
	ushr	v13.4s,v1.4s,#20
	eor	w21,w21,w8
	ushr	v17.4s,v2.4s,#20
	ror	w17,w17,#24
	ushr	v21.4s,v3.4s,#20
	ror	w19,w19,#24
	ushr	v25.4s,v4.4s,#20
	ror	w20,w20,#24
	ushr	v29.4s,v5.4s,#20
	ror	w21,w21,#24
	sli	v9.4s,v0.4s,#12
	add	w13,w13,w17
	sli	v13.4s,v1.4s,#12
	add	w14,w14,w19
	sli	v17.4s,v2.4s,#12
	add	w15,w15,w20
	sli	v21.4s,v3.4s,#12
	add	w16,w16,w21
	sli	v25.4s,v4.4s,#12
	eor	w9,w9,w13
	sli	v29.4s,v5.4s,#12
	eor	w10,w10,w14
	add	v8.4s,v8.4s,v9.4s
	eor	w11,w11,w15
	add	v12.4s,v12.4s,v13.4s
	eor	w12,w12,w16
	add	v16.4s,v16.4s,v17.4s
	ror	w9,w9,#25
	add	v20.4s,v20.4s,v21.4s
	ror	w10,w10,#25
	add	v24.4s,v24.4s,v25.4s
	ror	w11,w11,#25
	add	v28.4s,v28.4s,v29.4s
	ror	w12,w12,#25
	eor	v11.16b,v11.16b,v8.16b
	add	w5,w5,w10
	eor	v15.16b,v15.16b,v12.16b
	add	w6,w6,w11
	eor	v19.16b,v19.16b,v16.16b
	add	w7,w7,w12
	eor	v23.16b,v23.16b,v20.16b
	add	w8,w8,w9
	eor	v27.16b,v27.16b,v24.16b
	eor	w21,w21,w5
	eor	v31.16b,v31.16b,v28.16b
	eor	w17,w17,w6
	tbl	v11.16b,{v11.16b},v6.16b
	eor	w19,w19,w7
	tbl	v15.16b,{v15.16b},v6.16b
	eor	w20,w20,w8
	tbl	v19.16b,{v19.16b},v6.16b
	ror	w21,w21,#16
	tbl	v23.16b,{v23.16b},v6.16b
	ror	w17,w17,#16
	tbl	v27.16b,{v27.16b},v6.16b
	ror	w19,w19,#16
	tbl	v31.16b,{v31.16b},v6.16b
	ror	w20,w20,#16
	add	v10.4s,v10.4s,v11.4s
	add	w15,w15,w21
	add	v14.4s,v14.4s,v15.4s
	add	w16,w16,w17
	add	v18.4s,v18.4s,v19.4s
	add	w13,w13,w19
	add	v22.4s,v22.4s,v23.4s
	add	w14,w14,w20
	add	v26.4s,v26.4s,v27.4s
	eor	w10,w10,w15
	add	v30.4s,v30.4s,v31.4s
	eor	w11,w11,w16
	eor	v0.16b,v9.16b,v10.16b
	eor	w12,w12,w13
	eor	v1.16b,v13.16b,v14.16b
	eor	w9,w9,w14
	eor	v2.16b,v17.16b,v18.16b
	ror	w10,w10,#20
	eor	v3.16b,v21.16b,v22.16b
	ror	w11,w11,#20
	eor	v4.16b,v25.16b,v26.16b
	ror	w12,w12,#20
	eor	v5.16b,v29.16b,v30.16b
	ror	w9,w9,#20
	ushr	v9.4s,v0.4s,#25
	add	w5,w5,w10
	ushr	v13.4s,v1.4s,#25
	add	w6,w6,w11
	ushr	v17.4s,v2.4s,#25
	add	w7,w7,w12
	ushr	v21.4s,v3.4s,#25
	add	w8,w8,w9
	ushr	v25.4s,v4.4s,#25
	eor	w21,w21,w5
	ushr	v29.4s,v5.4s,#25
	eor	w17,w17,w6
	sli	v9.4s,v0.4s,#7
	eor	w19,w19,w7
	sli	v13.4s,v1.4s,#7
	eor	w20,w20,w8
	sli	v17.4s,v2.4s,#7
	ror	w21,w21,#24
	sli	v21.4s,v3.4s,#7
	ror	w17,w17,#24
	sli	v25.4s,v4.4s,#7
	ror	w19,w19,#24
	sli	v29.4s,v5.4s,#7
	ror	w20,w20,#24
	ext	v10.16b,v10.16b,v10.16b,#8
	add	w15,w15,w21
	ext	v14.16b,v14.16b,v14.16b,#8
	add	w16,w16,w17
	ext	v18.16b,v18.16b,v18.16b,#8
	add	w13,w13,w19
	ext	v22.16b,v22.16b,v22.16b,#8
	add	w14,w14,w20
	ext	v26.16b,v26.16b,v26.16b,#8
	eor	w10,w10,w15
	ext	v30.16b,v30.16b,v30.16b,#8
	eor	w11,w11,w16
	ext	v11.16b,v11.16b,v11.16b,#4
	eor	w12,w12,w13
	ext	v15.16b,v15.16b,v15.16b,#4
	eor	w9,w9,w14
	ext	v19.16b,v19.16b,v19.16b,#4
	ror	w10,w10,#25
	ext	v23.16b,v23.16b,v23.16b,#4
	ror	w11,w11,#25
	ext	v27.16b,v27.16b,v27.16b,#4
	ror	w12,w12,#25
	ext	v31.16b,v31.16b,v31.16b,#4
	ror	w9,w9,#25
	ext	v9.16b,v9.16b,v9.16b,#12
	ext	v13.16b,v13.16b,v13.16b,#12
	ext	v17.16b,v17.16b,v17.16b,#12
	ext	v21.16b,v21.16b,v21.16b,#12
	ext	v25.16b,v25.16b,v25.16b,#12
	ext	v29.16b,v29.16b,v29.16b,#12
	cbnz	x4,.Loop_upper_neon

	add	w5,w5,w22		// accumulate key block
	add	x6,x6,x22,lsr#32
	add	w7,w7,w23
	add	x8,x8,x23,lsr#32
	add	w9,w9,w24
	add	x10,x10,x24,lsr#32
	add	w11,w11,w25
	add	x12,x12,x25,lsr#32
	add	w13,w13,w26
	add	x14,x14,x26,lsr#32
	add	w15,w15,w27
	add	x16,x16,x27,lsr#32
	add	w17,w17,w28
	add	x19,x19,x28,lsr#32
	add	w20,w20,w30
	add	x21,x21,x30,lsr#32

	add	x5,x5,x6,lsl#32	// pack
	add	x7,x7,x8,lsl#32
	ldp	x6,x8,[x1,#0]		// load input
	add	x9,x9,x10,lsl#32
	add	x11,x11,x12,lsl#32
	ldp	x10,x12,[x1,#16]
	add	x13,x13,x14,lsl#32
	add	x15,x15,x16,lsl#32
	ldp	x14,x16,[x1,#32]
	add	x17,x17,x19,lsl#32
	add	x20,x20,x21,lsl#32
	ldp	x19,x21,[x1,#48]
	add	x1,x1,#64
#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	eor	x5,x5,x6
	eor	x7,x7,x8
	eor	x9,x9,x10
	eor	x11,x11,x12
	eor	x13,x13,x14
	eor	x15,x15,x16
	eor	x17,x17,x19
	eor	x20,x20,x21

	stp	x5,x7,[x0,#0]		// store output
	add	x28,x28,#1			// increment counter
	mov	w5,w22			// unpack key block
	lsr	x6,x22,#32
	stp	x9,x11,[x0,#16]
	mov	w7,w23
	lsr	x8,x23,#32
	stp	x13,x15,[x0,#32]
	mov	w9,w24
	lsr	x10,x24,#32
	stp	x17,x20,[x0,#48]
	add	x0,x0,#64
	mov	w11,w25
	lsr	x12,x25,#32
	mov	w13,w26
	lsr	x14,x26,#32
	mov	w15,w27
	lsr	x16,x27,#32
	mov	w17,w28
	lsr	x19,x28,#32
	mov	w20,w30
	lsr	x21,x30,#32

	mov	x4,#5
.Loop_lower_neon:
	sub	x4,x4,#1
	add	v8.4s,v8.4s,v9.4s
	add	w5,w5,w9
	add	v12.4s,v12.4s,v13.4s
	add	w6,w6,w10
	add	v16.4s,v16.4s,v17.4s
	add	w7,w7,w11
	add	v20.4s,v20.4s,v21.4s
	add	w8,w8,w12
	add	v24.4s,v24.4s,v25.4s
	eor	w17,w17,w5
	add	v28.4s,v28.4s,v29.4s
	eor	w19,w19,w6
	eor	v11.16b,v11.16b,v8.16b
	eor	w20,w20,w7
	eor	v15.16b,v15.16b,v12.16b
	eor	w21,w21,w8
	eor	v19.16b,v19.16b,v16.16b
	ror	w17,w17,#16
	eor	v23.16b,v23.16b,v20.16b
	ror	w19,w19,#16
	eor	v27.16b,v27.16b,v24.16b
	ror	w20,w20,#16
	eor	v31.16b,v31.16b,v28.16b
	ror	w21,w21,#16
	rev32	v11.8h,v11.8h
	add	w13,w13,w17
	rev32	v15.8h,v15.8h
	add	w14,w14,w19
	rev32	v19.8h,v19.8h
	add	w15,w15,w20
	rev32	v23.8h,v23.8h
	add	w16,w16,w21
	rev32	v27.8h,v27.8h
	eor	w9,w9,w13
	rev32	v31.8h,v31.8h
	eor	w10,w10,w14
	add	v10.4s,v10.4s,v11.4s
	eor	w11,w11,w15
	add	v14.4s,v14.4s,v15.4s
	eor	w12,w12,w16
	add	v18.4s,v18.4s,v19.4s
	ror	w9,w9,#20
	add	v22.4s,v22.4s,v23.4s
	ror	w10,w10,#20
	add	v26.4s,v26.4s,v27.4s
	ror	w11,w11,#20
	add	v30.4s,v30.4s,v31.4s
	ror	w12,w12,#20
	eor	v0.16b,v9.16b,v10.16b
	add	w5,w5,w9
	eor	v1.16b,v13.16b,v14.16b
	add	w6,w6,w10
	eor	v2.16b,v17.16b,v18.16b
	add	w7,w7,w11
	eor	v3.16b,v21.16b,v22.16b
	add	w8,w8,w12
	eor	v4.16b,v25.16b,v26.16b
	eor	w17,w17,w5
	eor	v5.16b,v29.16b,v30.16b
	eor	w19,w19,w6
	ushr	v9.4s,v0.4s,#20
	eor	w20,w20,w7
	ushr	v13.4s,v1.4s,#20
	eor	w21,w21,w8
	ushr	v17.4s,v2.4s,#20
	ror	w17,w17,#24
	ushr	v21.4s,v3.4s,#20
	ror	w19,w19,#24
	ushr	v25.4s,v4.4s,#20
	ror	w20,w20,#24
	ushr	v29.4s,v5.4s,#20
	ror	w21,w21,#24
	sli	v9.4s,v0.4s,#12
	add	w13,w13,w17
	sli	v13.4s,v1.4s,#12
	add	w14,w14,w19
	sli	v17.4s,v2.4s,#12
	add	w15,w15,w20
	sli	v21.4s,v3.4s,#12
	add	w16,w16,w21
	sli	v25.4s,v4.4s,#12
	eor	w9,w9,w13
	sli	v29.4s,v5.4s,#12
	eor	w10,w10,w14
	add	v8.4s,v8.4s,v9.4s
	eor	w11,w11,w15
	add	v12.4s,v12.4s,v13.4s
	eor	w12,w12,w16
	add	v16.4s,v16.4s,v17.4s
	ror	w9,w9,#25
	add	v20.4s,v20.4s,v21.4s
	ror	w10,w10,#25
	add	v24.4s,v24.4s,v25.4s
	ror	w11,w11,#25
	add	v28.4s,v28.4s,v29.4s
	ror	w12,w12,#25
	eor	v11.16b,v11.16b,v8.16b
	add	w5,w5,w10
	eor	v15.16b,v15.16b,v12.16b
	add	w6,w6,w11
	eor	v19.16b,v19.16b,v16.16b
	add	w7,w7,w12
	eor	v23.16b,v23.16b,v20.16b
	add	w8,w8,w9
	eor	v27.16b,v27.16b,v24.16b
	eor	w21,w21,w5
	eor	v31.16b,v31.16b,v28.16b
	eor	w17,w17,w6
	tbl	v11.16b,{v11.16b},v6.16b
	eor	w19,w19,w7
	tbl	v15.16b,{v15.16b},v6.16b
	eor	w20,w20,w8
	tbl	v19.16b,{v19.16b},v6.16b
	ror	w21,w21,#16
	tbl	v23.16b,{v23.16b},v6.16b
	ror	w17,w17,#16
	tbl	v27.16b,{v27.16b},v6.16b
	ror	w19,w19,#16
	tbl	v31.16b,{v31.16b},v6.16b
	ror	w20,w20,#16
	add	v10.4s,v10.4s,v11.4s
	add	w15,w15,w21
	add	v14.4s,v14.4s,v15.4s
	add	w16,w16,w17
	add	v18.4s,v18.4s,v19.4s
	add	w13,w13,w19
	add	v22.4s,v22.4s,v23.4s
	add	w14,w14,w20
	add	v26.4s,v26.4s,v27.4s
	eor	w10,w10,w15
	add	v30.4s,v30.4s,v31.4s
	eor	w11,w11,w16
	eor	v0.16b,v9.16b,v10.16b
	eor	w12,w12,w13
	eor	v1.16b,v13.16b,v14.16b
	eor	w9,w9,w14
	eor	v2.16b,v17.16b,v18.16b
	ror	w10,w10,#20
	eor	v3.16b,v21.16b,v22.16b
	ror	w11,w11,#20
	eor	v4.16b,v25.16b,v26.16b
	ror	w12,w12,#20
	eor	v5.16b,v29.16b,v30.16b
	ror	w9,w9,#20
	ushr	v9.4s,v0.4s,#25
	add	w5,w5,w10
	ushr	v13.4s,v1.4s,#25
	add	w6,w6,w11
	ushr	v17.4s,v2.4s,#25
	add	w7,w7,w12
	ushr	v21.4s,v3.4s,#25
	add	w8,w8,w9
	ushr	v25.4s,v4.4s,#25
	eor	w21,w21,w5
	ushr	v29.4s,v5.4s,#25
	eor	w17,w17,w6
	sli	v9.4s,v0.4s,#7
	eor	w19,w19,w7
	sli	v13.4s,v1.4s,#7
	eor	w20,w20,w8
	sli	v17.4s,v2.4s,#7
	ror	w21,w21,#24
	sli	v21.4s,v3.4s,#7
	ror	w17,w17,#24
	sli	v25.4s,v4.4s,#7
	ror	w19,w19,#24
	sli	v29.4s,v5.4s,#7
	ror	w20,w20,#24
	ext	v10.16b,v10.16b,v10.16b,#8
	add	w15,w15,w21
	ext	v14.16b,v14.16b,v14.16b,#8
	add	w16,w16,w17
	ext	v18.16b,v18.16b,v18.16b,#8
	add	w13,w13,w19
	ext	v22.16b,v22.16b,v22.16b,#8
	add	w14,w14,w20
	ext	v26.16b,v26.16b,v26.16b,#8
	eor	w10,w10,w15
	ext	v30.16b,v30.16b,v30.16b,#8
	eor	w11,w11,w16
	ext	v11.16b,v11.16b,v11.16b,#12
	eor	w12,w12,w13
	ext	v15.16b,v15.16b,v15.16b,#12
	eor	w9,w9,w14
	ext	v19.16b,v19.16b,v19.16b,#12
	ror	w10,w10,#25
	ext	v23.16b,v23.16b,v23.16b,#12
	ror	w11,w11,#25
	ext	v27.16b,v27.16b,v27.16b,#12
	ror	w12,w12,#25
	ext	v31.16b,v31.16b,v31.16b,#12
	ror	w9,w9,#25
	ext	v9.16b,v9.16b,v9.16b,#4
	ext	v13.16b,v13.16b,v13.16b,#4
	ext	v17.16b,v17.16b,v17.16b,#4
	ext	v21.16b,v21.16b,v21.16b,#4
	ext	v25.16b,v25.16b,v25.16b,#4
	ext	v29.16b,v29.16b,v29.16b,#4
	add	v8.4s,v8.4s,v9.4s
	add	w5,w5,w9
	add	v12.4s,v12.4s,v13.4s
	add	w6,w6,w10
	add	v16.4s,v16.4s,v17.4s
	add	w7,w7,w11
	add	v20.4s,v20.4s,v21.4s
	add	w8,w8,w12
	add	v24.4s,v24.4s,v25.4s
	eor	w17,w17,w5
	add	v28.4s,v28.4s,v29.4s
	eor	w19,w19,w6
	eor	v11.16b,v11.16b,v8.16b
	eor	w20,w20,w7
	eor	v15.16b,v15.16b,v12.16b
	eor	w21,w21,w8
	eor	v19.16b,v19.16b,v16.16b
	ror	w17,w17,#16
	eor	v23.16b,v23.16b,v20.16b
	ror	w19,w19,#16
	eor	v27.16b,v27.16b,v24.16b
	ror	w20,w20,#16
	eor	v31.16b,v31.16b,v28.16b
	ror	w21,w21,#16
	rev32	v11.8h,v11.8h
	add	w13,w13,w17
	rev32	v15.8h,v15.8h
	add	w14,w14,w19
	rev32	v19.8h,v19.8h
	add	w15,w15,w20
	rev32	v23.8h,v23.8h
	add	w16,w16,w21
	rev32	v27.8h,v27.8h
	eor	w9,w9,w13
	rev32	v31.8h,v31.8h
	eor	w10,w10,w14
	add	v10.4s,v10.4s,v11.4s
	eor	w11,w11,w15
	add	v14.4s,v14.4s,v15.4s
	eor	w12,w12,w16
	add	v18.4s,v18.4s,v19.4s
	ror	w9,w9,#20
	add	v22.4s,v22.4s,v23.4s
	ror	w10,w10,#20
	add	v26.4s,v26.4s,v27.4s
	ror	w11,w11,#20
	add	v30.4s,v30.4s,v31.4s
	ror	w12,w12,#20
	eor	v0.16b,v9.16b,v10.16b
	add	w5,w5,w9
	eor	v1.16b,v13.16b,v14.16b
	add	w6,w6,w10
	eor	v2.16b,v17.16b,v18.16b
	add	w7,w7,w11
	eor	v3.16b,v21.16b,v22.16b
	add	w8,w8,w12
	eor	v4.16b,v25.16b,v26.16b
	eor	w17,w17,w5
	eor	v5.16b,v29.16b,v30.16b
	eor	w19,w19,w6
	ushr	v9.4s,v0.4s,#20
	eor	w20,w20,w7
	ushr	v13.4s,v1.4s,#20
	eor	w21,w21,w8
	ushr	v17.4s,v2.4s,#20
	ror	w17,w17,#24
	ushr	v21.4s,v3.4s,#20
	ror	w19,w19,#24
	ushr	v25.4s,v4.4s,#20
	ror	w20,w20,#24
	ushr	v29.4s,v5.4s,#20
	ror	w21,w21,#24
	sli	v9.4s,v0.4s,#12
	add	w13,w13,w17
	sli	v13.4s,v1.4s,#12
	add	w14,w14,w19
	sli	v17.4s,v2.4s,#12
	add	w15,w15,w20
	sli	v21.4s,v3.4s,#12
	add	w16,w16,w21
	sli	v25.4s,v4.4s,#12
	eor	w9,w9,w13
	sli	v29.4s,v5.4s,#12
	eor	w10,w10,w14
	add	v8.4s,v8.4s,v9.4s
	eor	w11,w11,w15
	add	v12.4s,v12.4s,v13.4s
	eor	w12,w12,w16
	add	v16.4s,v16.4s,v17.4s
	ror	w9,w9,#25
	add	v20.4s,v20.4s,v21.4s
	ror	w10,w10,#25
	add	v24.4s,v24.4s,v25.4s
	ror	w11,w11,#25
	add	v28.4s,v28.4s,v29.4s
	ror	w12,w12,#25
	eor	v11.16b,v11.16b,v8.16b
	add	w5,w5,w10
	eor	v15.16b,v15.16b,v12.16b
	add	w6,w6,w11
	eor	v19.16b,v19.16b,v16.16b
	add	w7,w7,w12
	eor	v23.16b,v23.16b,v20.16b
	add	w8,w8,w9
	eor	v27.16b,v27.16b,v24.16b
	eor	w21,w21,w5
	eor	v31.16b,v31.16b,v28.16b
	eor	w17,w17,w6
	tbl	v11.16b,{v11.16b},v6.16b
	eor	w19,w19,w7
	tbl	v15.16b,{v15.16b},v6.16b
	eor	w20,w20,w8
	tbl	v19.16b,{v19.16b},v6.16b
	ror	w21,w21,#16
	tbl	v23.16b,{v23.16b},v6.16b
	ror	w17,w17,#16
	tbl	v27.16b,{v27.16b},v6.16b
	ror	w19,w19,#16
	tbl	v31.16b,{v31.16b},v6.16b
	ror	w20,w20,#16
	add	v10.4s,v10.4s,v11.4s
	add	w15,w15,w21
	add	v14.4s,v14.4s,v15.4s
	add	w16,w16,w17
	add	v18.4s,v18.4s,v19.4s
	add	w13,w13,w19
	add	v22.4s,v22.4s,v23.4s
	add	w14,w14,w20
	add	v26.4s,v26.4s,v27.4s
	eor	w10,w10,w15
	add	v30.4s,v30.4s,v31.4s
	eor	w11,w11,w16
	eor	v0.16b,v9.16b,v10.16b
	eor	w12,w12,w13
	eor	v1.16b,v13.16b,v14.16b
	eor	w9,w9,w14
	eor	v2.16b,v17.16b,v18.16b
	ror	w10,w10,#20
	eor	v3.16b,v21.16b,v22.16b
	ror	w11,w11,#20
	eor	v4.16b,v25.16b,v26.16b
	ror	w12,w12,#20
	eor	v5.16b,v29.16b,v30.16b
	ror	w9,w9,#20
	ushr	v9.4s,v0.4s,#25
	add	w5,w5,w10
	ushr	v13.4s,v1.4s,#25
	add	w6,w6,w11
	ushr	v17.4s,v2.4s,#25
	add	w7,w7,w12
	ushr	v21.4s,v3.4s,#25
	add	w8,w8,w9
	ushr	v25.4s,v4.4s,#25
	eor	w21,w21,w5
	ushr	v29.4s,v5.4s,#25
	eor	w17,w17,w6
	sli	v9.4s,v0.4s,#7
	eor	w19,w19,w7
	sli	v13.4s,v1.4s,#7
	eor	w20,w20,w8
	sli	v17.4s,v2.4s,#7
	ror	w21,w21,#24
	sli	v21.4s,v3.4s,#7
	ror	w17,w17,#24
	sli	v25.4s,v4.4s,#7
	ror	w19,w19,#24
	sli	v29.4s,v5.4s,#7
	ror	w20,w20,#24
	ext	v10.16b,v10.16b,v10.16b,#8
	add	w15,w15,w21
	ext	v14.16b,v14.16b,v14.16b,#8
	add	w16,w16,w17
	ext	v18.16b,v18.16b,v18.16b,#8
	add	w13,w13,w19
	ext	v22.16b,v22.16b,v22.16b,#8
	add	w14,w14,w20
	ext	v26.16b,v26.16b,v26.16b,#8
	eor	w10,w10,w15
	ext	v30.16b,v30.16b,v30.16b,#8
	eor	w11,w11,w16
	ext	v11.16b,v11.16b,v11.16b,#4
	eor	w12,w12,w13
	ext	v15.16b,v15.16b,v15.16b,#4
	eor	w9,w9,w14
	ext	v19.16b,v19.16b,v19.16b,#4
	ror	w10,w10,#25
	ext	v23.16b,v23.16b,v23.16b,#4
	ror	w11,w11,#25
	ext	v27.16b,v27.16b,v27.16b,#4
	ror	w12,w12,#25
	ext	v31.16b,v31.16b,v31.16b,#4
	ror	w9,w9,#25
	ext	v9.16b,v9.16b,v9.16b,#12
	ext	v13.16b,v13.16b,v13.16b,#12
	ext	v17.16b,v17.16b,v17.16b,#12
	ext	v21.16b,v21.16b,v21.16b,#12
	ext	v25.16b,v25.16b,v25.16b,#12
	ext	v29.16b,v29.16b,v29.16b,#12
	cbnz	x4,.Loop_lower_neon

	add	w5,w5,w22		// accumulate key block
	ldp	q0,q1,[sp,#0]
	add	x6,x6,x22,lsr#32
	ldp	q2,q3,[sp,#32]
	add	w7,w7,w23
	ldp	q4,q5,[sp,#64]
	add	x8,x8,x23,lsr#32
	ldr	q6,[sp,#96]
	add	v8.4s,v8.4s,v0.4s
	add	w9,w9,w24
	add	v12.4s,v12.4s,v0.4s
	add	x10,x10,x24,lsr#32
	add	v16.4s,v16.4s,v0.4s
	add	w11,w11,w25
	add	v20.4s,v20.4s,v0.4s
	add	x12,x12,x25,lsr#32
	add	v24.4s,v24.4s,v0.4s
	add	w13,w13,w26
	add	v28.4s,v28.4s,v0.4s
	add	x14,x14,x26,lsr#32
	add	v10.4s,v10.4s,v2.4s
	add	w15,w15,w27
	add	v14.4s,v14.4s,v2.4s
	add	x16,x16,x27,lsr#32
	add	v18.4s,v18.4s,v2.4s
	add	w17,w17,w28
	add	v22.4s,v22.4s,v2.4s
	add	x19,x19,x28,lsr#32
	add	v26.4s,v26.4s,v2.4s
	add	w20,w20,w30
	add	v30.4s,v30.4s,v2.4s
	add	x21,x21,x30,lsr#32
	add	v27.4s,v27.4s,v7.4s			// +4
	add	x5,x5,x6,lsl#32	// pack
	add	v31.4s,v31.4s,v7.4s			// +4
	add	x7,x7,x8,lsl#32
	add	v11.4s,v11.4s,v3.4s
	ldp	x6,x8,[x1,#0]		// load input
	add	v15.4s,v15.4s,v4.4s
	add	x9,x9,x10,lsl#32
	add	v19.4s,v19.4s,v5.4s
	add	x11,x11,x12,lsl#32
	add	v23.4s,v23.4s,v6.4s
	ldp	x10,x12,[x1,#16]
	add	v27.4s,v27.4s,v3.4s
	add	x13,x13,x14,lsl#32
	add	v31.4s,v31.4s,v4.4s
	add	x15,x15,x16,lsl#32
	add	v9.4s,v9.4s,v1.4s
	ldp	x14,x16,[x1,#32]
	add	v13.4s,v13.4s,v1.4s
	add	x17,x17,x19,lsl#32
	add	v17.4s,v17.4s,v1.4s
	add	x20,x20,x21,lsl#32
	add	v21.4s,v21.4s,v1.4s
	ldp	x19,x21,[x1,#48]
	add	v25.4s,v25.4s,v1.4s
	add	x1,x1,#64
	add	v29.4s,v29.4s,v1.4s

#ifdef	__AARCH64EB__
	rev	x5,x5
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x20,x20
#endif
	ld1	{v0.16b,v1.16b,v2.16b,v3.16b},[x1],#64
	eor	x5,x5,x6
	eor	x7,x7,x8
	eor	x9,x9,x10
	eor	x11,x11,x12
	eor	x13,x13,x14
	eor	v8.16b,v8.16b,v0.16b
	eor	x15,x15,x16
	eor	v9.16b,v9.16b,v1.16b
	eor	x17,x17,x19
	eor	v10.16b,v10.16b,v2.16b
	eor	x20,x20,x21
	eor	v11.16b,v11.16b,v3.16b
	ld1	{v0.16b,v1.16b,v2.16b,v3.16b},[x1],#64

	stp	x5,x7,[x0,#0]		// store output
	add	x28,x28,#7			// increment counter
	stp	x9,x11,[x0,#16]
	stp	x13,x15,[x0,#32]
	stp	x17,x20,[x0,#48]
	add	x0,x0,#64
	st1	{v8.16b,v9.16b,v10.16b,v11.16b},[x0],#64

	ld1	{v8.16b,v9.16b,v10.16b,v11.16b},[x1],#64
	eor	v12.16b,v12.16b,v0.16b
	eor	v13.16b,v13.16b,v1.16b
	eor	v14.16b,v14.16b,v2.16b
	eor	v15.16b,v15.16b,v3.16b
	st1	{v12.16b,v13.16b,v14.16b,v15.16b},[x0],#64

	ld1	{v12.16b,v13.16b,v14.16b,v15.16b},[x1],#64
	eor	v16.16b,v16.16b,v8.16b
	ldp	q0,q1,[sp,#0]
	eor	v17.16b,v17.16b,v9.16b
	ldp	q2,q3,[sp,#32]
	eor	v18.16b,v18.16b,v10.16b
	eor	v19.16b,v19.16b,v11.16b
	st1	{v16.16b,v17.16b,v18.16b,v19.16b},[x0],#64

	ld1	{v16.16b,v17.16b,v18.16b,v19.16b},[x1],#64
	eor	v20.16b,v20.16b,v12.16b
	eor	v21.16b,v21.16b,v13.16b
	eor	v22.16b,v22.16b,v14.16b
	eor	v23.16b,v23.16b,v15.16b
	st1	{v20.16b,v21.16b,v22.16b,v23.16b},[x0],#64

	ld1	{v20.16b,v21.16b,v22.16b,v23.16b},[x1],#64
	eor	v24.16b,v24.16b,v16.16b
	eor	v25.16b,v25.16b,v17.16b
	eor	v26.16b,v26.16b,v18.16b
	eor	v27.16b,v27.16b,v19.16b
	st1	{v24.16b,v25.16b,v26.16b,v27.16b},[x0],#64

	shl	v8.4s,v7.4s,#1			// 4 -> 8
	eor	v28.16b,v28.16b,v20.16b
	eor	v29.16b,v29.16b,v21.16b
	eor	v30.16b,v30.16b,v22.16b
	eor	v31.16b,v31.16b,v23.16b
	st1	{v28.16b,v29.16b,v30.16b,v31.16b},[x0],#64

	add	v3.4s,v3.4s,v8.4s			// += 8
	add	v4.4s,v4.4s,v8.4s
	add	v5.4s,v5.4s,v8.4s
	add	v6.4s,v6.4s,v8.4s

	b.hs	.Loop_outer_512_neon

	adds	x2,x2,#512
	ushr	v7.4s,v7.4s,#1			// 4 -> 2

	ldp	d10,d11,[sp,#128+16]		// meet ABI requirements
	ldp	d12,d13,[sp,#128+32]
	ldp	d14,d15,[sp,#128+48]

	stp	q0,q0,[sp,#0]		// wipe off-load area
	stp	q0,q0,[sp,#32]
	stp	q0,q0,[sp,#64]

	b.eq	.Ldone_512_neon

	sub	x3,x3,#16			// .Lone
	cmp	x2,#192
	add	sp,sp,#128
	sub	v3.4s,v3.4s,v7.4s		// -= 2
	ld1	{v8.4s,v9.4s},[x3]
	b.hs	.Loop_outer_neon

	ldp	d8,d9,[sp,#0]			// meet ABI requirements
	eor	v1.16b,v1.16b,v1.16b
	eor	v2.16b,v2.16b,v2.16b
	eor	v3.16b,v3.16b,v3.16b
	eor	v4.16b,v4.16b,v4.16b
	eor	v5.16b,v5.16b,v5.16b
	eor	v6.16b,v6.16b,v6.16b
	b	.Loop_outer

.Ldone_512_neon:
	ldp	d8,d9,[sp,#128+0]		// meet ABI requirements
	ldp	x19,x20,[x29,#16]
	add	sp,sp,#128+64
	ldp	x21,x22,[x29,#32]
	ldp	x23,x24,[x29,#48]
	ldp	x25,x26,[x29,#64]
	ldp	x27,x28,[x29,#80]
	ldp	x29,x30,[sp],#96
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ChaCha20_512_neon,.-ChaCha20_512_neon
