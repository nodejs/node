.text

.globl	ChaCha20_ctr32
.type	ChaCha20_ctr32,@function
.align	32
ChaCha20_ctr32:
	ltgr	%r4,%r4			# %r4==0?
	bzr	%r14
	aghi	%r4,-64
	lghi	%r1,-240
	stmg	%r6,%r15,48(%r15)
	slgr	%r2,%r3			# difference
	la	%r4,0(%r3,%r4)		# end of input minus 64
	larl	%r7,.Lsigma
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)

	lmg	%r8,%r11,0(%r5)		# load key
	lmg	%r12,%r13,0(%r6)		# load counter
	lmg	%r6,%r7,0(%r7)			# load sigma constant

	la	%r14,0(%r3)
	stg	%r2,240+3*8(%r15)
	stg	%r4,240+4*8(%r15)
	stmg	%r6,%r13,160(%r15)		# copy key schedule to stack
	srlg	%r10,%r12,32			# 32-bit counter value
	j	.Loop_outer

.align	16
.Loop_outer:
	lm	%r0,%r7,160+4*0(%r15)		# load x[0]-x[7]
	lm	%r8,%r9,160+4*10(%r15)		# load x[10]-x[11]
	lm	%r11,%r13,160+4*13(%r15)	# load x[13]-x[15]
	stm	%r8,%r9,160+4*8+4*10(%r15)	# offload x[10]-x[11]
	lm	%r8,%r9,160+4*8(%r15)		# load x[8]-x[9]
	st	%r10,160+4*12(%r15)		# save counter
	stg	%r14,240+2*8(%r15)		# save input pointer
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

	lg	%r14,240+2*8(%r15)		# pull input pointer
	stm	%r8,%r9,160+4*8+4*8(%r15)	# offload x[8]-x[9]
	lmg	%r8,%r9,240+3*8(%r15)

	al	%r0,160+4*0(%r15)	# accumulate key schedule
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

	la	%r8,0(%r8,%r14)		# reconstruct output pointer
	clgr	%r14,%r9
	jh	.Ltail

	x	%r0,4*0(%r14)			# xor with input
	x	%r1,4*1(%r14)
	st	%r0,4*0(%r8)		# store output
	x	%r2,4*2(%r14)
	st	%r1,4*1(%r8)
	x	%r3,4*3(%r14)
	st	%r2,4*2(%r8)
	x	%r4,4*4(%r14)
	st	%r3,4*3(%r8)
	 lm	%r0,%r3,160+4*8+4*8(%r15)	# load x[8]-x[11]
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
	al	%r10,160+4*12(%r15)	# increment counter
	 x	%r1,4*9(%r14)
	 st	%r0,4*8(%r8)
	 x	%r2,4*10(%r14)
	 st	%r1,4*9(%r8)
	 x	%r3,4*11(%r14)
	 st	%r2,4*10(%r8)
	 st	%r3,4*11(%r8)

	clgr	%r14,%r9			# done yet?
	la	%r14,64(%r14)
	jl	.Loop_outer

.Ldone:
	xgr	%r0,%r0
	xgr	%r1,%r1
	xgr	%r2,%r2
	xgr	%r3,%r3
	stmg	%r0,%r3,160+4*4(%r15)	# wipe key copy
	stmg	%r0,%r3,160+4*12(%r15)

	lmg	%r6,%r15,288(%r15)
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
.Lsigma:
.long	0x61707865,0x3320646e,0x79622d32,0x6b206574	# endian-neutral
.asciz	"ChaCha20 for s390x, CRYPTOGAMS by <appro@openssl.org>"
.align	4
