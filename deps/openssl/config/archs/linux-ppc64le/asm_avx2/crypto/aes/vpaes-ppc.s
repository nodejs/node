.machine	"any"

.abiversion	2
.text

.align	7
_vpaes_consts:
.Lk_mc_forward:
.byte	0x0e,0x0d,0x0c,0x0f,0x0a,0x09,0x08,0x0b,0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03
.byte	0x0a,0x09,0x08,0x0b,0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03,0x0e,0x0d,0x0c,0x0f
.byte	0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03,0x0e,0x0d,0x0c,0x0f,0x0a,0x09,0x08,0x0b
.byte	0x02,0x01,0x00,0x03,0x0e,0x0d,0x0c,0x0f,0x0a,0x09,0x08,0x0b,0x06,0x05,0x04,0x07
.Lk_mc_backward:
.byte	0x0c,0x0f,0x0e,0x0d,0x08,0x0b,0x0a,0x09,0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01
.byte	0x00,0x03,0x02,0x01,0x0c,0x0f,0x0e,0x0d,0x08,0x0b,0x0a,0x09,0x04,0x07,0x06,0x05
.byte	0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01,0x0c,0x0f,0x0e,0x0d,0x08,0x0b,0x0a,0x09
.byte	0x08,0x0b,0x0a,0x09,0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01,0x0c,0x0f,0x0e,0x0d
.Lk_sr:
.byte	0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00
.byte	0x0f,0x0a,0x05,0x00,0x0b,0x06,0x01,0x0c,0x07,0x02,0x0d,0x08,0x03,0x0e,0x09,0x04
.byte	0x0f,0x06,0x0d,0x04,0x0b,0x02,0x09,0x00,0x07,0x0e,0x05,0x0c,0x03,0x0a,0x01,0x08
.byte	0x0f,0x02,0x05,0x08,0x0b,0x0e,0x01,0x04,0x07,0x0a,0x0d,0x00,0x03,0x06,0x09,0x0c




.Lk_inv:
.byte	0x04,0x07,0x03,0x09,0x0a,0x0b,0x0c,0x02,0x0e,0x05,0x06,0x0f,0x0d,0x08,0x01,0xf0
.byte	0x03,0x0d,0x0e,0x0c,0x02,0x05,0x08,0x09,0x01,0x04,0x0a,0x06,0x0f,0x0b,0x07,0xf0
.Lk_ipt:
.byte	0xca,0xba,0xe0,0x90,0x52,0x22,0x78,0x08,0xc2,0xb2,0xe8,0x98,0x5a,0x2a,0x70,0x00
.byte	0xcd,0x80,0xb1,0xfc,0xb0,0xfd,0xcc,0x81,0x4c,0x01,0x30,0x7d,0x31,0x7c,0x4d,0x00
.Lk_sbo:
.byte	0x15,0xaa,0xbf,0x7a,0xc5,0x02,0xa8,0x78,0xd0,0xd2,0x6d,0x17,0x6f,0xbd,0xc7,0x00
.byte	0x8e,0x1e,0x90,0xd1,0x41,0x2b,0x35,0xfa,0xcf,0xe4,0x74,0xa5,0x5f,0xbb,0x6a,0x00
.Lk_sb1:
.byte	0x3b,0xf7,0xcc,0xc1,0x0d,0x2e,0xd9,0xef,0x36,0x18,0xd4,0x15,0xfa,0xe2,0x23,0x00
.byte	0xa5,0xdf,0x7a,0x6e,0x14,0x2a,0xf5,0x44,0xb1,0x9b,0xe1,0x8f,0xcb,0x50,0x3e,0x00
.Lk_sb2:
.byte	0xc2,0xa1,0x63,0xc8,0xab,0x82,0x23,0x4a,0x69,0xeb,0x88,0x40,0x0a,0xe1,0x29,0x00
.byte	0x5e,0xb7,0xe9,0x55,0xbc,0x98,0x2f,0xcd,0xe2,0x7a,0x93,0xc6,0x0b,0x71,0x24,0x00




.Lk_dipt:
.byte	0x15,0x4a,0x41,0x1e,0x11,0x4e,0x45,0x1a,0x0f,0x50,0x5b,0x04,0x0b,0x54,0x5f,0x00
.byte	0x12,0x77,0x17,0x72,0xf4,0x91,0xf1,0x94,0x86,0xe3,0x83,0xe6,0x60,0x05,0x65,0x00
.Lk_dsbo:
.byte	0xc7,0xaa,0x6d,0xb9,0xd4,0x94,0x3e,0x2d,0x13,0x87,0xea,0x53,0x7e,0xf9,0x40,0x00
.byte	0xca,0x4b,0x81,0x59,0xd8,0xc5,0x8e,0x9c,0x12,0xd7,0x56,0x0f,0x93,0x44,0x1d,0x00
.Lk_dsb9:
.byte	0xca,0xd5,0x1f,0x50,0x4f,0x99,0x4c,0xc9,0x85,0x1c,0x03,0x53,0x9a,0x86,0xd6,0x00
.byte	0x72,0x5e,0x2c,0x9e,0xb2,0xfb,0xa5,0x65,0xc0,0x3b,0x17,0x89,0xec,0xd7,0x49,0x00
.Lk_dsbd:
.byte	0xf5,0x6e,0x9b,0x13,0x88,0x2a,0x44,0x39,0x7d,0x57,0xcc,0xdf,0xe6,0xb1,0xa2,0x00
.byte	0x29,0x31,0x18,0x0d,0x15,0xde,0xef,0xd3,0x3c,0xe2,0xfa,0xf7,0x24,0xc6,0xcb,0x00
.Lk_dsbb:
.byte	0x60,0x26,0x46,0xf6,0xb0,0xf2,0xd4,0x04,0xd0,0x22,0x64,0x92,0x96,0xb4,0x42,0x00
.byte	0xf3,0xff,0x0c,0x3e,0x32,0x55,0xaa,0x6b,0xc1,0x94,0x98,0xa6,0xcd,0x59,0x67,0x00
.Lk_dsbe:
.byte	0x22,0x42,0x60,0x04,0x64,0xb4,0xf6,0xb0,0x46,0xf2,0x92,0x96,0x26,0xd4,0xd0,0x00
.byte	0x94,0x67,0xf3,0x6b,0x98,0x59,0x3e,0x32,0x0c,0x55,0xa6,0xcd,0xff,0xaa,0xc1,0x00




.Lk_dksd:
.byte	0x07,0x40,0xe3,0xa4,0x5a,0x1d,0xbe,0xf9,0xfe,0xb9,0x1a,0x5d,0xa3,0xe4,0x47,0x00
.byte	0x5f,0xdc,0x69,0xea,0xab,0x28,0x9d,0x1e,0x41,0xc2,0x77,0xf4,0xb5,0x36,0x83,0x00
.Lk_dksb:
.byte	0x03,0xd6,0x53,0x86,0x1c,0xc9,0x4c,0x99,0x9a,0x4f,0xca,0x1f,0x85,0x50,0xd5,0x00
.byte	0xd9,0x93,0x25,0x6f,0x7e,0x34,0x82,0xc8,0x11,0x5b,0xed,0xa7,0xb6,0xfc,0x4a,0x00
.Lk_dkse:
.byte	0x53,0x85,0x9a,0x4c,0x99,0x4f,0x50,0x86,0xd5,0x03,0x1c,0xca,0x1f,0xc9,0xd6,0x00
.byte	0xcd,0x5e,0xf9,0x6a,0x20,0xb3,0x14,0x87,0xa2,0x31,0x96,0x05,0x4f,0xdc,0x7b,0xe8
.Lk_dks9:
.byte	0x4a,0xed,0x93,0x34,0x82,0x25,0x5b,0xfc,0xb6,0x11,0x6f,0xc8,0x7e,0xd9,0xa7,0x00
.byte	0x8b,0xb8,0x9f,0xac,0xe9,0xda,0xfd,0xce,0x45,0x76,0x51,0x62,0x27,0x14,0x33,0x00

.Lk_rcon:
.byte	0xb6,0xee,0x9d,0xaf,0xb9,0x91,0x83,0x1f,0x81,0x7d,0x7c,0x4d,0x08,0x98,0x2a,0x70
.Lk_s63:
.byte	0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b,0x5b

.Lk_opt:
.byte	0xf7,0x97,0x41,0x21,0xde,0xbe,0x68,0x08,0xff,0x9f,0x49,0x29,0xd6,0xb6,0x60,0x00
.byte	0xe1,0x0d,0x5d,0xb1,0xb0,0x5c,0x0c,0xe0,0x01,0xed,0xbd,0x51,0x50,0xbc,0xec,0x00
.Lk_deskew:
.byte	0x1d,0xfe,0xb9,0x5a,0x5d,0xbe,0xf9,0x1a,0x07,0xe4,0xa3,0x40,0x47,0xa4,0xe3,0x00
.byte	0x28,0x41,0xc2,0xab,0xf4,0x9d,0x1e,0x77,0x5f,0x36,0xb5,0xdc,0x83,0xea,0x69,0x00
.align	5
.Lconsts:
	mflr	0
	bcl	20,31,$+4
	mflr	12
	addi	12,12,-0x308
	mtlr	0
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
.byte	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105,111,110,32,65,69,83,32,102,111,114,32,65,108,116,105,86,101,99,44,77,105,107,101,32,72,97,109,98,117,114,103,32,40,83,116,97,110,102,111,114,100,32,85,110,105,118,101,114,115,105,116,121,41,0
.align	2
.align	6






.align	4
_vpaes_encrypt_preheat:
	mflr	8
	bl	.Lconsts
	mtlr	8
	li	11, 0xc0
	li	10, 0xd0
	li	9,  0xe0
	li	8,  0xf0
	vxor	7, 7, 7
	vspltisb	8,4
	vspltisb	9,0x0f
	lvx	10, 12, 11
	li	11, 0x100
	lvx	11, 12, 10
	li	10, 0x110
	lvx	12, 12, 9
	li	9,  0x120
	lvx	13, 12, 8
	li	8,  0x130
	lvx	14, 12, 11
	li	11, 0x140
	lvx	15, 12, 10
	li	10, 0x150
	lvx	16, 12, 9
	lvx	17, 12, 8
	lvx	18, 12, 11
	lvx	19, 12, 10
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0















.align	5
_vpaes_encrypt_core:
	lwz	8, 240(5)
	li	9, 16
	lvx	5, 0, 5
	li	11, 0x10
	lvx	6, 9, 5
	addi	9, 9, 16
	vperm	5, 6, 5, 31
	addi	10, 11, 0x40
	vsrb	1, 0, 8
	vperm	0, 12, 12, 0
	vperm	1, 13, 13, 1
	vxor	0, 0, 5
	vxor	0, 0, 1
	mtctr	8
	b	.Lenc_entry

.align	4
.Lenc_loop:

	vperm	4, 17, 7, 2
	lvx	1, 12, 11
	addi	11, 11, 16
	vperm	0, 16, 7, 3
	vxor	4, 4, 5
	andi.	11, 11, 0x30
	vperm	5, 19, 7, 2
	vxor	0, 0, 4
	vperm	2, 18, 7, 3
	lvx	4, 12, 10
	addi	10, 11, 0x40
	vperm	3, 0, 7, 1
	vxor	2, 2, 5
	vperm	0, 0, 7, 4
	vxor	3, 3, 2
	vperm	4, 3, 7, 1
	vxor	0, 0, 3
	vxor	0, 0, 4

.Lenc_entry:

	vsrb	1, 0, 8
	vperm	5, 11, 11, 0
	vxor	0, 0, 1
	vperm	3, 10, 10, 1
	vperm	4, 10, 10, 0
	vand	0, 0, 9
	vxor	3, 3, 5
	vxor	4, 4, 5
	vperm	2, 10, 7, 3
	vor	5,6,6
	lvx	6, 9, 5
	vperm	3, 10, 7, 4
	addi	9, 9, 16
	vxor	2, 2, 0
	vperm	5, 6, 5, 31
	vxor	3, 3, 1
	bdnz	.Lenc_loop


	addi	10, 11, 0x80


	vperm	4, 14, 7, 2
	lvx	1, 12, 10
	vperm	0, 15, 7, 3
	vxor	4, 4, 5
	vxor	0, 0, 4
	vperm	0, 0, 7, 1
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.globl	vpaes_encrypt
.type	vpaes_encrypt,@function
.align	5
vpaes_encrypt:
.localentry	vpaes_encrypt,0

	stdu	1,-256(1)
	li	10,63
	li	11,79
	mflr	6
	li	7,-1
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
	stvx	31,11,1
	stw	7,252(1)
	li	0, -1
	std	6,272(1)
	or	0,0,0

	bl	_vpaes_encrypt_preheat

	lvsr	27, 0, 3
	lvx	0, 0, 3
	addi	3, 3, 15
	lvsl	29, 0, 4
	lvsr	31, 0, 5
	lvx	26, 0, 3
	vperm	0, 26, 0, 27

	bl	_vpaes_encrypt_core

	andi.	8, 4, 15
	li	9, 16
	beq	.Lenc_out_aligned

	vperm	0, 0, 0, 29
	mtctr	9
.Lenc_out_unaligned:
	stvebx	0, 0, 4
	addi	4, 4, 1
	bdnz	.Lenc_out_unaligned
	b	.Lenc_done

.align	4
.Lenc_out_aligned:
	stvx	0, 0, 4
.Lenc_done:

	li	10,63
	li	11,79
	mtlr	6
	or	7,7,7
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
	addi	1,1,256
	blr	
.long	0
.byte	0,12,0x04,1,0x80,0,3,0
.long	0
.size	vpaes_encrypt,.-vpaes_encrypt

.align	4
_vpaes_decrypt_preheat:
	mflr	8
	bl	.Lconsts
	mtlr	8
	li	11, 0xc0
	li	10, 0xd0
	li	9,  0x160
	li	8,  0x170
	vxor	7, 7, 7
	vspltisb	8,4
	vspltisb	9,0x0f
	lvx	10, 12, 11
	li	11, 0x180
	lvx	11, 12, 10
	li	10, 0x190
	lvx	12, 12, 9
	li	9,  0x1a0
	lvx	13, 12, 8
	li	8,  0x1b0
	lvx	14, 12, 11
	li	11, 0x1c0
	lvx	15, 12, 10
	li	10, 0x1d0
	lvx	16, 12, 9
	li	9,  0x1e0
	lvx	17, 12, 8
	li	8,  0x1f0
	lvx	18, 12, 11
	li	11, 0x200
	lvx	19, 12, 10
	li	10, 0x210
	lvx	20, 12, 9
	lvx	21, 12, 8
	lvx	22, 12, 11
	lvx	23, 12, 10
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0






.align	4
_vpaes_decrypt_core:
	lwz	8, 240(5)
	li	9, 16
	lvx	5, 0, 5
	li	11, 0x30
	lvx	6, 9, 5
	addi	9, 9, 16
	vperm	5, 6, 5, 31
	vsrb	1, 0, 8
	vperm	0, 12, 12, 0
	vperm	1, 13, 13, 1
	vxor	0, 0, 5
	vxor	0, 0, 1
	mtctr	8
	b	.Ldec_entry

.align	4
.Ldec_loop:



	lvx	0, 12, 11


	vperm	4, 16, 7, 2
	subi	11, 11, 16
	vperm	1, 17, 7, 3
	andi.	11, 11, 0x30
	vxor	5, 5, 4

	vxor	5, 5, 1


	vperm	4, 18, 7, 2
	vperm	5, 5, 7, 0
	vperm	1, 19, 7, 3
	vxor	5, 5, 4

	vxor	5, 5, 1


	vperm	4, 20, 7, 2
	vperm	5, 5, 7, 0
	vperm	1, 21, 7, 3
	vxor	5, 5, 4

	vxor	5, 5, 1


	vperm	4, 22, 7, 2
	vperm	5, 5, 7, 0
	vperm	1, 23, 7, 3
	vxor	0, 5, 4
	vxor	0, 0, 1

.Ldec_entry:

	vsrb	1, 0, 8
	vperm	2, 11, 11, 0
	vxor	0, 0, 1
	vperm	3, 10, 10, 1
	vperm	4, 10, 10, 0
	vand	0, 0, 9
	vxor	3, 3, 2
	vxor	4, 4, 2
	vperm	2, 10, 7, 3
	vor	5,6,6
	lvx	6, 9, 5
	vperm	3, 10, 7, 4
	addi	9, 9, 16
	vxor	2, 2, 0
	vperm	5, 6, 5, 31
	vxor	3, 3, 1
	bdnz	.Ldec_loop


	addi	10, 11, 0x80

	vperm	4, 14, 7, 2

	lvx	2, 12, 10
	vperm	1, 15, 7, 3
	vxor	4, 4, 5
	vxor	0, 1, 4
	vperm	0, 0, 7, 2
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.globl	vpaes_decrypt
.type	vpaes_decrypt,@function
.align	5
vpaes_decrypt:
.localentry	vpaes_decrypt,0

	stdu	1,-256(1)
	li	10,63
	li	11,79
	mflr	6
	li	7,-1
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
	stvx	31,11,1
	stw	7,252(1)
	li	0, -1
	std	6,272(1)
	or	0,0,0

	bl	_vpaes_decrypt_preheat

	lvsr	27, 0, 3
	lvx	0, 0, 3
	addi	3, 3, 15
	lvsl	29, 0, 4
	lvsr	31, 0, 5
	lvx	26, 0, 3
	vperm	0, 26, 0, 27

	bl	_vpaes_decrypt_core

	andi.	8, 4, 15
	li	9, 16
	beq	.Ldec_out_aligned

	vperm	0, 0, 0, 29
	mtctr	9
.Ldec_out_unaligned:
	stvebx	0, 0, 4
	addi	4, 4, 1
	bdnz	.Ldec_out_unaligned
	b	.Ldec_done

.align	4
.Ldec_out_aligned:
	stvx	0, 0, 4
.Ldec_done:

	li	10,63
	li	11,79
	mtlr	6
	or	7,7,7
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
	addi	1,1,256
	blr	
.long	0
.byte	0,12,0x04,1,0x80,0,3,0
.long	0
.size	vpaes_decrypt,.-vpaes_decrypt

.globl	vpaes_cbc_encrypt
.type	vpaes_cbc_encrypt,@function
.align	5
vpaes_cbc_encrypt:
.localentry	vpaes_cbc_encrypt,0

	cmpldi	5,16
	.long	0x4dc00020

	stdu	1,-272(1)
	mflr	0
	li	10,63
	li	11,79
	li	12,-1
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
	stvx	31,11,1
	stw	12,252(1)
	std	30,256(1)
	std	31,264(1)
	li	9, -16
	std	0, 288(1)

	and	30, 5, 9
	andi.	9, 4, 15
	mr	5, 6
	mr	31, 7
	li	6, -1
	mcrf	1, 0
	mr	7, 12
	or	6,6,6

	lvx	24, 0, 31
	li	9, 15
	lvsr	27, 0, 31
	lvx	25, 9, 31
	vperm	24, 25, 24, 27

	cmpwi	8, 0
	neg	8, 3
	vxor	7, 7, 7
	lvsr	31, 0, 5
	lvsl	29, 0, 4
	lvsl	27, 0, 8
	vnor	30, 7, 7
	lvx	26, 0, 3
	vperm	30, 30, 7, 29
	addi	3, 3, 15

	beq	.Lcbc_decrypt

	bl	_vpaes_encrypt_preheat
	li	0, 16

	beq	1, .Lcbc_enc_loop

	vor	0,26,26
	lvx	26, 0, 3
	addi	3, 3, 16
	vperm	0, 26, 0, 27
	vxor	0, 0, 24

	bl	_vpaes_encrypt_core

	andi.	8, 4, 15
	vor	24,0,0
	sub	9, 4, 8
	vperm	28, 0, 0, 29

.Lcbc_enc_head:
	stvebx	28, 8, 9
	cmpwi	8, 15
	addi	8, 8, 1
	bne	.Lcbc_enc_head

	sub.	30, 30, 0
	addi	4, 4, 16
	beq	.Lcbc_unaligned_done

.Lcbc_enc_loop:
	vor	0,26,26
	lvx	26, 0, 3
	addi	3, 3, 16
	vperm	0, 26, 0, 27
	vxor	0, 0, 24

	bl	_vpaes_encrypt_core

	vor	24,0,0
	sub.	30, 30, 0
	vperm	0, 0, 0, 29
	vsel	1, 28, 0, 30
	vor	28,0,0
	stvx	1, 0, 4
	addi	4, 4, 16
	bne	.Lcbc_enc_loop

	b	.Lcbc_done

.align	5
.Lcbc_decrypt:
	bl	_vpaes_decrypt_preheat
	li	0, 16

	beq	1, .Lcbc_dec_loop

	vor	0,26,26
	lvx	26, 0, 3
	addi	3, 3, 16
	vperm	0, 26, 0, 27
	vor	25,0,0

	bl	_vpaes_decrypt_core

	andi.	8, 4, 15
	vxor	0, 0, 24
	vor	24,25,25
	sub	9, 4, 8
	vperm	28, 0, 0, 29

.Lcbc_dec_head:
	stvebx	28, 8, 9
	cmpwi	8, 15
	addi	8, 8, 1
	bne	.Lcbc_dec_head

	sub.	30, 30, 0
	addi	4, 4, 16
	beq	.Lcbc_unaligned_done

.Lcbc_dec_loop:
	vor	0,26,26
	lvx	26, 0, 3
	addi	3, 3, 16
	vperm	0, 26, 0, 27
	vor	25,0,0

	bl	_vpaes_decrypt_core

	vxor	0, 0, 24
	vor	24,25,25
	sub.	30, 30, 0
	vperm	0, 0, 0, 29
	vsel	1, 28, 0, 30
	vor	28,0,0
	stvx	1, 0, 4
	addi	4, 4, 16
	bne	.Lcbc_dec_loop

.Lcbc_done:
	beq	1, .Lcbc_write_iv

.Lcbc_unaligned_done:
	andi.	8, 4, 15
	sub	4, 4, 8
	li	9, 0
.Lcbc_tail:
	stvebx	28, 9, 4
	addi	9, 9, 1
	cmpw	9, 8
	bne	.Lcbc_tail

.Lcbc_write_iv:
	neg	8, 31
	li	10, 4
	lvsr	29, 0, 8
	li	11, 8
	li	12, 12
	vperm	24, 24, 24, 29
	stvewx	24, 0, 31
	stvewx	24, 10, 31
	stvewx	24, 11, 31
	stvewx	24, 12, 31

	or	7,7,7
	li	10,63
	li	11,79
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
.Lcbc_abort:
	ld	0, 288(1)
	ld	30,256(1)
	ld	31,264(1)
	mtlr	0
	addi	1,1,272
	blr	
.long	0
.byte	0,12,0x04,1,0x80,2,6,0
.long	0
.size	vpaes_cbc_encrypt,.-vpaes_cbc_encrypt





.align	4
_vpaes_key_preheat:
	mflr	8
	bl	.Lconsts
	mtlr	8
	li	11, 0xc0
	li	10, 0xd0
	li	9,  0xe0
	li	8,  0xf0

	vspltisb	8,4
	vxor	9,9,9
	lvx	10, 12, 11
	li	11, 0x120
	lvx	11, 12, 10
	li	10, 0x130
	lvx	12, 12, 9
	li	9, 0x220
	lvx	13, 12, 8
	li	8, 0x230

	lvx	14, 12, 11
	li	11, 0x240
	lvx	15, 12, 10
	li	10, 0x250

	lvx	16, 12, 9
	li	9, 0x260
	lvx	17, 12, 8
	li	8, 0x270
	lvx	18, 12, 11
	li	11, 0x280
	lvx	19, 12, 10
	li	10, 0x290
	lvx	20, 12, 9
	li	9, 0x2a0
	lvx	21, 12, 8
	li	8, 0x2b0
	lvx	22, 12, 11
	lvx	23, 12, 10

	lvx	24, 12, 9
	lvx	25, 0, 12
	lvx	26, 12, 8
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.align	4
_vpaes_schedule_core:
	mflr	7

	bl	_vpaes_key_preheat


	neg	8, 3
	lvx	0, 0, 3
	addi	3, 3, 15
	lvsl	27, 0, 8
	lvx	6, 0, 3
	addi	3, 3, 8
	vperm	0, 6, 0, 27


	vor	3,0,0
	bl	_vpaes_schedule_transform
	vor	7,0,0

	bne	1, .Lschedule_am_decrypting


	li	8, 0x30
	li	9, 4
	li	10, 8
	li	11, 12

	lvsl	29, 0, 5
	vnor	30, 9, 9
	vperm	30, 30, 9, 29


	vperm	28, 0, 0, 29
	stvewx	28, 0, 5
	stvewx	28, 9, 5
	stvewx	28, 10, 5
	addi	10, 12, 0x80
	stvewx	28, 11, 5
	b	.Lschedule_go

.Lschedule_am_decrypting:
	srwi	8, 4, 1
	andi.	8, 8, 32
	xori	8, 8, 32
	addi	10, 12, 0x80

	lvx	1, 8, 10
	li	9, 4
	li	10, 8
	li	11, 12
	vperm	4, 3, 3, 1

	neg	0, 5
	lvsr	29, 0, 0
	vnor	30, 9, 9
	vperm	30, 9, 30, 29


	vperm	28, 4, 4, 29
	stvewx	28, 0, 5
	stvewx	28, 9, 5
	stvewx	28, 10, 5
	addi	10, 12, 0x80
	stvewx	28, 11, 5
	addi	5, 5, 15
	xori	8, 8, 0x30

.Lschedule_go:
	cmplwi	4, 192
	bgt	.Lschedule_256
	beq	.Lschedule_192










.Lschedule_128:
	li	0, 10
	mtctr	0

.Loop_schedule_128:
	bl	_vpaes_schedule_round
	bdz	.Lschedule_mangle_last
	bl	_vpaes_schedule_mangle
	b	.Loop_schedule_128
















.align	4
.Lschedule_192:
	li	0, 4
	lvx	0, 0, 3
	vperm	0, 0, 6, 27
	vsldoi	0, 0, 3, 16-8
	bl	_vpaes_schedule_transform
	vsldoi	6, 9, 0, 16-8
	vsldoi	6, 6, 9, 16-8
	mtctr	0

.Loop_schedule_192:
	bl	_vpaes_schedule_round
	vsldoi	0, 0, 6, 16-8
	bl	_vpaes_schedule_mangle
	bl	_vpaes_schedule_192_smear
	bl	_vpaes_schedule_mangle
	bl	_vpaes_schedule_round
	bdz	.Lschedule_mangle_last
	bl	_vpaes_schedule_mangle
	bl	_vpaes_schedule_192_smear
	b	.Loop_schedule_192











.align	4
.Lschedule_256:
	li	0, 7
	addi	3, 3, 8
	lvx	0, 0, 3
	vperm	0, 0, 6, 27
	bl	_vpaes_schedule_transform
	mtctr	0

.Loop_schedule_256:
	bl	_vpaes_schedule_mangle
	vor	6,0,0


	bl	_vpaes_schedule_round
	bdz	.Lschedule_mangle_last
	bl	_vpaes_schedule_mangle


	vspltw	0, 0, 3-3
	vor	5,7,7
	vor	7,6,6
	bl	_vpaes_schedule_low_round
	vor	7,5,5

	b	.Loop_schedule_256










.align	4
.Lschedule_mangle_last:

	li	11, 0x2e0
	li	9,  0x2f0
	bne	1, .Lschedule_mangle_last_dec


	lvx	1, 8, 10
	li	11, 0x2c0
	li	9,  0x2d0
	vperm	0, 0, 0, 1

	lvx	12, 11, 12
	lvx	13, 9, 12
	addi	5, 5, 16
	vxor	0, 0, 26
	bl	_vpaes_schedule_transform


	vperm	0, 0, 0, 29
	li	10, 4
	vsel	2, 28, 0, 30
	li	11, 8
	stvx	2, 0, 5
	li	12, 12
	stvewx	0, 0, 5
	stvewx	0, 10, 5
	stvewx	0, 11, 5
	stvewx	0, 12, 5
	b	.Lschedule_mangle_done

.align	4
.Lschedule_mangle_last_dec:
	lvx	12, 11, 12
	lvx	13, 9,  12
	addi	5, 5, -16
	vxor	0, 0, 26
	bl	_vpaes_schedule_transform


	addi	9, 5, -15
	vperm	0, 0, 0, 29
	li	10, 4
	vsel	2, 28, 0, 30
	li	11, 8
	stvx	2, 0, 5
	li	12, 12
	stvewx	0, 0, 9
	stvewx	0, 10, 9
	stvewx	0, 11, 9
	stvewx	0, 12, 9


.Lschedule_mangle_done:
	mtlr	7

	vxor	0, 0, 0
	vxor	1, 1, 1
	vxor	2, 2, 2
	vxor	3, 3, 3
	vxor	4, 4, 4
	vxor	5, 5, 5
	vxor	6, 6, 6
	vxor	7, 7, 7

	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0















.align	4
_vpaes_schedule_192_smear:
	vspltw	0, 7, 3-3
	vsldoi	1, 6, 9, 16-12
	vsldoi	0, 0, 7, 16-8
	vxor	6, 6, 1
	vxor	6, 6, 0
	vor	0,6,6
	vsldoi	6, 9, 6, 16-8
	vsldoi	6, 6, 9, 16-8
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0



















.align	4
_vpaes_schedule_round:


	vsldoi	1, 9, 24, 16-15
	vsldoi	24, 24, 24, 16-15
	vxor	7, 7, 1


	vspltw	0, 0, 3-3
	vsldoi	0, 0, 0, 16-1




_vpaes_schedule_low_round:

	vsldoi	1, 7, 9, 16-12
	vxor	7, 7, 1
	vspltisb	1, 0x0f
	vsldoi	4, 7, 9, 16-8


	vand	1, 1, 0
	vsrb	0, 0, 8
	vxor	7, 7, 4
	vperm	2, 11, 9, 1
	vxor	1, 1, 0
	vperm	3, 10, 9, 0
	vxor	3, 3, 2
	vperm	4, 10, 9, 1
	vxor	7, 7, 26
	vperm	3, 10, 9, 3
	vxor	4, 4, 2
	vperm	2, 10, 9, 4
	vxor	3, 3, 1
	vxor	2, 2, 0
	vperm	4, 15, 9, 3
	vperm	1, 14, 9, 2
	vxor	1, 1, 4


	vxor	0, 1, 7
	vxor	7, 1, 7
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0










.align	4
_vpaes_schedule_transform:

	vsrb	2, 0, 8

	vperm	0, 12, 12, 0

	vperm	2, 13, 13, 2
	vxor	0, 0, 2
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0
























.align	4
_vpaes_schedule_mangle:


	bne	1, .Lschedule_mangle_dec


	vxor	4, 0, 26
	addi	5, 5, 16
	vperm	4, 4, 4, 25
	vperm	1, 4, 4, 25
	vperm	3, 1, 1, 25
	vxor	4, 4, 1
	lvx	1, 8, 10
	vxor	3, 3, 4

	vperm	3, 3, 3, 1
	addi	8, 8, -16
	andi.	8, 8, 0x30


	vperm	1, 3, 3, 29
	vsel	2, 28, 1, 30
	vor	28,1,1
	stvx	2, 0, 5
	blr	

.align	4
.Lschedule_mangle_dec:


	vsrb	1, 0, 8



	vperm	2, 16, 16, 0

	vperm	3, 17, 17, 1
	vxor	3, 3, 2
	vperm	3, 3, 9, 25


	vperm	2, 18, 18, 0
	vxor	2, 2, 3

	vperm	3, 19, 19, 1
	vxor	3, 3, 2
	vperm	3, 3, 9, 25


	vperm	2, 20, 20, 0
	vxor	2, 2, 3

	vperm	3, 21, 21, 1
	vxor	3, 3, 2


	vperm	2, 22, 22, 0
	vperm	3, 3, 9, 25

	vperm	4, 23, 23, 1
	lvx	1, 8, 10
	vxor	2, 2, 3
	vxor	3, 4, 2

	addi	5, 5, -16

	vperm	3, 3, 3, 1
	addi	8, 8, -16
	andi.	8, 8, 0x30


	vperm	1, 3, 3, 29
	vsel	2, 28, 1, 30
	vor	28,1,1
	stvx	2, 0, 5
	blr	
.long	0
.byte	0,12,0x14,0,0,0,0,0

.globl	vpaes_set_encrypt_key
.type	vpaes_set_encrypt_key,@function
.align	5
vpaes_set_encrypt_key:
.localentry	vpaes_set_encrypt_key,0

	stdu	1,-256(1)
	li	10,63
	li	11,79
	mflr	0
	li	6,-1
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
	stvx	31,11,1
	stw	6,252(1)
	li	7, -1
	std	0, 272(1)
	or	7,7,7

	srwi	9, 4, 5
	addi	9, 9, 6
	stw	9, 240(5)

	cmplw	1,4,4
	li	8, 0x30
	bl	_vpaes_schedule_core

	ld	0, 272(1)
	li	10,63
	li	11,79
	or	6,6,6
	mtlr	0
	xor	3, 3, 3
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
	addi	1,1,256
	blr	
.long	0
.byte	0,12,0x04,1,0x80,0,3,0
.long	0
.size	vpaes_set_encrypt_key,.-vpaes_set_encrypt_key

.globl	vpaes_set_decrypt_key
.type	vpaes_set_decrypt_key,@function
.align	4
vpaes_set_decrypt_key:
.localentry	vpaes_set_decrypt_key,0

	stdu	1,-256(1)
	li	10,63
	li	11,79
	mflr	0
	li	6,-1
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
	stvx	31,11,1
	stw	6,252(1)
	li	7, -1
	std	0, 272(1)
	or	7,7,7

	srwi	9, 4, 5
	addi	9, 9, 6
	stw	9, 240(5)

	slwi	9, 9, 4
	add	5, 5, 9

	cmplwi	1, 4, 0
	srwi	8, 4, 1
	andi.	8, 8, 32
	xori	8, 8, 32
	bl	_vpaes_schedule_core

	ld	0,  272(1)
	li	10,63
	li	11,79
	or	6,6,6
	mtlr	0
	xor	3, 3, 3
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
	addi	1,1,256
	blr	
.long	0
.byte	0,12,0x04,1,0x80,0,3,0
.long	0
.size	vpaes_set_decrypt_key,.-vpaes_set_decrypt_key
