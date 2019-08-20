.machine	"any"
.abiversion	2
.text
.globl	poly1305_init_int
.type	poly1305_init_int,@function
.align	4
poly1305_init_int:
.localentry	poly1305_init_int,0

	xor	0,0,0
	std	0,0(3)
	std	0,8(3)
	std	0,16(3)

	cmpld	4,0
	beq-	.Lno_key
	ld	10,0(4)
	ld	11,8(4)
	lis	8,0xfff
	ori	8,8,0xfffc
	insrdi	8,8,32,0
	ori	7,8,3

	and	10,10,7
	and	11,11,8

	std	10,32(3)
	std	11,40(3)

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
.localentry	poly1305_blocks,0

	srdi.	5,5,4
	beq-	.Labort

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
	b	.Loop

.align	4
.Loop:
	ld	30,0(4)
	ld	31,8(4)
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

	bdnz	.Loop

	std	7,0(3)
	std	8,8(3)
	std	9,16(3)

	ld	27,152(1)
	ld	28,160(1)
	ld	29,168(1)
	ld	30,176(1)
	ld	31,184(1)
	addi	1,1,192
.Labort:
	blr	
.long	0
.byte	0,12,4,1,0x80,5,4,0
.size	poly1305_blocks,.-poly1305_blocks

.globl	poly1305_emit
.type	poly1305_emit,@function
.align	4
poly1305_emit:
.localentry	poly1305_emit,0

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
	addc	7,7,6
	adde	8,8,5
	std	7,0(4)
	std	8,8(4)
	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0
.size	poly1305_emit,.-poly1305_emit
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
