.text

.globl	poly1305_init
.type	poly1305_init,@function
.align	16
poly1305_init:
	lghi	%r0,0
	lghi	%r1,-1
	stg	%r0,0(%r2)		# zero hash value
	stg	%r0,8(%r2)
	stg	%r0,16(%r2)

	clr	%r3,%r0
	je	.Lno_key

	lrvg	%r4,0(%r3)		# load little-endian key
	lrvg	%r5,8(%r3)

	nihl	%r1,0xffc0		# 0xffffffc0ffffffff
	srlg	%r0,%r1,4		# 0x0ffffffc0fffffff
	srlg	%r1,%r1,4
	nill	%r1,0xfffc		# 0x0ffffffc0ffffffc

	ngr	%r4,%r0
	ngr	%r5,%r1

	stg	%r4,32(%r2)
	stg	%r5,40(%r2)

.Lno_key:
	lghi	%r2,0
	br	%r14
.size	poly1305_init,.-poly1305_init
.globl	poly1305_blocks
.type	poly1305_blocks,@function
.align	16
poly1305_blocks:
	srl	%r4,4			# fixed-up in 64-bit build
	lghi	%r0,0
	clr	%r4,%r0
	je	.Lno_data

	stm	%r6,%r14,24(%r15)

	llgfr   %r5,%r5		# clear upper half, much needed with
					# non-64-bit ABI
	lg	%r0,32(%r2)		# load key
	lg	%r1,40(%r2)

	lg	%r11,0(%r2)		# load hash value
	lg	%r13,8(%r2)
	lg	%r14,16(%r2)

	st	%r2,8(%r15)	# off-load %r2
	srlg	%r2,%r1,2
	algr	%r2,%r1			# s1 = r1 + r1>>2
	j	.Loop

.align	16
.Loop:
	lrvg	%r7,0(%r3)		# load little-endian input
	lrvg	%r9,8(%r3)
	la	%r3,16(%r3)

	algr	%r7,%r11		# accumulate input
	alcgr	%r9,%r13

	lgr	%r11,%r7
	mlgr	%r6,%r0		# h0*r0	  -> %r6:%r7
	lgr	%r13,%r9
	mlgr	%r8,%r2		# h1*5*r1 -> %r8:%r9

	mlgr	%r10,%r1			# h0*r1   -> %r10:%r11
	mlgr	%r12,%r0			# h1*r0   -> %r12:%r13
	alcgr	%r14,%r5

	algr	%r7,%r9
	lgr	%r9,%r14
	alcgr	%r6,%r8
	lghi	%r8,0

	algr	%r13,%r11
	alcgr	%r12,%r10

	msgr	%r9,%r2		# h2*s1
	msgr	%r14,%r0			# h2*r0

	algr	%r13,%r9
	alcgr	%r12,%r8		# %r8 is zero

	algr	%r13,%r6
	alcgr	%r14,%r12

	lghi	%r11,-4			# final reduction step
	ngr	%r11,%r14
	srlg	%r10,%r14,2
	algr	%r11,%r10
	lghi	%r12,3
	ngr	%r14,%r12

	algr	%r11,%r7
	alcgr	%r13,%r8		# %r8 is still zero
	alcgr	%r14,%r8		# %r8 is still zero

	brct	%r4,.Loop

	l	%r2,8(%r15)	# restore %r2

	stg	%r11,0(%r2)		# store hash value
	stg	%r13,8(%r2)
	stg	%r14,16(%r2)

	lm	%r6,%r14,24(%r15)
.Lno_data:
	br	%r14
.size	poly1305_blocks,.-poly1305_blocks
.globl	poly1305_emit
.type	poly1305_emit,@function
.align	16
poly1305_emit:
	stm	%r6,%r9,24(%r15)

	lg	%r5,0(%r2)
	lg	%r6,8(%r2)
	lg	%r7,16(%r2)

	lghi	%r0,5
	lghi	%r1,0
	lgr	%r8,%r5
	lgr	%r9,%r6

	algr	%r5,%r0			# compare to modulus
	alcgr	%r6,%r1
	alcgr	%r7,%r1

	srlg	%r7,%r7,2		# did it borrow/carry?
	slgr	%r1,%r7			# 0-%r7>>2
	lg	%r7,0(%r4)		# load nonce
	lghi	%r0,-1
	lg	%r2,8(%r4)
	xgr	%r0,%r1			# ~%r1

	ngr	%r5,%r1
	ngr	%r8,%r0
	ngr	%r6,%r1
	ngr	%r9,%r0
	ogr	%r5,%r8
	rllg	%r8,%r7,32		# flip nonce words
	ogr	%r6,%r9
	rllg	%r9,%r2,32

	algr	%r5,%r8			# accumulate nonce
	alcgr	%r6,%r9

	strvg	%r5,0(%r3)		# write little-endian result
	strvg	%r6,8(%r3)

	lm	%r6,%r9,24(%r15)
	br	%r14
.size	poly1305_emit,.-poly1305_emit

.string	"Poly1305 for s390x, CRYPTOGAMS by <appro@openssl.org>"
