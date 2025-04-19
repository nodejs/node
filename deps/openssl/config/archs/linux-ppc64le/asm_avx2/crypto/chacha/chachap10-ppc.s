
.globl	ChaCha20_ctr32_vsx_p10
.type	ChaCha20_ctr32_vsx_p10,@function
.align	5
ChaCha20_ctr32_vsx_p10:
.localentry	ChaCha20_ctr32_vsx_p10,0

	cmpldi	5,255
	ble	.Not_greater_than_8x
	b	ChaCha20_ctr32_vsx_8x
.Not_greater_than_8x:
	stdu	1,-224(1)
	mflr	0
	li	10,127
	li	11,143
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
	stw	12,220(1)
	li	12,-4096+63
	std	0, 240(1)
	or	12,12,12

	bl	.Lconsts
	.long	0x7E006619
	addi	12,12,0x70
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





	li	0,10
	mtctr	0
	b	.Loop_outer_vsx

.align	5
.Loop_outer_vsx:
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

.Loop_vsx_4x:
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

	bdnz	.Loop_vsx_4x

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






	cmpldi	5,0x40
	blt	.Ltail_vsx

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
	beq	.Ldone_vsx

	vadduwm	0,1,16
	vadduwm	4,5,17
	vadduwm	8,9,18
	vadduwm	12,13,19






	cmpldi	5,0x40
	blt	.Ltail_vsx

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
	beq	.Ldone_vsx

	vadduwm	0,2,16
	vadduwm	4,6,17
	vadduwm	8,10,18
	vadduwm	12,14,19






	cmpldi	5,0x40
	blt	.Ltail_vsx

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
	beq	.Ldone_vsx

	vadduwm	0,3,16
	vadduwm	4,7,17
	vadduwm	8,11,18
	vadduwm	12,15,19






	cmpldi	5,0x40
	blt	.Ltail_vsx

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
	bne	.Loop_outer_vsx

.Ldone_vsx:
	lwz	12,220(1)
	li	10,127
	li	11,143
	ld	0, 240(1)
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
	addi	1,1,224
	blr	

.align	4
.Ltail_vsx:
	addi	11,1,48
	mtctr	5
	.long	0x7C005F19
	.long	0x7C885F19
	.long	0x7D095F19
	.long	0x7D8A5F19
	subi	12,11,1
	subi	4,4,1
	subi	3,3,1

.Loop_tail_vsx:
	lbzu	6,1(12)
	lbzu	7,1(4)
	xor	6,6,7
	stbu	6,1(3)
	bdnz	.Loop_tail_vsx

	.long	0x7E005F19
	.long	0x7E085F19
	.long	0x7E095F19
	.long	0x7E0A5F19

	b	.Ldone_vsx
.long	0
.byte	0,12,0x04,1,0x80,0,5,0
.long	0
.size	ChaCha20_ctr32_vsx_p10,.-ChaCha20_ctr32_vsx_p10

.globl	ChaCha20_ctr32_vsx_8x
.type	ChaCha20_ctr32_vsx_8x,@function
.align	5
ChaCha20_ctr32_vsx_8x:
.localentry	ChaCha20_ctr32_vsx_8x,0

	stdu	1,-256(1)
	mflr	0
	li	10,127
	li	11,143
	li	12,-1
	stvx	24,10,1
	addi	10,10,32
	stvx	25,11,1
	addi	11,11,32
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
	stw	12,252(1)
	li	12,-4096+63
	std	0, 272(1)
	or	12,12,12

	bl	.Lconsts

	.long	0x7F606619
	addi	12,12,0x70
	li	8,16
	li	9,32
	li	10,48
	li	11,64

	vspltisw	16,-16
	vspltisw	20,12
	vspltisw	24,8
	vspltisw	28,7

	lvx	0,0,12
	lvx	1,8,12
	lvx	2,9,12
	lvx	3,10,12

	.long	0xF1308496
	.long	0xF154A496
	.long	0xF178C496
	.long	0xF19CE496
	.long	0xF2C00496
	.long	0xF2E10C96
	.long	0xF3021496
	.long	0xF3231C96

	.long	0x7F003619
	.long	0x7F283619
	.long	0x7F403E19
	vspltisw	30,4


	vxor	29,29,29
	.long	0x7F8B6619
	vspltw	2,26,0
	vsldoi	26,26,29,4
	vsldoi	26,29,26,12
	vadduwm	28,2,28
	vadduwm	30,28,30
	vspltw	0,25,2






	.long	0xF01BDC96
	.long	0xF038C496
	.long	0xF059CC96
	.long	0xF07AD496
	.long	0xF09CE496
	.long	0xF0BEF496
	.long	0xF1000496

	li	0,10
	mtctr	0
	b	.Loop_outer_vsx_8x

.align	5
.Loop_outer_vsx_8x:
	.long	0xF016B491
	.long	0xF037BC91
	.long	0xF058C491
	.long	0xF079CC91
	.long	0xF216B491
	.long	0xF237BC91
	.long	0xF258C491
	.long	0xF279CC91

	vspltw	4,24,0
	vspltw	5,24,1
	vspltw	6,24,2
	vspltw	7,24,3
	vspltw	20,24,0
	vspltw	21,24,1
	vspltw	22,24,2
	vspltw	23,24,3

	vspltw	8,25,0
	vspltw	9,25,1
	vspltw	10,25,2
	vspltw	11,25,3
	vspltw	24,25,0
	vspltw	27,25,3
	vspltw	25,25,1

	.long	0xF1842491
	vspltw	13,26,1
	vspltw	14,26,2
	vspltw	15,26,3
	.long	0xF3852C91
	vspltw	29,26,1
	vspltw	30,26,2
	vspltw	31,26,3
	.long	0xF3484491

.Loop_vsx_8x:
	.long	0xF1FBDC96
	.long	0xF3694C91
	vadduwm	0,0,4
	vadduwm	1,1,5
	vadduwm	2,2,6
	vadduwm	3,3,7
	vadduwm	16,16,20
	vadduwm	17,17,21
	vadduwm	18,18,22
	vadduwm	19,19,23
	vxor	12,12,0
	vxor	13,13,1
	vxor	14,14,2
	vxor	15,15,3
	vxor	28,28,16
	vxor	29,29,17
	vxor	30,30,18
	vxor	31,31,19
	vrlw	12,12,27
	vrlw	13,13,27
	vrlw	14,14,27
	vrlw	15,15,27
	vrlw	28,28,27
	vrlw	29,29,27
	vrlw	30,30,27
	vrlw	31,31,27
	.long	0xF1B39C96
	.long	0xF36F7C91
	.long	0xF26A5491
	vadduwm	8,8,12
	vadduwm	9,9,13
	vadduwm	10,10,14
	vadduwm	11,11,15
	vadduwm	24,24,28
	vadduwm	25,25,29
	vadduwm	26,26,30
	vadduwm	27,27,31
	vxor	4,4,8
	vxor	5,5,9
	vxor	6,6,10
	vxor	7,7,11
	vxor	20,20,24
	vxor	21,21,25
	vxor	22,22,26
	vxor	23,23,27
	vrlw	4,4,19
	vrlw	5,5,19
	vrlw	6,6,19
	vrlw	7,7,19
	vrlw	20,20,19
	vrlw	21,21,19
	vrlw	22,22,19
	vrlw	23,23,19
	.long	0xF26D6C91
	.long	0xF1FBDC96
	.long	0xF36B5C91
	vadduwm	0,0,4
	vadduwm	1,1,5
	vadduwm	2,2,6
	vadduwm	3,3,7
	vadduwm	16,16,20
	vadduwm	17,17,21
	vadduwm	18,18,22
	vadduwm	19,19,23
	vxor	12,12,0
	vxor	13,13,1
	vxor	14,14,2
	vxor	15,15,3
	vxor	28,28,16
	vxor	29,29,17
	vxor	30,30,18
	vxor	31,31,19
	vrlw	12,12,27
	vrlw	13,13,27
	vrlw	14,14,27
	vrlw	15,15,27
	vrlw	28,28,27
	vrlw	29,29,27
	vrlw	30,30,27
	vrlw	31,31,27
	.long	0xF36F7C91
	.long	0xF1B39C96
	.long	0xF26C6491
	vadduwm	8,8,12
	vadduwm	9,9,13
	vadduwm	10,10,14
	vadduwm	11,11,15
	vadduwm	24,24,28
	vadduwm	25,25,29
	vadduwm	26,26,30
	vadduwm	27,27,31
	vxor	4,4,8
	vxor	5,5,9
	vxor	6,6,10
	vxor	7,7,11
	vxor	20,20,24
	vxor	21,21,25
	vxor	22,22,26
	vxor	23,23,27
	vrlw	4,4,19
	vrlw	5,5,19
	vrlw	6,6,19
	vrlw	7,7,19
	vrlw	20,20,19
	vrlw	21,21,19
	vrlw	22,22,19
	vrlw	23,23,19
	.long	0xF26D6C91
	.long	0xF1F9CC96
	.long	0xF3294C91
	vadduwm	0,0,5
	vadduwm	1,1,6
	vadduwm	2,2,7
	vadduwm	3,3,4
	vadduwm	16,16,21
	vadduwm	17,17,22
	vadduwm	18,18,23
	vadduwm	19,19,20
	vxor	15,15,0
	vxor	12,12,1
	vxor	13,13,2
	vxor	14,14,3
	vxor	31,31,16
	vxor	28,28,17
	vxor	29,29,18
	vxor	30,30,19
	vrlw	15,15,25
	vrlw	12,12,25
	vrlw	13,13,25
	vrlw	14,14,25
	vrlw	31,31,25
	vrlw	28,28,25
	vrlw	29,29,25
	vrlw	30,30,25
	.long	0xF1B39C96
	.long	0xF32F7C91
	.long	0xF26A5491
	vadduwm	10,10,15
	vadduwm	11,11,12
	vadduwm	8,8,13
	vadduwm	9,9,14
	vadduwm	26,26,31
	vadduwm	27,27,28
	vadduwm	24,24,29
	vadduwm	25,25,30
	vxor	5,5,10
	vxor	6,6,11
	vxor	7,7,8
	vxor	4,4,9
	vxor	21,21,26
	vxor	22,22,27
	vxor	23,23,24
	vxor	20,20,25
	vrlw	5,5,19
	vrlw	6,6,19
	vrlw	7,7,19
	vrlw	4,4,19
	vrlw	21,21,19
	vrlw	22,22,19
	vrlw	23,23,19
	vrlw	20,20,19
	.long	0xF26D6C91
	.long	0xF1F9CC96
	.long	0xF32B5C91
	vadduwm	0,0,5
	vadduwm	1,1,6
	vadduwm	2,2,7
	vadduwm	3,3,4
	vadduwm	16,16,21
	vadduwm	17,17,22
	vadduwm	18,18,23
	vadduwm	19,19,20
	vxor	15,15,0
	vxor	12,12,1
	vxor	13,13,2
	vxor	14,14,3
	vxor	31,31,16
	vxor	28,28,17
	vxor	29,29,18
	vxor	30,30,19
	vrlw	15,15,25
	vrlw	12,12,25
	vrlw	13,13,25
	vrlw	14,14,25
	vrlw	31,31,25
	vrlw	28,28,25
	vrlw	29,29,25
	vrlw	30,30,25
	.long	0xF32F7C91
	.long	0xF1B39C96
	.long	0xF26C6491
	vadduwm	10,10,15
	vadduwm	11,11,12
	vadduwm	8,8,13
	vadduwm	9,9,14
	vadduwm	26,26,31
	vadduwm	27,27,28
	vadduwm	24,24,29
	vadduwm	25,25,30
	vxor	5,5,10
	vxor	6,6,11
	vxor	7,7,8
	vxor	4,4,9
	vxor	21,21,26
	vxor	22,22,27
	vxor	23,23,24
	vxor	20,20,25
	vrlw	5,5,19
	vrlw	6,6,19
	vrlw	7,7,19
	vrlw	4,4,19
	vrlw	21,21,19
	vrlw	22,22,19
	vrlw	23,23,19
	vrlw	20,20,19
	.long	0xF26D6C91

	bdnz	.Loop_vsx_8x
	.long	0xF1BCE496
	.long	0xF1DDEC96
	.long	0xF1FEF496
	.long	0xF21FFC96

	.long	0xF258C496
	.long	0xF279CC96
	.long	0xF29AD496
	.long	0xF2BBDC96

	.long	0xF0D6B496
	.long	0xF0F7BC96


	.long	0xF3600491
	.long	0xF3010C91
	.long	0xF3221491
	.long	0xF3431C91
	.long	0xF2C42491


	.long	0x12E00F8C
	.long	0x13821F8C
	.long	0x10000E8C
	.long	0x10421E8C

	.long	0x13A42F8C
	.long	0x13C63F8C
	.long	0x10842E8C
	.long	0x10C63E8C

	vadduwm	12,12,22

	.long	0xF0201057
	.long	0xF0601357
	.long	0xF017E057
	.long	0xF057E357
	.long	0xF0A43057
	.long	0xF0E43357
	.long	0xF09DF057
	.long	0xF0DDF357

	.long	0x12E84F8C
	.long	0x138A5F8C
	.long	0x11084E8C
	.long	0x114A5E8C
	.long	0x13AC6F8C
	.long	0x13CE7F8C
	.long	0x118C6E8C
	.long	0x11CE7E8C

	.long	0xF1285057
	.long	0xF1685357
	.long	0xF117E057
	.long	0xF157E357
	.long	0xF1AC7057
	.long	0xF1EC7357
	.long	0xF19DF057
	.long	0xF1DDF357

	vspltisw	23,8
	vadduwm	22,22,23
	.long	0xF096B496

	vadduwm	0,0,27
	vadduwm	4,4,24
	vadduwm	8,8,25
	vadduwm	12,12,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x

	.long	0x7EE02619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	23,23,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7EE01F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,1,27
	vadduwm	4,5,24
	vadduwm	8,9,25
	vadduwm	12,13,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x

	.long	0x7EE02619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	23,23,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7EE01F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,2,27
	vadduwm	4,6,24
	vadduwm	8,10,25
	vadduwm	12,14,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x

	.long	0x7EE02619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	23,23,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7EE01F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,3,27
	vadduwm	4,7,24
	vadduwm	8,11,25
	vadduwm	12,15,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x

	.long	0x7EE02619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	23,23,0
	vxor	28,28,4
	vxor	29,29,8
	vxor	30,30,12

	.long	0x7EE01F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x




	.long	0xF0A52C91

	.long	0xF1129491
	.long	0xF1339C91
	.long	0xF154A491
	.long	0xF175AC91

	.long	0xF18D6C91
	.long	0xF1AE7491
	.long	0xF1CF7C91
	.long	0xF1F08491
	vadduwm	12,12,5

	.long	0xF2C63491
	.long	0xF2E73C91


	.long	0x10908F8C
	.long	0x13929F8C
	.long	0x12108E8C
	.long	0x12529E8C
	.long	0x13B4AF8C
	.long	0x13D6BF8C
	.long	0x1294AE8C
	.long	0x12D6BE8C

	.long	0xF2309057
	.long	0xF2709357
	.long	0xF204E057
	.long	0xF244E357
	.long	0xF2B4B057
	.long	0xF2F4B357
	.long	0xF29DF057
	.long	0xF2DDF357

	.long	0x10884F8C
	.long	0x138A5F8C
	.long	0x11084E8C
	.long	0x114A5E8C
	.long	0x13AC6F8C
	.long	0x13CE7F8C
	.long	0x118C6E8C
	.long	0x11CE7E8C

	.long	0xF1285057
	.long	0xF1685357
	.long	0xF104E057
	.long	0xF144E357
	.long	0xF1AC7057
	.long	0xF1EC7357
	.long	0xF19DF057
	.long	0xF1DDF357

	vspltisw	4,8
	vadduwm	5,5,4
	.long	0xF0A52C96

	vadduwm	0,16,27
	vadduwm	1,20,24
	vadduwm	2,8,25
	vadduwm	3,12,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x_1

	.long	0x7C802619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	4,4,0
	vxor	28,28,1
	vxor	29,29,2
	vxor	30,30,3

	.long	0x7C801F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,17,27
	vadduwm	1,21,24
	vadduwm	2,9,25
	vadduwm	3,13,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x_1

	.long	0x7C802619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	4,4,0
	vxor	28,28,1
	vxor	29,29,2
	vxor	30,30,3

	.long	0x7C801F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,18,27
	vadduwm	1,22,24
	vadduwm	2,10,25
	vadduwm	3,14,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x_1

	.long	0x7C802619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	4,4,0
	vxor	28,28,1
	vxor	29,29,2
	vxor	30,30,3

	.long	0x7C801F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	vadduwm	0,19,27
	vadduwm	1,23,24
	vadduwm	2,11,25
	vadduwm	3,15,26






	cmpldi	5,0x40
	blt	.Ltail_vsx_8x_1

	.long	0x7C802619
	.long	0x7F882619
	.long	0x7FA92619
	.long	0x7FCA2619

	vxor	4,4,0
	vxor	28,28,1
	vxor	29,29,2
	vxor	30,30,3

	.long	0x7C801F19
	.long	0x7F881F19
	addi	4,4,0x40
	.long	0x7FA91F19
	subi	5,5,0x40
	.long	0x7FCA1F19
	addi	3,3,0x40
	beq	.Ldone_vsx_8x

	mtctr	0
	bne	.Loop_outer_vsx_8x

.Ldone_vsx_8x:
	lwz	12,252(1)
	li	10,127
	li	11,143
	ld	0, 272(1)
	or	12,12,12
	lvx	24,10,1
	addi	10,10,32
	lvx	25,11,1
	addi	11,11,32
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
	addi	1,1,256
	blr	

.align	4
.Ltail_vsx_8x:
	addi	11,1,48
	mtctr	5
	.long	0x7C005F19
	.long	0x7C885F19
	.long	0x7D095F19
	.long	0x7D8A5F19
	subi	12,11,1
	subi	4,4,1
	subi	3,3,1
	bl	.Loop_tail_vsx_8x
.Ltail_vsx_8x_1:
	addi	11,1,48
	mtctr	5
	.long	0x7C005F19
	.long	0x7C285F19
	.long	0x7C495F19
	.long	0x7C6A5F19
	subi	12,11,1
	subi	4,4,1
	subi	3,3,1
	bl	.Loop_tail_vsx_8x

.Loop_tail_vsx_8x:
	lbzu	6,1(12)
	lbzu	7,1(4)
	xor	6,6,7
	stbu	6,1(3)
	bdnz	.Loop_tail_vsx_8x

	.long	0x7F605F19
	.long	0x7F685F19
	.long	0x7F695F19
	.long	0x7F6A5F19

	b	.Ldone_vsx_8x
.long	0
.byte	0,12,0x04,1,0x80,0,5,0
.long	0
.size	ChaCha20_ctr32_vsx_8x,.-ChaCha20_ctr32_vsx_8x
.align	5
.Lconsts:
	mflr	0
	bcl	20,31,$+4
	mflr	12
	addi	12,12,56
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.space	28
.Lsigma:
.long	0x61707865,0x3320646e,0x79622d32,0x6b206574
.long	1,0,0,0
.long	2,0,0,0
.long	3,0,0,0
.long	4,0,0,0
.long	0x0e0f0c0d,0x0a0b0809,0x06070405,0x02030001
.long	0x0d0e0f0c,0x090a0b08,0x05060704,0x01020300
.long	0x61707865,0x61707865,0x61707865,0x61707865
.long	0x3320646e,0x3320646e,0x3320646e,0x3320646e
.long	0x79622d32,0x79622d32,0x79622d32,0x79622d32
.long	0x6b206574,0x6b206574,0x6b206574,0x6b206574
.long	0,1,2,3
.long	0x03020100,0x07060504,0x0b0a0908,0x0f0e0d0c
.byte	67,104,97,67,104,97,50,48,32,102,111,114,32,80,111,119,101,114,80,67,47,65,108,116,105,86,101,99,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2
