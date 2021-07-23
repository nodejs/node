.machine	"any"
.csect	.text[PR],7
.align	5


.globl	.bn_mul_mont_fixed_n6
.bn_mul_mont_fixed_n6:
	mr	9,3

	mtvsrd	32,14
	mtvsrd	33,15
	mtvsrd	34,20
	mtvsrd	35,21
	mtvsrd	36,22
	mtvsrd	37,23
	mtvsrd	38,24
	mtvsrd	39,25
	mtvsrd	40,26
	mtvsrd	41,27

	ld	7,0(7)

	ld	11,0(5)

	ld	12,0(4)
	mulld	20,12,11
	mulhdu	10,12,11

	ld	12,8(4)
	mulld	14,12,11
	addc	21,14,10
	mulhdu	10,12,11
	addze	10,10

	ld	12,16(4)
	mulld	14,12,11
	addc	22,14,10
	mulhdu	10,12,11
	addze	10,10

	ld	12,24(4)
	mulld	14,12,11
	addc	23,14,10
	mulhdu	10,12,11
	addze	10,10

	ld	12,32(4)
	mulld	14,12,11
	addc	24,14,10
	mulhdu	10,12,11
	addze	10,10

	ld	12,40(4)
	mulld	14,12,11
	addc	25,14,10
	mulhdu	10,12,11

	addze	26,10
	li	27,0

	li	15,0
	mtctr	8
	b	Lenter_6

Louter_6:
	ldx	11,5,15

	ld	12,0(4)
	mulld	14,12,11
	addc	20,20,14
	mulhdu	10,12,11
	addze	10,10

	ld	12,8(4)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	21,21,14
	addze	10,10

	ld	12,16(4)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	22,22,14
	addze	10,10

	ld	12,24(4)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	23,23,14
	addze	10,10

	ld	12,32(4)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	24,24,14
	addze	10,10

	ld	12,40(4)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	25,25,14
	addze	10,10

	addc	26,26,10
	addze	27,27
Lenter_6:
	mulld	11,20,7

	ld	12,0(6)
	mulld	14,11,12
	addc	14,20,14
	mulhdu	10,11,12
	addze	10,10

	ld	12,8(6)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	20,21,14
	addze	10,10

	ld	12,16(6)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	21,22,14
	addze	10,10

	ld	12,24(6)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	22,23,14
	addze	10,10

	ld	12,32(6)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	23,24,14
	addze	10,10

	ld	12,40(6)
	mulld	14,12,11
	addc	14,14,10
	mulhdu	10,12,11
	addze	10,10
	addc	24,25,14
	addze	10,10

	addc	25,26,10
	addze	26,27

	addi	15,15,8
	bc	25,0,Louter_6

	and.	26,26,26
	bne	Lsub_6

	cmpld	25,12
	blt	Lcopy_6

Lsub_6:
	ld	11,0(6)
	subfc	14,11,20
	std	14,0(9)

	ld	11,8(6)
	subfe	14,11,21
	std	14,8(9)

	ld	11,16(6)
	subfe	14,11,22
	std	14,16(9)

	ld	11,24(6)
	subfe	14,11,23
	std	14,24(9)

	ld	11,32(6)
	subfe	14,11,24
	std	14,32(9)

	subfe	14,12,25
	std	14,40(9)

	addme.	26,26
	beq	Lend_6

Lcopy_6:
	std	20,0(9)
	std	21,8(9)
	std	22,16(9)
	std	23,24(9)
	std	24,32(9)
	std	25,40(9)

Lend_6:
	mfvsrd	14,32
	mfvsrd	15,33
	mfvsrd	20,34
	mfvsrd	21,35
	mfvsrd	22,36
	mfvsrd	23,37
	mfvsrd	24,38
	mfvsrd	25,39
	mfvsrd	26,40
	mfvsrd	27,41

	li	3,1
	blr	


.globl	.bn_mul_mont_300_fixed_n6
.bn_mul_mont_300_fixed_n6:
	mr	9,3

	mtvsrd	32,14
	mtvsrd	33,15
	mtvsrd	34,20
	mtvsrd	35,21
	mtvsrd	36,22
	mtvsrd	37,23
	mtvsrd	38,24
	mtvsrd	39,25
	mtvsrd	40,26
	mtvsrd	41,27

	ld	7,0(7)

	ld	11,0(5)

	ld	12,0(4)
	mulld	20,12,11
	mulhdu	10,12,11

	ld	12,8(4)
	.long	0x12AC5AB3
	.long	0x114C5AB1

	ld	12,16(4)
	.long	0x12CC5AB3
	.long	0x114C5AB1

	ld	12,24(4)
	.long	0x12EC5AB3
	.long	0x114C5AB1

	ld	12,32(4)
	.long	0x130C5AB3
	.long	0x114C5AB1

	ld	12,40(4)
	.long	0x132C5AB3
	.long	0x134C5AB1

	li	27,0

	li	15,0
	mtctr	8
	b	Lenter_300_6

Louter_300_6:
	ldx	11,5,15

	ld	12,0(4)
	.long	0x11CC5D33
	.long	0x114C5D31
	mr	20,14

	ld	12,8(4)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	21,21,14
	addze	10,10

	ld	12,16(4)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	22,22,14
	addze	10,10

	ld	12,24(4)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	23,23,14
	addze	10,10

	ld	12,32(4)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	24,24,14
	addze	10,10

	ld	12,40(4)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	25,25,14
	addze	10,10

	addc	26,26,10
	addze	27,27
Lenter_300_6:
	mulld	11,20,7

	ld	12,0(6)
	.long	0x11CB6533
	.long	0x114B6531

	ld	12,8(6)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	20,21,14
	addze	10,10

	ld	12,16(6)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	21,22,14
	addze	10,10

	ld	12,24(6)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	22,23,14
	addze	10,10

	ld	12,32(6)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	23,24,14
	addze	10,10

	ld	12,40(6)
	.long	0x11CC5AB3
	.long	0x114C5AB1
	addc	24,25,14
	addze	10,10

	addc	25,26,10
	addze	26,27

	addi	15,15,8
	bc	25,0,Louter_300_6

	and.	26,26,26
	bne	Lsub_300_6

	cmpld	25,12
	blt	Lcopy_300_6

Lsub_300_6:
	ld	11,0(6)
	subfc	14,11,20
	std	14,0(9)

	ld	11,8(6)
	subfe	14,11,21
	std	14,8(9)

	ld	11,16(6)
	subfe	14,11,22
	std	14,16(9)

	ld	11,24(6)
	subfe	14,11,23
	std	14,24(9)

	ld	11,32(6)
	subfe	14,11,24
	std	14,32(9)

	subfe	14,12,25
	std	14,40(9)

	addme.	26,26
	beq	Lend_300_6

Lcopy_300_6:
	std	20,0(9)
	std	21,8(9)
	std	22,16(9)
	std	23,24(9)
	std	24,32(9)
	std	25,40(9)

Lend_300_6:
	mfvsrd	14,32
	mfvsrd	15,33
	mfvsrd	20,34
	mfvsrd	21,35
	mfvsrd	22,36
	mfvsrd	23,37
	mfvsrd	24,38
	mfvsrd	25,39
	mfvsrd	26,40
	mfvsrd	27,41

	li	3,1
	blr	

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,32,98,121,32,60,97,109,105,116,97,121,64,111,122,108,97,98,115,46,111,114,103,62,44,60,97,108,97,115,116,97,105,114,64,100,45,115,105,108,118,97,46,111,114,103,62,0
.align	2
