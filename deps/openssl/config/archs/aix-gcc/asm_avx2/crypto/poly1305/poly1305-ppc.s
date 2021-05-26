.machine	"any"
.csect	.text[PR],7
.globl	.poly1305_init_int
.align	4
.poly1305_init_int:
	xor	0,0,0
	stw	0,0(3)
	stw	0,4(3)
	stw	0,8(3)
	stw	0,12(3)
	stw	0,16(3)
	stw	0,24(3)

	cmplw	0,4,0
	beq-	Lno_key
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

Lno_key:
	xor	3,3,3
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0


.globl	.poly1305_blocks
.align	4
.poly1305_blocks:
Lpoly1305_blocks:
	srwi.	5,5,4
	beq-	Labort

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
	b	Loop

.align	4
Loop:
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

	bc	16,0,Loop

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
Labort:
	blr	
.long	0
.byte	0,12,4,1,0x80,18,4,0

.globl	.poly1305_emit
.align	5
.poly1305_emit:
	lwz	0,24(3)
	lwz	6,0(3)
	lwz	7,4(3)
	lwz	8,8(3)
	lwz	9,12(3)
	lwz	10,16(3)
	cmplwi	0,0
	beq	Lemit_base2_32

	slwi	11,7,26
	srwi	7,7,6
	slwi	12,8,20
	srwi	8,8,12
	addc	6,6,11
	slwi	11,9,14
	srwi	9,9,18
	adde	7,7,12
	slwi	12,10,8
	srwi	10,10,24
	adde	8,8,11
	adde	9,9,12
	addze	10,10

Lemit_base2_32:
	addic	0,6,5
	addze	0,7
	addze	0,8
	addze	0,9
	addze	0,10

	srwi	0,0,2
	neg	0,0
	andi.	0,0,5

	addc	6,6,0
	lwz	0,0(5)
	addze	7,7
	lwz	11,4(5)
	addze	8,8
	lwz	12,8(5)
	addze	9,9
	lwz	10,12(5)

	addc	6,6,0
	adde	7,7,11
	adde	8,8,12
	adde	9,9,10

	addi	3,4,-1
	addi	4,4,7

	stbu	6,1(3)
	srwi	6,6,8
	stbu	8,1(4)
	srwi	8,8,8

	stbu	6,1(3)
	srwi	6,6,8
	stbu	8,1(4)
	srwi	8,8,8

	stbu	6,1(3)
	srwi	6,6,8
	stbu	8,1(4)
	srwi	8,8,8

	stbu	6,1(3)
	stbu	8,1(4)

	stbu	7,1(3)
	srwi	7,7,8
	stbu	9,1(4)
	srwi	9,9,8

	stbu	7,1(3)
	srwi	7,7,8
	stbu	9,1(4)
	srwi	9,9,8

	stbu	7,1(3)
	srwi	7,7,8
	stbu	9,1(4)
	srwi	9,9,8

	stbu	7,1(3)
	stbu	9,1(4)

	blr	
.long	0
.byte	0,12,0x14,0,0,0,3,0

.globl	.poly1305_blocks_vsx
.align	5
.poly1305_blocks_vsx:
	lwz	7,24(3)
	cmplwi	5,128
	bge	__poly1305_blocks_vsx
	cmplwi	7,0
	beq	Lpoly1305_blocks

	lwz	7,0(3)
	lwz	8,4(3)
	lwz	9,8(3)
	lwz	10,12(3)
	lwz	11,16(3)

	slwi	0,8,26
	srwi	8,8,6
	slwi	12,9,20
	srwi	9,9,12
	addc	7,7,0
	slwi	0,10,14
	srwi	10,10,18
	adde	8,8,12
	slwi	12,11,8
	srwi	11,11,24
	adde	9,9,0
	li	0,0
	adde	10,10,12
	addze	11,11

	stw	7,0(3)
	stw	8,4(3)
	stw	9,8(3)
	stw	10,12(3)
	stw	11,16(3)
	stw	0,24(3)

	b	Lpoly1305_blocks
.long	0
.byte	0,12,0x14,0,0,0,4,0


.align	5
__poly1305_mul:
	.long	0x11E05088
	.long	0x12015088
	.long	0x12225088
	.long	0x12435088
	.long	0x12645088

	.long	0x12846088
	.long	0x11EFA0C0
	.long	0x12805888
	.long	0x1210A0C0
	.long	0x12815888
	.long	0x1231A0C0
	.long	0x12825888
	.long	0x1252A0C0
	.long	0x12835888
	.long	0x1273A0C0

	.long	0x12837088
	.long	0x11EFA0C0
	.long	0x12847088
	.long	0x1210A0C0
	.long	0x12806888
	.long	0x1231A0C0
	.long	0x12816888
	.long	0x1252A0C0
	.long	0x12826888
	.long	0x1273A0C0

	.long	0x12823888
	.long	0x11EFA0C0
	.long	0x12833888
	.long	0x1210A0C0
	.long	0x12843888
	.long	0x1231A0C0
	.long	0x12803088
	.long	0x1252A0C0
	.long	0x12813088
	.long	0x1273A0C0

	.long	0x12814888
	.long	0x11EFA0C0
	.long	0x12824888
	.long	0x1210A0C0
	.long	0x12834888
	.long	0x1231A0C0
	.long	0x12844888
	.long	0x1252A0C0
	.long	0x12804088
	.long	0x1273A0C0




	vspltisb	20,2
	.long	0x1092CEC4
	.long	0x102FCEC4
	vand	3,18,29
	vand	0,15,29
	.long	0x108498C0
	.long	0x102180C0

	.long	0x1264CEC4
	.long	0x1201CEC4
	vand	4,4,29
	vand	1,1,29
	.long	0x100098C0
	.long	0x105180C0

	.long	0x1273A5C4
	.long	0x1222CEC4
	vand	2,2,29
	.long	0x100098C0
	.long	0x106388C0

	.long	0x11E0CEC4
	.long	0x1243CEC4
	vand	0,0,29
	vand	3,3,29
	.long	0x102178C0
	.long	0x108490C0

	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0


.align	5
__poly1305_blocks_vsx:
	stwu	1,-384(1)
	mflr	0
	li	10,167
	li	11,183
	li	12,-1
	stvx	20,10,1
	addi	10,10,32
	stvx	21,11,1
	addi	11,11,32
	stvx	22,10,1
	addi	10,10,32
	stvx	23,10,1
	addi	10,10,32
	stvx	24,11,1
	addi	11,11,32
	stvx	25,10,1
	addi	10,10,32
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
	stw	12,360(1)
	li	12,-1
	or	12,12,12
	stw	27,364(1)
	stw	28,368(1)
	stw	29,372(1)
	stw	30,376(1)
	stw	31,380(1)
	stw	0,388(1)

	bl	LPICmeup

	li	27,0x10
	li	28,0x20
	li	29,0x30
	li	30,0x40
	li	31,0x50
	.long	0x7FA06699
	.long	0x7F3B6699
	.long	0x7F7C6699
	.long	0x7FFD6699
	.long	0x7FDE6699

	cmplwi	7,0
	bne	Lskip_init_vsx

	lwz	8,32(3)
	lwz	9,36(3)
	lwz	10,40(3)
	lwz	11,44(3)

	extrwi	7,8,26,6
	extrwi	8,8,6,0
	insrwi	8,9,20,6
	extrwi	9,9,12,0
	insrwi	9,10,14,6
	extrwi	10,10,18,0
	insrwi	10,11,8,6
	extrwi	11,11,24,0

	.long	0x7D4701E7
	slwi	7,8,2
	.long	0x7D6801E7
	add	8,8,7
	.long	0x7D8801E7
	slwi	8,9,2
	.long	0x7DA901E7
	add	9,9,8
	.long	0x7DC901E7
	slwi	9,10,2
	.long	0x7CCA01E7
	add	10,10,9
	.long	0x7CEA01E7
	slwi	10,11,2
	.long	0x7D0B01E7
	add	11,11,10
	.long	0x7D2B01E7

	vor	0,10,10
	vor	1,11,11
	vor	2,13,13
	vor	3,6,6
	vor	4,8,8

	bl	__poly1305_mul

	.long	0xF1405057
	.long	0xF1615857
	.long	0xF1A26857
	.long	0xF0C33057
	.long	0xF1044057
	.long	0xF0000057
	.long	0xF0210857
	.long	0xF0421057
	.long	0xF0631857
	.long	0xF0842057
	.long	0x118BA5C4
	.long	0x11CDA5C4
	.long	0x10E6A5C4
	.long	0x1128A5C4
	.long	0x118C58C0
	.long	0x11CE68C0
	.long	0x10E730C0
	.long	0x112940C0

	bl	__poly1305_mul

	addi	7,3,0x60
	lwz	8,0(3)
	lwz	9,4(3)
	lwz	10,8(3)
	lwz	11,12(3)
	lwz	0,16(3)

	.long	0x114A068C
	.long	0x116B0E8C
	.long	0x11AD168C
	.long	0x10C61E8C
	.long	0x1108268C
	vslw	12,11,20
	vslw	14,13,20
	vslw	7,6,20
	vslw	9,8,20
	vadduwm	12,12,11
	vadduwm	14,14,13
	vadduwm	7,7,6
	vadduwm	9,9,8

	.long	0x7D5D1F99
	.long	0x7D7E1F99
	.long	0x7D9F1F99
	.long	0x7DA03F99
	.long	0x7DDB3F99
	.long	0x7CDC3F99
	.long	0x7CFD3F99
	.long	0x7D1E3F99
	.long	0x7D3F3F99

	extrwi	7,8,26,6
	extrwi	8,8,6,0
	.long	0x7C0701E7
	insrwi	8,9,20,6
	extrwi	9,9,12,0
	.long	0x7C2801E7
	insrwi	9,10,14,6
	extrwi	10,10,18,0
	.long	0x7C4901E7
	insrwi	10,11,8,6
	extrwi	11,11,24,0
	.long	0x7C6A01E7
	insrwi	11,0,3,5
	.long	0x7C8B01E7
	li	0,1
	stw	0,24(3)
	b	Loaded_vsx

.align	4
Lskip_init_vsx:
	li	27,4
	li	28,8
	li	29,12
	li	30,16
	.long	0x7C001819
	.long	0x7C3B1819
	.long	0x7C5C1819
	.long	0x7C7D1819
	.long	0x7C9E1819

Loaded_vsx:
	li	27,0x10
	li	28,0x20
	li	29,0x30
	li	30,0x40
	li	31,0x50
	li	7,0x60
	li	8,0x70
	addi	10,3,64
	addi	11,1,39

	vxor	20,20,20
	.long	0xF000A057
	.long	0xF021A057
	.long	0xF042A057
	.long	0xF063A057
	.long	0xF084A057

	.long	0x7F5F6699
	.long	0x7EA02699
	.long	0x7EDB2699
	.long	0x7EFC2699
	.long	0x7F1D2699
	vperm	21,21,21,26
	vperm	22,22,22,26
	vperm	23,23,23,26
	vperm	24,24,24,26

	.long	0xF0B5B057
	vspltisb	26,4
	vperm	7,21,22,31
	vspltisb	28,14
	.long	0xF115B357

	.long	0x10C5CEC4
	.long	0x10E7D6C4
	.long	0x1128DEC4
	.long	0x1108E6C4
	vand	5,5,29
	vand	6,6,29
	vand	7,7,29
	vand	8,8,29

	.long	0xF2B7C057
	vperm	22,23,24,31
	.long	0xF2F7C357

	.long	0x1295CEC4
	.long	0x12D6D6C4
	.long	0x1317DEC4
	.long	0x12F7E6C4
	vand	21,21,29
	vand	20,20,29
	vand	22,22,29
	vand	23,23,29


	.long	0x11384E8C
	.long	0x10B52E8C
	.long	0x10D4368C
	.long	0x10F63E8C
	.long	0x1117468C
	vor	9,9,30

	.long	0x7D5D1A99
	.long	0x7D605299
	.long	0x7D9B5299
	.long	0x7DBC5299
	.long	0x7DDD5299
	.long	0x7EBE5299
	.long	0x7EDF5299
	.long	0x7EE75299
	.long	0x7F085299
	stvx	11,0,11
	stvx	12,27,11
	stvx	13,28,11
	stvx	14,29,11
	stvx	21,30,11
	stvx	22,31,11
	stvx	23,7,11
	stvx	24,8,11

	addi	4,4,0x40
	addi	12,12,0x50
	addi	0,5,-64
	srdi	0,0,6
	mtctr	0
	b	Loop_vsx

.align	4
Loop_vsx:














	.long	0x11E55288
	.long	0x12055A88
	.long	0x12256A88
	.long	0x12466A88

	.long	0x12865288
	.long	0x1210A0C0
	.long	0x12865A88
	.long	0x1231A0C0
	.long	0x12676A88
	.long	0x12896288
	.long	0x11EFA0C0
	.long	0x12875A88
	.long	0x1252A0C0
	lvx	12,31,11
	.long	0x12885A88
	.long	0x1273A0C0
	lvx	11,30,11

	.long	0x104238C0
	.long	0x100028C0
	.long	0x106340C0
	.long	0x102130C0
	.long	0x108448C0

	.long	0x12887288
	.long	0x11EFA0C0
	.long	0x12897288
	.long	0x1210A0C0
	.long	0x12875288
	.long	0x1231A0C0
	.long	0x12885288
	.long	0x1252A0C0
	lvx	14,8,11
	.long	0x12895288
	.long	0x1273A0C0
	lvx	13,7,11

	.long	0x12876288
	.long	0x11EFA0C0
	.long	0x12886288
	.long	0x1210A0C0
	.long	0x12896288
	.long	0x1231A0C0
	.long	0x12855A88
	.long	0x1252A0C0
	.long	0x12865A88
	.long	0x1273A0C0

	.long	0x7F406699
	.long	0x7EA02699
	.long	0x7EDB2699
	.long	0x7EFC2699
	.long	0x7F1D2699
	vperm	21,21,21,26
	vperm	22,22,22,26
	vperm	23,23,23,26
	vperm	24,24,24,26

	.long	0x12867288
	.long	0x11EFA0C0
	.long	0x12877288
	.long	0x1210A0C0
	.long	0x12887288
	.long	0x1231A0C0
	.long	0x12897288
	.long	0x1252A0C0
	.long	0x12856A88
	.long	0x1273A0C0

	.long	0xF0B5B057
	vspltisb	26,4
	vperm	7,21,22,31
	.long	0xF115B357


	.long	0x12805088
	.long	0x11EFA0C0
	.long	0x12815088
	.long	0x1210A0C0
	.long	0x12825088
	.long	0x1231A0C0
	.long	0x12835088
	.long	0x1252A0C0
	.long	0x12845088
	.long	0x1273A0C0

	.long	0xF2B7C057
	vperm	22,23,24,31
	.long	0xF2F7C357

	.long	0x12826088
	.long	0x11EFA0C0
	.long	0x12836088
	.long	0x1210A0C0
	.long	0x12846088
	.long	0x1231A0C0
	.long	0x12805888
	.long	0x1252A0C0
	lvx	12,27,11
	.long	0x12815888
	.long	0x1273A0C0
	lvx	11,0,11

	.long	0x10C5CEC4
	.long	0x10E7D6C4
	.long	0x1128DEC4
	.long	0x1108E6C4

	.long	0x12817088
	.long	0x11EFA0C0
	.long	0x12827088
	.long	0x1210A0C0
	.long	0x12837088
	.long	0x1231A0C0
	.long	0x12847088
	.long	0x1252A0C0
	lvx	14,29,11
	.long	0x12806888
	.long	0x1273A0C0
	lvx	13,28,11

	vand	5,5,29
	vand	6,6,29
	vand	7,7,29
	vand	8,8,29

	.long	0x12846088
	.long	0x11EFA0C0
	.long	0x12805888
	.long	0x1210A0C0
	.long	0x12815888
	.long	0x1231A0C0
	.long	0x12825888
	.long	0x1252A0C0
	.long	0x12835888
	.long	0x1273A0C0

	.long	0x12D6D6C4
	.long	0x1355CEC4
	.long	0x1317DEC4
	.long	0x12F7E6C4

	.long	0x12837088
	.long	0x11EFA0C0
	.long	0x12847088
	.long	0x1210A0C0
	.long	0x12806888
	.long	0x1231A0C0
	.long	0x12816888
	.long	0x1252A0C0
	.long	0x12826888
	.long	0x1273A0C0

	vand	21,21,29
	vand	26,26,29
	vand	22,22,29
	vand	23,23,29





	vspltisb	20,2
	.long	0x1092CEC4
	.long	0x102FCEC4
	vand	3,18,29
	vand	0,15,29
	.long	0x108498C0
	.long	0x102180C0

	.long	0x11384E8C
	.long	0x10B52E8C
	.long	0x10DA368C
	.long	0x10F63E8C
	.long	0x1117468C
	vor	9,9,30

	.long	0x1264CEC4
	.long	0x1201CEC4
	vand	4,4,29
	vand	1,1,29
	.long	0x100098C0
	.long	0x105180C0

	.long	0x1273A5C4
	.long	0x1222CEC4
	vand	2,2,29
	.long	0x100098C0
	.long	0x106388C0

	.long	0x11E0CEC4
	.long	0x1243CEC4
	vand	0,0,29
	vand	3,3,29
	.long	0x102178C0
	.long	0x108490C0

	addi	4,4,0x40
	bc	16,0,Loop_vsx

	neg	5,5
	andi.	5,5,0x30
	sub	4,4,5

	.long	0x7D5D1E99
	.long	0x7D605699
	.long	0x7D9B5699
	.long	0x7DBC5699
	.long	0x7DDD5699

Last_vsx:
	.long	0x11E55288
	.long	0x12065288
	.long	0x12275288
	.long	0x12485288
	.long	0x12695288

	.long	0x12896288
	.long	0x11EFA0C0
	.long	0x12855A88
	.long	0x1210A0C0
	.long	0x12865A88
	.long	0x1231A0C0
	.long	0x12875A88
	.long	0x1252A0C0
	.long	0x7D9F5699
	.long	0x12885A88
	.long	0x1273A0C0
	.long	0x7D7E5699

	.long	0x104238C0
	.long	0x100028C0
	.long	0x106340C0
	.long	0x102130C0
	.long	0x108448C0

	.long	0x12887288
	.long	0x11EFA0C0
	.long	0x12897288
	.long	0x1210A0C0
	.long	0x12856A88
	.long	0x1231A0C0
	.long	0x12866A88
	.long	0x1252A0C0
	.long	0x7DC85699
	.long	0x12876A88
	.long	0x1273A0C0
	.long	0x7DA75699

	.long	0x12876288
	.long	0x11EFA0C0
	.long	0x12886288
	.long	0x1210A0C0
	.long	0x12896288
	.long	0x1231A0C0
	.long	0x12855A88
	.long	0x1252A0C0
	.long	0x12865A88
	.long	0x1273A0C0

	.long	0x12867288
	.long	0x11EFA0C0
	.long	0x12877288
	.long	0x1210A0C0
	.long	0x12887288
	.long	0x1231A0C0
	.long	0x12897288
	.long	0x1252A0C0
	.long	0x12856A88
	.long	0x1273A0C0


	.long	0x12805088
	.long	0x11EFA0C0
	.long	0x12815088
	.long	0x1210A0C0
	.long	0x12825088
	.long	0x1231A0C0
	.long	0x12835088
	.long	0x1252A0C0
	.long	0x12845088
	.long	0x1273A0C0

	.long	0x12826088
	.long	0x11EFA0C0
	.long	0x12836088
	.long	0x1210A0C0
	.long	0x12846088
	.long	0x1231A0C0
	.long	0x12805888
	.long	0x1252A0C0
	.long	0x7D9B5699
	.long	0x12815888
	.long	0x1273A0C0
	.long	0x7D605699

	.long	0x12817088
	.long	0x11EFA0C0
	.long	0x12827088
	.long	0x1210A0C0
	.long	0x12837088
	.long	0x1231A0C0
	.long	0x12847088
	.long	0x1252A0C0
	.long	0x7DDD5699
	.long	0x12806888
	.long	0x1273A0C0
	.long	0x7DBC5699

	.long	0x12846088
	.long	0x11EFA0C0
	.long	0x12805888
	.long	0x1210A0C0
	.long	0x12815888
	.long	0x1231A0C0
	.long	0x12825888
	.long	0x1252A0C0
	.long	0x12835888
	.long	0x1273A0C0

	.long	0x12837088
	.long	0x11EFA0C0
	.long	0x12847088
	.long	0x1210A0C0
	.long	0x12806888
	.long	0x1231A0C0
	.long	0x12816888
	.long	0x1252A0C0
	.long	0x12826888
	.long	0x1273A0C0




	.long	0xF00F7A57
	.long	0xF0308257
	.long	0xF0518A57
	.long	0xF0729257
	.long	0xF0939A57
	.long	0x11EF00C0
	.long	0x121008C0
	.long	0x123110C0
	.long	0x125218C0
	.long	0x127320C0




	vspltisb	20,2
	.long	0x1092CEC4
	.long	0x102FCEC4
	vand	3,18,29
	vand	0,15,29
	.long	0x108498C0
	.long	0x102180C0

	.long	0x1264CEC4
	.long	0x1201CEC4
	vand	4,4,29
	vand	1,1,29
	.long	0x100098C0
	.long	0x105180C0

	.long	0x1273A5C4
	.long	0x1222CEC4
	vand	2,2,29
	.long	0x100098C0
	.long	0x106388C0

	.long	0x11E0CEC4
	.long	0x1243CEC4
	vand	0,0,29
	vand	3,3,29
	.long	0x102178C0
	.long	0x108490C0

	beq	Ldone_vsx

	add	6,12,5

	.long	0x7F406699
	.long	0x7EA02699
	.long	0x7EDB2699
	.long	0x7EFC2699
	.long	0x7F1D2699
	vperm	21,21,21,26
	vperm	22,22,22,26
	vperm	23,23,23,26
	vperm	24,24,24,26

	.long	0xF0B5B057
	vspltisb	26,4
	vperm	7,21,22,31
	.long	0xF115B357

	.long	0x10C5CEC4
	.long	0x10E7D6C4
	.long	0x1128DEC4
	.long	0x1108E6C4
	vand	5,5,29
	vand	6,6,29
	vand	7,7,29
	vand	8,8,29

	.long	0xF297C057
	vperm	21,23,24,31
	.long	0xF2D7C357

	.long	0x7DE03699
	.long	0x7E1D3699

	.long	0x12F4CEC4
	.long	0x12B5D6C4
	.long	0x1316DEC4
	.long	0x12D6E6C4
	vand	20,20,29
	vand	23,23,29
	vand	21,21,29
	vand	22,22,29


	.long	0x11384E8C
	.long	0x10B42E8C
	.long	0x10D7368C
	.long	0x10F53E8C
	.long	0x1116468C
	vor	9,9,30

	vperm	0,0,0,15
	vand	5,5,    16
	vperm	1,1,1,15
	vand	6,6,    16
	vperm	2,2,2,15
	vand	7,7,    16
	vperm	3,3,3,15
	vand	8,8,    16
	vperm	4,4,4,15
	vand	9,9,    16

	.long	0x10A500C0
	vxor	0,0,0
	.long	0x10C608C0
	vxor	1,1,1
	.long	0x10E710C0
	vxor	2,2,2
	.long	0x110818C0
	vxor	3,3,3
	.long	0x112920C0
	vxor	4,4,4

	xor.	5,5,5
	b	Last_vsx

.align	4
Ldone_vsx:
	lwz	0,388(1)
	li	27,4
	li	28,8
	li	29,12
	li	30,16
	.long	0x7C001919
	.long	0x7C3B1919
	.long	0x7C5C1919
	.long	0x7C7D1919
	.long	0x7C9E1919

	lwz	12,360(1)
	mtlr	0
	li	10,167
	li	11,183
	or	12,12,12
	lvx	20,10,1
	addi	10,10,32
	lvx	21,10,1
	addi	10,10,32
	lvx	22,11,1
	addi	11,11,32
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
	lwz	27,364(1)
	lwz	28,368(1)
	lwz	29,372(1)
	lwz	30,376(1)
	lwz	31,380(1)
	addi	1,1,384
	blr	
.long	0
.byte	0,12,0x04,1,0x80,5,4,0
.long	0


.align	6
LPICmeup:
	mflr	0
	bcl	20,31,$+4
	mflr	12
	addi	12,12,56
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.space	28

.long	0x00000000,0x03ffffff
.long	0x00000000,0x03ffffff
.long	0x00000000,0x0000001a
.long	0x00000000,0x0000001a
.long	0x00000000,0x00000028
.long	0x00000000,0x00000028
.long	0x00000000,0x0e0f0001
.long	0x00000000,0x1e1f1011
.long	0x01000000,0x01000000
.long	0x01000000,0x01000000
.long	0x07060504,0x03020100
.long	0x0f0e0d0c,0x0b0a0908

.long	0x00000000,0x00000000
.long	0x00000000,0x04050607
.long	0x04050607,0x00000000
.long	0x00000000,0x00000000
.long	0x00000000,0x00000000
.long	0x04050607,0x00000000

.long	0xffffffff,0x00000000
.long	0xffffffff,0xffffffff
.long	0xffffffff,0x00000000
.long	0xffffffff,0x00000000
.long	0x00000000,0x00000000
.long	0xffffffff,0x00000000
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,80,80,67,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,64,100,111,116,45,97,115,109,0
.align	2
