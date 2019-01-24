.machine	"any"
.text
.globl	poly1305_init_int
.type	poly1305_init_int,@function
.align	4
poly1305_init_int:
	xor	0,0,0
	stw	0,0(3)
	stw	0,4(3)
	stw	0,8(3)
	stw	0,12(3)
	stw	0,16(3)

	.long	0x7c040040
	beq-	.Lno_key
	li	8,4
	lwbrx	7,0,4
	li	9,8
	lwbrx	8,8,4
	li	10,12
	lwbrx	9,9,4
	lwbrx	10,10,4
	lis	0,0xf000
	li	12,-4
	andc	12,12,0

	andc	7,7,0
	and	8,8,12
	and	9,9,12
	and	10,10,12

	stw	7,32(3)
	stw	8,36(3)
	stw	9,40(3)
	stw	10,44(3)

.Lno_key:
	xor	3,3,3
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.size	poly1305_init_int,.-poly1305_init_int

.globl	poly1305_blocks
.type	poly1305_blocks,@function
.align	4
poly1305_blocks:
	srwi.	5,5,4
	beq-	.Labort

	stwu	1,-96(1)
	mflr	0
	stw	14,24(1)
	stw	15,28(1)
	stw	16,32(1)
	stw	17,36(1)
	stw	18,40(1)
	stw	19,44(1)
	stw	20,48(1)
	stw	21,52(1)
	stw	22,56(1)
	stw	23,60(1)
	stw	24,64(1)
	stw	25,68(1)
	stw	26,72(1)
	stw	27,76(1)
	stw	28,80(1)
	stw	29,84(1)
	stw	30,88(1)
	stw	31,92(1)
	stw	0,100(1)

	lwz	12,32(3)
	lwz	14,36(3)
	lwz	15,40(3)
	lwz	16,44(3)

	lwz	7,0(3)
	lwz	8,4(3)
	lwz	9,8(3)
	lwz	10,12(3)
	lwz	11,16(3)

	srwi	17,14,2
	srwi	18,15,2
	srwi	19,16,2
	add	17,17,14
	add	18,18,15
	add	19,19,16
	mtctr	5
	li	0,3
	b	.Loop

.align	4
.Loop:
	li	29,4
	lwbrx	28,0,4
	li	30,8
	lwbrx	29,29,4
	li	31,12
	lwbrx	30,30,4
	lwbrx	31,31,4
	addi	4,4,16

	addc	7,7,28
	adde	8,8,29
	adde	9,9,30

	mullw	28,7,12
	mulhwu	24,7,12

	mullw	29,7,14
	mulhwu	25,7,14

	mullw	30,7,15
	mulhwu	26,7,15

	adde	10,10,31
	adde	11,11,6

	mullw	31,7,16
	mulhwu	27,7,16

	mullw	20,8,19
	mulhwu	21,8,19

	mullw	22,8,12
	mulhwu	23,8,12
	addc	28,28,20
	adde	24,24,21

	mullw	20,8,14
	mulhwu	21,8,14
	addc	29,29,22
	adde	25,25,23

	mullw	22,8,15
	mulhwu	23,8,15
	addc	30,30,20
	adde	26,26,21

	mullw	20,9,18
	mulhwu	21,9,18
	addc	31,31,22
	adde	27,27,23

	mullw	22,9,19
	mulhwu	23,9,19
	addc	28,28,20
	adde	24,24,21

	mullw	20,9,12
	mulhwu	21,9,12
	addc	29,29,22
	adde	25,25,23

	mullw	22,9,14
	mulhwu	23,9,14
	addc	30,30,20
	adde	26,26,21

	mullw	20,10,17
	mulhwu	21,10,17
	addc	31,31,22
	adde	27,27,23

	mullw	22,10,18
	mulhwu	23,10,18
	addc	28,28,20
	adde	24,24,21

	mullw	20,10,19
	mulhwu	21,10,19
	addc	29,29,22
	adde	25,25,23

	mullw	22,10,12
	mulhwu	23,10,12
	addc	30,30,20
	adde	26,26,21

	mullw	20,11,17
	addc	31,31,22
	adde	27,27,23
	addc	29,29,20

	mullw	21,11,18
	addze	25,25
	addc	30,30,21
	addze	26,26

	mullw	22,11,19
	addc	31,31,22
	addze	27,27

	mullw	11,11,12

	addc	8,29,24
	adde	9,30,25
	adde	10,31,26
	adde	11,11,27

	andc	24,11,0
	and	11,11,0
	srwi	25,24,2
	add	24,24,25
	addc	7,28,24
	addze	8,8
	addze	9,9
	addze	10,10
	addze	11,11

	bdnz	.Loop

	stw	7,0(3)
	stw	8,4(3)
	stw	9,8(3)
	stw	10,12(3)
	stw	11,16(3)

	lwz	14,24(1)
	lwz	15,28(1)
	lwz	16,32(1)
	lwz	17,36(1)
	lwz	18,40(1)
	lwz	19,44(1)
	lwz	20,48(1)
	lwz	21,52(1)
	lwz	22,56(1)
	lwz	23,60(1)
	lwz	24,64(1)
	lwz	25,68(1)
	lwz	26,72(1)
	lwz	27,76(1)
	lwz	28,80(1)
	lwz	29,84(1)
	lwz	30,88(1)
	lwz	31,92(1)
	addi	1,1,96
.Labort:
	blr	
.long	0
.byte	0,12,4,1,0x80,18,4,0
.size	poly1305_blocks,.-poly1305_blocks

.globl	poly1305_emit
.type	poly1305_emit,@function
.align	4
poly1305_emit:
	stwu	1,-96(1)
	mflr	0
	stw	28,80(1)
	stw	29,84(1)
	stw	30,88(1)
	stw	31,92(1)
	stw	0,100(1)

	lwz	7,0(3)
	lwz	8,4(3)
	lwz	9,8(3)
	lwz	10,12(3)
	lwz	11,16(3)

	addic	28,7,5
	addze	29,8
	addze	30,9
	addze	31,10
	addze	0,11

	srwi	0,0,2
	neg	0,0

	andc	7,7,0
	and	28,28,0
	andc	8,8,0
	and	29,29,0
	or	7,7,28
	lwz	28,0(5)
	andc	9,9,0
	and	30,30,0
	or	8,8,29
	lwz	29,4(5)
	andc	10,10,0
	and	31,31,0
	or	9,9,30
	lwz	30,8(5)
	or	10,10,31
	lwz	31,12(5)

	addc	7,7,28
	adde	8,8,29
	adde	9,9,30
	adde	10,10,31
	li	29,4
	stwbrx	7,0,4
	li	30,8
	stwbrx	8,29,4
	li	31,12
	stwbrx	9,30,4
	stwbrx	10,31,4
	lwz	28,80(1)
	lwz	29,84(1)
	lwz	30,88(1)
	lwz	31,92(1)
	addi	1,1,96
	blr	
.long	0
.byte	0,12,4,1,0x80,4,3,0
.size	poly1305_emit,.-poly1305_emit
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
