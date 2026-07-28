#include "s390x_arch.h"
.text
.globl	ChaCha20_ctr32
.type	ChaCha20_ctr32,@function
.align	32
ChaCha20_ctr32:
	larl	%r1,OPENSSL_s390xcap_P
	lghi	%r0,64
	ltgr	%r4,%r4
	bzr	%r14
	lg	%r1,S390X_STFLE+16(%r1)
	clgr	%r4,%r0
	jle	.Lshort
	tmhh	%r1,16384
	jnz	.LChaCha20_ctr32_vx
.Lshort:
	aghi	%r4,-64
	lghi	%r1,-240
	stmg	%r6,%r15,6*8(%r15)
	slgr	%r2,%r3
	la	%r4,0(%r3,%r4)
	larl	%r7,.Lsigma
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)
	lmg	%r8,%r11,0(%r5)
	lmg	%r12,%r13,0(%r6)
	lmg	%r6,%r7,0(%r7)
	la	%r14,0(%r3)
	stg	%r2,240+3*8(%r15)
	stg	%r4,240+4*8(%r15)
	stmg	%r6,%r13,160(%r15)
	srlg	%r10,%r12,32
	j	.Loop_outer
.align	16
.Loop_outer:
	lm	%r0,%r7,160+4*0(%r15)
	lm	%r8,%r9,160+4*10(%r15)
	lm	%r11,%r13,160+4*13(%r15)
	stm	%r8,%r9,160+4*8+4*10(%r15)
	lm	%r8,%r9,160+4*8(%r15)
	st	%r10,160+4*12(%r15)
	stg	%r14,240+2*8(%r15)
	lhi	%r14,10
	j	.Loop
.align	4
.Loop:
	alr	%r0,%r4
	alr	%r1,%r5
	xr	%r10,%r0
	xr	%r11,%r1
	rll	%r10,%r10,16
	rll	%r11,%r11,16
	alr	%r8,%r10
	alr	%r9,%r11
	xr	%r4,%r8
	xr	%r5,%r9
	rll	%r4,%r4,12
	rll	%r5,%r5,12
	alr	%r0,%r4
	alr	%r1,%r5
	xr	%r10,%r0
	xr	%r11,%r1
	rll	%r10,%r10,8
	rll	%r11,%r11,8
	alr	%r8,%r10
	alr	%r9,%r11
	xr	%r4,%r8
	xr	%r5,%r9
	rll	%r4,%r4,7
	rll	%r5,%r5,7
	stm	%r8,%r9,160+4*8+4*8(%r15)
	lm	%r8,%r9,160+4*8+4*10(%r15)
	alr	%r2,%r6
	alr	%r3,%r7
	xr	%r12,%r2
	xr	%r13,%r3
	rll	%r12,%r12,16
	rll	%r13,%r13,16
	alr	%r8,%r12
	alr	%r9,%r13
	xr	%r6,%r8
	xr	%r7,%r9
	rll	%r6,%r6,12
	rll	%r7,%r7,12
	alr	%r2,%r6
	alr	%r3,%r7
	xr	%r12,%r2
	xr	%r13,%r3
	rll	%r12,%r12,8
	rll	%r13,%r13,8
	alr	%r8,%r12
	alr	%r9,%r13
	xr	%r6,%r8
	xr	%r7,%r9
	rll	%r6,%r6,7
	rll	%r7,%r7,7
	alr	%r0,%r5
	alr	%r1,%r6
	xr	%r13,%r0
	xr	%r10,%r1
	rll	%r13,%r13,16
	rll	%r10,%r10,16
	alr	%r8,%r13
	alr	%r9,%r10
	xr	%r5,%r8
	xr	%r6,%r9
	rll	%r5,%r5,12
	rll	%r6,%r6,12
	alr	%r0,%r5
	alr	%r1,%r6
	xr	%r13,%r0
	xr	%r10,%r1
	rll	%r13,%r13,8
	rll	%r10,%r10,8
	alr	%r8,%r13
	alr	%r9,%r10
	xr	%r5,%r8
	xr	%r6,%r9
	rll	%r5,%r5,7
	rll	%r6,%r6,7
	stm	%r8,%r9,160+4*8+4*10(%r15)
	lm	%r8,%r9,160+4*8+4*8(%r15)
	alr	%r2,%r7
	alr	%r3,%r4
	xr	%r11,%r2
	xr	%r12,%r3
	rll	%r11,%r11,16
	rll	%r12,%r12,16
	alr	%r8,%r11
	alr	%r9,%r12
	xr	%r7,%r8
	xr	%r4,%r9
	rll	%r7,%r7,12
	rll	%r4,%r4,12
	alr	%r2,%r7
	alr	%r3,%r4
	xr	%r11,%r2
	xr	%r12,%r3
	rll	%r11,%r11,8
	rll	%r12,%r12,8
	alr	%r8,%r11
	alr	%r9,%r12
	xr	%r7,%r8
	xr	%r4,%r9
	rll	%r7,%r7,7
	rll	%r4,%r4,7
	brct	%r14,.Loop
	lg	%r14,240+2*8(%r15)
	stm	%r8,%r9,160+4*8+4*8(%r15)
	lmg	%r8,%r9,240+3*8(%r15)
	al	%r0,160+4*0(%r15)
	al	%r1,160+4*1(%r15)
	al	%r2,160+4*2(%r15)
	al	%r3,160+4*3(%r15)
	al	%r4,160+4*4(%r15)
	al	%r5,160+4*5(%r15)
	al	%r6,160+4*6(%r15)
	al	%r7,160+4*7(%r15)
	lrvr	%r0,%r0
	lrvr	%r1,%r1
	lrvr	%r2,%r2
	lrvr	%r3,%r3
	lrvr	%r4,%r4
	lrvr	%r5,%r5
	lrvr	%r6,%r6
	lrvr	%r7,%r7
	al	%r10,160+4*12(%r15)
	al	%r11,160+4*13(%r15)
	al	%r12,160+4*14(%r15)
	al	%r13,160+4*15(%r15)
	lrvr	%r10,%r10
	lrvr	%r11,%r11
	lrvr	%r12,%r12
	lrvr	%r13,%r13
	la	%r8,0(%r8,%r14)
	clgr	%r14,%r9
	jh	.Ltail
	x	%r0,4*0(%r14)
	x	%r1,4*1(%r14)
	st	%r0,4*0(%r8)
	x	%r2,4*2(%r14)
	st	%r1,4*1(%r8)
	x	%r3,4*3(%r14)
	st	%r2,4*2(%r8)
	x	%r4,4*4(%r14)
	st	%r3,4*3(%r8)
	lm	%r0,%r3,160+4*8+4*8(%r15)
	x	%r5,4*5(%r14)
	st	%r4,4*4(%r8)
	x	%r6,4*6(%r14)
	al	%r0,160+4*8(%r15)
	st	%r5,4*5(%r8)
	x	%r7,4*7(%r14)
	al	%r1,160+4*9(%r15)
	st	%r6,4*6(%r8)
	x	%r10,4*12(%r14)
	al	%r2,160+4*10(%r15)
	st	%r7,4*7(%r8)
	x	%r11,4*13(%r14)
	al	%r3,160+4*11(%r15)
	st	%r10,4*12(%r8)
	x	%r12,4*14(%r14)
	st	%r11,4*13(%r8)
	x	%r13,4*15(%r14)
	st	%r12,4*14(%r8)
	lrvr	%r0,%r0
	st	%r13,4*15(%r8)
	lrvr	%r1,%r1
	lrvr	%r2,%r2
	lrvr	%r3,%r3
	lhi	%r10,1
	x	%r0,4*8(%r14)
	al	%r10,160+4*12(%r15)
	x	%r1,4*9(%r14)
	st	%r0,4*8(%r8)
	x	%r2,4*10(%r14)
	st	%r1,4*9(%r8)
	x	%r3,4*11(%r14)
	st	%r2,4*10(%r8)
	st	%r3,4*11(%r8)
	clgr	%r14,%r9
	la	%r14,64(%r14)
	jl	.Loop_outer
.Ldone:
	xgr	%r0,%r0
	xgr	%r1,%r1
	xgr	%r2,%r2
	xgr	%r3,%r3
	stmg	%r0,%r3,160+4*4(%r15)
	stmg	%r0,%r3,160+4*12(%r15)
	lmg	%r6,%r15,240+6*8(%r15)
	br	%r14
.align	16
.Ltail:
	la	%r9,64(%r9)
	stm	%r0,%r7,160+4*0(%r15)
	slgr	%r9,%r14
	lm	%r0,%r3,160+4*8+4*8(%r15)
	lghi	%r6,0
	stm	%r10,%r13,160+4*12(%r15)
	al	%r0,160+4*8(%r15)
	al	%r1,160+4*9(%r15)
	al	%r2,160+4*10(%r15)
	al	%r3,160+4*11(%r15)
	lrvr	%r0,%r0
	lrvr	%r1,%r1
	lrvr	%r2,%r2
	lrvr	%r3,%r3
	stm	%r0,%r3,160+4*8(%r15)
.Loop_tail:
	llgc	%r4,0(%r6,%r14)
	llgc	%r5,160(%r6,%r15)
	xr	%r5,%r4
	stc	%r5,0(%r6,%r8)
	la	%r6,1(%r6)
	brct	%r9,.Loop_tail
	j	.Ldone
.size	ChaCha20_ctr32,.-ChaCha20_ctr32
.align	32
ChaCha20_ctr32_4x:
.LChaCha20_ctr32_4x:
	stmg	%r6,%r7,6*8(%r15)
	lghi	%r1,-224
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)
	std	%f8,160+8*0(%r15)
	std	%f9,160+8*1(%r15)
	std	%f10,160+8*2(%r15)
	std	%f11,160+8*3(%r15)
	std	%f12,160+8*4(%r15)
	std	%f13,160+8*5(%r15)
	std	%f14,160+8*6(%r15)
	std	%f15,160+8*7(%r15)
	larl	%r7,.Lsigma
	lhi	%r0,10
	lhi	%r1,0
	.word	0xe700,0x7000,0x0806	# vl	%v16,0(%r7)
	.word	0xe710,0x5000,0x0806	# vl	%v17,0(%r5)
	.word	0xe720,0x5010,0x0806	# vl	%v18,16(%r5)
	.word	0xe730,0x6000,0x0806	# vl	%v19,0(%r6)
	.word	0xe7f0,0x7040,0x0806	# vl	%v31,0x40(%r7)
	.word	0xe7c0,0x7050,0x0806	# vl	%v28,0x50(%r7)
	.word	0xe7a3,000000,0x2c4d	# vrep	%v26,%v19,0,2
	.word	0xe731,000000,0x2822	# vlvg	%v19,%r1,0,2
	.word	0xe7aa,0xc000,0x2ef3	# va	%v26,%v26,%v28,2
	.word	0xe703,0x7060,0x0036	# vlm	%v0,%v3,0x60(%r7)
	.word	0xe741,000000,0x244d	# vrep	%v4,%v17,0,2
	.word	0xe751,0x0001,0x244d	# vrep	%v5,%v17,1,2
	.word	0xe761,0x0002,0x244d	# vrep	%v6,%v17,2,2
	.word	0xe771,0x0003,0x244d	# vrep	%v7,%v17,3,2
	.word	0xe782,000000,0x244d	# vrep	%v8,%v18,0,2
	.word	0xe792,0x0001,0x244d	# vrep	%v9,%v18,1,2
	.word	0xe7a2,0x0002,0x244d	# vrep	%v10,%v18,2,2
	.word	0xe7b2,0x0003,0x244d	# vrep	%v11,%v18,3,2
	.word	0xe7ca,000000,0x0456	# vlr	%v12,%v26
	.word	0xe7d3,0x0001,0x244d	# vrep	%v13,%v19,1,2
	.word	0xe7e3,0x0002,0x244d	# vrep	%v14,%v19,2,2
	.word	0xe7f3,0x0003,0x244d	# vrep	%v15,%v19,3,2
.Loop_4x:
	.word	0xe700,0x4000,0x20f3	# va	%v0,%v0,%v4,2
	.word	0xe7cc,000000,0x006d	# vx	%v12,%v12,%v0
	.word	0xe7cc,0x0010,0x2033	# verll	%v12,%v12,16,2
	.word	0xe711,0x5000,0x20f3	# va	%v1,%v1,%v5,2
	.word	0xe7dd,0x1000,0x006d	# vx	%v13,%v13,%v1
	.word	0xe7dd,0x0010,0x2033	# verll	%v13,%v13,16,2
	.word	0xe722,0x6000,0x20f3	# va	%v2,%v2,%v6,2
	.word	0xe7ee,0x2000,0x006d	# vx	%v14,%v14,%v2
	.word	0xe7ee,0x0010,0x2033	# verll	%v14,%v14,16,2
	.word	0xe733,0x7000,0x20f3	# va	%v3,%v3,%v7,2
	.word	0xe7ff,0x3000,0x006d	# vx	%v15,%v15,%v3
	.word	0xe7ff,0x0010,0x2033	# verll	%v15,%v15,16,2
	.word	0xe788,0xc000,0x20f3	# va	%v8,%v8,%v12,2
	.word	0xe744,0x8000,0x006d	# vx	%v4,%v4,%v8
	.word	0xe744,0x000c,0x2033	# verll	%v4,%v4,12,2
	.word	0xe799,0xd000,0x20f3	# va	%v9,%v9,%v13,2
	.word	0xe755,0x9000,0x006d	# vx	%v5,%v5,%v9
	.word	0xe755,0x000c,0x2033	# verll	%v5,%v5,12,2
	.word	0xe7aa,0xe000,0x20f3	# va	%v10,%v10,%v14,2
	.word	0xe766,0xa000,0x006d	# vx	%v6,%v6,%v10
	.word	0xe766,0x000c,0x2033	# verll	%v6,%v6,12,2
	.word	0xe7bb,0xf000,0x20f3	# va	%v11,%v11,%v15,2
	.word	0xe777,0xb000,0x006d	# vx	%v7,%v7,%v11
	.word	0xe777,0x000c,0x2033	# verll	%v7,%v7,12,2
	.word	0xe700,0x4000,0x20f3	# va	%v0,%v0,%v4,2
	.word	0xe7cc,000000,0x006d	# vx	%v12,%v12,%v0
	.word	0xe7cc,0x0008,0x2033	# verll	%v12,%v12,8,2
	.word	0xe711,0x5000,0x20f3	# va	%v1,%v1,%v5,2
	.word	0xe7dd,0x1000,0x006d	# vx	%v13,%v13,%v1
	.word	0xe7dd,0x0008,0x2033	# verll	%v13,%v13,8,2
	.word	0xe722,0x6000,0x20f3	# va	%v2,%v2,%v6,2
	.word	0xe7ee,0x2000,0x006d	# vx	%v14,%v14,%v2
	.word	0xe7ee,0x0008,0x2033	# verll	%v14,%v14,8,2
	.word	0xe733,0x7000,0x20f3	# va	%v3,%v3,%v7,2
	.word	0xe7ff,0x3000,0x006d	# vx	%v15,%v15,%v3
	.word	0xe7ff,0x0008,0x2033	# verll	%v15,%v15,8,2
	.word	0xe788,0xc000,0x20f3	# va	%v8,%v8,%v12,2
	.word	0xe744,0x8000,0x006d	# vx	%v4,%v4,%v8
	.word	0xe744,0x0007,0x2033	# verll	%v4,%v4,7,2
	.word	0xe799,0xd000,0x20f3	# va	%v9,%v9,%v13,2
	.word	0xe755,0x9000,0x006d	# vx	%v5,%v5,%v9
	.word	0xe755,0x0007,0x2033	# verll	%v5,%v5,7,2
	.word	0xe7aa,0xe000,0x20f3	# va	%v10,%v10,%v14,2
	.word	0xe766,0xa000,0x006d	# vx	%v6,%v6,%v10
	.word	0xe766,0x0007,0x2033	# verll	%v6,%v6,7,2
	.word	0xe7bb,0xf000,0x20f3	# va	%v11,%v11,%v15,2
	.word	0xe777,0xb000,0x006d	# vx	%v7,%v7,%v11
	.word	0xe777,0x0007,0x2033	# verll	%v7,%v7,7,2
	.word	0xe700,0x5000,0x20f3	# va	%v0,%v0,%v5,2
	.word	0xe7ff,000000,0x006d	# vx	%v15,%v15,%v0
	.word	0xe7ff,0x0010,0x2033	# verll	%v15,%v15,16,2
	.word	0xe711,0x6000,0x20f3	# va	%v1,%v1,%v6,2
	.word	0xe7cc,0x1000,0x006d	# vx	%v12,%v12,%v1
	.word	0xe7cc,0x0010,0x2033	# verll	%v12,%v12,16,2
	.word	0xe722,0x7000,0x20f3	# va	%v2,%v2,%v7,2
	.word	0xe7dd,0x2000,0x006d	# vx	%v13,%v13,%v2
	.word	0xe7dd,0x0010,0x2033	# verll	%v13,%v13,16,2
	.word	0xe733,0x4000,0x20f3	# va	%v3,%v3,%v4,2
	.word	0xe7ee,0x3000,0x006d	# vx	%v14,%v14,%v3
	.word	0xe7ee,0x0010,0x2033	# verll	%v14,%v14,16,2
	.word	0xe7aa,0xf000,0x20f3	# va	%v10,%v10,%v15,2
	.word	0xe755,0xa000,0x006d	# vx	%v5,%v5,%v10
	.word	0xe755,0x000c,0x2033	# verll	%v5,%v5,12,2
	.word	0xe7bb,0xc000,0x20f3	# va	%v11,%v11,%v12,2
	.word	0xe766,0xb000,0x006d	# vx	%v6,%v6,%v11
	.word	0xe766,0x000c,0x2033	# verll	%v6,%v6,12,2
	.word	0xe788,0xd000,0x20f3	# va	%v8,%v8,%v13,2
	.word	0xe777,0x8000,0x006d	# vx	%v7,%v7,%v8
	.word	0xe777,0x000c,0x2033	# verll	%v7,%v7,12,2
	.word	0xe799,0xe000,0x20f3	# va	%v9,%v9,%v14,2
	.word	0xe744,0x9000,0x006d	# vx	%v4,%v4,%v9
	.word	0xe744,0x000c,0x2033	# verll	%v4,%v4,12,2
	.word	0xe700,0x5000,0x20f3	# va	%v0,%v0,%v5,2
	.word	0xe7ff,000000,0x006d	# vx	%v15,%v15,%v0
	.word	0xe7ff,0x0008,0x2033	# verll	%v15,%v15,8,2
	.word	0xe711,0x6000,0x20f3	# va	%v1,%v1,%v6,2
	.word	0xe7cc,0x1000,0x006d	# vx	%v12,%v12,%v1
	.word	0xe7cc,0x0008,0x2033	# verll	%v12,%v12,8,2
	.word	0xe722,0x7000,0x20f3	# va	%v2,%v2,%v7,2
	.word	0xe7dd,0x2000,0x006d	# vx	%v13,%v13,%v2
	.word	0xe7dd,0x0008,0x2033	# verll	%v13,%v13,8,2
	.word	0xe733,0x4000,0x20f3	# va	%v3,%v3,%v4,2
	.word	0xe7ee,0x3000,0x006d	# vx	%v14,%v14,%v3
	.word	0xe7ee,0x0008,0x2033	# verll	%v14,%v14,8,2
	.word	0xe7aa,0xf000,0x20f3	# va	%v10,%v10,%v15,2
	.word	0xe755,0xa000,0x006d	# vx	%v5,%v5,%v10
	.word	0xe755,0x0007,0x2033	# verll	%v5,%v5,7,2
	.word	0xe7bb,0xc000,0x20f3	# va	%v11,%v11,%v12,2
	.word	0xe766,0xb000,0x006d	# vx	%v6,%v6,%v11
	.word	0xe766,0x0007,0x2033	# verll	%v6,%v6,7,2
	.word	0xe788,0xd000,0x20f3	# va	%v8,%v8,%v13,2
	.word	0xe777,0x8000,0x006d	# vx	%v7,%v7,%v8
	.word	0xe777,0x0007,0x2033	# verll	%v7,%v7,7,2
	.word	0xe799,0xe000,0x20f3	# va	%v9,%v9,%v14,2
	.word	0xe744,0x9000,0x006d	# vx	%v4,%v4,%v9
	.word	0xe744,0x0007,0x2033	# verll	%v4,%v4,7,2
	brct	%r0,.Loop_4x
	.word	0xe7cc,0xa000,0x22f3	# va	%v12,%v12,%v26,2
	.word	0xe7b0,0x1000,0x2861	# vmrh	%v27,%v0,%v1,2
	.word	0xe7c2,0x3000,0x2861	# vmrh	%v28,%v2,%v3,2
	.word	0xe7d0,0x1000,0x2860	# vmrl	%v29,%v0,%v1,2
	.word	0xe7e2,0x3000,0x2860	# vmrl	%v30,%v2,%v3,2
	.word	0xe70b,0xc000,0x0684	# vpdi	%v0,%v27,%v28,0
	.word	0xe71b,0xc000,0x5684	# vpdi	%v1,%v27,%v28,5
	.word	0xe72d,0xe000,0x0684	# vpdi	%v2,%v29,%v30,0
	.word	0xe73d,0xe000,0x5684	# vpdi	%v3,%v29,%v30,5
	.word	0xe7b4,0x5000,0x2861	# vmrh	%v27,%v4,%v5,2
	.word	0xe7c6,0x7000,0x2861	# vmrh	%v28,%v6,%v7,2
	.word	0xe7d4,0x5000,0x2860	# vmrl	%v29,%v4,%v5,2
	.word	0xe7e6,0x7000,0x2860	# vmrl	%v30,%v6,%v7,2
	.word	0xe74b,0xc000,0x0684	# vpdi	%v4,%v27,%v28,0
	.word	0xe75b,0xc000,0x5684	# vpdi	%v5,%v27,%v28,5
	.word	0xe76d,0xe000,0x0684	# vpdi	%v6,%v29,%v30,0
	.word	0xe77d,0xe000,0x5684	# vpdi	%v7,%v29,%v30,5
	.word	0xe7b8,0x9000,0x2861	# vmrh	%v27,%v8,%v9,2
	.word	0xe7ca,0xb000,0x2861	# vmrh	%v28,%v10,%v11,2
	.word	0xe7d8,0x9000,0x2860	# vmrl	%v29,%v8,%v9,2
	.word	0xe7ea,0xb000,0x2860	# vmrl	%v30,%v10,%v11,2
	.word	0xe78b,0xc000,0x0684	# vpdi	%v8,%v27,%v28,0
	.word	0xe79b,0xc000,0x5684	# vpdi	%v9,%v27,%v28,5
	.word	0xe7ad,0xe000,0x0684	# vpdi	%v10,%v29,%v30,0
	.word	0xe7bd,0xe000,0x5684	# vpdi	%v11,%v29,%v30,5
	.word	0xe7bc,0xd000,0x2861	# vmrh	%v27,%v12,%v13,2
	.word	0xe7ce,0xf000,0x2861	# vmrh	%v28,%v14,%v15,2
	.word	0xe7dc,0xd000,0x2860	# vmrl	%v29,%v12,%v13,2
	.word	0xe7ee,0xf000,0x2860	# vmrl	%v30,%v14,%v15,2
	.word	0xe7cb,0xc000,0x0684	# vpdi	%v12,%v27,%v28,0
	.word	0xe7db,0xc000,0x5684	# vpdi	%v13,%v27,%v28,5
	.word	0xe7ed,0xe000,0x0684	# vpdi	%v14,%v29,%v30,0
	.word	0xe7fd,0xe000,0x5684	# vpdi	%v15,%v29,%v30,5
	.word	0xe700,000000,0x22f3	# va	%v0,%v0,%v16,2
	.word	0xe744,0x1000,0x22f3	# va	%v4,%v4,%v17,2
	.word	0xe788,0x2000,0x22f3	# va	%v8,%v8,%v18,2
	.word	0xe7cc,0x3000,0x22f3	# va	%v12,%v12,%v19,2
	.word	0xe700,000000,0xf18c	# vperm	%v0,%v0,%v0,%v31
	.word	0xe744,0x4000,0xf18c	# vperm	%v4,%v4,%v4,%v31
	.word	0xe788,0x8000,0xf18c	# vperm	%v8,%v8,%v8,%v31
	.word	0xe7cc,0xc000,0xf18c	# vperm	%v12,%v12,%v12,%v31
	.word	0xe7be,0x3000,0x0c36	# vlm	%v27,%v30,0(%r3)
	.word	0xe7bb,000000,0x0c6d	# vx	%v27,%v27,%v0
	.word	0xe7cc,0x4000,0x0c6d	# vx	%v28,%v28,%v4
	.word	0xe7dd,0x8000,0x0c6d	# vx	%v29,%v29,%v8
	.word	0xe7ee,0xc000,0x0c6d	# vx	%v30,%v30,%v12
	.word	0xe7be,0x2000,0x0c3e	# vstm	%v27,%v30,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	.word	0xe701,000000,0x22f3	# va	%v0,%v1,%v16,2
	.word	0xe745,0x1000,0x22f3	# va	%v4,%v5,%v17,2
	.word	0xe789,0x2000,0x22f3	# va	%v8,%v9,%v18,2
	.word	0xe7cd,0x3000,0x22f3	# va	%v12,%v13,%v19,2
	.word	0xe700,000000,0xf18c	# vperm	%v0,%v0,%v0,%v31
	.word	0xe744,0x4000,0xf18c	# vperm	%v4,%v4,%v4,%v31
	.word	0xe788,0x8000,0xf18c	# vperm	%v8,%v8,%v8,%v31
	.word	0xe7cc,0xc000,0xf18c	# vperm	%v12,%v12,%v12,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_4x
	.word	0xe7be,0x3000,0x0c36	# vlm	%v27,%v30,0(%r3)
	.word	0xe7bb,000000,0x0c6d	# vx	%v27,%v27,%v0
	.word	0xe7cc,0x4000,0x0c6d	# vx	%v28,%v28,%v4
	.word	0xe7dd,0x8000,0x0c6d	# vx	%v29,%v29,%v8
	.word	0xe7ee,0xc000,0x0c6d	# vx	%v30,%v30,%v12
	.word	0xe7be,0x2000,0x0c3e	# vstm	%v27,%v30,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_4x
	.word	0xe702,000000,0x22f3	# va	%v0,%v2,%v16,2
	.word	0xe746,0x1000,0x22f3	# va	%v4,%v6,%v17,2
	.word	0xe78a,0x2000,0x22f3	# va	%v8,%v10,%v18,2
	.word	0xe7ce,0x3000,0x22f3	# va	%v12,%v14,%v19,2
	.word	0xe700,000000,0xf18c	# vperm	%v0,%v0,%v0,%v31
	.word	0xe744,0x4000,0xf18c	# vperm	%v4,%v4,%v4,%v31
	.word	0xe788,0x8000,0xf18c	# vperm	%v8,%v8,%v8,%v31
	.word	0xe7cc,0xc000,0xf18c	# vperm	%v12,%v12,%v12,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_4x
	.word	0xe7be,0x3000,0x0c36	# vlm	%v27,%v30,0(%r3)
	.word	0xe7bb,000000,0x0c6d	# vx	%v27,%v27,%v0
	.word	0xe7cc,0x4000,0x0c6d	# vx	%v28,%v28,%v4
	.word	0xe7dd,0x8000,0x0c6d	# vx	%v29,%v29,%v8
	.word	0xe7ee,0xc000,0x0c6d	# vx	%v30,%v30,%v12
	.word	0xe7be,0x2000,0x0c3e	# vstm	%v27,%v30,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_4x
	.word	0xe703,000000,0x22f3	# va	%v0,%v3,%v16,2
	.word	0xe747,0x1000,0x22f3	# va	%v4,%v7,%v17,2
	.word	0xe78b,0x2000,0x22f3	# va	%v8,%v11,%v18,2
	.word	0xe7cf,0x3000,0x22f3	# va	%v12,%v15,%v19,2
	.word	0xe700,000000,0xf18c	# vperm	%v0,%v0,%v0,%v31
	.word	0xe744,0x4000,0xf18c	# vperm	%v4,%v4,%v4,%v31
	.word	0xe788,0x8000,0xf18c	# vperm	%v8,%v8,%v8,%v31
	.word	0xe7cc,0xc000,0xf18c	# vperm	%v12,%v12,%v12,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_4x
	.word	0xe7be,0x3000,0x0c36	# vlm	%v27,%v30,0(%r3)
	.word	0xe7bb,000000,0x0c6d	# vx	%v27,%v27,%v0
	.word	0xe7cc,0x4000,0x0c6d	# vx	%v28,%v28,%v4
	.word	0xe7dd,0x8000,0x0c6d	# vx	%v29,%v29,%v8
	.word	0xe7ee,0xc000,0x0c6d	# vx	%v30,%v30,%v12
	.word	0xe7be,0x2000,0x0c3e	# vstm	%v27,%v30,0(%r2)
.Ldone_4x:
	ld	%f8,160+8*0(%r15)
	ld	%f9,160+8*1(%r15)
	ld	%f10,160+8*2(%r15)
	ld	%f11,160+8*3(%r15)
	ld	%f12,160+8*4(%r15)
	ld	%f13,160+8*5(%r15)
	ld	%f14,160+8*6(%r15)
	ld	%f15,160+8*7(%r15)
	lmg	%r6,%r7,224+6*8(%r15)
	la	%r15,224(%r15)
	br	%r14
.align	16
.Ltail_4x:
	.word	0xe7b8,000000,0x0856	# vlr	%v27,%v8
	ld	%f8,160+8*0(%r15)
	ld	%f9,160+8*1(%r15)
	ld	%f10,160+8*2(%r15)
	ld	%f11,160+8*3(%r15)
	.word	0xe7cc,000000,0x0856	# vlr	%v28,%v12
	ld	%f12,160+8*4(%r15)
	ld	%f13,160+8*5(%r15)
	ld	%f14,160+8*6(%r15)
	ld	%f15,160+8*7(%r15)
	.word	0xe700,0xf0a0,0x000e	# vst	%v0,160+0x00(%r15)
	.word	0xe740,0xf0b0,0x000e	# vst	%v4,160+0x10(%r15)
	.word	0xe7b0,0xf0c0,0x080e	# vst	%v27,160+0x20(%r15)
	.word	0xe7c0,0xf0d0,0x080e	# vst	%v28,160+0x30(%r15)
	lghi	%r1,0
.Loop_tail_4x:
	llgc	%r5,0(%r1,%r3)
	llgc	%r6,160(%r1,%r15)
	xr	%r6,%r5
	stc	%r6,0(%r1,%r2)
	la	%r1,1(%r1)
	brct	%r4,.Loop_tail_4x
	lmg	%r6,%r7,224+6*8(%r15)
	la	%r15,224(%r15)
	br	%r14
.size	ChaCha20_ctr32_4x,.-ChaCha20_ctr32_4x
.globl	ChaCha20_ctr32_vx
.align	32
ChaCha20_ctr32_vx:
.LChaCha20_ctr32_vx:
	.word	0xc24e,000000,0x0100	# clgfi	%r4,256
	jle	.LChaCha20_ctr32_4x
	stmg	%r6,%r7,6*8(%r15)
	lghi	%r1,-224
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)
	std	%f8,224-8*8(%r15)
	std	%f9,224-8*7(%r15)
	std	%f10,224-8*6(%r15)
	std	%f11,224-8*5(%r15)
	std	%f12,224-8*4(%r15)
	std	%f13,224-8*3(%r15)
	std	%f14,224-8*2(%r15)
	std	%f15,224-8*1(%r15)
	larl	%r7,.Lsigma
	lhi	%r0,10
	.word	0xe789,0x5000,0x0c36	# vlm	%v24,%v25,0(%r5)
	.word	0xe7a0,0x6000,0x0806	# vl	%v26,0(%r6)
	.word	0xe7bf,0x7000,0x0c36	# vlm	%v27,%v31,0(%r7)
.Loop_outer_vx:
	.word	0xe70b,000000,0x0456	# vlr	%v0,%v27
	.word	0xe718,000000,0x0456	# vlr	%v1,%v24
	.word	0xe74b,000000,0x0456	# vlr	%v4,%v27
	.word	0xe758,000000,0x0456	# vlr	%v5,%v24
	.word	0xe78b,000000,0x0456	# vlr	%v8,%v27
	.word	0xe798,000000,0x0456	# vlr	%v9,%v24
	.word	0xe7cb,000000,0x0456	# vlr	%v12,%v27
	.word	0xe7d8,000000,0x0456	# vlr	%v13,%v24
	.word	0xe70b,000000,0x0c56	# vlr	%v16,%v27
	.word	0xe718,000000,0x0c56	# vlr	%v17,%v24
	.word	0xe74b,000000,0x0c56	# vlr	%v20,%v27
	.word	0xe758,000000,0x0c56	# vlr	%v21,%v24
	.word	0xe73a,000000,0x0456	# vlr	%v3,%v26
	.word	0xe77a,0xc000,0x26f3	# va	%v7,%v26,%v28,2
	.word	0xe7ba,0xd000,0x26f3	# va	%v11,%v26,%v29,2
	.word	0xe7fa,0xe000,0x26f3	# va	%v15,%v26,%v30,2
	.word	0xe73b,0xd000,0x2af3	# va	%v19,%v11,%v29,2
	.word	0xe77b,0xe000,0x2af3	# va	%v23,%v11,%v30,2
	.word	0xe729,000000,0x0456	# vlr	%v2,%v25
	.word	0xe769,000000,0x0456	# vlr	%v6,%v25
	.word	0xe7a9,000000,0x0456	# vlr	%v10,%v25
	.word	0xe7e9,000000,0x0456	# vlr	%v14,%v25
	.word	0xe729,000000,0x0c56	# vlr	%v18,%v25
	.word	0xe769,000000,0x0c56	# vlr	%v22,%v25
	.word	0xe7c7,000000,0x0856	# vlr	%v28,%v7
	.word	0xe7db,000000,0x0856	# vlr	%v29,%v11
	.word	0xe7ef,000000,0x0856	# vlr	%v30,%v15
.align	4
.Loop_vx:
	.word	0xe700,0x1000,0x20f3	# va	%v0,%v0,%v1,2
	.word	0xe744,0x5000,0x20f3	# va	%v4,%v4,%v5,2
	.word	0xe788,0x9000,0x20f3	# va	%v8,%v8,%v9,2
	.word	0xe7cc,0xd000,0x20f3	# va	%v12,%v12,%v13,2
	.word	0xe700,0x1000,0x2ef3	# va	%v16,%v16,%v17,2
	.word	0xe744,0x5000,0x2ef3	# va	%v20,%v20,%v21,2
	.word	0xe733,000000,0x006d	# vx	%v3,%v3,%v0
	.word	0xe777,0x4000,0x006d	# vx	%v7,%v7,%v4
	.word	0xe7bb,0x8000,0x006d	# vx	%v11,%v11,%v8
	.word	0xe7ff,0xc000,0x006d	# vx	%v15,%v15,%v12
	.word	0xe733,000000,0x0e6d	# vx	%v19,%v19,%v16
	.word	0xe777,0x4000,0x0e6d	# vx	%v23,%v23,%v20
	.word	0xe733,0x0010,0x2033	# verll	%v3,%v3,16,2
	.word	0xe777,0x0010,0x2033	# verll	%v7,%v7,16,2
	.word	0xe7bb,0x0010,0x2033	# verll	%v11,%v11,16,2
	.word	0xe7ff,0x0010,0x2033	# verll	%v15,%v15,16,2
	.word	0xe733,0x0010,0x2c33	# verll	%v19,%v19,16,2
	.word	0xe777,0x0010,0x2c33	# verll	%v23,%v23,16,2
	.word	0xe722,0x3000,0x20f3	# va	%v2,%v2,%v3,2
	.word	0xe766,0x7000,0x20f3	# va	%v6,%v6,%v7,2
	.word	0xe7aa,0xb000,0x20f3	# va	%v10,%v10,%v11,2
	.word	0xe7ee,0xf000,0x20f3	# va	%v14,%v14,%v15,2
	.word	0xe722,0x3000,0x2ef3	# va	%v18,%v18,%v19,2
	.word	0xe766,0x7000,0x2ef3	# va	%v22,%v22,%v23,2
	.word	0xe711,0x2000,0x006d	# vx	%v1,%v1,%v2
	.word	0xe755,0x6000,0x006d	# vx	%v5,%v5,%v6
	.word	0xe799,0xa000,0x006d	# vx	%v9,%v9,%v10
	.word	0xe7dd,0xe000,0x006d	# vx	%v13,%v13,%v14
	.word	0xe711,0x2000,0x0e6d	# vx	%v17,%v17,%v18
	.word	0xe755,0x6000,0x0e6d	# vx	%v21,%v21,%v22
	.word	0xe711,0x000c,0x2033	# verll	%v1,%v1,12,2
	.word	0xe755,0x000c,0x2033	# verll	%v5,%v5,12,2
	.word	0xe799,0x000c,0x2033	# verll	%v9,%v9,12,2
	.word	0xe7dd,0x000c,0x2033	# verll	%v13,%v13,12,2
	.word	0xe711,0x000c,0x2c33	# verll	%v17,%v17,12,2
	.word	0xe755,0x000c,0x2c33	# verll	%v21,%v21,12,2
	.word	0xe700,0x1000,0x20f3	# va	%v0,%v0,%v1,2
	.word	0xe744,0x5000,0x20f3	# va	%v4,%v4,%v5,2
	.word	0xe788,0x9000,0x20f3	# va	%v8,%v8,%v9,2
	.word	0xe7cc,0xd000,0x20f3	# va	%v12,%v12,%v13,2
	.word	0xe700,0x1000,0x2ef3	# va	%v16,%v16,%v17,2
	.word	0xe744,0x5000,0x2ef3	# va	%v20,%v20,%v21,2
	.word	0xe733,000000,0x006d	# vx	%v3,%v3,%v0
	.word	0xe777,0x4000,0x006d	# vx	%v7,%v7,%v4
	.word	0xe7bb,0x8000,0x006d	# vx	%v11,%v11,%v8
	.word	0xe7ff,0xc000,0x006d	# vx	%v15,%v15,%v12
	.word	0xe733,000000,0x0e6d	# vx	%v19,%v19,%v16
	.word	0xe777,0x4000,0x0e6d	# vx	%v23,%v23,%v20
	.word	0xe733,0x0008,0x2033	# verll	%v3,%v3,8,2
	.word	0xe777,0x0008,0x2033	# verll	%v7,%v7,8,2
	.word	0xe7bb,0x0008,0x2033	# verll	%v11,%v11,8,2
	.word	0xe7ff,0x0008,0x2033	# verll	%v15,%v15,8,2
	.word	0xe733,0x0008,0x2c33	# verll	%v19,%v19,8,2
	.word	0xe777,0x0008,0x2c33	# verll	%v23,%v23,8,2
	.word	0xe722,0x3000,0x20f3	# va	%v2,%v2,%v3,2
	.word	0xe766,0x7000,0x20f3	# va	%v6,%v6,%v7,2
	.word	0xe7aa,0xb000,0x20f3	# va	%v10,%v10,%v11,2
	.word	0xe7ee,0xf000,0x20f3	# va	%v14,%v14,%v15,2
	.word	0xe722,0x3000,0x2ef3	# va	%v18,%v18,%v19,2
	.word	0xe766,0x7000,0x2ef3	# va	%v22,%v22,%v23,2
	.word	0xe711,0x2000,0x006d	# vx	%v1,%v1,%v2
	.word	0xe755,0x6000,0x006d	# vx	%v5,%v5,%v6
	.word	0xe799,0xa000,0x006d	# vx	%v9,%v9,%v10
	.word	0xe7dd,0xe000,0x006d	# vx	%v13,%v13,%v14
	.word	0xe711,0x2000,0x0e6d	# vx	%v17,%v17,%v18
	.word	0xe755,0x6000,0x0e6d	# vx	%v21,%v21,%v22
	.word	0xe711,0x0007,0x2033	# verll	%v1,%v1,7,2
	.word	0xe755,0x0007,0x2033	# verll	%v5,%v5,7,2
	.word	0xe799,0x0007,0x2033	# verll	%v9,%v9,7,2
	.word	0xe7dd,0x0007,0x2033	# verll	%v13,%v13,7,2
	.word	0xe711,0x0007,0x2c33	# verll	%v17,%v17,7,2
	.word	0xe755,0x0007,0x2c33	# verll	%v21,%v21,7,2
	.word	0xe722,0x2008,0x0077	# vsldb	%v2,%v2,%v2,8
	.word	0xe766,0x6008,0x0077	# vsldb	%v6,%v6,%v6,8
	.word	0xe7aa,0xa008,0x0077	# vsldb	%v10,%v10,%v10,8
	.word	0xe7ee,0xe008,0x0077	# vsldb	%v14,%v14,%v14,8
	.word	0xe722,0x2008,0x0e77	# vsldb	%v18,%v18,%v18,8
	.word	0xe766,0x6008,0x0e77	# vsldb	%v22,%v22,%v22,8
	.word	0xe711,0x1004,0x0077	# vsldb	%v1,%v1,%v1,4
	.word	0xe755,0x5004,0x0077	# vsldb	%v5,%v5,%v5,4
	.word	0xe799,0x9004,0x0077	# vsldb	%v9,%v9,%v9,4
	.word	0xe7dd,0xd004,0x0077	# vsldb	%v13,%v13,%v13,4
	.word	0xe711,0x1004,0x0e77	# vsldb	%v17,%v17,%v17,4
	.word	0xe755,0x5004,0x0e77	# vsldb	%v21,%v21,%v21,4
	.word	0xe733,0x300c,0x0077	# vsldb	%v3,%v3,%v3,12
	.word	0xe777,0x700c,0x0077	# vsldb	%v7,%v7,%v7,12
	.word	0xe7bb,0xb00c,0x0077	# vsldb	%v11,%v11,%v11,12
	.word	0xe7ff,0xf00c,0x0077	# vsldb	%v15,%v15,%v15,12
	.word	0xe733,0x300c,0x0e77	# vsldb	%v19,%v19,%v19,12
	.word	0xe777,0x700c,0x0e77	# vsldb	%v23,%v23,%v23,12
	.word	0xe700,0x1000,0x20f3	# va	%v0,%v0,%v1,2
	.word	0xe744,0x5000,0x20f3	# va	%v4,%v4,%v5,2
	.word	0xe788,0x9000,0x20f3	# va	%v8,%v8,%v9,2
	.word	0xe7cc,0xd000,0x20f3	# va	%v12,%v12,%v13,2
	.word	0xe700,0x1000,0x2ef3	# va	%v16,%v16,%v17,2
	.word	0xe744,0x5000,0x2ef3	# va	%v20,%v20,%v21,2
	.word	0xe733,000000,0x006d	# vx	%v3,%v3,%v0
	.word	0xe777,0x4000,0x006d	# vx	%v7,%v7,%v4
	.word	0xe7bb,0x8000,0x006d	# vx	%v11,%v11,%v8
	.word	0xe7ff,0xc000,0x006d	# vx	%v15,%v15,%v12
	.word	0xe733,000000,0x0e6d	# vx	%v19,%v19,%v16
	.word	0xe777,0x4000,0x0e6d	# vx	%v23,%v23,%v20
	.word	0xe733,0x0010,0x2033	# verll	%v3,%v3,16,2
	.word	0xe777,0x0010,0x2033	# verll	%v7,%v7,16,2
	.word	0xe7bb,0x0010,0x2033	# verll	%v11,%v11,16,2
	.word	0xe7ff,0x0010,0x2033	# verll	%v15,%v15,16,2
	.word	0xe733,0x0010,0x2c33	# verll	%v19,%v19,16,2
	.word	0xe777,0x0010,0x2c33	# verll	%v23,%v23,16,2
	.word	0xe722,0x3000,0x20f3	# va	%v2,%v2,%v3,2
	.word	0xe766,0x7000,0x20f3	# va	%v6,%v6,%v7,2
	.word	0xe7aa,0xb000,0x20f3	# va	%v10,%v10,%v11,2
	.word	0xe7ee,0xf000,0x20f3	# va	%v14,%v14,%v15,2
	.word	0xe722,0x3000,0x2ef3	# va	%v18,%v18,%v19,2
	.word	0xe766,0x7000,0x2ef3	# va	%v22,%v22,%v23,2
	.word	0xe711,0x2000,0x006d	# vx	%v1,%v1,%v2
	.word	0xe755,0x6000,0x006d	# vx	%v5,%v5,%v6
	.word	0xe799,0xa000,0x006d	# vx	%v9,%v9,%v10
	.word	0xe7dd,0xe000,0x006d	# vx	%v13,%v13,%v14
	.word	0xe711,0x2000,0x0e6d	# vx	%v17,%v17,%v18
	.word	0xe755,0x6000,0x0e6d	# vx	%v21,%v21,%v22
	.word	0xe711,0x000c,0x2033	# verll	%v1,%v1,12,2
	.word	0xe755,0x000c,0x2033	# verll	%v5,%v5,12,2
	.word	0xe799,0x000c,0x2033	# verll	%v9,%v9,12,2
	.word	0xe7dd,0x000c,0x2033	# verll	%v13,%v13,12,2
	.word	0xe711,0x000c,0x2c33	# verll	%v17,%v17,12,2
	.word	0xe755,0x000c,0x2c33	# verll	%v21,%v21,12,2
	.word	0xe700,0x1000,0x20f3	# va	%v0,%v0,%v1,2
	.word	0xe744,0x5000,0x20f3	# va	%v4,%v4,%v5,2
	.word	0xe788,0x9000,0x20f3	# va	%v8,%v8,%v9,2
	.word	0xe7cc,0xd000,0x20f3	# va	%v12,%v12,%v13,2
	.word	0xe700,0x1000,0x2ef3	# va	%v16,%v16,%v17,2
	.word	0xe744,0x5000,0x2ef3	# va	%v20,%v20,%v21,2
	.word	0xe733,000000,0x006d	# vx	%v3,%v3,%v0
	.word	0xe777,0x4000,0x006d	# vx	%v7,%v7,%v4
	.word	0xe7bb,0x8000,0x006d	# vx	%v11,%v11,%v8
	.word	0xe7ff,0xc000,0x006d	# vx	%v15,%v15,%v12
	.word	0xe733,000000,0x0e6d	# vx	%v19,%v19,%v16
	.word	0xe777,0x4000,0x0e6d	# vx	%v23,%v23,%v20
	.word	0xe733,0x0008,0x2033	# verll	%v3,%v3,8,2
	.word	0xe777,0x0008,0x2033	# verll	%v7,%v7,8,2
	.word	0xe7bb,0x0008,0x2033	# verll	%v11,%v11,8,2
	.word	0xe7ff,0x0008,0x2033	# verll	%v15,%v15,8,2
	.word	0xe733,0x0008,0x2c33	# verll	%v19,%v19,8,2
	.word	0xe777,0x0008,0x2c33	# verll	%v23,%v23,8,2
	.word	0xe722,0x3000,0x20f3	# va	%v2,%v2,%v3,2
	.word	0xe766,0x7000,0x20f3	# va	%v6,%v6,%v7,2
	.word	0xe7aa,0xb000,0x20f3	# va	%v10,%v10,%v11,2
	.word	0xe7ee,0xf000,0x20f3	# va	%v14,%v14,%v15,2
	.word	0xe722,0x3000,0x2ef3	# va	%v18,%v18,%v19,2
	.word	0xe766,0x7000,0x2ef3	# va	%v22,%v22,%v23,2
	.word	0xe711,0x2000,0x006d	# vx	%v1,%v1,%v2
	.word	0xe755,0x6000,0x006d	# vx	%v5,%v5,%v6
	.word	0xe799,0xa000,0x006d	# vx	%v9,%v9,%v10
	.word	0xe7dd,0xe000,0x006d	# vx	%v13,%v13,%v14
	.word	0xe711,0x2000,0x0e6d	# vx	%v17,%v17,%v18
	.word	0xe755,0x6000,0x0e6d	# vx	%v21,%v21,%v22
	.word	0xe711,0x0007,0x2033	# verll	%v1,%v1,7,2
	.word	0xe755,0x0007,0x2033	# verll	%v5,%v5,7,2
	.word	0xe799,0x0007,0x2033	# verll	%v9,%v9,7,2
	.word	0xe7dd,0x0007,0x2033	# verll	%v13,%v13,7,2
	.word	0xe711,0x0007,0x2c33	# verll	%v17,%v17,7,2
	.word	0xe755,0x0007,0x2c33	# verll	%v21,%v21,7,2
	.word	0xe722,0x2008,0x0077	# vsldb	%v2,%v2,%v2,8
	.word	0xe766,0x6008,0x0077	# vsldb	%v6,%v6,%v6,8
	.word	0xe7aa,0xa008,0x0077	# vsldb	%v10,%v10,%v10,8
	.word	0xe7ee,0xe008,0x0077	# vsldb	%v14,%v14,%v14,8
	.word	0xe722,0x2008,0x0e77	# vsldb	%v18,%v18,%v18,8
	.word	0xe766,0x6008,0x0e77	# vsldb	%v22,%v22,%v22,8
	.word	0xe711,0x100c,0x0077	# vsldb	%v1,%v1,%v1,12
	.word	0xe755,0x500c,0x0077	# vsldb	%v5,%v5,%v5,12
	.word	0xe799,0x900c,0x0077	# vsldb	%v9,%v9,%v9,12
	.word	0xe7dd,0xd00c,0x0077	# vsldb	%v13,%v13,%v13,12
	.word	0xe711,0x100c,0x0e77	# vsldb	%v17,%v17,%v17,12
	.word	0xe755,0x500c,0x0e77	# vsldb	%v21,%v21,%v21,12
	.word	0xe733,0x3004,0x0077	# vsldb	%v3,%v3,%v3,4
	.word	0xe777,0x7004,0x0077	# vsldb	%v7,%v7,%v7,4
	.word	0xe7bb,0xb004,0x0077	# vsldb	%v11,%v11,%v11,4
	.word	0xe7ff,0xf004,0x0077	# vsldb	%v15,%v15,%v15,4
	.word	0xe733,0x3004,0x0e77	# vsldb	%v19,%v19,%v19,4
	.word	0xe777,0x7004,0x0e77	# vsldb	%v23,%v23,%v23,4
	brct	%r0,.Loop_vx
	.word	0xe700,0xb000,0x22f3	# va	%v0,%v0,%v27,2
	.word	0xe711,0x8000,0x22f3	# va	%v1,%v1,%v24,2
	.word	0xe722,0x9000,0x22f3	# va	%v2,%v2,%v25,2
	.word	0xe733,0xa000,0x22f3	# va	%v3,%v3,%v26,2
	.word	0xe744,0xb000,0x22f3	# va	%v4,%v4,%v27,2
	.word	0xe777,0xc000,0x22f3	# va	%v7,%v7,%v28,2
	.word	0xe700,000000,0xf18c	# vperm	%v0,%v0,%v0,%v31
	.word	0xe711,0x1000,0xf18c	# vperm	%v1,%v1,%v1,%v31
	.word	0xe722,0x2000,0xf18c	# vperm	%v2,%v2,%v2,%v31
	.word	0xe733,0x3000,0xf18c	# vperm	%v3,%v3,%v3,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe7bb,0xd000,0x22f3	# va	%v11,%v11,%v29,2
	.word	0xe7ff,0xe000,0x22f3	# va	%v15,%v15,%v30,2
	.word	0xe7be,0x3000,0x0c36	# vlm	%v27,%v30,0(%r3)
	.word	0xe700,0xb000,0x026d	# vx	%v0,%v0,%v27
	.word	0xe711,0xc000,0x026d	# vx	%v1,%v1,%v28
	.word	0xe722,0xd000,0x026d	# vx	%v2,%v2,%v29
	.word	0xe733,0xe000,0x026d	# vx	%v3,%v3,%v30
	.word	0xe7be,0x7000,0x0c36	# vlm	%v27,%v30,0(%r7)
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_vx
	.word	0xe755,0x8000,0x22f3	# va	%v5,%v5,%v24,2
	.word	0xe766,0x9000,0x22f3	# va	%v6,%v6,%v25,2
	.word	0xe704,0x4000,0xf18c	# vperm	%v0,%v4,%v4,%v31
	.word	0xe715,0x5000,0xf18c	# vperm	%v1,%v5,%v5,%v31
	.word	0xe726,0x6000,0xf18c	# vperm	%v2,%v6,%v6,%v31
	.word	0xe737,0x7000,0xf18c	# vperm	%v3,%v7,%v7,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe747,0x3000,0x0036	# vlm	%v4,%v7,0(%r3)
	.word	0xe700,0x4000,0x006d	# vx	%v0,%v0,%v4
	.word	0xe711,0x5000,0x006d	# vx	%v1,%v1,%v5
	.word	0xe722,0x6000,0x006d	# vx	%v2,%v2,%v6
	.word	0xe733,0x7000,0x006d	# vx	%v3,%v3,%v7
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_vx
	.word	0xe788,0xb000,0x22f3	# va	%v8,%v8,%v27,2
	.word	0xe799,0x8000,0x22f3	# va	%v9,%v9,%v24,2
	.word	0xe7aa,0x9000,0x22f3	# va	%v10,%v10,%v25,2
	.word	0xe708,0x8000,0xf18c	# vperm	%v0,%v8,%v8,%v31
	.word	0xe719,0x9000,0xf18c	# vperm	%v1,%v9,%v9,%v31
	.word	0xe72a,0xa000,0xf18c	# vperm	%v2,%v10,%v10,%v31
	.word	0xe73b,0xb000,0xf18c	# vperm	%v3,%v11,%v11,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe747,0x3000,0x0036	# vlm	%v4,%v7,0(%r3)
	.word	0xe700,0x4000,0x006d	# vx	%v0,%v0,%v4
	.word	0xe711,0x5000,0x006d	# vx	%v1,%v1,%v5
	.word	0xe722,0x6000,0x006d	# vx	%v2,%v2,%v6
	.word	0xe733,0x7000,0x006d	# vx	%v3,%v3,%v7
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_vx
	.word	0xe7cc,0xb000,0x22f3	# va	%v12,%v12,%v27,2
	.word	0xe7dd,0x8000,0x22f3	# va	%v13,%v13,%v24,2
	.word	0xe7ee,0x9000,0x22f3	# va	%v14,%v14,%v25,2
	.word	0xe7ba,0xe000,0x26f3	# va	%v11,%v26,%v30,2
	.word	0xe70c,0xc000,0xf18c	# vperm	%v0,%v12,%v12,%v31
	.word	0xe71d,0xd000,0xf18c	# vperm	%v1,%v13,%v13,%v31
	.word	0xe72e,0xe000,0xf18c	# vperm	%v2,%v14,%v14,%v31
	.word	0xe73f,0xf000,0xf18c	# vperm	%v3,%v15,%v15,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe7fb,0xc000,0x22f3	# va	%v15,%v11,%v28,2
	.word	0xe747,0x3000,0x0036	# vlm	%v4,%v7,0(%r3)
	.word	0xe700,0x4000,0x006d	# vx	%v0,%v0,%v4
	.word	0xe711,0x5000,0x006d	# vx	%v1,%v1,%v5
	.word	0xe722,0x6000,0x006d	# vx	%v2,%v2,%v6
	.word	0xe733,0x7000,0x006d	# vx	%v3,%v3,%v7
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_vx
	.word	0xe700,0xb000,0x2ef3	# va	%v16,%v16,%v27,2
	.word	0xe711,0x8000,0x2ef3	# va	%v17,%v17,%v24,2
	.word	0xe722,0x9000,0x2ef3	# va	%v18,%v18,%v25,2
	.word	0xe733,0xf000,0x2cf3	# va	%v19,%v19,%v15,2
	.word	0xe7ff,0xc000,0x22f3	# va	%v15,%v15,%v28,2
	.word	0xe7ab,0xe000,0x2af3	# va	%v26,%v11,%v30,2
	.word	0xe700,000000,0xf78c	# vperm	%v0,%v16,%v16,%v31
	.word	0xe711,0x1000,0xf78c	# vperm	%v1,%v17,%v17,%v31
	.word	0xe722,0x2000,0xf78c	# vperm	%v2,%v18,%v18,%v31
	.word	0xe733,0x3000,0xf78c	# vperm	%v3,%v19,%v19,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe747,0x3000,0x0036	# vlm	%v4,%v7,0(%r3)
	.word	0xe700,0x4000,0x006d	# vx	%v0,%v0,%v4
	.word	0xe711,0x5000,0x006d	# vx	%v1,%v1,%v5
	.word	0xe722,0x6000,0x006d	# vx	%v2,%v2,%v6
	.word	0xe733,0x7000,0x006d	# vx	%v3,%v3,%v7
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	aghi	%r4,-64
	je	.Ldone_vx
	.word	0xe744,0xb000,0x2ef3	# va	%v20,%v20,%v27,2
	.word	0xe755,0x8000,0x2ef3	# va	%v21,%v21,%v24,2
	.word	0xe766,0x9000,0x2ef3	# va	%v22,%v22,%v25,2
	.word	0xe777,0xf000,0x2cf3	# va	%v23,%v23,%v15,2
	.word	0xe704,0x4000,0xf78c	# vperm	%v0,%v20,%v20,%v31
	.word	0xe715,0x5000,0xf78c	# vperm	%v1,%v21,%v21,%v31
	.word	0xe726,0x6000,0xf78c	# vperm	%v2,%v22,%v22,%v31
	.word	0xe737,0x7000,0xf78c	# vperm	%v3,%v23,%v23,%v31
	.word	0xc24e,000000,0x0040	# clgfi	%r4,64
	jl	.Ltail_vx
	.word	0xe747,0x3000,0x0036	# vlm	%v4,%v7,0(%r3)
	.word	0xe700,0x4000,0x006d	# vx	%v0,%v0,%v4
	.word	0xe711,0x5000,0x006d	# vx	%v1,%v1,%v5
	.word	0xe722,0x6000,0x006d	# vx	%v2,%v2,%v6
	.word	0xe733,0x7000,0x006d	# vx	%v3,%v3,%v7
	.word	0xe703,0x2000,0x003e	# vstm	%v0,%v3,0(%r2)
	la	%r3,0x40(%r3)
	la	%r2,0x40(%r2)
	lhi	%r0,10
	aghi	%r4,-64
	jne	.Loop_outer_vx
.Ldone_vx:
	ld	%f8,224-8*8(%r15)
	ld	%f9,224-8*7(%r15)
	ld	%f10,224-8*6(%r15)
	ld	%f11,224-8*5(%r15)
	ld	%f12,224-8*4(%r15)
	ld	%f13,224-8*3(%r15)
	ld	%f14,224-8*2(%r15)
	ld	%f15,224-8*1(%r15)
	lmg	%r6,%r7,224+6*8(%r15)
	la	%r15,224(%r15)
	br	%r14
.align	16
.Ltail_vx:
	ld	%f8,224-8*8(%r15)
	ld	%f9,224-8*7(%r15)
	ld	%f10,224-8*6(%r15)
	ld	%f11,224-8*5(%r15)
	ld	%f12,224-8*4(%r15)
	ld	%f13,224-8*3(%r15)
	ld	%f14,224-8*2(%r15)
	ld	%f15,224-8*1(%r15)
	.word	0xe703,0xf0a0,0x003e	# vstm	%v0,%v3,160(%r15)
	lghi	%r1,0
.Loop_tail_vx:
	llgc	%r5,0(%r1,%r3)
	llgc	%r6,160(%r1,%r15)
	xr	%r6,%r5
	stc	%r6,0(%r1,%r2)
	la	%r1,1(%r1)
	brct	%r4,.Loop_tail_vx
	lmg	%r6,%r7,224+6*8(%r15)
	la	%r15,224(%r15)
	br	%r14
.size	ChaCha20_ctr32_vx,.-ChaCha20_ctr32_vx
.align	32
.Lsigma:
.long	1634760805,857760878,2036477234,1797285236
.long	1,0,0,0
.long	2,0,0,0
.long	3,0,0,0
.long	50462976,117835012,185207048,252579084
.long	0,1,2,3
.long	1634760805,1634760805,1634760805,1634760805
.long	857760878,857760878,857760878,857760878
.long	2036477234,2036477234,2036477234,2036477234
.long	1797285236,1797285236,1797285236,1797285236
.asciz	"ChaCha20 for s390x, CRYPTOGAMS by <appro@openssl.org>"
.align	4
