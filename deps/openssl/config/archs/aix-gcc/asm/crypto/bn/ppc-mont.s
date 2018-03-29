.machine	"any"
.csect	.text[PR],7

.globl	.bn_mul_mont_int
.align	4
.bn_mul_mont_int:
	cmpwi	8,4
	mr	9,3
	li	3,0
	bclr	12,0
	cmpwi	8,32
	bgelr	
	slwi	8,8,2
	li	12,-4096
	addi	3,8,256
	subf	3,3,1
	and	3,3,12
	subf	3,1,3
	mr	12,1
	srwi	8,8,2
	stwux	1,1,3

	stw	20,-48(12)
	stw	21,-44(12)
	stw	22,-40(12)
	stw	23,-36(12)
	stw	24,-32(12)
	stw	25,-28(12)
	stw	26,-24(12)
	stw	27,-20(12)
	stw	28,-16(12)
	stw	29,-12(12)
	stw	30,-8(12)
	stw	31,-4(12)

	lwz	7,0(7)
	addi	8,8,-2

	lwz	23,0(5)
	lwz	10,0(4)
	addi	22,1,32
	mullw	25,10,23
	mulhwu	26,10,23

	lwz	10,4(4)
	lwz	11,0(6)

	mullw	24,25,7

	mullw	29,10,23
	mulhwu	30,10,23

	mullw	27,11,24
	mulhwu	28,11,24
	lwz	11,4(6)
	addc	27,27,25
	addze	28,28

	mullw	31,11,24
	mulhwu	0,11,24

	mtctr	8
	li	21,8
.align	4
L1st:
	lwzx	10,4,21
	addc	25,29,26
	lwzx	11,6,21
	addze	26,30
	mullw	29,10,23
	addc	27,31,28
	mulhwu	30,10,23
	addze	28,0
	mullw	31,11,24
	addc	27,27,25
	mulhwu	0,11,24
	addze	28,28
	stw	27,0(22)

	addi	21,21,4
	addi	22,22,4
	bc	16,0,L1st

	addc	25,29,26
	addze	26,30

	addc	27,31,28
	addze	28,0
	addc	27,27,25
	addze	28,28
	stw	27,0(22)

	li	3,0
	addc	28,28,26
	addze	3,3
	stw	28,4(22)

	li	20,4
.align	4
Louter:
	lwzx	23,5,20
	lwz	10,0(4)
	addi	22,1,32
	lwz	12,32(1)
	mullw	25,10,23
	mulhwu	26,10,23
	lwz	10,4(4)
	lwz	11,0(6)
	addc	25,25,12
	mullw	29,10,23
	addze	26,26
	mullw	24,25,7
	mulhwu	30,10,23
	mullw	27,11,24
	mulhwu	28,11,24
	lwz	11,4(6)
	addc	27,27,25
	mullw	31,11,24
	addze	28,28
	mulhwu	0,11,24

	mtctr	8
	li	21,8
.align	4
Linner:
	lwzx	10,4,21
	addc	25,29,26
	lwz	12,4(22)
	addze	26,30
	lwzx	11,6,21
	addc	27,31,28
	mullw	29,10,23
	addze	28,0
	mulhwu	30,10,23
	addc	25,25,12
	mullw	31,11,24
	addze	26,26
	mulhwu	0,11,24
	addc	27,27,25
	addi	21,21,4
	addze	28,28
	stw	27,0(22)
	addi	22,22,4
	bc	16,0,Linner

	lwz	12,4(22)
	addc	25,29,26
	addze	26,30
	addc	25,25,12
	addze	26,26

	addc	27,31,28
	addze	28,0
	addc	27,27,25
	addze	28,28
	stw	27,0(22)

	addic	3,3,-1
	li	3,0
	adde	28,28,26
	addze	3,3
	stw	28,4(22)

	slwi	12,8,2
	cmplw	0,20,12
	addi	20,20,4
	ble	Louter

	addi	8,8,2
	subfc	21,21,21
	addi	22,1,32
	mtctr	8

.align	4
Lsub:	lwzx	12,22,21
	lwzx	11,6,21
	subfe	10,11,12
	stwx	10,9,21
	addi	21,21,4
	bc	16,0,Lsub

	li	21,0
	mtctr	8
	subfe	3,21,3
	and	4,22,3
	andc	6,9,3
	or	4,4,6

.align	4
Lcopy:
	lwzx	12,4,21
	stwx	12,9,21
	stwx	21,22,21
	addi	21,21,4
	bc	16,0,Lcopy

	lwz	12,0(1)
	li	3,1
	lwz	20,-48(12)
	lwz	21,-44(12)
	lwz	22,-40(12)
	lwz	23,-36(12)
	lwz	24,-32(12)
	lwz	25,-28(12)
	lwz	26,-24(12)
	lwz	27,-20(12)
	lwz	28,-16(12)
	lwz	29,-12(12)
	lwz	30,-8(12)
	lwz	31,-4(12)
	mr	1,12
	blr	
.long	0
.byte	0,12,4,0,0x80,12,6,0
.long	0


.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
