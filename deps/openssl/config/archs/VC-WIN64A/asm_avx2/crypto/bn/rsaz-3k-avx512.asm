default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


global	ossl_rsaz_amm52x30_x1_ifma256

ALIGN	32
ossl_rsaz_amm52x30_x1_ifma256:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_rsaz_amm52x30_x1_ifma256:
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

	lea	rsp,[((-168))+rsp]
	vmovdqa64	XMMWORD[rsp],xmm6
	vmovdqa64	XMMWORD[16+rsp],xmm7
	vmovdqa64	XMMWORD[32+rsp],xmm8
	vmovdqa64	XMMWORD[48+rsp],xmm9
	vmovdqa64	XMMWORD[64+rsp],xmm10
	vmovdqa64	XMMWORD[80+rsp],xmm11
	vmovdqa64	XMMWORD[96+rsp],xmm12
	vmovdqa64	XMMWORD[112+rsp],xmm13
	vmovdqa64	XMMWORD[128+rsp],xmm14
	vmovdqa64	XMMWORD[144+rsp],xmm15
$L$ossl_rsaz_amm52x30_x1_ifma256_body:

	vpxord	ymm0,ymm0,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm4,ymm0
	vmovdqa64	ymm5,ymm0
	vmovdqa64	ymm6,ymm0
	vmovdqa64	ymm7,ymm0
	vmovdqa64	ymm8,ymm0
	vmovdqa64	ymm9,ymm0
	vmovdqa64	ymm10,ymm0

	xor	r9d,r9d

	mov	r11,rdx
	mov	rax,0xfffffffffffff


	mov	ebx,7

ALIGN	32
$L$loop7:
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
	lea	r11,[32+r11]
	dec	ebx
	jne	NEAR $L$loop7
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]

	vpbroadcastq	ymm0,r9
	vpblendd	ymm3,ymm3,ymm0,3



	vpsrlq	ymm0,ymm3,52
	vpsrlq	ymm1,ymm4,52
	vpsrlq	ymm2,ymm5,52
	vpsrlq	ymm19,ymm6,52
	vpsrlq	ymm20,ymm7,52
	vpsrlq	ymm21,ymm8,52
	vpsrlq	ymm22,ymm9,52
	vpsrlq	ymm23,ymm10,52


	valignq	ymm23,ymm23,ymm22,3
	valignq	ymm22,ymm22,ymm21,3
	valignq	ymm21,ymm21,ymm20,3
	valignq	ymm20,ymm20,ymm19,3
	valignq	ymm19,ymm19,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm5,ymm5,YMMWORD[$L$mask52x4]
	vpandq	ymm6,ymm6,YMMWORD[$L$mask52x4]
	vpandq	ymm7,ymm7,YMMWORD[$L$mask52x4]
	vpandq	ymm8,ymm8,YMMWORD[$L$mask52x4]
	vpandq	ymm9,ymm9,YMMWORD[$L$mask52x4]
	vpandq	ymm10,ymm10,YMMWORD[$L$mask52x4]


	vpaddq	ymm3,ymm3,ymm0
	vpaddq	ymm4,ymm4,ymm1
	vpaddq	ymm5,ymm5,ymm2
	vpaddq	ymm6,ymm6,ymm19
	vpaddq	ymm7,ymm7,ymm20
	vpaddq	ymm8,ymm8,ymm21
	vpaddq	ymm9,ymm9,ymm22
	vpaddq	ymm10,ymm10,ymm23



	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm4,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	shl	r13b,4
	or	r14b,r13b

	vpcmpuq	k1,ymm5,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm6,YMMWORD[$L$mask52x4],6
	kmovb	r13d,k1
	kmovb	r12d,k2
	shl	r12b,4
	or	r13b,r12b

	vpcmpuq	k1,ymm7,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm8,YMMWORD[$L$mask52x4],6
	kmovb	r12d,k1
	kmovb	r11d,k2
	shl	r11b,4
	or	r12b,r11b

	vpcmpuq	k1,ymm9,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm10,YMMWORD[$L$mask52x4],6
	kmovb	r11d,k1
	kmovb	r10d,k2
	shl	r10b,4
	or	r11b,r10b

	add	r14b,r14b
	adc	r13b,r13b
	adc	r12b,r12b
	adc	r11b,r11b


	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm4,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	shl	r8b,4
	or	r9b,r8b

	vpcmpuq	k1,ymm5,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm6,YMMWORD[$L$mask52x4],0
	kmovb	r8d,k1
	kmovb	edx,k2
	shl	dl,4
	or	r8b,dl

	vpcmpuq	k1,ymm7,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm8,YMMWORD[$L$mask52x4],0
	kmovb	edx,k1
	kmovb	ecx,k2
	shl	cl,4
	or	dl,cl

	vpcmpuq	k1,ymm9,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm10,YMMWORD[$L$mask52x4],0
	kmovb	ecx,k1
	kmovb	ebx,k2
	shl	bl,4
	or	cl,bl

	add	r14b,r9b
	adc	r13b,r8b
	adc	r12b,dl
	adc	r11b,cl

	xor	r14b,r9b
	xor	r13b,r8b
	xor	r12b,dl
	xor	r11b,cl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r13d
	shr	r13b,4
	kmovb	k4,r13d
	kmovb	k5,r12d
	shr	r12b,4
	kmovb	k6,r12d
	kmovb	k7,r11d

	vpsubq	ymm3{k1},ymm3,YMMWORD[$L$mask52x4]
	vpsubq	ymm4{k2},ymm4,YMMWORD[$L$mask52x4]
	vpsubq	ymm5{k3},ymm5,YMMWORD[$L$mask52x4]
	vpsubq	ymm6{k4},ymm6,YMMWORD[$L$mask52x4]
	vpsubq	ymm7{k5},ymm7,YMMWORD[$L$mask52x4]
	vpsubq	ymm8{k6},ymm8,YMMWORD[$L$mask52x4]
	vpsubq	ymm9{k7},ymm9,YMMWORD[$L$mask52x4]

	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm5,ymm5,YMMWORD[$L$mask52x4]
	vpandq	ymm6,ymm6,YMMWORD[$L$mask52x4]
	vpandq	ymm7,ymm7,YMMWORD[$L$mask52x4]
	vpandq	ymm8,ymm8,YMMWORD[$L$mask52x4]
	vpandq	ymm9,ymm9,YMMWORD[$L$mask52x4]

	shr	r11b,4
	kmovb	k1,r11d

	vpsubq	ymm10{k1},ymm10,YMMWORD[$L$mask52x4]

	vpandq	ymm10,ymm10,YMMWORD[$L$mask52x4]

	vmovdqu64	YMMWORD[rdi],ymm3
	vmovdqu64	YMMWORD[32+rdi],ymm4
	vmovdqu64	YMMWORD[64+rdi],ymm5
	vmovdqu64	YMMWORD[96+rdi],ymm6
	vmovdqu64	YMMWORD[128+rdi],ymm7
	vmovdqu64	YMMWORD[160+rdi],ymm8
	vmovdqu64	YMMWORD[192+rdi],ymm9
	vmovdqu64	YMMWORD[224+rdi],ymm10

	vzeroupper
	lea	rax,[rsp]

	vmovdqa64	xmm6,XMMWORD[rax]
	vmovdqa64	xmm7,XMMWORD[16+rax]
	vmovdqa64	xmm8,XMMWORD[32+rax]
	vmovdqa64	xmm9,XMMWORD[48+rax]
	vmovdqa64	xmm10,XMMWORD[64+rax]
	vmovdqa64	xmm11,XMMWORD[80+rax]
	vmovdqa64	xmm12,XMMWORD[96+rax]
	vmovdqa64	xmm13,XMMWORD[112+rax]
	vmovdqa64	xmm14,XMMWORD[128+rax]
	vmovdqa64	xmm15,XMMWORD[144+rax]
	lea	rax,[168+rsp]
	mov	r15,QWORD[rax]

	mov	r14,QWORD[8+rax]

	mov	r13,QWORD[16+rax]

	mov	r12,QWORD[24+rax]

	mov	rbp,QWORD[32+rax]

	mov	rbx,QWORD[40+rax]

	lea	rsp,[48+rax]

$L$ossl_rsaz_amm52x30_x1_ifma256_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_rsaz_amm52x30_x1_ifma256:
section	.rdata rdata align=32
ALIGN	32
$L$mask52x4:
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
	DQ	0xfffffffffffff
section	.text code align=64


global	ossl_rsaz_amm52x30_x2_ifma256

ALIGN	32
ossl_rsaz_amm52x30_x2_ifma256:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ossl_rsaz_amm52x30_x2_ifma256:
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

	lea	rsp,[((-168))+rsp]
	vmovdqa64	XMMWORD[rsp],xmm6
	vmovdqa64	XMMWORD[16+rsp],xmm7
	vmovdqa64	XMMWORD[32+rsp],xmm8
	vmovdqa64	XMMWORD[48+rsp],xmm9
	vmovdqa64	XMMWORD[64+rsp],xmm10
	vmovdqa64	XMMWORD[80+rsp],xmm11
	vmovdqa64	XMMWORD[96+rsp],xmm12
	vmovdqa64	XMMWORD[112+rsp],xmm13
	vmovdqa64	XMMWORD[128+rsp],xmm14
	vmovdqa64	XMMWORD[144+rsp],xmm15
$L$ossl_rsaz_amm52x30_x2_ifma256_body:

	vpxord	ymm0,ymm0,ymm0
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


	xor	r9d,r9d
	xor	r15d,r15d

	mov	r11,rdx
	mov	rax,0xfffffffffffff

	mov	ebx,30

ALIGN	32
$L$loop30:
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
	vpmadd52luq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52luq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52luq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52luq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52luq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52luq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52luq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52luq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52luq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52luq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52luq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52luq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52luq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52luq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52luq	ymm10,ymm2,YMMWORD[224+rcx]


	valignq	ymm3,ymm4,ymm3,1
	valignq	ymm4,ymm5,ymm4,1
	valignq	ymm5,ymm6,ymm5,1
	valignq	ymm6,ymm7,ymm6,1
	valignq	ymm7,ymm8,ymm7,1
	valignq	ymm8,ymm9,ymm8,1
	valignq	ymm9,ymm10,ymm9,1
	valignq	ymm10,ymm0,ymm10,1

	vmovq	r13,xmm3
	add	r9,r13

	vpmadd52huq	ymm3,ymm1,YMMWORD[rsi]
	vpmadd52huq	ymm4,ymm1,YMMWORD[32+rsi]
	vpmadd52huq	ymm5,ymm1,YMMWORD[64+rsi]
	vpmadd52huq	ymm6,ymm1,YMMWORD[96+rsi]
	vpmadd52huq	ymm7,ymm1,YMMWORD[128+rsi]
	vpmadd52huq	ymm8,ymm1,YMMWORD[160+rsi]
	vpmadd52huq	ymm9,ymm1,YMMWORD[192+rsi]
	vpmadd52huq	ymm10,ymm1,YMMWORD[224+rsi]

	vpmadd52huq	ymm3,ymm2,YMMWORD[rcx]
	vpmadd52huq	ymm4,ymm2,YMMWORD[32+rcx]
	vpmadd52huq	ymm5,ymm2,YMMWORD[64+rcx]
	vpmadd52huq	ymm6,ymm2,YMMWORD[96+rcx]
	vpmadd52huq	ymm7,ymm2,YMMWORD[128+rcx]
	vpmadd52huq	ymm8,ymm2,YMMWORD[160+rcx]
	vpmadd52huq	ymm9,ymm2,YMMWORD[192+rcx]
	vpmadd52huq	ymm10,ymm2,YMMWORD[224+rcx]
	mov	r13,QWORD[256+r11]

	vpbroadcastq	ymm1,r13
	mov	rdx,QWORD[256+rsi]
	mulx	r12,r13,r13
	add	r15,r13
	mov	r10,r12
	adc	r10,0

	mov	r13,QWORD[8+r8]
	imul	r13,r15
	and	r13,rax

	vpbroadcastq	ymm2,r13
	mov	rdx,QWORD[256+rcx]
	mulx	r12,r13,r13
	add	r15,r13
	adc	r10,r12

	shr	r15,52
	sal	r10,12
	or	r15,r10

	vpmadd52luq	ymm11,ymm1,YMMWORD[256+rsi]
	vpmadd52luq	ymm12,ymm1,YMMWORD[288+rsi]
	vpmadd52luq	ymm13,ymm1,YMMWORD[320+rsi]
	vpmadd52luq	ymm14,ymm1,YMMWORD[352+rsi]
	vpmadd52luq	ymm15,ymm1,YMMWORD[384+rsi]
	vpmadd52luq	ymm16,ymm1,YMMWORD[416+rsi]
	vpmadd52luq	ymm17,ymm1,YMMWORD[448+rsi]
	vpmadd52luq	ymm18,ymm1,YMMWORD[480+rsi]

	vpmadd52luq	ymm11,ymm2,YMMWORD[256+rcx]
	vpmadd52luq	ymm12,ymm2,YMMWORD[288+rcx]
	vpmadd52luq	ymm13,ymm2,YMMWORD[320+rcx]
	vpmadd52luq	ymm14,ymm2,YMMWORD[352+rcx]
	vpmadd52luq	ymm15,ymm2,YMMWORD[384+rcx]
	vpmadd52luq	ymm16,ymm2,YMMWORD[416+rcx]
	vpmadd52luq	ymm17,ymm2,YMMWORD[448+rcx]
	vpmadd52luq	ymm18,ymm2,YMMWORD[480+rcx]


	valignq	ymm11,ymm12,ymm11,1
	valignq	ymm12,ymm13,ymm12,1
	valignq	ymm13,ymm14,ymm13,1
	valignq	ymm14,ymm15,ymm14,1
	valignq	ymm15,ymm16,ymm15,1
	valignq	ymm16,ymm17,ymm16,1
	valignq	ymm17,ymm18,ymm17,1
	valignq	ymm18,ymm0,ymm18,1

	vmovq	r13,xmm11
	add	r15,r13

	vpmadd52huq	ymm11,ymm1,YMMWORD[256+rsi]
	vpmadd52huq	ymm12,ymm1,YMMWORD[288+rsi]
	vpmadd52huq	ymm13,ymm1,YMMWORD[320+rsi]
	vpmadd52huq	ymm14,ymm1,YMMWORD[352+rsi]
	vpmadd52huq	ymm15,ymm1,YMMWORD[384+rsi]
	vpmadd52huq	ymm16,ymm1,YMMWORD[416+rsi]
	vpmadd52huq	ymm17,ymm1,YMMWORD[448+rsi]
	vpmadd52huq	ymm18,ymm1,YMMWORD[480+rsi]

	vpmadd52huq	ymm11,ymm2,YMMWORD[256+rcx]
	vpmadd52huq	ymm12,ymm2,YMMWORD[288+rcx]
	vpmadd52huq	ymm13,ymm2,YMMWORD[320+rcx]
	vpmadd52huq	ymm14,ymm2,YMMWORD[352+rcx]
	vpmadd52huq	ymm15,ymm2,YMMWORD[384+rcx]
	vpmadd52huq	ymm16,ymm2,YMMWORD[416+rcx]
	vpmadd52huq	ymm17,ymm2,YMMWORD[448+rcx]
	vpmadd52huq	ymm18,ymm2,YMMWORD[480+rcx]
	lea	r11,[8+r11]
	dec	ebx
	jne	NEAR $L$loop30

	vpbroadcastq	ymm0,r9
	vpblendd	ymm3,ymm3,ymm0,3



	vpsrlq	ymm0,ymm3,52
	vpsrlq	ymm1,ymm4,52
	vpsrlq	ymm2,ymm5,52
	vpsrlq	ymm19,ymm6,52
	vpsrlq	ymm20,ymm7,52
	vpsrlq	ymm21,ymm8,52
	vpsrlq	ymm22,ymm9,52
	vpsrlq	ymm23,ymm10,52


	valignq	ymm23,ymm23,ymm22,3
	valignq	ymm22,ymm22,ymm21,3
	valignq	ymm21,ymm21,ymm20,3
	valignq	ymm20,ymm20,ymm19,3
	valignq	ymm19,ymm19,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm5,ymm5,YMMWORD[$L$mask52x4]
	vpandq	ymm6,ymm6,YMMWORD[$L$mask52x4]
	vpandq	ymm7,ymm7,YMMWORD[$L$mask52x4]
	vpandq	ymm8,ymm8,YMMWORD[$L$mask52x4]
	vpandq	ymm9,ymm9,YMMWORD[$L$mask52x4]
	vpandq	ymm10,ymm10,YMMWORD[$L$mask52x4]


	vpaddq	ymm3,ymm3,ymm0
	vpaddq	ymm4,ymm4,ymm1
	vpaddq	ymm5,ymm5,ymm2
	vpaddq	ymm6,ymm6,ymm19
	vpaddq	ymm7,ymm7,ymm20
	vpaddq	ymm8,ymm8,ymm21
	vpaddq	ymm9,ymm9,ymm22
	vpaddq	ymm10,ymm10,ymm23



	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm4,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	shl	r13b,4
	or	r14b,r13b

	vpcmpuq	k1,ymm5,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm6,YMMWORD[$L$mask52x4],6
	kmovb	r13d,k1
	kmovb	r12d,k2
	shl	r12b,4
	or	r13b,r12b

	vpcmpuq	k1,ymm7,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm8,YMMWORD[$L$mask52x4],6
	kmovb	r12d,k1
	kmovb	r11d,k2
	shl	r11b,4
	or	r12b,r11b

	vpcmpuq	k1,ymm9,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm10,YMMWORD[$L$mask52x4],6
	kmovb	r11d,k1
	kmovb	r10d,k2
	shl	r10b,4
	or	r11b,r10b

	add	r14b,r14b
	adc	r13b,r13b
	adc	r12b,r12b
	adc	r11b,r11b


	vpcmpuq	k1,ymm3,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm4,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	shl	r8b,4
	or	r9b,r8b

	vpcmpuq	k1,ymm5,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm6,YMMWORD[$L$mask52x4],0
	kmovb	r8d,k1
	kmovb	edx,k2
	shl	dl,4
	or	r8b,dl

	vpcmpuq	k1,ymm7,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm8,YMMWORD[$L$mask52x4],0
	kmovb	edx,k1
	kmovb	ecx,k2
	shl	cl,4
	or	dl,cl

	vpcmpuq	k1,ymm9,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm10,YMMWORD[$L$mask52x4],0
	kmovb	ecx,k1
	kmovb	ebx,k2
	shl	bl,4
	or	cl,bl

	add	r14b,r9b
	adc	r13b,r8b
	adc	r12b,dl
	adc	r11b,cl

	xor	r14b,r9b
	xor	r13b,r8b
	xor	r12b,dl
	xor	r11b,cl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r13d
	shr	r13b,4
	kmovb	k4,r13d
	kmovb	k5,r12d
	shr	r12b,4
	kmovb	k6,r12d
	kmovb	k7,r11d

	vpsubq	ymm3{k1},ymm3,YMMWORD[$L$mask52x4]
	vpsubq	ymm4{k2},ymm4,YMMWORD[$L$mask52x4]
	vpsubq	ymm5{k3},ymm5,YMMWORD[$L$mask52x4]
	vpsubq	ymm6{k4},ymm6,YMMWORD[$L$mask52x4]
	vpsubq	ymm7{k5},ymm7,YMMWORD[$L$mask52x4]
	vpsubq	ymm8{k6},ymm8,YMMWORD[$L$mask52x4]
	vpsubq	ymm9{k7},ymm9,YMMWORD[$L$mask52x4]

	vpandq	ymm3,ymm3,YMMWORD[$L$mask52x4]
	vpandq	ymm4,ymm4,YMMWORD[$L$mask52x4]
	vpandq	ymm5,ymm5,YMMWORD[$L$mask52x4]
	vpandq	ymm6,ymm6,YMMWORD[$L$mask52x4]
	vpandq	ymm7,ymm7,YMMWORD[$L$mask52x4]
	vpandq	ymm8,ymm8,YMMWORD[$L$mask52x4]
	vpandq	ymm9,ymm9,YMMWORD[$L$mask52x4]

	shr	r11b,4
	kmovb	k1,r11d

	vpsubq	ymm10{k1},ymm10,YMMWORD[$L$mask52x4]

	vpandq	ymm10,ymm10,YMMWORD[$L$mask52x4]

	vpbroadcastq	ymm0,r15
	vpblendd	ymm11,ymm11,ymm0,3



	vpsrlq	ymm0,ymm11,52
	vpsrlq	ymm1,ymm12,52
	vpsrlq	ymm2,ymm13,52
	vpsrlq	ymm19,ymm14,52
	vpsrlq	ymm20,ymm15,52
	vpsrlq	ymm21,ymm16,52
	vpsrlq	ymm22,ymm17,52
	vpsrlq	ymm23,ymm18,52


	valignq	ymm23,ymm23,ymm22,3
	valignq	ymm22,ymm22,ymm21,3
	valignq	ymm21,ymm21,ymm20,3
	valignq	ymm20,ymm20,ymm19,3
	valignq	ymm19,ymm19,ymm2,3
	valignq	ymm2,ymm2,ymm1,3
	valignq	ymm1,ymm1,ymm0,3
	valignq	ymm0,ymm0,YMMWORD[$L$zeros],3


	vpandq	ymm11,ymm11,YMMWORD[$L$mask52x4]
	vpandq	ymm12,ymm12,YMMWORD[$L$mask52x4]
	vpandq	ymm13,ymm13,YMMWORD[$L$mask52x4]
	vpandq	ymm14,ymm14,YMMWORD[$L$mask52x4]
	vpandq	ymm15,ymm15,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]
	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]


	vpaddq	ymm11,ymm11,ymm0
	vpaddq	ymm12,ymm12,ymm1
	vpaddq	ymm13,ymm13,ymm2
	vpaddq	ymm14,ymm14,ymm19
	vpaddq	ymm15,ymm15,ymm20
	vpaddq	ymm16,ymm16,ymm21
	vpaddq	ymm17,ymm17,ymm22
	vpaddq	ymm18,ymm18,ymm23



	vpcmpuq	k1,ymm11,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm12,YMMWORD[$L$mask52x4],6
	kmovb	r14d,k1
	kmovb	r13d,k2
	shl	r13b,4
	or	r14b,r13b

	vpcmpuq	k1,ymm13,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm14,YMMWORD[$L$mask52x4],6
	kmovb	r13d,k1
	kmovb	r12d,k2
	shl	r12b,4
	or	r13b,r12b

	vpcmpuq	k1,ymm15,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],6
	kmovb	r12d,k1
	kmovb	r11d,k2
	shl	r11b,4
	or	r12b,r11b

	vpcmpuq	k1,ymm17,YMMWORD[$L$mask52x4],6
	vpcmpuq	k2,ymm18,YMMWORD[$L$mask52x4],6
	kmovb	r11d,k1
	kmovb	r10d,k2
	shl	r10b,4
	or	r11b,r10b

	add	r14b,r14b
	adc	r13b,r13b
	adc	r12b,r12b
	adc	r11b,r11b


	vpcmpuq	k1,ymm11,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm12,YMMWORD[$L$mask52x4],0
	kmovb	r9d,k1
	kmovb	r8d,k2
	shl	r8b,4
	or	r9b,r8b

	vpcmpuq	k1,ymm13,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm14,YMMWORD[$L$mask52x4],0
	kmovb	r8d,k1
	kmovb	edx,k2
	shl	dl,4
	or	r8b,dl

	vpcmpuq	k1,ymm15,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm16,YMMWORD[$L$mask52x4],0
	kmovb	edx,k1
	kmovb	ecx,k2
	shl	cl,4
	or	dl,cl

	vpcmpuq	k1,ymm17,YMMWORD[$L$mask52x4],0
	vpcmpuq	k2,ymm18,YMMWORD[$L$mask52x4],0
	kmovb	ecx,k1
	kmovb	ebx,k2
	shl	bl,4
	or	cl,bl

	add	r14b,r9b
	adc	r13b,r8b
	adc	r12b,dl
	adc	r11b,cl

	xor	r14b,r9b
	xor	r13b,r8b
	xor	r12b,dl
	xor	r11b,cl

	kmovb	k1,r14d
	shr	r14b,4
	kmovb	k2,r14d
	kmovb	k3,r13d
	shr	r13b,4
	kmovb	k4,r13d
	kmovb	k5,r12d
	shr	r12b,4
	kmovb	k6,r12d
	kmovb	k7,r11d

	vpsubq	ymm11{k1},ymm11,YMMWORD[$L$mask52x4]
	vpsubq	ymm12{k2},ymm12,YMMWORD[$L$mask52x4]
	vpsubq	ymm13{k3},ymm13,YMMWORD[$L$mask52x4]
	vpsubq	ymm14{k4},ymm14,YMMWORD[$L$mask52x4]
	vpsubq	ymm15{k5},ymm15,YMMWORD[$L$mask52x4]
	vpsubq	ymm16{k6},ymm16,YMMWORD[$L$mask52x4]
	vpsubq	ymm17{k7},ymm17,YMMWORD[$L$mask52x4]

	vpandq	ymm11,ymm11,YMMWORD[$L$mask52x4]
	vpandq	ymm12,ymm12,YMMWORD[$L$mask52x4]
	vpandq	ymm13,ymm13,YMMWORD[$L$mask52x4]
	vpandq	ymm14,ymm14,YMMWORD[$L$mask52x4]
	vpandq	ymm15,ymm15,YMMWORD[$L$mask52x4]
	vpandq	ymm16,ymm16,YMMWORD[$L$mask52x4]
	vpandq	ymm17,ymm17,YMMWORD[$L$mask52x4]

	shr	r11b,4
	kmovb	k1,r11d

	vpsubq	ymm18{k1},ymm18,YMMWORD[$L$mask52x4]

	vpandq	ymm18,ymm18,YMMWORD[$L$mask52x4]

	vmovdqu64	YMMWORD[rdi],ymm3
	vmovdqu64	YMMWORD[32+rdi],ymm4
	vmovdqu64	YMMWORD[64+rdi],ymm5
	vmovdqu64	YMMWORD[96+rdi],ymm6
	vmovdqu64	YMMWORD[128+rdi],ymm7
	vmovdqu64	YMMWORD[160+rdi],ymm8
	vmovdqu64	YMMWORD[192+rdi],ymm9
	vmovdqu64	YMMWORD[224+rdi],ymm10

	vmovdqu64	YMMWORD[256+rdi],ymm11
	vmovdqu64	YMMWORD[288+rdi],ymm12
	vmovdqu64	YMMWORD[320+rdi],ymm13
	vmovdqu64	YMMWORD[352+rdi],ymm14
	vmovdqu64	YMMWORD[384+rdi],ymm15
	vmovdqu64	YMMWORD[416+rdi],ymm16
	vmovdqu64	YMMWORD[448+rdi],ymm17
	vmovdqu64	YMMWORD[480+rdi],ymm18

	vzeroupper
	lea	rax,[rsp]

	vmovdqa64	xmm6,XMMWORD[rax]
	vmovdqa64	xmm7,XMMWORD[16+rax]
	vmovdqa64	xmm8,XMMWORD[32+rax]
	vmovdqa64	xmm9,XMMWORD[48+rax]
	vmovdqa64	xmm10,XMMWORD[64+rax]
	vmovdqa64	xmm11,XMMWORD[80+rax]
	vmovdqa64	xmm12,XMMWORD[96+rax]
	vmovdqa64	xmm13,XMMWORD[112+rax]
	vmovdqa64	xmm14,XMMWORD[128+rax]
	vmovdqa64	xmm15,XMMWORD[144+rax]
	lea	rax,[168+rsp]
	mov	r15,QWORD[rax]

	mov	r14,QWORD[8+rax]

	mov	r13,QWORD[16+rax]

	mov	r12,QWORD[24+rax]

	mov	rbp,QWORD[32+rax]

	mov	rbx,QWORD[40+rax]

	lea	rsp,[48+rax]

$L$ossl_rsaz_amm52x30_x2_ifma256_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ossl_rsaz_amm52x30_x2_ifma256:
section	.text code align=64


ALIGN	32
global	ossl_extract_multiplier_2x30_win5

ossl_extract_multiplier_2x30_win5:

DB	243,15,30,250
	vmovdqa64	ymm30,YMMWORD[$L$ones]
	vpbroadcastq	ymm28,r8
	vpbroadcastq	ymm29,r9
	lea	rax,[16384+rdx]


	vpxor	xmm0,xmm0,xmm0
	vmovdqa64	ymm27,ymm0
	vmovdqa64	ymm1,ymm0
	vmovdqa64	ymm2,ymm0
	vmovdqa64	ymm3,ymm0
	vmovdqa64	ymm4,ymm0
	vmovdqa64	ymm5,ymm0
	vmovdqa64	ymm16,ymm0
	vmovdqa64	ymm17,ymm0
	vmovdqa64	ymm18,ymm0
	vmovdqa64	ymm19,ymm0
	vmovdqa64	ymm20,ymm0
	vmovdqa64	ymm21,ymm0
	vmovdqa64	ymm22,ymm0
	vmovdqa64	ymm23,ymm0
	vmovdqa64	ymm24,ymm0
	vmovdqa64	ymm25,ymm0

ALIGN	32
$L$loop:
	vpcmpq	k1,ymm28,ymm27,0
	vpcmpq	k2,ymm29,ymm27,0
	vmovdqu64	ymm26,YMMWORD[rdx]
	vpblendmq	ymm0{k1},ymm0,ymm26
	vmovdqu64	ymm26,YMMWORD[32+rdx]
	vpblendmq	ymm1{k1},ymm1,ymm26
	vmovdqu64	ymm26,YMMWORD[64+rdx]
	vpblendmq	ymm2{k1},ymm2,ymm26
	vmovdqu64	ymm26,YMMWORD[96+rdx]
	vpblendmq	ymm3{k1},ymm3,ymm26
	vmovdqu64	ymm26,YMMWORD[128+rdx]
	vpblendmq	ymm4{k1},ymm4,ymm26
	vmovdqu64	ymm26,YMMWORD[160+rdx]
	vpblendmq	ymm5{k1},ymm5,ymm26
	vmovdqu64	ymm26,YMMWORD[192+rdx]
	vpblendmq	ymm16{k1},ymm16,ymm26
	vmovdqu64	ymm26,YMMWORD[224+rdx]
	vpblendmq	ymm17{k1},ymm17,ymm26
	vmovdqu64	ymm26,YMMWORD[256+rdx]
	vpblendmq	ymm18{k2},ymm18,ymm26
	vmovdqu64	ymm26,YMMWORD[288+rdx]
	vpblendmq	ymm19{k2},ymm19,ymm26
	vmovdqu64	ymm26,YMMWORD[320+rdx]
	vpblendmq	ymm20{k2},ymm20,ymm26
	vmovdqu64	ymm26,YMMWORD[352+rdx]
	vpblendmq	ymm21{k2},ymm21,ymm26
	vmovdqu64	ymm26,YMMWORD[384+rdx]
	vpblendmq	ymm22{k2},ymm22,ymm26
	vmovdqu64	ymm26,YMMWORD[416+rdx]
	vpblendmq	ymm23{k2},ymm23,ymm26
	vmovdqu64	ymm26,YMMWORD[448+rdx]
	vpblendmq	ymm24{k2},ymm24,ymm26
	vmovdqu64	ymm26,YMMWORD[480+rdx]
	vpblendmq	ymm25{k2},ymm25,ymm26
	vpaddq	ymm27,ymm27,ymm30
	add	rdx,512
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
	vmovdqu64	YMMWORD[320+rcx],ymm20
	vmovdqu64	YMMWORD[352+rcx],ymm21
	vmovdqu64	YMMWORD[384+rcx],ymm22
	vmovdqu64	YMMWORD[416+rcx],ymm23
	vmovdqu64	YMMWORD[448+rcx],ymm24
	vmovdqu64	YMMWORD[480+rcx],ymm25

	DB	0F3h,0C3h		;repret


section	.rdata rdata align=32
ALIGN	32
$L$ones:
	DQ	1,1,1,1
$L$zeros:
	DQ	0,0,0,0
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
rsaz_avx_handler:
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

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$common_seh_tail

	mov	rax,QWORD[152+r8]

	lea	rsi,[rax]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

	lea	rax,[216+rax]

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
	DD	$L$SEH_begin_ossl_rsaz_amm52x30_x1_ifma256 wrt ..imagebase
	DD	$L$SEH_end_ossl_rsaz_amm52x30_x1_ifma256 wrt ..imagebase
	DD	$L$SEH_info_ossl_rsaz_amm52x30_x1_ifma256 wrt ..imagebase

	DD	$L$SEH_begin_ossl_rsaz_amm52x30_x2_ifma256 wrt ..imagebase
	DD	$L$SEH_end_ossl_rsaz_amm52x30_x2_ifma256 wrt ..imagebase
	DD	$L$SEH_info_ossl_rsaz_amm52x30_x2_ifma256 wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_ossl_rsaz_amm52x30_x1_ifma256:
DB	9,0,0,0
	DD	rsaz_avx_handler wrt ..imagebase
	DD	$L$ossl_rsaz_amm52x30_x1_ifma256_body wrt ..imagebase,$L$ossl_rsaz_amm52x30_x1_ifma256_epilogue wrt ..imagebase
$L$SEH_info_ossl_rsaz_amm52x30_x2_ifma256:
DB	9,0,0,0
	DD	rsaz_avx_handler wrt ..imagebase
	DD	$L$ossl_rsaz_amm52x30_x2_ifma256_body wrt ..imagebase,$L$ossl_rsaz_amm52x30_x2_ifma256_epilogue wrt ..imagebase
