.text

.type	__KeccakF1600,@function
.align	32
__KeccakF1600:
	stg	%r14,8*14(%r15)
	lg	%r0,160(%r2)
	lg	%r1,168(%r2)
	lg	%r5,176(%r2)
	lg	%r6,184(%r2)
	lg	%r7,192(%r2)
	larl	%r4,iotas
	j	.Loop

.align	16
.Loop:
	lg	%r8,0(%r2)
	lg	%r9,48(%r2)
	lg	%r10,96(%r2)
	lg	%r11,144(%r2)

	xgr	%r0,%r8
	xg	%r1,8(%r2)
	xg	%r5,16(%r2)
	xg	%r6,24(%r2)
	lgr	%r12,%r7
	xg	%r7,32(%r2)

	xg	%r0,40(%r2)
	xgr	%r1,%r9
	xg	%r5,56(%r2)
	xg	%r6,64(%r2)
	xg	%r7,72(%r2)

	xg	%r0,80(%r2)
	xg	%r1,88(%r2)
	xgr	%r5,%r10
	xg	%r6,104(%r2)
	xg	%r7,112(%r2)

	xg	%r0,120(%r2)
	xg	%r1,128(%r2)
	xg	%r5,136(%r2)
	xgr	%r6,%r11
	xg	%r7,152(%r2)

	lgr	%r13,%r5
	rllg	%r5,%r5,1
	xgr	%r5,%r0		# D[1] = ROL64(C[2], 1) ^ C[0]

	rllg	%r0,%r0,1
	xgr	%r0,%r6		# D[4] = ROL64(C[0], 1) ^ C[3]

	rllg	%r6,%r6,1
	xgr	%r6,%r1		# D[2] = ROL64(C[3], 1) ^ C[1]

	rllg	%r1,%r1,1
	xgr	%r1,%r7		# D[0] = ROL64(C[1], 1) ^ C[4]

	rllg	%r7,%r7,1
	xgr	%r7,%r13		# D[3] = ROL64(C[4], 1) ^ C[2]
	xgr	%r9,%r5
	xgr	%r10,%r6
	xgr	%r11,%r7
	 rllg	%r9,%r9,44
	xgr	%r12,%r0
	 rllg	%r10,%r10,43
	xgr	%r8,%r1

	lgr	%r13,%r9
	ogr	%r9,%r10
	 rllg	%r11,%r11,21
	xgr	%r9,%r8		#	    C[0] ^ ( C[1] | C[2])
	 rllg	%r12,%r12,14
	xg	%r9,0(%r4)
	la	%r4,8(%r4)
	stg	%r9,0(%r3)	# R[0][0] = C[0] ^ ( C[1] | C[2]) ^ iotas[i]

	lgr	%r14,%r12
	ngr	%r12,%r11
	 lghi	%r9,-1		# no 'not' instruction :-(
	xgr	%r12,%r10		#	    C[2] ^ ( C[4] & C[3])
	 xgr	%r10,%r9		# not	%r10
	stg	%r12,16(%r3)	# R[0][2] = C[2] ^ ( C[4] & C[3])
	 ogr	%r10,%r11
	 xgr	%r10,%r13		#	    C[1] ^ (~C[2] | C[3])

	ngr	%r13,%r8
	 stg	%r10,8(%r3)	# R[0][1] = C[1] ^ (~C[2] | C[3])
	xgr	%r13,%r14		#	    C[4] ^ ( C[1] & C[0])
	 ogr	%r14,%r8
	stg	%r13,32(%r3)	# R[0][4] = C[4] ^ ( C[1] & C[0])
	 xgr	%r14,%r11		#	    C[3] ^ ( C[4] | C[0])
	 stg	%r14,24(%r3)	# R[0][3] = C[3] ^ ( C[4] | C[0])


	lg	%r8,24(%r2)
	lg	%r12,176(%r2)
	lg	%r11,128(%r2)
	lg	%r9,72(%r2)
	lg	%r10,80(%r2)

	xgr	%r8,%r7
	xgr	%r12,%r6
	 rllg	%r8,%r8,28
	xgr	%r11,%r5
	 rllg	%r12,%r12,61
	xgr	%r9,%r0
	 rllg	%r11,%r11,45
	xgr	%r10,%r1

	lgr	%r13,%r8
	ogr	%r8,%r12
	 rllg	%r9,%r9,20
	xgr	%r8,%r11		#	    C[3] ^ (C[0] |  C[4])
	 rllg	%r10,%r10,3
	stg	%r8,64(%r3)	# R[1][3] = C[3] ^ (C[0] |  C[4])

	lgr	%r14,%r9
	ngr	%r9,%r13
	 lghi	%r8,-1		# no 'not' instruction :-(
	xgr	%r9,%r12		#	    C[4] ^ (C[1] &  C[0])
	 xgr	%r12,%r8		# not	%r12
	stg	%r9,72(%r3)	# R[1][4] = C[4] ^ (C[1] &  C[0])

	 ogr	%r12,%r11
	 xgr	%r12,%r10		#	    C[2] ^ (~C[4] | C[3])

	ngr	%r11,%r10
	 stg	%r12,56(%r3)	# R[1][2] = C[2] ^ (~C[4] | C[3])
	xgr	%r11,%r14		#	    C[1] ^ (C[3] &  C[2])
	 ogr	%r14,%r10
	stg	%r11,48(%r3)	# R[1][1] = C[1] ^ (C[3] &  C[2])
	 xgr	%r14,%r13		#	    C[0] ^ (C[1] |  C[2])
	 stg	%r14,40(%r3)	# R[1][0] = C[0] ^ (C[1] |  C[2])


	lg	%r10,104(%r2)
	lg	%r11,152(%r2)
	lg	%r9,56(%r2)
	lg	%r12,160(%r2)
	lg	%r8,8(%r2)

	xgr	%r10,%r7
	xgr	%r11,%r0
	 rllg	%r10,%r10,25
	xgr	%r9,%r6
	 rllg	%r11,%r11,8
	xgr	%r12,%r1
	 rllg	%r9,%r9,6
	xgr	%r8,%r5

	lgr	%r13,%r10
	ngr	%r10,%r11
	 rllg	%r12,%r12,18
	xgr	%r10,%r9		#	     C[1] ^ ( C[2] & C[3])
	lghi	%r14,-1		# no 'not' instruction :-(
	stg	%r10,88(%r3)	# R[2][1] =  C[1] ^ ( C[2] & C[3])

	xgr	%r11,%r14		# not	%r11
	lgr	%r14,%r12
	ngr	%r12,%r11
	 rllg	%r8,%r8,1
	xgr	%r12,%r13		#	     C[2] ^ ( C[4] & ~C[3])
	 ogr	%r13,%r9
	stg	%r12,96(%r3)	# R[2][2] =  C[2] ^ ( C[4] & ~C[3])
	 xgr	%r13,%r8		#	     C[0] ^ ( C[2] | C[1])

	ngr	%r9,%r8
	 stg	%r13,80(%r3)	# R[2][0] =  C[0] ^ ( C[2] | C[1])
	xgr	%r9,%r14		#	     C[4] ^ ( C[1] & C[0])
	 ogr	%r8,%r14
	stg	%r9,112(%r3)	# R[2][4] =  C[4] ^ ( C[1] & C[0])
	 xgr	%r8,%r11		#	    ~C[3] ^ ( C[0] | C[4])
	 stg	%r8,104(%r3)	# R[2][3] = ~C[3] ^ ( C[0] | C[4])


	lg	%r10,88(%r2)
	lg	%r11,136(%r2)
	lg	%r9,40(%r2)
	lg	%r12,184(%r2)
	lg	%r8,32(%r2)

	xgr	%r10,%r5
	xgr	%r11,%r6
	 rllg	%r10,%r10,10
	xgr	%r9,%r1
	 rllg	%r11,%r11,15
	xgr	%r12,%r7
	 rllg	%r9,%r9,36
	xgr	%r8,%r0
	 rllg	%r12,%r12,56

	lgr	%r13,%r10
	ogr	%r10,%r11
	lghi	%r14,-1		# no 'not' instruction :-(
	xgr	%r10,%r9		#	     C[1] ^ ( C[2] | C[3])
	xgr	%r11,%r14		# not	%r11
	stg	%r10,128(%r3)	# R[3][1] =  C[1] ^ ( C[2] | C[3])

	lgr	%r14,%r12
	ogr	%r12,%r11
	 rllg	%r8,%r8,27
	xgr	%r12,%r13		#	     C[2] ^ ( C[4] | ~C[3])
	 ngr	%r13,%r9
	stg	%r12,136(%r3)	# R[3][2] =  C[2] ^ ( C[4] | ~C[3])
	 xgr	%r13,%r8		#	     C[0] ^ ( C[2] & C[1])

	ogr	%r9,%r8
	 stg	%r13,120(%r3)	# R[3][0] =  C[0] ^ ( C[2] & C[1])
	xgr	%r9,%r14		#	     C[4] ^ ( C[1] | C[0])
	 ngr	%r8,%r14
	stg	%r9,152(%r3)	# R[3][4] =  C[4] ^ ( C[1] | C[0])
	 xgr	%r8,%r11		#	    ~C[3] ^ ( C[0] & C[4])
	 stg	%r8,144(%r3)	# R[3][3] = ~C[3] ^ ( C[0] & C[4])


	xg	%r6,16(%r2)
	xg	%r7,64(%r2)
	xg	%r5,168(%r2)
	xg	%r0,112(%r2)
	xgr	%r3,%r2		# xchg	%r3,%r2
	 rllg	%r6,%r6,62
	xg	%r1,120(%r2)
	 rllg	%r7,%r7,55
	xgr	%r2,%r3
	 rllg	%r5,%r5,2
	xgr	%r3,%r2
	 rllg	%r0,%r0,39
	lgr	%r13,%r6
	ngr	%r6,%r7
	lghi	%r14,-1		# no 'not' instruction :-(
	xgr	%r6,%r5		#	     C[4] ^ ( C[0] & C[1])
	xgr	%r7,%r14		# not	%r7
	stg	%r6,192(%r2)	# R[4][4] =  C[4] ^ ( C[0] & C[1])

	lgr	%r14,%r0
	ngr	%r0,%r7
	 rllg	%r1,%r1,41
	xgr	%r0,%r13		#	     C[0] ^ ( C[2] & ~C[1])
	 ogr	%r13,%r5
	stg	%r0,160(%r2)	# R[4][0] =  C[0] ^ ( C[2] & ~C[1])
	 xgr	%r13,%r1		#	     C[3] ^ ( C[0] | C[4])

	ngr	%r5,%r1
	 stg	%r13,184(%r2)	# R[4][3] =  C[3] ^ ( C[0] | C[4])
	xgr	%r5,%r14		#	     C[2] ^ ( C[4] & C[3])
	 ogr	%r1,%r14
	stg	%r5,176(%r2)	# R[4][2] =  C[2] ^ ( C[4] & C[3])
	 xgr	%r1,%r7		#	    ~C[1] ^ ( C[2] | C[3])

	lgr	%r7,%r6		# harmonize with the loop top
	lgr	%r6,%r13
	 stg	%r1,168(%r2)	# R[4][1] = ~C[1] ^ ( C[2] | C[3])

	tmll	%r4,255
	jnz	.Loop

	lg	%r14,8*14(%r15)
	br	%r14
.size	__KeccakF1600,.-__KeccakF1600
.type	KeccakF1600,@function
.align	32
KeccakF1600:
.LKeccakF1600:
	lghi	%r1,-360
	stmg	%r6,%r15,8*6(%r15)
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)

	lghi	%r8,-1		# no 'not' instruction :-(
	lghi	%r9,-1
	lghi	%r10,-1
	lghi	%r11,-1
	lghi	%r12,-1
	lghi	%r13,-1
	xg	%r8,8(%r2)
	xg	%r9,16(%r2)
	xg	%r10,64(%r2)
	xg	%r11,96(%r2)
	xg	%r12,136(%r2)
	xg	%r13,160(%r2)
	stmg	%r8,%r9,8(%r2)
	stg	%r10,64(%r2)
	stg	%r11,96(%r2)
	stg	%r12,136(%r2)
	stg	%r13,160(%r2)

	la	%r3,160(%r15)

	bras	%r14,__KeccakF1600

	lghi	%r8,-1		# no 'not' instruction :-(
	lghi	%r9,-1
	lghi	%r10,-1
	lghi	%r11,-1
	lghi	%r12,-1
	lghi	%r13,-1
	xg	%r8,8(%r2)
	xg	%r9,16(%r2)
	xg	%r10,64(%r2)
	xg	%r11,96(%r2)
	xg	%r12,136(%r2)
	xg	%r13,160(%r2)
	stmg	%r8,%r9,8(%r2)
	stg	%r10,64(%r2)
	stg	%r11,96(%r2)
	stg	%r12,136(%r2)
	stg	%r13,160(%r2)

	lmg	%r6,%r15,360+6*8(%r15)
	br	%r14
.size	KeccakF1600,.-KeccakF1600
.globl	SHA3_absorb
.type	SHA3_absorb,@function
.align	32
SHA3_absorb:
	lghi	%r1,-360
	stmg	%r5,%r15,8*5(%r15)
	lgr	%r0,%r15
	la	%r15,0(%r1,%r15)
	stg	%r0,0(%r15)

	lghi	%r8,-1		# no 'not' instruction :-(
	lghi	%r9,-1
	lghi	%r10,-1
	lghi	%r11,-1
	lghi	%r12,-1
	lghi	%r13,-1
	xg	%r8,8(%r2)
	xg	%r9,16(%r2)
	xg	%r10,64(%r2)
	xg	%r11,96(%r2)
	xg	%r12,136(%r2)
	xg	%r13,160(%r2)
	stmg	%r8,%r9,8(%r2)
	stg	%r10,64(%r2)
	stg	%r11,96(%r2)
	stg	%r12,136(%r2)
	stg	%r13,160(%r2)

.Loop_absorb:
	clgr	%r4,%r5
	jl	.Ldone_absorb

	srlg	%r5,%r5,3
	la	%r1,0(%r2)

.Lblock_absorb:
	lrvg	%r0,0(%r3)
	la	%r3,8(%r3)
	xg	%r0,0(%r1)
	aghi	%r4,-8
	stg	%r0,0(%r1)
	la	%r1,8(%r1)
	brct	%r5,.Lblock_absorb

	stmg	%r3,%r4,360+3*8(%r15)
	la	%r3,160(%r15)
	bras	%r14,__KeccakF1600
	lmg	%r3,%r5,360+3*8(%r15)
	j	.Loop_absorb

.align	16
.Ldone_absorb:
	lghi	%r8,-1		# no 'not' instruction :-(
	lghi	%r9,-1
	lghi	%r10,-1
	lghi	%r11,-1
	lghi	%r12,-1
	lghi	%r13,-1
	xg	%r8,8(%r2)
	xg	%r9,16(%r2)
	xg	%r10,64(%r2)
	xg	%r11,96(%r2)
	xg	%r12,136(%r2)
	xg	%r13,160(%r2)
	stmg	%r8,%r9,8(%r2)
	stg	%r10,64(%r2)
	stg	%r11,96(%r2)
	stg	%r12,136(%r2)
	stg	%r13,160(%r2)

	lgr	%r2,%r4		# return value

	lmg	%r6,%r15,360+6*8(%r15)
	br	%r14
.size	SHA3_absorb,.-SHA3_absorb
.globl	SHA3_squeeze
.type	SHA3_squeeze,@function
.align	32
SHA3_squeeze:
	srlg	%r5,%r5,3
	stg	%r14,2*8(%r15)
	lghi	%r14,8
	stg	%r5,5*8(%r15)
	la	%r1,0(%r2)
	cijne	%r6,0,.Lnext_block

	j	.Loop_squeeze

.align	16
.Loop_squeeze:
	clgr %r4,%r14
	jl	.Ltail_squeeze

	lrvg	%r0,0(%r1)
	la	%r1,8(%r1)
	stg	%r0,0(%r3)
	la	%r3,8(%r3)
	aghi	%r4,-8			# len -= 8
	jz	.Ldone_squeeze

	brct	%r5,.Loop_squeeze	# bsz--

.Lnext_block:
	stmg	%r3,%r4,3*8(%r15)
	bras	%r14,.LKeccakF1600
	lmg	%r3,%r5,3*8(%r15)
	lghi	%r14,8
	la	%r1,0(%r2)
	j	.Loop_squeeze

.Ltail_squeeze:
	lg	%r0,0(%r1)
.Loop_tail_squeeze:
	stc	%r0,0(%r3)
	la	%r3,1(%r3)
	srlg	%r0,%r0,8
	brct	%r4,.Loop_tail_squeeze

.Ldone_squeeze:
	lg	%r14,2*8(%r15)
	br	%r14
.size	SHA3_squeeze,.-SHA3_squeeze
.align	256
	.quad	0,0,0,0,0,0,0,0
.type	iotas,@object
iotas:
	.quad	0x0000000000000001
	.quad	0x0000000000008082
	.quad	0x800000000000808a
	.quad	0x8000000080008000
	.quad	0x000000000000808b
	.quad	0x0000000080000001
	.quad	0x8000000080008081
	.quad	0x8000000000008009
	.quad	0x000000000000008a
	.quad	0x0000000000000088
	.quad	0x0000000080008009
	.quad	0x000000008000000a
	.quad	0x000000008000808b
	.quad	0x800000000000008b
	.quad	0x8000000000008089
	.quad	0x8000000000008003
	.quad	0x8000000000008002
	.quad	0x8000000000000080
	.quad	0x000000000000800a
	.quad	0x800000008000000a
	.quad	0x8000000080008081
	.quad	0x8000000000008080
	.quad	0x0000000080000001
	.quad	0x8000000080008008
.size	iotas,.-iotas
.asciz	"Keccak-1600 absorb and squeeze for s390x, CRYPTOGAMS by <appro@openssl.org>"
