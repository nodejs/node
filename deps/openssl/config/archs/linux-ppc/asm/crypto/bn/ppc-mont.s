.machine	"any"
.text

.globl	bn_mul_mont_int
.type	bn_mul_mont_int,@function
.align	5
bn_mul_mont_int:
	mr	9,3
	li	3,0
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
.L1st:
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
	bdnz	.L1st

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
.Louter:
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
.Linner:
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
	bdnz	.Linner

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
	.long	0x7c146040
	addi	20,20,4
	ble	.Louter

	addi	8,8,2
	subfc	21,21,21
	addi	22,1,32
	mtctr	8

.align	4
.Lsub:	lwzx	12,22,21
	lwzx	11,6,21
	subfe	10,11,12
	stwx	10,9,21
	addi	21,21,4
	bdnz	.Lsub

	li	21,0
	mtctr	8
	subfe	3,21,3

.align	4
.Lcopy:
	lwzx	12,22,21
	lwzx	10,9,21
	and	12,12,3
	andc	10,10,3
	stwx	21,22,21
	or	10,10,12
	stwx	10,9,21
	addi	21,21,4
	bdnz	.Lcopy

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
.size	bn_mul_mont_int,.-bn_mul_mont_int
.globl	bn_mul4x_mont_int
.type	bn_mul4x_mont_int,@function
.align	5
bn_mul4x_mont_int:
	andi.	0,8,7
	bne	.Lmul4x_do
	.long	0x7c042840
	bne	.Lmul4x_do
	b	.Lsqr8x_do
.Lmul4x_do:
	slwi	8,8,2
	mr	9,1
	li	10,-32*4
	sub	10,10,8
	stwux	1,1,10

	stw	14,-4*18(9)
	stw	15,-4*17(9)
	stw	16,-4*16(9)
	stw	17,-4*15(9)
	stw	18,-4*14(9)
	stw	19,-4*13(9)
	stw	20,-4*12(9)
	stw	21,-4*11(9)
	stw	22,-4*10(9)
	stw	23,-4*9(9)
	stw	24,-4*8(9)
	stw	25,-4*7(9)
	stw	26,-4*6(9)
	stw	27,-4*5(9)
	stw	28,-4*4(9)
	stw	29,-4*3(9)
	stw	30,-4*2(9)
	stw	31,-4*1(9)

	subi	4,4,4
	subi	6,6,4
	subi	3,3,4
	lwz	7,0(7)

	add	14,5,8
	add	30,4,8
	subi	14,14,4*4

	lwz	27,4*0(5)
	li	22,0
	lwz	9,4*1(4)
	li	23,0
	lwz	10,4*2(4)
	li	24,0
	lwz	11,4*3(4)
	li	25,0
	lwzu	12,4*4(4)
	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)

	stw	3,4*6(1)
	stw	14,4*7(1)
	li	3,0
	addic	29,1,4*7
	li	31,0
	li	0,0
	b	.Loop_mul4x_1st_reduction

.align	5
.Loop_mul4x_1st_reduction:
	mullw	14,9,27
	addze	3,3
	mullw	15,10,27
	addi	31,31,4
	mullw	16,11,27
	andi.	31,31,4*4-1
	mullw	17,12,27
	addc	22,22,14
	mulhwu	14,9,27
	adde	23,23,15
	mulhwu	15,10,27
	adde	24,24,16
	mullw	28,22,7
	adde	25,25,17
	mulhwu	16,11,27
	addze	26,0
	mulhwu	17,12,27
	lwzx	27,5,31
	addc	23,23,14

	stwu	28,4(29)
	adde	24,24,15
	mullw	15,19,28
	adde	25,25,16
	mullw	16,20,28
	adde	26,26,17
	mullw	17,21,28










	addic	22,22,-1
	mulhwu	14,18,28
	adde	22,23,15
	mulhwu	15,19,28
	adde	23,24,16
	mulhwu	16,20,28
	adde	24,25,17
	mulhwu	17,21,28
	adde	25,26,3
	addze	3,0
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17

	bne	.Loop_mul4x_1st_reduction

	.long	0x7c1e2040
	beq	.Lmul4x4_post_condition

	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwzu	12,4*4(4)
	lwz	28,4*8(1)
	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)
	b	.Loop_mul4x_1st_tail

.align	5
.Loop_mul4x_1st_tail:
	mullw	14,9,27
	addze	3,3
	mullw	15,10,27
	addi	31,31,4
	mullw	16,11,27
	andi.	31,31,4*4-1
	mullw	17,12,27
	addc	22,22,14
	mulhwu	14,9,27
	adde	23,23,15
	mulhwu	15,10,27
	adde	24,24,16
	mulhwu	16,11,27
	adde	25,25,17
	mulhwu	17,12,27
	addze	26,0
	lwzx	27,5,31
	addc	23,23,14
	mullw	14,18,28
	adde	24,24,15
	mullw	15,19,28
	adde	25,25,16
	mullw	16,20,28
	adde	26,26,17
	mullw	17,21,28
	addc	22,22,14
	mulhwu	14,18,28
	adde	23,23,15
	mulhwu	15,19,28
	adde	24,24,16
	mulhwu	16,20,28
	adde	25,25,17
	adde	26,26,3
	mulhwu	17,21,28
	addze	3,0
	addi	28,1,4*8
	lwzx	28,28,31
	stwu	22,4(29)
	addc	22,23,14
	adde	23,24,15
	adde	24,25,16
	adde	25,26,17

	bne	.Loop_mul4x_1st_tail

	sub	15,30,8
	.long	0x7c1e2040
	beq	.Lmul4x_proceed

	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwzu	12,4*4(4)
	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)
	b	.Loop_mul4x_1st_tail

.align	5
.Lmul4x_proceed:
	lwzu	27,4*4(5)
	addze	3,3
	lwz	9,4*1(15)
	lwz	10,4*2(15)
	lwz	11,4*3(15)
	lwz	12,4*4(15)
	addi	4,15,4*4
	sub	6,6,8

	stw	22,4*1(29)
	stw	23,4*2(29)
	stw	24,4*3(29)
	stw	25,4*4(29)
	stw	3,4*5(29)
	lwz	22,4*12(1)
	lwz	23,4*13(1)
	lwz	24,4*14(1)
	lwz	25,4*15(1)

	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)
	addic	29,1,4*7
	li	3,0
	b	.Loop_mul4x_reduction

.align	5
.Loop_mul4x_reduction:
	mullw	14,9,27
	addze	3,3
	mullw	15,10,27
	addi	31,31,4
	mullw	16,11,27
	andi.	31,31,4*4-1
	mullw	17,12,27
	addc	22,22,14
	mulhwu	14,9,27
	adde	23,23,15
	mulhwu	15,10,27
	adde	24,24,16
	mullw	28,22,7
	adde	25,25,17
	mulhwu	16,11,27
	addze	26,0
	mulhwu	17,12,27
	lwzx	27,5,31
	addc	23,23,14

	stwu	28,4(29)
	adde	24,24,15
	mullw	15,19,28
	adde	25,25,16
	mullw	16,20,28
	adde	26,26,17
	mullw	17,21,28

	addic	22,22,-1
	mulhwu	14,18,28
	adde	22,23,15
	mulhwu	15,19,28
	adde	23,24,16
	mulhwu	16,20,28
	adde	24,25,17
	mulhwu	17,21,28
	adde	25,26,3
	addze	3,0
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17

	bne	.Loop_mul4x_reduction

	lwz	14,4*5(29)
	addze	3,3
	lwz	15,4*6(29)
	lwz	16,4*7(29)
	lwz	17,4*8(29)
	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwzu	12,4*4(4)
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17


	lwz	28,4*8(1)
	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)
	b	.Loop_mul4x_tail

.align	5
.Loop_mul4x_tail:
	mullw	14,9,27
	addze	3,3
	mullw	15,10,27
	addi	31,31,4
	mullw	16,11,27
	andi.	31,31,4*4-1
	mullw	17,12,27
	addc	22,22,14
	mulhwu	14,9,27
	adde	23,23,15
	mulhwu	15,10,27
	adde	24,24,16
	mulhwu	16,11,27
	adde	25,25,17
	mulhwu	17,12,27
	addze	26,0
	lwzx	27,5,31
	addc	23,23,14
	mullw	14,18,28
	adde	24,24,15
	mullw	15,19,28
	adde	25,25,16
	mullw	16,20,28
	adde	26,26,17
	mullw	17,21,28
	addc	22,22,14
	mulhwu	14,18,28
	adde	23,23,15
	mulhwu	15,19,28
	adde	24,24,16
	mulhwu	16,20,28
	adde	25,25,17
	mulhwu	17,21,28
	adde	26,26,3
	addi	28,1,4*8
	lwzx	28,28,31
	addze	3,0
	stwu	22,4(29)
	addc	22,23,14
	adde	23,24,15
	adde	24,25,16
	adde	25,26,17

	bne	.Loop_mul4x_tail

	lwz	14,4*5(29)
	sub	15,6,8
	addze	3,3
	.long	0x7c1e2040
	beq	.Loop_mul4x_break

	lwz	15,4*6(29)
	lwz	16,4*7(29)
	lwz	17,4*8(29)
	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwzu	12,4*4(4)
	addc	22,22,14
	adde	23,23,15
	adde	24,24,16
	adde	25,25,17


	lwz	18,4*1(6)
	lwz	19,4*2(6)
	lwz	20,4*3(6)
	lwzu	21,4*4(6)
	b	.Loop_mul4x_tail

.align	5
.Loop_mul4x_break:
	lwz	16,4*6(1)
	lwz	17,4*7(1)
	addc	9,22,14
	lwz	22,4*12(1)
	addze	10,23
	lwz	23,4*13(1)
	addze	11,24
	lwz	24,4*14(1)
	addze	12,25
	lwz	25,4*15(1)
	addze	3,3
	stw	9,4*1(29)
	sub	4,30,8
	stw	10,4*2(29)
	stw	11,4*3(29)
	stw	12,4*4(29)
	stw	3,4*5(29)

	lwz	18,4*1(15)
	lwz	19,4*2(15)
	lwz	20,4*3(15)
	lwz	21,4*4(15)
	addi	6,15,4*4
	.long	0x7c058840
	beq	.Lmul4x_post

	lwzu	27,4*4(5)
	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwzu	12,4*4(4)
	li	3,0
	addic	29,1,4*7
	b	.Loop_mul4x_reduction

.align	5
.Lmul4x_post:




	srwi	31,8,4
	mr	5,16
	subi	31,31,1
	mr	30,16
	subfc	14,18,22
	addi	29,1,4*15
	subfe	15,19,23

	mtctr	31
.Lmul4x_sub:
	lwz	18,4*1(6)
	lwz	22,4*1(29)
	subfe	16,20,24
	lwz	19,4*2(6)
	lwz	23,4*2(29)
	subfe	17,21,25
	lwz	20,4*3(6)
	lwz	24,4*3(29)
	lwzu	21,4*4(6)
	lwzu	25,4*4(29)
	stw	14,4*1(5)
	stw	15,4*2(5)
	subfe	14,18,22
	stw	16,4*3(5)
	stwu	17,4*4(5)
	subfe	15,19,23
	bdnz	.Lmul4x_sub

	lwz	9,4*1(30)
	stw	14,4*1(5)
	lwz	14,4*12(1)
	subfe	16,20,24
	lwz	10,4*2(30)
	stw	15,4*2(5)
	lwz	15,4*13(1)
	subfe	17,21,25
	subfe	3,0,3
	addi	29,1,4*12
	lwz	11,4*3(30)
	stw	16,4*3(5)
	lwz	16,4*14(1)
	lwz	12,4*4(30)
	stw	17,4*4(5)
	lwz	17,4*15(1)

	mtctr	31
.Lmul4x_cond_copy:
	and	14,14,3
	andc	9,9,3
	stw	0,4*0(29)
	and	15,15,3
	andc	10,10,3
	stw	0,4*1(29)
	and	16,16,3
	andc	11,11,3
	stw	0,4*2(29)
	and	17,17,3
	andc	12,12,3
	stw	0,4*3(29)
	or	22,14,9
	lwz	9,4*5(30)
	lwz	14,4*4(29)
	or	23,15,10
	lwz	10,4*6(30)
	lwz	15,4*5(29)
	or	24,16,11
	lwz	11,4*7(30)
	lwz	16,4*6(29)
	or	25,17,12
	lwz	12,4*8(30)
	lwz	17,4*7(29)
	addi	29,29,4*4
	stw	22,4*1(30)
	stw	23,4*2(30)
	stw	24,4*3(30)
	stwu	25,4*4(30)
	bdnz	.Lmul4x_cond_copy

	lwz	5,0(1)
	and	14,14,3
	andc	9,9,3
	stw	0,4*0(29)
	and	15,15,3
	andc	10,10,3
	stw	0,4*1(29)
	and	16,16,3
	andc	11,11,3
	stw	0,4*2(29)
	and	17,17,3
	andc	12,12,3
	stw	0,4*3(29)
	or	22,14,9
	or	23,15,10
	stw	0,4*4(29)
	or	24,16,11
	or	25,17,12
	stw	22,4*1(30)
	stw	23,4*2(30)
	stw	24,4*3(30)
	stw	25,4*4(30)

	b	.Lmul4x_done

.align	4
.Lmul4x4_post_condition:
	lwz	4,4*6(1)
	lwz	5,0(1)
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

	stw	9,4*1(4)
	stw	10,4*2(4)
	stw	11,4*3(4)
	stw	12,4*4(4)

.Lmul4x_done:
	stw	0,4*8(1)
	stw	0,4*9(1)
	stw	0,4*10(1)
	stw	0,4*11(1)
	li	3,1
	lwz	14,-4*18(5)
	lwz	15,-4*17(5)
	lwz	16,-4*16(5)
	lwz	17,-4*15(5)
	lwz	18,-4*14(5)
	lwz	19,-4*13(5)
	lwz	20,-4*12(5)
	lwz	21,-4*11(5)
	lwz	22,-4*10(5)
	lwz	23,-4*9(5)
	lwz	24,-4*8(5)
	lwz	25,-4*7(5)
	lwz	26,-4*6(5)
	lwz	27,-4*5(5)
	lwz	28,-4*4(5)
	lwz	29,-4*3(5)
	lwz	30,-4*2(5)
	lwz	31,-4*1(5)
	mr	1,5
	blr	
.long	0
.byte	0,12,4,0x20,0x80,18,6,0
.long	0
.size	bn_mul4x_mont_int,.-bn_mul4x_mont_int
.align	5
__bn_sqr8x_mont:
.Lsqr8x_do:
	mr	9,1
	slwi	10,8,3
	li	11,-32*4
	sub	10,11,10
	slwi	8,8,2
	stwux	1,1,10

	stw	14,-4*18(9)
	stw	15,-4*17(9)
	stw	16,-4*16(9)
	stw	17,-4*15(9)
	stw	18,-4*14(9)
	stw	19,-4*13(9)
	stw	20,-4*12(9)
	stw	21,-4*11(9)
	stw	22,-4*10(9)
	stw	23,-4*9(9)
	stw	24,-4*8(9)
	stw	25,-4*7(9)
	stw	26,-4*6(9)
	stw	27,-4*5(9)
	stw	28,-4*4(9)
	stw	29,-4*3(9)
	stw	30,-4*2(9)
	stw	31,-4*1(9)

	subi	4,4,4
	subi	18,6,4
	subi	3,3,4
	lwz	7,0(7)
	li	0,0

	add	6,4,8
	lwz	9,4*1(4)

	lwz	10,4*2(4)
	li	23,0
	lwz	11,4*3(4)
	li	24,0
	lwz	12,4*4(4)
	li	25,0
	lwz	14,4*5(4)
	li	26,0
	lwz	15,4*6(4)
	li	27,0
	lwz	16,4*7(4)
	li	28,0
	lwzu	17,4*8(4)
	li	29,0

	addi	5,1,4*11
	subic.	30,8,4*8
	b	.Lsqr8x_zero_start

.align	5
.Lsqr8x_zero:
	subic.	30,30,4*8
	stw	0,4*1(5)
	stw	0,4*2(5)
	stw	0,4*3(5)
	stw	0,4*4(5)
	stw	0,4*5(5)
	stw	0,4*6(5)
	stw	0,4*7(5)
	stw	0,4*8(5)
.Lsqr8x_zero_start:
	stw	0,4*9(5)
	stw	0,4*10(5)
	stw	0,4*11(5)
	stw	0,4*12(5)
	stw	0,4*13(5)
	stw	0,4*14(5)
	stw	0,4*15(5)
	stwu	0,4*16(5)
	bne	.Lsqr8x_zero

	stw	3,4*6(1)
	stw	18,4*7(1)
	stw	7,4*8(1)
	stw	5,4*9(1)
	stw	0,4*10(1)
	addi	5,1,4*11


.align	5
.Lsqr8x_outer_loop:





























	mullw	18,10,9
	mullw	19,11,9
	mullw	20,12,9
	mullw	21,14,9
	addc	23,23,18
	mullw	18,15,9
	adde	24,24,19
	mullw	19,16,9
	adde	25,25,20
	mullw	20,17,9
	adde	26,26,21
	mulhwu	21,10,9
	adde	27,27,18
	mulhwu	18,11,9
	adde	28,28,19
	mulhwu	19,12,9
	adde	29,29,20
	mulhwu	20,14,9
	stw	22,4*1(5)
	addze	22,0
	stw	23,4*2(5)
	addc	24,24,21
	mulhwu	21,15,9
	adde	25,25,18
	mulhwu	18,16,9
	adde	26,26,19
	mulhwu	19,17,9
	adde	27,27,20
	mullw	20,11,10
	adde	28,28,21
	mullw	21,12,10
	adde	29,29,18
	mullw	18,14,10
	adde	22,22,19

	mullw	19,15,10
	addc	25,25,20
	mullw	20,16,10
	adde	26,26,21
	mullw	21,17,10
	adde	27,27,18
	mulhwu	18,11,10
	adde	28,28,19
	mulhwu	19,12,10
	adde	29,29,20
	mulhwu	20,14,10
	adde	22,22,21
	mulhwu	21,15,10
	stw	24,4*3(5)
	addze	23,0
	stw	25,4*4(5)
	addc	26,26,18
	mulhwu	18,16,10
	adde	27,27,19
	mulhwu	19,17,10
	adde	28,28,20
	mullw	20,12,11
	adde	29,29,21
	mullw	21,14,11
	adde	22,22,18
	mullw	18,15,11
	adde	23,23,19

	mullw	19,16,11
	addc	27,27,20
	mullw	20,17,11
	adde	28,28,21
	mulhwu	21,12,11
	adde	29,29,18
	mulhwu	18,14,11
	adde	22,22,19
	mulhwu	19,15,11
	adde	23,23,20
	mulhwu	20,16,11
	stw	26,4*5(5)
	addze	24,0
	stw	27,4*6(5)
	addc	28,28,21
	mulhwu	21,17,11
	adde	29,29,18
	mullw	18,14,12
	adde	22,22,19
	mullw	19,15,12
	adde	23,23,20
	mullw	20,16,12
	adde	24,24,21

	mullw	21,17,12
	addc	29,29,18
	mulhwu	18,14,12
	adde	22,22,19
	mulhwu	19,15,12
	adde	23,23,20
	mulhwu	20,16,12
	adde	24,24,21
	mulhwu	21,17,12
	stw	28,4*7(5)
	addze	25,0
	stwu	29,4*8(5)
	addc	22,22,18
	mullw	18,15,14
	adde	23,23,19
	mullw	19,16,14
	adde	24,24,20
	mullw	20,17,14
	adde	25,25,21

	mulhwu	21,15,14
	addc	23,23,18
	mulhwu	18,16,14
	adde	24,24,19
	mulhwu	19,17,14
	adde	25,25,20
	mullw	20,16,15
	addze	26,0
	addc	24,24,21
	mullw	21,17,15
	adde	25,25,18
	mulhwu	18,16,15
	adde	26,26,19

	mulhwu	19,17,15
	addc	25,25,20
	mullw	20,17,16
	adde	26,26,21
	mulhwu	21,17,16
	addze	27,0
	addc	26,26,18
	.long	0x7c062040
	adde	27,27,19

	addc	27,27,20
	sub	18,6,8
	addze	28,0
	add	28,28,21

	beq	.Lsqr8x_outer_break

	mr	7,9
	lwz	9,4*1(5)
	lwz	10,4*2(5)
	lwz	11,4*3(5)
	lwz	12,4*4(5)
	lwz	14,4*5(5)
	lwz	15,4*6(5)
	lwz	16,4*7(5)
	lwz	17,4*8(5)
	addc	22,22,9
	lwz	9,4*1(4)
	adde	23,23,10
	lwz	10,4*2(4)
	adde	24,24,11
	lwz	11,4*3(4)
	adde	25,25,12
	lwz	12,4*4(4)
	adde	26,26,14
	lwz	14,4*5(4)
	adde	27,27,15
	lwz	15,4*6(4)
	adde	28,28,16
	lwz	16,4*7(4)
	subi	3,4,4*7
	addze	29,17
	lwzu	17,4*8(4)

	li	30,0
	b	.Lsqr8x_mul























.align	5
.Lsqr8x_mul:
	mullw	18,9,7
	addze	31,0
	mullw	19,10,7
	addi	30,30,4
	mullw	20,11,7
	andi.	30,30,4*8-1
	mullw	21,12,7
	addc	22,22,18
	mullw	18,14,7
	adde	23,23,19
	mullw	19,15,7
	adde	24,24,20
	mullw	20,16,7
	adde	25,25,21
	mullw	21,17,7
	adde	26,26,18
	mulhwu	18,9,7
	adde	27,27,19
	mulhwu	19,10,7
	adde	28,28,20
	mulhwu	20,11,7
	adde	29,29,21
	mulhwu	21,12,7
	addze	31,31
	stwu	22,4(5)
	addc	22,23,18
	mulhwu	18,14,7
	adde	23,24,19
	mulhwu	19,15,7
	adde	24,25,20
	mulhwu	20,16,7
	adde	25,26,21
	mulhwu	21,17,7
	lwzx	7,3,30
	adde	26,27,18
	adde	27,28,19
	adde	28,29,20
	adde	29,31,21

	bne	.Lsqr8x_mul


	.long	0x7c043040
	beq	.Lsqr8x_break

	lwz	9,4*1(5)
	lwz	10,4*2(5)
	lwz	11,4*3(5)
	lwz	12,4*4(5)
	lwz	14,4*5(5)
	lwz	15,4*6(5)
	lwz	16,4*7(5)
	lwz	17,4*8(5)
	addc	22,22,9
	lwz	9,4*1(4)
	adde	23,23,10
	lwz	10,4*2(4)
	adde	24,24,11
	lwz	11,4*3(4)
	adde	25,25,12
	lwz	12,4*4(4)
	adde	26,26,14
	lwz	14,4*5(4)
	adde	27,27,15
	lwz	15,4*6(4)
	adde	28,28,16
	lwz	16,4*7(4)
	adde	29,29,17
	lwzu	17,4*8(4)

	b	.Lsqr8x_mul

.align	5
.Lsqr8x_break:
	lwz	9,4*8(3)
	addi	4,3,4*15
	lwz	10,4*9(3)
	sub.	18,6,4
	lwz	11,4*10(3)
	sub	19,5,18
	lwz	12,4*11(3)
	lwz	14,4*12(3)
	lwz	15,4*13(3)
	lwz	16,4*14(3)
	lwz	17,4*15(3)
	beq	.Lsqr8x_outer_loop

	stw	22,4*1(5)
	lwz	22,4*1(19)
	stw	23,4*2(5)
	lwz	23,4*2(19)
	stw	24,4*3(5)
	lwz	24,4*3(19)
	stw	25,4*4(5)
	lwz	25,4*4(19)
	stw	26,4*5(5)
	lwz	26,4*5(19)
	stw	27,4*6(5)
	lwz	27,4*6(19)
	stw	28,4*7(5)
	lwz	28,4*7(19)
	stw	29,4*8(5)
	lwz	29,4*8(19)
	mr	5,19
	b	.Lsqr8x_outer_loop

.align	5
.Lsqr8x_outer_break:


	lwz	10,4*1(18)
	lwz	12,4*2(18)
	lwz	15,4*3(18)
	lwz	17,4*4(18)
	addi	4,18,4*4

	lwz	19,4*13(1)
	lwz	20,4*14(1)
	lwz	21,4*15(1)
	lwz	18,4*16(1)

	stw	22,4*1(5)
	srwi	30,8,4
	stw	23,4*2(5)
	subi	30,30,1
	stw	24,4*3(5)
	stw	25,4*4(5)
	stw	26,4*5(5)
	stw	27,4*6(5)
	stw	28,4*7(5)

	addi	5,1,4*11
	mullw	22,10,10
	mulhwu	10,10,10
	add	23,19,19
	srwi	19,19,32-1
	mullw	11,12,12
	mulhwu	12,12,12
	addc	23,23,10
	add	24,20,20
	srwi	20,20,32-1
	add	25,21,21
	srwi	21,21,32-1
	or	24,24,19

	mtctr	30
.Lsqr4x_shift_n_add:
	mullw	14,15,15
	mulhwu	15,15,15
	lwz	19,4*6(5)
	lwz	10,4*1(4)
	adde	24,24,11
	add	26,18,18
	srwi	18,18,32-1
	or	25,25,20
	lwz	20,4*7(5)
	adde	25,25,12
	lwz	12,4*2(4)
	add	27,19,19
	srwi	19,19,32-1
	or	26,26,21
	lwz	21,4*8(5)
	mullw	16,17,17
	mulhwu	17,17,17
	adde	26,26,14
	add	28,20,20
	srwi	20,20,32-1
	or	27,27,18
	lwz	18,4*9(5)
	adde	27,27,15
	lwz	15,4*3(4)
	add	29,21,21
	srwi	21,21,32-1
	or	28,28,19
	lwz	19,4*10(5)
	mullw	9,10,10
	mulhwu	10,10,10
	adde	28,28,16
	stw	22,4*1(5)
	add	22,18,18
	srwi	18,18,32-1
	or	29,29,20
	lwz	20,4*11(5)
	adde	29,29,17
	lwzu	17,4*4(4)
	stw	23,4*2(5)
	add	23,19,19
	srwi	19,19,32-1
	or	22,22,21
	lwz	21,4*12(5)
	mullw	11,12,12
	mulhwu	12,12,12
	adde	22,22,9
	stw	24,4*3(5)
	add	24,20,20
	srwi	20,20,32-1
	or	23,23,18
	lwz	18,4*13(5)
	adde	23,23,10
	stw	25,4*4(5)
	stw	26,4*5(5)
	stw	27,4*6(5)
	stw	28,4*7(5)
	stwu	29,4*8(5)
	add	25,21,21
	srwi	21,21,32-1
	or	24,24,19
	bdnz	.Lsqr4x_shift_n_add
	lwz	4,4*7(1)
	lwz	7,4*8(1)

	mullw	14,15,15
	mulhwu	15,15,15
	stw	22,4*1(5)
	lwz	22,4*12(1)
	lwz	19,4*6(5)
	adde	24,24,11
	add	26,18,18
	srwi	18,18,32-1
	or	25,25,20
	lwz	20,4*7(5)
	adde	25,25,12
	add	27,19,19
	srwi	19,19,32-1
	or	26,26,21
	mullw	16,17,17
	mulhwu	17,17,17
	adde	26,26,14
	add	28,20,20
	srwi	20,20,32-1
	or	27,27,18
	stw	23,4*2(5)
	lwz	23,4*13(1)
	adde	27,27,15
	or	28,28,19
	lwz	9,4*1(4)
	lwz	10,4*2(4)
	adde	28,28,16
	lwz	11,4*3(4)
	lwz	12,4*4(4)
	adde	29,17,20
	lwz	14,4*5(4)
	lwz	15,4*6(4)



	mullw	31,7,22
	li	30,8
	lwz	16,4*7(4)
	add	6,4,8
	lwzu	17,4*8(4)
	stw	24,4*3(5)
	lwz	24,4*14(1)
	stw	25,4*4(5)
	lwz	25,4*15(1)
	stw	26,4*5(5)
	lwz	26,4*16(1)
	stw	27,4*6(5)
	lwz	27,4*17(1)
	stw	28,4*7(5)
	lwz	28,4*18(1)
	stw	29,4*8(5)
	lwz	29,4*19(1)
	addi	5,1,4*11
	mtctr	30
	b	.Lsqr8x_reduction

.align	5
.Lsqr8x_reduction:

	mullw	19,10,31
	mullw	20,11,31
	stwu	31,4(5)
	mullw	21,12,31

	addic	22,22,-1
	mullw	18,14,31
	adde	22,23,19
	mullw	19,15,31
	adde	23,24,20
	mullw	20,16,31
	adde	24,25,21
	mullw	21,17,31
	adde	25,26,18
	mulhwu	18,9,31
	adde	26,27,19
	mulhwu	19,10,31
	adde	27,28,20
	mulhwu	20,11,31
	adde	28,29,21
	mulhwu	21,12,31
	addze	29,0
	addc	22,22,18
	mulhwu	18,14,31
	adde	23,23,19
	mulhwu	19,15,31
	adde	24,24,20
	mulhwu	20,16,31
	adde	25,25,21
	mulhwu	21,17,31
	mullw	31,7,22
	adde	26,26,18
	adde	27,27,19
	adde	28,28,20
	adde	29,29,21
	bdnz	.Lsqr8x_reduction

	lwz	18,4*1(5)
	lwz	19,4*2(5)
	lwz	20,4*3(5)
	lwz	21,4*4(5)
	subi	3,5,4*7
	.long	0x7c062040
	addc	22,22,18
	lwz	18,4*5(5)
	adde	23,23,19
	lwz	19,4*6(5)
	adde	24,24,20
	lwz	20,4*7(5)
	adde	25,25,21
	lwz	21,4*8(5)
	adde	26,26,18
	adde	27,27,19
	adde	28,28,20
	adde	29,29,21

	beq	.Lsqr8x8_post_condition

	lwz	7,4*0(3)
	lwz	9,4*1(4)
	lwz	10,4*2(4)
	lwz	11,4*3(4)
	lwz	12,4*4(4)
	lwz	14,4*5(4)
	lwz	15,4*6(4)
	lwz	16,4*7(4)
	lwzu	17,4*8(4)
	li	30,0

.align	5
.Lsqr8x_tail:
	mullw	18,9,7
	addze	31,0
	mullw	19,10,7
	addi	30,30,4
	mullw	20,11,7
	andi.	30,30,4*8-1
	mullw	21,12,7
	addc	22,22,18
	mullw	18,14,7
	adde	23,23,19
	mullw	19,15,7
	adde	24,24,20
	mullw	20,16,7
	adde	25,25,21
	mullw	21,17,7
	adde	26,26,18
	mulhwu	18,9,7
	adde	27,27,19
	mulhwu	19,10,7
	adde	28,28,20
	mulhwu	20,11,7
	adde	29,29,21
	mulhwu	21,12,7
	addze	31,31
	stwu	22,4(5)
	addc	22,23,18
	mulhwu	18,14,7
	adde	23,24,19
	mulhwu	19,15,7
	adde	24,25,20
	mulhwu	20,16,7
	adde	25,26,21
	mulhwu	21,17,7
	lwzx	7,3,30
	adde	26,27,18
	adde	27,28,19
	adde	28,29,20
	adde	29,31,21

	bne	.Lsqr8x_tail


	lwz	9,4*1(5)
	lwz	31,4*10(1)
	.long	0x7c062040
	lwz	10,4*2(5)
	sub	20,6,8
	lwz	11,4*3(5)
	lwz	12,4*4(5)
	lwz	14,4*5(5)
	lwz	15,4*6(5)
	lwz	16,4*7(5)
	lwz	17,4*8(5)
	beq	.Lsqr8x_tail_break

	addc	22,22,9
	lwz	9,4*1(4)
	adde	23,23,10
	lwz	10,4*2(4)
	adde	24,24,11
	lwz	11,4*3(4)
	adde	25,25,12
	lwz	12,4*4(4)
	adde	26,26,14
	lwz	14,4*5(4)
	adde	27,27,15
	lwz	15,4*6(4)
	adde	28,28,16
	lwz	16,4*7(4)
	adde	29,29,17
	lwzu	17,4*8(4)

	b	.Lsqr8x_tail

.align	5
.Lsqr8x_tail_break:
	lwz	7,4*8(1)
	lwz	21,4*9(1)
	addi	30,5,4*8

	addic	31,31,-1
	adde	18,22,9
	lwz	22,4*8(3)
	lwz	9,4*1(20)
	adde	19,23,10
	lwz	23,4*9(3)
	lwz	10,4*2(20)
	adde	24,24,11
	lwz	11,4*3(20)
	adde	25,25,12
	lwz	12,4*4(20)
	adde	26,26,14
	lwz	14,4*5(20)
	adde	27,27,15
	lwz	15,4*6(20)
	adde	28,28,16
	lwz	16,4*7(20)
	adde	29,29,17
	lwz	17,4*8(20)
	addi	4,20,4*8
	addze	20,0
	mullw	31,7,22
	stw	18,4*1(5)
	.long	0x7c1ea840
	stw	19,4*2(5)
	li	30,8
	stw	24,4*3(5)
	lwz	24,4*10(3)
	stw	25,4*4(5)
	lwz	25,4*11(3)
	stw	26,4*5(5)
	lwz	26,4*12(3)
	stw	27,4*6(5)
	lwz	27,4*13(3)
	stw	28,4*7(5)
	lwz	28,4*14(3)
	stw	29,4*8(5)
	lwz	29,4*15(3)
	stw	20,4*10(1)
	addi	5,3,4*7
	mtctr	30
	bne	.Lsqr8x_reduction






	lwz	3,4*6(1)
	srwi	30,8,5
	mr	7,5
	addi	5,5,4*8
	subi	30,30,1
	subfc	18,9,22
	subfe	19,10,23
	mr	31,20
	mr	6,3

	mtctr	30
	b	.Lsqr8x_sub

.align	5
.Lsqr8x_sub:
	lwz	9,4*1(4)
	lwz	22,4*1(5)
	lwz	10,4*2(4)
	lwz	23,4*2(5)
	subfe	20,11,24
	lwz	11,4*3(4)
	lwz	24,4*3(5)
	subfe	21,12,25
	lwz	12,4*4(4)
	lwz	25,4*4(5)
	stw	18,4*1(3)
	subfe	18,14,26
	lwz	14,4*5(4)
	lwz	26,4*5(5)
	stw	19,4*2(3)
	subfe	19,15,27
	lwz	15,4*6(4)
	lwz	27,4*6(5)
	stw	20,4*3(3)
	subfe	20,16,28
	lwz	16,4*7(4)
	lwz	28,4*7(5)
	stw	21,4*4(3)
	subfe	21,17,29
	lwzu	17,4*8(4)
	lwzu	29,4*8(5)
	stw	18,4*5(3)
	subfe	18,9,22
	stw	19,4*6(3)
	subfe	19,10,23
	stw	20,4*7(3)
	stwu	21,4*8(3)
	bdnz	.Lsqr8x_sub

	srwi	30,8,4
	lwz	9,4*1(6)
	lwz	22,4*1(7)
	subi	30,30,1
	lwz	10,4*2(6)
	lwz	23,4*2(7)
	subfe	20,11,24
	lwz	11,4*3(6)
	lwz	24,4*3(7)
	subfe	21,12,25
	lwz	12,4*4(6)
	lwzu	25,4*4(7)
	stw	18,4*1(3)
	subfe	18,14,26
	stw	19,4*2(3)
	subfe	19,15,27
	stw	20,4*3(3)
	subfe	20,16,28
	stw	21,4*4(3)
	subfe	21,17,29
	stw	18,4*5(3)
	subfe	31,0,31
	stw	19,4*6(3)
	stw	20,4*7(3)
	stw	21,4*8(3)

	addi	5,1,4*11
	mtctr	30

.Lsqr4x_cond_copy:
	andc	9,9,31
	stw	0,-4*3(7)
	and	22,22,31
	stw	0,-4*2(7)
	andc	10,10,31
	stw	0,-4*1(7)
	and	23,23,31
	stw	0,-4*0(7)
	andc	11,11,31
	stw	0,4*1(5)
	and	24,24,31
	stw	0,4*2(5)
	andc	12,12,31
	stw	0,4*3(5)
	and	25,25,31
	stwu	0,4*4(5)
	or	18,9,22
	lwz	9,4*5(6)
	lwz	22,4*1(7)
	or	19,10,23
	lwz	10,4*6(6)
	lwz	23,4*2(7)
	or	20,11,24
	lwz	11,4*7(6)
	lwz	24,4*3(7)
	or	21,12,25
	lwz	12,4*8(6)
	lwzu	25,4*4(7)
	stw	18,4*1(6)
	stw	19,4*2(6)
	stw	20,4*3(6)
	stwu	21,4*4(6)
	bdnz	.Lsqr4x_cond_copy

	lwz	4,0(1)
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
	stw	18,4*1(6)
	stw	19,4*2(6)
	stw	20,4*3(6)
	stw	21,4*4(6)

	b	.Lsqr8x_done

.align	5
.Lsqr8x8_post_condition:
	lwz	3,4*6(1)
	lwz	4,0(1)
	addze	31,0


	subfc	22,9,22
	subfe	23,10,23
	stw	0,4*12(1)
	stw	0,4*13(1)
	subfe	24,11,24
	stw	0,4*14(1)
	stw	0,4*15(1)
	subfe	25,12,25
	stw	0,4*16(1)
	stw	0,4*17(1)
	subfe	26,14,26
	stw	0,4*18(1)
	stw	0,4*19(1)
	subfe	27,15,27
	stw	0,4*20(1)
	stw	0,4*21(1)
	subfe	28,16,28
	stw	0,4*22(1)
	stw	0,4*23(1)
	subfe	29,17,29
	stw	0,4*24(1)
	stw	0,4*25(1)
	subfe	31,0,31
	stw	0,4*26(1)
	stw	0,4*27(1)

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
	stw	22,4*1(3)
	stw	23,4*2(3)
	stw	24,4*3(3)
	stw	25,4*4(3)
	stw	26,4*5(3)
	stw	27,4*6(3)
	stw	28,4*7(3)
	stw	29,4*8(3)

.Lsqr8x_done:
	stw	0,4*8(1)
	stw	0,4*10(1)

	lwz	14,-4*18(4)
	li	3,1
	lwz	15,-4*17(4)
	lwz	16,-4*16(4)
	lwz	17,-4*15(4)
	lwz	18,-4*14(4)
	lwz	19,-4*13(4)
	lwz	20,-4*12(4)
	lwz	21,-4*11(4)
	lwz	22,-4*10(4)
	lwz	23,-4*9(4)
	lwz	24,-4*8(4)
	lwz	25,-4*7(4)
	lwz	26,-4*6(4)
	lwz	27,-4*5(4)
	lwz	28,-4*4(4)
	lwz	29,-4*3(4)
	lwz	30,-4*2(4)
	lwz	31,-4*1(4)
	mr	1,4
	blr	
.long	0
.byte	0,12,4,0x20,0x80,18,6,0
.long	0
.size	__bn_sqr8x_mont,.-__bn_sqr8x_mont
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
