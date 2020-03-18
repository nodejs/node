.text

.globl	RC4
.type	RC4,@function
.align	64
RC4:
	stm	%r6,%r11,6*4(%r15)
	llgfr	%r3,%r3
	llgc	%r6,0(%r2)
	llgc	%r10,1(%r2)
	la	%r6,1(%r6)
	nill	%r6,0xff
	srlg	%r1,%r3,3
	ltgr	%r1,%r1
	llgc	%r8,2(%r6,%r2)
	jz	.Lshort
	j	.Loop8

.align	64
.Loop8:
	la	%r10,0(%r10,%r8)	# 0
	nill	%r10,255
	la	%r7,1(%r6)
	nill	%r7,255
	llgc	%r11,2(%r10,%r2)
	stc	%r8,2(%r10,%r2)
	llgc	%r9,2(%r7,%r2)
	stc	%r11,2(%r6,%r2)
	cr	%r7,%r10
	jne	.Lcmov0
	la	%r9,0(%r8)
.Lcmov0:
	la	%r11,0(%r11,%r8)
	nill	%r11,255
	la	%r10,0(%r10,%r9)	# 1
	nill	%r10,255
	la	%r6,1(%r7)
	nill	%r6,255
	llgc	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r9,2(%r10,%r2)
	llgc	%r8,2(%r6,%r2)
	stc	%r11,2(%r7,%r2)
	cr	%r6,%r10
	jne	.Lcmov1
	la	%r8,0(%r9)
.Lcmov1:
	la	%r11,0(%r11,%r9)
	nill	%r11,255
	la	%r10,0(%r10,%r8)	# 2
	nill	%r10,255
	la	%r7,1(%r6)
	nill	%r7,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r8,2(%r10,%r2)
	llgc	%r9,2(%r7,%r2)
	stc	%r11,2(%r6,%r2)
	cr	%r7,%r10
	jne	.Lcmov2
	la	%r9,0(%r8)
.Lcmov2:
	la	%r11,0(%r11,%r8)
	nill	%r11,255
	la	%r10,0(%r10,%r9)	# 3
	nill	%r10,255
	la	%r6,1(%r7)
	nill	%r6,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r9,2(%r10,%r2)
	llgc	%r8,2(%r6,%r2)
	stc	%r11,2(%r7,%r2)
	cr	%r6,%r10
	jne	.Lcmov3
	la	%r8,0(%r9)
.Lcmov3:
	la	%r11,0(%r11,%r9)
	nill	%r11,255
	la	%r10,0(%r10,%r8)	# 4
	nill	%r10,255
	la	%r7,1(%r6)
	nill	%r7,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r8,2(%r10,%r2)
	llgc	%r9,2(%r7,%r2)
	stc	%r11,2(%r6,%r2)
	cr	%r7,%r10
	jne	.Lcmov4
	la	%r9,0(%r8)
.Lcmov4:
	la	%r11,0(%r11,%r8)
	nill	%r11,255
	la	%r10,0(%r10,%r9)	# 5
	nill	%r10,255
	la	%r6,1(%r7)
	nill	%r6,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r9,2(%r10,%r2)
	llgc	%r8,2(%r6,%r2)
	stc	%r11,2(%r7,%r2)
	cr	%r6,%r10
	jne	.Lcmov5
	la	%r8,0(%r9)
.Lcmov5:
	la	%r11,0(%r11,%r9)
	nill	%r11,255
	la	%r10,0(%r10,%r8)	# 6
	nill	%r10,255
	la	%r7,1(%r6)
	nill	%r7,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r8,2(%r10,%r2)
	llgc	%r9,2(%r7,%r2)
	stc	%r11,2(%r6,%r2)
	cr	%r7,%r10
	jne	.Lcmov6
	la	%r9,0(%r8)
.Lcmov6:
	la	%r11,0(%r11,%r8)
	nill	%r11,255
	la	%r10,0(%r10,%r9)	# 7
	nill	%r10,255
	la	%r6,1(%r7)
	nill	%r6,255
	sllg	%r0,%r0,8
	ic	%r0,2(%r11,%r2)
	llgc	%r11,2(%r10,%r2)
	stc	%r9,2(%r10,%r2)
	llgc	%r8,2(%r6,%r2)
	stc	%r11,2(%r7,%r2)
	cr	%r6,%r10
	jne	.Lcmov7
	la	%r8,0(%r9)
.Lcmov7:
	la	%r11,0(%r11,%r9)
	nill	%r11,255
	lg	%r9,0(%r4)
	sllg	%r0,%r0,8
	la	%r4,8(%r4)
	ic	%r0,2(%r11,%r2)
	xgr	%r0,%r9
	stg	%r0,0(%r5)
	la	%r5,8(%r5)
	brctg	%r1,.Loop8

.Lshort:
	lghi	%r0,7
	ngr	%r3,%r0
	jz	.Lexit
	j	.Loop1

.align	16
.Loop1:
	la	%r10,0(%r10,%r8)
	nill	%r10,255
	llgc	%r11,2(%r10,%r2)
	stc	%r8,2(%r10,%r2)
	stc	%r11,2(%r6,%r2)
	ar	%r11,%r8
	ahi	%r6,1
	nill	%r11,255
	nill	%r6,255
	llgc	%r0,0(%r4)
	la	%r4,1(%r4)
	llgc	%r11,2(%r11,%r2)
	llgc	%r8,2(%r6,%r2)
	xr	%r0,%r11
	stc	%r0,0(%r5)
	la	%r5,1(%r5)
	brct	%r3,.Loop1

.Lexit:
	ahi	%r6,-1
	stc	%r6,0(%r2)
	stc	%r10,1(%r2)
	lm	%r6,%r11,6*4(%r15)
	br	%r14
.size	RC4,.-RC4
.string	"RC4 for s390x, CRYPTOGAMS by <appro@openssl.org>"

.globl	RC4_set_key
.type	RC4_set_key,@function
.align	64
RC4_set_key:
	stm	%r6,%r8,6*4(%r15)
	lhi	%r0,256
	la	%r1,0
	sth	%r1,0(%r2)
.align	4
.L1stloop:
	stc	%r1,2(%r1,%r2)
	la	%r1,1(%r1)
	brct	%r0,.L1stloop

	lghi	%r7,-256
	lr	%r0,%r3
	la	%r8,0
	la	%r1,0
.align	16
.L2ndloop:
	llgc	%r5,2+256(%r7,%r2)
	llgc	%r6,0(%r8,%r4)
	la	%r1,0(%r1,%r5)
	la	%r7,1(%r7)
	la	%r1,0(%r1,%r6)
	nill	%r1,255
	la	%r8,1(%r8)
	tml	%r7,255
	llgc	%r6,2(%r1,%r2)
	stc	%r6,2+256-1(%r7,%r2)
	stc	%r5,2(%r1,%r2)
	jz	.Ldone
	brct	%r0,.L2ndloop
	lr	%r0,%r3
	la	%r8,0
	j	.L2ndloop
.Ldone:
	lm	%r6,%r8,6*4(%r15)
	br	%r14
.size	RC4_set_key,.-RC4_set_key

.globl	RC4_options
.type	RC4_options,@function
.align	16
RC4_options:
	larl	%r2,.Loptions
	br	%r14
.size	RC4_options,.-RC4_options
.section	.rodata
.Loptions:
.align	8
.string	"rc4(8x,char)"
