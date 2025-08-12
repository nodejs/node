.machine	"any"
.csect	.text[PR],7

.globl	.bn_mul_mont_fixed_n6
.align	5
.bn_mul_mont_fixed_n6:

	std	14,-8(1)
	std	20,-16(1)
	std	21,-24(1)
	std	22,-32(1)
	std	23,-40(1)
	std	24,-48(1)
	std	25,-56(1)
	std	26,-64(1)
	std	27,-72(1)

	li	0,0
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

	li	9,0
	mtctr	8
	b	Lenter_6

.align	4
Louter_6:
	ldx	11,5,9

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
	addze	27,0
.align	4
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

	addi	9,9,8
	bc	16,0,Louter_6

	and.	26,26,26
	bne	Lsub_6

	cmpld	25,12
	blt	Lcopy_6

Lsub_6:
	ld	11,0(6)
	subfc	14,11,20
	std	14,0(3)

	ld	11,8(6)
	subfe	14,11,21
	std	14,8(3)

	ld	11,16(6)
	subfe	14,11,22
	std	14,16(3)

	ld	11,24(6)
	subfe	14,11,23
	std	14,24(3)

	ld	11,32(6)
	subfe	14,11,24
	std	14,32(3)

	subfe	14,12,25
	std	14,40(3)

	addme.	26,26
	beq	Lend_6

Lcopy_6:
	std	20,0(3)
	std	21,8(3)
	std	22,16(3)
	std	23,24(3)
	std	24,32(3)
	std	25,40(3)

Lend_6:
	ld	14,-8(1)
	ld	20,-16(1)
	ld	21,-24(1)
	ld	22,-32(1)
	ld	23,-40(1)
	ld	24,-48(1)
	ld	25,-56(1)
	ld	26,-64(1)
	ld	27,-72(1)

	li	3,1
	blr	


.globl	.bn_mul_mont_300_fixed_n6
.align	5
.bn_mul_mont_300_fixed_n6:

	std	14,-8(1)
	std	20,-16(1)
	std	21,-24(1)
	std	22,-32(1)
	std	23,-40(1)
	std	24,-48(1)
	std	25,-56(1)
	std	26,-64(1)
	std	27,-72(1)

	li	0,0
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

	li	9,0
	mtctr	8
	b	Lenter_300_6

.align	4
Louter_300_6:
	ldx	11,5,9

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
	addze	27,0
.align	4
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

	addi	9,9,8
	bc	16,0,Louter_300_6

	and.	26,26,26
	bne	Lsub_300_6

	cmpld	25,12
	blt	Lcopy_300_6

Lsub_300_6:
	ld	11,0(6)
	subfc	14,11,20
	std	14,0(3)

	ld	11,8(6)
	subfe	14,11,21
	std	14,8(3)

	ld	11,16(6)
	subfe	14,11,22
	std	14,16(3)

	ld	11,24(6)
	subfe	14,11,23
	std	14,24(3)

	ld	11,32(6)
	subfe	14,11,24
	std	14,32(3)

	subfe	14,12,25
	std	14,40(3)

	addme.	26,26
	beq	Lend_300_6

Lcopy_300_6:
	std	20,0(3)
	std	21,8(3)
	std	22,16(3)
	std	23,24(3)
	std	24,32(3)
	std	25,40(3)

Lend_300_6:
	ld	14,-8(1)
	ld	20,-16(1)
	ld	21,-24(1)
	ld	22,-32(1)
	ld	23,-40(1)
	ld	24,-48(1)
	ld	25,-56(1)
	ld	26,-64(1)
	ld	27,-72(1)

	li	3,1
	blr	

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,32,98,121,32,60,97,109,105,116,97,121,64,111,122,108,97,98,115,46,111,114,103,62,44,60,97,108,97,115,116,97,105,114,64,100,45,115,105,108,118,97,46,111,114,103,62,0
.align	2
