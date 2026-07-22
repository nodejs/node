.machine	"any"
.csect	.text[PR],7

.globl	.bn_mul_mont_int
.align	5
.bn_mul_mont_int:
	mr	9,3
	li	3,0
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
L1st:
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
	bc	16,0,L1st

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
Louter:
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
Linner:
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
	bc	16,0,Linner

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
	ble	Louter

	addi	8,8,2
	subfc	21,21,21
	addi	22,1,64
	mtctr	8

.align	4
Lsub:	ldx	12,22,21
	ldx	11,6,21
	subfe	10,11,12
	stdx	10,9,21
	addi	21,21,8
	bc	16,0,Lsub

	li	21,0
	mtctr	8
	subfe	3,21,3

.align	4
Lcopy:
	ldx	12,22,21
	ldx	10,9,21
	and	12,12,3
	andc	10,10,3
	stdx	21,22,21
	or	10,10,12
	stdx	10,9,21
	addi	21,21,8
	bc	16,0,Lcopy

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

.globl	.bn_mul4x_mont_int
.align	5
.bn_mul4x_mont_int:
	andi.	0,8,7
	bne	Lmul4x_do
	cmpld	4,5
	bne	Lmul4x_do
	b	Lsqr8x_do
Lmul4x_do:
	slwi	8,8,3
	mr	9,1
	li	10,-32*8
	sub	10,10,8
	stdux	1,1,10

	std	14,-8*18(9)
	std	15,-8*17(9)
	std	16,-8*16(9)
	std	17,-8*15(9)
	std	18,-8*14(9)
	std	19,-8*13(9)
	std	20,-8*12(9)
	std	21,-8*11(9)
	std	22,-8*10(9)
	std	23,-8*9(9)
	std	24,-8*8(9)
	std	25,-8*7(9)
	std	26,-8*6(9)
	std	27,-8*5(9)
	std	28,-8*4(9)
	std	29,-8*3(9)
	std	30,-8*2(9)
	std	31,-8*1(9)

	subi	4,4,8
	subi	6,6,8
	subi	3,3,8
	ld	7,0(7)

	add	14,5,8
	add	30,4,8
	subi	14,14,8*4

	ld	27,8*0(5)
	li	22,0
	ld	9,8*1(4)
	li	23,0
	ld	10,8*2(4)
	li	24,0
	ld	11,8*3(4)
	li	25,0
	ldu	12,8*4(4)
	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)

	std	3,8*6(1)
	std	14,8*7(1)
	li	3,0
	addic	29,1,8*7
	li	31,0
	li	0,0
	b	Loop_mul4x_1st_reduction

.align	5
Loop_mul4x_1st_reduction:
	mulld	14,9,27
	addze	3,3
	mulld	15,10,27
	addi	31,31,8
	mulld	16,11,27
	andi.	31,31,8*4-1
	mulld	17,12,27
	addc	22,22,14
	mulhdu	14,9,27
	adde	23,23,15
	mulhdu	15,10,27
	adde	24,24,16
	mulld	28,22,7
	adde	25,25,17
	mulhdu	16,11,27
	addze	26,0
	mulhdu	17,12,27
	ldx	27,5,31
	addc	23,23,14

	stdu	28,8(29)
	adde	24,24,15
	mulld	15,19,28
	adde	25,25,16
	mulld	16,20,28
	adde	26,26,17
	mulld	17,21,28










	addic	22,22,-1
	mulhdu	14,18,28
	adde	22,23,15
	mulhdu	15,19,28
	adde	23,24,16
	mulhdu	16,20,28
	adde	24,25,17
	mulhdu	17,21,28
	adde	25,26,3
	addze	3,0
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17

	bne	Loop_mul4x_1st_reduction

	cmpld	30,4
	beq	Lmul4x4_post_condition

	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ldu	12,8*4(4)
	ld	28,8*8(1)
	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)
	b	Loop_mul4x_1st_tail

.align	5
Loop_mul4x_1st_tail:
	mulld	14,9,27
	addze	3,3
	mulld	15,10,27
	addi	31,31,8
	mulld	16,11,27
	andi.	31,31,8*4-1
	mulld	17,12,27
	addc	22,22,14
	mulhdu	14,9,27
	adde	23,23,15
	mulhdu	15,10,27
	adde	24,24,16
	mulhdu	16,11,27
	adde	25,25,17
	mulhdu	17,12,27
	addze	26,0
	ldx	27,5,31
	addc	23,23,14
	mulld	14,18,28
	adde	24,24,15
	mulld	15,19,28
	adde	25,25,16
	mulld	16,20,28
	adde	26,26,17
	mulld	17,21,28
	addc	22,22,14
	mulhdu	14,18,28
	adde	23,23,15
	mulhdu	15,19,28
	adde	24,24,16
	mulhdu	16,20,28
	adde	25,25,17
	adde	26,26,3
	mulhdu	17,21,28
	addze	3,0
	addi	28,1,8*8
	ldx	28,28,31
	stdu	22,8(29)
	addc	22,23,14
	adde	23,24,15
	adde	24,25,16
	adde	25,26,17

	bne	Loop_mul4x_1st_tail

	sub	15,30,8
	cmpld	30,4
	beq	Lmul4x_proceed

	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ldu	12,8*4(4)
	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)
	b	Loop_mul4x_1st_tail

.align	5
Lmul4x_proceed:
	ldu	27,8*4(5)
	addze	3,3
	ld	9,8*1(15)
	ld	10,8*2(15)
	ld	11,8*3(15)
	ld	12,8*4(15)
	addi	4,15,8*4
	sub	6,6,8

	std	22,8*1(29)
	std	23,8*2(29)
	std	24,8*3(29)
	std	25,8*4(29)
	std	3,8*5(29)
	ld	22,8*12(1)
	ld	23,8*13(1)
	ld	24,8*14(1)
	ld	25,8*15(1)

	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)
	addic	29,1,8*7
	li	3,0
	b	Loop_mul4x_reduction

.align	5
Loop_mul4x_reduction:
	mulld	14,9,27
	addze	3,3
	mulld	15,10,27
	addi	31,31,8
	mulld	16,11,27
	andi.	31,31,8*4-1
	mulld	17,12,27
	addc	22,22,14
	mulhdu	14,9,27
	adde	23,23,15
	mulhdu	15,10,27
	adde	24,24,16
	mulld	28,22,7
	adde	25,25,17
	mulhdu	16,11,27
	addze	26,0
	mulhdu	17,12,27
	ldx	27,5,31
	addc	23,23,14

	stdu	28,8(29)
	adde	24,24,15
	mulld	15,19,28
	adde	25,25,16
	mulld	16,20,28
	adde	26,26,17
	mulld	17,21,28

	addic	22,22,-1
	mulhdu	14,18,28
	adde	22,23,15
	mulhdu	15,19,28
	adde	23,24,16
	mulhdu	16,20,28
	adde	24,25,17
	mulhdu	17,21,28
	adde	25,26,3
	addze	3,0
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17

	bne	Loop_mul4x_reduction

	ld	14,8*5(29)
	addze	3,3
	ld	15,8*6(29)
	ld	16,8*7(29)
	ld	17,8*8(29)
	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ldu	12,8*4(4)
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17


	ld	28,8*8(1)
	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)
	b	Loop_mul4x_tail

.align	5
Loop_mul4x_tail:
	mulld	14,9,27
	addze	3,3
	mulld	15,10,27
	addi	31,31,8
	mulld	16,11,27
	andi.	31,31,8*4-1
	mulld	17,12,27
	addc	22,22,14
	mulhdu	14,9,27
	adde	23,23,15
	mulhdu	15,10,27
	adde	24,24,16
	mulhdu	16,11,27
	adde	25,25,17
	mulhdu	17,12,27
	addze	26,0
	ldx	27,5,31
	addc	23,23,14
	mulld	14,18,28
	adde	24,24,15
	mulld	15,19,28
	adde	25,25,16
	mulld	16,20,28
	adde	26,26,17
	mulld	17,21,28
	addc	22,22,14
	mulhdu	14,18,28
	adde	23,23,15
	mulhdu	15,19,28
	adde	24,24,16
	mulhdu	16,20,28
	adde	25,25,17
	mulhdu	17,21,28
	adde	26,26,3
	addi	28,1,8*8
	ldx	28,28,31
	addze	3,0
	stdu	22,8(29)
	addc	22,23,14
	adde	23,24,15
	adde	24,25,16
	adde	25,26,17

	bne	Loop_mul4x_tail

	ld	14,8*5(29)
	sub	15,6,8
	addze	3,3
	cmpld	30,4
	beq	Loop_mul4x_break

	ld	15,8*6(29)
	ld	16,8*7(29)
	ld	17,8*8(29)
	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ldu	12,8*4(4)
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17


	ld	18,8*1(6)
	ld	19,8*2(6)
	ld	20,8*3(6)
	ldu	21,8*4(6)
	b	Loop_mul4x_tail

.align	5
Loop_mul4x_break:
	ld	16,8*6(1)
	ld	17,8*7(1)
	addc	9,22,14
	ld	22,8*12(1)
	addze	10,23
	ld	23,8*13(1)
	addze	11,24
	ld	24,8*14(1)
	addze	12,25
	ld	25,8*15(1)
	addze	3,3
	std	9,8*1(29)
	sub	4,30,8
	std	10,8*2(29)
	std	11,8*3(29)
	std	12,8*4(29)
	std	3,8*5(29)

	ld	18,8*1(15)
	ld	19,8*2(15)
	ld	20,8*3(15)
	ld	21,8*4(15)
	addi	6,15,8*4
	cmpld	5,17
	beq	Lmul4x_post

	ldu	27,8*4(5)
	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ldu	12,8*4(4)
	li	3,0
	addic	29,1,8*7
	b	Loop_mul4x_reduction

.align	5
Lmul4x_post:




	srwi	31,8,5
	mr	5,16
	subi	31,31,1
	mr	30,16
	subfc	14,18,22
	addi	29,1,8*15
	subfe	15,19,23

	mtctr	31
Lmul4x_sub:
	ld	18,8*1(6)
	ld	22,8*1(29)
	subfe	16,20,24
	ld	19,8*2(6)
	ld	23,8*2(29)
	subfe	17,21,25
	ld	20,8*3(6)
	ld	24,8*3(29)
	ldu	21,8*4(6)
	ldu	25,8*4(29)
	std	14,8*1(5)
	std	15,8*2(5)
	subfe	14,18,22
	std	16,8*3(5)
	stdu	17,8*4(5)
	subfe	15,19,23
	bc	16,0,Lmul4x_sub

	ld	9,8*1(30)
	std	14,8*1(5)
	ld	14,8*12(1)
	subfe	16,20,24
	ld	10,8*2(30)
	std	15,8*2(5)
	ld	15,8*13(1)
	subfe	17,21,25
	subfe	3,0,3
	addi	29,1,8*12
	ld	11,8*3(30)
	std	16,8*3(5)
	ld	16,8*14(1)
	ld	12,8*4(30)
	std	17,8*4(5)
	ld	17,8*15(1)

	mtctr	31
Lmul4x_cond_copy:
	and	14,14,3
	andc	9,9,3
	std	0,8*0(29)
	and	15,15,3
	andc	10,10,3
	std	0,8*1(29)
	and	16,16,3
	andc	11,11,3
	std	0,8*2(29)
	and	17,17,3
	andc	12,12,3
	std	0,8*3(29)
	or	22,14,9
	ld	9,8*5(30)
	ld	14,8*4(29)
	or	23,15,10
	ld	10,8*6(30)
	ld	15,8*5(29)
	or	24,16,11
	ld	11,8*7(30)
	ld	16,8*6(29)
	or	25,17,12
	ld	12,8*8(30)
	ld	17,8*7(29)
	addi	29,29,8*4
	std	22,8*1(30)
	std	23,8*2(30)
	std	24,8*3(30)
	stdu	25,8*4(30)
	bc	16,0,Lmul4x_cond_copy

	ld	5,0(1)
	and	14,14,3
	andc	9,9,3
	std	0,8*0(29)
	and	15,15,3
	andc	10,10,3
	std	0,8*1(29)
	and	16,16,3
	andc	11,11,3
	std	0,8*2(29)
	and	17,17,3
	andc	12,12,3
	std	0,8*3(29)
	or	22,14,9
	or	23,15,10
	std	0,8*4(29)
	or	24,16,11
	or	25,17,12
	std	22,8*1(30)
	std	23,8*2(30)
	std	24,8*3(30)
	std	25,8*4(30)

	b	Lmul4x_done

.align	4
Lmul4x4_post_condition:
	ld	4,8*6(1)
	ld	5,0(1)
	addze	3,3

	subfc	9,18,22
	subfe	10,19,23
	subfe	11,20,24
	subfe	12,21,25
	subfe	3,0,3

	and	18,18,3
	and	19,19,3
	addc	9,9,18
	and	20,20,3
	adde	10,10,19
	and	21,21,3
	adde	11,11,20
	adde	12,12,21

	std	9,8*1(4)
	std	10,8*2(4)
	std	11,8*3(4)
	std	12,8*4(4)

Lmul4x_done:
	std	0,8*8(1)
	std	0,8*9(1)
	std	0,8*10(1)
	std	0,8*11(1)
	li	3,1
	ld	14,-8*18(5)
	ld	15,-8*17(5)
	ld	16,-8*16(5)
	ld	17,-8*15(5)
	ld	18,-8*14(5)
	ld	19,-8*13(5)
	ld	20,-8*12(5)
	ld	21,-8*11(5)
	ld	22,-8*10(5)
	ld	23,-8*9(5)
	ld	24,-8*8(5)
	ld	25,-8*7(5)
	ld	26,-8*6(5)
	ld	27,-8*5(5)
	ld	28,-8*4(5)
	ld	29,-8*3(5)
	ld	30,-8*2(5)
	ld	31,-8*1(5)
	mr	1,5
	blr	
.long	0
.byte	0,12,4,0x20,0x80,18,6,0
.long	0

.align	5
__bn_sqr8x_mont:
Lsqr8x_do:
	mr	9,1
	slwi	10,8,4
	li	11,-32*8
	sub	10,11,10
	slwi	8,8,3
	stdux	1,1,10

	std	14,-8*18(9)
	std	15,-8*17(9)
	std	16,-8*16(9)
	std	17,-8*15(9)
	std	18,-8*14(9)
	std	19,-8*13(9)
	std	20,-8*12(9)
	std	21,-8*11(9)
	std	22,-8*10(9)
	std	23,-8*9(9)
	std	24,-8*8(9)
	std	25,-8*7(9)
	std	26,-8*6(9)
	std	27,-8*5(9)
	std	28,-8*4(9)
	std	29,-8*3(9)
	std	30,-8*2(9)
	std	31,-8*1(9)

	subi	4,4,8
	subi	18,6,8
	subi	3,3,8
	ld	7,0(7)
	li	0,0

	add	6,4,8
	ld	9,8*1(4)

	ld	10,8*2(4)
	li	23,0
	ld	11,8*3(4)
	li	24,0
	ld	12,8*4(4)
	li	25,0
	ld	14,8*5(4)
	li	26,0
	ld	15,8*6(4)
	li	27,0
	ld	16,8*7(4)
	li	28,0
	ldu	17,8*8(4)
	li	29,0

	addi	5,1,8*11
	subic.	30,8,8*8
	b	Lsqr8x_zero_start

.align	5
Lsqr8x_zero:
	subic.	30,30,8*8
	std	0,8*1(5)
	std	0,8*2(5)
	std	0,8*3(5)
	std	0,8*4(5)
	std	0,8*5(5)
	std	0,8*6(5)
	std	0,8*7(5)
	std	0,8*8(5)
Lsqr8x_zero_start:
	std	0,8*9(5)
	std	0,8*10(5)
	std	0,8*11(5)
	std	0,8*12(5)
	std	0,8*13(5)
	std	0,8*14(5)
	std	0,8*15(5)
	stdu	0,8*16(5)
	bne	Lsqr8x_zero

	std	3,8*6(1)
	std	18,8*7(1)
	std	7,8*8(1)
	std	5,8*9(1)
	std	0,8*10(1)
	addi	5,1,8*11


.align	5
Lsqr8x_outer_loop:





























	mulld	18,10,9
	mulld	19,11,9
	mulld	20,12,9
	mulld	21,14,9
	addc	23,23,18
	mulld	18,15,9
	adde	24,24,19
	mulld	19,16,9
	adde	25,25,20
	mulld	20,17,9
	adde	26,26,21
	mulhdu	21,10,9
	adde	27,27,18
	mulhdu	18,11,9
	adde	28,28,19
	mulhdu	19,12,9
	adde	29,29,20
	mulhdu	20,14,9
	std	22,8*1(5)
	addze	22,0
	std	23,8*2(5)
	addc	24,24,21
	mulhdu	21,15,9
	adde	25,25,18
	mulhdu	18,16,9
	adde	26,26,19
	mulhdu	19,17,9
	adde	27,27,20
	mulld	20,11,10
	adde	28,28,21
	mulld	21,12,10
	adde	29,29,18
	mulld	18,14,10
	adde	22,22,19

	mulld	19,15,10
	addc	25,25,20
	mulld	20,16,10
	adde	26,26,21
	mulld	21,17,10
	adde	27,27,18
	mulhdu	18,11,10
	adde	28,28,19
	mulhdu	19,12,10
	adde	29,29,20
	mulhdu	20,14,10
	adde	22,22,21
	mulhdu	21,15,10
	std	24,8*3(5)
	addze	23,0
	std	25,8*4(5)
	addc	26,26,18
	mulhdu	18,16,10
	adde	27,27,19
	mulhdu	19,17,10
	adde	28,28,20
	mulld	20,12,11
	adde	29,29,21
	mulld	21,14,11
	adde	22,22,18
	mulld	18,15,11
	adde	23,23,19

	mulld	19,16,11
	addc	27,27,20
	mulld	20,17,11
	adde	28,28,21
	mulhdu	21,12,11
	adde	29,29,18
	mulhdu	18,14,11
	adde	22,22,19
	mulhdu	19,15,11
	adde	23,23,20
	mulhdu	20,16,11
	std	26,8*5(5)
	addze	24,0
	std	27,8*6(5)
	addc	28,28,21
	mulhdu	21,17,11
	adde	29,29,18
	mulld	18,14,12
	adde	22,22,19
	mulld	19,15,12
	adde	23,23,20
	mulld	20,16,12
	adde	24,24,21

	mulld	21,17,12
	addc	29,29,18
	mulhdu	18,14,12
	adde	22,22,19
	mulhdu	19,15,12
	adde	23,23,20
	mulhdu	20,16,12
	adde	24,24,21
	mulhdu	21,17,12
	std	28,8*7(5)
	addze	25,0
	stdu	29,8*8(5)
	addc	22,22,18
	mulld	18,15,14
	adde	23,23,19
	mulld	19,16,14
	adde	24,24,20
	mulld	20,17,14
	adde	25,25,21

	mulhdu	21,15,14
	addc	23,23,18
	mulhdu	18,16,14
	adde	24,24,19
	mulhdu	19,17,14
	adde	25,25,20
	mulld	20,16,15
	addze	26,0
	addc	24,24,21
	mulld	21,17,15
	adde	25,25,18
	mulhdu	18,16,15
	adde	26,26,19

	mulhdu	19,17,15
	addc	25,25,20
	mulld	20,17,16
	adde	26,26,21
	mulhdu	21,17,16
	addze	27,0
	addc	26,26,18
	cmpld	6,4
	adde	27,27,19

	addc	27,27,20
	sub	18,6,8
	addze	28,0
	add	28,28,21

	beq	Lsqr8x_outer_break

	mr	7,9
	ld	9,8*1(5)
	ld	10,8*2(5)
	ld	11,8*3(5)
	ld	12,8*4(5)
	ld	14,8*5(5)
	ld	15,8*6(5)
	ld	16,8*7(5)
	ld	17,8*8(5)
	addc	22,22,9
	ld	9,8*1(4)
	adde	23,23,10
	ld	10,8*2(4)
	adde	24,24,11
	ld	11,8*3(4)
	adde	25,25,12
	ld	12,8*4(4)
	adde	26,26,14
	ld	14,8*5(4)
	adde	27,27,15
	ld	15,8*6(4)
	adde	28,28,16
	ld	16,8*7(4)
	subi	3,4,8*7
	addze	29,17
	ldu	17,8*8(4)

	li	30,0
	b	Lsqr8x_mul























.align	5
Lsqr8x_mul:
	mulld	18,9,7
	addze	31,0
	mulld	19,10,7
	addi	30,30,8
	mulld	20,11,7
	andi.	30,30,8*8-1
	mulld	21,12,7
	addc	22,22,18
	mulld	18,14,7
	adde	23,23,19
	mulld	19,15,7
	adde	24,24,20
	mulld	20,16,7
	adde	25,25,21
	mulld	21,17,7
	adde	26,26,18
	mulhdu	18,9,7
	adde	27,27,19
	mulhdu	19,10,7
	adde	28,28,20
	mulhdu	20,11,7
	adde	29,29,21
	mulhdu	21,12,7
	addze	31,31
	stdu	22,8(5)
	addc	22,23,18
	mulhdu	18,14,7
	adde	23,24,19
	mulhdu	19,15,7
	adde	24,25,20
	mulhdu	20,16,7
	adde	25,26,21
	mulhdu	21,17,7
	ldx	7,3,30
	adde	26,27,18
	adde	27,28,19
	adde	28,29,20
	adde	29,31,21

	bne	Lsqr8x_mul


	cmpld	4,6
	beq	Lsqr8x_break

	ld	9,8*1(5)
	ld	10,8*2(5)
	ld	11,8*3(5)
	ld	12,8*4(5)
	ld	14,8*5(5)
	ld	15,8*6(5)
	ld	16,8*7(5)
	ld	17,8*8(5)
	addc	22,22,9
	ld	9,8*1(4)
	adde	23,23,10
	ld	10,8*2(4)
	adde	24,24,11
	ld	11,8*3(4)
	adde	25,25,12
	ld	12,8*4(4)
	adde	26,26,14
	ld	14,8*5(4)
	adde	27,27,15
	ld	15,8*6(4)
	adde	28,28,16
	ld	16,8*7(4)
	adde	29,29,17
	ldu	17,8*8(4)

	b	Lsqr8x_mul

.align	5
Lsqr8x_break:
	ld	9,8*8(3)
	addi	4,3,8*15
	ld	10,8*9(3)
	sub.	18,6,4
	ld	11,8*10(3)
	sub	19,5,18
	ld	12,8*11(3)
	ld	14,8*12(3)
	ld	15,8*13(3)
	ld	16,8*14(3)
	ld	17,8*15(3)
	beq	Lsqr8x_outer_loop

	std	22,8*1(5)
	ld	22,8*1(19)
	std	23,8*2(5)
	ld	23,8*2(19)
	std	24,8*3(5)
	ld	24,8*3(19)
	std	25,8*4(5)
	ld	25,8*4(19)
	std	26,8*5(5)
	ld	26,8*5(19)
	std	27,8*6(5)
	ld	27,8*6(19)
	std	28,8*7(5)
	ld	28,8*7(19)
	std	29,8*8(5)
	ld	29,8*8(19)
	mr	5,19
	b	Lsqr8x_outer_loop

.align	5
Lsqr8x_outer_break:


	ld	10,8*1(18)
	ld	12,8*2(18)
	ld	15,8*3(18)
	ld	17,8*4(18)
	addi	4,18,8*4

	ld	19,8*13(1)
	ld	20,8*14(1)
	ld	21,8*15(1)
	ld	18,8*16(1)

	std	22,8*1(5)
	srwi	30,8,5
	std	23,8*2(5)
	subi	30,30,1
	std	24,8*3(5)
	std	25,8*4(5)
	std	26,8*5(5)
	std	27,8*6(5)
	std	28,8*7(5)

	addi	5,1,8*11
	mulld	22,10,10
	mulhdu	10,10,10
	add	23,19,19
	srdi	19,19,64-1
	mulld	11,12,12
	mulhdu	12,12,12
	addc	23,23,10
	add	24,20,20
	srdi	20,20,64-1
	add	25,21,21
	srdi	21,21,64-1
	or	24,24,19

	mtctr	30
Lsqr4x_shift_n_add:
	mulld	14,15,15
	mulhdu	15,15,15
	ld	19,8*6(5)
	ld	10,8*1(4)
	adde	24,24,11
	add	26,18,18
	srdi	18,18,64-1
	or	25,25,20
	ld	20,8*7(5)
	adde	25,25,12
	ld	12,8*2(4)
	add	27,19,19
	srdi	19,19,64-1
	or	26,26,21
	ld	21,8*8(5)
	mulld	16,17,17
	mulhdu	17,17,17
	adde	26,26,14
	add	28,20,20
	srdi	20,20,64-1
	or	27,27,18
	ld	18,8*9(5)
	adde	27,27,15
	ld	15,8*3(4)
	add	29,21,21
	srdi	21,21,64-1
	or	28,28,19
	ld	19,8*10(5)
	mulld	9,10,10
	mulhdu	10,10,10
	adde	28,28,16
	std	22,8*1(5)
	add	22,18,18
	srdi	18,18,64-1
	or	29,29,20
	ld	20,8*11(5)
	adde	29,29,17
	ldu	17,8*4(4)
	std	23,8*2(5)
	add	23,19,19
	srdi	19,19,64-1
	or	22,22,21
	ld	21,8*12(5)
	mulld	11,12,12
	mulhdu	12,12,12
	adde	22,22,9
	std	24,8*3(5)
	add	24,20,20
	srdi	20,20,64-1
	or	23,23,18
	ld	18,8*13(5)
	adde	23,23,10
	std	25,8*4(5)
	std	26,8*5(5)
	std	27,8*6(5)
	std	28,8*7(5)
	stdu	29,8*8(5)
	add	25,21,21
	srdi	21,21,64-1
	or	24,24,19
	bc	16,0,Lsqr4x_shift_n_add
	ld	4,8*7(1)
	ld	7,8*8(1)

	mulld	14,15,15
	mulhdu	15,15,15
	std	22,8*1(5)
	ld	22,8*12(1)
	ld	19,8*6(5)
	adde	24,24,11
	add	26,18,18
	srdi	18,18,64-1
	or	25,25,20
	ld	20,8*7(5)
	adde	25,25,12
	add	27,19,19
	srdi	19,19,64-1
	or	26,26,21
	mulld	16,17,17
	mulhdu	17,17,17
	adde	26,26,14
	add	28,20,20
	srdi	20,20,64-1
	or	27,27,18
	std	23,8*2(5)
	ld	23,8*13(1)
	adde	27,27,15
	or	28,28,19
	ld	9,8*1(4)
	ld	10,8*2(4)
	adde	28,28,16
	ld	11,8*3(4)
	ld	12,8*4(4)
	adde	29,17,20
	ld	14,8*5(4)
	ld	15,8*6(4)



	mulld	31,7,22
	li	30,8
	ld	16,8*7(4)
	add	6,4,8
	ldu	17,8*8(4)
	std	24,8*3(5)
	ld	24,8*14(1)
	std	25,8*4(5)
	ld	25,8*15(1)
	std	26,8*5(5)
	ld	26,8*16(1)
	std	27,8*6(5)
	ld	27,8*17(1)
	std	28,8*7(5)
	ld	28,8*18(1)
	std	29,8*8(5)
	ld	29,8*19(1)
	addi	5,1,8*11
	mtctr	30
	b	Lsqr8x_reduction

.align	5
Lsqr8x_reduction:

	mulld	19,10,31
	mulld	20,11,31
	stdu	31,8(5)
	mulld	21,12,31

	addic	22,22,-1
	mulld	18,14,31
	adde	22,23,19
	mulld	19,15,31
	adde	23,24,20
	mulld	20,16,31
	adde	24,25,21
	mulld	21,17,31
	adde	25,26,18
	mulhdu	18,9,31
	adde	26,27,19
	mulhdu	19,10,31
	adde	27,28,20
	mulhdu	20,11,31
	adde	28,29,21
	mulhdu	21,12,31
	addze	29,0
	addc	22,22,18
	mulhdu	18,14,31
	adde	23,23,19
	mulhdu	19,15,31
	adde	24,24,20
	mulhdu	20,16,31
	adde	25,25,21
	mulhdu	21,17,31
	mulld	31,7,22
	adde	26,26,18
	adde	27,27,19
	adde	28,28,20
	adde	29,29,21
	bc	16,0,Lsqr8x_reduction

	ld	18,8*1(5)
	ld	19,8*2(5)
	ld	20,8*3(5)
	ld	21,8*4(5)
	subi	3,5,8*7
	cmpld	6,4
	addc	22,22,18
	ld	18,8*5(5)
	adde	23,23,19
	ld	19,8*6(5)
	adde	24,24,20
	ld	20,8*7(5)
	adde	25,25,21
	ld	21,8*8(5)
	adde	26,26,18
	adde	27,27,19
	adde	28,28,20
	adde	29,29,21

	beq	Lsqr8x8_post_condition

	ld	7,8*0(3)
	ld	9,8*1(4)
	ld	10,8*2(4)
	ld	11,8*3(4)
	ld	12,8*4(4)
	ld	14,8*5(4)
	ld	15,8*6(4)
	ld	16,8*7(4)
	ldu	17,8*8(4)
	li	30,0

.align	5
Lsqr8x_tail:
	mulld	18,9,7
	addze	31,0
	mulld	19,10,7
	addi	30,30,8
	mulld	20,11,7
	andi.	30,30,8*8-1
	mulld	21,12,7
	addc	22,22,18
	mulld	18,14,7
	adde	23,23,19
	mulld	19,15,7
	adde	24,24,20
	mulld	20,16,7
	adde	25,25,21
	mulld	21,17,7
	adde	26,26,18
	mulhdu	18,9,7
	adde	27,27,19
	mulhdu	19,10,7
	adde	28,28,20
	mulhdu	20,11,7
	adde	29,29,21
	mulhdu	21,12,7
	addze	31,31
	stdu	22,8(5)
	addc	22,23,18
	mulhdu	18,14,7
	adde	23,24,19
	mulhdu	19,15,7
	adde	24,25,20
	mulhdu	20,16,7
	adde	25,26,21
	mulhdu	21,17,7
	ldx	7,3,30
	adde	26,27,18
	adde	27,28,19
	adde	28,29,20
	adde	29,31,21

	bne	Lsqr8x_tail


	ld	9,8*1(5)
	ld	31,8*10(1)
	cmpld	6,4
	ld	10,8*2(5)
	sub	20,6,8
	ld	11,8*3(5)
	ld	12,8*4(5)
	ld	14,8*5(5)
	ld	15,8*6(5)
	ld	16,8*7(5)
	ld	17,8*8(5)
	beq	Lsqr8x_tail_break

	addc	22,22,9
	ld	9,8*1(4)
	adde	23,23,10
	ld	10,8*2(4)
	adde	24,24,11
	ld	11,8*3(4)
	adde	25,25,12
	ld	12,8*4(4)
	adde	26,26,14
	ld	14,8*5(4)
	adde	27,27,15
	ld	15,8*6(4)
	adde	28,28,16
	ld	16,8*7(4)
	adde	29,29,17
	ldu	17,8*8(4)

	b	Lsqr8x_tail

.align	5
Lsqr8x_tail_break:
	ld	7,8*8(1)
	ld	21,8*9(1)
	addi	30,5,8*8

	addic	31,31,-1
	adde	18,22,9
	ld	22,8*8(3)
	ld	9,8*1(20)
	adde	19,23,10
	ld	23,8*9(3)
	ld	10,8*2(20)
	adde	24,24,11
	ld	11,8*3(20)
	adde	25,25,12
	ld	12,8*4(20)
	adde	26,26,14
	ld	14,8*5(20)
	adde	27,27,15
	ld	15,8*6(20)
	adde	28,28,16
	ld	16,8*7(20)
	adde	29,29,17
	ld	17,8*8(20)
	addi	4,20,8*8
	addze	20,0
	mulld	31,7,22
	std	18,8*1(5)
	cmpld	30,21
	std	19,8*2(5)
	li	30,8
	std	24,8*3(5)
	ld	24,8*10(3)
	std	25,8*4(5)
	ld	25,8*11(3)
	std	26,8*5(5)
	ld	26,8*12(3)
	std	27,8*6(5)
	ld	27,8*13(3)
	std	28,8*7(5)
	ld	28,8*14(3)
	std	29,8*8(5)
	ld	29,8*15(3)
	std	20,8*10(1)
	addi	5,3,8*7
	mtctr	30
	bne	Lsqr8x_reduction






	ld	3,8*6(1)
	srwi	30,8,6
	mr	7,5
	addi	5,5,8*8
	subi	30,30,1
	subfc	18,9,22
	subfe	19,10,23
	mr	31,20
	mr	6,3

	mtctr	30
	b	Lsqr8x_sub

.align	5
Lsqr8x_sub:
	ld	9,8*1(4)
	ld	22,8*1(5)
	ld	10,8*2(4)
	ld	23,8*2(5)
	subfe	20,11,24
	ld	11,8*3(4)
	ld	24,8*3(5)
	subfe	21,12,25
	ld	12,8*4(4)
	ld	25,8*4(5)
	std	18,8*1(3)
	subfe	18,14,26
	ld	14,8*5(4)
	ld	26,8*5(5)
	std	19,8*2(3)
	subfe	19,15,27
	ld	15,8*6(4)
	ld	27,8*6(5)
	std	20,8*3(3)
	subfe	20,16,28
	ld	16,8*7(4)
	ld	28,8*7(5)
	std	21,8*4(3)
	subfe	21,17,29
	ldu	17,8*8(4)
	ldu	29,8*8(5)
	std	18,8*5(3)
	subfe	18,9,22
	std	19,8*6(3)
	subfe	19,10,23
	std	20,8*7(3)
	stdu	21,8*8(3)
	bc	16,0,Lsqr8x_sub

	srwi	30,8,5
	ld	9,8*1(6)
	ld	22,8*1(7)
	subi	30,30,1
	ld	10,8*2(6)
	ld	23,8*2(7)
	subfe	20,11,24
	ld	11,8*3(6)
	ld	24,8*3(7)
	subfe	21,12,25
	ld	12,8*4(6)
	ldu	25,8*4(7)
	std	18,8*1(3)
	subfe	18,14,26
	std	19,8*2(3)
	subfe	19,15,27
	std	20,8*3(3)
	subfe	20,16,28
	std	21,8*4(3)
	subfe	21,17,29
	std	18,8*5(3)
	subfe	31,0,31
	std	19,8*6(3)
	std	20,8*7(3)
	std	21,8*8(3)

	addi	5,1,8*11
	mtctr	30

Lsqr4x_cond_copy:
	andc	9,9,31
	std	0,-8*3(7)
	and	22,22,31
	std	0,-8*2(7)
	andc	10,10,31
	std	0,-8*1(7)
	and	23,23,31
	std	0,-8*0(7)
	andc	11,11,31
	std	0,8*1(5)
	and	24,24,31
	std	0,8*2(5)
	andc	12,12,31
	std	0,8*3(5)
	and	25,25,31
	stdu	0,8*4(5)
	or	18,9,22
	ld	9,8*5(6)
	ld	22,8*1(7)
	or	19,10,23
	ld	10,8*6(6)
	ld	23,8*2(7)
	or	20,11,24
	ld	11,8*7(6)
	ld	24,8*3(7)
	or	21,12,25
	ld	12,8*8(6)
	ldu	25,8*4(7)
	std	18,8*1(6)
	std	19,8*2(6)
	std	20,8*3(6)
	stdu	21,8*4(6)
	bc	16,0,Lsqr4x_cond_copy

	ld	4,0(1)
	andc	9,9,31
	and	22,22,31
	andc	10,10,31
	and	23,23,31
	andc	11,11,31
	and	24,24,31
	andc	12,12,31
	and	25,25,31
	or	18,9,22
	or	19,10,23
	or	20,11,24
	or	21,12,25
	std	18,8*1(6)
	std	19,8*2(6)
	std	20,8*3(6)
	std	21,8*4(6)

	b	Lsqr8x_done

.align	5
Lsqr8x8_post_condition:
	ld	3,8*6(1)
	ld	4,0(1)
	addze	31,0


	subfc	22,9,22
	subfe	23,10,23
	std	0,8*12(1)
	std	0,8*13(1)
	subfe	24,11,24
	std	0,8*14(1)
	std	0,8*15(1)
	subfe	25,12,25
	std	0,8*16(1)
	std	0,8*17(1)
	subfe	26,14,26
	std	0,8*18(1)
	std	0,8*19(1)
	subfe	27,15,27
	std	0,8*20(1)
	std	0,8*21(1)
	subfe	28,16,28
	std	0,8*22(1)
	std	0,8*23(1)
	subfe	29,17,29
	std	0,8*24(1)
	std	0,8*25(1)
	subfe	31,0,31
	std	0,8*26(1)
	std	0,8*27(1)

	and	9,9,31
	and	10,10,31
	addc	22,22,9
	and	11,11,31
	adde	23,23,10
	and	12,12,31
	adde	24,24,11
	and	14,14,31
	adde	25,25,12
	and	15,15,31
	adde	26,26,14
	and	16,16,31
	adde	27,27,15
	and	17,17,31
	adde	28,28,16
	adde	29,29,17
	std	22,8*1(3)
	std	23,8*2(3)
	std	24,8*3(3)
	std	25,8*4(3)
	std	26,8*5(3)
	std	27,8*6(3)
	std	28,8*7(3)
	std	29,8*8(3)

Lsqr8x_done:
	std	0,8*8(1)
	std	0,8*10(1)

	ld	14,-8*18(4)
	li	3,1
	ld	15,-8*17(4)
	ld	16,-8*16(4)
	ld	17,-8*15(4)
	ld	18,-8*14(4)
	ld	19,-8*13(4)
	ld	20,-8*12(4)
	ld	21,-8*11(4)
	ld	22,-8*10(4)
	ld	23,-8*9(4)
	ld	24,-8*8(4)
	ld	25,-8*7(4)
	ld	26,-8*6(4)
	ld	27,-8*5(4)
	ld	28,-8*4(4)
	ld	29,-8*3(4)
	ld	30,-8*2(4)
	ld	31,-8*1(4)
	mr	1,4
	blr	
.long	0
.byte	0,12,4,0x20,0x80,18,6,0
.long	0

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
