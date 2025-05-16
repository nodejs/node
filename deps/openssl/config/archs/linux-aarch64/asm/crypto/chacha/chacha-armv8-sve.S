// Copyright 2022-2025  The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html
//
//
// ChaCha20 for ARMv8 via SVE
//
// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arm_arch.h"

.arch	armv8-a


.hidden	OPENSSL_armcap_P

.text

.section	.rodata
.align	5
.type	_chacha_sve_consts,%object
_chacha_sve_consts:
.Lchacha20_consts:
.quad	0x3320646e61707865,0x6b20657479622d32		// endian-neutral
.Lrot8:
.word	0x02010003,0x04040404,0x02010003,0x04040404
.size	_chacha_sve_consts,.-_chacha_sve_consts

.previous

.globl	ChaCha20_ctr32_sve
.type	ChaCha20_ctr32_sve,%function
.align	5
ChaCha20_ctr32_sve:
	AARCH64_VALID_CALL_TARGET
.inst	0x04a0e3e5	//cntw x5, ALL, MUL #1
	cmp	x2,x5,lsl #6
	b.lt	.Lreturn
	mov	x7,0
	adrp	x6,OPENSSL_armcap_P
	ldr	w6,[x6,#:lo12:OPENSSL_armcap_P]
	tst	w6,#ARMV8_SVE2
	b.eq	1f
	mov	x7,1
	b	2f
1:
	cmp	x5,4
	b.le	.Lreturn
	adrp	x6,.Lrot8
	add	x6,x6,#:lo12:.Lrot8
	ldp	w9,w10,[x6]
.inst	0x04aa4d3f	//index z31.s,w9,w10
2:
	AARCH64_SIGN_LINK_REGISTER
	stp	d8,d9,[sp,-192]!
	stp	d10,d11,[sp,16]
	stp	d12,d13,[sp,32]
	stp	d14,d15,[sp,48]
	stp	x16,x17,[sp,64]
	stp	x18,x19,[sp,80]
	stp	x20,x21,[sp,96]
	stp	x22,x23,[sp,112]
	stp	x24,x25,[sp,128]
	stp	x26,x27,[sp,144]
	stp	x28,x29,[sp,160]
	str	x30,[sp,176]

	adrp	x6,.Lchacha20_consts
	add	x6,x6,#:lo12:.Lchacha20_consts
	ldp	x23,x24,[x6]
	ldp	x25,x26,[x3]
	ldp	x27,x28,[x3, 16]
	ldp	x29,x30,[x4]
.inst	0x2599e3e0	//ptrues p0.s,ALL
#ifdef	__AARCH64EB__
	ror	x25,x25,#32
	ror	x26,x26,#32
	ror	x27,x27,#32
	ror	x28,x28,#32
	ror	x29,x29,#32
	ror	x30,x30,#32
#endif
	cbz	x7, 1f
.align	5
100:
	subs	x7,x2,x5,lsl #6
	b.lt	110f
	mov	x2,x7
	b.eq	101f
	cmp	x2,64
	b.lt	101f
	mixin=1
	lsr	x8,x23,#32
.inst	0x05a03ae0	//dup z0.s,w23
.inst	0x05a03af9	//dup z25.s,w23
.if	mixin == 1
	mov	w7,w23
.endif
.inst	0x05a03904	//dup z4.s,w8
.inst	0x05a0391a	//dup z26.s,w8
	lsr	x10,x24,#32
.inst	0x05a03b08	//dup z8.s,w24
.inst	0x05a03b1b	//dup z27.s,w24
.if	mixin == 1
	mov	w9,w24
.endif
.inst	0x05a0394c	//dup z12.s,w10
.inst	0x05a0395c	//dup z28.s,w10
	lsr	x12,x25,#32
.inst	0x05a03b21	//dup z1.s,w25
.inst	0x05a03b3d	//dup z29.s,w25
.if	mixin == 1
	mov	w11,w25
.endif
.inst	0x05a03985	//dup z5.s,w12
.inst	0x05a0399e	//dup z30.s,w12
	lsr	x14,x26,#32
.inst	0x05a03b49	//dup z9.s,w26
.inst	0x05a03b55	//dup z21.s,w26
.if	mixin == 1
	mov	w13,w26
.endif
.inst	0x05a039cd	//dup z13.s,w14
.inst	0x05a039d6	//dup z22.s,w14
	lsr	x16,x27,#32
.inst	0x05a03b62	//dup z2.s,w27
.inst	0x05a03b77	//dup z23.s,w27
.if	mixin == 1
	mov	w15,w27
.endif
.inst	0x05a03a06	//dup z6.s,w16
.inst	0x05a03a18	//dup z24.s,w16
	lsr	x18,x28,#32
.inst	0x05a03b8a	//dup z10.s,w28
.inst	0x05a03b91	//dup z17.s,w28
.if	mixin == 1
	mov	w17,w28
.endif
.inst	0x05a03a4e	//dup z14.s,w18
.inst	0x05a03a52	//dup z18.s,w18
	lsr	x22,x30,#32
.inst	0x05a03bcb	//dup z11.s,w30
.inst	0x05a03bd4	//dup z20.s,w30
.if	mixin == 1
	mov	w21,w30
.endif
.inst	0x05a03acf	//dup z15.s,w22
.inst	0x05a03adf	//dup z31.s,w22
.if	mixin == 1
	add	w20,w29,#1
	mov	w19,w29
.inst	0x04a14690	//index z16.s,w20,1
.inst	0x04a14683	//index z3.s,w20,1
.else
.inst	0x04a147b0	//index z16.s,w29,1
.inst	0x04a147a3	//index z3.s,w29,1
.endif
	lsr	x20,x29,#32
.inst	0x05a03a87	//dup z7.s,w20
.inst	0x05a03a93	//dup z19.s,w20
	mov	x6,#10
10:
.align	5
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04703403	//xar z3.s,z3.s,z0.s,16
.if	mixin == 1
	ror	w19,w19,16
.endif
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04703487	//xar z7.s,z7.s,z4.s,16
.if	mixin == 1
	ror	w20,w20,16
.endif
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x0470350b	//xar z11.s,z11.s,z8.s,16
.if	mixin == 1
	ror	w21,w21,16
.endif
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x0470358f	//xar z15.s,z15.s,z12.s,16
.if	mixin == 1
	ror	w22,w22,16
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x046c3441	//xar z1.s,z1.s,z2.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x046c34c5	//xar z5.s,z5.s,z6.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x046c3549	//xar z9.s,z9.s,z10.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046c35cd	//xar z13.s,z13.s,z14.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04683403	//xar z3.s,z3.s,z0.s,24
.if	mixin == 1
	ror	w19,w19,24
.endif
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04683487	//xar z7.s,z7.s,z4.s,24
.if	mixin == 1
	ror	w20,w20,24
.endif
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x0468350b	//xar z11.s,z11.s,z8.s,24
.if	mixin == 1
	ror	w21,w21,24
.endif
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x0468358f	//xar z15.s,z15.s,z12.s,24
.if	mixin == 1
	ror	w22,w22,24
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04673441	//xar z1.s,z1.s,z2.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x046734c5	//xar z5.s,z5.s,z6.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04673549	//xar z9.s,z9.s,z10.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046735cd	//xar z13.s,z13.s,z14.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x0470340f	//xar z15.s,z15.s,z0.s,16
.if	mixin == 1
	ror	w22,w22,16
.endif
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04703483	//xar z3.s,z3.s,z4.s,16
.if	mixin == 1
	ror	w19,w19,16
.endif
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04703507	//xar z7.s,z7.s,z8.s,16
.if	mixin == 1
	ror	w20,w20,16
.endif
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x0470358b	//xar z11.s,z11.s,z12.s,16
.if	mixin == 1
	ror	w21,w21,16
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x046c3545	//xar z5.s,z5.s,z10.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x046c35c9	//xar z9.s,z9.s,z14.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x046c344d	//xar z13.s,z13.s,z2.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046c34c1	//xar z1.s,z1.s,z6.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x0468340f	//xar z15.s,z15.s,z0.s,24
.if	mixin == 1
	ror	w22,w22,24
.endif
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04683483	//xar z3.s,z3.s,z4.s,24
.if	mixin == 1
	ror	w19,w19,24
.endif
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04683507	//xar z7.s,z7.s,z8.s,24
.if	mixin == 1
	ror	w20,w20,24
.endif
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x0468358b	//xar z11.s,z11.s,z12.s,24
.if	mixin == 1
	ror	w21,w21,24
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04673545	//xar z5.s,z5.s,z10.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x046735c9	//xar z9.s,z9.s,z14.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x0467344d	//xar z13.s,z13.s,z2.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046734c1	//xar z1.s,z1.s,z6.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
	sub	x6,x6,1
	cbnz	x6,10b
.if	mixin == 1
	add	w7,w7,w23
.endif
.inst	0x04b90000	//add z0.s,z0.s,z25.s
.if	mixin == 1
	add	x8,x8,x23,lsr #32
.endif
.inst	0x04ba0084	//add z4.s,z4.s,z26.s
.if	mixin == 1
	add	x7,x7,x8,lsl #32  // pack
.endif
.if	mixin == 1
	add	w9,w9,w24
.endif
.inst	0x04bb0108	//add z8.s,z8.s,z27.s
.if	mixin == 1
	add	x10,x10,x24,lsr #32
.endif
.inst	0x04bc018c	//add z12.s,z12.s,z28.s
.if	mixin == 1
	add	x9,x9,x10,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x8,x10,[x1],#16
.endif
.if	mixin == 1
	add	w11,w11,w25
.endif
.inst	0x04bd0021	//add z1.s,z1.s,z29.s
.if	mixin == 1
	add	x12,x12,x25,lsr #32
.endif
.inst	0x04be00a5	//add z5.s,z5.s,z30.s
.if	mixin == 1
	add	x11,x11,x12,lsl #32  // pack
.endif
.if	mixin == 1
	add	w13,w13,w26
.endif
.inst	0x04b50129	//add z9.s,z9.s,z21.s
.if	mixin == 1
	add	x14,x14,x26,lsr #32
.endif
.inst	0x04b601ad	//add z13.s,z13.s,z22.s
.if	mixin == 1
	add	x13,x13,x14,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x12,x14,[x1],#16
.endif
.if	mixin == 1
	add	w15,w15,w27
.endif
.inst	0x04b70042	//add z2.s,z2.s,z23.s
.if	mixin == 1
	add	x16,x16,x27,lsr #32
.endif
.inst	0x04b800c6	//add z6.s,z6.s,z24.s
.if	mixin == 1
	add	x15,x15,x16,lsl #32  // pack
.endif
.if	mixin == 1
	add	w17,w17,w28
.endif
.inst	0x04b1014a	//add z10.s,z10.s,z17.s
.if	mixin == 1
	add	x18,x18,x28,lsr #32
.endif
.inst	0x04b201ce	//add z14.s,z14.s,z18.s
.if	mixin == 1
	add	x17,x17,x18,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x16,x18,[x1],#16
.endif
.if	mixin == 1
	add	w19,w19,w29
.endif
.inst	0x04b00063	//add z3.s,z3.s,z16.s
.if	mixin == 1
	add	x20,x20,x29,lsr #32
.endif
.inst	0x04b300e7	//add z7.s,z7.s,z19.s
.if	mixin == 1
	add	x19,x19,x20,lsl #32  // pack
.endif
.if	mixin == 1
	add	w21,w21,w30
.endif
.inst	0x04b4016b	//add z11.s,z11.s,z20.s
.if	mixin == 1
	add	x22,x22,x30,lsr #32
.endif
.inst	0x04bf01ef	//add z15.s,z15.s,z31.s
.if	mixin == 1
	add	x21,x21,x22,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x20,x22,[x1],#16
.endif
#ifdef	__AARCH64EB__
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x19,x19
	rev	x21,x21
#endif
.if	mixin == 1
	add	x29,x29,#1
.endif
	cmp	x5,4
	b.ne	200f
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.if	mixin == 1
	eor	x11,x11,x12
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x13,x13,x14
.endif
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23021	//eor z1.d,z1.d,z18.d
.inst	0x04b33042	//eor z2.d,z2.d,z19.d
.inst	0x04b43063	//eor z3.d,z3.d,z20.d
.inst	0x04b53084	//eor z4.d,z4.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b730c6	//eor z6.d,z6.d,z23.d
.inst	0x04b830e7	//eor z7.d,z7.d,z24.d
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13108	//eor z8.d,z8.d,z17.d
.inst	0x04b23129	//eor z9.d,z9.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b4316b	//eor z11.d,z11.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b5318c	//eor z12.d,z12.d,z21.d
.inst	0x04b631ad	//eor z13.d,z13.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b731ce	//eor z14.d,z14.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x0],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	st1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	st1	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	b	210f
200:
.inst	0x05a16011	//zip1 z17.s,z0.s,z1.s
.inst	0x05a16412	//zip2 z18.s,z0.s,z1.s
.inst	0x05a36053	//zip1 z19.s,z2.s,z3.s
.inst	0x05a36454	//zip2 z20.s,z2.s,z3.s

.inst	0x05a56095	//zip1 z21.s,z4.s,z5.s
.inst	0x05a56496	//zip2 z22.s,z4.s,z5.s
.inst	0x05a760d7	//zip1 z23.s,z6.s,z7.s
.inst	0x05a764d8	//zip2 z24.s,z6.s,z7.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36621	//zip2 z1.d,z17.d,z19.d
.inst	0x05f46242	//zip1 z2.d,z18.d,z20.d
.inst	0x05f46643	//zip2 z3.d,z18.d,z20.d

.inst	0x05f762a4	//zip1 z4.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c6	//zip1 z6.d,z22.d,z24.d
.inst	0x05f866c7	//zip2 z7.d,z22.d,z24.d
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.inst	0x05a96111	//zip1 z17.s,z8.s,z9.s
.inst	0x05a96512	//zip2 z18.s,z8.s,z9.s
.inst	0x05ab6153	//zip1 z19.s,z10.s,z11.s
.inst	0x05ab6554	//zip2 z20.s,z10.s,z11.s

.inst	0x05ad6195	//zip1 z21.s,z12.s,z13.s
.inst	0x05ad6596	//zip2 z22.s,z12.s,z13.s
.inst	0x05af61d7	//zip1 z23.s,z14.s,z15.s
.inst	0x05af65d8	//zip2 z24.s,z14.s,z15.s

.inst	0x05f36228	//zip1 z8.d,z17.d,z19.d
.inst	0x05f36629	//zip2 z9.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664b	//zip2 z11.d,z18.d,z20.d

.inst	0x05f762ac	//zip1 z12.d,z21.d,z23.d
.inst	0x05f766ad	//zip2 z13.d,z21.d,z23.d
.inst	0x05f862ce	//zip1 z14.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x11,x11,x12
.endif
.if	mixin == 1
	eor	x13,x13,x14
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23084	//eor z4.d,z4.d,z18.d
.inst	0x04b33108	//eor z8.d,z8.d,z19.d
.inst	0x04b4318c	//eor z12.d,z12.d,z20.d
.inst	0x04b53021	//eor z1.d,z1.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b73129	//eor z9.d,z9.d,z23.d
.inst	0x04b831ad	//eor z13.d,z13.d,z24.d
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13042	//eor z2.d,z2.d,z17.d
.inst	0x04b230c6	//eor z6.d,z6.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b431ce	//eor z14.d,z14.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b53063	//eor z3.d,z3.d,z21.d
.inst	0x04b630e7	//eor z7.d,z7.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b7316b	//eor z11.d,z11.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
#ifdef  __AARCH64EB__
.inst	0x05a48000	//revb z0.s,p0/m,z0.s
#endif
.inst	0xe540e000	//st1w {z0.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48084	//revb z4.s,p0/m,z4.s
#endif
.inst	0xe541e004	//st1w {z4.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48108	//revb z8.s,p0/m,z8.s
#endif
.inst	0xe542e008	//st1w {z8.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4818c	//revb z12.s,p0/m,z12.s
#endif
.inst	0xe543e00c	//st1w {z12.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48021	//revb z1.s,p0/m,z1.s
#endif
.inst	0xe544e001	//st1w {z1.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480a5	//revb z5.s,p0/m,z5.s
#endif
.inst	0xe545e005	//st1w {z5.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48129	//revb z9.s,p0/m,z9.s
#endif
.inst	0xe546e009	//st1w {z9.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ad	//revb z13.s,p0/m,z13.s
#endif
.inst	0xe547e00d	//st1w {z13.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
#ifdef  __AARCH64EB__
.inst	0x05a48042	//revb z2.s,p0/m,z2.s
#endif
.inst	0xe540e002	//st1w {z2.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480c6	//revb z6.s,p0/m,z6.s
#endif
.inst	0xe541e006	//st1w {z6.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4814a	//revb z10.s,p0/m,z10.s
#endif
.inst	0xe542e00a	//st1w {z10.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ce	//revb z14.s,p0/m,z14.s
#endif
.inst	0xe543e00e	//st1w {z14.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48063	//revb z3.s,p0/m,z3.s
#endif
.inst	0xe544e003	//st1w {z3.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480e7	//revb z7.s,p0/m,z7.s
#endif
.inst	0xe545e007	//st1w {z7.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4816b	//revb z11.s,p0/m,z11.s
#endif
.inst	0xe546e00b	//st1w {z11.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ef	//revb z15.s,p0/m,z15.s
#endif
.inst	0xe547e00f	//st1w {z15.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
210:
.inst	0x04b0e3fd	//incw x29, ALL, MUL #1
	subs	x2,x2,64
	b.gt	100b
	b	110f
101:
	mixin=0
	lsr	x8,x23,#32
.inst	0x05a03ae0	//dup z0.s,w23
.inst	0x05a03af9	//dup z25.s,w23
.if	mixin == 1
	mov	w7,w23
.endif
.inst	0x05a03904	//dup z4.s,w8
.inst	0x05a0391a	//dup z26.s,w8
	lsr	x10,x24,#32
.inst	0x05a03b08	//dup z8.s,w24
.inst	0x05a03b1b	//dup z27.s,w24
.if	mixin == 1
	mov	w9,w24
.endif
.inst	0x05a0394c	//dup z12.s,w10
.inst	0x05a0395c	//dup z28.s,w10
	lsr	x12,x25,#32
.inst	0x05a03b21	//dup z1.s,w25
.inst	0x05a03b3d	//dup z29.s,w25
.if	mixin == 1
	mov	w11,w25
.endif
.inst	0x05a03985	//dup z5.s,w12
.inst	0x05a0399e	//dup z30.s,w12
	lsr	x14,x26,#32
.inst	0x05a03b49	//dup z9.s,w26
.inst	0x05a03b55	//dup z21.s,w26
.if	mixin == 1
	mov	w13,w26
.endif
.inst	0x05a039cd	//dup z13.s,w14
.inst	0x05a039d6	//dup z22.s,w14
	lsr	x16,x27,#32
.inst	0x05a03b62	//dup z2.s,w27
.inst	0x05a03b77	//dup z23.s,w27
.if	mixin == 1
	mov	w15,w27
.endif
.inst	0x05a03a06	//dup z6.s,w16
.inst	0x05a03a18	//dup z24.s,w16
	lsr	x18,x28,#32
.inst	0x05a03b8a	//dup z10.s,w28
.inst	0x05a03b91	//dup z17.s,w28
.if	mixin == 1
	mov	w17,w28
.endif
.inst	0x05a03a4e	//dup z14.s,w18
.inst	0x05a03a52	//dup z18.s,w18
	lsr	x22,x30,#32
.inst	0x05a03bcb	//dup z11.s,w30
.inst	0x05a03bd4	//dup z20.s,w30
.if	mixin == 1
	mov	w21,w30
.endif
.inst	0x05a03acf	//dup z15.s,w22
.inst	0x05a03adf	//dup z31.s,w22
.if	mixin == 1
	add	w20,w29,#1
	mov	w19,w29
.inst	0x04a14690	//index z16.s,w20,1
.inst	0x04a14683	//index z3.s,w20,1
.else
.inst	0x04a147b0	//index z16.s,w29,1
.inst	0x04a147a3	//index z3.s,w29,1
.endif
	lsr	x20,x29,#32
.inst	0x05a03a87	//dup z7.s,w20
.inst	0x05a03a93	//dup z19.s,w20
	mov	x6,#10
10:
.align	5
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04703403	//xar z3.s,z3.s,z0.s,16
.if	mixin == 1
	ror	w19,w19,16
.endif
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04703487	//xar z7.s,z7.s,z4.s,16
.if	mixin == 1
	ror	w20,w20,16
.endif
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x0470350b	//xar z11.s,z11.s,z8.s,16
.if	mixin == 1
	ror	w21,w21,16
.endif
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x0470358f	//xar z15.s,z15.s,z12.s,16
.if	mixin == 1
	ror	w22,w22,16
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x046c3441	//xar z1.s,z1.s,z2.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x046c34c5	//xar z5.s,z5.s,z6.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x046c3549	//xar z9.s,z9.s,z10.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046c35cd	//xar z13.s,z13.s,z14.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04683403	//xar z3.s,z3.s,z0.s,24
.if	mixin == 1
	ror	w19,w19,24
.endif
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04683487	//xar z7.s,z7.s,z4.s,24
.if	mixin == 1
	ror	w20,w20,24
.endif
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x0468350b	//xar z11.s,z11.s,z8.s,24
.if	mixin == 1
	ror	w21,w21,24
.endif
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x0468358f	//xar z15.s,z15.s,z12.s,24
.if	mixin == 1
	ror	w22,w22,24
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04673441	//xar z1.s,z1.s,z2.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x046734c5	//xar z5.s,z5.s,z6.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04673549	//xar z9.s,z9.s,z10.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046735cd	//xar z13.s,z13.s,z14.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x0470340f	//xar z15.s,z15.s,z0.s,16
.if	mixin == 1
	ror	w22,w22,16
.endif
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04703483	//xar z3.s,z3.s,z4.s,16
.if	mixin == 1
	ror	w19,w19,16
.endif
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04703507	//xar z7.s,z7.s,z8.s,16
.if	mixin == 1
	ror	w20,w20,16
.endif
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x0470358b	//xar z11.s,z11.s,z12.s,16
.if	mixin == 1
	ror	w21,w21,16
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x046c3545	//xar z5.s,z5.s,z10.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x046c35c9	//xar z9.s,z9.s,z14.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x046c344d	//xar z13.s,z13.s,z2.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046c34c1	//xar z1.s,z1.s,z6.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x0468340f	//xar z15.s,z15.s,z0.s,24
.if	mixin == 1
	ror	w22,w22,24
.endif
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04683483	//xar z3.s,z3.s,z4.s,24
.if	mixin == 1
	ror	w19,w19,24
.endif
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04683507	//xar z7.s,z7.s,z8.s,24
.if	mixin == 1
	ror	w20,w20,24
.endif
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x0468358b	//xar z11.s,z11.s,z12.s,24
.if	mixin == 1
	ror	w21,w21,24
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04673545	//xar z5.s,z5.s,z10.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x046735c9	//xar z9.s,z9.s,z14.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x0467344d	//xar z13.s,z13.s,z2.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046734c1	//xar z1.s,z1.s,z6.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
	sub	x6,x6,1
	cbnz	x6,10b
.if	mixin == 1
	add	w7,w7,w23
.endif
.inst	0x04b90000	//add z0.s,z0.s,z25.s
.if	mixin == 1
	add	x8,x8,x23,lsr #32
.endif
.inst	0x04ba0084	//add z4.s,z4.s,z26.s
.if	mixin == 1
	add	x7,x7,x8,lsl #32  // pack
.endif
.if	mixin == 1
	add	w9,w9,w24
.endif
.inst	0x04bb0108	//add z8.s,z8.s,z27.s
.if	mixin == 1
	add	x10,x10,x24,lsr #32
.endif
.inst	0x04bc018c	//add z12.s,z12.s,z28.s
.if	mixin == 1
	add	x9,x9,x10,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x8,x10,[x1],#16
.endif
.if	mixin == 1
	add	w11,w11,w25
.endif
.inst	0x04bd0021	//add z1.s,z1.s,z29.s
.if	mixin == 1
	add	x12,x12,x25,lsr #32
.endif
.inst	0x04be00a5	//add z5.s,z5.s,z30.s
.if	mixin == 1
	add	x11,x11,x12,lsl #32  // pack
.endif
.if	mixin == 1
	add	w13,w13,w26
.endif
.inst	0x04b50129	//add z9.s,z9.s,z21.s
.if	mixin == 1
	add	x14,x14,x26,lsr #32
.endif
.inst	0x04b601ad	//add z13.s,z13.s,z22.s
.if	mixin == 1
	add	x13,x13,x14,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x12,x14,[x1],#16
.endif
.if	mixin == 1
	add	w15,w15,w27
.endif
.inst	0x04b70042	//add z2.s,z2.s,z23.s
.if	mixin == 1
	add	x16,x16,x27,lsr #32
.endif
.inst	0x04b800c6	//add z6.s,z6.s,z24.s
.if	mixin == 1
	add	x15,x15,x16,lsl #32  // pack
.endif
.if	mixin == 1
	add	w17,w17,w28
.endif
.inst	0x04b1014a	//add z10.s,z10.s,z17.s
.if	mixin == 1
	add	x18,x18,x28,lsr #32
.endif
.inst	0x04b201ce	//add z14.s,z14.s,z18.s
.if	mixin == 1
	add	x17,x17,x18,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x16,x18,[x1],#16
.endif
.if	mixin == 1
	add	w19,w19,w29
.endif
.inst	0x04b00063	//add z3.s,z3.s,z16.s
.if	mixin == 1
	add	x20,x20,x29,lsr #32
.endif
.inst	0x04b300e7	//add z7.s,z7.s,z19.s
.if	mixin == 1
	add	x19,x19,x20,lsl #32  // pack
.endif
.if	mixin == 1
	add	w21,w21,w30
.endif
.inst	0x04b4016b	//add z11.s,z11.s,z20.s
.if	mixin == 1
	add	x22,x22,x30,lsr #32
.endif
.inst	0x04bf01ef	//add z15.s,z15.s,z31.s
.if	mixin == 1
	add	x21,x21,x22,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x20,x22,[x1],#16
.endif
#ifdef	__AARCH64EB__
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x19,x19
	rev	x21,x21
#endif
.if	mixin == 1
	add	x29,x29,#1
.endif
	cmp	x5,4
	b.ne	200f
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.if	mixin == 1
	eor	x11,x11,x12
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x13,x13,x14
.endif
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23021	//eor z1.d,z1.d,z18.d
.inst	0x04b33042	//eor z2.d,z2.d,z19.d
.inst	0x04b43063	//eor z3.d,z3.d,z20.d
.inst	0x04b53084	//eor z4.d,z4.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b730c6	//eor z6.d,z6.d,z23.d
.inst	0x04b830e7	//eor z7.d,z7.d,z24.d
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13108	//eor z8.d,z8.d,z17.d
.inst	0x04b23129	//eor z9.d,z9.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b4316b	//eor z11.d,z11.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b5318c	//eor z12.d,z12.d,z21.d
.inst	0x04b631ad	//eor z13.d,z13.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b731ce	//eor z14.d,z14.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x0],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	st1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	st1	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	b	210f
200:
.inst	0x05a16011	//zip1 z17.s,z0.s,z1.s
.inst	0x05a16412	//zip2 z18.s,z0.s,z1.s
.inst	0x05a36053	//zip1 z19.s,z2.s,z3.s
.inst	0x05a36454	//zip2 z20.s,z2.s,z3.s

.inst	0x05a56095	//zip1 z21.s,z4.s,z5.s
.inst	0x05a56496	//zip2 z22.s,z4.s,z5.s
.inst	0x05a760d7	//zip1 z23.s,z6.s,z7.s
.inst	0x05a764d8	//zip2 z24.s,z6.s,z7.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36621	//zip2 z1.d,z17.d,z19.d
.inst	0x05f46242	//zip1 z2.d,z18.d,z20.d
.inst	0x05f46643	//zip2 z3.d,z18.d,z20.d

.inst	0x05f762a4	//zip1 z4.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c6	//zip1 z6.d,z22.d,z24.d
.inst	0x05f866c7	//zip2 z7.d,z22.d,z24.d
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.inst	0x05a96111	//zip1 z17.s,z8.s,z9.s
.inst	0x05a96512	//zip2 z18.s,z8.s,z9.s
.inst	0x05ab6153	//zip1 z19.s,z10.s,z11.s
.inst	0x05ab6554	//zip2 z20.s,z10.s,z11.s

.inst	0x05ad6195	//zip1 z21.s,z12.s,z13.s
.inst	0x05ad6596	//zip2 z22.s,z12.s,z13.s
.inst	0x05af61d7	//zip1 z23.s,z14.s,z15.s
.inst	0x05af65d8	//zip2 z24.s,z14.s,z15.s

.inst	0x05f36228	//zip1 z8.d,z17.d,z19.d
.inst	0x05f36629	//zip2 z9.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664b	//zip2 z11.d,z18.d,z20.d

.inst	0x05f762ac	//zip1 z12.d,z21.d,z23.d
.inst	0x05f766ad	//zip2 z13.d,z21.d,z23.d
.inst	0x05f862ce	//zip1 z14.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x11,x11,x12
.endif
.if	mixin == 1
	eor	x13,x13,x14
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23084	//eor z4.d,z4.d,z18.d
.inst	0x04b33108	//eor z8.d,z8.d,z19.d
.inst	0x04b4318c	//eor z12.d,z12.d,z20.d
.inst	0x04b53021	//eor z1.d,z1.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b73129	//eor z9.d,z9.d,z23.d
.inst	0x04b831ad	//eor z13.d,z13.d,z24.d
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13042	//eor z2.d,z2.d,z17.d
.inst	0x04b230c6	//eor z6.d,z6.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b431ce	//eor z14.d,z14.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b53063	//eor z3.d,z3.d,z21.d
.inst	0x04b630e7	//eor z7.d,z7.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b7316b	//eor z11.d,z11.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
#ifdef  __AARCH64EB__
.inst	0x05a48000	//revb z0.s,p0/m,z0.s
#endif
.inst	0xe540e000	//st1w {z0.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48084	//revb z4.s,p0/m,z4.s
#endif
.inst	0xe541e004	//st1w {z4.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48108	//revb z8.s,p0/m,z8.s
#endif
.inst	0xe542e008	//st1w {z8.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4818c	//revb z12.s,p0/m,z12.s
#endif
.inst	0xe543e00c	//st1w {z12.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48021	//revb z1.s,p0/m,z1.s
#endif
.inst	0xe544e001	//st1w {z1.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480a5	//revb z5.s,p0/m,z5.s
#endif
.inst	0xe545e005	//st1w {z5.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48129	//revb z9.s,p0/m,z9.s
#endif
.inst	0xe546e009	//st1w {z9.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ad	//revb z13.s,p0/m,z13.s
#endif
.inst	0xe547e00d	//st1w {z13.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
#ifdef  __AARCH64EB__
.inst	0x05a48042	//revb z2.s,p0/m,z2.s
#endif
.inst	0xe540e002	//st1w {z2.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480c6	//revb z6.s,p0/m,z6.s
#endif
.inst	0xe541e006	//st1w {z6.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4814a	//revb z10.s,p0/m,z10.s
#endif
.inst	0xe542e00a	//st1w {z10.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ce	//revb z14.s,p0/m,z14.s
#endif
.inst	0xe543e00e	//st1w {z14.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48063	//revb z3.s,p0/m,z3.s
#endif
.inst	0xe544e003	//st1w {z3.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480e7	//revb z7.s,p0/m,z7.s
#endif
.inst	0xe545e007	//st1w {z7.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4816b	//revb z11.s,p0/m,z11.s
#endif
.inst	0xe546e00b	//st1w {z11.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ef	//revb z15.s,p0/m,z15.s
#endif
.inst	0xe547e00f	//st1w {z15.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
210:
.inst	0x04b0e3fd	//incw x29, ALL, MUL #1
110:
	b	2f
1:
.align	5
100:
	subs	x7,x2,x5,lsl #6
	b.lt	110f
	mov	x2,x7
	b.eq	101f
	cmp	x2,64
	b.lt	101f
	mixin=1
	lsr	x8,x23,#32
.inst	0x05a03ae0	//dup z0.s,w23
.inst	0x05a03af9	//dup z25.s,w23
.if	mixin == 1
	mov	w7,w23
.endif
.inst	0x05a03904	//dup z4.s,w8
.inst	0x05a0391a	//dup z26.s,w8
	lsr	x10,x24,#32
.inst	0x05a03b08	//dup z8.s,w24
.inst	0x05a03b1b	//dup z27.s,w24
.if	mixin == 1
	mov	w9,w24
.endif
.inst	0x05a0394c	//dup z12.s,w10
.inst	0x05a0395c	//dup z28.s,w10
	lsr	x12,x25,#32
.inst	0x05a03b21	//dup z1.s,w25
.inst	0x05a03b3d	//dup z29.s,w25
.if	mixin == 1
	mov	w11,w25
.endif
.inst	0x05a03985	//dup z5.s,w12
.inst	0x05a0399e	//dup z30.s,w12
	lsr	x14,x26,#32
.inst	0x05a03b49	//dup z9.s,w26
.inst	0x05a03b55	//dup z21.s,w26
.if	mixin == 1
	mov	w13,w26
.endif
.inst	0x05a039cd	//dup z13.s,w14
.inst	0x05a039d6	//dup z22.s,w14
	lsr	x16,x27,#32
.inst	0x05a03b62	//dup z2.s,w27
.inst	0x05a03b77	//dup z23.s,w27
.if	mixin == 1
	mov	w15,w27
.endif
.inst	0x05a03a06	//dup z6.s,w16
.inst	0x05a03a18	//dup z24.s,w16
	lsr	x18,x28,#32
.inst	0x05a03b8a	//dup z10.s,w28
.if	mixin == 1
	mov	w17,w28
.endif
.inst	0x05a03a4e	//dup z14.s,w18
	lsr	x22,x30,#32
.inst	0x05a03bcb	//dup z11.s,w30
.if	mixin == 1
	mov	w21,w30
.endif
.inst	0x05a03acf	//dup z15.s,w22
.if	mixin == 1
	add	w20,w29,#1
	mov	w19,w29
.inst	0x04a14690	//index z16.s,w20,1
.inst	0x04a14683	//index z3.s,w20,1
.else
.inst	0x04a147b0	//index z16.s,w29,1
.inst	0x04a147a3	//index z3.s,w29,1
.endif
	lsr	x20,x29,#32
.inst	0x05a03a87	//dup z7.s,w20
	mov	x6,#10
10:
.align	5
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.inst	0x04a03063	//eor z3.d,z3.d,z0.d
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04a430e7	//eor z7.d,z7.d,z4.d
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04a8316b	//eor z11.d,z11.d,z8.d
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x04ac31ef	//eor z15.d,z15.d,z12.d
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x05a58063	//revh z3.s,p0/m,z3.s
.if	mixin == 1
	ror	w19,w19,#16
.endif
.inst	0x05a580e7	//revh z7.s,p0/m,z7.s
.if	mixin == 1
	ror	w20,w20,#16
.endif
.inst	0x05a5816b	//revh z11.s,p0/m,z11.s
.if	mixin == 1
	ror	w21,w21,#16
.endif
.inst	0x05a581ef	//revh z15.s,p0/m,z15.s
.if	mixin == 1
	ror	w22,w22,#16
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.inst	0x04a23021	//eor z1.d,z1.d,z2.d
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04a630a5	//eor z5.d,z5.d,z6.d
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x04aa3129	//eor z9.d,z9.d,z10.d
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04ae31ad	//eor z13.d,z13.d,z14.d
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046c9c31	//lsl z17.s,z1.s,12
.inst	0x046c9cb2	//lsl z18.s,z5.s,12
.inst	0x046c9d33	//lsl z19.s,z9.s,12
.inst	0x046c9db4	//lsl z20.s,z13.s,12
.inst	0x046c9421	//lsr z1.s,z1.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x046c94a5	//lsr z5.s,z5.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.inst	0x046c9529	//lsr z9.s,z9.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.inst	0x046c95ad	//lsr z13.s,z13.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x04713021	//orr z1.d,z1.d,z17.d
.inst	0x047230a5	//orr z5.d,z5.d,z18.d
.inst	0x04733129	//orr z9.d,z9.d,z19.d
.inst	0x047431ad	//orr z13.d,z13.d,z20.d
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.inst	0x04a03063	//eor z3.d,z3.d,z0.d
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04a430e7	//eor z7.d,z7.d,z4.d
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04a8316b	//eor z11.d,z11.d,z8.d
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x04ac31ef	//eor z15.d,z15.d,z12.d
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x053f3063	//tbl z3.b,{z3.b},z31.b
.if	mixin == 1
	ror	w19,w19,#24
.endif
.inst	0x053f30e7	//tbl z7.b,{z7.b},z31.b
.if	mixin == 1
	ror	w20,w20,#24
.endif
.inst	0x053f316b	//tbl z11.b,{z11.b},z31.b
.if	mixin == 1
	ror	w21,w21,#24
.endif
.inst	0x053f31ef	//tbl z15.b,{z15.b},z31.b
.if	mixin == 1
	ror	w22,w22,#24
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.inst	0x04a23021	//eor z1.d,z1.d,z2.d
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04a630a5	//eor z5.d,z5.d,z6.d
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x04aa3129	//eor z9.d,z9.d,z10.d
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04ae31ad	//eor z13.d,z13.d,z14.d
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x04679c31	//lsl z17.s,z1.s,7
.inst	0x04679cb2	//lsl z18.s,z5.s,7
.inst	0x04679d33	//lsl z19.s,z9.s,7
.inst	0x04679db4	//lsl z20.s,z13.s,7
.inst	0x04679421	//lsr z1.s,z1.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.inst	0x046794a5	//lsr z5.s,z5.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.inst	0x04679529	//lsr z9.s,z9.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.inst	0x046795ad	//lsr z13.s,z13.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04713021	//orr z1.d,z1.d,z17.d
.inst	0x047230a5	//orr z5.d,z5.d,z18.d
.inst	0x04733129	//orr z9.d,z9.d,z19.d
.inst	0x047431ad	//orr z13.d,z13.d,z20.d
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.inst	0x04a031ef	//eor z15.d,z15.d,z0.d
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x04a43063	//eor z3.d,z3.d,z4.d
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04a830e7	//eor z7.d,z7.d,z8.d
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04ac316b	//eor z11.d,z11.d,z12.d
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x05a581ef	//revh z15.s,p0/m,z15.s
.if	mixin == 1
	ror	w22,w22,#16
.endif
.inst	0x05a58063	//revh z3.s,p0/m,z3.s
.if	mixin == 1
	ror	w19,w19,#16
.endif
.inst	0x05a580e7	//revh z7.s,p0/m,z7.s
.if	mixin == 1
	ror	w20,w20,#16
.endif
.inst	0x05a5816b	//revh z11.s,p0/m,z11.s
.if	mixin == 1
	ror	w21,w21,#16
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.inst	0x04aa30a5	//eor z5.d,z5.d,z10.d
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04ae3129	//eor z9.d,z9.d,z14.d
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x04a231ad	//eor z13.d,z13.d,z2.d
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x04a63021	//eor z1.d,z1.d,z6.d
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046c9cb1	//lsl z17.s,z5.s,12
.inst	0x046c9d32	//lsl z18.s,z9.s,12
.inst	0x046c9db3	//lsl z19.s,z13.s,12
.inst	0x046c9c34	//lsl z20.s,z1.s,12
.inst	0x046c94a5	//lsr z5.s,z5.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.inst	0x046c9529	//lsr z9.s,z9.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.inst	0x046c95ad	//lsr z13.s,z13.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x046c9421	//lsr z1.s,z1.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x047130a5	//orr z5.d,z5.d,z17.d
.inst	0x04723129	//orr z9.d,z9.d,z18.d
.inst	0x047331ad	//orr z13.d,z13.d,z19.d
.inst	0x04743021	//orr z1.d,z1.d,z20.d
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.inst	0x04a031ef	//eor z15.d,z15.d,z0.d
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x04a43063	//eor z3.d,z3.d,z4.d
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04a830e7	//eor z7.d,z7.d,z8.d
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04ac316b	//eor z11.d,z11.d,z12.d
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x053f31ef	//tbl z15.b,{z15.b},z31.b
.if	mixin == 1
	ror	w22,w22,#24
.endif
.inst	0x053f3063	//tbl z3.b,{z3.b},z31.b
.if	mixin == 1
	ror	w19,w19,#24
.endif
.inst	0x053f30e7	//tbl z7.b,{z7.b},z31.b
.if	mixin == 1
	ror	w20,w20,#24
.endif
.inst	0x053f316b	//tbl z11.b,{z11.b},z31.b
.if	mixin == 1
	ror	w21,w21,#24
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.inst	0x04aa30a5	//eor z5.d,z5.d,z10.d
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04ae3129	//eor z9.d,z9.d,z14.d
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x04a231ad	//eor z13.d,z13.d,z2.d
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x04a63021	//eor z1.d,z1.d,z6.d
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x04679cb1	//lsl z17.s,z5.s,7
.inst	0x04679d32	//lsl z18.s,z9.s,7
.inst	0x04679db3	//lsl z19.s,z13.s,7
.inst	0x04679c34	//lsl z20.s,z1.s,7
.inst	0x046794a5	//lsr z5.s,z5.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.inst	0x04679529	//lsr z9.s,z9.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.inst	0x046795ad	//lsr z13.s,z13.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04679421	//lsr z1.s,z1.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.inst	0x047130a5	//orr z5.d,z5.d,z17.d
.inst	0x04723129	//orr z9.d,z9.d,z18.d
.inst	0x047331ad	//orr z13.d,z13.d,z19.d
.inst	0x04743021	//orr z1.d,z1.d,z20.d
	sub	x6,x6,1
	cbnz	x6,10b
	lsr	x6,x28,#32
.inst	0x05a03b91	//dup z17.s,w28
.inst	0x05a038d2	//dup z18.s,w6
	lsr	x6,x29,#32
.inst	0x05a038d3	//dup z19.s,w6
	lsr	x6,x30,#32
.if	mixin == 1
	add	w7,w7,w23
.endif
.inst	0x04b90000	//add z0.s,z0.s,z25.s
.if	mixin == 1
	add	x8,x8,x23,lsr #32
.endif
.inst	0x04ba0084	//add z4.s,z4.s,z26.s
.if	mixin == 1
	add	x7,x7,x8,lsl #32  // pack
.endif
.if	mixin == 1
	add	w9,w9,w24
.endif
.inst	0x04bb0108	//add z8.s,z8.s,z27.s
.if	mixin == 1
	add	x10,x10,x24,lsr #32
.endif
.inst	0x04bc018c	//add z12.s,z12.s,z28.s
.if	mixin == 1
	add	x9,x9,x10,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x8,x10,[x1],#16
.endif
.if	mixin == 1
	add	w11,w11,w25
.endif
.inst	0x04bd0021	//add z1.s,z1.s,z29.s
.if	mixin == 1
	add	x12,x12,x25,lsr #32
.endif
.inst	0x04be00a5	//add z5.s,z5.s,z30.s
.if	mixin == 1
	add	x11,x11,x12,lsl #32  // pack
.endif
.if	mixin == 1
	add	w13,w13,w26
.endif
.inst	0x04b50129	//add z9.s,z9.s,z21.s
.if	mixin == 1
	add	x14,x14,x26,lsr #32
.endif
.inst	0x04b601ad	//add z13.s,z13.s,z22.s
.if	mixin == 1
	add	x13,x13,x14,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x12,x14,[x1],#16
.endif
.if	mixin == 1
	add	w15,w15,w27
.endif
.inst	0x04b70042	//add z2.s,z2.s,z23.s
.if	mixin == 1
	add	x16,x16,x27,lsr #32
.endif
.inst	0x04b800c6	//add z6.s,z6.s,z24.s
.if	mixin == 1
	add	x15,x15,x16,lsl #32  // pack
.endif
.if	mixin == 1
	add	w17,w17,w28
.endif
.inst	0x04b1014a	//add z10.s,z10.s,z17.s
.if	mixin == 1
	add	x18,x18,x28,lsr #32
.endif
.inst	0x04b201ce	//add z14.s,z14.s,z18.s
.if	mixin == 1
	add	x17,x17,x18,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x16,x18,[x1],#16
.endif
.inst	0x05a03bd4	//dup z20.s,w30
.inst	0x05a038d9	//dup z25.s,w6	// bak[15] not available for SVE
.if	mixin == 1
	add	w19,w19,w29
.endif
.inst	0x04b00063	//add z3.s,z3.s,z16.s
.if	mixin == 1
	add	x20,x20,x29,lsr #32
.endif
.inst	0x04b300e7	//add z7.s,z7.s,z19.s
.if	mixin == 1
	add	x19,x19,x20,lsl #32  // pack
.endif
.if	mixin == 1
	add	w21,w21,w30
.endif
.inst	0x04b4016b	//add z11.s,z11.s,z20.s
.if	mixin == 1
	add	x22,x22,x30,lsr #32
.endif
.inst	0x04b901ef	//add z15.s,z15.s,z25.s
.if	mixin == 1
	add	x21,x21,x22,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x20,x22,[x1],#16
.endif
#ifdef	__AARCH64EB__
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x19,x19
	rev	x21,x21
#endif
.if	mixin == 1
	add	x29,x29,#1
.endif
	cmp	x5,4
	b.ne	200f
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.if	mixin == 1
	eor	x11,x11,x12
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x13,x13,x14
.endif
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23021	//eor z1.d,z1.d,z18.d
.inst	0x04b33042	//eor z2.d,z2.d,z19.d
.inst	0x04b43063	//eor z3.d,z3.d,z20.d
.inst	0x04b53084	//eor z4.d,z4.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b730c6	//eor z6.d,z6.d,z23.d
.inst	0x04b830e7	//eor z7.d,z7.d,z24.d
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13108	//eor z8.d,z8.d,z17.d
.inst	0x04b23129	//eor z9.d,z9.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b4316b	//eor z11.d,z11.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b5318c	//eor z12.d,z12.d,z21.d
.inst	0x04b631ad	//eor z13.d,z13.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b731ce	//eor z14.d,z14.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x0],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	st1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	st1	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	b	210f
200:
.inst	0x05a16011	//zip1 z17.s,z0.s,z1.s
.inst	0x05a16412	//zip2 z18.s,z0.s,z1.s
.inst	0x05a36053	//zip1 z19.s,z2.s,z3.s
.inst	0x05a36454	//zip2 z20.s,z2.s,z3.s

.inst	0x05a56095	//zip1 z21.s,z4.s,z5.s
.inst	0x05a56496	//zip2 z22.s,z4.s,z5.s
.inst	0x05a760d7	//zip1 z23.s,z6.s,z7.s
.inst	0x05a764d8	//zip2 z24.s,z6.s,z7.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36621	//zip2 z1.d,z17.d,z19.d
.inst	0x05f46242	//zip1 z2.d,z18.d,z20.d
.inst	0x05f46643	//zip2 z3.d,z18.d,z20.d

.inst	0x05f762a4	//zip1 z4.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c6	//zip1 z6.d,z22.d,z24.d
.inst	0x05f866c7	//zip2 z7.d,z22.d,z24.d
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.inst	0x05a96111	//zip1 z17.s,z8.s,z9.s
.inst	0x05a96512	//zip2 z18.s,z8.s,z9.s
.inst	0x05ab6153	//zip1 z19.s,z10.s,z11.s
.inst	0x05ab6554	//zip2 z20.s,z10.s,z11.s

.inst	0x05ad6195	//zip1 z21.s,z12.s,z13.s
.inst	0x05ad6596	//zip2 z22.s,z12.s,z13.s
.inst	0x05af61d7	//zip1 z23.s,z14.s,z15.s
.inst	0x05af65d8	//zip2 z24.s,z14.s,z15.s

.inst	0x05f36228	//zip1 z8.d,z17.d,z19.d
.inst	0x05f36629	//zip2 z9.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664b	//zip2 z11.d,z18.d,z20.d

.inst	0x05f762ac	//zip1 z12.d,z21.d,z23.d
.inst	0x05f766ad	//zip2 z13.d,z21.d,z23.d
.inst	0x05f862ce	//zip1 z14.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x11,x11,x12
.endif
.if	mixin == 1
	eor	x13,x13,x14
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23084	//eor z4.d,z4.d,z18.d
.inst	0x04b33108	//eor z8.d,z8.d,z19.d
.inst	0x04b4318c	//eor z12.d,z12.d,z20.d
.inst	0x04b53021	//eor z1.d,z1.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b73129	//eor z9.d,z9.d,z23.d
.inst	0x04b831ad	//eor z13.d,z13.d,z24.d
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13042	//eor z2.d,z2.d,z17.d
.inst	0x04b230c6	//eor z6.d,z6.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b431ce	//eor z14.d,z14.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b53063	//eor z3.d,z3.d,z21.d
.inst	0x04b630e7	//eor z7.d,z7.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b7316b	//eor z11.d,z11.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
#ifdef  __AARCH64EB__
.inst	0x05a48000	//revb z0.s,p0/m,z0.s
#endif
.inst	0xe540e000	//st1w {z0.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48084	//revb z4.s,p0/m,z4.s
#endif
.inst	0xe541e004	//st1w {z4.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48108	//revb z8.s,p0/m,z8.s
#endif
.inst	0xe542e008	//st1w {z8.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4818c	//revb z12.s,p0/m,z12.s
#endif
.inst	0xe543e00c	//st1w {z12.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48021	//revb z1.s,p0/m,z1.s
#endif
.inst	0xe544e001	//st1w {z1.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480a5	//revb z5.s,p0/m,z5.s
#endif
.inst	0xe545e005	//st1w {z5.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48129	//revb z9.s,p0/m,z9.s
#endif
.inst	0xe546e009	//st1w {z9.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ad	//revb z13.s,p0/m,z13.s
#endif
.inst	0xe547e00d	//st1w {z13.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
#ifdef  __AARCH64EB__
.inst	0x05a48042	//revb z2.s,p0/m,z2.s
#endif
.inst	0xe540e002	//st1w {z2.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480c6	//revb z6.s,p0/m,z6.s
#endif
.inst	0xe541e006	//st1w {z6.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4814a	//revb z10.s,p0/m,z10.s
#endif
.inst	0xe542e00a	//st1w {z10.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ce	//revb z14.s,p0/m,z14.s
#endif
.inst	0xe543e00e	//st1w {z14.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48063	//revb z3.s,p0/m,z3.s
#endif
.inst	0xe544e003	//st1w {z3.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480e7	//revb z7.s,p0/m,z7.s
#endif
.inst	0xe545e007	//st1w {z7.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4816b	//revb z11.s,p0/m,z11.s
#endif
.inst	0xe546e00b	//st1w {z11.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ef	//revb z15.s,p0/m,z15.s
#endif
.inst	0xe547e00f	//st1w {z15.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
210:
.inst	0x04b0e3fd	//incw x29, ALL, MUL #1
	subs	x2,x2,64
	b.gt	100b
	b	110f
101:
	mixin=0
	lsr	x8,x23,#32
.inst	0x05a03ae0	//dup z0.s,w23
.inst	0x05a03af9	//dup z25.s,w23
.if	mixin == 1
	mov	w7,w23
.endif
.inst	0x05a03904	//dup z4.s,w8
.inst	0x05a0391a	//dup z26.s,w8
	lsr	x10,x24,#32
.inst	0x05a03b08	//dup z8.s,w24
.inst	0x05a03b1b	//dup z27.s,w24
.if	mixin == 1
	mov	w9,w24
.endif
.inst	0x05a0394c	//dup z12.s,w10
.inst	0x05a0395c	//dup z28.s,w10
	lsr	x12,x25,#32
.inst	0x05a03b21	//dup z1.s,w25
.inst	0x05a03b3d	//dup z29.s,w25
.if	mixin == 1
	mov	w11,w25
.endif
.inst	0x05a03985	//dup z5.s,w12
.inst	0x05a0399e	//dup z30.s,w12
	lsr	x14,x26,#32
.inst	0x05a03b49	//dup z9.s,w26
.inst	0x05a03b55	//dup z21.s,w26
.if	mixin == 1
	mov	w13,w26
.endif
.inst	0x05a039cd	//dup z13.s,w14
.inst	0x05a039d6	//dup z22.s,w14
	lsr	x16,x27,#32
.inst	0x05a03b62	//dup z2.s,w27
.inst	0x05a03b77	//dup z23.s,w27
.if	mixin == 1
	mov	w15,w27
.endif
.inst	0x05a03a06	//dup z6.s,w16
.inst	0x05a03a18	//dup z24.s,w16
	lsr	x18,x28,#32
.inst	0x05a03b8a	//dup z10.s,w28
.if	mixin == 1
	mov	w17,w28
.endif
.inst	0x05a03a4e	//dup z14.s,w18
	lsr	x22,x30,#32
.inst	0x05a03bcb	//dup z11.s,w30
.if	mixin == 1
	mov	w21,w30
.endif
.inst	0x05a03acf	//dup z15.s,w22
.if	mixin == 1
	add	w20,w29,#1
	mov	w19,w29
.inst	0x04a14690	//index z16.s,w20,1
.inst	0x04a14683	//index z3.s,w20,1
.else
.inst	0x04a147b0	//index z16.s,w29,1
.inst	0x04a147a3	//index z3.s,w29,1
.endif
	lsr	x20,x29,#32
.inst	0x05a03a87	//dup z7.s,w20
	mov	x6,#10
10:
.align	5
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.inst	0x04a03063	//eor z3.d,z3.d,z0.d
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04a430e7	//eor z7.d,z7.d,z4.d
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04a8316b	//eor z11.d,z11.d,z8.d
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x04ac31ef	//eor z15.d,z15.d,z12.d
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x05a58063	//revh z3.s,p0/m,z3.s
.if	mixin == 1
	ror	w19,w19,#16
.endif
.inst	0x05a580e7	//revh z7.s,p0/m,z7.s
.if	mixin == 1
	ror	w20,w20,#16
.endif
.inst	0x05a5816b	//revh z11.s,p0/m,z11.s
.if	mixin == 1
	ror	w21,w21,#16
.endif
.inst	0x05a581ef	//revh z15.s,p0/m,z15.s
.if	mixin == 1
	ror	w22,w22,#16
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.inst	0x04a23021	//eor z1.d,z1.d,z2.d
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04a630a5	//eor z5.d,z5.d,z6.d
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x04aa3129	//eor z9.d,z9.d,z10.d
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04ae31ad	//eor z13.d,z13.d,z14.d
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x046c9c31	//lsl z17.s,z1.s,12
.inst	0x046c9cb2	//lsl z18.s,z5.s,12
.inst	0x046c9d33	//lsl z19.s,z9.s,12
.inst	0x046c9db4	//lsl z20.s,z13.s,12
.inst	0x046c9421	//lsr z1.s,z1.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x046c94a5	//lsr z5.s,z5.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.inst	0x046c9529	//lsr z9.s,z9.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.inst	0x046c95ad	//lsr z13.s,z13.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x04713021	//orr z1.d,z1.d,z17.d
.inst	0x047230a5	//orr z5.d,z5.d,z18.d
.inst	0x04733129	//orr z9.d,z9.d,z19.d
.inst	0x047431ad	//orr z13.d,z13.d,z20.d
.inst	0x04a10000	//add z0.s,z0.s,z1.s
.if	mixin == 1
	add	w7,w7,w11
.endif
.inst	0x04a50084	//add z4.s,z4.s,z5.s
.if	mixin == 1
	add	w8,w8,w12
.endif
.inst	0x04a90108	//add z8.s,z8.s,z9.s
.if	mixin == 1
	add	w9,w9,w13
.endif
.inst	0x04ad018c	//add z12.s,z12.s,z13.s
.if	mixin == 1
	add	w10,w10,w14
.endif
.inst	0x04a03063	//eor z3.d,z3.d,z0.d
.if	mixin == 1
	eor	w19,w19,w7
.endif
.inst	0x04a430e7	//eor z7.d,z7.d,z4.d
.if	mixin == 1
	eor	w20,w20,w8
.endif
.inst	0x04a8316b	//eor z11.d,z11.d,z8.d
.if	mixin == 1
	eor	w21,w21,w9
.endif
.inst	0x04ac31ef	//eor z15.d,z15.d,z12.d
.if	mixin == 1
	eor	w22,w22,w10
.endif
.inst	0x053f3063	//tbl z3.b,{z3.b},z31.b
.if	mixin == 1
	ror	w19,w19,#24
.endif
.inst	0x053f30e7	//tbl z7.b,{z7.b},z31.b
.if	mixin == 1
	ror	w20,w20,#24
.endif
.inst	0x053f316b	//tbl z11.b,{z11.b},z31.b
.if	mixin == 1
	ror	w21,w21,#24
.endif
.inst	0x053f31ef	//tbl z15.b,{z15.b},z31.b
.if	mixin == 1
	ror	w22,w22,#24
.endif
.inst	0x04a30042	//add z2.s,z2.s,z3.s
.if	mixin == 1
	add	w15,w15,w19
.endif
.inst	0x04a700c6	//add z6.s,z6.s,z7.s
.if	mixin == 1
	add	w16,w16,w20
.endif
.inst	0x04ab014a	//add z10.s,z10.s,z11.s
.if	mixin == 1
	add	w17,w17,w21
.endif
.inst	0x04af01ce	//add z14.s,z14.s,z15.s
.if	mixin == 1
	add	w18,w18,w22
.endif
.inst	0x04a23021	//eor z1.d,z1.d,z2.d
.if	mixin == 1
	eor	w11,w11,w15
.endif
.inst	0x04a630a5	//eor z5.d,z5.d,z6.d
.if	mixin == 1
	eor	w12,w12,w16
.endif
.inst	0x04aa3129	//eor z9.d,z9.d,z10.d
.if	mixin == 1
	eor	w13,w13,w17
.endif
.inst	0x04ae31ad	//eor z13.d,z13.d,z14.d
.if	mixin == 1
	eor	w14,w14,w18
.endif
.inst	0x04679c31	//lsl z17.s,z1.s,7
.inst	0x04679cb2	//lsl z18.s,z5.s,7
.inst	0x04679d33	//lsl z19.s,z9.s,7
.inst	0x04679db4	//lsl z20.s,z13.s,7
.inst	0x04679421	//lsr z1.s,z1.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.inst	0x046794a5	//lsr z5.s,z5.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.inst	0x04679529	//lsr z9.s,z9.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.inst	0x046795ad	//lsr z13.s,z13.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04713021	//orr z1.d,z1.d,z17.d
.inst	0x047230a5	//orr z5.d,z5.d,z18.d
.inst	0x04733129	//orr z9.d,z9.d,z19.d
.inst	0x047431ad	//orr z13.d,z13.d,z20.d
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.inst	0x04a031ef	//eor z15.d,z15.d,z0.d
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x04a43063	//eor z3.d,z3.d,z4.d
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04a830e7	//eor z7.d,z7.d,z8.d
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04ac316b	//eor z11.d,z11.d,z12.d
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x05a581ef	//revh z15.s,p0/m,z15.s
.if	mixin == 1
	ror	w22,w22,#16
.endif
.inst	0x05a58063	//revh z3.s,p0/m,z3.s
.if	mixin == 1
	ror	w19,w19,#16
.endif
.inst	0x05a580e7	//revh z7.s,p0/m,z7.s
.if	mixin == 1
	ror	w20,w20,#16
.endif
.inst	0x05a5816b	//revh z11.s,p0/m,z11.s
.if	mixin == 1
	ror	w21,w21,#16
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.inst	0x04aa30a5	//eor z5.d,z5.d,z10.d
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04ae3129	//eor z9.d,z9.d,z14.d
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x04a231ad	//eor z13.d,z13.d,z2.d
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x04a63021	//eor z1.d,z1.d,z6.d
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x046c9cb1	//lsl z17.s,z5.s,12
.inst	0x046c9d32	//lsl z18.s,z9.s,12
.inst	0x046c9db3	//lsl z19.s,z13.s,12
.inst	0x046c9c34	//lsl z20.s,z1.s,12
.inst	0x046c94a5	//lsr z5.s,z5.s,20
.if	mixin == 1
	ror	w12,w12,20
.endif
.inst	0x046c9529	//lsr z9.s,z9.s,20
.if	mixin == 1
	ror	w13,w13,20
.endif
.inst	0x046c95ad	//lsr z13.s,z13.s,20
.if	mixin == 1
	ror	w14,w14,20
.endif
.inst	0x046c9421	//lsr z1.s,z1.s,20
.if	mixin == 1
	ror	w11,w11,20
.endif
.inst	0x047130a5	//orr z5.d,z5.d,z17.d
.inst	0x04723129	//orr z9.d,z9.d,z18.d
.inst	0x047331ad	//orr z13.d,z13.d,z19.d
.inst	0x04743021	//orr z1.d,z1.d,z20.d
.inst	0x04a50000	//add z0.s,z0.s,z5.s
.if	mixin == 1
	add	w7,w7,w12
.endif
.inst	0x04a90084	//add z4.s,z4.s,z9.s
.if	mixin == 1
	add	w8,w8,w13
.endif
.inst	0x04ad0108	//add z8.s,z8.s,z13.s
.if	mixin == 1
	add	w9,w9,w14
.endif
.inst	0x04a1018c	//add z12.s,z12.s,z1.s
.if	mixin == 1
	add	w10,w10,w11
.endif
.inst	0x04a031ef	//eor z15.d,z15.d,z0.d
.if	mixin == 1
	eor	w22,w22,w7
.endif
.inst	0x04a43063	//eor z3.d,z3.d,z4.d
.if	mixin == 1
	eor	w19,w19,w8
.endif
.inst	0x04a830e7	//eor z7.d,z7.d,z8.d
.if	mixin == 1
	eor	w20,w20,w9
.endif
.inst	0x04ac316b	//eor z11.d,z11.d,z12.d
.if	mixin == 1
	eor	w21,w21,w10
.endif
.inst	0x053f31ef	//tbl z15.b,{z15.b},z31.b
.if	mixin == 1
	ror	w22,w22,#24
.endif
.inst	0x053f3063	//tbl z3.b,{z3.b},z31.b
.if	mixin == 1
	ror	w19,w19,#24
.endif
.inst	0x053f30e7	//tbl z7.b,{z7.b},z31.b
.if	mixin == 1
	ror	w20,w20,#24
.endif
.inst	0x053f316b	//tbl z11.b,{z11.b},z31.b
.if	mixin == 1
	ror	w21,w21,#24
.endif
.inst	0x04af014a	//add z10.s,z10.s,z15.s
.if	mixin == 1
	add	w17,w17,w22
.endif
.inst	0x04a301ce	//add z14.s,z14.s,z3.s
.if	mixin == 1
	add	w18,w18,w19
.endif
.inst	0x04a70042	//add z2.s,z2.s,z7.s
.if	mixin == 1
	add	w15,w15,w20
.endif
.inst	0x04ab00c6	//add z6.s,z6.s,z11.s
.if	mixin == 1
	add	w16,w16,w21
.endif
.inst	0x04aa30a5	//eor z5.d,z5.d,z10.d
.if	mixin == 1
	eor	w12,w12,w17
.endif
.inst	0x04ae3129	//eor z9.d,z9.d,z14.d
.if	mixin == 1
	eor	w13,w13,w18
.endif
.inst	0x04a231ad	//eor z13.d,z13.d,z2.d
.if	mixin == 1
	eor	w14,w14,w15
.endif
.inst	0x04a63021	//eor z1.d,z1.d,z6.d
.if	mixin == 1
	eor	w11,w11,w16
.endif
.inst	0x04679cb1	//lsl z17.s,z5.s,7
.inst	0x04679d32	//lsl z18.s,z9.s,7
.inst	0x04679db3	//lsl z19.s,z13.s,7
.inst	0x04679c34	//lsl z20.s,z1.s,7
.inst	0x046794a5	//lsr z5.s,z5.s,25
.if	mixin == 1
	ror	w12,w12,25
.endif
.inst	0x04679529	//lsr z9.s,z9.s,25
.if	mixin == 1
	ror	w13,w13,25
.endif
.inst	0x046795ad	//lsr z13.s,z13.s,25
.if	mixin == 1
	ror	w14,w14,25
.endif
.inst	0x04679421	//lsr z1.s,z1.s,25
.if	mixin == 1
	ror	w11,w11,25
.endif
.inst	0x047130a5	//orr z5.d,z5.d,z17.d
.inst	0x04723129	//orr z9.d,z9.d,z18.d
.inst	0x047331ad	//orr z13.d,z13.d,z19.d
.inst	0x04743021	//orr z1.d,z1.d,z20.d
	sub	x6,x6,1
	cbnz	x6,10b
	lsr	x6,x28,#32
.inst	0x05a03b91	//dup z17.s,w28
.inst	0x05a038d2	//dup z18.s,w6
	lsr	x6,x29,#32
.inst	0x05a038d3	//dup z19.s,w6
	lsr	x6,x30,#32
.if	mixin == 1
	add	w7,w7,w23
.endif
.inst	0x04b90000	//add z0.s,z0.s,z25.s
.if	mixin == 1
	add	x8,x8,x23,lsr #32
.endif
.inst	0x04ba0084	//add z4.s,z4.s,z26.s
.if	mixin == 1
	add	x7,x7,x8,lsl #32  // pack
.endif
.if	mixin == 1
	add	w9,w9,w24
.endif
.inst	0x04bb0108	//add z8.s,z8.s,z27.s
.if	mixin == 1
	add	x10,x10,x24,lsr #32
.endif
.inst	0x04bc018c	//add z12.s,z12.s,z28.s
.if	mixin == 1
	add	x9,x9,x10,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x8,x10,[x1],#16
.endif
.if	mixin == 1
	add	w11,w11,w25
.endif
.inst	0x04bd0021	//add z1.s,z1.s,z29.s
.if	mixin == 1
	add	x12,x12,x25,lsr #32
.endif
.inst	0x04be00a5	//add z5.s,z5.s,z30.s
.if	mixin == 1
	add	x11,x11,x12,lsl #32  // pack
.endif
.if	mixin == 1
	add	w13,w13,w26
.endif
.inst	0x04b50129	//add z9.s,z9.s,z21.s
.if	mixin == 1
	add	x14,x14,x26,lsr #32
.endif
.inst	0x04b601ad	//add z13.s,z13.s,z22.s
.if	mixin == 1
	add	x13,x13,x14,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x12,x14,[x1],#16
.endif
.if	mixin == 1
	add	w15,w15,w27
.endif
.inst	0x04b70042	//add z2.s,z2.s,z23.s
.if	mixin == 1
	add	x16,x16,x27,lsr #32
.endif
.inst	0x04b800c6	//add z6.s,z6.s,z24.s
.if	mixin == 1
	add	x15,x15,x16,lsl #32  // pack
.endif
.if	mixin == 1
	add	w17,w17,w28
.endif
.inst	0x04b1014a	//add z10.s,z10.s,z17.s
.if	mixin == 1
	add	x18,x18,x28,lsr #32
.endif
.inst	0x04b201ce	//add z14.s,z14.s,z18.s
.if	mixin == 1
	add	x17,x17,x18,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x16,x18,[x1],#16
.endif
.inst	0x05a03bd4	//dup z20.s,w30
.inst	0x05a038d9	//dup z25.s,w6	// bak[15] not available for SVE
.if	mixin == 1
	add	w19,w19,w29
.endif
.inst	0x04b00063	//add z3.s,z3.s,z16.s
.if	mixin == 1
	add	x20,x20,x29,lsr #32
.endif
.inst	0x04b300e7	//add z7.s,z7.s,z19.s
.if	mixin == 1
	add	x19,x19,x20,lsl #32  // pack
.endif
.if	mixin == 1
	add	w21,w21,w30
.endif
.inst	0x04b4016b	//add z11.s,z11.s,z20.s
.if	mixin == 1
	add	x22,x22,x30,lsr #32
.endif
.inst	0x04b901ef	//add z15.s,z15.s,z25.s
.if	mixin == 1
	add	x21,x21,x22,lsl #32  // pack
.endif
.if	mixin == 1
	ldp	x20,x22,[x1],#16
.endif
#ifdef	__AARCH64EB__
	rev	x7,x7
	rev	x9,x9
	rev	x11,x11
	rev	x13,x13
	rev	x15,x15
	rev	x17,x17
	rev	x19,x19
	rev	x21,x21
#endif
.if	mixin == 1
	add	x29,x29,#1
.endif
	cmp	x5,4
	b.ne	200f
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.if	mixin == 1
	eor	x11,x11,x12
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x13,x13,x14
.endif
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23021	//eor z1.d,z1.d,z18.d
.inst	0x04b33042	//eor z2.d,z2.d,z19.d
.inst	0x04b43063	//eor z3.d,z3.d,z20.d
.inst	0x04b53084	//eor z4.d,z4.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b730c6	//eor z6.d,z6.d,z23.d
.inst	0x04b830e7	//eor z7.d,z7.d,z24.d
	ld1	{v17.4s,v18.4s,v19.4s,v20.4s},[x1],#64
	ld1	{v21.4s,v22.4s,v23.4s,v24.4s},[x1],#64
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13108	//eor z8.d,z8.d,z17.d
.inst	0x04b23129	//eor z9.d,z9.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b4316b	//eor z11.d,z11.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b5318c	//eor z12.d,z12.d,z21.d
.inst	0x04b631ad	//eor z13.d,z13.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b731ce	//eor z14.d,z14.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
	st1	{v0.4s,v1.4s,v2.4s,v3.4s},[x0],#64
	st1	{v4.4s,v5.4s,v6.4s,v7.4s},[x0],#64
	st1	{v8.4s,v9.4s,v10.4s,v11.4s},[x0],#64
	st1	{v12.4s,v13.4s,v14.4s,v15.4s},[x0],#64
	b	210f
200:
.inst	0x05a16011	//zip1 z17.s,z0.s,z1.s
.inst	0x05a16412	//zip2 z18.s,z0.s,z1.s
.inst	0x05a36053	//zip1 z19.s,z2.s,z3.s
.inst	0x05a36454	//zip2 z20.s,z2.s,z3.s

.inst	0x05a56095	//zip1 z21.s,z4.s,z5.s
.inst	0x05a56496	//zip2 z22.s,z4.s,z5.s
.inst	0x05a760d7	//zip1 z23.s,z6.s,z7.s
.inst	0x05a764d8	//zip2 z24.s,z6.s,z7.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36621	//zip2 z1.d,z17.d,z19.d
.inst	0x05f46242	//zip1 z2.d,z18.d,z20.d
.inst	0x05f46643	//zip2 z3.d,z18.d,z20.d

.inst	0x05f762a4	//zip1 z4.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c6	//zip1 z6.d,z22.d,z24.d
.inst	0x05f866c7	//zip2 z7.d,z22.d,z24.d
.if	mixin == 1
	eor	x7,x7,x8
.endif
.if	mixin == 1
	eor	x9,x9,x10
.endif
.inst	0x05a96111	//zip1 z17.s,z8.s,z9.s
.inst	0x05a96512	//zip2 z18.s,z8.s,z9.s
.inst	0x05ab6153	//zip1 z19.s,z10.s,z11.s
.inst	0x05ab6554	//zip2 z20.s,z10.s,z11.s

.inst	0x05ad6195	//zip1 z21.s,z12.s,z13.s
.inst	0x05ad6596	//zip2 z22.s,z12.s,z13.s
.inst	0x05af61d7	//zip1 z23.s,z14.s,z15.s
.inst	0x05af65d8	//zip2 z24.s,z14.s,z15.s

.inst	0x05f36228	//zip1 z8.d,z17.d,z19.d
.inst	0x05f36629	//zip2 z9.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664b	//zip2 z11.d,z18.d,z20.d

.inst	0x05f762ac	//zip1 z12.d,z21.d,z23.d
.inst	0x05f766ad	//zip2 z13.d,z21.d,z23.d
.inst	0x05f862ce	//zip1 z14.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x11,x11,x12
.endif
.if	mixin == 1
	eor	x13,x13,x14
.endif
.inst	0x05a46011	//zip1 z17.s,z0.s,z4.s
.inst	0x05a46412	//zip2 z18.s,z0.s,z4.s
.inst	0x05ac6113	//zip1 z19.s,z8.s,z12.s
.inst	0x05ac6514	//zip2 z20.s,z8.s,z12.s

.inst	0x05a56035	//zip1 z21.s,z1.s,z5.s
.inst	0x05a56436	//zip2 z22.s,z1.s,z5.s
.inst	0x05ad6137	//zip1 z23.s,z9.s,z13.s
.inst	0x05ad6538	//zip2 z24.s,z9.s,z13.s

.inst	0x05f36220	//zip1 z0.d,z17.d,z19.d
.inst	0x05f36624	//zip2 z4.d,z17.d,z19.d
.inst	0x05f46248	//zip1 z8.d,z18.d,z20.d
.inst	0x05f4664c	//zip2 z12.d,z18.d,z20.d

.inst	0x05f762a1	//zip1 z1.d,z21.d,z23.d
.inst	0x05f766a5	//zip2 z5.d,z21.d,z23.d
.inst	0x05f862c9	//zip1 z9.d,z22.d,z24.d
.inst	0x05f866cd	//zip2 z13.d,z22.d,z24.d
.if	mixin == 1
	eor	x15,x15,x16
.endif
.if	mixin == 1
	eor	x17,x17,x18
.endif
.inst	0x05a66051	//zip1 z17.s,z2.s,z6.s
.inst	0x05a66452	//zip2 z18.s,z2.s,z6.s
.inst	0x05ae6153	//zip1 z19.s,z10.s,z14.s
.inst	0x05ae6554	//zip2 z20.s,z10.s,z14.s

.inst	0x05a76075	//zip1 z21.s,z3.s,z7.s
.inst	0x05a76476	//zip2 z22.s,z3.s,z7.s
.inst	0x05af6177	//zip1 z23.s,z11.s,z15.s
.inst	0x05af6578	//zip2 z24.s,z11.s,z15.s

.inst	0x05f36222	//zip1 z2.d,z17.d,z19.d
.inst	0x05f36626	//zip2 z6.d,z17.d,z19.d
.inst	0x05f4624a	//zip1 z10.d,z18.d,z20.d
.inst	0x05f4664e	//zip2 z14.d,z18.d,z20.d

.inst	0x05f762a3	//zip1 z3.d,z21.d,z23.d
.inst	0x05f766a7	//zip2 z7.d,z21.d,z23.d
.inst	0x05f862cb	//zip1 z11.d,z22.d,z24.d
.inst	0x05f866cf	//zip2 z15.d,z22.d,z24.d
.if	mixin == 1
	eor	x19,x19,x20
.endif
.if	mixin == 1
	eor	x21,x21,x22
.endif
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.inst	0x04b13000	//eor z0.d,z0.d,z17.d
.inst	0x04b23084	//eor z4.d,z4.d,z18.d
.inst	0x04b33108	//eor z8.d,z8.d,z19.d
.inst	0x04b4318c	//eor z12.d,z12.d,z20.d
.inst	0x04b53021	//eor z1.d,z1.d,z21.d
.inst	0x04b630a5	//eor z5.d,z5.d,z22.d
.inst	0x04b73129	//eor z9.d,z9.d,z23.d
.inst	0x04b831ad	//eor z13.d,z13.d,z24.d
.inst	0xa540a031	//ld1w {z17.s},p0/z,[x1,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48231	//revb z17.s,p0/m,z17.s
#endif
.inst	0xa541a032	//ld1w {z18.s},p0/z,[x1,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48252	//revb z18.s,p0/m,z18.s
#endif
.inst	0xa542a033	//ld1w {z19.s},p0/z,[x1,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48273	//revb z19.s,p0/m,z19.s
#endif
.inst	0xa543a034	//ld1w {z20.s},p0/z,[x1,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48294	//revb z20.s,p0/m,z20.s
#endif
.inst	0xa544a035	//ld1w {z21.s},p0/z,[x1,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482b5	//revb z21.s,p0/m,z21.s
#endif
.inst	0xa545a036	//ld1w {z22.s},p0/z,[x1,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482d6	//revb z22.s,p0/m,z22.s
#endif
.inst	0xa546a037	//ld1w {z23.s},p0/z,[x1,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a482f7	//revb z23.s,p0/m,z23.s
#endif
.inst	0xa547a038	//ld1w {z24.s},p0/z,[x1,#7,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48318	//revb z24.s,p0/m,z24.s
#endif
.inst	0x04215101	//addvl x1,x1,8
.if	mixin == 1
	stp	x7,x9,[x0],#16
.endif
.inst	0x04b13042	//eor z2.d,z2.d,z17.d
.inst	0x04b230c6	//eor z6.d,z6.d,z18.d
.if	mixin == 1
	stp	x11,x13,[x0],#16
.endif
.inst	0x04b3314a	//eor z10.d,z10.d,z19.d
.inst	0x04b431ce	//eor z14.d,z14.d,z20.d
.if	mixin == 1
	stp	x15,x17,[x0],#16
.endif
.inst	0x04b53063	//eor z3.d,z3.d,z21.d
.inst	0x04b630e7	//eor z7.d,z7.d,z22.d
.if	mixin == 1
	stp	x19,x21,[x0],#16
.endif
.inst	0x04b7316b	//eor z11.d,z11.d,z23.d
.inst	0x04b831ef	//eor z15.d,z15.d,z24.d
#ifdef  __AARCH64EB__
.inst	0x05a48000	//revb z0.s,p0/m,z0.s
#endif
.inst	0xe540e000	//st1w {z0.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48084	//revb z4.s,p0/m,z4.s
#endif
.inst	0xe541e004	//st1w {z4.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48108	//revb z8.s,p0/m,z8.s
#endif
.inst	0xe542e008	//st1w {z8.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4818c	//revb z12.s,p0/m,z12.s
#endif
.inst	0xe543e00c	//st1w {z12.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48021	//revb z1.s,p0/m,z1.s
#endif
.inst	0xe544e001	//st1w {z1.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480a5	//revb z5.s,p0/m,z5.s
#endif
.inst	0xe545e005	//st1w {z5.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48129	//revb z9.s,p0/m,z9.s
#endif
.inst	0xe546e009	//st1w {z9.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ad	//revb z13.s,p0/m,z13.s
#endif
.inst	0xe547e00d	//st1w {z13.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
#ifdef  __AARCH64EB__
.inst	0x05a48042	//revb z2.s,p0/m,z2.s
#endif
.inst	0xe540e002	//st1w {z2.s},p0,[x0,#0,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480c6	//revb z6.s,p0/m,z6.s
#endif
.inst	0xe541e006	//st1w {z6.s},p0,[x0,#1,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4814a	//revb z10.s,p0/m,z10.s
#endif
.inst	0xe542e00a	//st1w {z10.s},p0,[x0,#2,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ce	//revb z14.s,p0/m,z14.s
#endif
.inst	0xe543e00e	//st1w {z14.s},p0,[x0,#3,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a48063	//revb z3.s,p0/m,z3.s
#endif
.inst	0xe544e003	//st1w {z3.s},p0,[x0,#4,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a480e7	//revb z7.s,p0/m,z7.s
#endif
.inst	0xe545e007	//st1w {z7.s},p0,[x0,#5,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a4816b	//revb z11.s,p0/m,z11.s
#endif
.inst	0xe546e00b	//st1w {z11.s},p0,[x0,#6,MUL VL]
#ifdef  __AARCH64EB__
.inst	0x05a481ef	//revb z15.s,p0/m,z15.s
#endif
.inst	0xe547e00f	//st1w {z15.s},p0,[x0,#7,MUL VL]
.inst	0x04205100	//addvl x0,x0,8
210:
.inst	0x04b0e3fd	//incw x29, ALL, MUL #1
110:
2:
	str	w29,[x4]
	ldp	d10,d11,[sp,16]
	ldp	d12,d13,[sp,32]
	ldp	d14,d15,[sp,48]
	ldp	x16,x17,[sp,64]
	ldp	x18,x19,[sp,80]
	ldp	x20,x21,[sp,96]
	ldp	x22,x23,[sp,112]
	ldp	x24,x25,[sp,128]
	ldp	x26,x27,[sp,144]
	ldp	x28,x29,[sp,160]
	ldr	x30,[sp,176]
	ldp	d8,d9,[sp],192
	AARCH64_VALIDATE_LINK_REGISTER
.Lreturn:
	ret
.size	ChaCha20_ctr32_sve,.-ChaCha20_ctr32_sve
