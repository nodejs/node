.machine	"any"
.text

.globl	bn_mul_mont_int
.type	bn_mul_mont_int,@function
.section	".opd","aw"
.align	3
bn_mul_mont_int:
.quad	.bn_mul_mont_int,.TOC.@tocbase,0
.previous

.align	4
.bn_mul_mont_int:
	cmpwi	8,4
	mr	9,3
	li	3,0
	.long	0x4d800020
	slwi	8,8,3
	li	12,-4096
	addi	3,8,352
	subf	3,3,1
	and	3,3,12
	subf	3,1,3
	mr	12,1
	srwi	8,8,3
	stdux	1,1,3

	std	20,-96(12)
	std	21,-88(12)
	std	22,-80(12)
	std	23,-72(12)
	std	24,-64(12)
	std	25,-56(12)
	std	26,-48(12)
	std	27,-40(12)
	std	28,-32(12)
	std	29,-24(12)
	std	30,-16(12)
	std	31,-8(12)

	ld	7,0(7)
	addi	8,8,-2

	ld	23,0(5)
	ld	10,0(4)
	addi	22,1,64
	mulld	25,10,23
	mulhdu	26,10,23

	ld	10,8(4)
	ld	11,0(6)

	mulld	24,25,7

	mulld	29,10,23
	mulhdu	30,10,23

	mulld	27,11,24
	mulhdu	28,11,24
	ld	11,8(6)
	addc	27,27,25
	addze	28,28

	mulld	31,11,24
	mulhdu	0,11,24

	mtctr	8
	li	21,16
.align	4
.L1st:
	ldx	10,4,21
	addc	25,29,26
	ldx	11,6,21
	addze	26,30
	mulld	29,10,23
	addc	27,31,28
	mulhdu	30,10,23
	addze	28,0
	mulld	31,11,24
	addc	27,27,25
	mulhdu	0,11,24
	addze	28,28
	std	27,0(22)

	addi	21,21,8
	addi	22,22,8
	bdnz	.L1st

	addc	25,29,26
	addze	26,30

	addc	27,31,28
	addze	28,0
	addc	27,27,25
	addze	28,28
	std	27,0(22)

	li	3,0
	addc	28,28,26
	addze	3,3
	std	28,8(22)

	li	20,8
.align	4
.Louter:
	ldx	23,5,20
	ld	10,0(4)
	addi	22,1,64
	ld	12,64(1)
	mulld	25,10,23
	mulhdu	26,10,23
	ld	10,8(4)
	ld	11,0(6)
	addc	25,25,12
	mulld	29,10,23
	addze	26,26
	mulld	24,25,7
	mulhdu	30,10,23
	mulld	27,11,24
	mulhdu	28,11,24
	ld	11,8(6)
	addc	27,27,25
	mulld	31,11,24
	addze	28,28
	mulhdu	0,11,24

	mtctr	8
	li	21,16
.align	4
.Linner:
	ldx	10,4,21
	addc	25,29,26
	ld	12,8(22)
	addze	26,30
	ldx	11,6,21
	addc	27,31,28
	mulld	29,10,23
	addze	28,0
	mulhdu	30,10,23
	addc	25,25,12
	mulld	31,11,24
	addze	26,26
	mulhdu	0,11,24
	addc	27,27,25
	addi	21,21,8
	addze	28,28
	std	27,0(22)
	addi	22,22,8
	bdnz	.Linner

	ld	12,8(22)
	addc	25,29,26
	addze	26,30
	addc	25,25,12
	addze	26,26

	addc	27,31,28
	addze	28,0
	addc	27,27,25
	addze	28,28
	std	27,0(22)

	addic	3,3,-1
	li	3,0
	adde	28,28,26
	addze	3,3
	std	28,8(22)

	slwi	12,8,3
	cmpld	20,12
	addi	20,20,8
	ble	.Louter

	addi	8,8,2
	subfc	21,21,21
	addi	22,1,64
	mtctr	8

.align	4
.Lsub:	ldx	12,22,21
	ldx	11,6,21
	subfe	10,11,12
	stdx	10,9,21
	addi	21,21,8
	bdnz	.Lsub

	li	21,0
	mtctr	8
	subfe	3,21,3
	and	4,22,3
	andc	6,9,3
	or	4,4,6

.align	4
.Lcopy:
	ldx	12,4,21
	stdx	12,9,21
	stdx	21,22,21
	addi	21,21,8
	bdnz	.Lcopy

	ld	12,0(1)
	li	3,1
	ld	20,-96(12)
	ld	21,-88(12)
	ld	22,-80(12)
	ld	23,-72(12)
	ld	24,-64(12)
	ld	25,-56(12)
	ld	26,-48(12)
	ld	27,-40(12)
	ld	28,-32(12)
	ld	29,-24(12)
	ld	30,-16(12)
	ld	31,-8(12)
	mr	1,12
	blr
.long	0
.byte	0,12,4,0,0x80,12,6,0
.long	0
.size	bn_mul_mont_int,.-.bn_mul_mont_int
.size	.bn_mul_mont_int,.-.bn_mul_mont_int

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
