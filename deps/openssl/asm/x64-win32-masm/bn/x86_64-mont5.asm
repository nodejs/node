OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

PUBLIC	bn_mul_mont_gather5

ALIGN	64
bn_mul_mont_gather5	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul_mont_gather5::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	test	r9d,3
	jnz	$L$mul_enter
	cmp	r9d,8
	jb	$L$mul_enter
	jmp	$L$mul4x_enter

ALIGN	16
$L$mul_enter::
	mov	r9d,r9d
	mov	r10d,DWORD PTR[56+rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-40))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
$L$mul_alloca::
	mov	rax,rsp
	lea	r11,QWORD PTR[2+r9]
	neg	r11
	lea	rsp,QWORD PTR[r11*8+rsp]
	and	rsp,-1024

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul_body::
	mov	r12,rdx
	mov	r11,r10
	shr	r10,3
	and	r11,7
	not	r10
	lea	rax,QWORD PTR[$L$magic_masks]
	and	r10,3
	lea	r12,QWORD PTR[96+r11*8+r12]
	movq	xmm4,QWORD PTR[r10*8+rax]
	movq	xmm5,QWORD PTR[8+r10*8+rax]
	movq	xmm6,QWORD PTR[16+r10*8+rax]
	movq	xmm7,QWORD PTR[24+r10*8+rax]

	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5
	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7
	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

DB	102,72,15,126,195

	mov	r8,QWORD PTR[r8]
	mov	rax,QWORD PTR[rsi]

	xor	r14,r14
	xor	r15,r15

	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5

	mov	rbp,r8
	mul	rbx
	mov	r10,rax
	mov	rax,QWORD PTR[rcx]

	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7

	imul	rbp,r10
	mov	r11,rdx

	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+rsi]
	adc	rdx,0
	mov	r13,rdx

	lea	r15,QWORD PTR[1+r15]
	jmp	$L$1st_enter

ALIGN	16
$L$1st::
	add	r13,rax
	mov	rax,QWORD PTR[r15*8+rsi]
	adc	rdx,0
	add	r13,r11
	mov	r11,r10
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],r13
	mov	r13,rdx

$L$1st_enter::
	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	adc	rdx,0
	lea	r15,QWORD PTR[1+r15]
	mov	r10,rdx

	mul	rbp
	cmp	r15,r9
	jne	$L$1st

DB	102,72,15,126,195

	add	r13,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	r13,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],r13
	mov	r13,rdx
	mov	r11,r10

	xor	rdx,rdx
	add	r13,r11
	adc	rdx,0
	mov	QWORD PTR[((-8))+r9*8+rsp],r13
	mov	QWORD PTR[r9*8+rsp],rdx

	lea	r14,QWORD PTR[1+r14]
	jmp	$L$outer
ALIGN	16
$L$outer::
	xor	r15,r15
	mov	rbp,r8
	mov	r10,QWORD PTR[rsp]

	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0

	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7

	imul	rbp,r10
	mov	r11,rdx

	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+rsi]
	adc	rdx,0
	mov	r10,QWORD PTR[8+rsp]
	mov	r13,rdx

	lea	r15,QWORD PTR[1+r15]
	jmp	$L$inner_enter

ALIGN	16
$L$inner::
	add	r13,rax
	mov	rax,QWORD PTR[r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	mov	r10,QWORD PTR[r15*8+rsp]
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],r13
	mov	r13,rdx

$L$inner_enter::
	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	adc	rdx,0
	add	r10,r11
	mov	r11,rdx
	adc	r11,0
	lea	r15,QWORD PTR[1+r15]

	mul	rbp
	cmp	r15,r9
	jne	$L$inner

DB	102,72,15,126,195

	add	r13,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	r13,r10
	mov	r10,QWORD PTR[r15*8+rsp]
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],r13
	mov	r13,rdx

	xor	rdx,rdx
	add	r13,r11
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-8))+r9*8+rsp],r13
	mov	QWORD PTR[r9*8+rsp],rdx

	lea	r14,QWORD PTR[1+r14]
	cmp	r14,r9
	jl	$L$outer

	xor	r14,r14
	mov	rax,QWORD PTR[rsp]
	lea	rsi,QWORD PTR[rsp]
	mov	r15,r9
	jmp	$L$sub
ALIGN	16
$L$sub::	sbb	rax,QWORD PTR[r14*8+rcx]
	mov	QWORD PTR[r14*8+rdi],rax
	mov	rax,QWORD PTR[8+r14*8+rsi]
	lea	r14,QWORD PTR[1+r14]
	dec	r15
	jnz	$L$sub

	sbb	rax,0
	xor	r14,r14
	and	rsi,rax
	not	rax
	mov	rcx,rdi
	and	rcx,rax
	mov	r15,r9
	or	rsi,rcx
ALIGN	16
$L$copy::
	mov	rax,QWORD PTR[r14*8+rsi]
	mov	QWORD PTR[r14*8+rsp],r14
	mov	QWORD PTR[r14*8+rdi],rax
	lea	r14,QWORD PTR[1+r14]
	sub	r15,1
	jnz	$L$copy

	mov	rsi,QWORD PTR[8+r9*8+rsp]
	mov	rax,1
	movaps	xmm6,XMMWORD PTR[rsi]
	movaps	xmm7,XMMWORD PTR[16+rsi]
	lea	rsi,QWORD PTR[40+rsi]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$mul_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mul_mont_gather5::
bn_mul_mont_gather5	ENDP

ALIGN	16
bn_mul4x_mont_gather5	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul4x_mont_gather5::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


$L$mul4x_enter::
	mov	r9d,r9d
	mov	r10d,DWORD PTR[56+rsp]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-40))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
$L$mul4x_alloca::
	mov	rax,rsp
	lea	r11,QWORD PTR[4+r9]
	neg	r11
	lea	rsp,QWORD PTR[r11*8+rsp]
	and	rsp,-1024

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul4x_body::
	mov	QWORD PTR[16+r9*8+rsp],rdi
	mov	r12,rdx
	mov	r11,r10
	shr	r10,3
	and	r11,7
	not	r10
	lea	rax,QWORD PTR[$L$magic_masks]
	and	r10,3
	lea	r12,QWORD PTR[96+r11*8+r12]
	movq	xmm4,QWORD PTR[r10*8+rax]
	movq	xmm5,QWORD PTR[8+r10*8+rax]
	movq	xmm6,QWORD PTR[16+r10*8+rax]
	movq	xmm7,QWORD PTR[24+r10*8+rax]

	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5
	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7
	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

DB	102,72,15,126,195
	mov	r8,QWORD PTR[r8]
	mov	rax,QWORD PTR[rsi]

	xor	r14,r14
	xor	r15,r15

	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5

	mov	rbp,r8
	mul	rbx
	mov	r10,rax
	mov	rax,QWORD PTR[rcx]

	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7

	imul	rbp,r10
	mov	r11,rdx

	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,QWORD PTR[4+r15]
	adc	rdx,0
	mov	QWORD PTR[rsp],rdi
	mov	r13,rdx
	jmp	$L$1st4x
ALIGN	16
$L$1st4x::
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi
	mov	r13,rdx

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[8+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+r15*8+rcx]
	adc	rdx,0
	lea	r15,QWORD PTR[4+r15]
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-32))+r15*8+rsp],rdi
	mov	r13,rdx
	cmp	r15,r9
	jl	$L$1st4x

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi
	mov	r13,rdx

DB	102,72,15,126,195

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	QWORD PTR[r15*8+rsp],rdi

	lea	r14,QWORD PTR[1+r14]
ALIGN	4
$L$outer4x::
	xor	r15,r15
	movq	xmm0,QWORD PTR[((-96))+r12]
	movq	xmm1,QWORD PTR[((-32))+r12]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r12]
	pand	xmm1,xmm5

	mov	r10,QWORD PTR[rsp]
	mov	rbp,r8
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0

	movq	xmm3,QWORD PTR[96+r12]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7

	imul	rbp,r10
	mov	r11,rdx

	por	xmm0,xmm2
	lea	r12,QWORD PTR[256+r12]
	por	xmm0,xmm3

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[8+rsp]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,QWORD PTR[4+r15]
	adc	rdx,0
	mov	r13,rdx
	jmp	$L$inner4x
ALIGN	16
$L$inner4x::
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD PTR[((-16))+r15*8+rsp]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-32))+r15*8+rsp],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[((-8))+r15*8+rsp]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
	mov	r13,rdx

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD PTR[r15*8+rsp]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[8+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[8+r15*8+rsp]
	adc	rdx,0
	lea	r15,QWORD PTR[4+r15]
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-40))+r15*8+rsp],r13
	mov	r13,rdx
	cmp	r15,r9
	jl	$L$inner4x

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+r15*8+rcx]
	adc	rdx,0
	add	r10,QWORD PTR[((-16))+r15*8+rsp]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-32))+r15*8+rsp],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+r15*8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[((-8))+r15*8+rsp]
	adc	rdx,0
	lea	r14,QWORD PTR[1+r14]
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
	mov	r13,rdx

DB	102,72,15,126,195
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	add	r13,QWORD PTR[r9*8+rsp]
	adc	rdi,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	QWORD PTR[r15*8+rsp],rdi

	cmp	r14,r9
	jl	$L$outer4x
	mov	rdi,QWORD PTR[16+r9*8+rsp]
	mov	rax,QWORD PTR[rsp]
	pxor	xmm0,xmm0
	mov	rdx,QWORD PTR[8+rsp]
	shr	r9,2
	lea	rsi,QWORD PTR[rsp]
	xor	r14,r14

	sub	rax,QWORD PTR[rcx]
	mov	rbx,QWORD PTR[16+rsi]
	mov	rbp,QWORD PTR[24+rsi]
	sbb	rdx,QWORD PTR[8+rcx]
	lea	r15,QWORD PTR[((-1))+r9]
	jmp	$L$sub4x
ALIGN	16
$L$sub4x::
	mov	QWORD PTR[r14*8+rdi],rax
	mov	QWORD PTR[8+r14*8+rdi],rdx
	sbb	rbx,QWORD PTR[16+r14*8+rcx]
	mov	rax,QWORD PTR[32+r14*8+rsi]
	mov	rdx,QWORD PTR[40+r14*8+rsi]
	sbb	rbp,QWORD PTR[24+r14*8+rcx]
	mov	QWORD PTR[16+r14*8+rdi],rbx
	mov	QWORD PTR[24+r14*8+rdi],rbp
	sbb	rax,QWORD PTR[32+r14*8+rcx]
	mov	rbx,QWORD PTR[48+r14*8+rsi]
	mov	rbp,QWORD PTR[56+r14*8+rsi]
	sbb	rdx,QWORD PTR[40+r14*8+rcx]
	lea	r14,QWORD PTR[4+r14]
	dec	r15
	jnz	$L$sub4x

	mov	QWORD PTR[r14*8+rdi],rax
	mov	rax,QWORD PTR[32+r14*8+rsi]
	sbb	rbx,QWORD PTR[16+r14*8+rcx]
	mov	QWORD PTR[8+r14*8+rdi],rdx
	sbb	rbp,QWORD PTR[24+r14*8+rcx]
	mov	QWORD PTR[16+r14*8+rdi],rbx

	sbb	rax,0
	mov	QWORD PTR[24+r14*8+rdi],rbp
	xor	r14,r14
	and	rsi,rax
	not	rax
	mov	rcx,rdi
	and	rcx,rax
	lea	r15,QWORD PTR[((-1))+r9]
	or	rsi,rcx

	movdqu	xmm1,XMMWORD PTR[rsi]
	movdqa	XMMWORD PTR[rsp],xmm0
	movdqu	XMMWORD PTR[rdi],xmm1
	jmp	$L$copy4x
ALIGN	16
$L$copy4x::
	movdqu	xmm2,XMMWORD PTR[16+r14*1+rsi]
	movdqu	xmm1,XMMWORD PTR[32+r14*1+rsi]
	movdqa	XMMWORD PTR[16+r14*1+rsp],xmm0
	movdqu	XMMWORD PTR[16+r14*1+rdi],xmm2
	movdqa	XMMWORD PTR[32+r14*1+rsp],xmm0
	movdqu	XMMWORD PTR[32+r14*1+rdi],xmm1
	lea	r14,QWORD PTR[32+r14]
	dec	r15
	jnz	$L$copy4x

	shl	r9,2
	movdqu	xmm2,XMMWORD PTR[16+r14*1+rsi]
	movdqa	XMMWORD PTR[16+r14*1+rsp],xmm0
	movdqu	XMMWORD PTR[16+r14*1+rdi],xmm2
	mov	rsi,QWORD PTR[8+r9*8+rsp]
	mov	rax,1
	movaps	xmm6,XMMWORD PTR[rsi]
	movaps	xmm7,XMMWORD PTR[16+rsi]
	lea	rsi,QWORD PTR[40+rsi]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$mul4x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mul4x_mont_gather5::
bn_mul4x_mont_gather5	ENDP
PUBLIC	bn_scatter5

ALIGN	16
bn_scatter5	PROC PUBLIC
	cmp	rdx,0
	jz	$L$scatter_epilogue
	lea	r8,QWORD PTR[r9*8+r8]
$L$scatter::
	mov	rax,QWORD PTR[rcx]
	lea	rcx,QWORD PTR[8+rcx]
	mov	QWORD PTR[r8],rax
	lea	r8,QWORD PTR[256+r8]
	sub	rdx,1
	jnz	$L$scatter
$L$scatter_epilogue::
	DB	0F3h,0C3h		;repret
bn_scatter5	ENDP

PUBLIC	bn_gather5

ALIGN	16
bn_gather5	PROC PUBLIC
$L$SEH_begin_bn_gather5::

DB	048h,083h,0ech,028h

DB	00fh,029h,034h,024h

DB	00fh,029h,07ch,024h,010h

	mov	r11,r9
	shr	r9,3
	and	r11,7
	not	r9
	lea	rax,QWORD PTR[$L$magic_masks]
	and	r9,3
	lea	r8,QWORD PTR[96+r11*8+r8]
	movq	xmm4,QWORD PTR[r9*8+rax]
	movq	xmm5,QWORD PTR[8+r9*8+rax]
	movq	xmm6,QWORD PTR[16+r9*8+rax]
	movq	xmm7,QWORD PTR[24+r9*8+rax]
	jmp	$L$gather
ALIGN	16
$L$gather::
	movq	xmm0,QWORD PTR[((-96))+r8]
	movq	xmm1,QWORD PTR[((-32))+r8]
	pand	xmm0,xmm4
	movq	xmm2,QWORD PTR[32+r8]
	pand	xmm1,xmm5
	movq	xmm3,QWORD PTR[96+r8]
	pand	xmm2,xmm6
	por	xmm0,xmm1
	pand	xmm3,xmm7
	por	xmm0,xmm2
	lea	r8,QWORD PTR[256+r8]
	por	xmm0,xmm3

	movq	QWORD PTR[rcx],xmm0
	lea	rcx,QWORD PTR[8+rcx]
	sub	rdx,1
	jnz	$L$gather
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	lea	rsp,QWORD PTR[40+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_gather5::
bn_gather5	ENDP
ALIGN	64
$L$magic_masks::
	DD	0,0,0,0,0,0,-1,-1
	DD	0,0,0,0,0,0,0,0
DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
DB	112,108,105,99,97,116,105,111,110,32,119,105,116,104,32,115
DB	99,97,116,116,101,114,47,103,97,116,104,101,114,32,102,111
DB	114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79
DB	71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111
DB	112,101,110,115,115,108,46,111,114,103,62,0
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
mul_handler	PROC PRIVATE
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

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	lea	rax,QWORD PTR[88+rax]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[8+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	r10,QWORD PTR[192+r8]
	mov	rax,QWORD PTR[8+r10*8+rax]

	movaps	xmm0,XMMWORD PTR[rax]
	movaps	xmm1,XMMWORD PTR[16+rax]
	lea	rax,QWORD PTR[88+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp
	mov	QWORD PTR[216+r8],r12
	mov	QWORD PTR[224+r8],r13
	mov	QWORD PTR[232+r8],r14
	mov	QWORD PTR[240+r8],r15
	movups	XMMWORD PTR[512+r8],xmm0
	movups	XMMWORD PTR[528+r8],xmm1

$L$common_seh_tail::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	mov	rdi,QWORD PTR[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0a548f3fch


	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD PTR[8+rsi]
	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[16+rsi]
	mov	r10,QWORD PTR[40+rsi]
	lea	r11,QWORD PTR[56+rsi]
	lea	r12,QWORD PTR[24+rsi]
	mov	QWORD PTR[32+rsp],r10
	mov	QWORD PTR[40+rsp],r11
	mov	QWORD PTR[48+rsp],r12
	mov	QWORD PTR[56+rsp],rcx
	call	QWORD PTR[__imp_RtlVirtualUnwind]

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
mul_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_bn_mul_mont_gather5
	DD	imagerel $L$SEH_end_bn_mul_mont_gather5
	DD	imagerel $L$SEH_info_bn_mul_mont_gather5

	DD	imagerel $L$SEH_begin_bn_mul4x_mont_gather5
	DD	imagerel $L$SEH_end_bn_mul4x_mont_gather5
	DD	imagerel $L$SEH_info_bn_mul4x_mont_gather5

	DD	imagerel $L$SEH_begin_bn_gather5
	DD	imagerel $L$SEH_end_bn_gather5
	DD	imagerel $L$SEH_info_bn_gather5

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_bn_mul_mont_gather5::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul_alloca,imagerel $L$mul_body,imagerel $L$mul_epilogue

ALIGN	8
$L$SEH_info_bn_mul4x_mont_gather5::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul4x_alloca,imagerel $L$mul4x_body,imagerel $L$mul4x_epilogue

ALIGN	8
$L$SEH_info_bn_gather5::
DB	001h,00dh,005h,000h
DB	00dh,078h,001h,000h

DB	008h,068h,000h,000h

DB	004h,042h,000h,000h

ALIGN	8

.xdata	ENDS
END
