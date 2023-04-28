.abiversion	2
.text

.type	KeccakF1600_int,@function
.align	5
KeccakF1600_int:
.localentry	KeccakF1600_int,0

	li	0,24
	mtctr	0
	b	.Loop
.align	4
.Loop:
	xor	0,7,12
	std	11,96(1)
	xor	3,8,6
	std	16,104(1)
	xor	4,9,14
	std	21,112(1)
	xor	5,10,15
	std	26,120(1)
	xor	11,11,16
	xor	0,0,17
	xor	3,3,18
	xor	4,4,19
	xor	5,5,20
	xor	11,11,21
	xor	0,0,22
	xor	3,3,23
	xor	4,4,24
	xor	5,5,25
	xor	11,11,26
	xor	0,0,27
	xor	4,4,29
	xor	3,3,28
	xor	5,5,30
	rotldi	16,4,1
	xor	11,11,31
	rotldi	21,5,1
	xor	16,16,0
	rotldi	26,11,1

	xor	8,8,16
	xor	6,6,16
	xor	18,18,16
	xor	23,23,16
	xor	28,28,16

	rotldi	16,0,1
	xor	21,21,3
	xor	4,4,26
	rotldi	26,3,1
	xor	5,5,16
	xor	11,11,26

	xor	3,   9,21
	xor	14,14,21
	xor	19,19,21
	xor	24,24,21
	xor	29,29,21

	xor	7,7,11
	xor	12,12,11
	xor	17,17,11
	xor	22,22,11
	xor	27,27,11
	ld	11,96(1)
	xor	0,   10,4
	ld	16,104(1)
	xor	15,15,4
	ld	21,112(1)
	xor	20,20,4
	ld	26,120(1)
	xor	25,25,4
	xor	30,30,4

	xor	4,   11,5
	xor	16,16,5
	xor	21,21,5
	xor	26,26,5
	xor	31,31,5

	mr	5,8
	rotldi	8,6,44

	rotldi	9,19,43

	rotldi	10,25,21

	rotldi	11,31,14

	rotldi	6,16,20
	rotldi	19,20,25
	rotldi	25,24,15
	rotldi	31,28,2

	rotldi	16,29,61
	rotldi	20,26,8
	rotldi	24,18,10
	rotldi	28,15,55

	rotldi	29,21,39
	rotldi	26,30,56
	rotldi	18,14,6
	rotldi	15,23,45

	rotldi	21,27,18
	rotldi	30,22,41
	rotldi	14,17,3
	rotldi	23,12,36

	rotldi	12,0,28
	rotldi	17,5,1
	rotldi	22,4,27
	rotldi	27,3,62

	andc	0,9,8
	andc	3,10,9
	andc	4,7,11
	andc	5,8,7
	xor	7,7,0
	andc	0,11,10
	xor	8,8,3
	ld	3,80(1)
	xor	10,10,4
	xor	11,11,5
	xor	9,9,0
	ldu	5,8(3)

	andc	0,14,6
	std	3,80(1)
	andc	3,15,14
	andc	4,12,16
	xor	7,7,5
	andc	5,6,12
	xor	12,12,0
	andc	0,16,15
	xor	6,6,3
	xor	15,15,4
	xor	16,16,5
	xor	14,14,0

	andc	0,19,18
	andc	3,20,19
	andc	4,17,21
	andc	5,18,17
	xor	17,17,0
	andc	0,21,20
	xor	18,18,3
	xor	20,20,4
	xor	21,21,5
	xor	19,19,0

	andc	0,24,23
	andc	3,25,24
	andc	4,22,26
	andc	5,23,22
	xor	22,22,0
	andc	0,26,25
	xor	23,23,3
	xor	25,25,4
	xor	26,26,5
	xor	24,24,0

	andc	0,29,28
	andc	3,30,29
	andc	4,27,31
	andc	5,28,27
	xor	27,27,0
	andc	0,31,30
	xor	28,28,3
	xor	30,30,4
	xor	31,31,5
	xor	29,29,0

	bdnz	.Loop

	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.size	KeccakF1600_int,.-KeccakF1600_int

.type	KeccakF1600,@function
.align	5
KeccakF1600:
.localentry	KeccakF1600,0

	stdu	1,-272(1)
	mflr	0
	std	14,128(1)
	std	15,136(1)
	std	16,144(1)
	std	17,152(1)
	std	18,160(1)
	std	19,168(1)
	std	20,176(1)
	std	21,184(1)
	std	22,192(1)
	std	23,200(1)
	std	24,208(1)
	std	25,216(1)
	std	26,224(1)
	std	27,232(1)
	std	28,240(1)
	std	29,248(1)
	std	30,256(1)
	std	31,264(1)
	std	0,288(1)

	bl	PICmeup
	subi	12,12,8

	std	3,48(1)



	std	12,80(1)

	ld	7,0(3)
	ld	8,8(3)
	ld	9,16(3)
	ld	10,24(3)
	ld	11,32(3)
	ld	12,40(3)
	ld	6,48(3)
	ld	14,56(3)
	ld	15,64(3)
	ld	16,72(3)
	ld	17,80(3)
	ld	18,88(3)
	ld	19,96(3)
	ld	20,104(3)
	ld	21,112(3)
	ld	22,120(3)
	ld	23,128(3)
	ld	24,136(3)
	ld	25,144(3)
	ld	26,152(3)
	ld	27,160(3)
	ld	28,168(3)
	ld	29,176(3)
	ld	30,184(3)
	ld	31,192(3)

	bl	KeccakF1600_int

	ld	3,48(1)
	std	7,0(3)
	std	8,8(3)
	std	9,16(3)
	std	10,24(3)
	std	11,32(3)
	std	12,40(3)
	std	6,48(3)
	std	14,56(3)
	std	15,64(3)
	std	16,72(3)
	std	17,80(3)
	std	18,88(3)
	std	19,96(3)
	std	20,104(3)
	std	21,112(3)
	std	22,120(3)
	std	23,128(3)
	std	24,136(3)
	std	25,144(3)
	std	26,152(3)
	std	27,160(3)
	std	28,168(3)
	std	29,176(3)
	std	30,184(3)
	std	31,192(3)

	ld	0,288(1)
	ld	14,128(1)
	ld	15,136(1)
	ld	16,144(1)
	ld	17,152(1)
	ld	18,160(1)
	ld	19,168(1)
	ld	20,176(1)
	ld	21,184(1)
	ld	22,192(1)
	ld	23,200(1)
	ld	24,208(1)
	ld	25,216(1)
	ld	26,224(1)
	ld	27,232(1)
	ld	28,240(1)
	ld	29,248(1)
	ld	30,256(1)
	ld	31,264(1)
	mtlr	0
	addi	1,1,272
	blr	
.long	0
.byte	0,12,4,1,0x80,18,1,0
.long	0
.size	KeccakF1600,.-KeccakF1600

.type	dword_le_load,@function
.align	5
dword_le_load:
.localentry	dword_le_load,0

	lbz	0,1(3)
	lbz	4,2(3)
	lbz	5,3(3)
	insrdi	0,4,8,48
	lbz	4,4(3)
	insrdi	0,5,8,40
	lbz	5,5(3)
	insrdi	0,4,8,32
	lbz	4,6(3)
	insrdi	0,5,8,24
	lbz	5,7(3)
	insrdi	0,4,8,16
	lbzu	4,8(3)
	insrdi	0,5,8,8
	insrdi	0,4,8,0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,1,0
.long	0
.size	dword_le_load,.-dword_le_load

.globl	SHA3_absorb
.type	SHA3_absorb,@function
.type	SHA3_absorb,@function
.align	5
SHA3_absorb:
.localentry	SHA3_absorb,0

	stdu	1,-272(1)
	mflr	0
	std	14,128(1)
	std	15,136(1)
	std	16,144(1)
	std	17,152(1)
	std	18,160(1)
	std	19,168(1)
	std	20,176(1)
	std	21,184(1)
	std	22,192(1)
	std	23,200(1)
	std	24,208(1)
	std	25,216(1)
	std	26,224(1)
	std	27,232(1)
	std	28,240(1)
	std	29,248(1)
	std	30,256(1)
	std	31,264(1)
	std	0,288(1)

	bl	PICmeup
	subi	4,4,1
	subi	12,12,8

	std	3,48(1)
	std	4,56(1)
	std	5,64(1)
	std	6,72(1)
	mr	0,6
	std	12,80(1)

	ld	7,0(3)
	ld	8,8(3)
	ld	9,16(3)
	ld	10,24(3)
	ld	11,32(3)
	ld	12,40(3)
	ld	6,48(3)
	ld	14,56(3)
	ld	15,64(3)
	ld	16,72(3)
	ld	17,80(3)
	ld	18,88(3)
	ld	19,96(3)
	ld	20,104(3)
	ld	21,112(3)
	ld	22,120(3)
	ld	23,128(3)
	ld	24,136(3)
	ld	25,144(3)
	ld	26,152(3)
	ld	27,160(3)
	ld	28,168(3)
	ld	29,176(3)
	ld	30,184(3)
	ld	31,192(3)

	mr	3,4
	mr	4,5
	mr	5,0

	b	.Loop_absorb

.align	4
.Loop_absorb:
	cmpld	4,5
	blt	.Labsorbed

	sub	4,4,5
	srwi	5,5,3
	std	4,64(1)
	mtctr	5
	bl	dword_le_load
	xor	7,7,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	8,8,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	9,9,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	10,10,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	11,11,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	12,12,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	6,6,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	14,14,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	15,15,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	16,16,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	17,17,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	18,18,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	19,19,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	20,20,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	21,21,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	22,22,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	23,23,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	24,24,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	25,25,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	26,26,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	27,27,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	28,28,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	29,29,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	30,30,0
	bdz	.Lprocess_block
	bl	dword_le_load
	xor	31,31,0

.Lprocess_block:
	std	3,56(1)

	bl	KeccakF1600_int

	ld	0,80(1)
	ld	5,72(1)
	ld	4,64(1)
	ld	3,56(1)
	addic	0,0,-192
	std	0,80(1)

	b	.Loop_absorb

.align	4
.Labsorbed:
	ld	3,48(1)
	std	7,0(3)
	std	8,8(3)
	std	9,16(3)
	std	10,24(3)
	std	11,32(3)
	std	12,40(3)
	std	6,48(3)
	std	14,56(3)
	std	15,64(3)
	std	16,72(3)
	std	17,80(3)
	std	18,88(3)
	std	19,96(3)
	std	20,104(3)
	std	21,112(3)
	std	22,120(3)
	std	23,128(3)
	std	24,136(3)
	std	25,144(3)
	std	26,152(3)
	std	27,160(3)
	std	28,168(3)
	std	29,176(3)
	std	30,184(3)
	std	31,192(3)

	mr	3,4
	ld	0,288(1)
	ld	14,128(1)
	ld	15,136(1)
	ld	16,144(1)
	ld	17,152(1)
	ld	18,160(1)
	ld	19,168(1)
	ld	20,176(1)
	ld	21,184(1)
	ld	22,192(1)
	ld	23,200(1)
	ld	24,208(1)
	ld	25,216(1)
	ld	26,224(1)
	ld	27,232(1)
	ld	28,240(1)
	ld	29,248(1)
	ld	30,256(1)
	ld	31,264(1)
	mtlr	0
	addi	1,1,272
	blr	
.long	0
.byte	0,12,4,1,0x80,18,4,0
.long	0
.size	SHA3_absorb,.-SHA3_absorb
.globl	SHA3_squeeze
.type	SHA3_squeeze,@function
.type	SHA3_squeeze,@function
.align	5
SHA3_squeeze:
.localentry	SHA3_squeeze,0

	stdu	1,-80(1)
	mflr	0
	std	28,48(1)
	std	29,56(1)
	std	30,64(1)
	std	31,72(1)
	std	0,96(1)

	mr	28,3
	subi	3,3,8
	subi	29,4,1
	mr	30,5
	mr	31,6
	b	.Loop_squeeze

.align	4
.Loop_squeeze:
	ldu	0,8(3)
	cmpldi	30,8
	blt	.Lsqueeze_tail

	stb	0,1(29)
	srdi	0,0,8
	stb	0,2(29)
	srdi	0,0,8
	stb	0,3(29)
	srdi	0,0,8
	stb	0,4(29)
	srdi	0,0,8
	stb	0,5(29)
	srdi	0,0,8
	stb	0,6(29)
	srdi	0,0,8
	stb	0,7(29)
	srdi	0,0,8
	stbu	0,8(29)

	subic.	30,30,8
	beq	.Lsqueeze_done

	subic.	6,6,8
	bgt	.Loop_squeeze

	mr	3,28
	bl	KeccakF1600
	subi	3,28,8
	mr	6,31
	b	.Loop_squeeze

.align	4
.Lsqueeze_tail:
	mtctr	30
.Loop_tail:
	stbu	0,1(29)
	srdi	0,0,8
	bdnz	.Loop_tail

.Lsqueeze_done:
	ld	0,96(1)
	ld	28,48(1)
	ld	29,56(1)
	ld	30,64(1)
	ld	31,72(1)
	mtlr	0
	addi	1,1,80
	blr	
.long	0
.byte	0,12,4,1,0x80,4,4,0
.long	0
.size	SHA3_squeeze,.-SHA3_squeeze
.align	6
PICmeup:
	mflr	0
	bcl	20,31,$+4
	mflr	12
	addi	12,12,56
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.space	28
.type	iotas,@object
iotas:
.long	0x00000001,0x00000000
.long	0x00008082,0x00000000
.long	0x0000808a,0x80000000
.long	0x80008000,0x80000000
.long	0x0000808b,0x00000000
.long	0x80000001,0x00000000
.long	0x80008081,0x80000000
.long	0x00008009,0x80000000
.long	0x0000008a,0x00000000
.long	0x00000088,0x00000000
.long	0x80008009,0x00000000
.long	0x8000000a,0x00000000
.long	0x8000808b,0x00000000
.long	0x0000008b,0x80000000
.long	0x00008089,0x80000000
.long	0x00008003,0x80000000
.long	0x00008002,0x80000000
.long	0x00000080,0x80000000
.long	0x0000800a,0x00000000
.long	0x8000000a,0x80000000
.long	0x80008081,0x80000000
.long	0x00008080,0x80000000
.long	0x80000001,0x00000000
.long	0x80008008,0x80000000
.size	iotas,.-iotas
.byte	75,101,99,99,97,107,45,49,54,48,48,32,97,98,115,111,114,98,32,97,110,100,32,115,113,117,101,101,122,101,32,102,111,114,32,80,80,67,54,52,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
