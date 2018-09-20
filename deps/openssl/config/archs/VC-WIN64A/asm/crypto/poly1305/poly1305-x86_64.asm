default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

global	poly1305_init

global	poly1305_blocks

global	poly1305_emit



ALIGN	32
poly1305_init:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_init:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	xor	rax,rax
	mov	QWORD[rdi],rax
	mov	QWORD[8+rdi],rax
	mov	QWORD[16+rdi],rax

	cmp	rsi,0
	je	NEAR $L$no_key

	lea	r10,[poly1305_blocks]
	lea	r11,[poly1305_emit]
	mov	r9,QWORD[((OPENSSL_ia32cap_P+4))]
	lea	rax,[poly1305_blocks_avx]
	lea	rcx,[poly1305_emit_avx]
	bt	r9,28
	cmovc	r10,rax
	cmovc	r11,rcx
	lea	rax,[poly1305_blocks_avx2]
	bt	r9,37
	cmovc	r10,rax
	mov	rax,0x0ffffffc0fffffff
	mov	rcx,0x0ffffffc0ffffffc
	and	rax,QWORD[rsi]
	and	rcx,QWORD[8+rsi]
	mov	QWORD[24+rdi],rax
	mov	QWORD[32+rdi],rcx
	mov	QWORD[rdx],r10
	mov	QWORD[8+rdx],r11
	mov	eax,1
$L$no_key:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_init:


ALIGN	32
poly1305_blocks:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_blocks:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


$L$blocks:
	shr	rdx,4
	jz	NEAR $L$no_data

	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$blocks_body:

	mov	r15,rdx

	mov	r11,QWORD[24+rdi]
	mov	r13,QWORD[32+rdi]

	mov	r14,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	rbp,QWORD[16+rdi]

	mov	r12,r13
	shr	r13,2
	mov	rax,r12
	add	r13,r12
	jmp	NEAR $L$oop

ALIGN	32
$L$oop:
	add	r14,QWORD[rsi]
	adc	rbx,QWORD[8+rsi]
	lea	rsi,[16+rsi]
	adc	rbp,rcx
	mul	r14
	mov	r9,rax
	mov	rax,r11
	mov	r10,rdx

	mul	r14
	mov	r14,rax
	mov	rax,r11
	mov	r8,rdx

	mul	rbx
	add	r9,rax
	mov	rax,r13
	adc	r10,rdx

	mul	rbx
	mov	rbx,rbp
	add	r14,rax
	adc	r8,rdx

	imul	rbx,r13
	add	r9,rbx
	mov	rbx,r8
	adc	r10,0

	imul	rbp,r11
	add	rbx,r9
	mov	rax,-4
	adc	r10,rbp

	and	rax,r10
	mov	rbp,r10
	shr	r10,2
	and	rbp,3
	add	rax,r10
	add	r14,rax
	adc	rbx,0
	adc	rbp,0
	mov	rax,r12
	dec	r15
	jnz	NEAR $L$oop

	mov	QWORD[rdi],r14
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rbp

	mov	r15,QWORD[rsp]
	mov	r14,QWORD[8+rsp]
	mov	r13,QWORD[16+rsp]
	mov	r12,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]
	mov	rbx,QWORD[40+rsp]
	lea	rsp,[48+rsp]
$L$no_data:
$L$blocks_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_blocks:


ALIGN	32
poly1305_emit:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_emit:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


$L$emit:
	mov	r8,QWORD[rdi]
	mov	r9,QWORD[8+rdi]
	mov	r10,QWORD[16+rdi]

	mov	rax,r8
	add	r8,5
	mov	rcx,r9
	adc	r9,0
	adc	r10,0
	shr	r10,2
	cmovnz	rax,r8
	cmovnz	rcx,r9

	add	rax,QWORD[rdx]
	adc	rcx,QWORD[8+rdx]
	mov	QWORD[rsi],rax
	mov	QWORD[8+rsi],rcx

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_emit:

ALIGN	32
__poly1305_block:
	mul	r14
	mov	r9,rax
	mov	rax,r11
	mov	r10,rdx

	mul	r14
	mov	r14,rax
	mov	rax,r11
	mov	r8,rdx

	mul	rbx
	add	r9,rax
	mov	rax,r13
	adc	r10,rdx

	mul	rbx
	mov	rbx,rbp
	add	r14,rax
	adc	r8,rdx

	imul	rbx,r13
	add	r9,rbx
	mov	rbx,r8
	adc	r10,0

	imul	rbp,r11
	add	rbx,r9
	mov	rax,-4
	adc	r10,rbp

	and	rax,r10
	mov	rbp,r10
	shr	r10,2
	and	rbp,3
	add	rax,r10
	add	r14,rax
	adc	rbx,0
	adc	rbp,0
	DB	0F3h,0C3h		;repret



ALIGN	32
__poly1305_init_avx:
	mov	r14,r11
	mov	rbx,r12
	xor	rbp,rbp

	lea	rdi,[((48+64))+rdi]

	mov	rax,r12
	call	__poly1305_block

	mov	eax,0x3ffffff
	mov	edx,0x3ffffff
	mov	r8,r14
	and	eax,r14d
	mov	r9,r11
	and	edx,r11d
	mov	DWORD[((-64))+rdi],eax
	shr	r8,26
	mov	DWORD[((-60))+rdi],edx
	shr	r9,26

	mov	eax,0x3ffffff
	mov	edx,0x3ffffff
	and	eax,r8d
	and	edx,r9d
	mov	DWORD[((-48))+rdi],eax
	lea	eax,[rax*4+rax]
	mov	DWORD[((-44))+rdi],edx
	lea	edx,[rdx*4+rdx]
	mov	DWORD[((-32))+rdi],eax
	shr	r8,26
	mov	DWORD[((-28))+rdi],edx
	shr	r9,26

	mov	rax,rbx
	mov	rdx,r12
	shl	rax,12
	shl	rdx,12
	or	rax,r8
	or	rdx,r9
	and	eax,0x3ffffff
	and	edx,0x3ffffff
	mov	DWORD[((-16))+rdi],eax
	lea	eax,[rax*4+rax]
	mov	DWORD[((-12))+rdi],edx
	lea	edx,[rdx*4+rdx]
	mov	DWORD[rdi],eax
	mov	r8,rbx
	mov	DWORD[4+rdi],edx
	mov	r9,r12

	mov	eax,0x3ffffff
	mov	edx,0x3ffffff
	shr	r8,14
	shr	r9,14
	and	eax,r8d
	and	edx,r9d
	mov	DWORD[16+rdi],eax
	lea	eax,[rax*4+rax]
	mov	DWORD[20+rdi],edx
	lea	edx,[rdx*4+rdx]
	mov	DWORD[32+rdi],eax
	shr	r8,26
	mov	DWORD[36+rdi],edx
	shr	r9,26

	mov	rax,rbp
	shl	rax,24
	or	r8,rax
	mov	DWORD[48+rdi],r8d
	lea	r8,[r8*4+r8]
	mov	DWORD[52+rdi],r9d
	lea	r9,[r9*4+r9]
	mov	DWORD[64+rdi],r8d
	mov	DWORD[68+rdi],r9d

	mov	rax,r12
	call	__poly1305_block

	mov	eax,0x3ffffff
	mov	r8,r14
	and	eax,r14d
	shr	r8,26
	mov	DWORD[((-52))+rdi],eax

	mov	edx,0x3ffffff
	and	edx,r8d
	mov	DWORD[((-36))+rdi],edx
	lea	edx,[rdx*4+rdx]
	shr	r8,26
	mov	DWORD[((-20))+rdi],edx

	mov	rax,rbx
	shl	rax,12
	or	rax,r8
	and	eax,0x3ffffff
	mov	DWORD[((-4))+rdi],eax
	lea	eax,[rax*4+rax]
	mov	r8,rbx
	mov	DWORD[12+rdi],eax

	mov	edx,0x3ffffff
	shr	r8,14
	and	edx,r8d
	mov	DWORD[28+rdi],edx
	lea	edx,[rdx*4+rdx]
	shr	r8,26
	mov	DWORD[44+rdi],edx

	mov	rax,rbp
	shl	rax,24
	or	r8,rax
	mov	DWORD[60+rdi],r8d
	lea	r8,[r8*4+r8]
	mov	DWORD[76+rdi],r8d

	mov	rax,r12
	call	__poly1305_block

	mov	eax,0x3ffffff
	mov	r8,r14
	and	eax,r14d
	shr	r8,26
	mov	DWORD[((-56))+rdi],eax

	mov	edx,0x3ffffff
	and	edx,r8d
	mov	DWORD[((-40))+rdi],edx
	lea	edx,[rdx*4+rdx]
	shr	r8,26
	mov	DWORD[((-24))+rdi],edx

	mov	rax,rbx
	shl	rax,12
	or	rax,r8
	and	eax,0x3ffffff
	mov	DWORD[((-8))+rdi],eax
	lea	eax,[rax*4+rax]
	mov	r8,rbx
	mov	DWORD[8+rdi],eax

	mov	edx,0x3ffffff
	shr	r8,14
	and	edx,r8d
	mov	DWORD[24+rdi],edx
	lea	edx,[rdx*4+rdx]
	shr	r8,26
	mov	DWORD[40+rdi],edx

	mov	rax,rbp
	shl	rax,24
	or	r8,rax
	mov	DWORD[56+rdi],r8d
	lea	r8,[r8*4+r8]
	mov	DWORD[72+rdi],r8d

	lea	rdi,[((-48-64))+rdi]
	DB	0F3h,0C3h		;repret



ALIGN	32
poly1305_blocks_avx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_blocks_avx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	mov	r8d,DWORD[20+rdi]
	cmp	rdx,128
	jae	NEAR $L$blocks_avx
	test	r8d,r8d
	jz	NEAR $L$blocks

$L$blocks_avx:
	and	rdx,-16
	jz	NEAR $L$no_data_avx

	vzeroupper

	test	r8d,r8d
	jz	NEAR $L$base2_64_avx

	test	rdx,31
	jz	NEAR $L$even_avx

	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$blocks_avx_body:

	mov	r15,rdx

	mov	r8,QWORD[rdi]
	mov	r9,QWORD[8+rdi]
	mov	ebp,DWORD[16+rdi]

	mov	r11,QWORD[24+rdi]
	mov	r13,QWORD[32+rdi]


	mov	r14d,r8d
	and	r8,-2147483648
	mov	r12,r9
	mov	ebx,r9d
	and	r9,-2147483648

	shr	r8,6
	shl	r12,52
	add	r14,r8
	shr	rbx,12
	shr	r9,18
	add	r14,r12
	adc	rbx,r9

	mov	r8,rbp
	shl	r8,40
	shr	rbp,24
	add	rbx,r8
	adc	rbp,0

	mov	r9,-4
	mov	r8,rbp
	and	r9,rbp
	shr	r8,2
	and	rbp,3
	add	r8,r9
	add	r14,r8
	adc	rbx,0
	adc	rbp,0

	mov	r12,r13
	mov	rax,r13
	shr	r13,2
	add	r13,r12

	add	r14,QWORD[rsi]
	adc	rbx,QWORD[8+rsi]
	lea	rsi,[16+rsi]
	adc	rbp,rcx

	call	__poly1305_block

	test	rcx,rcx
	jz	NEAR $L$store_base2_64_avx


	mov	rax,r14
	mov	rdx,r14
	shr	r14,52
	mov	r11,rbx
	mov	r12,rbx
	shr	rdx,26
	and	rax,0x3ffffff
	shl	r11,12
	and	rdx,0x3ffffff
	shr	rbx,14
	or	r14,r11
	shl	rbp,24
	and	r14,0x3ffffff
	shr	r12,40
	and	rbx,0x3ffffff
	or	rbp,r12

	sub	r15,16
	jz	NEAR $L$store_base2_26_avx

	vmovd	xmm0,eax
	vmovd	xmm1,edx
	vmovd	xmm2,r14d
	vmovd	xmm3,ebx
	vmovd	xmm4,ebp
	jmp	NEAR $L$proceed_avx

ALIGN	32
$L$store_base2_64_avx:
	mov	QWORD[rdi],r14
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rbp
	jmp	NEAR $L$done_avx

ALIGN	16
$L$store_base2_26_avx:
	mov	DWORD[rdi],eax
	mov	DWORD[4+rdi],edx
	mov	DWORD[8+rdi],r14d
	mov	DWORD[12+rdi],ebx
	mov	DWORD[16+rdi],ebp
ALIGN	16
$L$done_avx:
	mov	r15,QWORD[rsp]
	mov	r14,QWORD[8+rsp]
	mov	r13,QWORD[16+rsp]
	mov	r12,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]
	mov	rbx,QWORD[40+rsp]
	lea	rsp,[48+rsp]
$L$no_data_avx:
$L$blocks_avx_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

ALIGN	32
$L$base2_64_avx:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$base2_64_avx_body:

	mov	r15,rdx

	mov	r11,QWORD[24+rdi]
	mov	r13,QWORD[32+rdi]

	mov	r14,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	ebp,DWORD[16+rdi]

	mov	r12,r13
	mov	rax,r13
	shr	r13,2
	add	r13,r12

	test	rdx,31
	jz	NEAR $L$init_avx

	add	r14,QWORD[rsi]
	adc	rbx,QWORD[8+rsi]
	lea	rsi,[16+rsi]
	adc	rbp,rcx
	sub	r15,16

	call	__poly1305_block

$L$init_avx:

	mov	rax,r14
	mov	rdx,r14
	shr	r14,52
	mov	r8,rbx
	mov	r9,rbx
	shr	rdx,26
	and	rax,0x3ffffff
	shl	r8,12
	and	rdx,0x3ffffff
	shr	rbx,14
	or	r14,r8
	shl	rbp,24
	and	r14,0x3ffffff
	shr	r9,40
	and	rbx,0x3ffffff
	or	rbp,r9

	vmovd	xmm0,eax
	vmovd	xmm1,edx
	vmovd	xmm2,r14d
	vmovd	xmm3,ebx
	vmovd	xmm4,ebp
	mov	DWORD[20+rdi],1

	call	__poly1305_init_avx

$L$proceed_avx:
	mov	rdx,r15

	mov	r15,QWORD[rsp]
	mov	r14,QWORD[8+rsp]
	mov	r13,QWORD[16+rsp]
	mov	r12,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]
	mov	rbx,QWORD[40+rsp]
	lea	rax,[48+rsp]
	lea	rsp,[48+rsp]
$L$base2_64_avx_epilogue:
	jmp	NEAR $L$do_avx

ALIGN	32
$L$even_avx:
	vmovd	xmm0,DWORD[rdi]
	vmovd	xmm1,DWORD[4+rdi]
	vmovd	xmm2,DWORD[8+rdi]
	vmovd	xmm3,DWORD[12+rdi]
	vmovd	xmm4,DWORD[16+rdi]

$L$do_avx:
	lea	r11,[((-248))+rsp]
	sub	rsp,0x218
	vmovdqa	XMMWORD[80+r11],xmm6
	vmovdqa	XMMWORD[96+r11],xmm7
	vmovdqa	XMMWORD[112+r11],xmm8
	vmovdqa	XMMWORD[128+r11],xmm9
	vmovdqa	XMMWORD[144+r11],xmm10
	vmovdqa	XMMWORD[160+r11],xmm11
	vmovdqa	XMMWORD[176+r11],xmm12
	vmovdqa	XMMWORD[192+r11],xmm13
	vmovdqa	XMMWORD[208+r11],xmm14
	vmovdqa	XMMWORD[224+r11],xmm15
$L$do_avx_body:
	sub	rdx,64
	lea	rax,[((-32))+rsi]
	cmovc	rsi,rax

	vmovdqu	xmm14,XMMWORD[48+rdi]
	lea	rdi,[112+rdi]
	lea	rcx,[$L$const]



	vmovdqu	xmm5,XMMWORD[32+rsi]
	vmovdqu	xmm6,XMMWORD[48+rsi]
	vmovdqa	xmm15,XMMWORD[64+rcx]

	vpsrldq	xmm7,xmm5,6
	vpsrldq	xmm8,xmm6,6
	vpunpckhqdq	xmm9,xmm5,xmm6
	vpunpcklqdq	xmm5,xmm5,xmm6
	vpunpcklqdq	xmm8,xmm7,xmm8

	vpsrlq	xmm9,xmm9,40
	vpsrlq	xmm6,xmm5,26
	vpand	xmm5,xmm5,xmm15
	vpsrlq	xmm7,xmm8,4
	vpand	xmm6,xmm6,xmm15
	vpsrlq	xmm8,xmm8,30
	vpand	xmm7,xmm7,xmm15
	vpand	xmm8,xmm8,xmm15
	vpor	xmm9,xmm9,XMMWORD[32+rcx]

	jbe	NEAR $L$skip_loop_avx


	vmovdqu	xmm11,XMMWORD[((-48))+rdi]
	vmovdqu	xmm12,XMMWORD[((-32))+rdi]
	vpshufd	xmm13,xmm14,0xEE
	vpshufd	xmm10,xmm14,0x44
	vmovdqa	XMMWORD[(-144)+r11],xmm13
	vmovdqa	XMMWORD[rsp],xmm10
	vpshufd	xmm14,xmm11,0xEE
	vmovdqu	xmm10,XMMWORD[((-16))+rdi]
	vpshufd	xmm11,xmm11,0x44
	vmovdqa	XMMWORD[(-128)+r11],xmm14
	vmovdqa	XMMWORD[16+rsp],xmm11
	vpshufd	xmm13,xmm12,0xEE
	vmovdqu	xmm11,XMMWORD[rdi]
	vpshufd	xmm12,xmm12,0x44
	vmovdqa	XMMWORD[(-112)+r11],xmm13
	vmovdqa	XMMWORD[32+rsp],xmm12
	vpshufd	xmm14,xmm10,0xEE
	vmovdqu	xmm12,XMMWORD[16+rdi]
	vpshufd	xmm10,xmm10,0x44
	vmovdqa	XMMWORD[(-96)+r11],xmm14
	vmovdqa	XMMWORD[48+rsp],xmm10
	vpshufd	xmm13,xmm11,0xEE
	vmovdqu	xmm10,XMMWORD[32+rdi]
	vpshufd	xmm11,xmm11,0x44
	vmovdqa	XMMWORD[(-80)+r11],xmm13
	vmovdqa	XMMWORD[64+rsp],xmm11
	vpshufd	xmm14,xmm12,0xEE
	vmovdqu	xmm11,XMMWORD[48+rdi]
	vpshufd	xmm12,xmm12,0x44
	vmovdqa	XMMWORD[(-64)+r11],xmm14
	vmovdqa	XMMWORD[80+rsp],xmm12
	vpshufd	xmm13,xmm10,0xEE
	vmovdqu	xmm12,XMMWORD[64+rdi]
	vpshufd	xmm10,xmm10,0x44
	vmovdqa	XMMWORD[(-48)+r11],xmm13
	vmovdqa	XMMWORD[96+rsp],xmm10
	vpshufd	xmm14,xmm11,0xEE
	vpshufd	xmm11,xmm11,0x44
	vmovdqa	XMMWORD[(-32)+r11],xmm14
	vmovdqa	XMMWORD[112+rsp],xmm11
	vpshufd	xmm13,xmm12,0xEE
	vmovdqa	xmm14,XMMWORD[rsp]
	vpshufd	xmm12,xmm12,0x44
	vmovdqa	XMMWORD[(-16)+r11],xmm13
	vmovdqa	XMMWORD[128+rsp],xmm12

	jmp	NEAR $L$oop_avx

ALIGN	32
$L$oop_avx:




















	vpmuludq	xmm10,xmm14,xmm5
	vpmuludq	xmm11,xmm14,xmm6
	vmovdqa	XMMWORD[32+r11],xmm2
	vpmuludq	xmm12,xmm14,xmm7
	vmovdqa	xmm2,XMMWORD[16+rsp]
	vpmuludq	xmm13,xmm14,xmm8
	vpmuludq	xmm14,xmm14,xmm9

	vmovdqa	XMMWORD[r11],xmm0
	vpmuludq	xmm0,xmm9,XMMWORD[32+rsp]
	vmovdqa	XMMWORD[16+r11],xmm1
	vpmuludq	xmm1,xmm2,xmm8
	vpaddq	xmm10,xmm10,xmm0
	vpaddq	xmm14,xmm14,xmm1
	vmovdqa	XMMWORD[48+r11],xmm3
	vpmuludq	xmm0,xmm2,xmm7
	vpmuludq	xmm1,xmm2,xmm6
	vpaddq	xmm13,xmm13,xmm0
	vmovdqa	xmm3,XMMWORD[48+rsp]
	vpaddq	xmm12,xmm12,xmm1
	vmovdqa	XMMWORD[64+r11],xmm4
	vpmuludq	xmm2,xmm2,xmm5
	vpmuludq	xmm0,xmm3,xmm7
	vpaddq	xmm11,xmm11,xmm2

	vmovdqa	xmm4,XMMWORD[64+rsp]
	vpaddq	xmm14,xmm14,xmm0
	vpmuludq	xmm1,xmm3,xmm6
	vpmuludq	xmm3,xmm3,xmm5
	vpaddq	xmm13,xmm13,xmm1
	vmovdqa	xmm2,XMMWORD[80+rsp]
	vpaddq	xmm12,xmm12,xmm3
	vpmuludq	xmm0,xmm4,xmm9
	vpmuludq	xmm4,xmm4,xmm8
	vpaddq	xmm11,xmm11,xmm0
	vmovdqa	xmm3,XMMWORD[96+rsp]
	vpaddq	xmm10,xmm10,xmm4

	vmovdqa	xmm4,XMMWORD[128+rsp]
	vpmuludq	xmm1,xmm2,xmm6
	vpmuludq	xmm2,xmm2,xmm5
	vpaddq	xmm14,xmm14,xmm1
	vpaddq	xmm13,xmm13,xmm2
	vpmuludq	xmm0,xmm3,xmm9
	vpmuludq	xmm1,xmm3,xmm8
	vpaddq	xmm12,xmm12,xmm0
	vmovdqu	xmm0,XMMWORD[rsi]
	vpaddq	xmm11,xmm11,xmm1
	vpmuludq	xmm3,xmm3,xmm7
	vpmuludq	xmm7,xmm4,xmm7
	vpaddq	xmm10,xmm10,xmm3

	vmovdqu	xmm1,XMMWORD[16+rsi]
	vpaddq	xmm11,xmm11,xmm7
	vpmuludq	xmm8,xmm4,xmm8
	vpmuludq	xmm9,xmm4,xmm9
	vpsrldq	xmm2,xmm0,6
	vpaddq	xmm12,xmm12,xmm8
	vpaddq	xmm13,xmm13,xmm9
	vpsrldq	xmm3,xmm1,6
	vpmuludq	xmm9,xmm5,XMMWORD[112+rsp]
	vpmuludq	xmm5,xmm4,xmm6
	vpunpckhqdq	xmm4,xmm0,xmm1
	vpaddq	xmm14,xmm14,xmm9
	vmovdqa	xmm9,XMMWORD[((-144))+r11]
	vpaddq	xmm10,xmm10,xmm5

	vpunpcklqdq	xmm0,xmm0,xmm1
	vpunpcklqdq	xmm3,xmm2,xmm3


	vpsrldq	xmm4,xmm4,5
	vpsrlq	xmm1,xmm0,26
	vpand	xmm0,xmm0,xmm15
	vpsrlq	xmm2,xmm3,4
	vpand	xmm1,xmm1,xmm15
	vpand	xmm4,xmm4,XMMWORD[rcx]
	vpsrlq	xmm3,xmm3,30
	vpand	xmm2,xmm2,xmm15
	vpand	xmm3,xmm3,xmm15
	vpor	xmm4,xmm4,XMMWORD[32+rcx]

	vpaddq	xmm0,xmm0,XMMWORD[r11]
	vpaddq	xmm1,xmm1,XMMWORD[16+r11]
	vpaddq	xmm2,xmm2,XMMWORD[32+r11]
	vpaddq	xmm3,xmm3,XMMWORD[48+r11]
	vpaddq	xmm4,xmm4,XMMWORD[64+r11]

	lea	rax,[32+rsi]
	lea	rsi,[64+rsi]
	sub	rdx,64
	cmovc	rsi,rax










	vpmuludq	xmm5,xmm9,xmm0
	vpmuludq	xmm6,xmm9,xmm1
	vpaddq	xmm10,xmm10,xmm5
	vpaddq	xmm11,xmm11,xmm6
	vmovdqa	xmm7,XMMWORD[((-128))+r11]
	vpmuludq	xmm5,xmm9,xmm2
	vpmuludq	xmm6,xmm9,xmm3
	vpaddq	xmm12,xmm12,xmm5
	vpaddq	xmm13,xmm13,xmm6
	vpmuludq	xmm9,xmm9,xmm4
	vpmuludq	xmm5,xmm4,XMMWORD[((-112))+r11]
	vpaddq	xmm14,xmm14,xmm9

	vpaddq	xmm10,xmm10,xmm5
	vpmuludq	xmm6,xmm7,xmm2
	vpmuludq	xmm5,xmm7,xmm3
	vpaddq	xmm13,xmm13,xmm6
	vmovdqa	xmm8,XMMWORD[((-96))+r11]
	vpaddq	xmm14,xmm14,xmm5
	vpmuludq	xmm6,xmm7,xmm1
	vpmuludq	xmm7,xmm7,xmm0
	vpaddq	xmm12,xmm12,xmm6
	vpaddq	xmm11,xmm11,xmm7

	vmovdqa	xmm9,XMMWORD[((-80))+r11]
	vpmuludq	xmm5,xmm8,xmm2
	vpmuludq	xmm6,xmm8,xmm1
	vpaddq	xmm14,xmm14,xmm5
	vpaddq	xmm13,xmm13,xmm6
	vmovdqa	xmm7,XMMWORD[((-64))+r11]
	vpmuludq	xmm8,xmm8,xmm0
	vpmuludq	xmm5,xmm9,xmm4
	vpaddq	xmm12,xmm12,xmm8
	vpaddq	xmm11,xmm11,xmm5
	vmovdqa	xmm8,XMMWORD[((-48))+r11]
	vpmuludq	xmm9,xmm9,xmm3
	vpmuludq	xmm6,xmm7,xmm1
	vpaddq	xmm10,xmm10,xmm9

	vmovdqa	xmm9,XMMWORD[((-16))+r11]
	vpaddq	xmm14,xmm14,xmm6
	vpmuludq	xmm7,xmm7,xmm0
	vpmuludq	xmm5,xmm8,xmm4
	vpaddq	xmm13,xmm13,xmm7
	vpaddq	xmm12,xmm12,xmm5
	vmovdqu	xmm5,XMMWORD[32+rsi]
	vpmuludq	xmm7,xmm8,xmm3
	vpmuludq	xmm8,xmm8,xmm2
	vpaddq	xmm11,xmm11,xmm7
	vmovdqu	xmm6,XMMWORD[48+rsi]
	vpaddq	xmm10,xmm10,xmm8

	vpmuludq	xmm2,xmm9,xmm2
	vpmuludq	xmm3,xmm9,xmm3
	vpsrldq	xmm7,xmm5,6
	vpaddq	xmm11,xmm11,xmm2
	vpmuludq	xmm4,xmm9,xmm4
	vpsrldq	xmm8,xmm6,6
	vpaddq	xmm2,xmm12,xmm3
	vpaddq	xmm3,xmm13,xmm4
	vpmuludq	xmm4,xmm0,XMMWORD[((-32))+r11]
	vpmuludq	xmm0,xmm9,xmm1
	vpunpckhqdq	xmm9,xmm5,xmm6
	vpaddq	xmm4,xmm14,xmm4
	vpaddq	xmm0,xmm10,xmm0

	vpunpcklqdq	xmm5,xmm5,xmm6
	vpunpcklqdq	xmm8,xmm7,xmm8


	vpsrldq	xmm9,xmm9,5
	vpsrlq	xmm6,xmm5,26
	vmovdqa	xmm14,XMMWORD[rsp]
	vpand	xmm5,xmm5,xmm15
	vpsrlq	xmm7,xmm8,4
	vpand	xmm6,xmm6,xmm15
	vpand	xmm9,xmm9,XMMWORD[rcx]
	vpsrlq	xmm8,xmm8,30
	vpand	xmm7,xmm7,xmm15
	vpand	xmm8,xmm8,xmm15
	vpor	xmm9,xmm9,XMMWORD[32+rcx]





	vpsrlq	xmm13,xmm3,26
	vpand	xmm3,xmm3,xmm15
	vpaddq	xmm4,xmm4,xmm13

	vpsrlq	xmm10,xmm0,26
	vpand	xmm0,xmm0,xmm15
	vpaddq	xmm1,xmm11,xmm10

	vpsrlq	xmm10,xmm4,26
	vpand	xmm4,xmm4,xmm15

	vpsrlq	xmm11,xmm1,26
	vpand	xmm1,xmm1,xmm15
	vpaddq	xmm2,xmm2,xmm11

	vpaddq	xmm0,xmm0,xmm10
	vpsllq	xmm10,xmm10,2
	vpaddq	xmm0,xmm0,xmm10

	vpsrlq	xmm12,xmm2,26
	vpand	xmm2,xmm2,xmm15
	vpaddq	xmm3,xmm3,xmm12

	vpsrlq	xmm10,xmm0,26
	vpand	xmm0,xmm0,xmm15
	vpaddq	xmm1,xmm1,xmm10

	vpsrlq	xmm13,xmm3,26
	vpand	xmm3,xmm3,xmm15
	vpaddq	xmm4,xmm4,xmm13

	ja	NEAR $L$oop_avx

$L$skip_loop_avx:



	vpshufd	xmm14,xmm14,0x10
	add	rdx,32
	jnz	NEAR $L$ong_tail_avx

	vpaddq	xmm7,xmm7,xmm2
	vpaddq	xmm5,xmm5,xmm0
	vpaddq	xmm6,xmm6,xmm1
	vpaddq	xmm8,xmm8,xmm3
	vpaddq	xmm9,xmm9,xmm4

$L$ong_tail_avx:
	vmovdqa	XMMWORD[32+r11],xmm2
	vmovdqa	XMMWORD[r11],xmm0
	vmovdqa	XMMWORD[16+r11],xmm1
	vmovdqa	XMMWORD[48+r11],xmm3
	vmovdqa	XMMWORD[64+r11],xmm4







	vpmuludq	xmm12,xmm14,xmm7
	vpmuludq	xmm10,xmm14,xmm5
	vpshufd	xmm2,XMMWORD[((-48))+rdi],0x10
	vpmuludq	xmm11,xmm14,xmm6
	vpmuludq	xmm13,xmm14,xmm8
	vpmuludq	xmm14,xmm14,xmm9

	vpmuludq	xmm0,xmm2,xmm8
	vpaddq	xmm14,xmm14,xmm0
	vpshufd	xmm3,XMMWORD[((-32))+rdi],0x10
	vpmuludq	xmm1,xmm2,xmm7
	vpaddq	xmm13,xmm13,xmm1
	vpshufd	xmm4,XMMWORD[((-16))+rdi],0x10
	vpmuludq	xmm0,xmm2,xmm6
	vpaddq	xmm12,xmm12,xmm0
	vpmuludq	xmm2,xmm2,xmm5
	vpaddq	xmm11,xmm11,xmm2
	vpmuludq	xmm3,xmm3,xmm9
	vpaddq	xmm10,xmm10,xmm3

	vpshufd	xmm2,XMMWORD[rdi],0x10
	vpmuludq	xmm1,xmm4,xmm7
	vpaddq	xmm14,xmm14,xmm1
	vpmuludq	xmm0,xmm4,xmm6
	vpaddq	xmm13,xmm13,xmm0
	vpshufd	xmm3,XMMWORD[16+rdi],0x10
	vpmuludq	xmm4,xmm4,xmm5
	vpaddq	xmm12,xmm12,xmm4
	vpmuludq	xmm1,xmm2,xmm9
	vpaddq	xmm11,xmm11,xmm1
	vpshufd	xmm4,XMMWORD[32+rdi],0x10
	vpmuludq	xmm2,xmm2,xmm8
	vpaddq	xmm10,xmm10,xmm2

	vpmuludq	xmm0,xmm3,xmm6
	vpaddq	xmm14,xmm14,xmm0
	vpmuludq	xmm3,xmm3,xmm5
	vpaddq	xmm13,xmm13,xmm3
	vpshufd	xmm2,XMMWORD[48+rdi],0x10
	vpmuludq	xmm1,xmm4,xmm9
	vpaddq	xmm12,xmm12,xmm1
	vpshufd	xmm3,XMMWORD[64+rdi],0x10
	vpmuludq	xmm0,xmm4,xmm8
	vpaddq	xmm11,xmm11,xmm0
	vpmuludq	xmm4,xmm4,xmm7
	vpaddq	xmm10,xmm10,xmm4

	vpmuludq	xmm2,xmm2,xmm5
	vpaddq	xmm14,xmm14,xmm2
	vpmuludq	xmm1,xmm3,xmm9
	vpaddq	xmm13,xmm13,xmm1
	vpmuludq	xmm0,xmm3,xmm8
	vpaddq	xmm12,xmm12,xmm0
	vpmuludq	xmm1,xmm3,xmm7
	vpaddq	xmm11,xmm11,xmm1
	vpmuludq	xmm3,xmm3,xmm6
	vpaddq	xmm10,xmm10,xmm3

	jz	NEAR $L$short_tail_avx

	vmovdqu	xmm0,XMMWORD[rsi]
	vmovdqu	xmm1,XMMWORD[16+rsi]

	vpsrldq	xmm2,xmm0,6
	vpsrldq	xmm3,xmm1,6
	vpunpckhqdq	xmm4,xmm0,xmm1
	vpunpcklqdq	xmm0,xmm0,xmm1
	vpunpcklqdq	xmm3,xmm2,xmm3

	vpsrlq	xmm4,xmm4,40
	vpsrlq	xmm1,xmm0,26
	vpand	xmm0,xmm0,xmm15
	vpsrlq	xmm2,xmm3,4
	vpand	xmm1,xmm1,xmm15
	vpsrlq	xmm3,xmm3,30
	vpand	xmm2,xmm2,xmm15
	vpand	xmm3,xmm3,xmm15
	vpor	xmm4,xmm4,XMMWORD[32+rcx]

	vpshufd	xmm9,XMMWORD[((-64))+rdi],0x32
	vpaddq	xmm0,xmm0,XMMWORD[r11]
	vpaddq	xmm1,xmm1,XMMWORD[16+r11]
	vpaddq	xmm2,xmm2,XMMWORD[32+r11]
	vpaddq	xmm3,xmm3,XMMWORD[48+r11]
	vpaddq	xmm4,xmm4,XMMWORD[64+r11]




	vpmuludq	xmm5,xmm9,xmm0
	vpaddq	xmm10,xmm10,xmm5
	vpmuludq	xmm6,xmm9,xmm1
	vpaddq	xmm11,xmm11,xmm6
	vpmuludq	xmm5,xmm9,xmm2
	vpaddq	xmm12,xmm12,xmm5
	vpshufd	xmm7,XMMWORD[((-48))+rdi],0x32
	vpmuludq	xmm6,xmm9,xmm3
	vpaddq	xmm13,xmm13,xmm6
	vpmuludq	xmm9,xmm9,xmm4
	vpaddq	xmm14,xmm14,xmm9

	vpmuludq	xmm5,xmm7,xmm3
	vpaddq	xmm14,xmm14,xmm5
	vpshufd	xmm8,XMMWORD[((-32))+rdi],0x32
	vpmuludq	xmm6,xmm7,xmm2
	vpaddq	xmm13,xmm13,xmm6
	vpshufd	xmm9,XMMWORD[((-16))+rdi],0x32
	vpmuludq	xmm5,xmm7,xmm1
	vpaddq	xmm12,xmm12,xmm5
	vpmuludq	xmm7,xmm7,xmm0
	vpaddq	xmm11,xmm11,xmm7
	vpmuludq	xmm8,xmm8,xmm4
	vpaddq	xmm10,xmm10,xmm8

	vpshufd	xmm7,XMMWORD[rdi],0x32
	vpmuludq	xmm6,xmm9,xmm2
	vpaddq	xmm14,xmm14,xmm6
	vpmuludq	xmm5,xmm9,xmm1
	vpaddq	xmm13,xmm13,xmm5
	vpshufd	xmm8,XMMWORD[16+rdi],0x32
	vpmuludq	xmm9,xmm9,xmm0
	vpaddq	xmm12,xmm12,xmm9
	vpmuludq	xmm6,xmm7,xmm4
	vpaddq	xmm11,xmm11,xmm6
	vpshufd	xmm9,XMMWORD[32+rdi],0x32
	vpmuludq	xmm7,xmm7,xmm3
	vpaddq	xmm10,xmm10,xmm7

	vpmuludq	xmm5,xmm8,xmm1
	vpaddq	xmm14,xmm14,xmm5
	vpmuludq	xmm8,xmm8,xmm0
	vpaddq	xmm13,xmm13,xmm8
	vpshufd	xmm7,XMMWORD[48+rdi],0x32
	vpmuludq	xmm6,xmm9,xmm4
	vpaddq	xmm12,xmm12,xmm6
	vpshufd	xmm8,XMMWORD[64+rdi],0x32
	vpmuludq	xmm5,xmm9,xmm3
	vpaddq	xmm11,xmm11,xmm5
	vpmuludq	xmm9,xmm9,xmm2
	vpaddq	xmm10,xmm10,xmm9

	vpmuludq	xmm7,xmm7,xmm0
	vpaddq	xmm14,xmm14,xmm7
	vpmuludq	xmm6,xmm8,xmm4
	vpaddq	xmm13,xmm13,xmm6
	vpmuludq	xmm5,xmm8,xmm3
	vpaddq	xmm12,xmm12,xmm5
	vpmuludq	xmm6,xmm8,xmm2
	vpaddq	xmm11,xmm11,xmm6
	vpmuludq	xmm8,xmm8,xmm1
	vpaddq	xmm10,xmm10,xmm8

$L$short_tail_avx:



	vpsrldq	xmm9,xmm14,8
	vpsrldq	xmm8,xmm13,8
	vpsrldq	xmm6,xmm11,8
	vpsrldq	xmm5,xmm10,8
	vpsrldq	xmm7,xmm12,8
	vpaddq	xmm13,xmm13,xmm8
	vpaddq	xmm14,xmm14,xmm9
	vpaddq	xmm10,xmm10,xmm5
	vpaddq	xmm11,xmm11,xmm6
	vpaddq	xmm12,xmm12,xmm7




	vpsrlq	xmm3,xmm13,26
	vpand	xmm13,xmm13,xmm15
	vpaddq	xmm14,xmm14,xmm3

	vpsrlq	xmm0,xmm10,26
	vpand	xmm10,xmm10,xmm15
	vpaddq	xmm11,xmm11,xmm0

	vpsrlq	xmm4,xmm14,26
	vpand	xmm14,xmm14,xmm15

	vpsrlq	xmm1,xmm11,26
	vpand	xmm11,xmm11,xmm15
	vpaddq	xmm12,xmm12,xmm1

	vpaddq	xmm10,xmm10,xmm4
	vpsllq	xmm4,xmm4,2
	vpaddq	xmm10,xmm10,xmm4

	vpsrlq	xmm2,xmm12,26
	vpand	xmm12,xmm12,xmm15
	vpaddq	xmm13,xmm13,xmm2

	vpsrlq	xmm0,xmm10,26
	vpand	xmm10,xmm10,xmm15
	vpaddq	xmm11,xmm11,xmm0

	vpsrlq	xmm3,xmm13,26
	vpand	xmm13,xmm13,xmm15
	vpaddq	xmm14,xmm14,xmm3

	vmovd	DWORD[(-112)+rdi],xmm10
	vmovd	DWORD[(-108)+rdi],xmm11
	vmovd	DWORD[(-104)+rdi],xmm12
	vmovd	DWORD[(-100)+rdi],xmm13
	vmovd	DWORD[(-96)+rdi],xmm14
	vmovdqa	xmm6,XMMWORD[80+r11]
	vmovdqa	xmm7,XMMWORD[96+r11]
	vmovdqa	xmm8,XMMWORD[112+r11]
	vmovdqa	xmm9,XMMWORD[128+r11]
	vmovdqa	xmm10,XMMWORD[144+r11]
	vmovdqa	xmm11,XMMWORD[160+r11]
	vmovdqa	xmm12,XMMWORD[176+r11]
	vmovdqa	xmm13,XMMWORD[192+r11]
	vmovdqa	xmm14,XMMWORD[208+r11]
	vmovdqa	xmm15,XMMWORD[224+r11]
	lea	rsp,[248+r11]
$L$do_avx_epilogue:
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_blocks_avx:


ALIGN	32
poly1305_emit_avx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_emit_avx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	cmp	DWORD[20+rdi],0
	je	NEAR $L$emit

	mov	eax,DWORD[rdi]
	mov	ecx,DWORD[4+rdi]
	mov	r8d,DWORD[8+rdi]
	mov	r11d,DWORD[12+rdi]
	mov	r10d,DWORD[16+rdi]

	shl	rcx,26
	mov	r9,r8
	shl	r8,52
	add	rax,rcx
	shr	r9,12
	add	r8,rax
	adc	r9,0

	shl	r11,14
	mov	rax,r10
	shr	r10,24
	add	r9,r11
	shl	rax,40
	add	r9,rax
	adc	r10,0

	mov	rax,r10
	mov	rcx,r10
	and	r10,3
	shr	rax,2
	and	rcx,-4
	add	rax,rcx
	add	r8,rax
	adc	r9,0
	adc	r10,0

	mov	rax,r8
	add	r8,5
	mov	rcx,r9
	adc	r9,0
	adc	r10,0
	shr	r10,2
	cmovnz	rax,r8
	cmovnz	rcx,r9

	add	rax,QWORD[rdx]
	adc	rcx,QWORD[8+rdx]
	mov	QWORD[rsi],rax
	mov	QWORD[8+rsi],rcx

	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_emit_avx:

ALIGN	32
poly1305_blocks_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_poly1305_blocks_avx2:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	mov	r8d,DWORD[20+rdi]
	cmp	rdx,128
	jae	NEAR $L$blocks_avx2
	test	r8d,r8d
	jz	NEAR $L$blocks

$L$blocks_avx2:
	and	rdx,-16
	jz	NEAR $L$no_data_avx2

	vzeroupper

	test	r8d,r8d
	jz	NEAR $L$base2_64_avx2

	test	rdx,63
	jz	NEAR $L$even_avx2

	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$blocks_avx2_body:

	mov	r15,rdx

	mov	r8,QWORD[rdi]
	mov	r9,QWORD[8+rdi]
	mov	ebp,DWORD[16+rdi]

	mov	r11,QWORD[24+rdi]
	mov	r13,QWORD[32+rdi]


	mov	r14d,r8d
	and	r8,-2147483648
	mov	r12,r9
	mov	ebx,r9d
	and	r9,-2147483648

	shr	r8,6
	shl	r12,52
	add	r14,r8
	shr	rbx,12
	shr	r9,18
	add	r14,r12
	adc	rbx,r9

	mov	r8,rbp
	shl	r8,40
	shr	rbp,24
	add	rbx,r8
	adc	rbp,0

	mov	r9,-4
	mov	r8,rbp
	and	r9,rbp
	shr	r8,2
	and	rbp,3
	add	r8,r9
	add	r14,r8
	adc	rbx,0
	adc	rbp,0

	mov	r12,r13
	mov	rax,r13
	shr	r13,2
	add	r13,r12

$L$base2_26_pre_avx2:
	add	r14,QWORD[rsi]
	adc	rbx,QWORD[8+rsi]
	lea	rsi,[16+rsi]
	adc	rbp,rcx
	sub	r15,16

	call	__poly1305_block
	mov	rax,r12

	test	r15,63
	jnz	NEAR $L$base2_26_pre_avx2

	test	rcx,rcx
	jz	NEAR $L$store_base2_64_avx2


	mov	rax,r14
	mov	rdx,r14
	shr	r14,52
	mov	r11,rbx
	mov	r12,rbx
	shr	rdx,26
	and	rax,0x3ffffff
	shl	r11,12
	and	rdx,0x3ffffff
	shr	rbx,14
	or	r14,r11
	shl	rbp,24
	and	r14,0x3ffffff
	shr	r12,40
	and	rbx,0x3ffffff
	or	rbp,r12

	test	r15,r15
	jz	NEAR $L$store_base2_26_avx2

	vmovd	xmm0,eax
	vmovd	xmm1,edx
	vmovd	xmm2,r14d
	vmovd	xmm3,ebx
	vmovd	xmm4,ebp
	jmp	NEAR $L$proceed_avx2

ALIGN	32
$L$store_base2_64_avx2:
	mov	QWORD[rdi],r14
	mov	QWORD[8+rdi],rbx
	mov	QWORD[16+rdi],rbp
	jmp	NEAR $L$done_avx2

ALIGN	16
$L$store_base2_26_avx2:
	mov	DWORD[rdi],eax
	mov	DWORD[4+rdi],edx
	mov	DWORD[8+rdi],r14d
	mov	DWORD[12+rdi],ebx
	mov	DWORD[16+rdi],ebp
ALIGN	16
$L$done_avx2:
	mov	r15,QWORD[rsp]
	mov	r14,QWORD[8+rsp]
	mov	r13,QWORD[16+rsp]
	mov	r12,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]
	mov	rbx,QWORD[40+rsp]
	lea	rsp,[48+rsp]
$L$no_data_avx2:
$L$blocks_avx2_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

ALIGN	32
$L$base2_64_avx2:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$base2_64_avx2_body:

	mov	r15,rdx

	mov	r11,QWORD[24+rdi]
	mov	r13,QWORD[32+rdi]

	mov	r14,QWORD[rdi]
	mov	rbx,QWORD[8+rdi]
	mov	ebp,DWORD[16+rdi]

	mov	r12,r13
	mov	rax,r13
	shr	r13,2
	add	r13,r12

	test	rdx,63
	jz	NEAR $L$init_avx2

$L$base2_64_pre_avx2:
	add	r14,QWORD[rsi]
	adc	rbx,QWORD[8+rsi]
	lea	rsi,[16+rsi]
	adc	rbp,rcx
	sub	r15,16

	call	__poly1305_block
	mov	rax,r12

	test	r15,63
	jnz	NEAR $L$base2_64_pre_avx2

$L$init_avx2:

	mov	rax,r14
	mov	rdx,r14
	shr	r14,52
	mov	r8,rbx
	mov	r9,rbx
	shr	rdx,26
	and	rax,0x3ffffff
	shl	r8,12
	and	rdx,0x3ffffff
	shr	rbx,14
	or	r14,r8
	shl	rbp,24
	and	r14,0x3ffffff
	shr	r9,40
	and	rbx,0x3ffffff
	or	rbp,r9

	vmovd	xmm0,eax
	vmovd	xmm1,edx
	vmovd	xmm2,r14d
	vmovd	xmm3,ebx
	vmovd	xmm4,ebp
	mov	DWORD[20+rdi],1

	call	__poly1305_init_avx

$L$proceed_avx2:
	mov	rdx,r15

	mov	r15,QWORD[rsp]
	mov	r14,QWORD[8+rsp]
	mov	r13,QWORD[16+rsp]
	mov	r12,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]
	mov	rbx,QWORD[40+rsp]
	lea	rax,[48+rsp]
	lea	rsp,[48+rsp]
$L$base2_64_avx2_epilogue:
	jmp	NEAR $L$do_avx2

ALIGN	32
$L$even_avx2:
	vmovd	xmm0,DWORD[rdi]
	vmovd	xmm1,DWORD[4+rdi]
	vmovd	xmm2,DWORD[8+rdi]
	vmovd	xmm3,DWORD[12+rdi]
	vmovd	xmm4,DWORD[16+rdi]

$L$do_avx2:
	lea	r11,[((-248))+rsp]
	sub	rsp,0x1c8
	vmovdqa	XMMWORD[80+r11],xmm6
	vmovdqa	XMMWORD[96+r11],xmm7
	vmovdqa	XMMWORD[112+r11],xmm8
	vmovdqa	XMMWORD[128+r11],xmm9
	vmovdqa	XMMWORD[144+r11],xmm10
	vmovdqa	XMMWORD[160+r11],xmm11
	vmovdqa	XMMWORD[176+r11],xmm12
	vmovdqa	XMMWORD[192+r11],xmm13
	vmovdqa	XMMWORD[208+r11],xmm14
	vmovdqa	XMMWORD[224+r11],xmm15
$L$do_avx2_body:
	lea	rdi,[((48+64))+rdi]
	lea	rcx,[$L$const]


	vmovdqu	xmm9,XMMWORD[((-64))+rdi]
	and	rsp,-512
	vmovdqu	xmm10,XMMWORD[((-48))+rdi]
	vmovdqu	xmm6,XMMWORD[((-32))+rdi]
	vmovdqu	xmm11,XMMWORD[((-16))+rdi]
	vmovdqu	xmm12,XMMWORD[rdi]
	vmovdqu	xmm13,XMMWORD[16+rdi]
	vmovdqu	xmm14,XMMWORD[32+rdi]
	vpermq	ymm9,ymm9,0x15
	vmovdqu	xmm15,XMMWORD[48+rdi]
	vpermq	ymm10,ymm10,0x15
	vpshufd	ymm9,ymm9,0xc8
	vmovdqu	xmm5,XMMWORD[64+rdi]
	vpermq	ymm6,ymm6,0x15
	vpshufd	ymm10,ymm10,0xc8
	vmovdqa	YMMWORD[rsp],ymm9
	vpermq	ymm11,ymm11,0x15
	vpshufd	ymm6,ymm6,0xc8
	vmovdqa	YMMWORD[32+rsp],ymm10
	vpermq	ymm12,ymm12,0x15
	vpshufd	ymm11,ymm11,0xc8
	vmovdqa	YMMWORD[64+rsp],ymm6
	vpermq	ymm13,ymm13,0x15
	vpshufd	ymm12,ymm12,0xc8
	vmovdqa	YMMWORD[96+rsp],ymm11
	vpermq	ymm14,ymm14,0x15
	vpshufd	ymm13,ymm13,0xc8
	vmovdqa	YMMWORD[128+rsp],ymm12
	vpermq	ymm15,ymm15,0x15
	vpshufd	ymm14,ymm14,0xc8
	vmovdqa	YMMWORD[160+rsp],ymm13
	vpermq	ymm5,ymm5,0x15
	vpshufd	ymm15,ymm15,0xc8
	vmovdqa	YMMWORD[192+rsp],ymm14
	vpshufd	ymm5,ymm5,0xc8
	vmovdqa	YMMWORD[224+rsp],ymm15
	vmovdqa	YMMWORD[256+rsp],ymm5
	vmovdqa	ymm5,YMMWORD[64+rcx]



	vmovdqu	xmm7,XMMWORD[rsi]
	vmovdqu	xmm8,XMMWORD[16+rsi]
	vinserti128	ymm7,ymm7,XMMWORD[32+rsi],1
	vinserti128	ymm8,ymm8,XMMWORD[48+rsi],1
	lea	rsi,[64+rsi]

	vpsrldq	ymm9,ymm7,6
	vpsrldq	ymm10,ymm8,6
	vpunpckhqdq	ymm6,ymm7,ymm8
	vpunpcklqdq	ymm9,ymm9,ymm10
	vpunpcklqdq	ymm7,ymm7,ymm8

	vpsrlq	ymm10,ymm9,30
	vpsrlq	ymm9,ymm9,4
	vpsrlq	ymm8,ymm7,26
	vpsrlq	ymm6,ymm6,40
	vpand	ymm9,ymm9,ymm5
	vpand	ymm7,ymm7,ymm5
	vpand	ymm8,ymm8,ymm5
	vpand	ymm10,ymm10,ymm5
	vpor	ymm6,ymm6,YMMWORD[32+rcx]

	lea	rax,[144+rsp]
	vpaddq	ymm2,ymm9,ymm2
	sub	rdx,64
	jz	NEAR $L$tail_avx2
	jmp	NEAR $L$oop_avx2

ALIGN	32
$L$oop_avx2:








	vpaddq	ymm0,ymm7,ymm0
	vmovdqa	ymm7,YMMWORD[rsp]
	vpaddq	ymm1,ymm8,ymm1
	vmovdqa	ymm8,YMMWORD[32+rsp]
	vpaddq	ymm3,ymm10,ymm3
	vmovdqa	ymm9,YMMWORD[96+rsp]
	vpaddq	ymm4,ymm6,ymm4
	vmovdqa	ymm10,YMMWORD[48+rax]
	vmovdqa	ymm5,YMMWORD[112+rax]
















	vpmuludq	ymm13,ymm7,ymm2
	vpmuludq	ymm14,ymm8,ymm2
	vpmuludq	ymm15,ymm9,ymm2
	vpmuludq	ymm11,ymm10,ymm2
	vpmuludq	ymm12,ymm5,ymm2

	vpmuludq	ymm6,ymm8,ymm0
	vpmuludq	ymm2,ymm8,ymm1
	vpaddq	ymm12,ymm12,ymm6
	vpaddq	ymm13,ymm13,ymm2
	vpmuludq	ymm6,ymm8,ymm3
	vpmuludq	ymm2,ymm4,YMMWORD[64+rsp]
	vpaddq	ymm15,ymm15,ymm6
	vpaddq	ymm11,ymm11,ymm2
	vmovdqa	ymm8,YMMWORD[((-16))+rax]

	vpmuludq	ymm6,ymm7,ymm0
	vpmuludq	ymm2,ymm7,ymm1
	vpaddq	ymm11,ymm11,ymm6
	vpaddq	ymm12,ymm12,ymm2
	vpmuludq	ymm6,ymm7,ymm3
	vpmuludq	ymm2,ymm7,ymm4
	vmovdqu	xmm7,XMMWORD[rsi]
	vpaddq	ymm14,ymm14,ymm6
	vpaddq	ymm15,ymm15,ymm2
	vinserti128	ymm7,ymm7,XMMWORD[32+rsi],1

	vpmuludq	ymm6,ymm8,ymm3
	vpmuludq	ymm2,ymm8,ymm4
	vmovdqu	xmm8,XMMWORD[16+rsi]
	vpaddq	ymm11,ymm11,ymm6
	vpaddq	ymm12,ymm12,ymm2
	vmovdqa	ymm2,YMMWORD[16+rax]
	vpmuludq	ymm6,ymm9,ymm1
	vpmuludq	ymm9,ymm9,ymm0
	vpaddq	ymm14,ymm14,ymm6
	vpaddq	ymm13,ymm13,ymm9
	vinserti128	ymm8,ymm8,XMMWORD[48+rsi],1
	lea	rsi,[64+rsi]

	vpmuludq	ymm6,ymm2,ymm1
	vpmuludq	ymm2,ymm2,ymm0
	vpsrldq	ymm9,ymm7,6
	vpaddq	ymm15,ymm15,ymm6
	vpaddq	ymm14,ymm14,ymm2
	vpmuludq	ymm6,ymm10,ymm3
	vpmuludq	ymm2,ymm10,ymm4
	vpsrldq	ymm10,ymm8,6
	vpaddq	ymm12,ymm12,ymm6
	vpaddq	ymm13,ymm13,ymm2
	vpunpckhqdq	ymm6,ymm7,ymm8

	vpmuludq	ymm3,ymm5,ymm3
	vpmuludq	ymm4,ymm5,ymm4
	vpunpcklqdq	ymm7,ymm7,ymm8
	vpaddq	ymm2,ymm13,ymm3
	vpaddq	ymm3,ymm14,ymm4
	vpunpcklqdq	ymm10,ymm9,ymm10
	vpmuludq	ymm4,ymm0,YMMWORD[80+rax]
	vpmuludq	ymm0,ymm5,ymm1
	vmovdqa	ymm5,YMMWORD[64+rcx]
	vpaddq	ymm4,ymm15,ymm4
	vpaddq	ymm0,ymm11,ymm0




	vpsrlq	ymm14,ymm3,26
	vpand	ymm3,ymm3,ymm5
	vpaddq	ymm4,ymm4,ymm14

	vpsrlq	ymm11,ymm0,26
	vpand	ymm0,ymm0,ymm5
	vpaddq	ymm1,ymm12,ymm11

	vpsrlq	ymm15,ymm4,26
	vpand	ymm4,ymm4,ymm5

	vpsrlq	ymm9,ymm10,4

	vpsrlq	ymm12,ymm1,26
	vpand	ymm1,ymm1,ymm5
	vpaddq	ymm2,ymm2,ymm12

	vpaddq	ymm0,ymm0,ymm15
	vpsllq	ymm15,ymm15,2
	vpaddq	ymm0,ymm0,ymm15

	vpand	ymm9,ymm9,ymm5
	vpsrlq	ymm8,ymm7,26

	vpsrlq	ymm13,ymm2,26
	vpand	ymm2,ymm2,ymm5
	vpaddq	ymm3,ymm3,ymm13

	vpaddq	ymm2,ymm2,ymm9
	vpsrlq	ymm10,ymm10,30

	vpsrlq	ymm11,ymm0,26
	vpand	ymm0,ymm0,ymm5
	vpaddq	ymm1,ymm1,ymm11

	vpsrlq	ymm6,ymm6,40

	vpsrlq	ymm14,ymm3,26
	vpand	ymm3,ymm3,ymm5
	vpaddq	ymm4,ymm4,ymm14

	vpand	ymm7,ymm7,ymm5
	vpand	ymm8,ymm8,ymm5
	vpand	ymm10,ymm10,ymm5
	vpor	ymm6,ymm6,YMMWORD[32+rcx]

	sub	rdx,64
	jnz	NEAR $L$oop_avx2

DB	0x66,0x90
$L$tail_avx2:







	vpaddq	ymm0,ymm7,ymm0
	vmovdqu	ymm7,YMMWORD[4+rsp]
	vpaddq	ymm1,ymm8,ymm1
	vmovdqu	ymm8,YMMWORD[36+rsp]
	vpaddq	ymm3,ymm10,ymm3
	vmovdqu	ymm9,YMMWORD[100+rsp]
	vpaddq	ymm4,ymm6,ymm4
	vmovdqu	ymm10,YMMWORD[52+rax]
	vmovdqu	ymm5,YMMWORD[116+rax]

	vpmuludq	ymm13,ymm7,ymm2
	vpmuludq	ymm14,ymm8,ymm2
	vpmuludq	ymm15,ymm9,ymm2
	vpmuludq	ymm11,ymm10,ymm2
	vpmuludq	ymm12,ymm5,ymm2

	vpmuludq	ymm6,ymm8,ymm0
	vpmuludq	ymm2,ymm8,ymm1
	vpaddq	ymm12,ymm12,ymm6
	vpaddq	ymm13,ymm13,ymm2
	vpmuludq	ymm6,ymm8,ymm3
	vpmuludq	ymm2,ymm4,YMMWORD[68+rsp]
	vpaddq	ymm15,ymm15,ymm6
	vpaddq	ymm11,ymm11,ymm2

	vpmuludq	ymm6,ymm7,ymm0
	vpmuludq	ymm2,ymm7,ymm1
	vpaddq	ymm11,ymm11,ymm6
	vmovdqu	ymm8,YMMWORD[((-12))+rax]
	vpaddq	ymm12,ymm12,ymm2
	vpmuludq	ymm6,ymm7,ymm3
	vpmuludq	ymm2,ymm7,ymm4
	vpaddq	ymm14,ymm14,ymm6
	vpaddq	ymm15,ymm15,ymm2

	vpmuludq	ymm6,ymm8,ymm3
	vpmuludq	ymm2,ymm8,ymm4
	vpaddq	ymm11,ymm11,ymm6
	vpaddq	ymm12,ymm12,ymm2
	vmovdqu	ymm2,YMMWORD[20+rax]
	vpmuludq	ymm6,ymm9,ymm1
	vpmuludq	ymm9,ymm9,ymm0
	vpaddq	ymm14,ymm14,ymm6
	vpaddq	ymm13,ymm13,ymm9

	vpmuludq	ymm6,ymm2,ymm1
	vpmuludq	ymm2,ymm2,ymm0
	vpaddq	ymm15,ymm15,ymm6
	vpaddq	ymm14,ymm14,ymm2
	vpmuludq	ymm6,ymm10,ymm3
	vpmuludq	ymm2,ymm10,ymm4
	vpaddq	ymm12,ymm12,ymm6
	vpaddq	ymm13,ymm13,ymm2

	vpmuludq	ymm3,ymm5,ymm3
	vpmuludq	ymm4,ymm5,ymm4
	vpaddq	ymm2,ymm13,ymm3
	vpaddq	ymm3,ymm14,ymm4
	vpmuludq	ymm4,ymm0,YMMWORD[84+rax]
	vpmuludq	ymm0,ymm5,ymm1
	vmovdqa	ymm5,YMMWORD[64+rcx]
	vpaddq	ymm4,ymm15,ymm4
	vpaddq	ymm0,ymm11,ymm0




	vpsrldq	ymm8,ymm12,8
	vpsrldq	ymm9,ymm2,8
	vpsrldq	ymm10,ymm3,8
	vpsrldq	ymm6,ymm4,8
	vpsrldq	ymm7,ymm0,8
	vpaddq	ymm12,ymm12,ymm8
	vpaddq	ymm2,ymm2,ymm9
	vpaddq	ymm3,ymm3,ymm10
	vpaddq	ymm4,ymm4,ymm6
	vpaddq	ymm0,ymm0,ymm7

	vpermq	ymm10,ymm3,0x2
	vpermq	ymm6,ymm4,0x2
	vpermq	ymm7,ymm0,0x2
	vpermq	ymm8,ymm12,0x2
	vpermq	ymm9,ymm2,0x2
	vpaddq	ymm3,ymm3,ymm10
	vpaddq	ymm4,ymm4,ymm6
	vpaddq	ymm0,ymm0,ymm7
	vpaddq	ymm12,ymm12,ymm8
	vpaddq	ymm2,ymm2,ymm9




	vpsrlq	ymm14,ymm3,26
	vpand	ymm3,ymm3,ymm5
	vpaddq	ymm4,ymm4,ymm14

	vpsrlq	ymm11,ymm0,26
	vpand	ymm0,ymm0,ymm5
	vpaddq	ymm1,ymm12,ymm11

	vpsrlq	ymm15,ymm4,26
	vpand	ymm4,ymm4,ymm5

	vpsrlq	ymm12,ymm1,26
	vpand	ymm1,ymm1,ymm5
	vpaddq	ymm2,ymm2,ymm12

	vpaddq	ymm0,ymm0,ymm15
	vpsllq	ymm15,ymm15,2
	vpaddq	ymm0,ymm0,ymm15

	vpsrlq	ymm13,ymm2,26
	vpand	ymm2,ymm2,ymm5
	vpaddq	ymm3,ymm3,ymm13

	vpsrlq	ymm11,ymm0,26
	vpand	ymm0,ymm0,ymm5
	vpaddq	ymm1,ymm1,ymm11

	vpsrlq	ymm14,ymm3,26
	vpand	ymm3,ymm3,ymm5
	vpaddq	ymm4,ymm4,ymm14

	vmovd	DWORD[(-112)+rdi],xmm0
	vmovd	DWORD[(-108)+rdi],xmm1
	vmovd	DWORD[(-104)+rdi],xmm2
	vmovd	DWORD[(-100)+rdi],xmm3
	vmovd	DWORD[(-96)+rdi],xmm4
	vmovdqa	xmm6,XMMWORD[80+r11]
	vmovdqa	xmm7,XMMWORD[96+r11]
	vmovdqa	xmm8,XMMWORD[112+r11]
	vmovdqa	xmm9,XMMWORD[128+r11]
	vmovdqa	xmm10,XMMWORD[144+r11]
	vmovdqa	xmm11,XMMWORD[160+r11]
	vmovdqa	xmm12,XMMWORD[176+r11]
	vmovdqa	xmm13,XMMWORD[192+r11]
	vmovdqa	xmm14,XMMWORD[208+r11]
	vmovdqa	xmm15,XMMWORD[224+r11]
	lea	rsp,[248+r11]
$L$do_avx2_epilogue:
	vzeroupper
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_poly1305_blocks_avx2:
ALIGN	64
$L$const:
$L$mask24:
	DD	0x0ffffff,0,0x0ffffff,0,0x0ffffff,0,0x0ffffff,0
$L$129:
	DD	16777216,0,16777216,0,16777216,0,16777216,0
$L$mask26:
	DD	0x3ffffff,0,0x3ffffff,0,0x3ffffff,0,0x3ffffff,0
$L$five:
	DD	5,0,5,0,5,0,5,0
DB	80,111,108,121,49,51,48,53,32,102,111,114,32,120,56,54
DB	95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32
DB	98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115
DB	108,46,111,114,103,62,0
ALIGN	16
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
se_handler:
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

	jmp	NEAR $L$common_seh_tail



ALIGN	16
avx_handler:
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

	mov	rax,QWORD[208+r8]

	lea	rsi,[80+rax]
	lea	rax,[248+rax]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

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
	DD	$L$SEH_begin_poly1305_init wrt ..imagebase
	DD	$L$SEH_end_poly1305_init wrt ..imagebase
	DD	$L$SEH_info_poly1305_init wrt ..imagebase

	DD	$L$SEH_begin_poly1305_blocks wrt ..imagebase
	DD	$L$SEH_end_poly1305_blocks wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks wrt ..imagebase

	DD	$L$SEH_begin_poly1305_emit wrt ..imagebase
	DD	$L$SEH_end_poly1305_emit wrt ..imagebase
	DD	$L$SEH_info_poly1305_emit wrt ..imagebase
	DD	$L$SEH_begin_poly1305_blocks_avx wrt ..imagebase
	DD	$L$base2_64_avx wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx_1 wrt ..imagebase

	DD	$L$base2_64_avx wrt ..imagebase
	DD	$L$even_avx wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx_2 wrt ..imagebase

	DD	$L$even_avx wrt ..imagebase
	DD	$L$SEH_end_poly1305_blocks_avx wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx_3 wrt ..imagebase

	DD	$L$SEH_begin_poly1305_emit_avx wrt ..imagebase
	DD	$L$SEH_end_poly1305_emit_avx wrt ..imagebase
	DD	$L$SEH_info_poly1305_emit_avx wrt ..imagebase
	DD	$L$SEH_begin_poly1305_blocks_avx2 wrt ..imagebase
	DD	$L$base2_64_avx2 wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx2_1 wrt ..imagebase

	DD	$L$base2_64_avx2 wrt ..imagebase
	DD	$L$even_avx2 wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx2_2 wrt ..imagebase

	DD	$L$even_avx2 wrt ..imagebase
	DD	$L$SEH_end_poly1305_blocks_avx2 wrt ..imagebase
	DD	$L$SEH_info_poly1305_blocks_avx2_3 wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_poly1305_init:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$SEH_begin_poly1305_init wrt ..imagebase,$L$SEH_begin_poly1305_init wrt ..imagebase

$L$SEH_info_poly1305_blocks:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$blocks_body wrt ..imagebase,$L$blocks_epilogue wrt ..imagebase

$L$SEH_info_poly1305_emit:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$SEH_begin_poly1305_emit wrt ..imagebase,$L$SEH_begin_poly1305_emit wrt ..imagebase
$L$SEH_info_poly1305_blocks_avx_1:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$blocks_avx_body wrt ..imagebase,$L$blocks_avx_epilogue wrt ..imagebase

$L$SEH_info_poly1305_blocks_avx_2:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$base2_64_avx_body wrt ..imagebase,$L$base2_64_avx_epilogue wrt ..imagebase

$L$SEH_info_poly1305_blocks_avx_3:
DB	9,0,0,0
	DD	avx_handler wrt ..imagebase
	DD	$L$do_avx_body wrt ..imagebase,$L$do_avx_epilogue wrt ..imagebase

$L$SEH_info_poly1305_emit_avx:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$SEH_begin_poly1305_emit_avx wrt ..imagebase,$L$SEH_begin_poly1305_emit_avx wrt ..imagebase
$L$SEH_info_poly1305_blocks_avx2_1:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$blocks_avx2_body wrt ..imagebase,$L$blocks_avx2_epilogue wrt ..imagebase

$L$SEH_info_poly1305_blocks_avx2_2:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$base2_64_avx2_body wrt ..imagebase,$L$base2_64_avx2_epilogue wrt ..imagebase

$L$SEH_info_poly1305_blocks_avx2_3:
DB	9,0,0,0
	DD	avx_handler wrt ..imagebase
	DD	$L$do_avx2_body wrt ..imagebase,$L$do_avx2_epilogue wrt ..imagebase
