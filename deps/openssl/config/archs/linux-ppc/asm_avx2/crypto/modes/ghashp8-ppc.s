.machine	"any"

.text

.globl	gcm_init_p8
.type	gcm_init_p8,@function
.align	5
gcm_init_p8:
	li	0,-4096
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
	.long	0x7D202699

	vspltisb	8,-16
	vspltisb	5,1
	vaddubm	8,8,8
	vxor	4,4,4
	vor	8,8,5
	vsldoi	8,8,4,15
	vsldoi	6,4,5,1
	vaddubm	8,8,8
	vspltisb	7,7
	vor	8,8,6
	vspltb	6,9,0
	vsl	9,9,5
	vsrab	6,6,7
	vand	6,6,8
	vxor	3,9,6

	vsldoi	9,3,3,8
	vsldoi	8,4,8,8
	vsldoi	11,4,9,8
	vsldoi	10,9,4,8

	.long	0x7D001F99
	.long	0x7D681F99
	li	8,0x40
	.long	0x7D291F99
	li	9,0x50
	.long	0x7D4A1F99
	li	10,0x60

	.long	0x10035CC8
	.long	0x10234CC8
	.long	0x104354C8

	.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
	.long	0x100044C8
	vxor	6,6,2
	vxor	16,0,6

	vsldoi	17,16,16,8
	vsldoi	19,4,17,8
	vsldoi	18,17,4,8

	.long	0x7E681F99
	li	8,0x70
	.long	0x7E291F99
	li	9,0x80
	.long	0x7E4A1F99
	li	10,0x90
	.long	0x10039CC8
	.long	0x11B09CC8
	.long	0x10238CC8
	.long	0x11D08CC8
	.long	0x104394C8
	.long	0x11F094C8

	.long	0x10E044C8
	.long	0x114D44C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vsldoi	11,14,4,8
	vsldoi	9,4,14,8
	vxor	0,0,5
	vxor	2,2,6
	vxor	13,13,11
	vxor	15,15,9

	vsldoi	0,0,0,8
	vsldoi	13,13,13,8
	vxor	0,0,7
	vxor	13,13,10

	vsldoi	6,0,0,8
	vsldoi	9,13,13,8
	.long	0x100044C8
	.long	0x11AD44C8
	vxor	6,6,2
	vxor	9,9,15
	vxor	0,0,6
	vxor	13,13,9

	vsldoi	9,0,0,8
	vsldoi	17,13,13,8
	vsldoi	11,4,9,8
	vsldoi	10,9,4,8
	vsldoi	19,4,17,8
	vsldoi	18,17,4,8

	.long	0x7D681F99
	li	8,0xa0
	.long	0x7D291F99
	li	9,0xb0
	.long	0x7D4A1F99
	li	10,0xc0
	.long	0x7E681F99
	.long	0x7E291F99
	.long	0x7E4A1F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	gcm_init_p8,.-gcm_init_p8
.globl	gcm_gmult_p8
.type	gcm_gmult_p8,@function
.align	5
gcm_gmult_p8:
	lis	0,0xfff8
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
	.long	0x7C601E99

	.long	0x7D682699

	.long	0x7D292699

	.long	0x7D4A2699

	.long	0x7D002699

	vxor	4,4,4

	.long	0x10035CC8
	.long	0x10234CC8
	.long	0x104354C8

	.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
	.long	0x100044C8
	vxor	6,6,2
	vxor	0,0,6


	.long	0x7C001F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,2,0
.long	0
.size	gcm_gmult_p8,.-gcm_gmult_p8

.globl	gcm_ghash_p8
.type	gcm_ghash_p8,@function
.align	5
gcm_ghash_p8:
	li	0,-4096
	li	8,0x10
	mfspr	12,256
	li	9,0x20
	mtspr	256,0
	li	10,0x30
	.long	0x7C001E99

	.long	0x7D682699
	li	8,0x40

	.long	0x7D292699
	li	9,0x50

	.long	0x7D4A2699
	li	10,0x60

	.long	0x7D002699

	vxor	4,4,4

	cmplwi	6,64
	bge	.Lgcm_ghash_p8_4x

	.long	0x7C602E99
	addi	5,5,16
	subic.	6,6,16

	vxor	3,3,0
	beq	.Lshort

	.long	0x7E682699
	li	8,16
	.long	0x7E292699
	add	9,5,6
	.long	0x7E4A2699
	b	.Loop_2x

.align	5
.Loop_2x:
	.long	0x7E002E99


	subic	6,6,32
	.long	0x10039CC8
	.long	0x11B05CC8
	subfe	0,0,0
	.long	0x10238CC8
	.long	0x11D04CC8
	and	0,0,6
	.long	0x104394C8
	.long	0x11F054C8
	add	5,5,0

	vxor	0,0,13
	vxor	1,1,14

	.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	2,2,15
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7
	.long	0x7C682E99
	addi	5,5,32

	vsldoi	6,0,0,8
	.long	0x100044C8

	vxor	6,6,2
	vxor	3,3,6
	vxor	3,3,0
	.long	0x7c092840
	bgt	.Loop_2x

	cmplwi	6,0
	bne	.Leven

.Lshort:
	.long	0x10035CC8
	.long	0x10234CC8
	.long	0x104354C8

	.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
	.long	0x100044C8
	vxor	6,6,2

.Leven:
	vxor	0,0,6

	.long	0x7C001F99

	mtspr	256,12
	blr	
.long	0
.byte	0,12,0x14,0,0,0,4,0
.long	0
.align	5
.gcm_ghash_p8_4x:
.Lgcm_ghash_p8_4x:
	stwu	1,-232(1)
	li	10,39
	li	11,55
	stvx	20,10,1
	addi	10,10,32
	stvx	21,11,1
	addi	11,11,32
	stvx	22,10,1
	addi	10,10,32
	stvx	23,11,1
	addi	11,11,32
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
	li	10,0x60
	stvx	31,11,1
	li	0,-1
	stw	12,228(1)
	mtspr	256,0

	lvsl	5,0,8

	li	8,0x70
	.long	0x7E292699
	li	9,0x80
	vspltisb	6,8

	li	10,0x90
	.long	0x7EE82699
	li	8,0xa0
	.long	0x7F092699
	li	9,0xb0
	.long	0x7F2A2699
	li	10,0xc0
	.long	0x7FA82699
	li	8,0x10
	.long	0x7FC92699
	li	9,0x20
	.long	0x7FEA2699
	li	10,0x30

	vsldoi	7,4,6,8
	vaddubm	18,5,7
	vaddubm	19,6,18

	srwi	6,6,4

	.long	0x7C602E99
	.long	0x7E082E99
	subic.	6,6,8
	.long	0x7EC92E99
	.long	0x7F8A2E99
	addi	5,5,0x40





	vxor	2,3,0

	.long	0x11B0BCC8
	.long	0x11D0C4C8
	.long	0x11F0CCC8

	vperm	11,17,9,18
	vperm	5,22,28,19
	vperm	10,17,9,19
	vperm	6,22,28,18
	.long	0x12B68CC8
	.long	0x12855CC8
	.long	0x137C4CC8
	.long	0x134654C8

	vxor	21,21,14
	vxor	20,20,13
	vxor	27,27,21
	vxor	26,26,15

	blt	.Ltail_4x

.Loop_4x:
	.long	0x7C602E99
	.long	0x7E082E99
	subic.	6,6,4
	.long	0x7EC92E99
	.long	0x7F8A2E99
	addi	5,5,0x40





	.long	0x1002ECC8
	.long	0x1022F4C8
	.long	0x1042FCC8
	.long	0x11B0BCC8
	.long	0x11D0C4C8
	.long	0x11F0CCC8

	vxor	0,0,20
	vxor	1,1,27
	vxor	2,2,26
	vperm	5,22,28,19
	vperm	6,22,28,18

	.long	0x10E044C8
	.long	0x12855CC8
	.long	0x134654C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
	.long	0x12B68CC8
	.long	0x137C4CC8
	.long	0x100044C8

	vxor	20,20,13
	vxor	26,26,15
	vxor	2,2,3
	vxor	21,21,14
	vxor	2,2,6
	vxor	27,27,21
	vxor	2,2,0
	bge	.Loop_4x

.Ltail_4x:
	.long	0x1002ECC8
	.long	0x1022F4C8
	.long	0x1042FCC8

	vxor	0,0,20
	vxor	1,1,27

	.long	0x10E044C8

	vsldoi	5,1,4,8
	vsldoi	6,4,1,8
	vxor	2,2,26
	vxor	0,0,5
	vxor	2,2,6

	vsldoi	0,0,0,8
	vxor	0,0,7

	vsldoi	6,0,0,8
	.long	0x100044C8
	vxor	6,6,2
	vxor	0,0,6

	addic.	6,6,4
	beq	.Ldone_4x

	.long	0x7C602E99
	cmplwi	6,2
	li	6,-4
	blt	.Lone
	.long	0x7E082E99
	beq	.Ltwo

.Lthree:
	.long	0x7EC92E99




	vxor	2,3,0
	vor	29,23,23
	vor	30,24,24
	vor	31,25,25

	vperm	5,16,22,19
	vperm	6,16,22,18
	.long	0x12B08CC8
	.long	0x13764CC8
	.long	0x12855CC8
	.long	0x134654C8

	vxor	27,27,21
	b	.Ltail_4x

.align	4
.Ltwo:



	vxor	2,3,0
	vperm	5,4,16,19
	vperm	6,4,16,18

	vsldoi	29,4,17,8
	vor	30,17,17
	vsldoi	31,17,4,8

	.long	0x12855CC8
	.long	0x13704CC8
	.long	0x134654C8

	b	.Ltail_4x

.align	4
.Lone:


	vsldoi	29,4,9,8
	vor	30,9,9
	vsldoi	31,9,4,8

	vxor	2,3,0
	vxor	20,20,20
	vxor	27,27,27
	vxor	26,26,26

	b	.Ltail_4x

.Ldone_4x:

	.long	0x7C001F99

	li	10,39
	li	11,55
	mtspr	256,12
	lvx	20,10,1
	addi	10,10,32
	lvx	21,11,1
	addi	11,11,32
	lvx	22,10,1
	addi	10,10,32
	lvx	23,11,1
	addi	11,11,32
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
	addi	1,1,232
	blr	
.long	0
.byte	0,12,0x04,0,0x80,0,4,0
.long	0
.size	gcm_ghash_p8,.-gcm_ghash_p8

.byte	71,72,65,83,72,32,102,111,114,32,80,111,119,101,114,73,83,65,32,50,46,48,55,44,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2
