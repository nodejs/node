default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64

EXTERN	OPENSSL_ia32cap_P












global	ossl_aes_cfb128_vaes_eligible



ossl_aes_cfb128_vaes_eligible:

DB	243,15,30,250

	mov	ecx,DWORD[((OPENSSL_ia32cap_P+8))]
	xor	eax,eax




	and	ecx,0x40030000
	cmp	ecx,0x40030000
	jne	NEAR $L$aes_cfb128_vaes_eligible_done

	mov	ecx,DWORD[((OPENSSL_ia32cap_P+12))]




	and	ecx,0x200
	cmp	ecx,0x200
	cmove	eax,ecx

$L$aes_cfb128_vaes_eligible_done:
	DB	0F3h,0C3h		;repret


global	ossl_aes_cfb128_vaes_enc


ossl_aes_cfb128_vaes_enc:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_aes_cfb128_vaes_enc:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250

	mov	r11,QWORD[r9]


	test	rdx,rdx
	jz	NEAR $L$aes_cfb128_vaes_enc_done

	test	r11,r11
	jz	NEAR $L$aes_cfb128_enc_mid





	mov	r10,rcx

	mov	rcx,0x10
	sub	rcx,r11
	cmp	rcx,rdx
	cmova	rcx,rdx

	mov	rax,1
	shl	rax,cl
	dec	rax
	kmovq	k1,rax

	mov	rax,r11
	add	rax,rcx
	and	al,0x0F

	lea	r11,[r8*1+r11]
	vmovdqu8	xmm0{k1}{z},[r11]
	vmovdqu8	xmm1{k1}{z},[rdi]
	vpxor	xmm2,xmm1,xmm0
	vmovdqu8	XMMWORD[rsi]{k1},xmm2
	vmovdqu8	XMMWORD[r11]{k1},xmm2

	add	rdi,rcx
	add	rsi,rcx
	sub	rdx,rcx
	jz	NEAR $L$aes_cfb128_enc_zero_pre

	mov	rcx,r10

$L$aes_cfb128_enc_mid:
	vmovdqu8	xmm17,XMMWORD[rcx]
	vmovdqu8	xmm18,XMMWORD[16+rcx]
	vmovdqu8	xmm19,XMMWORD[32+rcx]
	vmovdqu8	xmm20,XMMWORD[48+rcx]
	vmovdqu8	xmm21,XMMWORD[64+rcx]
	vmovdqu8	xmm22,XMMWORD[80+rcx]
	vmovdqu8	xmm23,XMMWORD[96+rcx]
	vmovdqu8	xmm24,XMMWORD[112+rcx]
	vmovdqu8	xmm25,XMMWORD[128+rcx]
	vmovdqu8	xmm26,XMMWORD[144+rcx]
	vmovdqu8	xmm27,XMMWORD[160+rcx]
	vmovdqu8	xmm28,XMMWORD[176+rcx]
	vmovdqu8	xmm29,XMMWORD[192+rcx]
	vmovdqu8	xmm30,XMMWORD[208+rcx]
	vmovdqu8	xmm31,XMMWORD[224+rcx]

	mov	r11d,DWORD[240+rcx]





	vmovdqu	xmm2,XMMWORD[r8]

	cmp	rdx,0x10
	jb	NEAR $L$aes_cfb128_enc_post


$L$oop_aes_cfb128_enc_main:
	sub	rdx,0x10

	vmovdqu	xmm3,XMMWORD[rdi]
	lea	rdi,[16+rdi]
	vpxord	xmm2,xmm2,xmm17
	vaesenc	xmm2,xmm2,xmm18
	vaesenc	xmm2,xmm2,xmm19
	vaesenc	xmm2,xmm2,xmm20
	vaesenc	xmm2,xmm2,xmm21
	vaesenc	xmm2,xmm2,xmm22
	vaesenc	xmm2,xmm2,xmm23
	vaesenc	xmm2,xmm2,xmm24
	vaesenc	xmm2,xmm2,xmm25
	vaesenc	xmm2,xmm2,xmm26

	cmp	r11d,0x09
	ja	NEAR $L$aes_cfb128_enc_mid_192_256

	vaesenclast	xmm2,xmm2,xmm27
	jmp	NEAR $L$aes_cfb128_enc_mid_end


$L$aes_cfb128_enc_mid_192_256:

	vaesenc	xmm2,xmm2,xmm27
	vaesenc	xmm2,xmm2,xmm28

	cmp	r11d,0x0B
	ja	NEAR $L$aes_cfb128_enc_mid_256

	vaesenclast	xmm2,xmm2,xmm29
	jmp	NEAR $L$aes_cfb128_enc_mid_end


$L$aes_cfb128_enc_mid_256:

	vaesenc	xmm2,xmm2,xmm29
	vaesenc	xmm2,xmm2,xmm30
	vaesenclast	xmm2,xmm2,xmm31


$L$aes_cfb128_enc_mid_end:

	vpxor	xmm2,xmm2,xmm3
	cmp	rdx,0x10
	vmovdqu	XMMWORD[rsi],xmm2
	lea	rsi,[16+rsi]
	jae	NEAR $L$oop_aes_cfb128_enc_main

	xor	eax,eax

	vmovdqu	XMMWORD[r8],xmm2

$L$aes_cfb128_enc_post:





	test	rdx,rdx
	jz	NEAR $L$aes_cfb128_enc_zero_all
	vpxord	xmm2,xmm2,xmm17
	vaesenc	xmm2,xmm2,xmm18
	vaesenc	xmm2,xmm2,xmm19
	vaesenc	xmm2,xmm2,xmm20
	vaesenc	xmm2,xmm2,xmm21
	vaesenc	xmm2,xmm2,xmm22
	vaesenc	xmm2,xmm2,xmm23
	vaesenc	xmm2,xmm2,xmm24
	vaesenc	xmm2,xmm2,xmm25
	vaesenc	xmm2,xmm2,xmm26

	cmp	r11d,0x09
	ja	NEAR $L$aes_cfb128_enc_post_192_256

	vaesenclast	xmm2,xmm2,xmm27
	jmp	NEAR $L$aes_cfb128_enc_post_end


$L$aes_cfb128_enc_post_192_256:

	vaesenc	xmm2,xmm2,xmm27
	vaesenc	xmm2,xmm2,xmm28

	cmp	r11d,0x0B
	ja	NEAR $L$aes_cfb128_enc_post_256

	vaesenclast	xmm2,xmm2,xmm29
	jmp	NEAR $L$aes_cfb128_enc_post_end


$L$aes_cfb128_enc_post_256:

	vaesenc	xmm2,xmm2,xmm29
	vaesenc	xmm2,xmm2,xmm30
	vaesenclast	xmm2,xmm2,xmm31


$L$aes_cfb128_enc_post_end:

	mov	rax,rdx

	mov	r11,1
	mov	cl,dl
	shl	r11,cl
	dec	r11
	kmovq	k1,r11

	vmovdqu8	xmm1{k1}{z},[rdi]
	vpxor	xmm0,xmm1,xmm2
	vmovdqu8	XMMWORD[rsi]{k1},xmm0
	vmovdqu8	XMMWORD[r8],xmm0



$L$aes_cfb128_enc_zero_all:
	vpxord	xmm17,xmm17,xmm17
	vpxord	xmm18,xmm18,xmm18
	vpxord	xmm19,xmm19,xmm19
	vpxord	xmm20,xmm20,xmm20
	vpxord	xmm21,xmm21,xmm21
	vpxord	xmm22,xmm22,xmm22
	vpxord	xmm23,xmm23,xmm23
	vpxord	xmm24,xmm24,xmm24
	vpxord	xmm25,xmm25,xmm25
	vpxord	xmm26,xmm26,xmm26
	vpxord	xmm27,xmm27,xmm27
	vpxord	xmm28,xmm28,xmm28
	vpxord	xmm29,xmm29,xmm29
	vpxord	xmm30,xmm30,xmm30
	vpxord	xmm31,xmm31,xmm31

	vpxor	xmm3,xmm3,xmm3

$L$aes_cfb128_enc_zero_pre:
	vpxor	xmm0,xmm0,xmm0
	vpxor	xmm1,xmm1,xmm1
	vpxor	xmm2,xmm2,xmm2

	mov	QWORD[r9],rax

	vzeroupper

$L$aes_cfb128_vaes_enc_done:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_aes_cfb128_vaes_enc:
global	ossl_aes_cfb128_vaes_dec


ossl_aes_cfb128_vaes_dec:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_aes_cfb128_vaes_dec:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250

	mov	r11,QWORD[r9]


	test	rdx,rdx
	jz	NEAR $L$aes_cfb128_vaes_dec_done
	sub	rsp,0x10

	vmovdqu	XMMWORD[rsp],xmm6
	test	r11,r11
	jz	NEAR $L$aes_cfb128_dec_mid





	mov	r10,rcx

	mov	rcx,0x10
	sub	rcx,r11
	cmp	rcx,rdx
	cmova	rcx,rdx

	mov	rax,1
	shl	rax,cl
	dec	rax
	kmovq	k1,rax

	lea	rax,[rcx*1+r11]
	and	al,0x0F

	lea	r11,[r8*1+r11]
	vmovdqu8	xmm0{k1}{z},[r11]
	vmovdqu8	xmm1{k1}{z},[rdi]
	vpxor	xmm2,xmm1,xmm0
	vmovdqu8	XMMWORD[rsi]{k1},xmm2
	vmovdqu8	XMMWORD[r11]{k1},xmm1

	add	rdi,rcx
	add	rsi,rcx
	sub	rdx,rcx
	jz	NEAR $L$aes_cfb128_dec_zero_pre

	mov	rcx,r10

$L$aes_cfb128_dec_mid:
	vbroadcasti32x4	zmm17,XMMWORD[rcx]
	vbroadcasti32x4	zmm18,XMMWORD[16+rcx]
	vbroadcasti32x4	zmm19,XMMWORD[32+rcx]
	vbroadcasti32x4	zmm20,XMMWORD[48+rcx]
	vbroadcasti32x4	zmm21,XMMWORD[64+rcx]
	vbroadcasti32x4	zmm22,XMMWORD[80+rcx]
	vbroadcasti32x4	zmm23,XMMWORD[96+rcx]
	vbroadcasti32x4	zmm24,XMMWORD[112+rcx]
	vbroadcasti32x4	zmm25,XMMWORD[128+rcx]
	vbroadcasti32x4	zmm26,XMMWORD[144+rcx]
	vbroadcasti32x4	zmm27,XMMWORD[160+rcx]
	vbroadcasti32x4	zmm28,XMMWORD[176+rcx]
	vbroadcasti32x4	zmm29,XMMWORD[192+rcx]
	vbroadcasti32x4	zmm30,XMMWORD[208+rcx]
	vbroadcasti32x4	zmm31,XMMWORD[224+rcx]

	mov	r11d,DWORD[240+rcx]






	vbroadcasti32x4	zmm2,XMMWORD[r8]

	cmp	rdx,0x100
	jb	NEAR $L$aes_cfb128_dec_check_4x







$L$oop_aes_cfb128_dec_mid_16x:
	sub	rdx,0x100




	vmovdqu32	zmm3,ZMMWORD[rdi]

	vmovdqu32	zmm5,ZMMWORD[64+rdi]

	vmovdqu32	zmm1,ZMMWORD[128+rdi]

	vmovdqu32	zmm16,ZMMWORD[192+rdi]


	valignq	zmm2,zmm3,zmm2,6

	valignq	zmm4,zmm5,zmm3,6

	valignq	zmm0,zmm1,zmm5,6

	valignq	zmm6,zmm16,zmm1,6

	lea	rdi,[256+rdi]
	vpxord	zmm2,zmm2,zmm17
	vpxord	zmm4,zmm4,zmm17
	vpxord	zmm0,zmm0,zmm17
	vpxord	zmm6,zmm6,zmm17

	vaesenc	zmm2,zmm2,zmm18
	vaesenc	zmm4,zmm4,zmm18
	vaesenc	zmm0,zmm0,zmm18
	vaesenc	zmm6,zmm6,zmm18

	vaesenc	zmm2,zmm2,zmm19
	vaesenc	zmm4,zmm4,zmm19
	vaesenc	zmm0,zmm0,zmm19
	vaesenc	zmm6,zmm6,zmm19

	vaesenc	zmm2,zmm2,zmm20
	vaesenc	zmm4,zmm4,zmm20
	vaesenc	zmm0,zmm0,zmm20
	vaesenc	zmm6,zmm6,zmm20

	vaesenc	zmm2,zmm2,zmm21
	vaesenc	zmm4,zmm4,zmm21
	vaesenc	zmm0,zmm0,zmm21
	vaesenc	zmm6,zmm6,zmm21

	vaesenc	zmm2,zmm2,zmm22
	vaesenc	zmm4,zmm4,zmm22
	vaesenc	zmm0,zmm0,zmm22
	vaesenc	zmm6,zmm6,zmm22

	vaesenc	zmm2,zmm2,zmm23
	vaesenc	zmm4,zmm4,zmm23
	vaesenc	zmm0,zmm0,zmm23
	vaesenc	zmm6,zmm6,zmm23

	vaesenc	zmm2,zmm2,zmm24
	vaesenc	zmm4,zmm4,zmm24
	vaesenc	zmm0,zmm0,zmm24
	vaesenc	zmm6,zmm6,zmm24

	vaesenc	zmm2,zmm2,zmm25
	vaesenc	zmm4,zmm4,zmm25
	vaesenc	zmm0,zmm0,zmm25
	vaesenc	zmm6,zmm6,zmm25

	vaesenc	zmm2,zmm2,zmm26
	vaesenc	zmm4,zmm4,zmm26
	vaesenc	zmm0,zmm0,zmm26
	vaesenc	zmm6,zmm6,zmm26

	cmp	r11d,0x09
	ja	NEAR $L$aes_cfb128_dec_mid_16x_192_256

	vaesenclast	zmm2,zmm2,zmm27
	vaesenclast	zmm4,zmm4,zmm27
	vaesenclast	zmm0,zmm0,zmm27
	vaesenclast	zmm6,zmm6,zmm27
	jmp	NEAR $L$aes_cfb128_dec_mid_16x_end


$L$aes_cfb128_dec_mid_16x_192_256:

	vaesenc	zmm2,zmm2,zmm27
	vaesenc	zmm4,zmm4,zmm27
	vaesenc	zmm0,zmm0,zmm27
	vaesenc	zmm6,zmm6,zmm27

	vaesenc	zmm2,zmm2,zmm28
	vaesenc	zmm4,zmm4,zmm28
	vaesenc	zmm0,zmm0,zmm28
	vaesenc	zmm6,zmm6,zmm28

	cmp	r11d,0x0B
	ja	NEAR $L$aes_cfb128_dec_mid_16x_256

	vaesenclast	zmm2,zmm2,zmm29
	vaesenclast	zmm4,zmm4,zmm29
	vaesenclast	zmm0,zmm0,zmm29
	vaesenclast	zmm6,zmm6,zmm29
	jmp	NEAR $L$aes_cfb128_dec_mid_16x_end


$L$aes_cfb128_dec_mid_16x_256:

	vaesenc	zmm2,zmm2,zmm29
	vaesenc	zmm4,zmm4,zmm29
	vaesenc	zmm0,zmm0,zmm29
	vaesenc	zmm6,zmm6,zmm29

	vaesenc	zmm2,zmm2,zmm30
	vaesenc	zmm4,zmm4,zmm30
	vaesenc	zmm0,zmm0,zmm30
	vaesenc	zmm6,zmm6,zmm30

	vaesenclast	zmm2,zmm2,zmm31
	vaesenclast	zmm4,zmm4,zmm31
	vaesenclast	zmm0,zmm0,zmm31
	vaesenclast	zmm6,zmm6,zmm31


$L$aes_cfb128_dec_mid_16x_end:

	vpxord	zmm2,zmm2,zmm3
	vpxord	zmm4,zmm4,zmm5
	vpxord	zmm0,zmm0,zmm1
	vpxord	zmm6,zmm6,zmm16

	cmp	rdx,0x100

	vmovdqu32	ZMMWORD[rsi],zmm2
	vmovdqu32	ZMMWORD[64+rsi],zmm4
	vmovdqu32	ZMMWORD[128+rsi],zmm0
	vmovdqu32	ZMMWORD[192+rsi],zmm6

	vmovdqu8	zmm2,zmm16

	lea	rsi,[256+rsi]

	jae	NEAR $L$oop_aes_cfb128_dec_mid_16x

	vextracti64x2	xmm2,zmm16,3
	vinserti32x4	zmm2,zmm2,xmm2,3

	xor	eax,eax

	vmovdqu	XMMWORD[r8],xmm2

$L$aes_cfb128_dec_check_4x:
	cmp	rdx,0x40
	jb	NEAR $L$aes_cfb128_dec_check_1x









$L$oop_aes_cfb128_dec_mid_4x:
	sub	rdx,0x40


	vmovdqu32	zmm3,ZMMWORD[rdi]


	valignq	zmm2,zmm3,zmm2,6

	lea	rdi,[64+rdi]
	vpxord	zmm2,zmm2,zmm17
	vaesenc	zmm2,zmm2,zmm18
	vaesenc	zmm2,zmm2,zmm19
	vaesenc	zmm2,zmm2,zmm20
	vaesenc	zmm2,zmm2,zmm21
	vaesenc	zmm2,zmm2,zmm22
	vaesenc	zmm2,zmm2,zmm23
	vaesenc	zmm2,zmm2,zmm24
	vaesenc	zmm2,zmm2,zmm25
	vaesenc	zmm2,zmm2,zmm26

	cmp	r11d,0x09
	ja	NEAR $L$aes_cfb128_dec_mid_4x_192_256

	vaesenclast	zmm2,zmm2,zmm27
	jmp	NEAR $L$aes_cfb128_dec_mid_4x_end


$L$aes_cfb128_dec_mid_4x_192_256:

	vaesenc	zmm2,zmm2,zmm27
	vaesenc	zmm2,zmm2,zmm28

	cmp	r11d,0x0B
	ja	NEAR $L$aes_cfb128_dec_mid_4x_256

	vaesenclast	zmm2,zmm2,zmm29
	jmp	NEAR $L$aes_cfb128_dec_mid_4x_end


$L$aes_cfb128_dec_mid_4x_256:

	vaesenc	zmm2,zmm2,zmm29
	vaesenc	zmm2,zmm2,zmm30
	vaesenclast	zmm2,zmm2,zmm31


$L$aes_cfb128_dec_mid_4x_end:
	vpxord	zmm2,zmm2,zmm3
	cmp	rdx,0x40
	vmovdqu32	ZMMWORD[rsi],zmm2
	vmovdqu8	zmm2,zmm3
	lea	rsi,[64+rsi]

	jae	NEAR $L$oop_aes_cfb128_dec_mid_4x

	vextracti64x2	xmm2,zmm2,3


	xor	eax,eax

	vmovdqu	XMMWORD[r8],xmm2

$L$aes_cfb128_dec_check_1x:
	cmp	rdx,0x10
	jb	NEAR $L$aes_cfb128_dec_post








$L$oop_aes_cfb128_dec_mid_1x:
	sub	rdx,0x10

	vmovdqu	xmm3,XMMWORD[rdi]
	lea	rdi,[16+rdi]
	vpxord	xmm2,xmm2,xmm17
	vaesenc	xmm2,xmm2,xmm18
	vaesenc	xmm2,xmm2,xmm19
	vaesenc	xmm2,xmm2,xmm20
	vaesenc	xmm2,xmm2,xmm21
	vaesenc	xmm2,xmm2,xmm22
	vaesenc	xmm2,xmm2,xmm23
	vaesenc	xmm2,xmm2,xmm24
	vaesenc	xmm2,xmm2,xmm25
	vaesenc	xmm2,xmm2,xmm26

	cmp	r11d,0x09
	ja	NEAR $L$oop_aes_cfb128_dec_mid_1x_inner_192_256

	vaesenclast	xmm2,xmm2,xmm27
	jmp	NEAR $L$oop_aes_cfb128_dec_mid_1x_inner_end


$L$oop_aes_cfb128_dec_mid_1x_inner_192_256:

	vaesenc	xmm2,xmm2,xmm27
	vaesenc	xmm2,xmm2,xmm28

	cmp	r11d,0x0B
	ja	NEAR $L$oop_aes_cfb128_dec_mid_1x_inner_256

	vaesenclast	xmm2,xmm2,xmm29
	jmp	NEAR $L$oop_aes_cfb128_dec_mid_1x_inner_end


$L$oop_aes_cfb128_dec_mid_1x_inner_256:

	vaesenc	xmm2,xmm2,xmm29
	vaesenc	xmm2,xmm2,xmm30
	vaesenclast	xmm2,xmm2,xmm31


$L$oop_aes_cfb128_dec_mid_1x_inner_end:
	vpxor	xmm2,xmm2,xmm3
	cmp	rdx,0x10
	vmovdqu	XMMWORD[rsi],xmm2
	vmovdqu8	xmm2,xmm3
	lea	rsi,[16+rsi]
	jae	NEAR $L$oop_aes_cfb128_dec_mid_1x

	xor	eax,eax

	vmovdqu	XMMWORD[r8],xmm2

$L$aes_cfb128_dec_post:





	test	rdx,rdx
	jz	NEAR $L$aes_cfb128_dec_zero_all
	vpxord	xmm2,xmm2,xmm17
	vaesenc	xmm2,xmm2,xmm18
	vaesenc	xmm2,xmm2,xmm19
	vaesenc	xmm2,xmm2,xmm20
	vaesenc	xmm2,xmm2,xmm21
	vaesenc	xmm2,xmm2,xmm22
	vaesenc	xmm2,xmm2,xmm23
	vaesenc	xmm2,xmm2,xmm24
	vaesenc	xmm2,xmm2,xmm25
	vaesenc	xmm2,xmm2,xmm26

	cmp	r11d,0x09
	ja	NEAR $L$oop_aes_cfb128_dec_post_192_256

	vaesenclast	xmm2,xmm2,xmm27
	jmp	NEAR $L$oop_aes_cfb128_dec_post_end


$L$oop_aes_cfb128_dec_post_192_256:

	vaesenc	xmm2,xmm2,xmm27
	vaesenc	xmm2,xmm2,xmm28

	cmp	r11d,0x0B
	ja	NEAR $L$oop_aes_cfb128_dec_post_256

	vaesenclast	xmm2,xmm2,xmm29
	jmp	NEAR $L$oop_aes_cfb128_dec_post_end


$L$oop_aes_cfb128_dec_post_256:

	vaesenc	xmm2,xmm2,xmm29
	vaesenc	xmm2,xmm2,xmm30
	vaesenclast	xmm2,xmm2,xmm31


$L$oop_aes_cfb128_dec_post_end:

	mov	rax,rdx
	mov	r11,1
	mov	cl,dl
	shl	r11,cl
	dec	r11
	kmovq	k1,r11

	vmovdqu8	xmm1{k1}{z},[rdi]
	vpxor	xmm0,xmm1,xmm2
	vmovdqu8	XMMWORD[rsi]{k1},xmm0
	vpblendmb	xmm2{k1},xmm2,xmm1

	vmovdqu8	XMMWORD[r8],xmm2



$L$aes_cfb128_dec_zero_all:
	vpxord	xmm17,xmm17,xmm17
	vpxord	xmm18,xmm18,xmm18
	vpxord	xmm19,xmm19,xmm19
	vpxord	xmm20,xmm20,xmm20
	vpxord	xmm21,xmm21,xmm21
	vpxord	xmm22,xmm22,xmm22
	vpxord	xmm23,xmm23,xmm23
	vpxord	xmm24,xmm24,xmm24
	vpxord	xmm25,xmm25,xmm25
	vpxord	xmm26,xmm26,xmm26
	vpxord	xmm27,xmm27,xmm27
	vpxord	xmm28,xmm28,xmm28
	vpxord	xmm29,xmm29,xmm29
	vpxord	xmm30,xmm30,xmm30
	vpxord	xmm31,xmm31,xmm31

	vpxord	xmm3,xmm3,xmm3
	vpxord	xmm4,xmm4,xmm4
	vpxord	xmm5,xmm5,xmm5
	vpxord	xmm6,xmm6,xmm6
	vpxord	xmm16,xmm16,xmm16

$L$aes_cfb128_dec_zero_pre:

	vpxord	xmm0,xmm0,xmm0
	vpxord	xmm1,xmm1,xmm1
	vpxord	xmm2,xmm2,xmm2

	vzeroupper
	vmovdqu	xmm6,XMMWORD[rsp]
	add	rsp,16

	mov	QWORD[r9],rax

$L$aes_cfb128_vaes_dec_done:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_aes_cfb128_vaes_dec:
