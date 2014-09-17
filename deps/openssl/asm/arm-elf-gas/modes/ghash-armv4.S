#include "arm_arch.h"

.text
.code	32

.type	rem_4bit,%object
.align	5
rem_4bit:
.short	0x0000,0x1C20,0x3840,0x2460
.short	0x7080,0x6CA0,0x48C0,0x54E0
.short	0xE100,0xFD20,0xD940,0xC560
.short	0x9180,0x8DA0,0xA9C0,0xB5E0
.size	rem_4bit,.-rem_4bit

.type	rem_4bit_get,%function
rem_4bit_get:
	sub	r2,pc,#8
	sub	r2,r2,#32	@ &rem_4bit
	b	.Lrem_4bit_got
	nop
.size	rem_4bit_get,.-rem_4bit_get

.global	gcm_ghash_4bit
.type	gcm_ghash_4bit,%function
gcm_ghash_4bit:
	sub	r12,pc,#8
	add	r3,r2,r3		@ r3 to point at the end
	stmdb	sp!,{r3-r11,lr}		@ save r3/end too
	sub	r12,r12,#48		@ &rem_4bit

	ldmia	r12,{r4-r11}		@ copy rem_4bit ...
	stmdb	sp!,{r4-r11}		@ ... to stack

	ldrb	r12,[r2,#15]
	ldrb	r14,[r0,#15]
.Louter:
	eor	r12,r12,r14
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	mov	r3,#14

	add	r7,r1,r12,lsl#4
	ldmia	r7,{r4-r7}	@ load Htbl[nlo]
	add	r11,r1,r14
	ldrb	r12,[r2,#14]

	and	r14,r4,#0xf		@ rem
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	add	r14,r14,r14
	eor	r4,r8,r4,lsr#4
	ldrh	r8,[sp,r14]		@ rem_4bit[rem]
	eor	r4,r4,r5,lsl#28
	ldrb	r14,[r0,#14]
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
	eor	r12,r12,r14
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	eor	r7,r7,r8,lsl#16

.Linner:
	add	r11,r1,r12,lsl#4
	and	r12,r4,#0xf		@ rem
	subs	r3,r3,#1
	add	r12,r12,r12
	ldmia	r11,{r8-r11}	@ load Htbl[nlo]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	ldrh	r8,[sp,r12]		@ rem_4bit[rem]
	eor	r6,r10,r6,lsr#4
	ldrplb	r12,[r2,r3]
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	add	r14,r14,r14
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	eor	r4,r8,r4,lsr#4
	ldrplb	r8,[r0,r3]
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	ldrh	r9,[sp,r14]
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eorpl	r12,r12,r8
	eor	r7,r11,r7,lsr#4
	andpl	r14,r12,#0xf0
	andpl	r12,r12,#0x0f
	eor	r7,r7,r9,lsl#16	@ ^= rem_4bit[rem]
	bpl	.Linner

	ldr	r3,[sp,#32]		@ re-load r3/end
	add	r2,r2,#16
	mov	r14,r4
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r4,r4
	str	r4,[r0,#12]
#elif defined(__ARMEB__)
	str	r4,[r0,#12]
#else
	mov	r9,r4,lsr#8
	strb	r4,[r0,#12+3]
	mov	r10,r4,lsr#16
	strb	r9,[r0,#12+2]
	mov	r11,r4,lsr#24
	strb	r10,[r0,#12+1]
	strb	r11,[r0,#12]
#endif
	cmp	r2,r3
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r5,r5
	str	r5,[r0,#8]
#elif defined(__ARMEB__)
	str	r5,[r0,#8]
#else
	mov	r9,r5,lsr#8
	strb	r5,[r0,#8+3]
	mov	r10,r5,lsr#16
	strb	r9,[r0,#8+2]
	mov	r11,r5,lsr#24
	strb	r10,[r0,#8+1]
	strb	r11,[r0,#8]
#endif
	ldrneb	r12,[r2,#15]
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r6,r6
	str	r6,[r0,#4]
#elif defined(__ARMEB__)
	str	r6,[r0,#4]
#else
	mov	r9,r6,lsr#8
	strb	r6,[r0,#4+3]
	mov	r10,r6,lsr#16
	strb	r9,[r0,#4+2]
	mov	r11,r6,lsr#24
	strb	r10,[r0,#4+1]
	strb	r11,[r0,#4]
#endif


#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r7,r7
	str	r7,[r0,#0]
#elif defined(__ARMEB__)
	str	r7,[r0,#0]
#else
	mov	r9,r7,lsr#8
	strb	r7,[r0,#0+3]
	mov	r10,r7,lsr#16
	strb	r9,[r0,#0+2]
	mov	r11,r7,lsr#24
	strb	r10,[r0,#0+1]
	strb	r11,[r0,#0]
#endif


	bne	.Louter

	add	sp,sp,#36
#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r11,pc}
#else
	ldmia	sp!,{r4-r11,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
#endif
.size	gcm_ghash_4bit,.-gcm_ghash_4bit

.global	gcm_gmult_4bit
.type	gcm_gmult_4bit,%function
gcm_gmult_4bit:
	stmdb	sp!,{r4-r11,lr}
	ldrb	r12,[r0,#15]
	b	rem_4bit_get
.Lrem_4bit_got:
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	mov	r3,#14

	add	r7,r1,r12,lsl#4
	ldmia	r7,{r4-r7}	@ load Htbl[nlo]
	ldrb	r12,[r0,#14]

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	add	r14,r14,r14
	eor	r4,r8,r4,lsr#4
	ldrh	r8,[r2,r14]	@ rem_4bit[rem]
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
	and	r14,r12,#0xf0
	eor	r7,r7,r8,lsl#16
	and	r12,r12,#0x0f

.Loop:
	add	r11,r1,r12,lsl#4
	and	r12,r4,#0xf		@ rem
	subs	r3,r3,#1
	add	r12,r12,r12
	ldmia	r11,{r8-r11}	@ load Htbl[nlo]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	ldrh	r8,[r2,r12]	@ rem_4bit[rem]
	eor	r6,r10,r6,lsr#4
	ldrplb	r12,[r0,r3]
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	add	r14,r14,r14
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	ldrh	r8,[r2,r14]	@ rem_4bit[rem]
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
	andpl	r14,r12,#0xf0
	andpl	r12,r12,#0x0f
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	bpl	.Loop
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r4,r4
	str	r4,[r0,#12]
#elif defined(__ARMEB__)
	str	r4,[r0,#12]
#else
	mov	r9,r4,lsr#8
	strb	r4,[r0,#12+3]
	mov	r10,r4,lsr#16
	strb	r9,[r0,#12+2]
	mov	r11,r4,lsr#24
	strb	r10,[r0,#12+1]
	strb	r11,[r0,#12]
#endif


#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r5,r5
	str	r5,[r0,#8]
#elif defined(__ARMEB__)
	str	r5,[r0,#8]
#else
	mov	r9,r5,lsr#8
	strb	r5,[r0,#8+3]
	mov	r10,r5,lsr#16
	strb	r9,[r0,#8+2]
	mov	r11,r5,lsr#24
	strb	r10,[r0,#8+1]
	strb	r11,[r0,#8]
#endif


#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r6,r6
	str	r6,[r0,#4]
#elif defined(__ARMEB__)
	str	r6,[r0,#4]
#else
	mov	r9,r6,lsr#8
	strb	r6,[r0,#4+3]
	mov	r10,r6,lsr#16
	strb	r9,[r0,#4+2]
	mov	r11,r6,lsr#24
	strb	r10,[r0,#4+1]
	strb	r11,[r0,#4]
#endif


#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r7,r7
	str	r7,[r0,#0]
#elif defined(__ARMEB__)
	str	r7,[r0,#0]
#else
	mov	r9,r7,lsr#8
	strb	r7,[r0,#0+3]
	mov	r10,r7,lsr#16
	strb	r9,[r0,#0+2]
	mov	r11,r7,lsr#24
	strb	r10,[r0,#0+1]
	strb	r11,[r0,#0]
#endif


#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r11,pc}
#else
	ldmia	sp!,{r4-r11,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
#endif
.size	gcm_gmult_4bit,.-gcm_gmult_4bit
#if __ARM_ARCH__>=7
.fpu	neon

.global	gcm_gmult_neon
.type	gcm_gmult_neon,%function
.align	4
gcm_gmult_neon:
	sub		r1,#16		@ point at H in GCM128_CTX
	vld1.64		d29,[r0,:64]!@ load Xi
	vmov.i32	d5,#0xe1		@ our irreducible polynomial
	vld1.64		d28,[r0,:64]!
	vshr.u64	d5,#32
	vldmia		r1,{d0-d1}	@ load H
	veor		q12,q12
#ifdef __ARMEL__
	vrev64.8	q14,q14
#endif
	veor		q13,q13
	veor		q11,q11
	mov		r1,#16
	veor		q10,q10
	mov		r3,#16
	veor		d2,d2
	vdup.8		d4,d28[0]	@ broadcast lowest byte
	b		.Linner_neon
.size	gcm_gmult_neon,.-gcm_gmult_neon

.global	gcm_ghash_neon
.type	gcm_ghash_neon,%function
.align	4
gcm_ghash_neon:
	vld1.64		d21,[r0,:64]!	@ load Xi
	vmov.i32	d5,#0xe1		@ our irreducible polynomial
	vld1.64		d20,[r0,:64]!
	vshr.u64	d5,#32
	vldmia		r0,{d0-d1}		@ load H
	veor		q12,q12
	nop
#ifdef __ARMEL__
	vrev64.8	q10,q10
#endif
.Louter_neon:
	vld1.64		d29,[r2]!	@ load inp
	veor		q13,q13
	vld1.64		d28,[r2]!
	veor		q11,q11
	mov		r1,#16
#ifdef __ARMEL__
	vrev64.8	q14,q14
#endif
	veor		d2,d2
	veor		q14,q10			@ inp^=Xi
	veor		q10,q10
	vdup.8		d4,d28[0]	@ broadcast lowest byte
.Linner_neon:
	subs		r1,r1,#1
	vmull.p8	q9,d1,d4		@ H.lo·Xi[i]
	vmull.p8	q8,d0,d4		@ H.hi·Xi[i]
	vext.8		q14,q12,#1		@ IN>>=8

	veor		q10,q13		@ modulo-scheduled part
	vshl.i64	d22,#48
	vdup.8		d4,d28[0]	@ broadcast lowest byte
	veor		d3,d18,d20

	veor		d21,d22
	vuzp.8		q9,q8
	vsli.8		d2,d3,#1		@ compose the "carry" byte
	vext.8		q10,q12,#1		@ Z>>=8

	vmull.p8	q11,d2,d5		@ "carry"·0xe1
	vshr.u8		d2,d3,#7		@ save Z's bottom bit
	vext.8		q13,q9,q12,#1	@ Qlo>>=8
	veor		q10,q8
	bne		.Linner_neon

	veor		q10,q13		@ modulo-scheduled artefact
	vshl.i64	d22,#48
	veor		d21,d22

	@ finalization, normalize Z:Zo
	vand		d2,d5		@ suffices to mask the bit
	vshr.u64	d3,d20,#63
	vshl.i64	q10,#1
	subs		r3,#16
	vorr		q10,q1		@ Z=Z:Zo<<1
	bne		.Louter_neon

#ifdef __ARMEL__
	vrev64.8	q10,q10
#endif
	sub		r0,#16

	vst1.64		d21,[r0,:64]!	@ write out Xi
	vst1.64		d20,[r0,:64]

	.word	0xe12fff1e
.size	gcm_ghash_neon,.-gcm_ghash_neon
#endif
.asciz  "GHASH for ARMv4/NEON, CRYPTOGAMS by <appro@openssl.org>"
.align  2
