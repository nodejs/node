default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

global	SHA3_avx512vl_capable

ALIGN	32
SHA3_avx512vl_capable:
	mov	ecx,DWORD[((OPENSSL_ia32cap_P+8))]
	xor	eax,eax

	and	ecx,3221422080
	cmp	ecx,3221422080
	cmove	eax,ecx
	DB	0F3h,0C3h		;repret

section	.text code align=64
















ALIGN	32
keccak_1600_permute:

	mov	r13d,24
	lea	r14,[iotas]

ALIGN	32
$L$keccak_rnd_loop:







	vmovdqa64	ymm25,ymm0
	vpternlogq	ymm25,ymm10,ymm5,0x96
	vmovdqa64	ymm26,ymm1
	vpternlogq	ymm26,ymm6,ymm11,0x96
	vmovdqa64	ymm27,ymm2
	vpternlogq	ymm27,ymm7,ymm12,0x96

	vmovdqa64	ymm28,ymm3
	vpternlogq	ymm28,ymm8,ymm13,0x96
	vmovdqa64	ymm29,ymm4
	vpternlogq	ymm29,ymm9,ymm14,0x96
	vpternlogq	ymm25,ymm15,ymm20,0x96

	vpternlogq	ymm26,ymm16,ymm21,0x96
	vpternlogq	ymm27,ymm17,ymm22,0x96
	vpternlogq	ymm28,ymm18,ymm23,0x96






	vprolq	ymm30,ymm26,1
	vprolq	ymm31,ymm27,1
	vpternlogq	ymm29,ymm19,ymm24,0x96






	vpternlogq	ymm0,ymm29,ymm30,0x96
	vpternlogq	ymm10,ymm29,ymm30,0x96
	vpternlogq	ymm20,ymm29,ymm30,0x96

	vpternlogq	ymm5,ymm29,ymm30,0x96
	vpternlogq	ymm15,ymm29,ymm30,0x96
	vprolq	ymm30,ymm28,1

	vpternlogq	ymm6,ymm25,ymm31,0x96
	vpternlogq	ymm16,ymm25,ymm31,0x96
	vpternlogq	ymm1,ymm25,ymm31,0x96

	vpternlogq	ymm11,ymm25,ymm31,0x96
	vpternlogq	ymm21,ymm25,ymm31,0x96
	vprolq	ymm31,ymm29,1

	vpbroadcastq	ymm29,QWORD[r14]
	add	r14,8

	vpternlogq	ymm12,ymm26,ymm30,0x96
	vpternlogq	ymm7,ymm26,ymm30,0x96
	vpternlogq	ymm22,ymm26,ymm30,0x96

	vpternlogq	ymm17,ymm26,ymm30,0x96
	vpternlogq	ymm2,ymm26,ymm30,0x96
	vprolq	ymm30,ymm25,1















	vpternlogq	ymm3,ymm27,ymm31,0x96
	vpternlogq	ymm13,ymm27,ymm31,0x96
	vpternlogq	ymm23,ymm27,ymm31,0x96

	vprolq	ymm6,ymm6,44
	vpternlogq	ymm18,ymm27,ymm31,0x96
	vpternlogq	ymm8,ymm27,ymm31,0x96

	vprolq	ymm12,ymm12,43
	vprolq	ymm18,ymm18,21
	vpternlogq	ymm24,ymm28,ymm30,0x96

	vprolq	ymm24,ymm24,14
	vprolq	ymm3,ymm3,28
	vpternlogq	ymm9,ymm28,ymm30,0x96

	vprolq	ymm9,ymm9,20
	vprolq	ymm10,ymm10,3
	vpternlogq	ymm19,ymm28,ymm30,0x96

	vprolq	ymm16,ymm16,45
	vprolq	ymm22,ymm22,61
	vpternlogq	ymm4,ymm28,ymm30,0x96

	vprolq	ymm1,ymm1,1
	vprolq	ymm7,ymm7,6
	vpternlogq	ymm14,ymm28,ymm30,0x96








	vprolq	ymm13,ymm13,25
	vprolq	ymm19,ymm19,8
	vmovdqa64	ymm30,ymm0
	vpternlogq	ymm30,ymm6,ymm12,0xD2

	vprolq	ymm20,ymm20,18
	vprolq	ymm4,ymm4,27
	vpxorq	ymm30,ymm30,ymm29

	vprolq	ymm5,ymm5,36
	vprolq	ymm11,ymm11,10
	vmovdqa64	ymm31,ymm6
	vpternlogq	ymm31,ymm12,ymm18,0xD2

	vprolq	ymm17,ymm17,15
	vprolq	ymm23,ymm23,56
	vpternlogq	ymm12,ymm18,ymm24,0xD2

	vprolq	ymm2,ymm2,62
	vprolq	ymm8,ymm8,55
	vpternlogq	ymm18,ymm24,ymm0,0xD2

	vprolq	ymm14,ymm14,39
	vprolq	ymm15,ymm15,41
	vpternlogq	ymm24,ymm0,ymm6,0xD2
	vmovdqa64	ymm0,ymm30
	vmovdqa64	ymm6,ymm31

	vprolq	ymm21,ymm21,2
	vmovdqa64	ymm30,ymm3
	vpternlogq	ymm30,ymm9,ymm10,0xD2
	vmovdqa64	ymm31,ymm9
	vpternlogq	ymm31,ymm10,ymm16,0xD2

	vpternlogq	ymm10,ymm16,ymm22,0xD2
	vpternlogq	ymm16,ymm22,ymm3,0xD2
	vpternlogq	ymm22,ymm3,ymm9,0xD2
	vmovdqa64	ymm3,ymm30
	vmovdqa64	ymm9,ymm31

	vmovdqa64	ymm30,ymm1
	vpternlogq	ymm30,ymm7,ymm13,0xD2
	vmovdqa64	ymm31,ymm7
	vpternlogq	ymm31,ymm13,ymm19,0xD2
	vpternlogq	ymm13,ymm19,ymm20,0xD2

	vpternlogq	ymm19,ymm20,ymm1,0xD2
	vpternlogq	ymm20,ymm1,ymm7,0xD2
	vmovdqa64	ymm1,ymm30
	vmovdqa64	ymm7,ymm31
	vmovdqa64	ymm30,ymm4
	vpternlogq	ymm30,ymm5,ymm11,0xD2

	vmovdqa64	ymm31,ymm5
	vpternlogq	ymm31,ymm11,ymm17,0xD2
	vpternlogq	ymm11,ymm17,ymm23,0xD2
	vpternlogq	ymm17,ymm23,ymm4,0xD2

	vpternlogq	ymm23,ymm4,ymm5,0xD2
	vmovdqa64	ymm4,ymm30
	vmovdqa64	ymm5,ymm31
	vmovdqa64	ymm30,ymm2
	vpternlogq	ymm30,ymm8,ymm14,0xD2
	vmovdqa64	ymm31,ymm8
	vpternlogq	ymm31,ymm14,ymm15,0xD2

	vpternlogq	ymm14,ymm15,ymm21,0xD2
	vpternlogq	ymm15,ymm21,ymm2,0xD2
	vpternlogq	ymm21,ymm2,ymm8,0xD2
	vmovdqa64	ymm2,ymm30
	vmovdqa64	ymm8,ymm31


	vmovdqa64	ymm30,ymm3
	vmovdqa64	ymm3,ymm18
	vmovdqa64	ymm18,ymm17
	vmovdqa64	ymm17,ymm11
	vmovdqa64	ymm11,ymm7
	vmovdqa64	ymm7,ymm10
	vmovdqa64	ymm10,ymm1
	vmovdqa64	ymm1,ymm6
	vmovdqa64	ymm6,ymm9
	vmovdqa64	ymm9,ymm22
	vmovdqa64	ymm22,ymm14
	vmovdqa64	ymm14,ymm20
	vmovdqa64	ymm20,ymm2
	vmovdqa64	ymm2,ymm12
	vmovdqa64	ymm12,ymm13
	vmovdqa64	ymm13,ymm19
	vmovdqa64	ymm19,ymm23
	vmovdqa64	ymm23,ymm15
	vmovdqa64	ymm15,ymm4
	vmovdqa64	ymm4,ymm24
	vmovdqa64	ymm24,ymm21
	vmovdqa64	ymm21,ymm8
	vmovdqa64	ymm8,ymm16
	vmovdqa64	ymm16,ymm5
	vmovdqa64	ymm5,ymm30

	dec	r13d
	jnz	NEAR $L$keccak_rnd_loop
	DB	0F3h,0C3h		;repret




global	keccak_1600_init_state

ALIGN	32
keccak_1600_init_state:

	vpxorq	ymm0,ymm0,ymm0
	vmovdqa64	ymm1,ymm0
	vmovdqa64	ymm2,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm4,ymm0
	vmovdqa64	ymm5,ymm0
	vmovdqa64	ymm6,ymm0
	vmovdqa64	ymm7,ymm0
	vmovdqa64	ymm8,ymm0
	vmovdqa64	ymm9,ymm0
	vmovdqa64	ymm10,ymm0
	vmovdqa64	ymm11,ymm0
	vmovdqa64	ymm12,ymm0
	vmovdqa64	ymm13,ymm0
	vmovdqa64	ymm14,ymm0
	vmovdqa64	ymm15,ymm0
	vmovdqa64	ymm16,ymm0
	vmovdqa64	ymm17,ymm0
	vmovdqa64	ymm18,ymm0
	vmovdqa64	ymm19,ymm0
	vmovdqa64	ymm20,ymm0
	vmovdqa64	ymm21,ymm0
	vmovdqa64	ymm22,ymm0
	vmovdqa64	ymm23,ymm0
	vmovdqa64	ymm24,ymm0
	DB	0F3h,0C3h		;repret



global	keccak_1600_load_state_x4

ALIGN	32
keccak_1600_load_state_x4:

	vmovdqu64	ymm0,YMMWORD[rdi]
	vmovdqu64	ymm1,YMMWORD[32+rdi]
	vmovdqu64	ymm2,YMMWORD[64+rdi]
	vmovdqu64	ymm3,YMMWORD[96+rdi]
	vmovdqu64	ymm4,YMMWORD[128+rdi]
	vmovdqu64	ymm5,YMMWORD[160+rdi]
	vmovdqu64	ymm6,YMMWORD[192+rdi]
	vmovdqu64	ymm7,YMMWORD[224+rdi]
	vmovdqu64	ymm8,YMMWORD[256+rdi]
	vmovdqu64	ymm9,YMMWORD[288+rdi]
	vmovdqu64	ymm10,YMMWORD[320+rdi]
	vmovdqu64	ymm11,YMMWORD[352+rdi]
	vmovdqu64	ymm12,YMMWORD[384+rdi]
	vmovdqu64	ymm13,YMMWORD[416+rdi]
	vmovdqu64	ymm14,YMMWORD[448+rdi]
	vmovdqu64	ymm15,YMMWORD[480+rdi]
	vmovdqu64	ymm16,YMMWORD[512+rdi]
	vmovdqu64	ymm17,YMMWORD[544+rdi]
	vmovdqu64	ymm18,YMMWORD[576+rdi]
	vmovdqu64	ymm19,YMMWORD[608+rdi]
	vmovdqu64	ymm20,YMMWORD[640+rdi]
	vmovdqu64	ymm21,YMMWORD[672+rdi]
	vmovdqu64	ymm22,YMMWORD[704+rdi]
	vmovdqu64	ymm23,YMMWORD[736+rdi]
	vmovdqu64	ymm24,YMMWORD[768+rdi]
	DB	0F3h,0C3h		;repret




global	keccak_1600_save_state_x4

ALIGN	32
keccak_1600_save_state_x4:

	vmovdqu64	YMMWORD[rdi],ymm0
	vmovdqu64	YMMWORD[32+rdi],ymm1
	vmovdqu64	YMMWORD[64+rdi],ymm2
	vmovdqu64	YMMWORD[96+rdi],ymm3
	vmovdqu64	YMMWORD[128+rdi],ymm4
	vmovdqu64	YMMWORD[160+rdi],ymm5
	vmovdqu64	YMMWORD[192+rdi],ymm6
	vmovdqu64	YMMWORD[224+rdi],ymm7
	vmovdqu64	YMMWORD[256+rdi],ymm8
	vmovdqu64	YMMWORD[288+rdi],ymm9
	vmovdqu64	YMMWORD[320+rdi],ymm10
	vmovdqu64	YMMWORD[352+rdi],ymm11
	vmovdqu64	YMMWORD[384+rdi],ymm12
	vmovdqu64	YMMWORD[416+rdi],ymm13
	vmovdqu64	YMMWORD[448+rdi],ymm14
	vmovdqu64	YMMWORD[480+rdi],ymm15
	vmovdqu64	YMMWORD[512+rdi],ymm16
	vmovdqu64	YMMWORD[544+rdi],ymm17
	vmovdqu64	YMMWORD[576+rdi],ymm18
	vmovdqu64	YMMWORD[608+rdi],ymm19
	vmovdqu64	YMMWORD[640+rdi],ymm20
	vmovdqu64	YMMWORD[672+rdi],ymm21
	vmovdqu64	YMMWORD[704+rdi],ymm22
	vmovdqu64	YMMWORD[736+rdi],ymm23
	vmovdqu64	YMMWORD[768+rdi],ymm24
	DB	0F3h,0C3h		;repret













global	keccak_1600_partial_add_x4

ALIGN	32
keccak_1600_partial_add_x4:

	mov	r9,QWORD[800+r10]
	test	r9d,7
	jz	NEAR $L$start_aligned_to_4x8


	mov	r15,r9

	and	r9d,7
	neg	r9d
	add	r9d,8
	cmp	r12d,r9d
	cmovnae	r9d,r12d

	lea	rbx,[byte_kmask_0_to_7]
	kmovb	k1,[r9*1+rbx]

	mov	rbx,r15
	and	ebx,~7
	lea	r10,[rbx*4+r10]

	mov	rbx,r15
	and	ebx,7

	vmovdqu8	ymm31,YMMWORD[r10]
	vmovdqu8	YMMWORD[r10],ymm31

	vmovdqu8	xmm31{k1}{z},[rsi]
	vmovdqu8	xmm30{k1}{z},[rbx*1+r10]
	vpxorq	xmm31,xmm31,xmm30
	vmovdqu8	XMMWORD[rbx*1+r10]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[rdx]
	vmovdqu8	xmm30{k1}{z},[8+rbx*1+r10]
	vpxorq	xmm31,xmm31,xmm30
	vmovdqu8	XMMWORD[8+rbx*1+r10]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[rcx]
	vmovdqu8	xmm30{k1}{z},[16+rbx*1+r10]
	vpxorq	xmm31,xmm31,xmm30
	vmovdqu8	XMMWORD[16+rbx*1+r10]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[r8]
	vmovdqu8	xmm30{k1}{z},[24+rbx*1+r10]
	vpxorq	xmm31,xmm31,xmm30
	vmovdqu8	XMMWORD[24+rbx*1+r10]{k1},xmm31

	sub	r12,r9
	jz	NEAR $L$zero_bytes

	add	rsi,r9
	add	rdx,r9
	add	rcx,r9
	add	r8,r9
	add	r10,32
	xor	r9,r9
	jmp	NEAR $L$ymm_loop

$L$start_aligned_to_4x8:
	lea	r10,[r9*4+r10]
	xor	r9,r9

ALIGN	32
$L$ymm_loop:
	cmp	r12d,8
	jb	NEAR $L$lt_8_bytes

	vmovq	xmm31,QWORD[r9*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[r9*1+rdx],1
	vmovq	xmm30,QWORD[r9*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[r9*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm31,ymm31,YMMWORD[r9*4+r10]
	vmovdqu64	YMMWORD[r9*4+r10],ymm31
	add	r9,8
	sub	r12,8
	jz	NEAR $L$zero_bytes

	jmp	NEAR $L$ymm_loop

ALIGN	32
$L$zero_bytes:
	add	rsi,r9
	add	rdx,r9
	add	rcx,r9
	add	r8,r9
	DB	0F3h,0C3h		;repret

ALIGN	32
$L$lt_8_bytes:
	add	rsi,r9
	add	rdx,r9
	add	rcx,r9
	add	r8,r9
	lea	r10,[r9*4+r10]

	lea	rbx,[byte_kmask_0_to_7]
	kmovb	k1,[r12*1+rbx]

	vmovdqu8	xmm31{k1}{z},[rsi]
	vmovdqu8	xmm30{k1}{z},[rdx]
	vpunpcklqdq	xmm31,xmm31,xmm30
	vmovdqu8	xmm30{k1}{z},[rcx]
	vmovdqu8	xmm29{k1}{z},[r8]
	vpunpcklqdq	xmm30,xmm30,xmm29
	vinserti32x4	ymm31,ymm31,xmm30,1

	vpxorq	ymm31,ymm31,YMMWORD[r10]
	vmovdqu64	YMMWORD[r10],ymm31

	add	rsi,r12
	add	rdx,r12
	add	rcx,r12
	add	r8,r12
	DB	0F3h,0C3h		;repret













global	keccak_1600_extract_bytes_x4

ALIGN	32
keccak_1600_extract_bytes_x4:

	or	r12,r12
	jz	NEAR $L$extract_zero_bytes

	test	r11d,7
	jz	NEAR $L$extract_start_aligned_to_4x8


	mov	r9,r11

	and	r9d,7
	neg	r9d
	add	r9d,8
	cmp	r12d,r9d
	cmovnae	r9d,r12d

	lea	rbx,[byte_kmask_0_to_7]
	kmovb	k1,[r9*1+rbx]

	mov	rbx,r11
	and	ebx,~7
	lea	r10,[rbx*4+r10]

	mov	rbx,r11
	and	ebx,7

	vmovdqu8	xmm31{k1}{z},[rbx*1+r10]
	vmovdqu8	XMMWORD[rdi]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[8+rbx*1+r10]
	vmovdqu8	XMMWORD[rsi]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[16+rbx*1+r10]
	vmovdqu8	XMMWORD[rdx]{k1},xmm31

	vmovdqu8	xmm31{k1}{z},[24+rbx*1+r10]
	vmovdqu8	XMMWORD[rcx]{k1},xmm31


	add	rdi,r9
	add	rsi,r9
	add	rdx,r9
	add	rcx,r9


	sub	r12,r9
	jz	NEAR $L$extract_zero_bytes


	add	r10,32
	xor	r9,r9
	jmp	NEAR $L$extract_ymm_loop

$L$extract_start_aligned_to_4x8:
	lea	r10,[r11*4+r10]
	xor	r9,r9

ALIGN	32
$L$extract_ymm_loop:
	cmp	r12,8
	jb	NEAR $L$extract_lt_8_bytes

	vmovdqu64	xmm31,XMMWORD[r10]
	vmovdqu64	xmm30,XMMWORD[16+r10]
	vmovq	QWORD[r9*1+rdi],xmm31
	vpextrq	XMMWORD[r9*1+rsi],xmm31,1
	vmovq	QWORD[r9*1+rdx],xmm30
	vpextrq	XMMWORD[r9*1+rcx],xmm30,1
	add	r9,8
	sub	r12,8
	jz	NEAR $L$zero_bytes_left

	add	r10,32
	jmp	NEAR $L$extract_ymm_loop

ALIGN	32
$L$zero_bytes_left:

	add	rdi,r9
	add	rsi,r9
	add	rdx,r9
	add	rcx,r9
$L$extract_zero_bytes:
	DB	0F3h,0C3h		;repret

ALIGN	32
$L$extract_lt_8_bytes:
	add	rdi,r9
	add	rsi,r9
	add	rdx,r9
	add	rcx,r9

	lea	r9,[byte_kmask_0_to_7]
	kmovb	k1,[r12*1+r9]

	vmovq	xmm31,QWORD[r10]
	vmovdqu8	XMMWORD[rdi]{k1},xmm31
	vmovq	xmm31,QWORD[8+r10]
	vmovdqu8	XMMWORD[rsi]{k1},xmm31
	vmovq	xmm31,QWORD[16+r10]
	vmovdqu8	XMMWORD[rdx]{k1},xmm31
	vmovq	xmm31,QWORD[24+r10]
	vmovdqu8	XMMWORD[rcx]{k1},xmm31


	add	rdi,r12
	add	rsi,r12
	add	rdx,r12
	add	rcx,r12
	DB	0F3h,0C3h		;repret























global	SHA3_shake128_x4_avx512vl

ALIGN	32
SHA3_shake128_x4_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake128_x4_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



	push	rbp

	mov	rbp,rsp
	push	rbx

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

	sub	rsp,856
	mov	rbx,rsp

$L$shake128_x4_body:
	mov	QWORD[rbx],rdi
	mov	QWORD[8+rbx],rsi
	mov	QWORD[16+rbx],rdx
	mov	QWORD[24+rbx],rcx
	mov	QWORD[32+rbx],r8

	lea	rdi,[48+rbx]
	mov	QWORD[40+rbx],rdi


	call	keccak_1600_init_state

	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],0

	mov	rdi,QWORD[40+rbx]
	mov	rsi,r9


	mov	rdx,QWORD[64+rbp]
	mov	rcx,QWORD[72+rbp]
	mov	r8,QWORD[80+rbp]
	mov	r9,QWORD[88+rbp]

	sub	rsp,32
	call	SHA3_shake128_x4_inc_absorb_avx512vl_internal
	add	rsp,32

	mov	rdi,QWORD[40+rbx]
	call	$L$_SHA3_shake128_x4_inc_finalize_avx512vl


	mov	rdi,QWORD[rbx]
	mov	rsi,QWORD[8+rbx]
	mov	rdx,QWORD[16+rbx]
	mov	rcx,QWORD[24+rbx]
	mov	r8,QWORD[32+rbx]
	mov	r9,QWORD[40+rbx]
	sub	rsp,32
	call	SHA3_shake128_x4_inc_squeeze_avx512vl_internal
	add	rsp,32


	lea	r9,[48+rbx]
	vpxorq	ymm31,ymm31,ymm31
	vmovdqu64	YMMWORD[r9],ymm31
	vmovdqu64	YMMWORD[32+r9],ymm31
	vmovdqu64	YMMWORD[64+r9],ymm31
	vmovdqu64	YMMWORD[96+r9],ymm31
	vmovdqu64	YMMWORD[128+r9],ymm31
	vmovdqu64	YMMWORD[160+r9],ymm31
	vmovdqu64	YMMWORD[192+r9],ymm31
	vmovdqu64	YMMWORD[224+r9],ymm31
	vmovdqu64	YMMWORD[256+r9],ymm31
	vmovdqu64	YMMWORD[288+r9],ymm31
	vmovdqu64	YMMWORD[320+r9],ymm31
	vmovdqu64	YMMWORD[352+r9],ymm31
	vmovdqu64	YMMWORD[384+r9],ymm31
	vmovdqu64	YMMWORD[416+r9],ymm31
	vmovdqu64	YMMWORD[448+r9],ymm31
	vmovdqu64	YMMWORD[480+r9],ymm31
	vmovdqu64	YMMWORD[512+r9],ymm31
	vmovdqu64	YMMWORD[544+r9],ymm31
	vmovdqu64	YMMWORD[576+r9],ymm31
	vmovdqu64	YMMWORD[608+r9],ymm31
	vmovdqu64	YMMWORD[640+r9],ymm31
	vmovdqu64	YMMWORD[672+r9],ymm31
	vmovdqu64	YMMWORD[704+r9],ymm31
	vmovdqu64	YMMWORD[736+r9],ymm31
	vmovdqu64	YMMWORD[768+r9],ymm31
	vmovq	QWORD[800+r9],xmm31

$L$shake128_x4_epilogue:
	vmovups	xmm6,XMMWORD[((856+0))+rsp]
	vmovups	xmm7,XMMWORD[((856+16))+rsp]
	vmovups	xmm8,XMMWORD[((856+32))+rsp]
	vmovups	xmm9,XMMWORD[((856+48))+rsp]
	vmovups	xmm10,XMMWORD[((856+64))+rsp]
	vmovups	xmm11,XMMWORD[((856+80))+rsp]
	vmovups	xmm12,XMMWORD[((856+96))+rsp]
	vmovups	xmm13,XMMWORD[((856+112))+rsp]
	vmovups	xmm14,XMMWORD[((856+128))+rsp]
	vmovups	xmm15,XMMWORD[((856+144))+rsp]
	add	rsp,160
	add	rsp,856
	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake128_x4_avx512vl:






ALIGN	32
$L$SEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl_internal:
SHA3_shake128_x4_inc_absorb_avx512vl_internal:
	mov	rax,rsp
	mov	QWORD[8+rsp],rdi
	mov	QWORD[16+rsp],rsi
	jmp	NEAR $L$_SHA3_shake128_x4_inc_absorb_avx512vl
$L$SEH_end_SHA3_shake128_x4_inc_absorb_avx512vl_internal:













global	SHA3_shake128_x4_inc_absorb_avx512vl

ALIGN	32
SHA3_shake128_x4_inc_absorb_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]


$L$_SHA3_shake128_x4_inc_absorb_avx512vl:

	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

$L$shake128_absorb_body:

	mov	r14,QWORD[800+rdi]
	or	r14,r14
	je	NEAR $L$shake128_absorb_main_loop_start


	mov	r12,168
	sub	r12,r14

	cmp	r9,r12
	jbe	NEAR $L$shake128_absorb_skip_permute

	sub	r9,r12
	mov	r11,r9


	mov	r10,rdi
	call	keccak_1600_partial_add_x4

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	mov	QWORD[800+rdi],0
	jmp	NEAR $L$shake128_absorb_partial_block_done

$L$shake128_absorb_skip_permute:

	mov	r10,rdi
	mov	r12,r9
	mov	r11,r9
	call	keccak_1600_partial_add_x4

	lea	r15,[r14*1+r11]
	mov	QWORD[800+rdi],r15

	cmp	r15,168
	jb	NEAR $L$shake128_absorb_exit

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],0
	jmp	NEAR $L$shake128_absorb_exit

$L$shake128_absorb_main_loop_start:
	call	keccak_1600_load_state_x4
	mov	r11,r9

$L$shake128_absorb_partial_block_done:
	xor	r12,r12


ALIGN	32
$L$shake128_absorb_while_loop:
	cmp	r11,168
	jb	NEAR $L$shake128_absorb_while_loop_done


	vmovq	xmm31,QWORD[r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[r12*1+rdx],1
	vmovq	xmm30,QWORD[r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm0,ymm0,ymm31
	vmovq	xmm31,QWORD[8+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[8+r12*1+rdx],1
	vmovq	xmm30,QWORD[8+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[8+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm1,ymm1,ymm31
	vmovq	xmm31,QWORD[16+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[16+r12*1+rdx],1
	vmovq	xmm30,QWORD[16+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[16+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm2,ymm2,ymm31
	vmovq	xmm31,QWORD[24+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[24+r12*1+rdx],1
	vmovq	xmm30,QWORD[24+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[24+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm3,ymm3,ymm31
	vmovq	xmm31,QWORD[32+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[32+r12*1+rdx],1
	vmovq	xmm30,QWORD[32+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[32+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm4,ymm4,ymm31
	vmovq	xmm31,QWORD[40+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[40+r12*1+rdx],1
	vmovq	xmm30,QWORD[40+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[40+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm5,ymm5,ymm31
	vmovq	xmm31,QWORD[48+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[48+r12*1+rdx],1
	vmovq	xmm30,QWORD[48+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[48+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm6,ymm6,ymm31
	vmovq	xmm31,QWORD[56+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[56+r12*1+rdx],1
	vmovq	xmm30,QWORD[56+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[56+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm7,ymm7,ymm31
	vmovq	xmm31,QWORD[64+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[64+r12*1+rdx],1
	vmovq	xmm30,QWORD[64+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[64+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm8,ymm8,ymm31
	vmovq	xmm31,QWORD[72+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[72+r12*1+rdx],1
	vmovq	xmm30,QWORD[72+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[72+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm9,ymm9,ymm31
	vmovq	xmm31,QWORD[80+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[80+r12*1+rdx],1
	vmovq	xmm30,QWORD[80+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[80+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm10,ymm10,ymm31
	vmovq	xmm31,QWORD[88+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[88+r12*1+rdx],1
	vmovq	xmm30,QWORD[88+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[88+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm11,ymm11,ymm31
	vmovq	xmm31,QWORD[96+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[96+r12*1+rdx],1
	vmovq	xmm30,QWORD[96+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[96+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm12,ymm12,ymm31
	vmovq	xmm31,QWORD[104+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[104+r12*1+rdx],1
	vmovq	xmm30,QWORD[104+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[104+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm13,ymm13,ymm31
	vmovq	xmm31,QWORD[112+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[112+r12*1+rdx],1
	vmovq	xmm30,QWORD[112+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[112+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm14,ymm14,ymm31
	vmovq	xmm31,QWORD[120+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[120+r12*1+rdx],1
	vmovq	xmm30,QWORD[120+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[120+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm15,ymm15,ymm31
	vmovq	xmm31,QWORD[128+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[128+r12*1+rdx],1
	vmovq	xmm30,QWORD[128+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[128+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm16,ymm16,ymm31
	vmovq	xmm31,QWORD[136+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[136+r12*1+rdx],1
	vmovq	xmm30,QWORD[136+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[136+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm17,ymm17,ymm31
	vmovq	xmm31,QWORD[144+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[144+r12*1+rdx],1
	vmovq	xmm30,QWORD[144+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[144+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm18,ymm18,ymm31
	vmovq	xmm31,QWORD[152+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[152+r12*1+rdx],1
	vmovq	xmm30,QWORD[152+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[152+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm19,ymm19,ymm31
	vmovq	xmm31,QWORD[160+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[160+r12*1+rdx],1
	vmovq	xmm30,QWORD[160+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[160+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm20,ymm20,ymm31
	sub	r11,168
	add	r12,168
	call	keccak_1600_permute

	jmp	NEAR $L$shake128_absorb_while_loop

ALIGN	32
$L$shake128_absorb_while_loop_done:
	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],r11
	or	r11,r11
	jz	NEAR $L$shake128_absorb_exit

	mov	QWORD[800+rdi],0


	mov	r10,rdi
	add	rsi,r12
	add	rdx,r12
	add	rcx,r12
	add	r8,r12
	mov	r12,r11
	call	keccak_1600_partial_add_x4

	mov	QWORD[800+rdi],r11

$L$shake128_absorb_exit:

	vpxorq	xmm16,xmm16,xmm16
	vmovdqa64	ymm17,ymm16
	vmovdqa64	ymm18,ymm16
	vmovdqa64	ymm19,ymm16
	vmovdqa64	ymm20,ymm16
	vmovdqa64	ymm21,ymm16
	vmovdqa64	ymm22,ymm16
	vmovdqa64	ymm23,ymm16
	vmovdqa64	ymm24,ymm16
	vmovdqa64	ymm25,ymm16
	vmovdqa64	ymm26,ymm16
	vmovdqa64	ymm27,ymm16
	vmovdqa64	ymm28,ymm16
	vmovdqa64	ymm29,ymm16
	vmovdqa64	ymm30,ymm16
	vmovdqa64	ymm31,ymm16
$L$shake128_absorb_epilogue:
	vzeroall
	vmovups	xmm6,XMMWORD[rsp]
	vmovups	xmm7,XMMWORD[16+rsp]
	vmovups	xmm8,XMMWORD[32+rsp]
	vmovups	xmm9,XMMWORD[48+rsp]
	vmovups	xmm10,XMMWORD[64+rsp]
	vmovups	xmm11,XMMWORD[80+rsp]
	vmovups	xmm12,XMMWORD[96+rsp]
	vmovups	xmm13,XMMWORD[112+rsp]
	vmovups	xmm14,XMMWORD[128+rsp]
	vmovups	xmm15,XMMWORD[144+rsp]
	add	rsp,160

	pop	r15

	pop	r14

	pop	r13

	pop	r12

	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake128_x4_inc_absorb_avx512vl:









global	SHA3_shake128_x4_inc_finalize_avx512vl

ALIGN	32
SHA3_shake128_x4_inc_finalize_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake128_x4_inc_finalize_avx512vl:
	mov	rdi,rcx


$L$_SHA3_shake128_x4_inc_finalize_avx512vl:

	mov	r11,QWORD[800+rdi]
	mov	r10,r11
	and	r10d,~7
	and	r11d,7


	vmovdqu32	ymm31,YMMWORD[r10*4+rdi]
	lea	r9,[shake_msg_pad_x4]
	sub	r9,r11
	vmovdqu32	ymm30,YMMWORD[r9]
	vpxorq	ymm31,ymm31,ymm30
	vmovdqu32	YMMWORD[r10*4+rdi],ymm31


	vmovdqu32	ymm31,YMMWORD[640+rdi]
	vmovdqa32	ymm30,YMMWORD[shake_terminator_byte_x4]
	vpxorq	ymm31,ymm31,ymm30
	vmovdqu32	YMMWORD[640+rdi],ymm31

	mov	QWORD[800+rdi],0
	vpxorq	ymm31,ymm31,ymm31
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake128_x4_inc_finalize_avx512vl:






ALIGN	32
$L$SEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
	mov	rax,rsp
	mov	QWORD[8+rsp],rdi
	mov	QWORD[16+rsp],rsi
	jmp	NEAR $L$_SHA3_shake128_x4_inc_squeeze_avx512vl
$L$SEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:













global	SHA3_shake128_x4_inc_squeeze_avx512vl

ALIGN	32
SHA3_shake128_x4_inc_squeeze_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]


$L$_SHA3_shake128_x4_inc_squeeze_avx512vl:

	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

$L$shake128_squeeze_body:
	or	r8,r8
	jz	NEAR $L$shake128_squeeze_done


	mov	r15,QWORD[800+r9]
	or	r15,r15
	jnz	NEAR $L$shake128_squeeze_no_init_permute

	mov	r14,rdi
	mov	rdi,r9
	call	keccak_1600_load_state_x4

	mov	rdi,r14

	xor	rbp,rbp
	jmp	NEAR $L$shake128_squeeze_loop

ALIGN	32
$L$shake128_squeeze_no_init_permute:

	mov	r10,r9
	mov	r14,r9

	mov	r12,r15
	cmp	r8,r15
	cmovnae	r12,r8

	sub	r8,r12

	mov	r11d,168
	sub	r11,r15

	sub	r15,r12
	mov	QWORD[800+r9],r15

	call	keccak_1600_extract_bytes_x4
	mov	r9,r14

	or	r15,r15
	jnz	NEAR $L$shake128_squeeze_done

	mov	r13,rdi
	mov	rdi,r14
	call	keccak_1600_load_state_x4

	mov	rdi,r13
	xor	rbp,rbp

ALIGN	32
$L$shake128_squeeze_loop:
	cmp	r8,168
	jb	NEAR $L$shake128_squeeze_final_extract

	call	keccak_1600_permute


	vextracti64x2	xmm31,ymm0,1
	vmovq	QWORD[rbp*1+rdi],xmm0
	vpextrq	XMMWORD[rbp*1+rsi],xmm0,1
	vmovq	QWORD[rbp*1+rdx],xmm31
	vpextrq	XMMWORD[rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm1,1
	vmovq	QWORD[8+rbp*1+rdi],xmm1
	vpextrq	XMMWORD[8+rbp*1+rsi],xmm1,1
	vmovq	QWORD[8+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[8+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm2,1
	vmovq	QWORD[16+rbp*1+rdi],xmm2
	vpextrq	XMMWORD[16+rbp*1+rsi],xmm2,1
	vmovq	QWORD[16+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[16+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm3,1
	vmovq	QWORD[24+rbp*1+rdi],xmm3
	vpextrq	XMMWORD[24+rbp*1+rsi],xmm3,1
	vmovq	QWORD[24+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[24+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm4,1
	vmovq	QWORD[32+rbp*1+rdi],xmm4
	vpextrq	XMMWORD[32+rbp*1+rsi],xmm4,1
	vmovq	QWORD[32+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[32+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm5,1
	vmovq	QWORD[40+rbp*1+rdi],xmm5
	vpextrq	XMMWORD[40+rbp*1+rsi],xmm5,1
	vmovq	QWORD[40+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[40+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm6,1
	vmovq	QWORD[48+rbp*1+rdi],xmm6
	vpextrq	XMMWORD[48+rbp*1+rsi],xmm6,1
	vmovq	QWORD[48+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[48+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm7,1
	vmovq	QWORD[56+rbp*1+rdi],xmm7
	vpextrq	XMMWORD[56+rbp*1+rsi],xmm7,1
	vmovq	QWORD[56+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[56+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm8,1
	vmovq	QWORD[64+rbp*1+rdi],xmm8
	vpextrq	XMMWORD[64+rbp*1+rsi],xmm8,1
	vmovq	QWORD[64+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[64+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm9,1
	vmovq	QWORD[72+rbp*1+rdi],xmm9
	vpextrq	XMMWORD[72+rbp*1+rsi],xmm9,1
	vmovq	QWORD[72+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[72+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm10,1
	vmovq	QWORD[80+rbp*1+rdi],xmm10
	vpextrq	XMMWORD[80+rbp*1+rsi],xmm10,1
	vmovq	QWORD[80+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[80+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm11,1
	vmovq	QWORD[88+rbp*1+rdi],xmm11
	vpextrq	XMMWORD[88+rbp*1+rsi],xmm11,1
	vmovq	QWORD[88+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[88+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm12,1
	vmovq	QWORD[96+rbp*1+rdi],xmm12
	vpextrq	XMMWORD[96+rbp*1+rsi],xmm12,1
	vmovq	QWORD[96+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[96+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm13,1
	vmovq	QWORD[104+rbp*1+rdi],xmm13
	vpextrq	XMMWORD[104+rbp*1+rsi],xmm13,1
	vmovq	QWORD[104+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[104+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm14,1
	vmovq	QWORD[112+rbp*1+rdi],xmm14
	vpextrq	XMMWORD[112+rbp*1+rsi],xmm14,1
	vmovq	QWORD[112+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[112+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm15,1
	vmovq	QWORD[120+rbp*1+rdi],xmm15
	vpextrq	XMMWORD[120+rbp*1+rsi],xmm15,1
	vmovq	QWORD[120+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[120+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm16,1
	vmovq	QWORD[128+rbp*1+rdi],xmm16
	vpextrq	XMMWORD[128+rbp*1+rsi],xmm16,1
	vmovq	QWORD[128+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[128+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm17,1
	vmovq	QWORD[136+rbp*1+rdi],xmm17
	vpextrq	XMMWORD[136+rbp*1+rsi],xmm17,1
	vmovq	QWORD[136+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[136+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm18,1
	vmovq	QWORD[144+rbp*1+rdi],xmm18
	vpextrq	XMMWORD[144+rbp*1+rsi],xmm18,1
	vmovq	QWORD[144+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[144+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm19,1
	vmovq	QWORD[152+rbp*1+rdi],xmm19
	vpextrq	XMMWORD[152+rbp*1+rsi],xmm19,1
	vmovq	QWORD[152+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[152+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm20,1
	vmovq	QWORD[160+rbp*1+rdi],xmm20
	vpextrq	XMMWORD[160+rbp*1+rsi],xmm20,1
	vmovq	QWORD[160+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[160+rbp*1+rcx],xmm31,1
	add	rbp,168
	sub	r8,168
	jmp	NEAR $L$shake128_squeeze_loop

ALIGN	32
$L$shake128_squeeze_final_extract:
	or	r8,r8
	jz	NEAR $L$shake128_squeeze_no_end_permute


	add	rdi,rbp
	add	rsi,rbp
	add	rdx,rbp
	add	rcx,rbp

	mov	r15d,168
	sub	r15,r8
	mov	QWORD[800+r9],r15

	call	keccak_1600_permute

	mov	r14,rdi
	mov	rdi,r9
	call	keccak_1600_save_state_x4

	mov	rdi,r14


	mov	r10,r9
	mov	r12,r8
	xor	r11,r11
	call	keccak_1600_extract_bytes_x4

	jmp	NEAR $L$shake128_squeeze_done

$L$shake128_squeeze_no_end_permute:
	mov	QWORD[800+r9],0
	mov	rdi,r9
	call	keccak_1600_save_state_x4

$L$shake128_squeeze_done:

	vpxorq	xmm16,xmm16,xmm16
	vmovdqa64	ymm17,ymm16
	vmovdqa64	ymm18,ymm16
	vmovdqa64	ymm19,ymm16
	vmovdqa64	ymm20,ymm16
	vmovdqa64	ymm21,ymm16
	vmovdqa64	ymm22,ymm16
	vmovdqa64	ymm23,ymm16
	vmovdqa64	ymm24,ymm16
	vmovdqa64	ymm25,ymm16
	vmovdqa64	ymm26,ymm16
	vmovdqa64	ymm27,ymm16
	vmovdqa64	ymm28,ymm16
	vmovdqa64	ymm29,ymm16
	vmovdqa64	ymm30,ymm16
	vmovdqa64	ymm31,ymm16
$L$shake128_squeeze_epilogue:
	vzeroall
	vmovups	xmm6,XMMWORD[rsp]
	vmovups	xmm7,XMMWORD[16+rsp]
	vmovups	xmm8,XMMWORD[32+rsp]
	vmovups	xmm9,XMMWORD[48+rsp]
	vmovups	xmm10,XMMWORD[64+rsp]
	vmovups	xmm11,XMMWORD[80+rsp]
	vmovups	xmm12,XMMWORD[96+rsp]
	vmovups	xmm13,XMMWORD[112+rsp]
	vmovups	xmm14,XMMWORD[128+rsp]
	vmovups	xmm15,XMMWORD[144+rsp]
	add	rsp,160

	pop	r15

	pop	r14

	pop	r13

	pop	r12

	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl:





















global	SHA3_shake256_x4_avx512vl

ALIGN	32
SHA3_shake256_x4_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake256_x4_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]



	push	rbp

	mov	rbp,rsp
	push	rbx

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

	sub	rsp,856
	mov	rbx,rsp

$L$shake256_x4_body:
	mov	QWORD[rbx],rdi
	mov	QWORD[8+rbx],rsi
	mov	QWORD[16+rbx],rdx
	mov	QWORD[24+rbx],rcx
	mov	QWORD[32+rbx],r8

	lea	rdi,[48+rbx]
	mov	QWORD[40+rbx],rdi


	call	keccak_1600_init_state

	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],0

	mov	rdi,QWORD[40+rbx]
	mov	rsi,r9


	mov	rdx,QWORD[64+rbp]
	mov	rcx,QWORD[72+rbp]
	mov	r8,QWORD[80+rbp]
	mov	r9,QWORD[88+rbp]

	sub	rsp,32
	call	SHA3_shake256_x4_inc_absorb_avx512vl_internal
	add	rsp,32

	mov	rdi,QWORD[40+rbx]
	call	$L$_SHA3_shake256_x4_inc_finalize_avx512vl


	mov	rdi,QWORD[rbx]
	mov	rsi,QWORD[8+rbx]
	mov	rdx,QWORD[16+rbx]
	mov	rcx,QWORD[24+rbx]
	mov	r8,QWORD[32+rbx]
	mov	r9,QWORD[40+rbx]
	sub	rsp,32
	call	SHA3_shake256_x4_inc_squeeze_avx512vl_internal
	add	rsp,32


	lea	r9,[48+rbx]
	vpxorq	ymm31,ymm31,ymm31
	vmovdqu64	YMMWORD[r9],ymm31
	vmovdqu64	YMMWORD[32+r9],ymm31
	vmovdqu64	YMMWORD[64+r9],ymm31
	vmovdqu64	YMMWORD[96+r9],ymm31
	vmovdqu64	YMMWORD[128+r9],ymm31
	vmovdqu64	YMMWORD[160+r9],ymm31
	vmovdqu64	YMMWORD[192+r9],ymm31
	vmovdqu64	YMMWORD[224+r9],ymm31
	vmovdqu64	YMMWORD[256+r9],ymm31
	vmovdqu64	YMMWORD[288+r9],ymm31
	vmovdqu64	YMMWORD[320+r9],ymm31
	vmovdqu64	YMMWORD[352+r9],ymm31
	vmovdqu64	YMMWORD[384+r9],ymm31
	vmovdqu64	YMMWORD[416+r9],ymm31
	vmovdqu64	YMMWORD[448+r9],ymm31
	vmovdqu64	YMMWORD[480+r9],ymm31
	vmovdqu64	YMMWORD[512+r9],ymm31
	vmovdqu64	YMMWORD[544+r9],ymm31
	vmovdqu64	YMMWORD[576+r9],ymm31
	vmovdqu64	YMMWORD[608+r9],ymm31
	vmovdqu64	YMMWORD[640+r9],ymm31
	vmovdqu64	YMMWORD[672+r9],ymm31
	vmovdqu64	YMMWORD[704+r9],ymm31
	vmovdqu64	YMMWORD[736+r9],ymm31
	vmovdqu64	YMMWORD[768+r9],ymm31
	vmovq	QWORD[800+r9],xmm31

$L$shake256_x4_epilogue:
	vmovups	xmm6,XMMWORD[((856+0))+rsp]
	vmovups	xmm7,XMMWORD[((856+16))+rsp]
	vmovups	xmm8,XMMWORD[((856+32))+rsp]
	vmovups	xmm9,XMMWORD[((856+48))+rsp]
	vmovups	xmm10,XMMWORD[((856+64))+rsp]
	vmovups	xmm11,XMMWORD[((856+80))+rsp]
	vmovups	xmm12,XMMWORD[((856+96))+rsp]
	vmovups	xmm13,XMMWORD[((856+112))+rsp]
	vmovups	xmm14,XMMWORD[((856+128))+rsp]
	vmovups	xmm15,XMMWORD[((856+144))+rsp]
	add	rsp,160
	add	rsp,856
	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake256_x4_avx512vl:






ALIGN	32
$L$SEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl_internal:
SHA3_shake256_x4_inc_absorb_avx512vl_internal:
	mov	rax,rsp
	mov	QWORD[8+rsp],rdi
	mov	QWORD[16+rsp],rsi
	jmp	NEAR $L$_SHA3_shake256_x4_inc_absorb_avx512vl
$L$SEH_end_SHA3_shake256_x4_inc_absorb_avx512vl_internal:













global	SHA3_shake256_x4_inc_absorb_avx512vl

ALIGN	32
SHA3_shake256_x4_inc_absorb_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]


$L$_SHA3_shake256_x4_inc_absorb_avx512vl:

	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

$L$shake256_absorb_body:

	mov	r14,QWORD[800+rdi]
	or	r14,r14
	je	NEAR $L$shake256_absorb_main_loop_start


	mov	r12,136
	sub	r12,r14

	cmp	r9,r12
	jbe	NEAR $L$shake256_absorb_skip_permute

	sub	r9,r12
	mov	r11,r9


	mov	r10,rdi
	call	keccak_1600_partial_add_x4

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	mov	QWORD[800+rdi],0
	jmp	NEAR $L$shake256_absorb_partial_block_done

$L$shake256_absorb_skip_permute:

	mov	r10,rdi
	mov	r12,r9
	mov	r11,r9
	call	keccak_1600_partial_add_x4

	lea	r15,[r14*1+r11]
	mov	QWORD[800+rdi],r15

	cmp	r15,136
	jb	NEAR $L$shake256_absorb_exit

	call	keccak_1600_load_state_x4

	call	keccak_1600_permute

	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],0
	jmp	NEAR $L$shake256_absorb_exit

$L$shake256_absorb_main_loop_start:
	call	keccak_1600_load_state_x4
	mov	r11,r9

$L$shake256_absorb_partial_block_done:
	xor	r12,r12


ALIGN	32
$L$shake256_absorb_while_loop:
	cmp	r11,136
	jb	NEAR $L$shake256_absorb_while_loop_done


	vmovq	xmm31,QWORD[r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[r12*1+rdx],1
	vmovq	xmm30,QWORD[r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm0,ymm0,ymm31
	vmovq	xmm31,QWORD[8+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[8+r12*1+rdx],1
	vmovq	xmm30,QWORD[8+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[8+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm1,ymm1,ymm31
	vmovq	xmm31,QWORD[16+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[16+r12*1+rdx],1
	vmovq	xmm30,QWORD[16+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[16+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm2,ymm2,ymm31
	vmovq	xmm31,QWORD[24+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[24+r12*1+rdx],1
	vmovq	xmm30,QWORD[24+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[24+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm3,ymm3,ymm31
	vmovq	xmm31,QWORD[32+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[32+r12*1+rdx],1
	vmovq	xmm30,QWORD[32+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[32+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm4,ymm4,ymm31
	vmovq	xmm31,QWORD[40+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[40+r12*1+rdx],1
	vmovq	xmm30,QWORD[40+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[40+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm5,ymm5,ymm31
	vmovq	xmm31,QWORD[48+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[48+r12*1+rdx],1
	vmovq	xmm30,QWORD[48+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[48+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm6,ymm6,ymm31
	vmovq	xmm31,QWORD[56+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[56+r12*1+rdx],1
	vmovq	xmm30,QWORD[56+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[56+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm7,ymm7,ymm31
	vmovq	xmm31,QWORD[64+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[64+r12*1+rdx],1
	vmovq	xmm30,QWORD[64+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[64+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm8,ymm8,ymm31
	vmovq	xmm31,QWORD[72+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[72+r12*1+rdx],1
	vmovq	xmm30,QWORD[72+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[72+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm9,ymm9,ymm31
	vmovq	xmm31,QWORD[80+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[80+r12*1+rdx],1
	vmovq	xmm30,QWORD[80+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[80+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm10,ymm10,ymm31
	vmovq	xmm31,QWORD[88+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[88+r12*1+rdx],1
	vmovq	xmm30,QWORD[88+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[88+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm11,ymm11,ymm31
	vmovq	xmm31,QWORD[96+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[96+r12*1+rdx],1
	vmovq	xmm30,QWORD[96+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[96+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm12,ymm12,ymm31
	vmovq	xmm31,QWORD[104+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[104+r12*1+rdx],1
	vmovq	xmm30,QWORD[104+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[104+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm13,ymm13,ymm31
	vmovq	xmm31,QWORD[112+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[112+r12*1+rdx],1
	vmovq	xmm30,QWORD[112+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[112+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm14,ymm14,ymm31
	vmovq	xmm31,QWORD[120+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[120+r12*1+rdx],1
	vmovq	xmm30,QWORD[120+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[120+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm15,ymm15,ymm31
	vmovq	xmm31,QWORD[128+r12*1+rsi]
	vpinsrq	xmm31,xmm31,QWORD[128+r12*1+rdx],1
	vmovq	xmm30,QWORD[128+r12*1+rcx]
	vpinsrq	xmm30,xmm30,QWORD[128+r12*1+r8],1
	vinserti32x4	ymm31,ymm31,xmm30,1
	vpxorq	ymm16,ymm16,ymm31
	sub	r11,136
	add	r12,136
	call	keccak_1600_permute

	jmp	NEAR $L$shake256_absorb_while_loop

ALIGN	32
$L$shake256_absorb_while_loop_done:
	call	keccak_1600_save_state_x4

	mov	QWORD[800+rdi],r11
	or	r11,r11
	jz	NEAR $L$shake256_absorb_exit

	mov	QWORD[800+rdi],0


	mov	r10,rdi
	add	rsi,r12
	add	rdx,r12
	add	rcx,r12
	add	r8,r12
	mov	r12,r11
	call	keccak_1600_partial_add_x4

	mov	QWORD[800+rdi],r11

$L$shake256_absorb_exit:

	vpxorq	xmm16,xmm16,xmm16
	vmovdqa64	ymm17,ymm16
	vmovdqa64	ymm18,ymm16
	vmovdqa64	ymm19,ymm16
	vmovdqa64	ymm20,ymm16
	vmovdqa64	ymm21,ymm16
	vmovdqa64	ymm22,ymm16
	vmovdqa64	ymm23,ymm16
	vmovdqa64	ymm24,ymm16
	vmovdqa64	ymm25,ymm16
	vmovdqa64	ymm26,ymm16
	vmovdqa64	ymm27,ymm16
	vmovdqa64	ymm28,ymm16
	vmovdqa64	ymm29,ymm16
	vmovdqa64	ymm30,ymm16
	vmovdqa64	ymm31,ymm16
$L$shake256_absorb_epilogue:
	vzeroall
	vmovups	xmm6,XMMWORD[rsp]
	vmovups	xmm7,XMMWORD[16+rsp]
	vmovups	xmm8,XMMWORD[32+rsp]
	vmovups	xmm9,XMMWORD[48+rsp]
	vmovups	xmm10,XMMWORD[64+rsp]
	vmovups	xmm11,XMMWORD[80+rsp]
	vmovups	xmm12,XMMWORD[96+rsp]
	vmovups	xmm13,XMMWORD[112+rsp]
	vmovups	xmm14,XMMWORD[128+rsp]
	vmovups	xmm15,XMMWORD[144+rsp]
	add	rsp,160

	pop	r15

	pop	r14

	pop	r13

	pop	r12

	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake256_x4_inc_absorb_avx512vl:









global	SHA3_shake256_x4_inc_finalize_avx512vl

ALIGN	32
SHA3_shake256_x4_inc_finalize_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake256_x4_inc_finalize_avx512vl:
	mov	rdi,rcx


$L$_SHA3_shake256_x4_inc_finalize_avx512vl:

	mov	r11,QWORD[800+rdi]
	mov	r10,r11
	and	r10d,~7
	and	r11d,7


	vmovdqu32	ymm31,YMMWORD[r10*4+rdi]
	lea	r9,[shake_msg_pad_x4]
	sub	r9,r11
	vmovdqu32	ymm30,YMMWORD[r9]
	vpxorq	ymm31,ymm31,ymm30
	vmovdqu32	YMMWORD[r10*4+rdi],ymm31


	vmovdqu32	ymm31,YMMWORD[512+rdi]
	vmovdqa32	ymm30,YMMWORD[shake_terminator_byte_x4]
	vpxorq	ymm31,ymm31,ymm30
	vmovdqu32	YMMWORD[512+rdi],ymm31

	mov	QWORD[800+rdi],0
	vpxorq	ymm31,ymm31,ymm31
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake256_x4_inc_finalize_avx512vl:






ALIGN	32
$L$SEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
	mov	rax,rsp
	mov	QWORD[8+rsp],rdi
	mov	QWORD[16+rsp],rsi
	jmp	NEAR $L$_SHA3_shake256_x4_inc_squeeze_avx512vl
$L$SEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:













global	SHA3_shake256_x4_inc_squeeze_avx512vl

ALIGN	32
SHA3_shake256_x4_inc_squeeze_avx512vl:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]
	mov	r9,QWORD[48+rsp]


$L$_SHA3_shake256_x4_inc_squeeze_avx512vl:

	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

	sub	rsp,160
	vmovups	XMMWORD[rsp],xmm6
	vmovups	XMMWORD[16+rsp],xmm7
	vmovups	XMMWORD[32+rsp],xmm8
	vmovups	XMMWORD[48+rsp],xmm9
	vmovups	XMMWORD[64+rsp],xmm10
	vmovups	XMMWORD[80+rsp],xmm11
	vmovups	XMMWORD[96+rsp],xmm12
	vmovups	XMMWORD[112+rsp],xmm13
	vmovups	XMMWORD[128+rsp],xmm14
	vmovups	XMMWORD[144+rsp],xmm15

$L$shake256_squeeze_body:
	or	r8,r8
	jz	NEAR $L$shake256_squeeze_done


	mov	r15,QWORD[800+r9]
	or	r15,r15
	jnz	NEAR $L$shake256_squeeze_no_init_permute

	mov	r14,rdi
	mov	rdi,r9
	call	keccak_1600_load_state_x4

	mov	rdi,r14

	xor	rbp,rbp
	jmp	NEAR $L$shake256_squeeze_loop

ALIGN	32
$L$shake256_squeeze_no_init_permute:

	mov	r10,r9
	mov	r14,r9

	mov	r12,r15
	cmp	r8,r15
	cmovnae	r12,r8

	sub	r8,r12

	mov	r11d,136
	sub	r11,r15

	sub	r15,r12
	mov	QWORD[800+r9],r15

	call	keccak_1600_extract_bytes_x4
	mov	r9,r14

	or	r15,r15
	jnz	NEAR $L$shake256_squeeze_done

	mov	r13,rdi
	mov	rdi,r14
	call	keccak_1600_load_state_x4

	mov	rdi,r13
	xor	rbp,rbp

ALIGN	32
$L$shake256_squeeze_loop:
	cmp	r8,136
	jb	NEAR $L$shake256_squeeze_final_extract

	call	keccak_1600_permute


	vextracti64x2	xmm31,ymm0,1
	vmovq	QWORD[rbp*1+rdi],xmm0
	vpextrq	XMMWORD[rbp*1+rsi],xmm0,1
	vmovq	QWORD[rbp*1+rdx],xmm31
	vpextrq	XMMWORD[rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm1,1
	vmovq	QWORD[8+rbp*1+rdi],xmm1
	vpextrq	XMMWORD[8+rbp*1+rsi],xmm1,1
	vmovq	QWORD[8+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[8+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm2,1
	vmovq	QWORD[16+rbp*1+rdi],xmm2
	vpextrq	XMMWORD[16+rbp*1+rsi],xmm2,1
	vmovq	QWORD[16+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[16+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm3,1
	vmovq	QWORD[24+rbp*1+rdi],xmm3
	vpextrq	XMMWORD[24+rbp*1+rsi],xmm3,1
	vmovq	QWORD[24+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[24+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm4,1
	vmovq	QWORD[32+rbp*1+rdi],xmm4
	vpextrq	XMMWORD[32+rbp*1+rsi],xmm4,1
	vmovq	QWORD[32+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[32+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm5,1
	vmovq	QWORD[40+rbp*1+rdi],xmm5
	vpextrq	XMMWORD[40+rbp*1+rsi],xmm5,1
	vmovq	QWORD[40+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[40+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm6,1
	vmovq	QWORD[48+rbp*1+rdi],xmm6
	vpextrq	XMMWORD[48+rbp*1+rsi],xmm6,1
	vmovq	QWORD[48+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[48+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm7,1
	vmovq	QWORD[56+rbp*1+rdi],xmm7
	vpextrq	XMMWORD[56+rbp*1+rsi],xmm7,1
	vmovq	QWORD[56+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[56+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm8,1
	vmovq	QWORD[64+rbp*1+rdi],xmm8
	vpextrq	XMMWORD[64+rbp*1+rsi],xmm8,1
	vmovq	QWORD[64+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[64+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm9,1
	vmovq	QWORD[72+rbp*1+rdi],xmm9
	vpextrq	XMMWORD[72+rbp*1+rsi],xmm9,1
	vmovq	QWORD[72+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[72+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm10,1
	vmovq	QWORD[80+rbp*1+rdi],xmm10
	vpextrq	XMMWORD[80+rbp*1+rsi],xmm10,1
	vmovq	QWORD[80+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[80+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm11,1
	vmovq	QWORD[88+rbp*1+rdi],xmm11
	vpextrq	XMMWORD[88+rbp*1+rsi],xmm11,1
	vmovq	QWORD[88+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[88+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm12,1
	vmovq	QWORD[96+rbp*1+rdi],xmm12
	vpextrq	XMMWORD[96+rbp*1+rsi],xmm12,1
	vmovq	QWORD[96+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[96+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm13,1
	vmovq	QWORD[104+rbp*1+rdi],xmm13
	vpextrq	XMMWORD[104+rbp*1+rsi],xmm13,1
	vmovq	QWORD[104+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[104+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm14,1
	vmovq	QWORD[112+rbp*1+rdi],xmm14
	vpextrq	XMMWORD[112+rbp*1+rsi],xmm14,1
	vmovq	QWORD[112+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[112+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm15,1
	vmovq	QWORD[120+rbp*1+rdi],xmm15
	vpextrq	XMMWORD[120+rbp*1+rsi],xmm15,1
	vmovq	QWORD[120+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[120+rbp*1+rcx],xmm31,1
	vextracti64x2	xmm31,ymm16,1
	vmovq	QWORD[128+rbp*1+rdi],xmm16
	vpextrq	XMMWORD[128+rbp*1+rsi],xmm16,1
	vmovq	QWORD[128+rbp*1+rdx],xmm31
	vpextrq	XMMWORD[128+rbp*1+rcx],xmm31,1
	add	rbp,136
	sub	r8,136
	jmp	NEAR $L$shake256_squeeze_loop

ALIGN	32
$L$shake256_squeeze_final_extract:
	or	r8,r8
	jz	NEAR $L$shake256_squeeze_no_end_permute


	add	rdi,rbp
	add	rsi,rbp
	add	rdx,rbp
	add	rcx,rbp

	mov	r15d,136
	sub	r15,r8
	mov	QWORD[800+r9],r15

	call	keccak_1600_permute

	mov	r14,rdi
	mov	rdi,r9
	call	keccak_1600_save_state_x4

	mov	rdi,r14


	mov	r10,r9
	mov	r12,r8
	xor	r11,r11
	call	keccak_1600_extract_bytes_x4

	jmp	NEAR $L$shake256_squeeze_done

$L$shake256_squeeze_no_end_permute:
	mov	QWORD[800+r9],0
	mov	rdi,r9
	call	keccak_1600_save_state_x4

$L$shake256_squeeze_done:

	vpxorq	xmm16,xmm16,xmm16
	vmovdqa64	ymm17,ymm16
	vmovdqa64	ymm18,ymm16
	vmovdqa64	ymm19,ymm16
	vmovdqa64	ymm20,ymm16
	vmovdqa64	ymm21,ymm16
	vmovdqa64	ymm22,ymm16
	vmovdqa64	ymm23,ymm16
	vmovdqa64	ymm24,ymm16
	vmovdqa64	ymm25,ymm16
	vmovdqa64	ymm26,ymm16
	vmovdqa64	ymm27,ymm16
	vmovdqa64	ymm28,ymm16
	vmovdqa64	ymm29,ymm16
	vmovdqa64	ymm30,ymm16
	vmovdqa64	ymm31,ymm16
$L$shake256_squeeze_epilogue:
	vzeroall
	vmovups	xmm6,XMMWORD[rsp]
	vmovups	xmm7,XMMWORD[16+rsp]
	vmovups	xmm8,XMMWORD[32+rsp]
	vmovups	xmm9,XMMWORD[48+rsp]
	vmovups	xmm10,XMMWORD[64+rsp]
	vmovups	xmm11,XMMWORD[80+rsp]
	vmovups	xmm12,XMMWORD[96+rsp]
	vmovups	xmm13,XMMWORD[112+rsp]
	vmovups	xmm14,XMMWORD[128+rsp]
	vmovups	xmm15,XMMWORD[144+rsp]
	add	rsp,160

	pop	r15

	pop	r14

	pop	r13

	pop	r12

	pop	rbx

	pop	rbp

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl:
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
keccak_se_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	mov	rsi,QWORD[8+r9]
	mov	r11,QWORD[56+r9]

	mov	r10d,DWORD[r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jb	NEAR $L$keccak_in_prologue

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$keccak_in_epilogue






	mov	rdx,QWORD[152+r8]
	mov	r10d,DWORD[8+r11]
	lea	rax,[r10*1+rdx]
	jmp	NEAR $L$keccak_restore_body_or_epilogue

$L$keccak_in_epilogue:
	mov	rdx,QWORD[152+r8]
	mov	r10d,DWORD[20+r11]
	lea	rax,[r10*1+rdx]

$L$keccak_restore_body_or_epilogue:
	mov	rcx,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rcx

	mov	r10d,DWORD[16+r11]
	cmp	r10d,6
	jne	NEAR $L$keccak_restore_two

	mov	r12,QWORD[((-24))+rax]
	mov	r13,QWORD[((-32))+rax]
	mov	r14,QWORD[((-40))+rax]
	mov	r15,QWORD[((-48))+rax]
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

$L$keccak_restore_two:
	mov	rbp,QWORD[((-8))+rax]
	mov	rbx,QWORD[((-16))+rax]
	mov	QWORD[160+r8],rbp
	mov	QWORD[144+r8],rbx

	mov	r10d,DWORD[12+r11]
	cmp	r10d,-1
	je	NEAR $L$keccak_in_prologue

	lea	rsi,[r10*1+rdx]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

$L$keccak_in_prologue:
	mov	rcx,QWORD[8+rax]
	mov	rdx,QWORD[16+rax]
	mov	QWORD[176+r8],rcx
	mov	QWORD[168+r8],rdx
	mov	QWORD[152+r8],rax

	mov	rdi,QWORD[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0xa548f3fc

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD[8+rsi]
	mov	r8,QWORD[rsi]
	mov	r9,QWORD[16+rsi]
	mov	r10,QWORD[40+rsi]
	lea	r11,[56+rsi]
	lea	r12,[24+rsi]
	mov	QWORD[32+rsp],r10
	mov	QWORD[40+rsp],r11
	mov	QWORD[48+rsp],r12
	mov	QWORD[56+rsp],rcx
	call	QWORD[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret


section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_SHA3_shake128_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake128_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake128_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake128_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake128_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake128_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake128_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake128_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake128_x4_inc_squeeze_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake128_x4_inc_squeeze_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake256_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake256_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake256_x4_avx512vl wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake256_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake256_x4_inc_absorb_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake256_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake256_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake256_x4_inc_absorb_avx512vl wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl_internal wrt ..imagebase
	DD	$L$SEH_begin_SHA3_shake256_x4_inc_squeeze_avx512vl wrt ..imagebase
	DD	$L$SEH_end_SHA3_shake256_x4_inc_squeeze_avx512vl wrt ..imagebase
	DD	$L$SEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_SHA3_shake128_x4_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake128_x4_body wrt ..imagebase,$L$shake128_x4_epilogue wrt ..imagebase
	DD	1032,856,2,1032
$L$SEH_info_SHA3_shake128_x4_inc_absorb_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake128_absorb_body wrt ..imagebase,$L$shake128_absorb_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake128_x4_inc_absorb_avx512vl_internal:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake128_absorb_body wrt ..imagebase,$L$shake128_absorb_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake128_squeeze_body wrt ..imagebase,$L$shake128_squeeze_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake128_x4_inc_squeeze_avx512vl_internal:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake128_squeeze_body wrt ..imagebase,$L$shake128_squeeze_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake256_x4_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake256_x4_body wrt ..imagebase,$L$shake256_x4_epilogue wrt ..imagebase
	DD	1032,856,2,1032
$L$SEH_info_SHA3_shake256_x4_inc_absorb_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake256_absorb_body wrt ..imagebase,$L$shake256_absorb_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake256_x4_inc_absorb_avx512vl_internal:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake256_absorb_body wrt ..imagebase,$L$shake256_absorb_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake256_squeeze_body wrt ..imagebase,$L$shake256_squeeze_epilogue wrt ..imagebase
	DD	208,0,6,208
$L$SEH_info_SHA3_shake256_x4_inc_squeeze_avx512vl_internal:
DB	9,0,0,0
	DD	keccak_se_handler wrt ..imagebase
	DD	$L$shake256_squeeze_body wrt ..imagebase,$L$shake256_squeeze_epilogue wrt ..imagebase
	DD	208,0,6,208

section	.rdata rdata align=128
ALIGN	128

iotas:
	DQ	0x0000000000000001
	DQ	0x0000000000008082
	DQ	0x800000000000808a
	DQ	0x8000000080008000
	DQ	0x000000000000808b
	DQ	0x0000000080000001
	DQ	0x8000000080008081
	DQ	0x8000000000008009
	DQ	0x000000000000008a
	DQ	0x0000000000000088
	DQ	0x0000000080008009
	DQ	0x000000008000000a
	DQ	0x000000008000808b
	DQ	0x800000000000008b
	DQ	0x8000000000008089
	DQ	0x8000000000008003
	DQ	0x8000000000008002
	DQ	0x8000000000000080
	DQ	0x000000000000800a
	DQ	0x800000008000000a
	DQ	0x8000000080008081
	DQ	0x8000000000008080
	DQ	0x0000000080000001
	DQ	0x8000000080008008


ALIGN	8
byte_kmask_0_to_7:
DB	0x00,0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f

ALIGN	32
shake_terminator_byte_x4:
DB	0,0,0,0,0,0,0,0x80
DB	0,0,0,0,0,0,0,0x80
DB	0,0,0,0,0,0,0,0x80
DB	0,0,0,0,0,0,0,0x80

ALIGN	8
DB	0,0,0,0,0,0,0,0
shake_msg_pad_x4:
DB	0x1F,0,0,0,0,0,0,0
DB	0x1F,0,0,0,0,0,0,0
DB	0x1F,0,0,0,0,0,0,0
DB	0x1F,0,0,0,0,0,0,0

DB	75,101,99,99,97,107,45,49,54,48,48,32,97,98,115,111
DB	114,98,32,97,110,100,32,115,113,117,101,101,122,101,32,102
DB	111,114,32,65,86,88,53,49,50,86,76,44,32,67,82,89
DB	80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114
DB	111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
