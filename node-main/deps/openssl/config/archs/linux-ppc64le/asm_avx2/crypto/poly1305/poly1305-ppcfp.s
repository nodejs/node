.machine	"any"
.abiversion	2
.text

.globl	poly1305_init_fpu
.type	poly1305_init_fpu,@function
.align	6
poly1305_init_fpu:
.localentry	poly1305_init_fpu,0

	stdu	1,-48(1)
	mflr	6
	std	6,64(1)

	bl	.LPICmeup

	xor	0,0,0
	mtlr	6

	lfd	8,8*0(5)
	lfd	9,8*1(5)
	lfd	10,8*2(5)
	lfd	11,8*3(5)
	lfd	12,8*4(5)
	lfd	13,8*5(5)

	stfd	8,8*0(3)
	stfd	9,8*1(3)
	stfd	10,8*2(3)
	stfd	11,8*3(3)

	cmpld	4,0
	beq-	.Lno_key

	lfd	6,8*13(5)
	mffs	7

	stfd	8,8*4(3)
	stfd	9,8*5(3)
	stfd	10,8*6(3)
	stfd	11,8*7(3)

	li	8,4
	li	9,8
	li	10,12
	lwzx	7,0,4
	lwzx	8,8,4
	lwzx	9,9,4
	lwzx	10,10,4

	lis	11,0xf000
	ori	12,11,3
	andc	7,7,11
	andc	8,8,12
	andc	9,9,12
	andc	10,10,12

	stw	7,32(3)
	stw	8,40(3)
	stw	9,48(3)
	stw	10,56(3)

	mtfsf	255,6
	stfd	8,8*18(3)
	stfd	9,8*19(3)
	stfd	10,8*20(3)
	stfd	11,8*21(3)
	stfd	12,8*22(3)
	stfd	13,8*23(3)

	lfd	0,8*4(3)
	lfd	2,8*5(3)
	lfd	4,8*6(3)
	lfd	6,8*7(3)

	fsub	0,0,8
	fsub	2,2,9
	fsub	4,4,10
	fsub	6,6,11

	lfd	8,8*6(5)
	lfd	9,8*7(5)
	lfd	10,8*8(5)
	lfd	11,8*9(5)

	fmul	3,2,13
	fmul	5,4,13
	stfd	7,8*15(3)
	fmul	7,6,13

	fadd	1,0,8
	stfd	3,8*12(3)
	fadd	3,2,9
	stfd	5,8*13(3)
	fadd	5,4,10
	stfd	7,8*14(3)
	fadd	7,6,11

	fsub	1,1,8
	fsub	3,3,9
	fsub	5,5,10
	fsub	7,7,11

	lfd	8,8*10(5)
	lfd	9,8*11(5)
	lfd	10,8*12(5)

	fsub	0,0,1
	fsub	2,2,3
	fsub	4,4,5
	fsub	6,6,7

	stfd	1,8*5(3)
	stfd	3,8*7(3)
	stfd	5,8*9(3)
	stfd	7,8*11(3)

	stfd	0,8*4(3)
	stfd	2,8*6(3)
	stfd	4,8*8(3)
	stfd	6,8*10(3)

	lfd	2,8*12(3)
	lfd	4,8*13(3)
	lfd	6,8*14(3)
	lfd	0,8*15(3)

	fadd	3,2,8
	fadd	5,4,9
	fadd	7,6,10

	fsub	3,3,8
	fsub	5,5,9
	fsub	7,7,10

	fsub	2,2,3
	fsub	4,4,5
	fsub	6,6,7

	stfd	3,8*13(3)
	stfd	5,8*15(3)
	stfd	7,8*17(3)

	stfd	2,8*12(3)
	stfd	4,8*14(3)
	stfd	6,8*16(3)

	mtfsf	255,0
.Lno_key:
	xor	3,3,3
	addi	1,1,48
	blr	
.long	0
.byte	0,12,4,1,0x80,0,2,0
.size	poly1305_init_fpu,.-poly1305_init_fpu

.globl	poly1305_blocks_fpu
.type	poly1305_blocks_fpu,@function
.align	4
poly1305_blocks_fpu:
.localentry	poly1305_blocks_fpu,0

	srwi.	5,5,4
	beq-	.Labort

	stdu	1,-240(1)
	mflr	0
	stfd	14,96(1)
	stfd	15,104(1)
	stfd	16,112(1)
	stfd	17,120(1)
	stfd	18,128(1)
	stfd	19,136(1)
	stfd	20,144(1)
	stfd	21,152(1)
	stfd	22,160(1)
	stfd	23,168(1)
	stfd	24,176(1)
	stfd	25,184(1)
	stfd	26,192(1)
	stfd	27,200(1)
	stfd	28,208(1)
	stfd	29,216(1)
	stfd	30,224(1)
	stfd	31,232(1)
	std	0,256(1)

	xor	0,0,0
	li	10,1
	mtctr	5
	neg	5,5
	stw	0,84(1)
	stw	10,80(1)

	lfd	8,8*18(3)
	lfd	9,8*19(3)
	lfd	10,8*20(3)
	lfd	11,8*21(3)
	lfd	12,8*22(3)
	lfd	13,8*23(3)

	lfd	0,8*0(3)
	lfd	2,8*1(3)
	lfd	4,8*2(3)
	lfd	6,8*3(3)

	stfd	8,48(1)
	oris	10,6,18736
	stfd	9,56(1)
	stfd	10,64(1)
	stw	10,76(1)

	li	11,4
	li	12,8
	li	6,12
	lwzx	7,0,4
	lwzx	8,11,4
	lwzx	9,12,4
	lwzx	10,6,4
	addi	4,4,16

	stw	7,48(1)
	stw	8,56(1)
	stw	9,64(1)
	stw	10,72(1)

	mffs	28
	lfd	29,80(1)
	lfd	14,8*4(3)
	lfd	15,8*5(3)
	lfd	16,8*6(3)
	lfd	17,8*7(3)
	lfd	18,8*8(3)
	lfd	19,8*9(3)
	lfd	24,8*10(3)
	lfd	25,8*11(3)
	lfd	26,8*12(3)
	lfd	27,8*13(3)
	lfd	20,8*14(3)
	lfd	21,8*15(3)
	lfd	22,8*16(3)
	lfd	23,8*17(3)

	stfd	28,80(1)
	mtfsf	255,29

	addic	5,5,1
	addze	0,0
	slwi.	0,0,4
	sub	4,4,0

	lfd	28,48(1)
	lfd	29,56(1)
	lfd	30,64(1)
	lfd	31,72(1)

	fsub	0,0,8
	lwzx	7,0,4
	fsub	2,2,9
	lwzx	8,11,4
	fsub	4,4,10
	lwzx	9,12,4
	fsub	6,6,11
	lwzx	10,6,4

	fsub	28,28,8
	addi	4,4,16
	fsub	29,29,9
	fsub	30,30,10
	fsub	31,31,11

	fadd	28,28,0
	stw	7,48(1)
	fadd	29,29,2
	stw	8,56(1)
	fadd	30,30,4
	stw	9,64(1)
	fadd	31,31,6
	stw	10,72(1)

	b	.Lentry

.align	4
.Loop:
	fsub	30,30,8
	addic	5,5,1
	fsub	31,31,9
	addze	0,0
	fsub	26,26,10
	slwi.	0,0,4
	fsub	27,27,11
	sub	4,4,0

	fadd	0,0,30
	fadd	1,1,31
	fadd	4,4,26
	fadd	5,5,27


	fadd	26,2,10
	lwzx	7,0,4
	fadd	27,3,10
	lwzx	8,11,4
	fadd	30,6,12
	lwzx	9,12,4
	fadd	31,7,12
	lwzx	10,6,4
	fadd	24,0,9
	addi	4,4,16
	fadd	25,1,9
	fadd	28,4,11
	fadd	29,5,11

	fsub	26,26,10
	stw	7,48(1)
	fsub	27,27,10
	stw	8,56(1)
	fsub	30,30,12
	stw	9,64(1)
	fsub	31,31,12
	stw	10,72(1)
	fsub	24,24,9
	fsub	25,25,9
	fsub	28,28,11
	fsub	29,29,11

	fsub	2,2,26
	fsub	3,3,27
	fsub	6,6,30
	fsub	7,7,31
	fsub	4,4,28
	fsub	5,5,29
	fsub	0,0,24
	fsub	1,1,25

	fadd	2,2,24
	fadd	3,3,25
	fadd	6,6,28
	fadd	7,7,29
	fadd	4,4,26
	fadd	5,5,27
	fmadd	0,30,13,0
	fmadd	1,31,13,1

	fadd	29,2,3
	lfd	26,8*12(3)
	fadd	31,6,7
	lfd	27,8*13(3)
	fadd	30,4,5
	lfd	24,8*10(3)
	fadd	28,0,1
	lfd	25,8*11(3)
.Lentry:
	fmul	0,22,29
	fmul	1,23,29
	fmul	4,16,29
	fmul	5,17,29
	fmul	2,14,29
	fmul	3,15,29
	fmul	6,18,29
	fmul	7,19,29

	fmadd	0,26,31,0
	fmadd	1,27,31,1
	fmadd	4,22,31,4
	fmadd	5,23,31,5
	fmadd	2,20,31,2
	fmadd	3,21,31,3
	fmadd	6,14,31,6
	fmadd	7,15,31,7

	fmadd	0,20,30,0
	fmadd	1,21,30,1
	fmadd	4,14,30,4
	fmadd	5,15,30,5
	fmadd	2,22,30,2
	fmadd	3,23,30,3
	fmadd	6,16,30,6
	fmadd	7,17,30,7

	fmadd	0,14,28,0
	lfd	30,48(1)
	fmadd	1,15,28,1
	lfd	31,56(1)
	fmadd	4,18,28,4
	lfd	26,64(1)
	fmadd	5,19,28,5
	lfd	27,72(1)
	fmadd	2,16,28,2
	fmadd	3,17,28,3
	fmadd	6,24,28,6
	fmadd	7,25,28,7

	bdnz	.Loop


	fadd	24,0,9
	fadd	25,1,9
	fadd	28,4,11
	fadd	29,5,11
	fadd	26,2,10
	fadd	27,3,10
	fadd	30,6,12
	fadd	31,7,12

	fsub	24,24,9
	fsub	25,25,9
	fsub	28,28,11
	fsub	29,29,11
	fsub	26,26,10
	fsub	27,27,10
	fsub	30,30,12
	fsub	31,31,12

	fsub	2,2,26
	fsub	3,3,27
	fsub	6,6,30
	fsub	7,7,31
	fsub	4,4,28
	fsub	5,5,29
	fsub	0,0,24
	fsub	1,1,25

	fadd	2,2,24
	fadd	3,3,25
	fadd	6,6,28
	fadd	7,7,29
	fadd	4,4,26
	fadd	5,5,27
	fmadd	0,30,13,0
	fmadd	1,31,13,1

	fadd	29,2,3
	fadd	31,6,7
	fadd	30,4,5
	fadd	28,0,1

	lfd	0,80(1)
	fadd	29,29,9
	fadd	31,31,11
	fadd	30,30,10
	fadd	28,28,8

	stfd	29,8*1(3)
	stfd	31,8*3(3)
	stfd	30,8*2(3)
	stfd	28,8*0(3)

	mtfsf	255,0
	lfd	14,96(1)
	lfd	15,104(1)
	lfd	16,112(1)
	lfd	17,120(1)
	lfd	18,128(1)
	lfd	19,136(1)
	lfd	20,144(1)
	lfd	21,152(1)
	lfd	22,160(1)
	lfd	23,168(1)
	lfd	24,176(1)
	lfd	25,184(1)
	lfd	26,192(1)
	lfd	27,200(1)
	lfd	28,208(1)
	lfd	29,216(1)
	lfd	30,224(1)
	lfd	31,232(1)
	addi	1,1,240
.Labort:
	blr	
.long	0
.byte	0,12,4,1,0x80,0,4,0
.size	poly1305_blocks_fpu,.-poly1305_blocks_fpu
.globl	poly1305_emit_fpu
.type	poly1305_emit_fpu,@function
.align	4
poly1305_emit_fpu:
.localentry	poly1305_emit_fpu,0

	stdu	1,-80(1)
	mflr	0
	std	28,48(1)
	std	29,56(1)
	std	30,64(1)
	std	31,72(1)
	std	0,96(1)

	lwz	28,4(3)
	lwz	7,0(3)
	lwz	29,12(3)
	lwz	8,8(3)
	lwz	30,20(3)
	lwz	9,16(3)
	lwz	31,28(3)
	lwz	10,24(3)

	lis	0,0xfff0
	andc	28,28,0
	andc	29,29,0
	andc	30,30,0
	andc	31,31,0
	li	0,3

	srwi	6,31,2
	and	11,31,0
	andc	31,31,0
	add	31,31,6
	add	7,7,31
	add	8,8,28
	add	9,9,29
	add	10,10,30

	srdi	28,7,32
	add	8,8,28
	srdi	29,8,32
	add	9,9,29
	srdi	30,9,32
	add	10,10,30
	srdi	31,10,32
	add	11,11,31

	insrdi	7,8,32,0
	insrdi	9,10,32,0

	addic	28,7,5
	addze	29,9
	addze	30,11

	srdi	0,30,2
	neg	0,0
	sradi	0,0,63
	ld	30,0(5)
	ld	31,8(5)

	andc	7,7,0
	and	28,28,0
	andc	9,9,0
	and	29,29,0
	or	7,7,28
	or	9,9,29
	addc	7,7,30
	adde	9,9,31

	srdi	8,7,32
	srdi	10,9,32
	stw	7,0(4)
	stw	8,4(4)
	stw	9,8(4)
	stw	10,12(4)
	ld	28,48(1)
	ld	29,56(1)
	ld	30,64(1)
	ld	31,72(1)
	addi	1,1,80
	blr	
.long	0
.byte	0,12,4,1,0x80,4,3,0
.size	poly1305_emit_fpu,.-poly1305_emit_fpu
.align	6
.LPICmeup:
	mflr	0
	bcl	20,31,$+4
	mflr	5
	addi	5,5,56
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.space	28

.long	0x00000000,0x43300000
.long	0x00000000,0x45300000
.long	0x00000000,0x47300000
.long	0x00000000,0x49300000
.long	0x00000000,0x4b500000

.long	0x00000000,0x37f40000

.long	0x00000000,0x44300000
.long	0x00000000,0x46300000
.long	0x00000000,0x48300000
.long	0x00000000,0x4a300000
.long	0x00000000,0x3e300000
.long	0x00000000,0x40300000
.long	0x00000000,0x42300000

.long	0x00000001,0x00000000
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,80,80,67,32,70,80,85,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	4
