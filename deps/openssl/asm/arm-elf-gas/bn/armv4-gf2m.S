#include "arm_arch.h"

.text
.code	32

#if __ARM_ARCH__>=7
.fpu	neon

.type	mul_1x1_neon,%function
.align	5
mul_1x1_neon:
	vshl.u64	d2,d16,#8	@ q1-q3 are slided

	vmull.p8	q0,d16,d17	@ a·bb
	vshl.u64	d4,d16,#16
	vmull.p8	q1,d2,d17	@ a<<8·bb
	vshl.u64	d6,d16,#24
	vmull.p8	q2,d4,d17	@ a<<16·bb
	vshr.u64	d2,#8
	vmull.p8	q3,d6,d17	@ a<<24·bb
	vshl.u64	d3,#24
	veor		d0,d2
	vshr.u64	d4,#16
	veor		d0,d3
	vshl.u64	d5,#16
	veor		d0,d4
	vshr.u64	d6,#24
	veor		d0,d5
	vshl.u64	d7,#8
	veor		d0,d6
	veor		d0,d7
	.word	0xe12fff1e
.size	mul_1x1_neon,.-mul_1x1_neon
#endif
.type	mul_1x1_ialu,%function
.align	5
mul_1x1_ialu:
	mov	r4,#0
	bic	r5,r1,#3<<30		@ a1=a&0x3fffffff
	str	r4,[sp,#0]		@ tab[0]=0
	add	r6,r5,r5		@ a2=a1<<1
	str	r5,[sp,#4]		@ tab[1]=a1
	eor	r7,r5,r6		@ a1^a2
	str	r6,[sp,#8]		@ tab[2]=a2
	mov	r8,r5,lsl#2		@ a4=a1<<2
	str	r7,[sp,#12]		@ tab[3]=a1^a2
	eor	r9,r5,r8		@ a1^a4
	str	r8,[sp,#16]		@ tab[4]=a4
	eor	r4,r6,r8		@ a2^a4
	str	r9,[sp,#20]		@ tab[5]=a1^a4
	eor	r7,r7,r8		@ a1^a2^a4
	str	r4,[sp,#24]		@ tab[6]=a2^a4
	and	r8,r12,r0,lsl#2
	str	r7,[sp,#28]		@ tab[7]=a1^a2^a4

	and	r9,r12,r0,lsr#1
	ldr	r5,[sp,r8]		@ tab[b       & 0x7]
	and	r8,r12,r0,lsr#4
	ldr	r7,[sp,r9]		@ tab[b >>  3 & 0x7]
	and	r9,r12,r0,lsr#7
	ldr	r6,[sp,r8]		@ tab[b >>  6 & 0x7]
	eor	r5,r5,r7,lsl#3	@ stall
	mov	r4,r7,lsr#29
	ldr	r7,[sp,r9]		@ tab[b >>  9 & 0x7]

	and	r8,r12,r0,lsr#10
	eor	r5,r5,r6,lsl#6
	eor	r4,r4,r6,lsr#26
	ldr	r6,[sp,r8]		@ tab[b >> 12 & 0x7]

	and	r9,r12,r0,lsr#13
	eor	r5,r5,r7,lsl#9
	eor	r4,r4,r7,lsr#23
	ldr	r7,[sp,r9]		@ tab[b >> 15 & 0x7]

	and	r8,r12,r0,lsr#16
	eor	r5,r5,r6,lsl#12
	eor	r4,r4,r6,lsr#20
	ldr	r6,[sp,r8]		@ tab[b >> 18 & 0x7]

	and	r9,r12,r0,lsr#19
	eor	r5,r5,r7,lsl#15
	eor	r4,r4,r7,lsr#17
	ldr	r7,[sp,r9]		@ tab[b >> 21 & 0x7]

	and	r8,r12,r0,lsr#22
	eor	r5,r5,r6,lsl#18
	eor	r4,r4,r6,lsr#14
	ldr	r6,[sp,r8]		@ tab[b >> 24 & 0x7]

	and	r9,r12,r0,lsr#25
	eor	r5,r5,r7,lsl#21
	eor	r4,r4,r7,lsr#11
	ldr	r7,[sp,r9]		@ tab[b >> 27 & 0x7]

	tst	r1,#1<<30
	and	r8,r12,r0,lsr#28
	eor	r5,r5,r6,lsl#24
	eor	r4,r4,r6,lsr#8
	ldr	r6,[sp,r8]		@ tab[b >> 30      ]

	eorne	r5,r5,r0,lsl#30
	eorne	r4,r4,r0,lsr#2
	tst	r1,#1<<31
	eor	r5,r5,r7,lsl#27
	eor	r4,r4,r7,lsr#5
	eorne	r5,r5,r0,lsl#31
	eorne	r4,r4,r0,lsr#1
	eor	r5,r5,r6,lsl#30
	eor	r4,r4,r6,lsr#2

	mov	pc,lr
.size	mul_1x1_ialu,.-mul_1x1_ialu
.global	bn_GF2m_mul_2x2
.type	bn_GF2m_mul_2x2,%function
.align	5
bn_GF2m_mul_2x2:
#if __ARM_ARCH__>=7
	ldr	r12,.LOPENSSL_armcap
.Lpic:	ldr	r12,[pc,r12]
	tst	r12,#1
	beq	.Lialu

	veor	d18,d18
	vmov.32	d19,r3,r3		@ two copies of b1
	vmov.32	d18[0],r1		@ a1

	veor	d20,d20
	vld1.32	d21[],[sp,:32]	@ two copies of b0
	vmov.32	d20[0],r2		@ a0
	mov	r12,lr

	vmov	d16,d18
	vmov	d17,d19
	bl	mul_1x1_neon		@ a1·b1
	vmov	d22,d0

	vmov	d16,d20
	vmov	d17,d21
	bl	mul_1x1_neon		@ a0·b0
	vmov	d23,d0

	veor	d16,d20,d18
	veor	d17,d21,d19
	veor	d20,d23,d22
	bl	mul_1x1_neon		@ (a0+a1)·(b0+b1)

	veor	d0,d20			@ (a0+a1)·(b0+b1)-a0·b0-a1·b1
	vshl.u64 d1,d0,#32
	vshr.u64 d0,d0,#32
	veor	d23,d1
	veor	d22,d0
	vst1.32	{d23[0]},[r0,:32]!
	vst1.32	{d23[1]},[r0,:32]!
	vst1.32	{d22[0]},[r0,:32]!
	vst1.32	{d22[1]},[r0,:32]
	bx	r12
.align	4
.Lialu:
#endif
	stmdb	sp!,{r4-r10,lr}
	mov	r10,r0			@ reassign 1st argument
	mov	r0,r3			@ r0=b1
	ldr	r3,[sp,#32]		@ load b0
	mov	r12,#7<<2
	sub	sp,sp,#32		@ allocate tab[8]

	bl	mul_1x1_ialu		@ a1·b1
	str	r5,[r10,#8]
	str	r4,[r10,#12]

	eor	r0,r0,r3		@ flip b0 and b1
	 eor	r1,r1,r2		@ flip a0 and a1
	eor	r3,r3,r0
	 eor	r2,r2,r1
	eor	r0,r0,r3
	 eor	r1,r1,r2
	bl	mul_1x1_ialu		@ a0·b0
	str	r5,[r10]
	str	r4,[r10,#4]

	eor	r1,r1,r2
	eor	r0,r0,r3
	bl	mul_1x1_ialu		@ (a1+a0)·(b1+b0)
	ldmia	r10,{r6-r9}
	eor	r5,r5,r4
	eor	r4,r4,r7
	eor	r5,r5,r6
	eor	r4,r4,r8
	eor	r5,r5,r9
	eor	r4,r4,r9
	str	r4,[r10,#8]
	eor	r5,r5,r4
	add	sp,sp,#32		@ destroy tab[8]
	str	r5,[r10,#4]

#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r10,pc}
#else
	ldmia	sp!,{r4-r10,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
#endif
.size	bn_GF2m_mul_2x2,.-bn_GF2m_mul_2x2
#if __ARM_ARCH__>=7
.align	5
.LOPENSSL_armcap:
.word	OPENSSL_armcap_P-(.Lpic+8)
#endif
.asciz	"GF(2^m) Multiplication for ARMv4/NEON, CRYPTOGAMS by <appro@openssl.org>"
.align	5

.comm	OPENSSL_armcap_P,4,4
