.machine	"any"
.csect	.text[PR],7

.globl	.ChaCha20_ctr32_int
.align	5
.ChaCha20_ctr32_int:
__ChaCha20_ctr32_int:
	cmplwi	5,0
	bclr	14,2

	stwu	1,-160(1)
	mflr	0

	stw	14,88(1)
	stw	15,92(1)
	stw	16,96(1)
	stw	17,100(1)
	stw	18,104(1)
	stw	19,108(1)
	stw	20,112(1)
	stw	21,116(1)
	stw	22,120(1)
	stw	23,124(1)
	stw	24,128(1)
	stw	25,132(1)
	stw	26,136(1)
	stw	27,140(1)
	stw	28,144(1)
	stw	29,148(1)
	stw	30,152(1)
	stw	31,156(1)
	stw	0,164(1)

	lwz	11,0(7)
	lwz	12,4(7)
	lwz	14,8(7)
	lwz	15,12(7)

	bl	__ChaCha20_1x

	lwz	0,164(1)
	lwz	14,88(1)
	lwz	15,92(1)
	lwz	16,96(1)
	lwz	17,100(1)
	lwz	18,104(1)
	lwz	19,108(1)
	lwz	20,112(1)
	lwz	21,116(1)
	lwz	22,120(1)
	lwz	23,124(1)
	lwz	24,128(1)
	lwz	25,132(1)
	lwz	26,136(1)
	lwz	27,140(1)
	lwz	28,144(1)
	lwz	29,148(1)
	lwz	30,152(1)
	lwz	31,156(1)
	mtlr	0
	addi	1,1,160
	blr	
.long	0
.byte	0,12,4,1,0x80,18,5,0
.long	0


.align	5
__ChaCha20_1x:
Loop_outer:
	lis	16,0x6170
	lis	17,0x3320
	lis	18,0x7962
	lis	19,0x6b20
	ori	16,16,0x7865
	ori	17,17,0x646e
	ori	18,18,0x2d32
	ori	19,19,0x6574

	li	0,10
	lwz	20,0(6)
	lwz	21,4(6)
	lwz	22,8(6)
	lwz	23,12(6)
	lwz	24,16(6)
	mr	28,11
	lwz	25,20(6)
	mr	29,12
	lwz	26,24(6)
	mr	30,14
	lwz	27,28(6)
	mr	31,15

	mr	7,20
	mr	8,21
	mr	9,22
	mr	10,23

	mtctr	0
Loop:
	add	16,16,20
	add	17,17,21
	add	18,18,22
	add	19,19,23
	xor	28,28,16
	xor	29,29,17
	xor	30,30,18
	xor	31,31,19
	rotlwi	28,28,16
	rotlwi	29,29,16
	rotlwi	30,30,16
	rotlwi	31,31,16
	add	24,24,28
	add	25,25,29
	add	26,26,30
	add	27,27,31
	xor	20,20,24
	xor	21,21,25
	xor	22,22,26
	xor	23,23,27
	rotlwi	20,20,12
	rotlwi	21,21,12
	rotlwi	22,22,12
	rotlwi	23,23,12
	add	16,16,20
	add	17,17,21
	add	18,18,22
	add	19,19,23
	xor	28,28,16
	xor	29,29,17
	xor	30,30,18
	xor	31,31,19
	rotlwi	28,28,8
	rotlwi	29,29,8
	rotlwi	30,30,8
	rotlwi	31,31,8
	add	24,24,28
	add	25,25,29
	add	26,26,30
	add	27,27,31
	xor	20,20,24
	xor	21,21,25
	xor	22,22,26
	xor	23,23,27
	rotlwi	20,20,7
	rotlwi	21,21,7
	rotlwi	22,22,7
	rotlwi	23,23,7
	add	16,16,21
	add	17,17,22
	add	18,18,23
	add	19,19,20
	xor	31,31,16
	xor	28,28,17
	xor	29,29,18
	xor	30,30,19
	rotlwi	31,31,16
	rotlwi	28,28,16
	rotlwi	29,29,16
	rotlwi	30,30,16
	add	26,26,31
	add	27,27,28
	add	24,24,29
	add	25,25,30
	xor	21,21,26
	xor	22,22,27
	xor	23,23,24
	xor	20,20,25
	rotlwi	21,21,12
	rotlwi	22,22,12
	rotlwi	23,23,12
	rotlwi	20,20,12
	add	16,16,21
	add	17,17,22
	add	18,18,23
	add	19,19,20
	xor	31,31,16
	xor	28,28,17
	xor	29,29,18
	xor	30,30,19
	rotlwi	31,31,8
	rotlwi	28,28,8
	rotlwi	29,29,8
	rotlwi	30,30,8
	add	26,26,31
	add	27,27,28
	add	24,24,29
	add	25,25,30
	xor	21,21,26
	xor	22,22,27
	xor	23,23,24
	xor	20,20,25
	rotlwi	21,21,7
	rotlwi	22,22,7
	rotlwi	23,23,7
	rotlwi	20,20,7
	bc	16,0,Loop

	subic	5,5,64
	addi	16,16,0x7865
	addi	17,17,0x646e
	addi	18,18,0x2d32
	addi	19,19,0x6574
	addis	16,16,0x6170
	addis	17,17,0x3320
	addis	18,18,0x7962
	addis	19,19,0x6b20

	subfe.	0,0,0
	add	20,20,7
	lwz	7,16(6)
	add	21,21,8
	lwz	8,20(6)
	add	22,22,9
	lwz	9,24(6)
	add	23,23,10
	lwz	10,28(6)
	add	24,24,7
	add	25,25,8
	add	26,26,9
	add	27,27,10

	add	28,28,11
	add	29,29,12
	add	30,30,14
	add	31,31,15
	addi	11,11,1
	mr	7,16
	rotlwi	16,16,8
	rlwimi	16,7,24,0,7
	rlwimi	16,7,24,16,23
	mr	8,17
	rotlwi	17,17,8
	rlwimi	17,8,24,0,7
	rlwimi	17,8,24,16,23
	mr	9,18
	rotlwi	18,18,8
	rlwimi	18,9,24,0,7
	rlwimi	18,9,24,16,23
	mr	10,19
	rotlwi	19,19,8
	rlwimi	19,10,24,0,7
	rlwimi	19,10,24,16,23
	mr	7,20
	rotlwi	20,20,8
	rlwimi	20,7,24,0,7
	rlwimi	20,7,24,16,23
	mr	8,21
	rotlwi	21,21,8
	rlwimi	21,8,24,0,7
	rlwimi	21,8,24,16,23
	mr	9,22
	rotlwi	22,22,8
	rlwimi	22,9,24,0,7
	rlwimi	22,9,24,16,23
	mr	10,23
	rotlwi	23,23,8
	rlwimi	23,10,24,0,7
	rlwimi	23,10,24,16,23
	mr	7,24
	rotlwi	24,24,8
	rlwimi	24,7,24,0,7
	rlwimi	24,7,24,16,23
	mr	8,25
	rotlwi	25,25,8
	rlwimi	25,8,24,0,7
	rlwimi	25,8,24,16,23
	mr	9,26
	rotlwi	26,26,8
	rlwimi	26,9,24,0,7
	rlwimi	26,9,24,16,23
	mr	10,27
	rotlwi	27,27,8
	rlwimi	27,10,24,0,7
	rlwimi	27,10,24,16,23
	mr	7,28
	rotlwi	28,28,8
	rlwimi	28,7,24,0,7
	rlwimi	28,7,24,16,23
	mr	8,29
	rotlwi	29,29,8
	rlwimi	29,8,24,0,7
	rlwimi	29,8,24,16,23
	mr	9,30
	rotlwi	30,30,8
	rlwimi	30,9,24,0,7
	rlwimi	30,9,24,16,23
	mr	10,31
	rotlwi	31,31,8
	rlwimi	31,10,24,0,7
	rlwimi	31,10,24,16,23
	bne	Ltail

	lwz	7,0(4)
	lwz	8,4(4)
	cmplwi	5,0
	lwz	9,8(4)
	lwz	10,12(4)
	xor	16,16,7
	lwz	7,16(4)
	xor	17,17,8
	lwz	8,20(4)
	xor	18,18,9
	lwz	9,24(4)
	xor	19,19,10
	lwz	10,28(4)
	xor	20,20,7
	lwz	7,32(4)
	xor	21,21,8
	lwz	8,36(4)
	xor	22,22,9
	lwz	9,40(4)
	xor	23,23,10
	lwz	10,44(4)
	xor	24,24,7
	lwz	7,48(4)
	xor	25,25,8
	lwz	8,52(4)
	xor	26,26,9
	lwz	9,56(4)
	xor	27,27,10
	lwz	10,60(4)
	xor	28,28,7
	stw	16,0(3)
	xor	29,29,8
	stw	17,4(3)
	xor	30,30,9
	stw	18,8(3)
	xor	31,31,10
	stw	19,12(3)
	stw	20,16(3)
	stw	21,20(3)
	stw	22,24(3)
	stw	23,28(3)
	stw	24,32(3)
	stw	25,36(3)
	stw	26,40(3)
	stw	27,44(3)
	stw	28,48(3)
	stw	29,52(3)
	stw	30,56(3)
	addi	4,4,64
	stw	31,60(3)
	addi	3,3,64

	bne	Loop_outer

	blr	

.align	4
Ltail:
	addi	5,5,64
	subi	4,4,1
	subi	3,3,1
	addi	7,1,24-1
	mtctr	5

	stw	16,24(1)
	stw	17,28(1)
	stw	18,32(1)
	stw	19,36(1)
	stw	20,40(1)
	stw	21,44(1)
	stw	22,48(1)
	stw	23,52(1)
	stw	24,56(1)
	stw	25,60(1)
	stw	26,64(1)
	stw	27,68(1)
	stw	28,72(1)
	stw	29,76(1)
	stw	30,80(1)
	stw	31,84(1)

Loop_tail:
	lbzu	11,1(4)
	lbzu	16,1(7)
	xor	12,11,16
	stbu	12,1(3)
	bc	16,0,Loop_tail

	stw	1,24(1)
	stw	1,28(1)
	stw	1,32(1)
	stw	1,36(1)
	stw	1,40(1)
	stw	1,44(1)
	stw	1,48(1)
	stw	1,52(1)
	stw	1,56(1)
	stw	1,60(1)
	stw	1,64(1)
	stw	1,68(1)
	stw	1,72(1)
	stw	1,76(1)
	stw	1,80(1)
	stw	1,84(1)

	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.globl	.ChaCha20_ctr32_vmx
.align	5
.ChaCha20_ctr32_vmx:
	cmplwi	5,256
	blt	__ChaCha20_ctr32_int

	stwu	1,-320(1)
	mflr	0
	li	10,103
	li	11,119
	li	12,-1
	stvx	23,10,1
	addi	10,10,32
	stvx	24,11,1
	addi	11,11,32
	stvx	25,10,1
	addi	10,10,32
	stvx	26,11,1
	addi	11,11,32
	stvx	27,10,1
	addi	10,10,32
	stvx	28,11,1
	addi	11,11,32
	stvx	29,10,1
	addi	10,10,32
	stvx	30,11,1
	stvx	31,10,1
	stw	12,244(1)
	stw	14,248(1)
	stw	15,252(1)
	stw	16,256(1)
	stw	17,260(1)
	stw	18,264(1)
	stw	19,268(1)
	stw	20,272(1)
	stw	21,276(1)
	stw	22,280(1)
	stw	23,284(1)
	stw	24,288(1)
	stw	25,292(1)
	stw	26,296(1)
	stw	27,300(1)
	stw	28,304(1)
	stw	29,308(1)
	stw	30,312(1)
	stw	31,316(1)
	li	12,-4096+511
	stw	0, 324(1)
	or	12,12,12

	bl	Lconsts
	li	16,16
	li	17,32
	li	18,48
	li	19,64
	li	20,31
	li	21,15

	lvx	13,0,6
	lvsl	29,0,6
	lvx	14,16,6
	lvx	27,20,6

	lvx	15,0,7
	lvsl	30,0,7
	lvx	28,21,7

	lvx	12,0,12
	lvx	17,16,12
	lvx	18,17,12
	lvx	19,18,12
	lvx	23,19,12

	vperm	13,13,14,29
	vperm	14,14,27,29
	vperm	15,15,28,30

	lwz	11,0(7)
	lwz	12,4(7)
	vadduwm	15,15,17
	lwz	14,8(7)
	vadduwm	16,15,17
	lwz	15,12(7)
	vadduwm	17,16,17

	vxor	29,29,29
	vspltisw	26,-1
	lvsl	24,0,4
	lvsr	25,0,3
	vperm	26,29,26,25

	lvsl	29,0,16
	vspltisb	30,3
	vxor	29,29,30
	vxor	25,25,30
	vperm	24,24,24,29

	li	0,10
	b	Loop_outer_vmx

.align	4
Loop_outer_vmx:
	lis	16,0x6170
	lis	17,0x3320
	vor	0,12,12
	lis	18,0x7962
	lis	19,0x6b20
	vor	4,12,12
	ori	16,16,0x7865
	ori	17,17,0x646e
	vor	8,12,12
	ori	18,18,0x2d32
	ori	19,19,0x6574
	vor	1,13,13

	lwz	20,0(6)
	vor	5,13,13
	lwz	21,4(6)
	vor	9,13,13
	lwz	22,8(6)
	vor	2,14,14
	lwz	23,12(6)
	vor	6,14,14
	lwz	24,16(6)
	vor	10,14,14
	mr	28,11
	lwz	25,20(6)
	vor	3,15,15
	mr	29,12
	lwz	26,24(6)
	vor	7,16,16
	mr	30,14
	lwz	27,28(6)
	vor	11,17,17
	mr	31,15

	mr	7,20
	mr	8,21
	mr	9,22
	mr	10,23

	vspltisw	27,12
	vspltisw	28,7

	mtctr	0
	nop	
Loop_vmx:
	vadduwm	0,0,1
	vadduwm	4,4,5
	vadduwm	8,8,9
	add	16,16,20
	add	17,17,21
	add	18,18,22
	vxor	3,3,0
	vxor	7,7,4
	vxor	11,11,8
	add	19,19,23
	xor	28,28,16
	xor	29,29,17
	vperm	3,3,3,19
	vperm	7,7,7,19
	vperm	11,11,11,19
	xor	30,30,18
	xor	31,31,19
	rotlwi	28,28,16
	vadduwm	2,2,3
	vadduwm	6,6,7
	vadduwm	10,10,11
	rotlwi	29,29,16
	rotlwi	30,30,16
	rotlwi	31,31,16
	vxor	1,1,2
	vxor	5,5,6
	vxor	9,9,10
	add	24,24,28
	add	25,25,29
	add	26,26,30
	vrlw	1,1,27
	vrlw	5,5,27
	vrlw	9,9,27
	add	27,27,31
	xor	20,20,24
	xor	21,21,25
	vadduwm	0,0,1
	vadduwm	4,4,5
	vadduwm	8,8,9
	xor	22,22,26
	xor	23,23,27
	rotlwi	20,20,12
	vxor	3,3,0
	vxor	7,7,4
	vxor	11,11,8
	rotlwi	21,21,12
	rotlwi	22,22,12
	rotlwi	23,23,12
	vperm	3,3,3,23
	vperm	7,7,7,23
	vperm	11,11,11,23
	add	16,16,20
	add	17,17,21
	add	18,18,22
	vadduwm	2,2,3
	vadduwm	6,6,7
	vadduwm	10,10,11
	add	19,19,23
	xor	28,28,16
	xor	29,29,17
	vxor	1,1,2
	vxor	5,5,6
	vxor	9,9,10
	xor	30,30,18
	xor	31,31,19
	rotlwi	28,28,8
	vrlw	1,1,28
	vrlw	5,5,28
	vrlw	9,9,28
	rotlwi	29,29,8
	rotlwi	30,30,8
	rotlwi	31,31,8
	vsldoi	2,2,2, 16-8
	vsldoi	6,6,6, 16-8
	vsldoi	10,10,10, 16-8
	add	24,24,28
	add	25,25,29
	add	26,26,30
	vsldoi	1,1,1, 16-12
	vsldoi	5,5,5, 16-12
	vsldoi	9,9,9, 16-12
	add	27,27,31
	xor	20,20,24
	xor	21,21,25
	vsldoi	3,3,3, 16-4
	vsldoi	7,7,7, 16-4
	vsldoi	11,11,11, 16-4
	xor	22,22,26
	xor	23,23,27
	rotlwi	20,20,7
	rotlwi	21,21,7
	rotlwi	22,22,7
	rotlwi	23,23,7
	vadduwm	0,0,1
	vadduwm	4,4,5
	vadduwm	8,8,9
	add	16,16,21
	add	17,17,22
	add	18,18,23
	vxor	3,3,0
	vxor	7,7,4
	vxor	11,11,8
	add	19,19,20
	xor	31,31,16
	xor	28,28,17
	vperm	3,3,3,19
	vperm	7,7,7,19
	vperm	11,11,11,19
	xor	29,29,18
	xor	30,30,19
	rotlwi	31,31,16
	vadduwm	2,2,3
	vadduwm	6,6,7
	vadduwm	10,10,11
	rotlwi	28,28,16
	rotlwi	29,29,16
	rotlwi	30,30,16
	vxor	1,1,2
	vxor	5,5,6
	vxor	9,9,10
	add	26,26,31
	add	27,27,28
	add	24,24,29
	vrlw	1,1,27
	vrlw	5,5,27
	vrlw	9,9,27
	add	25,25,30
	xor	21,21,26
	xor	22,22,27
	vadduwm	0,0,1
	vadduwm	4,4,5
	vadduwm	8,8,9
	xor	23,23,24
	xor	20,20,25
	rotlwi	21,21,12
	vxor	3,3,0
	vxor	7,7,4
	vxor	11,11,8
	rotlwi	22,22,12
	rotlwi	23,23,12
	rotlwi	20,20,12
	vperm	3,3,3,23
	vperm	7,7,7,23
	vperm	11,11,11,23
	add	16,16,21
	add	17,17,22
	add	18,18,23
	vadduwm	2,2,3
	vadduwm	6,6,7
	vadduwm	10,10,11
	add	19,19,20
	xor	31,31,16
	xor	28,28,17
	vxor	1,1,2
	vxor	5,5,6
	vxor	9,9,10
	xor	29,29,18
	xor	30,30,19
	rotlwi	31,31,8
	vrlw	1,1,28
	vrlw	5,5,28
	vrlw	9,9,28
	rotlwi	28,28,8
	rotlwi	29,29,8
	rotlwi	30,30,8
	vsldoi	2,2,2, 16-8
	vsldoi	6,6,6, 16-8
	vsldoi	10,10,10, 16-8
	add	26,26,31
	add	27,27,28
	add	24,24,29
	vsldoi	1,1,1, 16-4
	vsldoi	5,5,5, 16-4
	vsldoi	9,9,9, 16-4
	add	25,25,30
	xor	21,21,26
	xor	22,22,27
	vsldoi	3,3,3, 16-12
	vsldoi	7,7,7, 16-12
	vsldoi	11,11,11, 16-12
	xor	23,23,24
	xor	20,20,25
	rotlwi	21,21,7
	rotlwi	22,22,7
	rotlwi	23,23,7
	rotlwi	20,20,7
	bc	16,0,Loop_vmx

	subi	5,5,256
	addi	16,16,0x7865
	addi	17,17,0x646e
	addi	18,18,0x2d32
	addi	19,19,0x6574
	addis	16,16,0x6170
	addis	17,17,0x3320
	addis	18,18,0x7962
	addis	19,19,0x6b20
	add	20,20,7
	lwz	7,16(6)
	add	21,21,8
	lwz	8,20(6)
	add	22,22,9
	lwz	9,24(6)
	add	23,23,10
	lwz	10,28(6)
	add	24,24,7
	add	25,25,8
	add	26,26,9
	add	27,27,10
	add	28,28,11
	add	29,29,12
	add	30,30,14
	add	31,31,15

	vadduwm	0,0,12
	vadduwm	4,4,12
	vadduwm	8,8,12
	vadduwm	1,1,13
	vadduwm	5,5,13
	vadduwm	9,9,13
	vadduwm	2,2,14
	vadduwm	6,6,14
	vadduwm	10,10,14
	vadduwm	3,3,15
	vadduwm	7,7,16
	vadduwm	11,11,17

	addi	11,11,4
	vadduwm	15,15,18
	vadduwm	16,16,18
	vadduwm	17,17,18

	mr	7,16
	rotlwi	16,16,8
	rlwimi	16,7,24,0,7
	rlwimi	16,7,24,16,23
	mr	8,17
	rotlwi	17,17,8
	rlwimi	17,8,24,0,7
	rlwimi	17,8,24,16,23
	mr	9,18
	rotlwi	18,18,8
	rlwimi	18,9,24,0,7
	rlwimi	18,9,24,16,23
	mr	10,19
	rotlwi	19,19,8
	rlwimi	19,10,24,0,7
	rlwimi	19,10,24,16,23
	mr	7,20
	rotlwi	20,20,8
	rlwimi	20,7,24,0,7
	rlwimi	20,7,24,16,23
	mr	8,21
	rotlwi	21,21,8
	rlwimi	21,8,24,0,7
	rlwimi	21,8,24,16,23
	mr	9,22
	rotlwi	22,22,8
	rlwimi	22,9,24,0,7
	rlwimi	22,9,24,16,23
	mr	10,23
	rotlwi	23,23,8
	rlwimi	23,10,24,0,7
	rlwimi	23,10,24,16,23
	mr	7,24
	rotlwi	24,24,8
	rlwimi	24,7,24,0,7
	rlwimi	24,7,24,16,23
	mr	8,25
	rotlwi	25,25,8
	rlwimi	25,8,24,0,7
	rlwimi	25,8,24,16,23
	mr	9,26
	rotlwi	26,26,8
	rlwimi	26,9,24,0,7
	rlwimi	26,9,24,16,23
	mr	10,27
	rotlwi	27,27,8
	rlwimi	27,10,24,0,7
	rlwimi	27,10,24,16,23
	mr	7,28
	rotlwi	28,28,8
	rlwimi	28,7,24,0,7
	rlwimi	28,7,24,16,23
	mr	8,29
	rotlwi	29,29,8
	rlwimi	29,8,24,0,7
	rlwimi	29,8,24,16,23
	mr	9,30
	rotlwi	30,30,8
	rlwimi	30,9,24,0,7
	rlwimi	30,9,24,16,23
	mr	10,31
	rotlwi	31,31,8
	rlwimi	31,10,24,0,7
	rlwimi	31,10,24,16,23
	lwz	7,0(4)
	lwz	8,4(4)
	lwz	9,8(4)
	lwz	10,12(4)
	xor	16,16,7
	lwz	7,16(4)
	xor	17,17,8
	lwz	8,20(4)
	xor	18,18,9
	lwz	9,24(4)
	xor	19,19,10
	lwz	10,28(4)
	xor	20,20,7
	lwz	7,32(4)
	xor	21,21,8
	lwz	8,36(4)
	xor	22,22,9
	lwz	9,40(4)
	xor	23,23,10
	lwz	10,44(4)
	xor	24,24,7
	lwz	7,48(4)
	xor	25,25,8
	lwz	8,52(4)
	xor	26,26,9
	lwz	9,56(4)
	xor	27,27,10
	lwz	10,60(4)
	xor	28,28,7
	stw	16,0(3)
	xor	29,29,8
	stw	17,4(3)
	xor	30,30,9
	stw	18,8(3)
	xor	31,31,10
	stw	19,12(3)
	addi	4,4,64
	stw	20,16(3)
	li	7,16
	stw	21,20(3)
	li	8,32
	stw	22,24(3)
	li	9,48
	stw	23,28(3)
	li	10,64
	stw	24,32(3)
	stw	25,36(3)
	stw	26,40(3)
	stw	27,44(3)
	stw	28,48(3)
	stw	29,52(3)
	stw	30,56(3)
	stw	31,60(3)
	addi	3,3,64

	lvx	27,0,4
	lvx	28,7,4
	lvx	29,8,4
	lvx	30,9,4
	lvx	31,10,4
	addi	4,4,64

	vperm	27,27,28,24
	vperm	28,28,29,24
	vperm	29,29,30,24
	vperm	30,30,31,24
	vxor	0,0,27
	vxor	1,1,28
	lvx	28,7,4
	vxor	2,2,29
	lvx	29,8,4
	vxor	3,3,30
	lvx	30,9,4
	lvx	27,10,4
	addi	4,4,64
	li	10,63
	vperm	0,0,0,25
	vperm	1,1,1,25
	vperm	2,2,2,25
	vperm	3,3,3,25

	vperm	31,31,28,24
	vperm	28,28,29,24
	vperm	29,29,30,24
	vperm	30,30,27,24
	vxor	4,4,31
	vxor	5,5,28
	lvx	28,7,4
	vxor	6,6,29
	lvx	29,8,4
	vxor	7,7,30
	lvx	30,9,4
	lvx	31,10,4
	addi	4,4,64
	vperm	4,4,4,25
	vperm	5,5,5,25
	vperm	6,6,6,25
	vperm	7,7,7,25

	vperm	27,27,28,24
	vperm	28,28,29,24
	vperm	29,29,30,24
	vperm	30,30,31,24
	vxor	8,8,27
	vxor	9,9,28
	vxor	10,10,29
	vxor	11,11,30
	vperm	8,8,8,25
	vperm	9,9,9,25
	vperm	10,10,10,25
	vperm	11,11,11,25

	andi.	17,3,15
	mr	16,3

	vsel	27,0,1,26
	vsel	28,1,2,26
	vsel	29,2,3,26
	vsel	30,3,4,26
	vsel	1,4,5,26
	vsel	2,5,6,26
	vsel	3,6,7,26
	vsel	4,7,8,26
	vsel	5,8,9,26
	vsel	6,9,10,26
	vsel	7,10,11,26


	stvx	27,7,3
	stvx	28,8,3
	stvx	29,9,3
	addi	3,3,64
	stvx	30,0,3
	stvx	1,7,3
	stvx	2,8,3
	stvx	3,9,3
	addi	3,3,64
	stvx	4,0,3
	stvx	5,7,3
	stvx	6,8,3
	stvx	7,9,3
	addi	3,3,64

	beq	Laligned_vmx

	sub	18,3,17
	li	19,0
Lunaligned_tail_vmx:
	stvebx	11,19,18
	addi	19,19,1
	cmpw	19,17
	bne	Lunaligned_tail_vmx

	sub	18,16,17
Lunaligned_head_vmx:
	stvebx	0,17,18
	cmpwi	17,15
	addi	17,17,1
	bne	Lunaligned_head_vmx

	cmplwi	5,255
	bgt	Loop_outer_vmx

	b	Ldone_vmx

.align	4
Laligned_vmx:
	stvx	0,0,16

	cmplwi	5,255
	bgt	Loop_outer_vmx
	nop	

Ldone_vmx:
	cmplwi	5,0
	bnel	__ChaCha20_1x

	lwz	12,244(1)
	li	10,103
	li	11,119
	or	12,12,12
	lvx	23,10,1
	addi	10,10,32
	lvx	24,11,1
	addi	11,11,32
	lvx	25,10,1
	addi	10,10,32
	lvx	26,11,1
	addi	11,11,32
	lvx	27,10,1
	addi	10,10,32
	lvx	28,11,1
	addi	11,11,32
	lvx	29,10,1
	addi	10,10,32
	lvx	30,11,1
	lvx	31,10,1
	lwz	0, 324(1)
	lwz	14,248(1)
	lwz	15,252(1)
	lwz	16,256(1)
	lwz	17,260(1)
	lwz	18,264(1)
	lwz	19,268(1)
	lwz	20,272(1)
	lwz	21,276(1)
	lwz	22,280(1)
	lwz	23,284(1)
	lwz	24,288(1)
	lwz	25,292(1)
	lwz	26,296(1)
	lwz	27,300(1)
	lwz	28,304(1)
	lwz	29,308(1)
	lwz	30,312(1)
	lwz	31,316(1)
	mtlr	0
	addi	1,1,320
	blr	
.long	0
.byte	0,12,0x04,1,0x80,18,5,0
.long	0


.globl	.ChaCha20_ctr32_vsx
.align	5
.ChaCha20_ctr32_vsx:
	stwu	1,-200(1)
	mflr	0
	li	10,103
	li	11,119
	li	12,-1
	stvx	26,10,1
	addi	10,10,32
	stvx	27,11,1
	addi	11,11,32
	stvx	28,10,1
	addi	10,10,32
	stvx	29,11,1
	addi	11,11,32
	stvx	30,10,1
	stvx	31,11,1
	stw	12,196(1)
	li	12,-4096+63
	stw	0, 204(1)
	or	12,12,12

	bl	Lconsts
	.long	0x7E006619
	addi	12,12,0x50
	li	8,16
	li	9,32
	li	10,48
	li	11,64

	.long	0x7E203619
	.long	0x7E483619
	.long	0x7E603E19

	vxor	27,27,27
	.long	0x7F8B6619
	vspltw	26,19,0
	vsldoi	19,19,27,4
	vsldoi	19,27,19,12
	vadduwm	26,26,28

	lvsl	31,0,8
	vspltisb	27,3
	vxor	31,31,27

	li	0,10
	mtctr	0
	b	Loop_outer_vsx

.align	5
Loop_outer_vsx:
	lvx	0,0,12
	lvx	1,8,12
	lvx	2,9,12
	lvx	3,10,12

	vspltw	4,17,0
	vspltw	5,17,1
	vspltw	6,17,2
	vspltw	7,17,3

	vspltw	8,18,0
	vspltw	9,18,1
	vspltw	10,18,2
	vspltw	11,18,3

	vor	12,26,26
	vspltw	13,19,1
	vspltw	14,19,2
	vspltw	15,19,3

	vspltisw	27,-16
	vspltisw	28,12
	vspltisw	29,8
	vspltisw	30,7

Loop_vsx:
	vadduwm	0,0,4
	vadduwm	1,1,5
	vadduwm	2,2,6
	vadduwm	3,3,7
	vxor	12,12,0
	vxor	13,13,1
	vxor	14,14,2
	vxor	15,15,3
	vrlw	12,12,27
	vrlw	13,13,27
	vrlw	14,14,27
	vrlw	15,15,27
	vadduwm	8,8,12
	vadduwm	9,9,13
	vadduwm	10,10,14
	vadduwm	11,11,15
	vxor	4,4,8
	vxor	5,5,9
	vxor	6,6,10
	vxor	7,7,11
	vrlw	4,4,28
	vrlw	5,5,28
	vrlw	6,6,28
	vrlw	7,7,28
	vadduwm	0,0,4
	vadduwm	1,1,5
	vadduwm	2,2,6
	vadduwm	3,3,7
	vxor	12,12,0
	vxor	13,13,1
	vxor	14,14,2
	vxor	15,15,3
	vrlw	12,12,29
	vrlw	13,13,29
	vrlw	14,14,29
	vrlw	15,15,29
	vadduwm	8,8,12
	vadduwm	9,9,13
	vadduwm	10,10,14
	vadduwm	11,11,15
	vxor	4,4,8
	vxor	5,5,9
	vxor	6,6,10
	vxor	7,7,11
	vrlw	4,4,30
	vrlw	5,5,30
	vrlw	6,6,30
	vrlw	7,7,30
	vadduwm	0,0,5
	vadduwm	1,1,6
	vadduwm	2,2,7
	vadduwm	3,3,4
	vxor	15,15,0
	vxor	12,12,1
	vxor	13,13,2
	vxor	14,14,3
	vrlw	15,15,27
	vrlw	12,12,27
	vrlw	13,13,27
	vrlw	14,14,27
	vadduwm	10,10,15
	vadduwm	11,11,12
	vadduwm	8,8,13
	vadduwm	9,9,14
	vxor	5,5,10
	vxor	6,6,11
	vxor	7,7,8
	vxor	4,4,9
	vrlw	5,5,28
	vrlw	6,6,28
	vrlw	7,7,28
	vrlw	4,4,28
	vadduwm	0,0,5
	vadduwm	1,1,6
	vadduwm	2,2,7
	vadduwm	3,3,4
	vxor	15,15,0
	vxor	12,12,1
	vxor	13,13,2
	vxor	14,14,3
	vrlw	15,15,29
	vrlw	12,12,29
	vrlw	13,13,29
	vrlw	14,14,29
	vadduwm	10,10,15
	vadduwm	11,11,12
	vadduwm	8,8,13
	vadduwm	9,9,14
	vxor	5,5,10
	vxor	6,6,11
	vxor	7,7,8
	vxor	4,4,9
	vrlw	5,5,30
	vrlw	6,6,30
	vrlw	7,7,30
	vrlw	4,4,30
	bc	16,0,Loop_vsx

	vadduwm	12,12,26

	.long	0x13600F8C
	.long	0x13821F8C
	.long	0x10000E8C
	.long	0x10421E8C
	.long	0x13A42F8C
	.long	0x13C63F8C
	.long	0xF0201057
	.long	0xF0601357
	.long	0xF01BE057
	.long	0xF05BE357

	.long	0x10842E8C
	.long	0x10C63E8C
	.long	0x13684F8C
	.long	0x138A5F8C
	.long	0xF0A43057
	.long	0xF0E43357
	.long	0xF09DF057
	.long	0xF0DDF357

	.long	0x11084E8C
	.long	0x114A5E8C
	.long	0x13AC6F8C
	.long	0x13CE7F8C
	.long	0xF1285057
	.long	0xF1685357
	.long	0xF11BE057
	.long	0xF15BE357

	.long	0x118C6E8C
	.long	0x11CE7E8C
	vspltisw	27,4
	vadduwm	26,26,27
	.long	0xF1AC7057
	.long	0xF1EC7357
	.long	0xF19DF057
	.long	0xF1DDF357

	vadduwm	0,0,16
	vadduwm	4,4,17
	vadduwm	8,8,18
	vadduwm	12,12,19

	vperm	0,0,0,31
	vperm	4,4,4,31
	vperm	8,8,8,31
	vperm	12,12,12,31

	cmplwi	5,0x40
	blt	Ltail_vsx

	.long	0x7F602619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	27,27,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7F601F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	Ldone_vsx

	vadduwm	0,1,16
	vadduwm	4,5,17
	vadduwm	8,9,18
	vadduwm	12,13,19

	vperm	0,0,0,31
	vperm	4,4,4,31
	vperm	8,8,8,31
	vperm	12,12,12,31

	cmplwi	5,0x40
	blt	Ltail_vsx

	.long	0x7F602619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	27,27,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7F601F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	Ldone_vsx

	vadduwm	0,2,16
	vadduwm	4,6,17
	vadduwm	8,10,18
	vadduwm	12,14,19

	vperm	0,0,0,31
	vperm	4,4,4,31
	vperm	8,8,8,31
	vperm	12,12,12,31

	cmplwi	5,0x40
	blt	Ltail_vsx

	.long	0x7F602619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	27,27,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7F601F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	Ldone_vsx

	vadduwm	0,3,16
	vadduwm	4,7,17
	vadduwm	8,11,18
	vadduwm	12,15,19

	vperm	0,0,0,31
	vperm	4,4,4,31
	vperm	8,8,8,31
	vperm	12,12,12,31

	cmplwi	5,0x40
	blt	Ltail_vsx

	.long	0x7F602619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	27,27,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7F601F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	mtctr	0
	bne	Loop_outer_vsx

Ldone_vsx:
	lwz	12,196(1)
	li	10,103
	li	11,119
	lwz	0, 204(1)
	or	12,12,12
	lvx	26,10,1
	addi	10,10,32
	lvx	27,11,1
	addi	11,11,32
	lvx	28,10,1
	addi	10,10,32
	lvx	29,11,1
	addi	11,11,32
	lvx	30,10,1
	lvx	31,11,1
	mtlr	0
	addi	1,1,200
	blr	

.align	4
Ltail_vsx:
	addi	11,1,24
	mtctr	5
	.long	0x7C005F19
	.long	0x7C885F19
	.long	0x7D095F19
	.long	0x7D8A5F19
	subi	12,11,1
	subi	4,4,1
	subi	3,3,1

Loop_tail_vsx:
	lbzu	6,1(12)
	lbzu	7,1(4)
	xor	6,6,7
	stbu	6,1(3)
	bc	16,0,Loop_tail_vsx

	.long	0x7E005F19
	.long	0x7E085F19
	.long	0x7E095F19
	.long	0x7E0A5F19

	b	Ldone_vsx
.long	0
.byte	0,12,0x04,1,0x80,0,5,0
.long	0

.align	5
Lconsts:
	mflr	0
	bcl	20,31,$+4
	mflr	12
	addi	12,12,56
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.space	28
Lsigma:
.long	0x61707865,0x3320646e,0x79622d32,0x6b206574
.long	1,0,0,0
.long	4,0,0,0
.long	0x02030001,0x06070405,0x0a0b0809,0x0e0f0c0d
.long	0x01020300,0x05060704,0x090a0b08,0x0d0e0f0c
.long	0x61707865,0x61707865,0x61707865,0x61707865
.long	0x3320646e,0x3320646e,0x3320646e,0x3320646e
.long	0x79622d32,0x79622d32,0x79622d32,0x79622d32
.long	0x6b206574,0x6b206574,0x6b206574,0x6b206574
.long	0,1,2,3
.byte	67,104,97,67,104,97,50,48,32,102,111,114,32,80,111,119,101,114,80,67,47,65,108,116,105,86,101,99,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2
