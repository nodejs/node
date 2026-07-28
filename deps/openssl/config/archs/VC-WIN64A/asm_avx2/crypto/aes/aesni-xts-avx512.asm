default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64

EXTERN	OPENSSL_ia32cap_P
global	aesni_xts_avx512_eligible

ALIGN	32
aesni_xts_avx512_eligible:
	mov	ecx,DWORD[((OPENSSL_ia32cap_P+8))]
	xor	eax,eax

	and	ecx,0xc0030000
	cmp	ecx,0xc0030000
	jne	NEAR $L$_done
	mov	ecx,DWORD[((OPENSSL_ia32cap_P+12))]

	and	ecx,0x640
	cmp	ecx,0x640
	cmove	eax,ecx
$L$_done:
	DB	0F3h,0C3h		;repret

global	aesni_xts_128_encrypt_avx512


ALIGN	32
aesni_xts_128_encrypt_avx512:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_128_encrypt_avx512:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250
	push	rbp
	mov	rbp,rsp
	sub	rsp,312
	and	rsp,0xffffffffffffffc0
	mov	QWORD[288+rsp],rbx
	mov	QWORD[((288 + 8))+rsp],rdi
	mov	QWORD[((288 + 16))+rsp],rsi
	vmovdqa	XMMWORD[(128 + 0)+rsp],xmm6
	vmovdqa	XMMWORD[(128 + 16)+rsp],xmm7
	vmovdqa	XMMWORD[(128 + 32)+rsp],xmm8
	vmovdqa	XMMWORD[(128 + 48)+rsp],xmm9
	vmovdqa	XMMWORD[(128 + 64)+rsp],xmm10
	vmovdqa	XMMWORD[(128 + 80)+rsp],xmm11
	vmovdqa	XMMWORD[(128 + 96)+rsp],xmm12
	vmovdqa	XMMWORD[(128 + 112)+rsp],xmm13
	vmovdqa	XMMWORD[(128 + 128)+rsp],xmm14
	vmovdqa	XMMWORD[(128 + 144)+rsp],xmm15
	mov	r10,0x87
	vmovdqu	xmm1,XMMWORD[r9]
	vpxor	xmm1,xmm1,XMMWORD[r8]
	vaesenc	xmm1,xmm1,XMMWORD[16+r8]
	vaesenc	xmm1,xmm1,XMMWORD[32+r8]
	vaesenc	xmm1,xmm1,XMMWORD[48+r8]
	vaesenc	xmm1,xmm1,XMMWORD[64+r8]
	vaesenc	xmm1,xmm1,XMMWORD[80+r8]
	vaesenc	xmm1,xmm1,XMMWORD[96+r8]
	vaesenc	xmm1,xmm1,XMMWORD[112+r8]
	vaesenc	xmm1,xmm1,XMMWORD[128+r8]
	vaesenc	xmm1,xmm1,XMMWORD[144+r8]
	vaesenclast	xmm1,xmm1,XMMWORD[160+r8]
	vmovdqa	XMMWORD[rsp],xmm1
	mov	QWORD[((8 + 40))+rbp],rdi
	mov	QWORD[((8 + 48))+rbp],rsi

	cmp	rdx,0x80
	jl	NEAR $L$_less_than_128_bytes_hEgxyDlCngwrfFe
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x100
	jge	NEAR $L$_start_by16_hEgxyDlCngwrfFe
	cmp	rdx,0x80
	jge	NEAR $L$_start_by8_hEgxyDlCngwrfFe

$L$_do_n_blocks_hEgxyDlCngwrfFe:
	cmp	rdx,0x0
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	cmp	rdx,0x70
	jge	NEAR $L$_remaining_num_blocks_is_7_hEgxyDlCngwrfFe
	cmp	rdx,0x60
	jge	NEAR $L$_remaining_num_blocks_is_6_hEgxyDlCngwrfFe
	cmp	rdx,0x50
	jge	NEAR $L$_remaining_num_blocks_is_5_hEgxyDlCngwrfFe
	cmp	rdx,0x40
	jge	NEAR $L$_remaining_num_blocks_is_4_hEgxyDlCngwrfFe
	cmp	rdx,0x30
	jge	NEAR $L$_remaining_num_blocks_is_3_hEgxyDlCngwrfFe
	cmp	rdx,0x20
	jge	NEAR $L$_remaining_num_blocks_is_2_hEgxyDlCngwrfFe
	cmp	rdx,0x10
	jge	NEAR $L$_remaining_num_blocks_is_1_hEgxyDlCngwrfFe
	vmovdqa	xmm8,xmm0
	vmovdqa	xmm0,xmm9
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe

$L$_remaining_num_blocks_is_7_hEgxyDlCngwrfFe:
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]
	add	rdi,0x70
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vextracti32x4	xmm0,zmm10,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe

$L$_remaining_num_blocks_is_6_hEgxyDlCngwrfFe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,0x60
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,0x60
	vextracti32x4	xmm8,zmm2,0x1
	vextracti32x4	xmm0,zmm10,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe

$L$_remaining_num_blocks_is_5_hEgxyDlCngwrfFe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[64+rdi]
	add	rdi,0x50
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu	XMMWORD[64+rsi],xmm2
	add	rsi,0x50
	vmovdqa	xmm8,xmm2
	vextracti32x4	xmm0,zmm10,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe

$L$_remaining_num_blocks_is_4_hEgxyDlCngwrfFe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,0x40
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,0x40
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa64	xmm0,xmm10
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_remaining_num_blocks_is_3_hEgxyDlCngwrfFe:
	mov	r8,-1
	shr	r8,0x10
	kmovq	k1,r8
	vmovdqu8	zmm1{k1},[rdi]
	add	rdi,0x30
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi]{k1},zmm1
	add	rsi,0x30
	vextracti32x4	xmm8,zmm1,0x2
	vextracti32x4	xmm0,zmm9,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_remaining_num_blocks_is_2_hEgxyDlCngwrfFe:
	vmovdqu8	ymm1,YMMWORD[rdi]
	add	rdi,0x20
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu	YMMWORD[rsi],ymm1
	add	rsi,0x20
	vextracti32x4	xmm8,zmm1,0x1
	vextracti32x4	xmm0,zmm9,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_remaining_num_blocks_is_1_hEgxyDlCngwrfFe:
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rcx]
	vaesenclast	xmm1,xmm1,XMMWORD[160+rcx]
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	vextracti32x4	xmm0,zmm9,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe


$L$_start_by16_hEgxyDlCngwrfFe:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm11,zmm9,0x1
	vpxord	zmm11,zmm11,zmm14
	vpsrldq	zmm15,zmm10,0xf
	vpclmulqdq	zmm16,zmm15,zmm25,0x0
	vpslldq	zmm12,zmm10,0x1
	vpxord	zmm12,zmm12,zmm16

$L$_main_loop_run_16_hEgxyDlCngwrfFe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	zmm3,ZMMWORD[128+rdi]
	vmovdqu8	zmm4,ZMMWORD[192+rdi]
	add	rdi,0x100
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpxorq	zmm3,zmm3,zmm0
	vpxorq	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm11,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm11,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm12,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm12,0x1
	vpxord	zmm16,zmm16,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm15,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm17,zmm15,0x1
	vpxord	zmm17,zmm17,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm16,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm18,zmm16,0x1
	vpxord	zmm18,zmm18,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vaesenclast	zmm3,zmm3,zmm0
	vaesenclast	zmm4,zmm4,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12

	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqa32	zmm11,zmm17
	vmovdqa32	zmm12,zmm18
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	vmovdqu8	ZMMWORD[128+rsi],zmm3
	vmovdqu8	ZMMWORD[192+rsi],zmm4
	add	rsi,0x100
	sub	rdx,0x100
	cmp	rdx,0x100
	jae	NEAR $L$_main_loop_run_16_hEgxyDlCngwrfFe
	cmp	rdx,0x80
	jae	NEAR $L$_main_loop_run_8_hEgxyDlCngwrfFe
	vextracti32x4	xmm0,zmm4,0x3
	jmp	NEAR $L$_do_n_blocks_hEgxyDlCngwrfFe

$L$_start_by8_hEgxyDlCngwrfFe:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5

$L$_main_loop_run_8_hEgxyDlCngwrfFe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	add	rdi,0x80
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm9,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm10,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm10,0x1
	vpxord	zmm16,zmm16,zmm14

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	add	rsi,0x80
	sub	rdx,0x80
	cmp	rdx,0x80
	jae	NEAR $L$_main_loop_run_8_hEgxyDlCngwrfFe
	vextracti32x4	xmm0,zmm2,0x3
	jmp	NEAR $L$_do_n_blocks_hEgxyDlCngwrfFe

$L$_steal_cipher_hEgxyDlCngwrfFe:
	vmovdqa	xmm2,xmm8
	lea	rax,[vpshufb_shf_table]
	vmovdqu	xmm10,XMMWORD[rdx*1+rax]
	vpshufb	xmm8,xmm8,xmm10
	vmovdqu	xmm3,XMMWORD[((-16))+rdx*1+rdi]
	vmovdqu	XMMWORD[(-16)+rdx*1+rsi],xmm8
	lea	rax,[vpshufb_shf_table]
	add	rax,16
	sub	rax,rdx
	vmovdqu	xmm10,XMMWORD[rax]
	vpxor	xmm10,xmm10,XMMWORD[mask1]
	vpshufb	xmm3,xmm3,xmm10
	vpblendvb	xmm3,xmm3,xmm2,xmm10
	vpxor	xmm8,xmm3,xmm0
	vpxor	xmm8,xmm8,XMMWORD[rcx]
	vaesenc	xmm8,xmm8,XMMWORD[16+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[32+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[48+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[64+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[80+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[96+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[112+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[128+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[144+rcx]
	vaesenclast	xmm8,xmm8,XMMWORD[160+rcx]
	vpxor	xmm8,xmm8,xmm0
	vmovdqu	XMMWORD[(-16)+rsi],xmm8
$L$_ret_hEgxyDlCngwrfFe:
	mov	rbx,QWORD[288+rsp]
	xor	r8,r8
	mov	QWORD[288+rsp],r8

	vpxorq	zmm0,zmm0,zmm0
	mov	rdi,QWORD[((288 + 8))+rsp]
	mov	QWORD[((288 + 8))+rsp],r8
	mov	rsi,QWORD[((288 + 16))+rsp]
	mov	QWORD[((288 + 16))+rsp],r8

	vmovdqa	xmm6,XMMWORD[((128 + 0))+rsp]
	vmovdqa	xmm7,XMMWORD[((128 + 16))+rsp]
	vmovdqa	xmm8,XMMWORD[((128 + 32))+rsp]
	vmovdqa	xmm9,XMMWORD[((128 + 48))+rsp]


	vmovdqa64	ZMMWORD[128+rsp],zmm0

	vmovdqa	xmm10,XMMWORD[((128 + 64))+rsp]
	vmovdqa	xmm11,XMMWORD[((128 + 80))+rsp]
	vmovdqa	xmm12,XMMWORD[((128 + 96))+rsp]
	vmovdqa	xmm13,XMMWORD[((128 + 112))+rsp]


	vmovdqa64	ZMMWORD[(128 + 64)+rsp],zmm0

	vmovdqa	xmm14,XMMWORD[((128 + 128))+rsp]
	vmovdqa	xmm15,XMMWORD[((128 + 144))+rsp]



	vmovdqa	YMMWORD[(128 + 128)+rsp],ymm0
	mov	rsp,rbp
	pop	rbp
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$_less_than_128_bytes_hEgxyDlCngwrfFe:
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x10
	jb	NEAR $L$_ret_hEgxyDlCngwrfFe
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8d,0xaa
	kmovq	k2,r8
	mov	r8,rdx
	and	r8,0x70
	cmp	r8,0x60
	je	NEAR $L$_num_blocks_is_6_hEgxyDlCngwrfFe
	cmp	r8,0x50
	je	NEAR $L$_num_blocks_is_5_hEgxyDlCngwrfFe
	cmp	r8,0x40
	je	NEAR $L$_num_blocks_is_4_hEgxyDlCngwrfFe
	cmp	r8,0x30
	je	NEAR $L$_num_blocks_is_3_hEgxyDlCngwrfFe
	cmp	r8,0x20
	je	NEAR $L$_num_blocks_is_2_hEgxyDlCngwrfFe
	cmp	r8,0x10
	je	NEAR $L$_num_blocks_is_1_hEgxyDlCngwrfFe

$L$_num_blocks_is_7_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]

	add	rdi,0x70
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vextracti32x4	xmm0,zmm10,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_6_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,96
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,96

	vextracti32x4	xmm8,ymm2,0x1
	vextracti32x4	xmm0,zmm10,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_5_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	xmm2,XMMWORD[64+rdi]
	add	rdi,80
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	XMMWORD[64+rsi],xmm2
	add	rsi,80

	vmovdqa	xmm8,xmm2
	vextracti32x4	xmm0,zmm10,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_4_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,64
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,64
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa	xmm0,xmm10
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_3_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1{k1},[rdi]
	add	rdi,48
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi]{k1},zmm1
	add	rsi,48
	vextracti32x4	xmm8,zmm1,2
	vextracti32x4	xmm0,zmm9,3
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_2_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4

	vmovdqu8	ymm1,YMMWORD[rdi]
	add	rdi,32
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu8	YMMWORD[rsi],ymm1
	add	rsi,32

	vextracti32x4	xmm8,ymm1,1
	vextracti32x4	xmm0,zmm9,2
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe
$L$_num_blocks_is_1_hEgxyDlCngwrfFe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4

	vmovdqu8	xmm1,XMMWORD[rdi]
	add	rdi,16
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu8	XMMWORD[rsi],xmm1
	add	rsi,16

	vmovdqa	xmm8,xmm1
	vextracti32x4	xmm0,zmm9,1
	and	rdx,0xf
	je	NEAR $L$_ret_hEgxyDlCngwrfFe
	jmp	NEAR $L$_steal_cipher_hEgxyDlCngwrfFe

global	aesni_xts_128_decrypt_avx512


ALIGN	32
aesni_xts_128_decrypt_avx512:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_128_decrypt_avx512:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250
	push	rbp
	mov	rbp,rsp
	sub	rsp,312
	and	rsp,0xffffffffffffffc0
	mov	QWORD[288+rsp],rbx
	mov	QWORD[((288 + 8))+rsp],rdi
	mov	QWORD[((288 + 16))+rsp],rsi
	vmovdqa	XMMWORD[(128 + 0)+rsp],xmm6
	vmovdqa	XMMWORD[(128 + 16)+rsp],xmm7
	vmovdqa	XMMWORD[(128 + 32)+rsp],xmm8
	vmovdqa	XMMWORD[(128 + 48)+rsp],xmm9
	vmovdqa	XMMWORD[(128 + 64)+rsp],xmm10
	vmovdqa	XMMWORD[(128 + 80)+rsp],xmm11
	vmovdqa	XMMWORD[(128 + 96)+rsp],xmm12
	vmovdqa	XMMWORD[(128 + 112)+rsp],xmm13
	vmovdqa	XMMWORD[(128 + 128)+rsp],xmm14
	vmovdqa	XMMWORD[(128 + 144)+rsp],xmm15
	mov	r10,0x87
	vmovdqu	xmm1,XMMWORD[r9]
	vpxor	xmm1,xmm1,XMMWORD[r8]
	vaesenc	xmm1,xmm1,XMMWORD[16+r8]
	vaesenc	xmm1,xmm1,XMMWORD[32+r8]
	vaesenc	xmm1,xmm1,XMMWORD[48+r8]
	vaesenc	xmm1,xmm1,XMMWORD[64+r8]
	vaesenc	xmm1,xmm1,XMMWORD[80+r8]
	vaesenc	xmm1,xmm1,XMMWORD[96+r8]
	vaesenc	xmm1,xmm1,XMMWORD[112+r8]
	vaesenc	xmm1,xmm1,XMMWORD[128+r8]
	vaesenc	xmm1,xmm1,XMMWORD[144+r8]
	vaesenclast	xmm1,xmm1,XMMWORD[160+r8]
	vmovdqa	XMMWORD[rsp],xmm1
	mov	QWORD[((8 + 40))+rbp],rdi
	mov	QWORD[((8 + 48))+rbp],rsi

	cmp	rdx,0x80
	jb	NEAR $L$_less_than_128_bytes_amivrujEyduiFoi
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x100
	jge	NEAR $L$_start_by16_amivrujEyduiFoi
	jmp	NEAR $L$_start_by8_amivrujEyduiFoi

$L$_do_n_blocks_amivrujEyduiFoi:
	cmp	rdx,0x0
	je	NEAR $L$_ret_amivrujEyduiFoi
	cmp	rdx,0x70
	jge	NEAR $L$_remaining_num_blocks_is_7_amivrujEyduiFoi
	cmp	rdx,0x60
	jge	NEAR $L$_remaining_num_blocks_is_6_amivrujEyduiFoi
	cmp	rdx,0x50
	jge	NEAR $L$_remaining_num_blocks_is_5_amivrujEyduiFoi
	cmp	rdx,0x40
	jge	NEAR $L$_remaining_num_blocks_is_4_amivrujEyduiFoi
	cmp	rdx,0x30
	jge	NEAR $L$_remaining_num_blocks_is_3_amivrujEyduiFoi
	cmp	rdx,0x20
	jge	NEAR $L$_remaining_num_blocks_is_2_amivrujEyduiFoi
	cmp	rdx,0x10
	jge	NEAR $L$_remaining_num_blocks_is_1_amivrujEyduiFoi


	vmovdqu	xmm1,xmm5

	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[(-16)+rsi],xmm1
	vmovdqa	xmm8,xmm1


	mov	r8,0x1
	kmovq	k1,r8
	vpsllq	xmm13,xmm9,0x3f
	vpsraq	xmm14,xmm13,0x3f
	vpandq	xmm5,xmm14,xmm25
	vpxorq	xmm9{k1},xmm9,xmm5
	vpsrldq	xmm10,xmm9,0x8
DB	98,211,181,8,115,194,1
	vpslldq	xmm13,xmm13,0x8
	vpxorq	xmm0,xmm0,xmm13
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_remaining_num_blocks_is_7_amivrujEyduiFoi:
	mov	r8,0xffffffffffffffff
	shr	r8,0x10
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]
	add	rdi,0x70
	and	rdx,0xf
	je	NEAR $L$_done_7_remain_amivrujEyduiFoi
	vextracti32x4	xmm12,zmm10,0x2
	vextracti32x4	xmm13,zmm10,0x3
	vinserti32x4	zmm10,zmm10,xmm13,0x2

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_7_remain_amivrujEyduiFoi:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_6_amivrujEyduiFoi:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,0x60
	and	rdx,0xf
	je	NEAR $L$_done_6_remain_amivrujEyduiFoi
	vextracti32x4	xmm12,zmm10,0x1
	vextracti32x4	xmm13,zmm10,0x2
	vinserti32x4	zmm10,zmm10,xmm13,0x1

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,0x60
	vextracti32x4	xmm8,zmm2,0x1
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_6_remain_amivrujEyduiFoi:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_5_amivrujEyduiFoi:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[64+rdi]
	add	rdi,0x50
	and	rdx,0xf
	je	NEAR $L$_done_5_remain_amivrujEyduiFoi
	vmovdqa	xmm12,xmm10
	vextracti32x4	xmm10,zmm10,0x1

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu	XMMWORD[64+rsi],xmm2
	add	rsi,0x50
	vmovdqa	xmm8,xmm2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_5_remain_amivrujEyduiFoi:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	XMMWORD[64+rsi],xmm2
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_4_amivrujEyduiFoi:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,0x40
	and	rdx,0xf
	je	NEAR $L$_done_4_remain_amivrujEyduiFoi
	vextracti32x4	xmm12,zmm9,0x3
	vinserti32x4	zmm9,zmm9,xmm10,0x3

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,0x40
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_4_remain_amivrujEyduiFoi:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_3_amivrujEyduiFoi:
	vmovdqu	xmm1,XMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	add	rdi,0x30
	and	rdx,0xf
	je	NEAR $L$_done_3_remain_amivrujEyduiFoi
	vextracti32x4	xmm13,zmm9,0x2
	vextracti32x4	xmm10,zmm9,0x1
	vextracti32x4	xmm11,zmm9,0x3
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x30
	vmovdqa	xmm8,xmm3
	vmovdqa	xmm0,xmm13
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_3_remain_amivrujEyduiFoi:
	vextracti32x4	xmm10,zmm9,0x1
	vextracti32x4	xmm11,zmm9,0x2
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_2_amivrujEyduiFoi:
	vmovdqu	xmm1,XMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	add	rdi,0x20
	and	rdx,0xf
	je	NEAR $L$_done_2_remain_amivrujEyduiFoi
	vextracti32x4	xmm10,zmm9,0x2
	vextracti32x4	xmm12,zmm9,0x1
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x20
	vmovdqa	xmm8,xmm2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_2_remain_amivrujEyduiFoi:
	vextracti32x4	xmm10,zmm9,0x1
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_remaining_num_blocks_is_1_amivrujEyduiFoi:
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	and	rdx,0xf
	je	NEAR $L$_done_1_remain_amivrujEyduiFoi
	vextracti32x4	xmm11,zmm9,0x1
	vpxor	xmm1,xmm1,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	vmovdqa	xmm0,xmm9
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_1_remain_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[rsi],xmm1
	jmp	NEAR $L$_ret_amivrujEyduiFoi

$L$_start_by16_amivrujEyduiFoi:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8


	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4


	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5


	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm11,zmm9,0x1
	vpxord	zmm11,zmm11,zmm14

	vpsrldq	zmm15,zmm10,0xf
	vpclmulqdq	zmm16,zmm15,zmm25,0x0
	vpslldq	zmm12,zmm10,0x1
	vpxord	zmm12,zmm12,zmm16

$L$_main_loop_run_16_amivrujEyduiFoi:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	zmm3,ZMMWORD[128+rdi]
	vmovdqu8	zmm4,ZMMWORD[192+rdi]
	vmovdqu8	xmm5,XMMWORD[240+rdi]
	add	rdi,0x100
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpxorq	zmm3,zmm3,zmm0
	vpxorq	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm11,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm11,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm12,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm12,0x1
	vpxord	zmm16,zmm16,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm15,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm17,zmm15,0x1
	vpxord	zmm17,zmm17,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm16,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm18,zmm16,0x1
	vpxord	zmm18,zmm18,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0
	vaesdeclast	zmm3,zmm3,zmm0
	vaesdeclast	zmm4,zmm4,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12

	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqa32	zmm11,zmm17
	vmovdqa32	zmm12,zmm18
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	vmovdqu8	ZMMWORD[128+rsi],zmm3
	vmovdqu8	ZMMWORD[192+rsi],zmm4
	add	rsi,0x100
	sub	rdx,0x100
	cmp	rdx,0x100
	jge	NEAR $L$_main_loop_run_16_amivrujEyduiFoi

	cmp	rdx,0x80
	jge	NEAR $L$_main_loop_run_8_amivrujEyduiFoi
	jmp	NEAR $L$_do_n_blocks_amivrujEyduiFoi

$L$_start_by8_amivrujEyduiFoi:

	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8


	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4


	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5

$L$_main_loop_run_8_amivrujEyduiFoi:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	xmm5,XMMWORD[112+rdi]
	add	rdi,0x80

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm9,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm10,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm10,0x1
	vpxord	zmm16,zmm16,zmm14

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	add	rsi,0x80
	sub	rdx,0x80
	cmp	rdx,0x80
	jge	NEAR $L$_main_loop_run_8_amivrujEyduiFoi
	jmp	NEAR $L$_do_n_blocks_amivrujEyduiFoi

$L$_steal_cipher_amivrujEyduiFoi:

	vmovdqa	xmm2,xmm8


	lea	rax,[vpshufb_shf_table]
	vmovdqu	xmm10,XMMWORD[rdx*1+rax]
	vpshufb	xmm8,xmm8,xmm10


	vmovdqu	xmm3,XMMWORD[((-16))+rdx*1+rdi]
	vmovdqu	XMMWORD[(-16)+rdx*1+rsi],xmm8


	lea	rax,[vpshufb_shf_table]
	add	rax,16
	sub	rax,rdx
	vmovdqu	xmm10,XMMWORD[rax]
	vpxor	xmm10,xmm10,XMMWORD[mask1]
	vpshufb	xmm3,xmm3,xmm10

	vpblendvb	xmm3,xmm3,xmm2,xmm10


	vpxor	xmm8,xmm3,xmm0


	vpxor	xmm8,xmm8,XMMWORD[rcx]
	vaesdec	xmm8,xmm8,XMMWORD[16+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[32+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[48+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[64+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[80+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[96+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[112+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[128+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[144+rcx]
	vaesdeclast	xmm8,xmm8,XMMWORD[160+rcx]

	vpxor	xmm8,xmm8,xmm0

$L$_done_amivrujEyduiFoi:

	vmovdqu	XMMWORD[(-16)+rsi],xmm8
$L$_ret_amivrujEyduiFoi:
	mov	rbx,QWORD[288+rsp]
	xor	r8,r8
	mov	QWORD[288+rsp],r8

	vpxorq	zmm0,zmm0,zmm0
	mov	rdi,QWORD[((288 + 8))+rsp]
	mov	QWORD[((288 + 8))+rsp],r8
	mov	rsi,QWORD[((288 + 16))+rsp]
	mov	QWORD[((288 + 16))+rsp],r8

	vmovdqa	xmm6,XMMWORD[((128 + 0))+rsp]
	vmovdqa	xmm7,XMMWORD[((128 + 16))+rsp]
	vmovdqa	xmm8,XMMWORD[((128 + 32))+rsp]
	vmovdqa	xmm9,XMMWORD[((128 + 48))+rsp]


	vmovdqa64	ZMMWORD[128+rsp],zmm0

	vmovdqa	xmm10,XMMWORD[((128 + 64))+rsp]
	vmovdqa	xmm11,XMMWORD[((128 + 80))+rsp]
	vmovdqa	xmm12,XMMWORD[((128 + 96))+rsp]
	vmovdqa	xmm13,XMMWORD[((128 + 112))+rsp]


	vmovdqa64	ZMMWORD[(128 + 64)+rsp],zmm0

	vmovdqa	xmm14,XMMWORD[((128 + 128))+rsp]
	vmovdqa	xmm15,XMMWORD[((128 + 144))+rsp]



	vmovdqa	YMMWORD[(128 + 128)+rsp],ymm0
	mov	rsp,rbp
	pop	rbp
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$_less_than_128_bytes_amivrujEyduiFoi:
	cmp	rdx,0x10
	jb	NEAR $L$_ret_amivrujEyduiFoi

	mov	r8,rdx
	and	r8,0x70
	cmp	r8,0x60
	je	NEAR $L$_num_blocks_is_6_amivrujEyduiFoi
	cmp	r8,0x50
	je	NEAR $L$_num_blocks_is_5_amivrujEyduiFoi
	cmp	r8,0x40
	je	NEAR $L$_num_blocks_is_4_amivrujEyduiFoi
	cmp	r8,0x30
	je	NEAR $L$_num_blocks_is_3_amivrujEyduiFoi
	cmp	r8,0x20
	je	NEAR $L$_num_blocks_is_2_amivrujEyduiFoi
	cmp	r8,0x10
	je	NEAR $L$_num_blocks_is_1_amivrujEyduiFoi

$L$_num_blocks_is_7_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[80+rsp],rax
	mov	QWORD[((80 + 8))+rsp],rbx
	vmovdqa	xmm14,XMMWORD[80+rsp]
	vmovdqu	xmm6,XMMWORD[80+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[96+rsp],rax
	mov	QWORD[((96 + 8))+rsp],rbx
	vmovdqa	xmm15,XMMWORD[96+rsp]
	vmovdqu	xmm7,XMMWORD[96+rdi]
	add	rdi,0x70
	and	rdx,0xf
	je	NEAR $L$_done_7_amivrujEyduiFoi

$L$_steal_cipher_7_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm16,xmm15
	vmovdqa	xmm15,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vpxor	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vaesdeclast	xmm7,xmm7,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	add	rsi,0x70
	vmovdqa64	xmm0,xmm16
	vmovdqa	xmm8,xmm7
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_7_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vpxor	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vaesdeclast	xmm7,xmm7,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	add	rsi,0x70
	vmovdqa	xmm8,xmm7
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_6_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[80+rsp],rax
	mov	QWORD[((80 + 8))+rsp],rbx
	vmovdqa	xmm14,XMMWORD[80+rsp]
	vmovdqu	xmm6,XMMWORD[80+rdi]
	add	rdi,0x60
	and	rdx,0xf
	je	NEAR $L$_done_6_amivrujEyduiFoi

$L$_steal_cipher_6_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm15,xmm14
	vmovdqa	xmm14,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	add	rsi,0x60
	vmovdqa	xmm0,xmm15
	vmovdqa	xmm8,xmm6
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_6_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	add	rsi,0x60
	vmovdqa	xmm8,xmm6
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_5_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	add	rdi,0x50
	and	rdx,0xf
	je	NEAR $L$_done_5_amivrujEyduiFoi

$L$_steal_cipher_5_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm14,xmm13
	vmovdqa	xmm13,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	add	rsi,0x50
	vmovdqa	xmm0,xmm14
	vmovdqa	xmm8,xmm5
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_5_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	add	rsi,0x50
	vmovdqa	xmm8,xmm5
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_4_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	add	rdi,0x40
	and	rdx,0xf
	je	NEAR $L$_done_4_amivrujEyduiFoi

$L$_steal_cipher_4_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm13,xmm12
	vmovdqa	xmm12,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x40
	vmovdqa	xmm0,xmm13
	vmovdqa	xmm8,xmm4
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_4_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x40
	vmovdqa	xmm8,xmm4
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_3_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	add	rdi,0x30
	and	rdx,0xf
	je	NEAR $L$_done_3_amivrujEyduiFoi

$L$_steal_cipher_3_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm12,xmm11
	vmovdqa	xmm11,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x30
	vmovdqa	xmm0,xmm12
	vmovdqa	xmm8,xmm3
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_3_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x30
	vmovdqa	xmm8,xmm3
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_2_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	add	rdi,0x20
	and	rdx,0xf
	je	NEAR $L$_done_2_amivrujEyduiFoi

$L$_steal_cipher_2_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm11,xmm10
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x20
	vmovdqa	xmm0,xmm11
	vmovdqa	xmm8,xmm2
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_2_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x20
	vmovdqa	xmm8,xmm2
	jmp	NEAR $L$_done_amivrujEyduiFoi

$L$_num_blocks_is_1_amivrujEyduiFoi:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	and	rdx,0xf
	je	NEAR $L$_done_1_amivrujEyduiFoi

$L$_steal_cipher_1_amivrujEyduiFoi:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm10,xmm9
	vmovdqa	xmm9,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	add	rsi,0x10
	vmovdqa	xmm0,xmm10
	vmovdqa	xmm8,xmm1
	jmp	NEAR $L$_steal_cipher_amivrujEyduiFoi

$L$_done_1_amivrujEyduiFoi:
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	jmp	NEAR $L$_done_amivrujEyduiFoi

global	aesni_xts_256_encrypt_avx512


ALIGN	32
aesni_xts_256_encrypt_avx512:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_256_encrypt_avx512:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250
	push	rbp
	mov	rbp,rsp
	sub	rsp,312
	and	rsp,0xffffffffffffffc0
	mov	QWORD[288+rsp],rbx
	mov	QWORD[((288 + 8))+rsp],rdi
	mov	QWORD[((288 + 16))+rsp],rsi
	vmovdqa	XMMWORD[(128 + 0)+rsp],xmm6
	vmovdqa	XMMWORD[(128 + 16)+rsp],xmm7
	vmovdqa	XMMWORD[(128 + 32)+rsp],xmm8
	vmovdqa	XMMWORD[(128 + 48)+rsp],xmm9
	vmovdqa	XMMWORD[(128 + 64)+rsp],xmm10
	vmovdqa	XMMWORD[(128 + 80)+rsp],xmm11
	vmovdqa	XMMWORD[(128 + 96)+rsp],xmm12
	vmovdqa	XMMWORD[(128 + 112)+rsp],xmm13
	vmovdqa	XMMWORD[(128 + 128)+rsp],xmm14
	vmovdqa	XMMWORD[(128 + 144)+rsp],xmm15
	mov	r10,0x87
	vmovdqu	xmm1,XMMWORD[r9]
	vpxor	xmm1,xmm1,XMMWORD[r8]
	vaesenc	xmm1,xmm1,XMMWORD[16+r8]
	vaesenc	xmm1,xmm1,XMMWORD[32+r8]
	vaesenc	xmm1,xmm1,XMMWORD[48+r8]
	vaesenc	xmm1,xmm1,XMMWORD[64+r8]
	vaesenc	xmm1,xmm1,XMMWORD[80+r8]
	vaesenc	xmm1,xmm1,XMMWORD[96+r8]
	vaesenc	xmm1,xmm1,XMMWORD[112+r8]
	vaesenc	xmm1,xmm1,XMMWORD[128+r8]
	vaesenc	xmm1,xmm1,XMMWORD[144+r8]
	vaesenc	xmm1,xmm1,XMMWORD[160+r8]
	vaesenc	xmm1,xmm1,XMMWORD[176+r8]
	vaesenc	xmm1,xmm1,XMMWORD[192+r8]
	vaesenc	xmm1,xmm1,XMMWORD[208+r8]
	vaesenclast	xmm1,xmm1,XMMWORD[224+r8]
	vmovdqa	XMMWORD[rsp],xmm1
	mov	QWORD[((8 + 40))+rbp],rdi
	mov	QWORD[((8 + 48))+rbp],rsi

	cmp	rdx,0x80
	jl	NEAR $L$_less_than_128_bytes_wcpqaDvsGlbjGoe
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x100
	jge	NEAR $L$_start_by16_wcpqaDvsGlbjGoe
	cmp	rdx,0x80
	jge	NEAR $L$_start_by8_wcpqaDvsGlbjGoe

$L$_do_n_blocks_wcpqaDvsGlbjGoe:
	cmp	rdx,0x0
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	cmp	rdx,0x70
	jge	NEAR $L$_remaining_num_blocks_is_7_wcpqaDvsGlbjGoe
	cmp	rdx,0x60
	jge	NEAR $L$_remaining_num_blocks_is_6_wcpqaDvsGlbjGoe
	cmp	rdx,0x50
	jge	NEAR $L$_remaining_num_blocks_is_5_wcpqaDvsGlbjGoe
	cmp	rdx,0x40
	jge	NEAR $L$_remaining_num_blocks_is_4_wcpqaDvsGlbjGoe
	cmp	rdx,0x30
	jge	NEAR $L$_remaining_num_blocks_is_3_wcpqaDvsGlbjGoe
	cmp	rdx,0x20
	jge	NEAR $L$_remaining_num_blocks_is_2_wcpqaDvsGlbjGoe
	cmp	rdx,0x10
	jge	NEAR $L$_remaining_num_blocks_is_1_wcpqaDvsGlbjGoe
	vmovdqa	xmm8,xmm0
	vmovdqa	xmm0,xmm9
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe

$L$_remaining_num_blocks_is_7_wcpqaDvsGlbjGoe:
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]
	add	rdi,0x70
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vextracti32x4	xmm0,zmm10,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe

$L$_remaining_num_blocks_is_6_wcpqaDvsGlbjGoe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,0x60
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,0x60
	vextracti32x4	xmm8,zmm2,0x1
	vextracti32x4	xmm0,zmm10,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe

$L$_remaining_num_blocks_is_5_wcpqaDvsGlbjGoe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[64+rdi]
	add	rdi,0x50
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu	XMMWORD[64+rsi],xmm2
	add	rsi,0x50
	vmovdqa	xmm8,xmm2
	vextracti32x4	xmm0,zmm10,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe

$L$_remaining_num_blocks_is_4_wcpqaDvsGlbjGoe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,0x40
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,0x40
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa64	xmm0,xmm10
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_remaining_num_blocks_is_3_wcpqaDvsGlbjGoe:
	mov	r8,-1
	shr	r8,0x10
	kmovq	k1,r8
	vmovdqu8	zmm1{k1},[rdi]
	add	rdi,0x30
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi]{k1},zmm1
	add	rsi,0x30
	vextracti32x4	xmm8,zmm1,0x2
	vextracti32x4	xmm0,zmm9,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_remaining_num_blocks_is_2_wcpqaDvsGlbjGoe:
	vmovdqu8	ymm1,YMMWORD[rdi]
	add	rdi,0x20
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[176+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[192+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[208+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[224+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu	YMMWORD[rsi],ymm1
	add	rsi,0x20
	vextracti32x4	xmm8,zmm1,0x1
	vextracti32x4	xmm0,zmm9,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_remaining_num_blocks_is_1_wcpqaDvsGlbjGoe:
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm1,xmm1,XMMWORD[rcx]
	vaesenc	xmm1,xmm1,XMMWORD[16+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[32+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[48+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[64+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[80+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[96+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[112+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[128+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[144+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[160+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[176+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[192+rcx]
	vaesenc	xmm1,xmm1,XMMWORD[208+rcx]
	vaesenclast	xmm1,xmm1,XMMWORD[224+rcx]
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	vextracti32x4	xmm0,zmm9,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe


$L$_start_by16_wcpqaDvsGlbjGoe:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm11,zmm9,0x1
	vpxord	zmm11,zmm11,zmm14
	vpsrldq	zmm15,zmm10,0xf
	vpclmulqdq	zmm16,zmm15,zmm25,0x0
	vpslldq	zmm12,zmm10,0x1
	vpxord	zmm12,zmm12,zmm16

$L$_main_loop_run_16_wcpqaDvsGlbjGoe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	zmm3,ZMMWORD[128+rdi]
	vmovdqu8	zmm4,ZMMWORD[192+rdi]
	add	rdi,0x100
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpxorq	zmm3,zmm3,zmm0
	vpxorq	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm11,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm11,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm12,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm12,0x1
	vpxord	zmm16,zmm16,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm15,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm17,zmm15,0x1
	vpxord	zmm17,zmm17,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm16,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm18,zmm16,0x1
	vpxord	zmm18,zmm18,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vaesenc	zmm3,zmm3,zmm0
	vaesenc	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vaesenclast	zmm3,zmm3,zmm0
	vaesenclast	zmm4,zmm4,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12

	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqa32	zmm11,zmm17
	vmovdqa32	zmm12,zmm18
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	vmovdqu8	ZMMWORD[128+rsi],zmm3
	vmovdqu8	ZMMWORD[192+rsi],zmm4
	add	rsi,0x100
	sub	rdx,0x100
	cmp	rdx,0x100
	jae	NEAR $L$_main_loop_run_16_wcpqaDvsGlbjGoe
	cmp	rdx,0x80
	jae	NEAR $L$_main_loop_run_8_wcpqaDvsGlbjGoe
	vextracti32x4	xmm0,zmm4,0x3
	jmp	NEAR $L$_do_n_blocks_wcpqaDvsGlbjGoe

$L$_start_by8_wcpqaDvsGlbjGoe:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5

$L$_main_loop_run_8_wcpqaDvsGlbjGoe:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	add	rdi,0x80
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm9,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm10,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm10,0x1
	vpxord	zmm16,zmm16,zmm14

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	add	rsi,0x80
	sub	rdx,0x80
	cmp	rdx,0x80
	jae	NEAR $L$_main_loop_run_8_wcpqaDvsGlbjGoe
	vextracti32x4	xmm0,zmm2,0x3
	jmp	NEAR $L$_do_n_blocks_wcpqaDvsGlbjGoe

$L$_steal_cipher_wcpqaDvsGlbjGoe:
	vmovdqa	xmm2,xmm8
	lea	rax,[vpshufb_shf_table]
	vmovdqu	xmm10,XMMWORD[rdx*1+rax]
	vpshufb	xmm8,xmm8,xmm10
	vmovdqu	xmm3,XMMWORD[((-16))+rdx*1+rdi]
	vmovdqu	XMMWORD[(-16)+rdx*1+rsi],xmm8
	lea	rax,[vpshufb_shf_table]
	add	rax,16
	sub	rax,rdx
	vmovdqu	xmm10,XMMWORD[rax]
	vpxor	xmm10,xmm10,XMMWORD[mask1]
	vpshufb	xmm3,xmm3,xmm10
	vpblendvb	xmm3,xmm3,xmm2,xmm10
	vpxor	xmm8,xmm3,xmm0
	vpxor	xmm8,xmm8,XMMWORD[rcx]
	vaesenc	xmm8,xmm8,XMMWORD[16+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[32+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[48+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[64+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[80+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[96+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[112+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[128+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[144+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[160+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[176+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[192+rcx]
	vaesenc	xmm8,xmm8,XMMWORD[208+rcx]
	vaesenclast	xmm8,xmm8,XMMWORD[224+rcx]
	vpxor	xmm8,xmm8,xmm0
	vmovdqu	XMMWORD[(-16)+rsi],xmm8
$L$_ret_wcpqaDvsGlbjGoe:
	mov	rbx,QWORD[288+rsp]
	xor	r8,r8
	mov	QWORD[288+rsp],r8

	vpxorq	zmm0,zmm0,zmm0
	mov	rdi,QWORD[((288 + 8))+rsp]
	mov	QWORD[((288 + 8))+rsp],r8
	mov	rsi,QWORD[((288 + 16))+rsp]
	mov	QWORD[((288 + 16))+rsp],r8

	vmovdqa	xmm6,XMMWORD[((128 + 0))+rsp]
	vmovdqa	xmm7,XMMWORD[((128 + 16))+rsp]
	vmovdqa	xmm8,XMMWORD[((128 + 32))+rsp]
	vmovdqa	xmm9,XMMWORD[((128 + 48))+rsp]


	vmovdqa64	ZMMWORD[128+rsp],zmm0

	vmovdqa	xmm10,XMMWORD[((128 + 64))+rsp]
	vmovdqa	xmm11,XMMWORD[((128 + 80))+rsp]
	vmovdqa	xmm12,XMMWORD[((128 + 96))+rsp]
	vmovdqa	xmm13,XMMWORD[((128 + 112))+rsp]


	vmovdqa64	ZMMWORD[(128 + 64)+rsp],zmm0

	vmovdqa	xmm14,XMMWORD[((128 + 128))+rsp]
	vmovdqa	xmm15,XMMWORD[((128 + 144))+rsp]



	vmovdqa	YMMWORD[(128 + 128)+rsp],ymm0
	mov	rsp,rbp
	pop	rbp
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$_less_than_128_bytes_wcpqaDvsGlbjGoe:
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x10
	jb	NEAR $L$_ret_wcpqaDvsGlbjGoe
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8d,0xaa
	kmovq	k2,r8
	mov	r8,rdx
	and	r8,0x70
	cmp	r8,0x60
	je	NEAR $L$_num_blocks_is_6_wcpqaDvsGlbjGoe
	cmp	r8,0x50
	je	NEAR $L$_num_blocks_is_5_wcpqaDvsGlbjGoe
	cmp	r8,0x40
	je	NEAR $L$_num_blocks_is_4_wcpqaDvsGlbjGoe
	cmp	r8,0x30
	je	NEAR $L$_num_blocks_is_3_wcpqaDvsGlbjGoe
	cmp	r8,0x20
	je	NEAR $L$_num_blocks_is_2_wcpqaDvsGlbjGoe
	cmp	r8,0x10
	je	NEAR $L$_num_blocks_is_1_wcpqaDvsGlbjGoe

$L$_num_blocks_is_7_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]

	add	rdi,0x70
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vextracti32x4	xmm0,zmm10,0x3
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_6_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,96
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,96

	vextracti32x4	xmm8,ymm2,0x1
	vextracti32x4	xmm0,zmm10,0x2
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_5_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	xmm2,XMMWORD[64+rdi]
	add	rdi,80
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vpternlogq	zmm2,zmm10,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vaesenc	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vaesenclast	zmm2,zmm2,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	XMMWORD[64+rsi],xmm2
	add	rsi,80

	vmovdqa	xmm8,xmm2
	vextracti32x4	xmm0,zmm10,0x1
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_4_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x00
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,64
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,64
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa	xmm0,xmm10
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_3_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4
	mov	r8,0x0000ffffffffffff
	kmovq	k1,r8
	vmovdqu8	zmm1{k1},[rdi]
	add	rdi,48
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpternlogq	zmm1,zmm9,zmm0,0x96
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesenc	zmm1,zmm1,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesenclast	zmm1,zmm1,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vmovdqu8	ZMMWORD[rsi]{k1},zmm1
	add	rsi,48
	vextracti32x4	xmm8,zmm1,2
	vextracti32x4	xmm0,zmm9,3
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_2_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4

	vmovdqu8	ymm1,YMMWORD[rdi]
	add	rdi,32
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[176+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[192+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[208+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[224+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu8	YMMWORD[rsi],ymm1
	add	rsi,32

	vextracti32x4	xmm8,ymm1,1
	vextracti32x4	xmm0,zmm9,2
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe
$L$_num_blocks_is_1_wcpqaDvsGlbjGoe:
	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x00
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4

	vmovdqu8	xmm1,XMMWORD[rdi]
	add	rdi,16
	vbroadcasti32x4	ymm0,YMMWORD[rcx]
	vpternlogq	ymm1,ymm9,ymm0,0x96
	vbroadcasti32x4	ymm0,YMMWORD[16+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[32+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[48+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[64+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[80+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[96+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[112+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[128+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[144+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[160+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[176+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[192+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[208+rcx]
	vaesenc	ymm1,ymm1,ymm0
	vbroadcasti32x4	ymm0,YMMWORD[224+rcx]
	vaesenclast	ymm1,ymm1,ymm0
	vpxorq	ymm1,ymm1,ymm9
	vmovdqu8	XMMWORD[rsi],xmm1
	add	rsi,16

	vmovdqa	xmm8,xmm1
	vextracti32x4	xmm0,zmm9,1
	and	rdx,0xf
	je	NEAR $L$_ret_wcpqaDvsGlbjGoe
	jmp	NEAR $L$_steal_cipher_wcpqaDvsGlbjGoe

global	aesni_xts_256_decrypt_avx512


ALIGN	32
aesni_xts_256_decrypt_avx512:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_256_decrypt_avx512:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



DB	243,15,30,250
	push	rbp
	mov	rbp,rsp
	sub	rsp,312
	and	rsp,0xffffffffffffffc0
	mov	QWORD[288+rsp],rbx
	mov	QWORD[((288 + 8))+rsp],rdi
	mov	QWORD[((288 + 16))+rsp],rsi
	vmovdqa	XMMWORD[(128 + 0)+rsp],xmm6
	vmovdqa	XMMWORD[(128 + 16)+rsp],xmm7
	vmovdqa	XMMWORD[(128 + 32)+rsp],xmm8
	vmovdqa	XMMWORD[(128 + 48)+rsp],xmm9
	vmovdqa	XMMWORD[(128 + 64)+rsp],xmm10
	vmovdqa	XMMWORD[(128 + 80)+rsp],xmm11
	vmovdqa	XMMWORD[(128 + 96)+rsp],xmm12
	vmovdqa	XMMWORD[(128 + 112)+rsp],xmm13
	vmovdqa	XMMWORD[(128 + 128)+rsp],xmm14
	vmovdqa	XMMWORD[(128 + 144)+rsp],xmm15
	mov	r10,0x87
	vmovdqu	xmm1,XMMWORD[r9]
	vpxor	xmm1,xmm1,XMMWORD[r8]
	vaesenc	xmm1,xmm1,XMMWORD[16+r8]
	vaesenc	xmm1,xmm1,XMMWORD[32+r8]
	vaesenc	xmm1,xmm1,XMMWORD[48+r8]
	vaesenc	xmm1,xmm1,XMMWORD[64+r8]
	vaesenc	xmm1,xmm1,XMMWORD[80+r8]
	vaesenc	xmm1,xmm1,XMMWORD[96+r8]
	vaesenc	xmm1,xmm1,XMMWORD[112+r8]
	vaesenc	xmm1,xmm1,XMMWORD[128+r8]
	vaesenc	xmm1,xmm1,XMMWORD[144+r8]
	vaesenc	xmm1,xmm1,XMMWORD[160+r8]
	vaesenc	xmm1,xmm1,XMMWORD[176+r8]
	vaesenc	xmm1,xmm1,XMMWORD[192+r8]
	vaesenc	xmm1,xmm1,XMMWORD[208+r8]
	vaesenclast	xmm1,xmm1,XMMWORD[224+r8]
	vmovdqa	XMMWORD[rsp],xmm1
	mov	QWORD[((8 + 40))+rbp],rdi
	mov	QWORD[((8 + 48))+rbp],rsi

	cmp	rdx,0x80
	jb	NEAR $L$_less_than_128_bytes_EmbgEptodyewbFa
	vpbroadcastq	zmm25,r10
	cmp	rdx,0x100
	jge	NEAR $L$_start_by16_EmbgEptodyewbFa
	jmp	NEAR $L$_start_by8_EmbgEptodyewbFa

$L$_do_n_blocks_EmbgEptodyewbFa:
	cmp	rdx,0x0
	je	NEAR $L$_ret_EmbgEptodyewbFa
	cmp	rdx,0x70
	jge	NEAR $L$_remaining_num_blocks_is_7_EmbgEptodyewbFa
	cmp	rdx,0x60
	jge	NEAR $L$_remaining_num_blocks_is_6_EmbgEptodyewbFa
	cmp	rdx,0x50
	jge	NEAR $L$_remaining_num_blocks_is_5_EmbgEptodyewbFa
	cmp	rdx,0x40
	jge	NEAR $L$_remaining_num_blocks_is_4_EmbgEptodyewbFa
	cmp	rdx,0x30
	jge	NEAR $L$_remaining_num_blocks_is_3_EmbgEptodyewbFa
	cmp	rdx,0x20
	jge	NEAR $L$_remaining_num_blocks_is_2_EmbgEptodyewbFa
	cmp	rdx,0x10
	jge	NEAR $L$_remaining_num_blocks_is_1_EmbgEptodyewbFa


	vmovdqu	xmm1,xmm5

	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[(-16)+rsi],xmm1
	vmovdqa	xmm8,xmm1


	mov	r8,0x1
	kmovq	k1,r8
	vpsllq	xmm13,xmm9,0x3f
	vpsraq	xmm14,xmm13,0x3f
	vpandq	xmm5,xmm14,xmm25
	vpxorq	xmm9{k1},xmm9,xmm5
	vpsrldq	xmm10,xmm9,0x8
DB	98,211,181,8,115,194,1
	vpslldq	xmm13,xmm13,0x8
	vpxorq	xmm0,xmm0,xmm13
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_7_EmbgEptodyewbFa:
	mov	r8,0xffffffffffffffff
	shr	r8,0x10
	kmovq	k1,r8
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2{k1},[64+rdi]
	add	rdi,0x70
	and	rdx,0xf
	je	NEAR $L$_done_7_remain_EmbgEptodyewbFa
	vextracti32x4	xmm12,zmm10,0x2
	vextracti32x4	xmm13,zmm10,0x3
	vinserti32x4	zmm10,zmm10,xmm13,0x2

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	add	rsi,0x70
	vextracti32x4	xmm8,zmm2,0x2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_7_remain_EmbgEptodyewbFa:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi]{k1},zmm2
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_6_EmbgEptodyewbFa:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	ymm2,YMMWORD[64+rdi]
	add	rdi,0x60
	and	rdx,0xf
	je	NEAR $L$_done_6_remain_EmbgEptodyewbFa
	vextracti32x4	xmm12,zmm10,0x1
	vextracti32x4	xmm13,zmm10,0x2
	vinserti32x4	zmm10,zmm10,xmm13,0x1

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	add	rsi,0x60
	vextracti32x4	xmm8,zmm2,0x1
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_6_remain_EmbgEptodyewbFa:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	YMMWORD[64+rsi],ymm2
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_5_EmbgEptodyewbFa:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[64+rdi]
	add	rdi,0x50
	and	rdx,0xf
	je	NEAR $L$_done_5_remain_EmbgEptodyewbFa
	vmovdqa	xmm12,xmm10
	vextracti32x4	xmm10,zmm10,0x1

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu	XMMWORD[64+rsi],xmm2
	add	rsi,0x50
	vmovdqa	xmm8,xmm2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_5_remain_EmbgEptodyewbFa:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	XMMWORD[64+rsi],xmm2
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_4_EmbgEptodyewbFa:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	add	rdi,0x40
	and	rdx,0xf
	je	NEAR $L$_done_4_remain_EmbgEptodyewbFa
	vextracti32x4	xmm12,zmm9,0x3
	vinserti32x4	zmm9,zmm9,xmm10,0x3

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	add	rsi,0x40
	vextracti32x4	xmm8,zmm1,0x3
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_4_remain_EmbgEptodyewbFa:

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_3_EmbgEptodyewbFa:
	vmovdqu	xmm1,XMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	add	rdi,0x30
	and	rdx,0xf
	je	NEAR $L$_done_3_remain_EmbgEptodyewbFa
	vextracti32x4	xmm13,zmm9,0x2
	vextracti32x4	xmm10,zmm9,0x1
	vextracti32x4	xmm11,zmm9,0x3
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x30
	vmovdqa	xmm8,xmm3
	vmovdqa	xmm0,xmm13
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_3_remain_EmbgEptodyewbFa:
	vextracti32x4	xmm10,zmm9,0x1
	vextracti32x4	xmm11,zmm9,0x2
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_2_EmbgEptodyewbFa:
	vmovdqu	xmm1,XMMWORD[rdi]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	add	rdi,0x20
	and	rdx,0xf
	je	NEAR $L$_done_2_remain_EmbgEptodyewbFa
	vextracti32x4	xmm10,zmm9,0x2
	vextracti32x4	xmm12,zmm9,0x1
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x20
	vmovdqa	xmm8,xmm2
	vmovdqa	xmm0,xmm12
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_2_remain_EmbgEptodyewbFa:
	vextracti32x4	xmm10,zmm9,0x1
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_remaining_num_blocks_is_1_EmbgEptodyewbFa:
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	and	rdx,0xf
	je	NEAR $L$_done_1_remain_EmbgEptodyewbFa
	vextracti32x4	xmm11,zmm9,0x1
	vpxor	xmm1,xmm1,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	vmovdqa	xmm0,xmm9
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_1_remain_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	XMMWORD[rsi],xmm1
	jmp	NEAR $L$_ret_EmbgEptodyewbFa

$L$_start_by16_EmbgEptodyewbFa:
	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8


	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4


	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5


	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm11,zmm9,0x1
	vpxord	zmm11,zmm11,zmm14

	vpsrldq	zmm15,zmm10,0xf
	vpclmulqdq	zmm16,zmm15,zmm25,0x0
	vpslldq	zmm12,zmm10,0x1
	vpxord	zmm12,zmm12,zmm16

$L$_main_loop_run_16_EmbgEptodyewbFa:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	zmm3,ZMMWORD[128+rdi]
	vmovdqu8	zmm4,ZMMWORD[192+rdi]
	vmovdqu8	xmm5,XMMWORD[240+rdi]
	add	rdi,0x100
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12
	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpxorq	zmm3,zmm3,zmm0
	vpxorq	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm11,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm11,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm12,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm12,0x1
	vpxord	zmm16,zmm16,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm15,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm17,zmm15,0x1
	vpxord	zmm17,zmm17,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vpsrldq	zmm13,zmm16,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm18,zmm16,0x1
	vpxord	zmm18,zmm18,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vaesdec	zmm3,zmm3,zmm0
	vaesdec	zmm4,zmm4,zmm0
	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0
	vaesdeclast	zmm3,zmm3,zmm0
	vaesdeclast	zmm4,zmm4,zmm0
	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10
	vpxorq	zmm3,zmm3,zmm11
	vpxorq	zmm4,zmm4,zmm12

	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqa32	zmm11,zmm17
	vmovdqa32	zmm12,zmm18
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	vmovdqu8	ZMMWORD[128+rsi],zmm3
	vmovdqu8	ZMMWORD[192+rsi],zmm4
	add	rsi,0x100
	sub	rdx,0x100
	cmp	rdx,0x100
	jge	NEAR $L$_main_loop_run_16_EmbgEptodyewbFa

	cmp	rdx,0x80
	jge	NEAR $L$_main_loop_run_8_EmbgEptodyewbFa
	jmp	NEAR $L$_do_n_blocks_EmbgEptodyewbFa

$L$_start_by8_EmbgEptodyewbFa:

	vbroadcasti32x4	zmm0,ZMMWORD[rsp]
	vbroadcasti32x4	zmm8,ZMMWORD[shufb_15_7]
	mov	r8,0xaa
	kmovq	k2,r8


	vpshufb	zmm1,zmm0,zmm8
	vpsllvq	zmm4,zmm0,ZMMWORD[const_dq3210]
	vpsrlvq	zmm2,zmm1,ZMMWORD[const_dq5678]
	vpclmulqdq	zmm3,zmm2,zmm25,0x0
	vpxorq	zmm4{k2},zmm4,zmm2
	vpxord	zmm9,zmm3,zmm4


	vpsllvq	zmm5,zmm0,ZMMWORD[const_dq7654]
	vpsrlvq	zmm6,zmm1,ZMMWORD[const_dq1234]
	vpclmulqdq	zmm7,zmm6,zmm25,0x0
	vpxorq	zmm5{k2},zmm5,zmm6
	vpxord	zmm10,zmm7,zmm5

$L$_main_loop_run_8_EmbgEptodyewbFa:
	vmovdqu8	zmm1,ZMMWORD[rdi]
	vmovdqu8	zmm2,ZMMWORD[64+rdi]
	vmovdqu8	xmm5,XMMWORD[112+rdi]
	add	rdi,0x80

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vbroadcasti32x4	zmm0,ZMMWORD[rcx]
	vpxorq	zmm1,zmm1,zmm0
	vpxorq	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm9,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm15,zmm9,0x1
	vpxord	zmm15,zmm15,zmm14
	vbroadcasti32x4	zmm0,ZMMWORD[16+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[32+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[48+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0
	vpsrldq	zmm13,zmm10,0xf
	vpclmulqdq	zmm14,zmm13,zmm25,0x0
	vpslldq	zmm16,zmm10,0x1
	vpxord	zmm16,zmm16,zmm14

	vbroadcasti32x4	zmm0,ZMMWORD[64+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[80+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[96+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[112+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[128+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[144+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[160+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[176+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[192+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[208+rcx]
	vaesdec	zmm1,zmm1,zmm0
	vaesdec	zmm2,zmm2,zmm0


	vbroadcasti32x4	zmm0,ZMMWORD[224+rcx]
	vaesdeclast	zmm1,zmm1,zmm0
	vaesdeclast	zmm2,zmm2,zmm0

	vpxorq	zmm1,zmm1,zmm9
	vpxorq	zmm2,zmm2,zmm10


	vmovdqa32	zmm9,zmm15
	vmovdqa32	zmm10,zmm16
	vmovdqu8	ZMMWORD[rsi],zmm1
	vmovdqu8	ZMMWORD[64+rsi],zmm2
	add	rsi,0x80
	sub	rdx,0x80
	cmp	rdx,0x80
	jge	NEAR $L$_main_loop_run_8_EmbgEptodyewbFa
	jmp	NEAR $L$_do_n_blocks_EmbgEptodyewbFa

$L$_steal_cipher_EmbgEptodyewbFa:

	vmovdqa	xmm2,xmm8


	lea	rax,[vpshufb_shf_table]
	vmovdqu	xmm10,XMMWORD[rdx*1+rax]
	vpshufb	xmm8,xmm8,xmm10


	vmovdqu	xmm3,XMMWORD[((-16))+rdx*1+rdi]
	vmovdqu	XMMWORD[(-16)+rdx*1+rsi],xmm8


	lea	rax,[vpshufb_shf_table]
	add	rax,16
	sub	rax,rdx
	vmovdqu	xmm10,XMMWORD[rax]
	vpxor	xmm10,xmm10,XMMWORD[mask1]
	vpshufb	xmm3,xmm3,xmm10

	vpblendvb	xmm3,xmm3,xmm2,xmm10


	vpxor	xmm8,xmm3,xmm0


	vpxor	xmm8,xmm8,XMMWORD[rcx]
	vaesdec	xmm8,xmm8,XMMWORD[16+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[32+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[48+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[64+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[80+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[96+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[112+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[128+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[144+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[160+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[176+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[192+rcx]
	vaesdec	xmm8,xmm8,XMMWORD[208+rcx]
	vaesdeclast	xmm8,xmm8,XMMWORD[224+rcx]

	vpxor	xmm8,xmm8,xmm0

$L$_done_EmbgEptodyewbFa:

	vmovdqu	XMMWORD[(-16)+rsi],xmm8
$L$_ret_EmbgEptodyewbFa:
	mov	rbx,QWORD[288+rsp]
	xor	r8,r8
	mov	QWORD[288+rsp],r8

	vpxorq	zmm0,zmm0,zmm0
	mov	rdi,QWORD[((288 + 8))+rsp]
	mov	QWORD[((288 + 8))+rsp],r8
	mov	rsi,QWORD[((288 + 16))+rsp]
	mov	QWORD[((288 + 16))+rsp],r8

	vmovdqa	xmm6,XMMWORD[((128 + 0))+rsp]
	vmovdqa	xmm7,XMMWORD[((128 + 16))+rsp]
	vmovdqa	xmm8,XMMWORD[((128 + 32))+rsp]
	vmovdqa	xmm9,XMMWORD[((128 + 48))+rsp]


	vmovdqa64	ZMMWORD[128+rsp],zmm0

	vmovdqa	xmm10,XMMWORD[((128 + 64))+rsp]
	vmovdqa	xmm11,XMMWORD[((128 + 80))+rsp]
	vmovdqa	xmm12,XMMWORD[((128 + 96))+rsp]
	vmovdqa	xmm13,XMMWORD[((128 + 112))+rsp]


	vmovdqa64	ZMMWORD[(128 + 64)+rsp],zmm0

	vmovdqa	xmm14,XMMWORD[((128 + 128))+rsp]
	vmovdqa	xmm15,XMMWORD[((128 + 144))+rsp]



	vmovdqa	YMMWORD[(128 + 128)+rsp],ymm0
	mov	rsp,rbp
	pop	rbp
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$_less_than_128_bytes_EmbgEptodyewbFa:
	cmp	rdx,0x10
	jb	NEAR $L$_ret_EmbgEptodyewbFa

	mov	r8,rdx
	and	r8,0x70
	cmp	r8,0x60
	je	NEAR $L$_num_blocks_is_6_EmbgEptodyewbFa
	cmp	r8,0x50
	je	NEAR $L$_num_blocks_is_5_EmbgEptodyewbFa
	cmp	r8,0x40
	je	NEAR $L$_num_blocks_is_4_EmbgEptodyewbFa
	cmp	r8,0x30
	je	NEAR $L$_num_blocks_is_3_EmbgEptodyewbFa
	cmp	r8,0x20
	je	NEAR $L$_num_blocks_is_2_EmbgEptodyewbFa
	cmp	r8,0x10
	je	NEAR $L$_num_blocks_is_1_EmbgEptodyewbFa

$L$_num_blocks_is_7_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[80+rsp],rax
	mov	QWORD[((80 + 8))+rsp],rbx
	vmovdqa	xmm14,XMMWORD[80+rsp]
	vmovdqu	xmm6,XMMWORD[80+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[96+rsp],rax
	mov	QWORD[((96 + 8))+rsp],rbx
	vmovdqa	xmm15,XMMWORD[96+rsp]
	vmovdqu	xmm7,XMMWORD[96+rdi]
	add	rdi,0x70
	and	rdx,0xf
	je	NEAR $L$_done_7_EmbgEptodyewbFa

$L$_steal_cipher_7_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm16,xmm15
	vmovdqa	xmm15,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vpxor	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vaesdeclast	xmm7,xmm7,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	add	rsi,0x70
	vmovdqa64	xmm0,xmm16
	vmovdqa	xmm8,xmm7
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_7_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vpxor	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vaesdec	xmm7,xmm7,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vaesdeclast	xmm7,xmm7,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm7,xmm7,xmm15
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	vmovdqu	XMMWORD[80+rsi],xmm6
	add	rsi,0x70
	vmovdqa	xmm8,xmm7
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_6_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[80+rsp],rax
	mov	QWORD[((80 + 8))+rsp],rbx
	vmovdqa	xmm14,XMMWORD[80+rsp]
	vmovdqu	xmm6,XMMWORD[80+rdi]
	add	rdi,0x60
	and	rdx,0xf
	je	NEAR $L$_done_6_EmbgEptodyewbFa

$L$_steal_cipher_6_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm15,xmm14
	vmovdqa	xmm14,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	add	rsi,0x60
	vmovdqa	xmm0,xmm15
	vmovdqa	xmm8,xmm6
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_6_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vpxor	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vaesdec	xmm6,xmm6,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vaesdeclast	xmm6,xmm6,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vpxor	xmm6,xmm6,xmm14
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	vmovdqu	XMMWORD[64+rsi],xmm5
	add	rsi,0x60
	vmovdqa	xmm8,xmm6
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_5_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[64+rsp],rax
	mov	QWORD[((64 + 8))+rsp],rbx
	vmovdqa	xmm13,XMMWORD[64+rsp]
	vmovdqu	xmm5,XMMWORD[64+rdi]
	add	rdi,0x50
	and	rdx,0xf
	je	NEAR $L$_done_5_EmbgEptodyewbFa

$L$_steal_cipher_5_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm14,xmm13
	vmovdqa	xmm13,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	add	rsi,0x50
	vmovdqa	xmm0,xmm14
	vmovdqa	xmm8,xmm5
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_5_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vpxor	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vaesdec	xmm5,xmm5,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vaesdeclast	xmm5,xmm5,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vpxor	xmm5,xmm5,xmm13
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	vmovdqu	XMMWORD[48+rsi],xmm4
	add	rsi,0x50
	vmovdqa	xmm8,xmm5
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_4_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[48+rsp],rax
	mov	QWORD[((48 + 8))+rsp],rbx
	vmovdqa	xmm12,XMMWORD[48+rsp]
	vmovdqu	xmm4,XMMWORD[48+rdi]
	add	rdi,0x40
	and	rdx,0xf
	je	NEAR $L$_done_4_EmbgEptodyewbFa

$L$_steal_cipher_4_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm13,xmm12
	vmovdqa	xmm12,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x40
	vmovdqa	xmm0,xmm13
	vmovdqa	xmm8,xmm4
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_4_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vpxor	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vaesdec	xmm4,xmm4,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vaesdeclast	xmm4,xmm4,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vpxor	xmm4,xmm4,xmm12
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	vmovdqu	XMMWORD[32+rsi],xmm3
	add	rsi,0x40
	vmovdqa	xmm8,xmm4
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_3_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[32+rsp],rax
	mov	QWORD[((32 + 8))+rsp],rbx
	vmovdqa	xmm11,XMMWORD[32+rsp]
	vmovdqu	xmm3,XMMWORD[32+rdi]
	add	rdi,0x30
	and	rdx,0xf
	je	NEAR $L$_done_3_EmbgEptodyewbFa

$L$_steal_cipher_3_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm12,xmm11
	vmovdqa	xmm11,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x30
	vmovdqa	xmm0,xmm12
	vmovdqa	xmm8,xmm3
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_3_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vpxor	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vaesdec	xmm3,xmm3,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vaesdeclast	xmm3,xmm3,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vpxor	xmm3,xmm3,xmm11
	vmovdqu	XMMWORD[rsi],xmm1
	vmovdqu	XMMWORD[16+rsi],xmm2
	add	rsi,0x30
	vmovdqa	xmm8,xmm3
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_2_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[((16 + 8))+rsp],rbx
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vmovdqu	xmm2,XMMWORD[16+rdi]
	add	rdi,0x20
	and	rdx,0xf
	je	NEAR $L$_done_2_EmbgEptodyewbFa

$L$_steal_cipher_2_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm11,xmm10
	vmovdqa	xmm10,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x20
	vmovdqa	xmm0,xmm11
	vmovdqa	xmm8,xmm2
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_2_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vpxor	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vaesdec	xmm2,xmm2,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vaesdeclast	xmm2,xmm2,xmm0
	vpxor	xmm1,xmm1,xmm9
	vpxor	xmm2,xmm2,xmm10
	vmovdqu	XMMWORD[rsi],xmm1
	add	rsi,0x20
	vmovdqa	xmm8,xmm2
	jmp	NEAR $L$_done_EmbgEptodyewbFa

$L$_num_blocks_is_1_EmbgEptodyewbFa:
	vmovdqa	xmm9,XMMWORD[rsp]
	mov	rax,QWORD[rsp]
	mov	rbx,QWORD[8+rsp]
	vmovdqu	xmm1,XMMWORD[rdi]
	add	rdi,0x10
	and	rdx,0xf
	je	NEAR $L$_done_1_EmbgEptodyewbFa

$L$_steal_cipher_1_EmbgEptodyewbFa:
	xor	r11,r11
	shl	rax,1
	adc	rbx,rbx
	cmovc	r11,r10
	xor	rax,r11
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rbx
	vmovdqa64	xmm10,xmm9
	vmovdqa	xmm9,XMMWORD[16+rsp]
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	add	rsi,0x10
	vmovdqa	xmm0,xmm10
	vmovdqa	xmm8,xmm1
	jmp	NEAR $L$_steal_cipher_EmbgEptodyewbFa

$L$_done_1_EmbgEptodyewbFa:
	vpxor	xmm1,xmm1,xmm9
	vmovdqu	xmm0,XMMWORD[rcx]
	vpxor	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[16+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[32+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[48+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[64+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[80+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[96+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[112+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[128+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[144+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[160+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[176+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[192+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[208+rcx]
	vaesdec	xmm1,xmm1,xmm0
	vmovdqu	xmm0,XMMWORD[224+rcx]
	vaesdeclast	xmm1,xmm1,xmm0
	vpxor	xmm1,xmm1,xmm9
	add	rsi,0x10
	vmovdqa	xmm8,xmm1
	jmp	NEAR $L$_done_EmbgEptodyewbFa

section	.rdata rdata align=8
ALIGN	16

vpshufb_shf_table:
	DQ	0x8786858483828100,0x8f8e8d8c8b8a8988
	DQ	0x0706050403020100,0x000e0d0c0b0a0908

mask1:
	DQ	0x8080808080808080,0x8080808080808080

const_dq3210:
	DQ	0,0,1,1,2,2,3,3
const_dq5678:
	DQ	8,8,7,7,6,6,5,5
const_dq7654:
	DQ	4,4,5,5,6,6,7,7
const_dq1234:
	DQ	4,4,3,3,2,2,1,1

shufb_15_7:
DB	15,0xff,0xff,0xff,0xff,0xff,0xff,0xff,7,0xff,0xff
DB	0xff,0xff,0xff,0xff,0xff

section	.text code align=64

