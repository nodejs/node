.machine	"any"
.csect	.text[PR],7
.globl	.poly1305_init_int
.align	4
.poly1305_init_int:
	xor	0,0,0
	std	0,0(3)
	std	0,8(3)
	std	0,16(3)

	cmpld	4,0
	beq-	Lno_key
	li	7,4
	lwbrx	10,0,4
	li	11,8
	lwbrx	7,7,4
	li	8,12
	lwbrx	11,11,4
	lwbrx	8,8,4
	insrdi	10,7,32,0
	insrdi	11,8,32,0
	lis	8,0xfff
	ori	8,8,0xfffc
	insrdi	8,8,32,0
	ori	7,8,3

	and	10,10,7
	and	11,11,8

	std	10,32(3)
	std	11,40(3)

Lno_key:
	xor	3,3,3
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0


.globl	.poly1305_blocks
.align	4
.poly1305_blocks:
	srdi.	5,5,4
	beq-	Labort

	stdu	1,-192(1)
	mflr	0
	std	27,152(1)
	std	28,160(1)
	std	29,168(1)
	std	30,176(1)
	std	31,184(1)
	std	0,208(1)

	ld	27,32(3)
	ld	28,40(3)

	ld	7,0(3)
	ld	8,8(3)
	ld	9,16(3)

	srdi	29,28,2
	mtctr	5
	add	29,29,28
	li	0,3
	b	Loop

.align	4
Loop:
	li	10,4
	lwbrx	30,0,4
	li	31,8
	lwbrx	10,10,4
	li	11,12
	lwbrx	31,31,4
	lwbrx	11,11,4
	insrdi	30,10,32,0
	insrdi	31,11,32,0
	addi	4,4,16

	addc	7,7,30
	adde	8,8,31

	mulld	10,7,27
	mulhdu	11,7,27
	adde	9,9,6

	mulld	30,8,29
	mulhdu	31,8,29
	addc	10,10,30
	adde	11,11,31

	mulld	30,7,28
	mulhdu	12,7,28
	addc	11,11,30
	addze	12,12

	mulld	30,8,27
	mulhdu	31,8,27
	addc	11,11,30
	adde	12,12,31

	mulld	30,9,29
	mulld	31,9,27
	addc	11,11,30
	adde	12,12,31

	andc	30,12,0
	and	9,12,0
	srdi	31,30,2
	add	30,30,31
	addc	7,10,30
	addze	8,11
	addze	9,9

	bc	16,0,Loop

	std	7,0(3)
	std	8,8(3)
	std	9,16(3)

	ld	27,152(1)
	ld	28,160(1)
	ld	29,168(1)
	ld	30,176(1)
	ld	31,184(1)
	addi	1,1,192
Labort:
	blr	
.long	0
.byte	0,12,4,1,0x80,5,4,0


.globl	.poly1305_emit
.align	4
.poly1305_emit:
	ld	7,0(3)
	ld	8,8(3)
	ld	9,16(3)
	ld	6,0(5)
	ld	5,8(5)

	addic	10,7,5
	addze	11,8
	addze	12,9

	srdi	0,12,2
	neg	0,0

	andc	7,7,0
	and	10,10,0
	andc	8,8,0
	and	11,11,0
	or	7,7,10
	or	8,8,11
	rotldi	6,6,32
	rotldi	5,5,32
	addc	7,7,6
	adde	8,8,5
	rldicl	0,7,32,32
	li	10,4
	stwbrx	7,0,4
	rldicl	7,8,32,32
	li	11,8
	stwbrx	0,10,4
	li	12,12
	stwbrx	8,11,4
	stwbrx	7,12,4
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0

.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
