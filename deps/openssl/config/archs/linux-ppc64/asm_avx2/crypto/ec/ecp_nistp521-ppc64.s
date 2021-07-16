.text

.globl	p521_felem_mul
.type	p521_felem_mul,@function
.section	".opd","aw"
.align	3
p521_felem_mul:
.quad	.p521_felem_mul,.TOC.@tocbase,0
.previous
.p521_felem_mul:

	mr	12,1
	stdu	1,-16*13(1)

	stxv	52,-16*12(12)
	stxv	53,-16*11(12)
	stxv	54,-16*10(12)
	stxv	55,-16*9(12)
	stxv	56,-16*8(12)
	stxv	57,-16*7(12)
	stxv	58,-16*6(12)
	stxv	59,-16*5(12)
	stxv	60,-16*4(12)
	stxv	61,-16*3(12)
	stxv	62,-16*2(12)
	stxv	63,-16*1(12)

	vspltisw	0,0

	lxsd	13,0(4)
	lxsd	14,8(4)
	lxsd	15,16(4)
	lxsd	16,24(4)
	lxsd	17,32(4)
	lxsd	18,40(4)
	lxsd	19,48(4)
	lxsd	20,56(4)
	lxsd	21,64(4)

	lxsd	3,0(5)
	lxsd	4,8(5)
	lxsd	5,16(5)
	lxsd	6,24(5)
	lxsd	7,32(5)
	lxsd	8,40(5)
	lxsd	9,48(5)
	lxsd	10,56(5)
	lxsd	11,64(5)

	.long	0x12ED1823

	xxpermdi	33,45,46,0b00
	xxpermdi	34,36,35,0b00
	.long	0x13011023

	xxpermdi	34,37,36,0b00
	.long	0x13211023
	.long	0x132F1E63

	xxpermdi	34,38,37,0b00
	.long	0x13411023
	xxpermdi	44,47,48,0b00
	xxpermdi	54,36,35,0b00
	.long	0x134CB6A3

	xxpermdi	34,39,38,0b00
	.long	0x13611023
	xxpermdi	54,37,36,0b00
	.long	0x136CB6E3
	.long	0x13711EE3

	xxpermdi	34,40,39,0b00
	.long	0x13811023
	xxpermdi	54,38,37,0b00
	.long	0x138CB723

	xxpermdi	34,41,40,0b00
	.long	0x13A11023
	xxpermdi	54,39,38,0b00
	.long	0x13ACB763

	xxpermdi	34,42,41,0b00
	.long	0x13C11023
	xxpermdi	54,40,39,0b00
	.long	0x13CCB7A3

	xxpermdi	34,43,42,0b00
	.long	0x13E11023
	xxpermdi	54,41,40,0b00
	.long	0x13ECB7E3

	xxpermdi	33,49,50,0b00
	xxpermdi	34,36,35,0b00
	.long	0x13811723

	xxpermdi	34,37,36,0b00
	.long	0x13A11763
	.long	0x13B31F63

	xxpermdi	34,38,37,0b00
	.long	0x13C117A3
	xxpermdi	44,51,52,0b00
	xxpermdi	54,36,35,0b00
	.long	0x13CCB7A3

	xxpermdi	34,39,38,0b00
	.long	0x13E117E3
	xxpermdi	54,37,36,0b00
	.long	0x13ECB7E3
	.long	0x13F51FE3

	li	8,0
	li	9,1
	mtvsrdd	33,9,8
	.long	0x10630DC4
	.long	0x10840DC4
	.long	0x10A50DC4
	.long	0x10C60DC4
	.long	0x10E70DC4
	.long	0x11080DC4
	.long	0x11290DC4
	.long	0x114A0DC4
	.long	0x116B0DC4

	.long	0x13D55FA3

	xxpermdi	34,43,42,0b00
	xxpermdi	33,52,53,0b00
	.long	0x13A11763

	xxpermdi	33,51,52,0b00
	.long	0x13811723
	.long	0x13954F23

	xxpermdi	33,50,51,0b00
	.long	0x136116E3
	xxpermdi	54,41,40,0b00
	xxpermdi	44,52,53,0b00
	.long	0x136CB6E3

	xxpermdi	33,49,50,0b00
	.long	0x134116A3
	xxpermdi	44,51,52,0b00
	.long	0x134CB6A3
	.long	0x13553EA3

	xxpermdi	33,48,49,0b00
	.long	0x13211663
	xxpermdi	44,50,51,0b00
	.long	0x132CB663

	xxpermdi	33,47,48,0b00
	.long	0x13011623
	xxpermdi	44,49,50,0b00
	.long	0x130CB623

	xxpermdi	33,46,47,0b00
	.long	0x12E115E3
	xxpermdi	44,48,49,0b00
	.long	0x12ECB5E3

	xxpermdi	34,39,38,0b00
	xxpermdi	33,52,53,0b00
	.long	0x13211663

	xxpermdi	33,51,52,0b00
	.long	0x13011623
	.long	0x13152E23

	xxpermdi	33,50,51,0b00
	.long	0x12E115E3
	xxpermdi	54,37,36,0b00
	xxpermdi	44,52,53,0b00
	.long	0x12ECB5E3

	stxv	55,0(3)
	stxv	56,16(3)
	stxv	57,32(3)
	stxv	58,48(3)
	stxv	59,64(3)
	stxv	60,80(3)
	stxv	61,96(3)
	stxv	62,112(3)
	stxv	63,128(3)

	ld	12,0(1)
	lxv	52,-16*12(12)
	lxv	53,-16*11(12)
	lxv	54,-16*10(12)
	lxv	55,-16*9(12)
	lxv	56,-16*8(12)
	lxv	57,-16*7(12)
	lxv	58,-16*6(12)
	lxv	59,-16*5(12)
	lxv	60,-16*4(12)
	lxv	61,-16*3(12)
	lxv	62,-16*2(12)
	lxv	63,-16*1(12)
	mr	1,12

	blr	
.size	.p521_felem_mul,.-.p521_felem_mul
.size	p521_felem_mul,.-.p521_felem_mul

.globl	p521_felem_square
.type	p521_felem_square,@function
.section	".opd","aw"
.align	3
p521_felem_square:
.quad	.p521_felem_square,.TOC.@tocbase,0
.previous
.p521_felem_square:

	mr	12,1
	stdu	1,-16*13(1)

	stxv	52,-16*12(12)
	stxv	53,-16*11(12)
	stxv	54,-16*10(12)
	stxv	55,-16*9(12)
	stxv	56,-16*8(12)
	stxv	57,-16*7(12)
	stxv	58,-16*6(12)
	stxv	59,-16*5(12)
	stxv	60,-16*4(12)
	stxv	61,-16*3(12)
	stxv	62,-16*2(12)
	stxv	63,-16*1(12)

	vspltisw	0,0

	lxsd	13,0(4)
	lxsd	14,8(4)
	lxsd	15,16(4)
	lxsd	16,24(4)
	lxsd	17,32(4)
	lxsd	18,40(4)
	lxsd	19,48(4)
	lxsd	20,56(4)
	lxsd	21,64(4)

	li	8,0
	li	9,1
	mtvsrdd	33,9,8
	.long	0x106D0DC4
	.long	0x108E0DC4
	.long	0x10AF0DC4
	.long	0x10D00DC4
	.long	0x10F10DC4
	.long	0x11120DC4
	.long	0x11330DC4
	.long	0x11540DC4
	.long	0x11750DC4
	.long	0x12ED6823

	.long	0x130D2023

	xxpermdi	33,45,46,0b00
	xxpermdi	34,37,46,0b00
	.long	0x13211023

	xxpermdi	34,38,37,0b00
	.long	0x13411023

	xxpermdi	34,39,38,0b00
	.long	0x13611023
	.long	0x136F7EE3

	xxpermdi	34,40,39,0b00
	.long	0x13811023
	.long	0x138F3723

	xxpermdi	34,41,40,0b00
	.long	0x13A11023
	xxpermdi	44,47,48,0b00
	xxpermdi	54,39,48,0b00
	.long	0x13ACB763

	xxpermdi	34,42,41,0b00
	.long	0x13C11023
	xxpermdi	54,40,39,0b00
	.long	0x13CCB7A3

	xxpermdi	34,43,42,0b00
	.long	0x13E11023
	xxpermdi	54,41,40,0b00
	.long	0x13ECB7E3
	.long	0x13F18FE3

	.long	0x13124623

	.long	0x13534EA3

	.long	0x13945723

	.long	0x13D55FA3

	mtvsrdd	33,9,8
	.long	0x11080DC4
	.long	0x11290DC4
	.long	0x114A0DC4
	.long	0x116B0DC4

	.long	0x13B45F63

	.long	0x13935F23

	xxpermdi	34,43,42,0b00
	xxpermdi	33,50,51,0b00
	.long	0x136116E3

	xxpermdi	33,49,50,0b00
	.long	0x134116A3

	xxpermdi	33,48,49,0b00
	.long	0x13211663
	.long	0x13324E63

	xxpermdi	33,47,48,0b00
	.long	0x13011623
	.long	0x13114E23

	xxpermdi	33,46,47,0b00
	.long	0x12E115E3
	xxpermdi	34,41,40,0b00
	xxpermdi	33,48,49,0b00
	.long	0x12E115E3

	stxv	55,0(3)
	stxv	56,16(3)
	stxv	57,32(3)
	stxv	58,48(3)
	stxv	59,64(3)
	stxv	60,80(3)
	stxv	61,96(3)
	stxv	62,112(3)
	stxv	63,128(3)

	ld	12,0(1)
	lxv	52,-16*12(12)
	lxv	53,-16*11(12)
	lxv	54,-16*10(12)
	lxv	55,-16*9(12)
	lxv	56,-16*8(12)
	lxv	57,-16*7(12)
	lxv	58,-16*6(12)
	lxv	59,-16*5(12)
	lxv	60,-16*4(12)
	lxv	61,-16*3(12)
	lxv	62,-16*2(12)
	lxv	63,-16*1(12)
	mr	1,12

	blr	
.size	.p521_felem_square,.-.p521_felem_square
.size	p521_felem_square,.-.p521_felem_square

