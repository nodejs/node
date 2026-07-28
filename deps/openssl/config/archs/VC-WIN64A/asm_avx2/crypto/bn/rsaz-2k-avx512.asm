default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
EXTERN	OPENSSL_ia32cap_P
global	ossl_rsaz_avx512ifma_eligible

ALIGN	32
ossl_rsaz_avx512ifma_eligible:
	mov	ecx,DWORD[((OPENSSL_ia32cap_P+8))]
	xor	eax,eax
	and	ecx,2149777408
	cmp	ecx,2149777408
	cmove	eax,ecx
	DB	0F3h,0C3h		;repret

section	.text code align=64


global	ossl_rsaz_amm52x20_x1_ifma256

ALIGN	32
ossl_rsaz_amm52x20_x1_ifma256:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_rsaz_amm52x20_x1_ifma256:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



DB	243,15,30,250
	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$ossl_rsaz_amm52x20_x1_ifma256_body:


	vpxord	ymm0,ymm0,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm16,ymm0
	vmovdqa64	ymm17,ymm0
	vmovdqa64	ymm18,ymm0
	vmovdqa64	ymm19,ymm0

	xor	r9d,r9d

	mov	r11,rdx
	mov	rax,0xfffffffffffff


	mov	ebx,5

ALIGN	32
$L$loop5:
	mov	r13,QWORD[r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[rsi]
	mulx	r12,r13,r13
	add	r9,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,r8
	imul	r13,r9
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[rcx]
	mulx	r12,r13,r13
	add	r9,r13
	adc	r10,r12

	shr	r9,52
	sal	r10,12
	or	r9,r10

	vpmadd52luq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm19,ymm2,YMMWORD[128+rcx]


	valignq	ymm3,ymm16,ymm3,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm19,ymm18,1
	valignq	ymm19,ymm0,ymm19,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm19,ymm2,YMMWORD[128+rcx]
	mov	r13,QWORD[8+r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[rsi]
	mulx	r12,r13,r13
	add	r9,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,r8
	imul	r13,r9
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[rcx]
	mulx	r12,r13,r13
	add	r9,r13
	adc	r10,r12

	shr	r9,52
	sal	r10,12
	or	r9,r10

	vpmadd52luq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm19,ymm2,YMMWORD[128+rcx]


	valignq	ymm3,ymm16,ymm3,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm19,ymm18,1
	valignq	ymm19,ymm0,ymm19,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm19,ymm2,YMMWORD[128+rcx]
	mov	r13,QWORD[16+r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[rsi]
	mulx	r12,r13,r13
	add	r9,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,r8
	imul	r13,r9
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[rcx]
	mulx	r12,r13,r13
	add	r9,r13
	adc	r10,r12

	shr	r9,52
	sal	r10,12
	or	r9,r10

	vpmadd52luq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm19,ymm2,YMMWORD[128+rcx]


	valignq	ymm3,ymm16,ymm3,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm19,ymm18,1
	valignq	ymm19,ymm0,ymm19,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm19,ymm2,YMMWORD[128+rcx]
	mov	r13,QWORD[24+r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[rsi]
	mulx	r12,r13,r13
	add	r9,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,r8
	imul	r13,r9
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[rcx]
	mulx	r12,r13,r13
	add	r9,r13
	adc	r10,r12

	shr	r9,52
	sal	r10,12
	or	r9,r10

	vpmadd52luq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm19,ymm2,YMMWORD[128+rcx]


	valignq	ymm3,ymm16,ymm3,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm19,ymm18,1
	valignq	ymm19,ymm0,ymm19,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm19,ymm2,YMMWORD[128+rcx]
	lea	r11,[32+r11]
	dec	ebx
	jne	NEAR $L$loop5

	vpbroadcastq	ymm0,r9
	vpblendd	ymm3,ymm3,ymm0,3



	vpsrlq	ymm0,ymm3,52
	vpsrlq	ymm1,ymm16,52
	vpsrlq	ymm2,ymm17,52
	vpsrlq	ymm25,ymm18,52
	vpsrlq	ymm26,ymm19,52


	valignq	ymm26,ymm26,ymm25,3
	valignq	ymm25,ymm25,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]
	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]
	vpandq	ymm19,ymm19,YMMWORD[$L$mask52x4]


	vpaddq	ymm3,ymm3,ymm0
	vpaddq	ymm16,ymm16,ymm1
	vpaddq	ymm17,ymm17,ymm2
	vpaddq	ymm18,ymm18,ymm25
	vpaddq	ymm19,ymm19,ymm26



	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],6
	vpcmpuq	k3,ymm17,YMMWORD[$L$mask52x4],6
	vpcmpuq	k4,ymm18,YMMWORD[$L$mask52x4],6
	vpcmpuq	k5,ymm19,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	kmovb	r12d,k3
	kmovb	r11d,k4
	kmovb	r10d,k5


	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],0
	vpcmpuq	k3,ymm17,YMMWORD[$L$mask52x4],0
	vpcmpuq	k4,ymm18,YMMWORD[$L$mask52x4],0
	vpcmpuq	k5,ymm19,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	kmovb	ebx,k3
	kmovb	ecx,k4
	kmovb	edx,k5



	shl	r13b,4
	or	r14b,r13b
	shl	r11b,4
	or	r12b,r11b

	add	r14b,r14b
	adc	r12b,r12b
	adc	r10b,r10b

	shl	r8b,4
	or	r9b,r8b
	shl	cl,4
	or	bl,cl

	add	r14b,r9b
	adc	r12b,bl
	adc	r10b,dl

	xor	r14b,r9b
	xor	r12b,bl
	xor	r10b,dl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r12d
	shr	r12b,4
	kmovb	k4,r12d
	kmovb	k5,r10d


	vpsubq	ymm3{k1},ymm3,YMMWORD[$L$mask52x4]
	vpsubq	ymm16{k2},ymm16,YMMWORD[$L$mask52x4]
	vpsubq	ymm17{k3},ymm17,YMMWORD[$L$mask52x4]
	vpsubq	ymm18{k4},ymm18,YMMWORD[$L$mask52x4]
	vpsubq	ymm19{k5},ymm19,YMMWORD[$L$mask52x4]

	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]
	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]
	vpandq	ymm19,ymm19,YMMWORD[$L$mask52x4]

	vmovdqu64	YMMWORD[rdi],ymm3
	vmovdqu64	YMMWORD[32+rdi],ymm16
	vmovdqu64	YMMWORD[64+rdi],ymm17
	vmovdqu64	YMMWORD[96+rdi],ymm18
	vmovdqu64	YMMWORD[128+rdi],ymm19

	vzeroupper
	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbp,QWORD[32+rsp]

	mov	rbx,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ossl_rsaz_amm52x20_x1_ifma256_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_rsaz_amm52x20_x1_ifma256:
section	.rdata rdata align=32
ALIGN	32
$L$mask52x4:
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
section	.text code align=64


global	ossl_rsaz_amm52x20_x2_ifma256

ALIGN	32
ossl_rsaz_amm52x20_x2_ifma256:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_rsaz_amm52x20_x2_ifma256:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]



DB	243,15,30,250
	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

$L$ossl_rsaz_amm52x20_x2_ifma256_body:


	vpxord	ymm0,ymm0,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm16,ymm0
	vmovdqa64	ymm17,ymm0
	vmovdqa64	ymm18,ymm0
	vmovdqa64	ymm19,ymm0
	vmovdqa64	ymm4,ymm0
	vmovdqa64	ymm20,ymm0
	vmovdqa64	ymm21,ymm0
	vmovdqa64	ymm22,ymm0
	vmovdqa64	ymm23,ymm0

	xor	r9d,r9d
	xor	r15d,r15d

	mov	r11,rdx
	mov	rax,0xfffffffffffff

	mov	ebx,20

ALIGN	32
$L$loop20:
	mov	r13,QWORD[r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[rsi]
	mulx	r12,r13,r13
	add	r9,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,QWORD[r8]
	imul	r13,r9
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[rcx]
	mulx	r12,r13,r13
	add	r9,r13
	adc	r10,r12

	shr	r9,52
	sal	r10,12
	or	r9,r10

	vpmadd52luq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm19,ymm2,YMMWORD[128+rcx]


	valignq	ymm3,ymm16,ymm3,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm19,ymm18,1
	valignq	ymm19,ymm0,ymm19,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm19,ymm1,YMMWORD[128+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm19,ymm2,YMMWORD[128+rcx]
	mov	r13,QWORD[160+r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[160+rsi]
	mulx	r12,r13,r13
	add	r15,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,QWORD[8+r8]
	imul	r13,r15
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[160+rcx]
	mulx	r12,r13,r13
	add	r15,r13
	adc	r10,r12

	shr	r15,52
	sal	r10,12
	or	r15,r10

	vpmadd52luq	ymm4,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm20,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm21,ymm1,YMMWORD[224+rsi]
	vpmadd52luq	ymm22,ymm1,YMMWORD[256+rsi]
	vpmadd52luq	ymm23,ymm1,YMMWORD[288+rsi]

	vpmadd52luq	ymm4,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm20,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm21,ymm2,YMMWORD[224+rcx]
	vpmadd52luq	ymm22,ymm2,YMMWORD[256+rcx]
	vpmadd52luq	ymm23,ymm2,YMMWORD[288+rcx]


	valignq	ymm4,ymm20,ymm4,1
	valignq	ymm20,ymm21,ymm20,1
	valignq	ymm21,ymm22,ymm21,1
	valignq	ymm22,ymm23,ymm22,1
	valignq	ymm23,ymm0,ymm23,1

	vmovq	r13,xmm4
	add	r15,r13

	vpmadd52huq	ymm4,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm20,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm21,ymm1,YMMWORD[224+rsi]
	vpmadd52huq	ymm22,ymm1,YMMWORD[256+rsi]
	vpmadd52huq	ymm23,ymm1,YMMWORD[288+rsi]

	vpmadd52huq	ymm4,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm20,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm21,ymm2,YMMWORD[224+rcx]
	vpmadd52huq	ymm22,ymm2,YMMWORD[256+rcx]
	vpmadd52huq	ymm23,ymm2,YMMWORD[288+rcx]
	lea	r11,[8+r11]
	dec	ebx
	jne	NEAR $L$loop20

	vpbroadcastq	ymm0,r9
	vpblendd	ymm3,ymm3,ymm0,3



	vpsrlq	ymm0,ymm3,52
	vpsrlq	ymm1,ymm16,52
	vpsrlq	ymm2,ymm17,52
	vpsrlq	ymm25,ymm18,52
	vpsrlq	ymm26,ymm19,52


	valignq	ymm26,ymm26,ymm25,3
	valignq	ymm25,ymm25,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]
	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]
	vpandq	ymm19,ymm19,YMMWORD[$L$mask52x4]


	vpaddq	ymm3,ymm3,ymm0
	vpaddq	ymm16,ymm16,ymm1
	vpaddq	ymm17,ymm17,ymm2
	vpaddq	ymm18,ymm18,ymm25
	vpaddq	ymm19,ymm19,ymm26



	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],6
	vpcmpuq	k3,ymm17,YMMWORD[$L$mask52x4],6
	vpcmpuq	k4,ymm18,YMMWORD[$L$mask52x4],6
	vpcmpuq	k5,ymm19,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	kmovb	r12d,k3
	kmovb	r11d,k4
	kmovb	r10d,k5


	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],0
	vpcmpuq	k3,ymm17,YMMWORD[$L$mask52x4],0
	vpcmpuq	k4,ymm18,YMMWORD[$L$mask52x4],0
	vpcmpuq	k5,ymm19,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	kmovb	ebx,k3
	kmovb	ecx,k4
	kmovb	edx,k5



	shl	r13b,4
	or	r14b,r13b
	shl	r11b,4
	or	r12b,r11b

	add	r14b,r14b
	adc	r12b,r12b
	adc	r10b,r10b

	shl	r8b,4
	or	r9b,r8b
	shl	cl,4
	or	bl,cl

	add	r14b,r9b
	adc	r12b,bl
	adc	r10b,dl

	xor	r14b,r9b
	xor	r12b,bl
	xor	r10b,dl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r12d
	shr	r12b,4
	kmovb	k4,r12d
	kmovb	k5,r10d


	vpsubq	ymm3{k1},ymm3,YMMWORD[$L$mask52x4]
	vpsubq	ymm16{k2},ymm16,YMMWORD[$L$mask52x4]
	vpsubq	ymm17{k3},ymm17,YMMWORD[$L$mask52x4]
	vpsubq	ymm18{k4},ymm18,YMMWORD[$L$mask52x4]
	vpsubq	ymm19{k5},ymm19,YMMWORD[$L$mask52x4]

	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]
	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]
	vpandq	ymm19,ymm19,YMMWORD[$L$mask52x4]

	vpbroadcastq	ymm0,r15
	vpblendd	ymm4,ymm4,ymm0,3



	vpsrlq	ymm0,ymm4,52
	vpsrlq	ymm1,ymm20,52
	vpsrlq	ymm2,ymm21,52
	vpsrlq	ymm25,ymm22,52
	vpsrlq	ymm26,ymm23,52


	valignq	ymm26,ymm26,ymm25,3
	valignq	ymm25,ymm25,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm20,ymm20,YMMWORD[$L$mask52x4]
	vpandq	ymm21,ymm21,YMMWORD[$L$mask52x4]
	vpandq	ymm22,ymm22,YMMWORD[$L$mask52x4]
	vpandq	ymm23,ymm23,YMMWORD[$L$mask52x4]


	vpaddq	ymm4,ymm4,ymm0
	vpaddq	ymm20,ymm20,ymm1
	vpaddq	ymm21,ymm21,ymm2
	vpaddq	ymm22,ymm22,ymm25
	vpaddq	ymm23,ymm23,ymm26



	vpcmpuq	k1,ymm4,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm20,YMMWORD[$L$mask52x4],6
	vpcmpuq	k3,ymm21,YMMWORD[$L$mask52x4],6
	vpcmpuq	k4,ymm22,YMMWORD[$L$mask52x4],6
	vpcmpuq	k5,ymm23,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	kmovb	r12d,k3
	kmovb	r11d,k4
	kmovb	r10d,k5


	vpcmpuq	k1,ymm4,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm20,YMMWORD[$L$mask52x4],0
	vpcmpuq	k3,ymm21,YMMWORD[$L$mask52x4],0
	vpcmpuq	k4,ymm22,YMMWORD[$L$mask52x4],0
	vpcmpuq	k5,ymm23,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	kmovb	ebx,k3
	kmovb	ecx,k4
	kmovb	edx,k5



	shl	r13b,4
	or	r14b,r13b
	shl	r11b,4
	or	r12b,r11b

	add	r14b,r14b
	adc	r12b,r12b
	adc	r10b,r10b

	shl	r8b,4
	or	r9b,r8b
	shl	cl,4
	or	bl,cl

	add	r14b,r9b
	adc	r12b,bl
	adc	r10b,dl

	xor	r14b,r9b
	xor	r12b,bl
	xor	r10b,dl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r12d
	shr	r12b,4
	kmovb	k4,r12d
	kmovb	k5,r10d


	vpsubq	ymm4{k1},ymm4,YMMWORD[$L$mask52x4]
	vpsubq	ymm20{k2},ymm20,YMMWORD[$L$mask52x4]
	vpsubq	ymm21{k3},ymm21,YMMWORD[$L$mask52x4]
	vpsubq	ymm22{k4},ymm22,YMMWORD[$L$mask52x4]
	vpsubq	ymm23{k5},ymm23,YMMWORD[$L$mask52x4]

	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm20,ymm20,YMMWORD[$L$mask52x4]
	vpandq	ymm21,ymm21,YMMWORD[$L$mask52x4]
	vpandq	ymm22,ymm22,YMMWORD[$L$mask52x4]
	vpandq	ymm23,ymm23,YMMWORD[$L$mask52x4]

	vmovdqu64	YMMWORD[rdi],ymm3
	vmovdqu64	YMMWORD[32+rdi],ymm16
	vmovdqu64	YMMWORD[64+rdi],ymm17
	vmovdqu64	YMMWORD[96+rdi],ymm18
	vmovdqu64	YMMWORD[128+rdi],ymm19

	vmovdqu64	YMMWORD[160+rdi],ymm4
	vmovdqu64	YMMWORD[192+rdi],ymm20
	vmovdqu64	YMMWORD[224+rdi],ymm21
	vmovdqu64	YMMWORD[256+rdi],ymm22
	vmovdqu64	YMMWORD[288+rdi],ymm23

	vzeroupper
	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbp,QWORD[32+rsp]

	mov	rbx,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ossl_rsaz_amm52x20_x2_ifma256_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_rsaz_amm52x20_x2_ifma256:
section	.text code align=64


ALIGN	32
global	ossl_extract_multiplier_2x20_win5

ossl_extract_multiplier_2x20_win5:

DB	243,15,30,250
	vmovdqa64	ymm24,YMMWORD[$L$ones]
	vpbroadcastq	ymm22,r8
	vpbroadcastq	ymm23,r9
	lea	rax,[10240+rdx]


	vpxor	xmm0,xmm0,xmm0
	vmovdqa64	ymm21,ymm0
	vmovdqa64	ymm1,ymm0
	vmovdqa64	ymm2,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm4,ymm0
	vmovdqa64	ymm5,ymm0
	vmovdqa64	ymm16,ymm0
	vmovdqa64	ymm17,ymm0
	vmovdqa64	ymm18,ymm0
	vmovdqa64	ymm19,ymm0

ALIGN	32
$L$loop:
	vpcmpq	k1,ymm22,ymm21,0
	vpcmpq	k2,ymm23,ymm21,0
	vmovdqu64	ymm20,YMMWORD[rdx]
	vpblendmq	ymm0{k1},ymm0,ymm20
	vmovdqu64	ymm20,YMMWORD[32+rdx]
	vpblendmq	ymm1{k1},ymm1,ymm20
	vmovdqu64	ymm20,YMMWORD[64+rdx]
	vpblendmq	ymm2{k1},ymm2,ymm20
	vmovdqu64	ymm20,YMMWORD[96+rdx]
	vpblendmq	ymm3{k1},ymm3,ymm20
	vmovdqu64	ymm20,YMMWORD[128+rdx]
	vpblendmq	ymm4{k1},ymm4,ymm20
	vmovdqu64	ymm20,YMMWORD[160+rdx]
	vpblendmq	ymm5{k2},ymm5,ymm20
	vmovdqu64	ymm20,YMMWORD[192+rdx]
	vpblendmq	ymm16{k2},ymm16,ymm20
	vmovdqu64	ymm20,YMMWORD[224+rdx]
	vpblendmq	ymm17{k2},ymm17,ymm20
	vmovdqu64	ymm20,YMMWORD[256+rdx]
	vpblendmq	ymm18{k2},ymm18,ymm20
	vmovdqu64	ymm20,YMMWORD[288+rdx]
	vpblendmq	ymm19{k2},ymm19,ymm20
	vpaddq	ymm21,ymm21,ymm24
	add	rdx,320
	cmp	rax,rdx
	jne	NEAR $L$loop
	vmovdqu64	YMMWORD[rcx],ymm0
	vmovdqu64	YMMWORD[32+rcx],ymm1
	vmovdqu64	YMMWORD[64+rcx],ymm2
	vmovdqu64	YMMWORD[96+rcx],ymm3
	vmovdqu64	YMMWORD[128+rcx],ymm4
	vmovdqu64	YMMWORD[160+rcx],ymm5
	vmovdqu64	YMMWORD[192+rcx],ymm16
	vmovdqu64	YMMWORD[224+rcx],ymm17
	vmovdqu64	YMMWORD[256+rcx],ymm18
	vmovdqu64	YMMWORD[288+rcx],ymm19
	DB	0F3h,0C3h		;repret


section	.rdata rdata align=32
ALIGN	32
$L$ones:
	DQ	1,1,1,1
$L$zeros:
	DQ	0,0,0,0
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
rsaz_def_handler:
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
	jb	NEAR $L$common_seh_tail

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$common_seh_tail

	lea	rax,[48+rax]

	mov	rbx,QWORD[((-8))+rax]
	mov	rbp,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r13,QWORD[((-32))+rax]
	mov	r14,QWORD[((-40))+rax]
	mov	r15,QWORD[((-48))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

$L$common_seh_tail:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi

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
	DD	$L$SEH_begin_ossl_rsaz_amm52x20_x1_ifma256 wrt ..imagebase
	DD	$L$SEH_end_ossl_rsaz_amm52x20_x1_ifma256 wrt ..imagebase
	DD	$L$SEH_info_ossl_rsaz_amm52x20_x1_ifma256 wrt ..imagebase

	DD	$L$SEH_begin_ossl_rsaz_amm52x20_x2_ifma256 wrt ..imagebase
	DD	$L$SEH_end_ossl_rsaz_amm52x20_x2_ifma256 wrt ..imagebase
	DD	$L$SEH_info_ossl_rsaz_amm52x20_x2_ifma256 wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_ossl_rsaz_amm52x20_x1_ifma256:
DB	9,0,0,0
	DD	rsaz_def_handler wrt ..imagebase
	DD	$L$ossl_rsaz_amm52x20_x1_ifma256_body wrt ..imagebase,$L$ossl_rsaz_amm52x20_x1_ifma256_epilogue wrt ..imagebase
$L$SEH_info_ossl_rsaz_amm52x20_x2_ifma256:
DB	9,0,0,0
	DD	rsaz_def_handler wrt ..imagebase
	DD	$L$ossl_rsaz_amm52x20_x2_ifma256_body wrt ..imagebase,$L$ossl_rsaz_amm52x20_x2_ifma256_epilogue wrt ..imagebase
