.text

.type	_mul_1x1,@function
.align	16
_mul_1x1:
	lgr	%r6,%r3
	sllg	%r7,%r3,1
	sllg	%r8,%r3,2
	sllg	%r9,%r3,3

	srag	%r3,%r6,63			# broadcast 63rd bit
	nihh	%r6,0x1fff
	srag	%r12,%r7,63			# broadcast 62nd bit
	nihh	%r7,0x3fff
	srag	%r13,%r8,63			# broadcast 61st bit
	nihh	%r8,0x7fff
	ngr	%r3,%r5
	ngr	%r12,%r5
	ngr	%r13,%r5

	lghi	%r0,0
	lgr	%r10,%r6
	stg	%r0,96(%r15)	# tab[0]=0
	xgr	%r10,%r7
	stg	%r6,104(%r15)	# tab[1]=a1
	 lgr	%r11,%r8
	stg	%r7,112(%r15)	# tab[2]=a2
	 xgr	%r11,%r9
	stg	%r10,120(%r15)	# tab[3]=a1^a2
	 xgr	%r6,%r8

	stg	%r8,128(%r15)	# tab[4]=a4
	xgr	%r7,%r8
	stg	%r6,136(%r15)	# tab[5]=a1^a4
	xgr	%r10,%r8
	stg	%r7,144(%r15)	# tab[6]=a2^a4
	 xgr	%r6,%r11
	stg	%r10,152(%r15)	# tab[7]=a1^a2^a4
	 xgr	%r7,%r11

	stg	%r9,160(%r15)	# tab[8]=a8
	xgr	%r10,%r11
	stg	%r6,168(%r15)	# tab[9]=a1^a8
	 xgr	%r6,%r8
	stg	%r7,176(%r15)	# tab[10]=a2^a8
	 xgr	%r7,%r8
	stg	%r10,184(%r15)	# tab[11]=a1^a2^a8

	xgr	%r10,%r8
	stg	%r11,192(%r15)	# tab[12]=a4^a8
	 srlg	%r4,%r3,1
	stg	%r6,200(%r15)	# tab[13]=a1^a4^a8
	 sllg	%r3,%r3,63
	stg	%r7,208(%r15)	# tab[14]=a2^a4^a8
	 srlg	%r0,%r12,2
	stg	%r10,216(%r15)	# tab[15]=a1^a2^a4^a8

	lghi	%r9,120
	sllg	%r6,%r12,62
	 sllg	%r12,%r5,3
	srlg	%r1,%r13,3
	 ngr	%r12,%r9
	sllg	%r7,%r13,61
	 srlg	%r13,%r5,4-3
	xgr	%r4,%r0
	 ngr	%r13,%r9
	xgr	%r3,%r6
	xgr	%r4,%r1
	xgr	%r3,%r7

	xg	%r3,96(%r12,%r15)
	srlg	%r12,%r5,8-3
	ngr	%r12,%r9
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,12-3
	sllg	%r0,%r1,4
	ngr	%r13,%r9
	srlg	%r1,%r1,60
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,16-3
	sllg	%r1,%r0,8
	ngr	%r12,%r9
	srlg	%r0,%r0,56
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,20-3
	sllg	%r0,%r1,12
	ngr	%r13,%r9
	srlg	%r1,%r1,52
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,24-3
	sllg	%r1,%r0,16
	ngr	%r12,%r9
	srlg	%r0,%r0,48
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,28-3
	sllg	%r0,%r1,20
	ngr	%r13,%r9
	srlg	%r1,%r1,44
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,32-3
	sllg	%r1,%r0,24
	ngr	%r12,%r9
	srlg	%r0,%r0,40
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,36-3
	sllg	%r0,%r1,28
	ngr	%r13,%r9
	srlg	%r1,%r1,36
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,40-3
	sllg	%r1,%r0,32
	ngr	%r12,%r9
	srlg	%r0,%r0,32
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,44-3
	sllg	%r0,%r1,36
	ngr	%r13,%r9
	srlg	%r1,%r1,28
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,48-3
	sllg	%r1,%r0,40
	ngr	%r12,%r9
	srlg	%r0,%r0,24
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,52-3
	sllg	%r0,%r1,44
	ngr	%r13,%r9
	srlg	%r1,%r1,20
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	srlg	%r12,%r5,56-3
	sllg	%r1,%r0,48
	ngr	%r12,%r9
	srlg	%r0,%r0,16
	xgr	%r3,%r1
	xgr	%r4,%r0
	lg	%r1,96(%r13,%r15)
	srlg	%r13,%r5,60-3
	sllg	%r0,%r1,52
	ngr	%r13,%r9
	srlg	%r1,%r1,12
	xgr	%r3,%r0
	xgr	%r4,%r1
	lg	%r0,96(%r12,%r15)
	sllg	%r1,%r0,56
	srlg	%r0,%r0,8
	xgr	%r3,%r1
	xgr	%r4,%r0

	lg	%r1,96(%r13,%r15)
	sllg	%r0,%r1,60
	srlg	%r1,%r1,4
	xgr	%r3,%r0
	xgr	%r4,%r1

	br	%r14
.size	_mul_1x1,.-_mul_1x1

.globl	bn_GF2m_mul_2x2
.type	bn_GF2m_mul_2x2,@function
.align	16
bn_GF2m_mul_2x2:
	stm	%r3,%r15,3*4(%r15)

	lghi	%r1,-96-128
	la	%r0,0(%r15)
	la	%r15,0(%r1,%r15)			# alloca
	st	%r0,0(%r15)			# back chain
	sllg	%r3,%r3,32
	sllg	%r5,%r5,32
	or	%r3,%r4
	or	%r5,%r6
	bras	%r14,_mul_1x1
	rllg	%r3,%r3,32
	rllg	%r4,%r4,32
	stmg	%r3,%r4,0(%r2)
	lm	%r6,%r15,248(%r15)
	br	%r14
.size	bn_GF2m_mul_2x2,.-bn_GF2m_mul_2x2
.string	"GF(2^m) Multiplication for s390x, CRYPTOGAMS by <appro@openssl.org>"
