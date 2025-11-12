.machine	"any"
.abiversion	2
.text





.macro	.Loop_aes_middle4x
	xxlor	19+32, 1, 1
	xxlor	20+32, 2, 2
	xxlor	21+32, 3, 3
	xxlor	22+32, 4, 4

	.long	0x11EF9D08
	.long	0x12109D08
	.long	0x12319D08
	.long	0x12529D08

	.long	0x11EFA508
	.long	0x1210A508
	.long	0x1231A508
	.long	0x1252A508

	.long	0x11EFAD08
	.long	0x1210AD08
	.long	0x1231AD08
	.long	0x1252AD08

	.long	0x11EFB508
	.long	0x1210B508
	.long	0x1231B508
	.long	0x1252B508

	xxlor	19+32, 5, 5
	xxlor	20+32, 6, 6
	xxlor	21+32, 7, 7
	xxlor	22+32, 8, 8

	.long	0x11EF9D08
	.long	0x12109D08
	.long	0x12319D08
	.long	0x12529D08

	.long	0x11EFA508
	.long	0x1210A508
	.long	0x1231A508
	.long	0x1252A508

	.long	0x11EFAD08
	.long	0x1210AD08
	.long	0x1231AD08
	.long	0x1252AD08

	.long	0x11EFB508
	.long	0x1210B508
	.long	0x1231B508
	.long	0x1252B508

	xxlor	23+32, 9, 9
	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
.endm	





.macro	.Loop_aes_middle8x
	xxlor	23+32, 1, 1
	xxlor	24+32, 2, 2
	xxlor	25+32, 3, 3
	xxlor	26+32, 4, 4

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	.long	0x11EFCD08
	.long	0x1210CD08
	.long	0x1231CD08
	.long	0x1252CD08
	.long	0x1273CD08
	.long	0x1294CD08
	.long	0x12B5CD08
	.long	0x12D6CD08

	.long	0x11EFD508
	.long	0x1210D508
	.long	0x1231D508
	.long	0x1252D508
	.long	0x1273D508
	.long	0x1294D508
	.long	0x12B5D508
	.long	0x12D6D508

	xxlor	23+32, 5, 5
	xxlor	24+32, 6, 6
	xxlor	25+32, 7, 7
	xxlor	26+32, 8, 8

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	.long	0x11EFCD08
	.long	0x1210CD08
	.long	0x1231CD08
	.long	0x1252CD08
	.long	0x1273CD08
	.long	0x1294CD08
	.long	0x12B5CD08
	.long	0x12D6CD08

	.long	0x11EFD508
	.long	0x1210D508
	.long	0x1231D508
	.long	0x1252D508
	.long	0x1273D508
	.long	0x1294D508
	.long	0x12B5D508
	.long	0x12D6D508

	xxlor	23+32, 9, 9
	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08
.endm	




ppc_aes_gcm_ghash:
	vxor	15, 15, 0

	xxlxor	29, 29, 29

	.long	0x12EC7CC8
	.long	0x130984C8
	.long	0x13268CC8
	.long	0x134394C8

	vxor	23, 23, 24
	vxor	23, 23, 25
	vxor	23, 23, 26

	.long	0x130D7CC8
	.long	0x132A84C8
	.long	0x13478CC8
	.long	0x136494C8

	vxor	24, 24, 25
	vxor	24, 24, 26
	vxor	24, 24, 27


	.long	0x139714C8

	xxlor	29+32, 29, 29
	vsldoi	26, 24, 29, 8
	vsldoi	29, 29, 24, 8
	vxor	23, 23, 26

	vsldoi	23, 23, 23, 8
	vxor	23, 23, 28

	.long	0x130E7CC8
	.long	0x132B84C8
	.long	0x13488CC8
	.long	0x136594C8

	vxor	24, 24, 25
	vxor	24, 24, 26
	vxor	24, 24, 27

	vxor	24, 24, 29


	vsldoi	27, 23, 23, 8
	.long	0x12F714C8
	vxor	27, 27, 24
	vxor	23, 23, 27

	xxlor	32, 23+32, 23+32

	blr	





.macro	ppc_aes_gcm_ghash2_4x

	vxor	15, 15, 0

	xxlxor	29, 29, 29

	.long	0x12EC7CC8
	.long	0x130984C8
	.long	0x13268CC8
	.long	0x134394C8

	vxor	23, 23, 24
	vxor	23, 23, 25
	vxor	23, 23, 26

	.long	0x130D7CC8
	.long	0x132A84C8
	.long	0x13478CC8
	.long	0x136494C8

	vxor	24, 24, 25
	vxor	24, 24, 26


	.long	0x139714C8

	xxlor	29+32, 29, 29

	vxor	24, 24, 27
	vsldoi	26, 24, 29, 8
	vsldoi	29, 29, 24, 8
	vxor	23, 23, 26

	vsldoi	23, 23, 23, 8
	vxor	23, 23, 28

	.long	0x130E7CC8
	.long	0x132B84C8
	.long	0x13488CC8
	.long	0x136594C8

	vxor	24, 24, 25
	vxor	24, 24, 26
	vxor	24, 24, 27

	vxor	24, 24, 29


	vsldoi	27, 23, 23, 8
	.long	0x12F714C8
	vxor	27, 27, 24
	vxor	27, 23, 27


	.long	0x1309A4C8
	.long	0x1326ACC8
	.long	0x1343B4C8
	vxor	19, 19, 27
	.long	0x12EC9CC8

	vxor	23, 23, 24
	vxor	23, 23, 25
	vxor	23, 23, 26

	.long	0x130D9CC8
	.long	0x132AA4C8
	.long	0x1347ACC8
	.long	0x1364B4C8

	vxor	24, 24, 25
	vxor	24, 24, 26


	.long	0x139714C8

	xxlor	29+32, 29, 29

	vxor	24, 24, 27
	vsldoi	26, 24, 29, 8
	vsldoi	29, 29, 24, 8
	vxor	23, 23, 26

	vsldoi	23, 23, 23, 8
	vxor	23, 23, 28

	.long	0x130E9CC8
	.long	0x132BA4C8
	.long	0x1348ACC8
	.long	0x1365B4C8

	vxor	24, 24, 25
	vxor	24, 24, 26
	vxor	24, 24, 27

	vxor	24, 24, 29


	vsldoi	27, 23, 23, 8
	.long	0x12F714C8
	vxor	27, 27, 24
	vxor	23, 23, 27

	xxlor	32, 23+32, 23+32

.endm	




.macro	ppc_update_hash_1x
	vxor	28, 28, 0

	vxor	19, 19, 19

	.long	0x12C3E4C8
	.long	0x12E4E4C8
	.long	0x1305E4C8

	.long	0x137614C8

	vsldoi	25, 23, 19, 8
	vsldoi	26, 19, 23, 8
	vxor	22, 22, 25
	vxor	24, 24, 26

	vsldoi	22, 22, 22, 8
	vxor	22, 22, 27

	vsldoi	20, 22, 22, 8
	.long	0x12D614C8
	vxor	20, 20, 24
	vxor	22, 22, 20

	vor	0,22,22

.endm	













.global	ppc_aes_gcm_encrypt
.align	5
ppc_aes_gcm_encrypt:
_ppc_aes_gcm_encrypt:

	stdu	1,-512(1)
	mflr	0

	std	14,112(1)
	std	15,120(1)
	std	16,128(1)
	std	17,136(1)
	std	18,144(1)
	std	19,152(1)
	std	20,160(1)
	std	21,168(1)
	li	9, 256
	stvx	20, 9, 1
	addi	9, 9, 16
	stvx	21, 9, 1
	addi	9, 9, 16
	stvx	22, 9, 1
	addi	9, 9, 16
	stvx	23, 9, 1
	addi	9, 9, 16
	stvx	24, 9, 1
	addi	9, 9, 16
	stvx	25, 9, 1
	addi	9, 9, 16
	stvx	26, 9, 1
	addi	9, 9, 16
	stvx	27, 9, 1
	addi	9, 9, 16
	stvx	28, 9, 1
	addi	9, 9, 16
	stvx	29, 9, 1
	addi	9, 9, 16
	stvx	30, 9, 1
	addi	9, 9, 16
	stvx	31, 9, 1
	std	0, 528(1)


	lxvb16x	32, 0, 8


	li	10, 32
	lxvd2x	2+32, 10, 8
	li	10, 48
	lxvd2x	3+32, 10, 8
	li	10, 64
	lxvd2x	4+32, 10, 8
	li	10, 80
	lxvd2x	5+32, 10, 8

	li	10, 96
	lxvd2x	6+32, 10, 8
	li	10, 112
	lxvd2x	7+32, 10, 8
	li	10, 128
	lxvd2x	8+32, 10, 8

	li	10, 144
	lxvd2x	9+32, 10, 8
	li	10, 160
	lxvd2x	10+32, 10, 8
	li	10, 176
	lxvd2x	11+32, 10, 8

	li	10, 192
	lxvd2x	12+32, 10, 8
	li	10, 208
	lxvd2x	13+32, 10, 8
	li	10, 224
	lxvd2x	14+32, 10, 8


	lxvb16x	30+32, 0, 7

	mr	12, 5
	li	11, 0


	vxor	31, 31, 31
	vspltisb	22,1
	vsldoi	31, 31, 22,1


	lxv	0, 0(6)
	lxv	1, 0x10(6)
	lxv	2, 0x20(6)
	lxv	3, 0x30(6)
	lxv	4, 0x40(6)
	lxv	5, 0x50(6)
	lxv	6, 0x60(6)
	lxv	7, 0x70(6)
	lxv	8, 0x80(6)
	lxv	9, 0x90(6)
	lxv	10, 0xa0(6)


	lwz	9,240(6)



	xxlor	32+29, 0, 0
	vxor	15, 30, 29

	cmpdi	9, 10
	beq	.Loop_aes_gcm_8x


	lxv	11, 0xb0(6)
	lxv	12, 0xc0(6)

	cmpdi	9, 12
	beq	.Loop_aes_gcm_8x


	lxv	13, 0xd0(6)
	lxv	14, 0xe0(6)
	cmpdi	9, 14
	beq	.Loop_aes_gcm_8x

	b	aes_gcm_out

.align	5
.Loop_aes_gcm_8x:
	mr	14, 3
	mr	9, 4


	li	10, 128
	divdu	10, 5, 10
	cmpdi	10, 0
	beq	.Loop_last_block

	.long	0x13DEF8C0
	vxor	16, 30, 29
	.long	0x13DEF8C0
	vxor	17, 30, 29
	.long	0x13DEF8C0
	vxor	18, 30, 29
	.long	0x13DEF8C0
	vxor	19, 30, 29
	.long	0x13DEF8C0
	vxor	20, 30, 29
	.long	0x13DEF8C0
	vxor	21, 30, 29
	.long	0x13DEF8C0
	vxor	22, 30, 29

	mtctr	10

	li	15, 16
	li	16, 32
	li	17, 48
	li	18, 64
	li	19, 80
	li	20, 96
	li	21, 112

	lwz	10, 240(6)

.Loop_8x_block:

	lxvb16x	15, 0, 14
	lxvb16x	16, 15, 14
	lxvb16x	17, 16, 14
	lxvb16x	18, 17, 14
	lxvb16x	19, 18, 14
	lxvb16x	20, 19, 14
	lxvb16x	21, 20, 14
	lxvb16x	22, 21, 14
	addi	14, 14, 128

.Loop_aes_middle8x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_next_ghash


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_next_ghash


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_next_ghash
	b	aes_gcm_out

Do_next_ghash:



	.long	0x11EFBD09
	.long	0x1210BD09

	xxlxor	47, 47, 15
	stxvb16x	47, 0, 9
	xxlxor	48, 48, 16
	stxvb16x	48, 15, 9

	.long	0x1231BD09
	.long	0x1252BD09

	xxlxor	49, 49, 17
	stxvb16x	49, 16, 9
	xxlxor	50, 50, 18
	stxvb16x	50, 17, 9

	.long	0x1273BD09
	.long	0x1294BD09

	xxlxor	51, 51, 19
	stxvb16x	51, 18, 9
	xxlxor	52, 52, 20
	stxvb16x	52, 19, 9

	.long	0x12B5BD09
	.long	0x12D6BD09

	xxlxor	53, 53, 21
	stxvb16x	53, 20, 9
	xxlxor	54, 54, 22
	stxvb16x	54, 21, 9

	addi	9, 9, 128


	ppc_aes_gcm_ghash2_4x	

	xxlor	27+32, 0, 0
	.long	0x13DEF8C0
	vor	29,30,30
	vxor	15, 30, 27
	.long	0x13DEF8C0
	vxor	16, 30, 27
	.long	0x13DEF8C0
	vxor	17, 30, 27
	.long	0x13DEF8C0
	vxor	18, 30, 27
	.long	0x13DEF8C0
	vxor	19, 30, 27
	.long	0x13DEF8C0
	vxor	20, 30, 27
	.long	0x13DEF8C0
	vxor	21, 30, 27
	.long	0x13DEF8C0
	vxor	22, 30, 27

	addi	12, 12, -128
	addi	11, 11, 128

	bdnz	.Loop_8x_block

	vor	30,29,29

.Loop_last_block:
	cmpdi	12, 0
	beq	aes_gcm_out


	li	10, 16
	divdu	10, 12, 10

	mtctr	10

	lwz	10, 240(6)

	cmpdi	12, 16
	blt	Final_block

.macro	.Loop_aes_middle_1x
	xxlor	19+32, 1, 1
	xxlor	20+32, 2, 2
	xxlor	21+32, 3, 3
	xxlor	22+32, 4, 4

	.long	0x11EF9D08
	.long	0x11EFA508
	.long	0x11EFAD08
	.long	0x11EFB508

	xxlor	19+32, 5, 5
	xxlor	20+32, 6, 6
	xxlor	21+32, 7, 7
	xxlor	22+32, 8, 8

	.long	0x11EF9D08
	.long	0x11EFA508
	.long	0x11EFAD08
	.long	0x11EFB508

	xxlor	19+32, 9, 9
	.long	0x11EF9D08
.endm	

Next_rem_block:
	lxvb16x	15, 0, 14

.Loop_aes_middle_1x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_next_1x


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_next_1x


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_next_1x

Do_next_1x:
	.long	0x11EFBD09

	xxlxor	47, 47, 15
	stxvb16x	47, 0, 9
	addi	14, 14, 16
	addi	9, 9, 16

	vor	28,15,15
	ppc_update_hash_1x	

	addi	12, 12, -16
	addi	11, 11, 16
	xxlor	19+32, 0, 0
	.long	0x13DEF8C0
	vxor	15, 30, 19

	bdnz	Next_rem_block

	cmpdi	12, 0
	beq	aes_gcm_out

Final_block:
.Loop_aes_middle_1x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_final_1x


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_final_1x


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_final_1x

Do_final_1x:
	.long	0x11EFBD09

	lxvb16x	15, 0, 14
	xxlxor	47, 47, 15


	li	15, 16
	sub	15, 15, 12

	vspltisb	16,-1
	vspltisb	17,0
	li	10, 192
	stvx	16, 10, 1
	addi	10, 10, 16
	stvx	17, 10, 1

	addi	10, 1, 192
	lxvb16x	16, 15, 10
	xxland	47, 47, 16

	vor	28,15,15
	ppc_update_hash_1x	


	bl	Write_partial_block

	b	aes_gcm_out







Write_partial_block:
	li	10, 192
	stxvb16x	15+32, 10, 1


	addi	10, 9, -1
	addi	16, 1, 191

	mtctr	12
	li	15, 0

Write_last_byte:
	lbzu	14, 1(16)
	stbu	14, 1(10)
	bdnz	Write_last_byte
	blr	

aes_gcm_out:

	stxvb16x	32, 0, 8
	add	3, 11, 12

	li	9, 256
	lvx	20, 9, 1
	addi	9, 9, 16
	lvx	21, 9, 1
	addi	9, 9, 16
	lvx	22, 9, 1
	addi	9, 9, 16
	lvx	23, 9, 1
	addi	9, 9, 16
	lvx	24, 9, 1
	addi	9, 9, 16
	lvx	25, 9, 1
	addi	9, 9, 16
	lvx	26, 9, 1
	addi	9, 9, 16
	lvx	27, 9, 1
	addi	9, 9, 16
	lvx	28, 9, 1
	addi	9, 9, 16
	lvx	29, 9, 1
	addi	9, 9, 16
	lvx	30, 9, 1
	addi	9, 9, 16
	lvx	31, 9, 1

	ld	0, 528(1)
	ld	14,112(1)
	ld	15,120(1)
	ld	16,128(1)
	ld	17,136(1)
	ld	18,144(1)
	ld	19,152(1)
	ld	20,160(1)
	ld	21,168(1)

	mtlr	0
	addi	1, 1, 512
	blr	




.global	ppc_aes_gcm_decrypt
.align	5
ppc_aes_gcm_decrypt:
_ppc_aes_gcm_decrypt:

	stdu	1,-512(1)
	mflr	0

	std	14,112(1)
	std	15,120(1)
	std	16,128(1)
	std	17,136(1)
	std	18,144(1)
	std	19,152(1)
	std	20,160(1)
	std	21,168(1)
	li	9, 256
	stvx	20, 9, 1
	addi	9, 9, 16
	stvx	21, 9, 1
	addi	9, 9, 16
	stvx	22, 9, 1
	addi	9, 9, 16
	stvx	23, 9, 1
	addi	9, 9, 16
	stvx	24, 9, 1
	addi	9, 9, 16
	stvx	25, 9, 1
	addi	9, 9, 16
	stvx	26, 9, 1
	addi	9, 9, 16
	stvx	27, 9, 1
	addi	9, 9, 16
	stvx	28, 9, 1
	addi	9, 9, 16
	stvx	29, 9, 1
	addi	9, 9, 16
	stvx	30, 9, 1
	addi	9, 9, 16
	stvx	31, 9, 1
	std	0, 528(1)


	lxvb16x	32, 0, 8


	li	10, 32
	lxvd2x	2+32, 10, 8
	li	10, 48
	lxvd2x	3+32, 10, 8
	li	10, 64
	lxvd2x	4+32, 10, 8
	li	10, 80
	lxvd2x	5+32, 10, 8

	li	10, 96
	lxvd2x	6+32, 10, 8
	li	10, 112
	lxvd2x	7+32, 10, 8
	li	10, 128
	lxvd2x	8+32, 10, 8

	li	10, 144
	lxvd2x	9+32, 10, 8
	li	10, 160
	lxvd2x	10+32, 10, 8
	li	10, 176
	lxvd2x	11+32, 10, 8

	li	10, 192
	lxvd2x	12+32, 10, 8
	li	10, 208
	lxvd2x	13+32, 10, 8
	li	10, 224
	lxvd2x	14+32, 10, 8


	lxvb16x	30+32, 0, 7

	mr	12, 5
	li	11, 0


	vxor	31, 31, 31
	vspltisb	22,1
	vsldoi	31, 31, 22,1


	lxv	0, 0(6)
	lxv	1, 0x10(6)
	lxv	2, 0x20(6)
	lxv	3, 0x30(6)
	lxv	4, 0x40(6)
	lxv	5, 0x50(6)
	lxv	6, 0x60(6)
	lxv	7, 0x70(6)
	lxv	8, 0x80(6)
	lxv	9, 0x90(6)
	lxv	10, 0xa0(6)


	lwz	9,240(6)



	xxlor	32+29, 0, 0
	vxor	15, 30, 29

	cmpdi	9, 10
	beq	.Loop_aes_gcm_8x_dec


	lxv	11, 0xb0(6)
	lxv	12, 0xc0(6)

	cmpdi	9, 12
	beq	.Loop_aes_gcm_8x_dec


	lxv	13, 0xd0(6)
	lxv	14, 0xe0(6)
	cmpdi	9, 14
	beq	.Loop_aes_gcm_8x_dec

	b	aes_gcm_out

.align	5
.Loop_aes_gcm_8x_dec:
	mr	14, 3
	mr	9, 4


	li	10, 128
	divdu	10, 5, 10
	cmpdi	10, 0
	beq	.Loop_last_block_dec

	.long	0x13DEF8C0
	vxor	16, 30, 29
	.long	0x13DEF8C0
	vxor	17, 30, 29
	.long	0x13DEF8C0
	vxor	18, 30, 29
	.long	0x13DEF8C0
	vxor	19, 30, 29
	.long	0x13DEF8C0
	vxor	20, 30, 29
	.long	0x13DEF8C0
	vxor	21, 30, 29
	.long	0x13DEF8C0
	vxor	22, 30, 29

	mtctr	10

	li	15, 16
	li	16, 32
	li	17, 48
	li	18, 64
	li	19, 80
	li	20, 96
	li	21, 112

	lwz	10, 240(6)

.Loop_8x_block_dec:

	lxvb16x	15, 0, 14
	lxvb16x	16, 15, 14
	lxvb16x	17, 16, 14
	lxvb16x	18, 17, 14
	lxvb16x	19, 18, 14
	lxvb16x	20, 19, 14
	lxvb16x	21, 20, 14
	lxvb16x	22, 21, 14
	addi	14, 14, 128

.Loop_aes_middle8x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_last_aes_dec


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_last_aes_dec


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x1210BD08
	.long	0x1231BD08
	.long	0x1252BD08
	.long	0x1273BD08
	.long	0x1294BD08
	.long	0x12B5BD08
	.long	0x12D6BD08

	.long	0x11EFC508
	.long	0x1210C508
	.long	0x1231C508
	.long	0x1252C508
	.long	0x1273C508
	.long	0x1294C508
	.long	0x12B5C508
	.long	0x12D6C508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_last_aes_dec
	b	aes_gcm_out

Do_last_aes_dec:



	.long	0x11EFBD09
	.long	0x1210BD09

	xxlxor	47, 47, 15
	stxvb16x	47, 0, 9
	xxlxor	48, 48, 16
	stxvb16x	48, 15, 9

	.long	0x1231BD09
	.long	0x1252BD09

	xxlxor	49, 49, 17
	stxvb16x	49, 16, 9
	xxlxor	50, 50, 18
	stxvb16x	50, 17, 9

	.long	0x1273BD09
	.long	0x1294BD09

	xxlxor	51, 51, 19
	stxvb16x	51, 18, 9
	xxlxor	52, 52, 20
	stxvb16x	52, 19, 9

	.long	0x12B5BD09
	.long	0x12D6BD09

	xxlxor	53, 53, 21
	stxvb16x	53, 20, 9
	xxlxor	54, 54, 22
	stxvb16x	54, 21, 9

	addi	9, 9, 128

	xxlor	15+32, 15, 15
	xxlor	16+32, 16, 16
	xxlor	17+32, 17, 17
	xxlor	18+32, 18, 18
	xxlor	19+32, 19, 19
	xxlor	20+32, 20, 20
	xxlor	21+32, 21, 21
	xxlor	22+32, 22, 22


	ppc_aes_gcm_ghash2_4x	

	xxlor	27+32, 0, 0
	.long	0x13DEF8C0
	vor	29,30,30
	vxor	15, 30, 27
	.long	0x13DEF8C0
	vxor	16, 30, 27
	.long	0x13DEF8C0
	vxor	17, 30, 27
	.long	0x13DEF8C0
	vxor	18, 30, 27
	.long	0x13DEF8C0
	vxor	19, 30, 27
	.long	0x13DEF8C0
	vxor	20, 30, 27
	.long	0x13DEF8C0
	vxor	21, 30, 27
	.long	0x13DEF8C0
	vxor	22, 30, 27
	addi	12, 12, -128
	addi	11, 11, 128

	bdnz	.Loop_8x_block_dec

	vor	30,29,29

.Loop_last_block_dec:
	cmpdi	12, 0
	beq	aes_gcm_out


	li	10, 16
	divdu	10, 12, 10

	mtctr	10

	lwz	10,240(6)

	cmpdi	12, 16
	blt	Final_block_dec

Next_rem_block_dec:
	lxvb16x	15, 0, 14

.Loop_aes_middle_1x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_next_1x_dec


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_next_1x_dec


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_next_1x_dec

Do_next_1x_dec:
	.long	0x11EFBD09

	xxlxor	47, 47, 15
	stxvb16x	47, 0, 9
	addi	14, 14, 16
	addi	9, 9, 16

	xxlor	28+32, 15, 15
	ppc_update_hash_1x	

	addi	12, 12, -16
	addi	11, 11, 16
	xxlor	19+32, 0, 0
	.long	0x13DEF8C0
	vxor	15, 30, 19

	bdnz	Next_rem_block_dec

	cmpdi	12, 0
	beq	aes_gcm_out

Final_block_dec:
.Loop_aes_middle_1x	

	xxlor	23+32, 10, 10

	cmpdi	10, 10
	beq	Do_final_1x_dec


	xxlor	24+32, 11, 11

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 12, 12

	cmpdi	10, 12
	beq	Do_final_1x_dec


	xxlor	24+32, 13, 13

	.long	0x11EFBD08
	.long	0x11EFC508

	xxlor	23+32, 14, 14

	cmpdi	10, 14
	beq	Do_final_1x_dec

Do_final_1x_dec:
	.long	0x11EFBD09

	lxvb16x	15, 0, 14
	xxlxor	47, 47, 15


	li	15, 16
	sub	15, 15, 12

	vspltisb	16,-1
	vspltisb	17,0
	li	10, 192
	stvx	16, 10, 1
	addi	10, 10, 16
	stvx	17, 10, 1

	addi	10, 1, 192
	lxvb16x	16, 15, 10
	xxland	47, 47, 16

	xxlor	28+32, 15, 15
	ppc_update_hash_1x	


	bl	Write_partial_block

	b	aes_gcm_out
