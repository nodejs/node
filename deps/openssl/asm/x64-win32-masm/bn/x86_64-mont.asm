OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	bn_mul_mont

ALIGN	16
bn_mul_mont	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul_mont::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	r9d,r9d
	mov	rax,rsp
	test	r9d,3
	jnz	$L$mul_enter
	cmp	r9d,8
	jb	$L$mul_enter
	mov	r11d,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	cmp	rdx,rsi
	jne	$L$mul4x_enter
	test	r9d,7
	jz	$L$sqr8x_enter
	jmp	$L$mul4x_enter

ALIGN	16
$L$mul_enter::
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	neg	r9
	mov	r11,rsp
	lea	r10,QWORD PTR[((-16))+r9*8+rsp]
	neg	r9
	and	r10,-1024







	sub	r11,r10
	and	r11,-4096
	lea	rsp,QWORD PTR[r11*1+r10]
	mov	r11,QWORD PTR[rsp]
	cmp	rsp,r10
	ja	$L$mul_page_walk
	jmp	$L$mul_page_walk_done

ALIGN	16
$L$mul_page_walk::
	lea	rsp,QWORD PTR[((-4096))+rsp]
	mov	r11,QWORD PTR[rsp]
	cmp	rsp,r10
	ja	$L$mul_page_walk
$L$mul_page_walk_done::

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul_body::
	mov	r12,rdx
	mov	r8,QWORD PTR[r8]
	mov	rbx,QWORD PTR[r12]
	mov	rax,QWORD PTR[rsi]

	xor	r14,r14
	xor	r15,r15

	mov	rbp,r8
	mul	rbx
	mov	r10,rax
	mov	rax,QWORD PTR[rcx]

	imul	rbp,r10
	mov	r11,rdx

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
	mov	rbx,QWORD PTR[r14*8+r12]
	xor	r15,r15
	mov	rbp,r8
	mov	r10,QWORD PTR[rsp]
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0

	imul	rbp,r10
	mov	r11,rdx

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
	jb	$L$outer

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
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$mul_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mul_mont::
bn_mul_mont	ENDP

ALIGN	16
bn_mul4x_mont	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mul4x_mont::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	r9d,r9d
	mov	rax,rsp
$L$mul4x_enter::
	and	r11d,080100h
	cmp	r11d,080100h
	je	$L$mulx4x_enter
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	neg	r9
	mov	r11,rsp
	lea	r10,QWORD PTR[((-32))+r9*8+rsp]
	neg	r9
	and	r10,-1024

	sub	r11,r10
	and	r11,-4096
	lea	rsp,QWORD PTR[r11*1+r10]
	mov	r11,QWORD PTR[rsp]
	cmp	rsp,r10
	ja	$L$mul4x_page_walk
	jmp	$L$mul4x_page_walk_done

$L$mul4x_page_walk::
	lea	rsp,QWORD PTR[((-4096))+rsp]
	mov	r11,QWORD PTR[rsp]
	cmp	rsp,r10
	ja	$L$mul4x_page_walk
$L$mul4x_page_walk_done::

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul4x_body::
	mov	QWORD PTR[16+r9*8+rsp],rdi
	mov	r12,rdx
	mov	r8,QWORD PTR[r8]
	mov	rbx,QWORD PTR[r12]
	mov	rax,QWORD PTR[rsi]

	xor	r14,r14
	xor	r15,r15

	mov	rbp,r8
	mul	rbx
	mov	r10,rax
	mov	rax,QWORD PTR[rcx]

	imul	rbp,r10
	mov	r11,rdx

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
	jb	$L$1st4x

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

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	QWORD PTR[r15*8+rsp],rdi

	lea	r14,QWORD PTR[1+r14]
ALIGN	4
$L$outer4x::
	mov	rbx,QWORD PTR[r14*8+r12]
	xor	r15,r15
	mov	r10,QWORD PTR[rsp]
	mov	rbp,r8
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0

	imul	rbp,r10
	mov	r11,rdx

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
	mov	QWORD PTR[rsp],rdi
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
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
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
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi
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
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
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
	mov	QWORD PTR[((-32))+r15*8+rsp],rdi
	mov	r13,rdx
	cmp	r15,r9
	jb	$L$inner4x

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
	mov	QWORD PTR[((-24))+r15*8+rsp],r13
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
	mov	QWORD PTR[((-16))+r15*8+rsp],rdi
	mov	r13,rdx

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	add	r13,QWORD PTR[r9*8+rsp]
	adc	rdi,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	QWORD PTR[r15*8+rsp],rdi

	cmp	r14,r9
	jb	$L$outer4x
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
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$mul4x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mul4x_mont::
bn_mul4x_mont	ENDP
EXTERN	bn_sqrx8x_internal:NEAR
EXTERN	bn_sqr8x_internal:NEAR


ALIGN	32
bn_sqr8x_mont	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_sqr8x_mont::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	rax,rsp
$L$sqr8x_enter::
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$sqr8x_prologue::

	mov	r10d,r9d
	shl	r9d,3
	shl	r10,3+2
	neg	r9






	lea	r11,QWORD PTR[((-64))+r9*2+rsp]
	mov	rbp,rsp
	mov	r8,QWORD PTR[r8]
	sub	r11,rsi
	and	r11,4095
	cmp	r10,r11
	jb	$L$sqr8x_sp_alt
	sub	rbp,r11
	lea	rbp,QWORD PTR[((-64))+r9*2+rbp]
	jmp	$L$sqr8x_sp_done

ALIGN	32
$L$sqr8x_sp_alt::
	lea	r10,QWORD PTR[((4096-64))+r9*2]
	lea	rbp,QWORD PTR[((-64))+r9*2+rbp]
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rbp,r11
$L$sqr8x_sp_done::
	and	rbp,-64
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,QWORD PTR[rbp*1+r11]
	mov	r10,QWORD PTR[rsp]
	cmp	rsp,rbp
	ja	$L$sqr8x_page_walk
	jmp	$L$sqr8x_page_walk_done

ALIGN	16
$L$sqr8x_page_walk::
	lea	rsp,QWORD PTR[((-4096))+rsp]
	mov	r10,QWORD PTR[rsp]
	cmp	rsp,rbp
	ja	$L$sqr8x_page_walk
$L$sqr8x_page_walk_done::

	mov	r10,r9
	neg	r9

	mov	QWORD PTR[32+rsp],r8
	mov	QWORD PTR[40+rsp],rax
$L$sqr8x_body::

DB	102,72,15,110,209
	pxor	xmm0,xmm0
DB	102,72,15,110,207
DB	102,73,15,110,218
	mov	eax,DWORD PTR[((OPENSSL_ia32cap_P+8))]
	and	eax,080100h
	cmp	eax,080100h
	jne	$L$sqr8x_nox

	call	bn_sqrx8x_internal




	lea	rbx,QWORD PTR[rcx*1+r8]
	mov	r9,rcx
	mov	rdx,rcx
DB	102,72,15,126,207
	sar	rcx,3+2
	jmp	$L$sqr8x_sub

ALIGN	32
$L$sqr8x_nox::
	call	bn_sqr8x_internal




	lea	rbx,QWORD PTR[r9*1+rdi]
	mov	rcx,r9
	mov	rdx,r9
DB	102,72,15,126,207
	sar	rcx,3+2
	jmp	$L$sqr8x_sub

ALIGN	32
$L$sqr8x_sub::
	mov	r12,QWORD PTR[rbx]
	mov	r13,QWORD PTR[8+rbx]
	mov	r14,QWORD PTR[16+rbx]
	mov	r15,QWORD PTR[24+rbx]
	lea	rbx,QWORD PTR[32+rbx]
	sbb	r12,QWORD PTR[rbp]
	sbb	r13,QWORD PTR[8+rbp]
	sbb	r14,QWORD PTR[16+rbp]
	sbb	r15,QWORD PTR[24+rbp]
	lea	rbp,QWORD PTR[32+rbp]
	mov	QWORD PTR[rdi],r12
	mov	QWORD PTR[8+rdi],r13
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15
	lea	rdi,QWORD PTR[32+rdi]
	inc	rcx
	jnz	$L$sqr8x_sub

	sbb	rax,0
	lea	rbx,QWORD PTR[r9*1+rbx]
	lea	rdi,QWORD PTR[r9*1+rdi]

DB	102,72,15,110,200
	pxor	xmm0,xmm0
	pshufd	xmm1,xmm1,0
	mov	rsi,QWORD PTR[40+rsp]
	jmp	$L$sqr8x_cond_copy

ALIGN	32
$L$sqr8x_cond_copy::
	movdqa	xmm2,XMMWORD PTR[rbx]
	movdqa	xmm3,XMMWORD PTR[16+rbx]
	lea	rbx,QWORD PTR[32+rbx]
	movdqu	xmm4,XMMWORD PTR[rdi]
	movdqu	xmm5,XMMWORD PTR[16+rdi]
	lea	rdi,QWORD PTR[32+rdi]
	movdqa	XMMWORD PTR[(-32)+rbx],xmm0
	movdqa	XMMWORD PTR[(-16)+rbx],xmm0
	movdqa	XMMWORD PTR[(-32)+rdx*1+rbx],xmm0
	movdqa	XMMWORD PTR[(-16)+rdx*1+rbx],xmm0
	pcmpeqd	xmm0,xmm1
	pand	xmm2,xmm1
	pand	xmm3,xmm1
	pand	xmm4,xmm0
	pand	xmm5,xmm0
	pxor	xmm0,xmm0
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqu	XMMWORD PTR[(-32)+rdi],xmm4
	movdqu	XMMWORD PTR[(-16)+rdi],xmm5
	add	r9,32
	jnz	$L$sqr8x_cond_copy

	mov	rax,1
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$sqr8x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_sqr8x_mont::
bn_sqr8x_mont	ENDP

ALIGN	32
bn_mulx4x_mont	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_mulx4x_mont::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	rax,rsp
$L$mulx4x_enter::
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
$L$mulx4x_prologue::

	shl	r9d,3
	xor	r10,r10
	sub	r10,r9
	mov	r8,QWORD PTR[r8]
	lea	rbp,QWORD PTR[((-72))+r10*1+rsp]
	and	rbp,-128
	mov	r11,rsp
	sub	r11,rbp
	and	r11,-4096
	lea	rsp,QWORD PTR[rbp*1+r11]
	mov	r10,QWORD PTR[rsp]
	cmp	rsp,rbp
	ja	$L$mulx4x_page_walk
	jmp	$L$mulx4x_page_walk_done

ALIGN	16
$L$mulx4x_page_walk::
	lea	rsp,QWORD PTR[((-4096))+rsp]
	mov	r10,QWORD PTR[rsp]
	cmp	rsp,rbp
	ja	$L$mulx4x_page_walk
$L$mulx4x_page_walk_done::

	lea	r10,QWORD PTR[r9*1+rdx]












	mov	QWORD PTR[rsp],r9
	shr	r9,5
	mov	QWORD PTR[16+rsp],r10
	sub	r9,1
	mov	QWORD PTR[24+rsp],r8
	mov	QWORD PTR[32+rsp],rdi
	mov	QWORD PTR[40+rsp],rax
	mov	QWORD PTR[48+rsp],r9
	jmp	$L$mulx4x_body

ALIGN	32
$L$mulx4x_body::
	lea	rdi,QWORD PTR[8+rdx]
	mov	rdx,QWORD PTR[rdx]
	lea	rbx,QWORD PTR[((64+32))+rsp]
	mov	r9,rdx

	mulx	rax,r8,QWORD PTR[rsi]
	mulx	r14,r11,QWORD PTR[8+rsi]
	add	r11,rax
	mov	QWORD PTR[8+rsp],rdi
	mulx	r13,r12,QWORD PTR[16+rsi]
	adc	r12,r14
	adc	r13,0

	mov	rdi,r8
	imul	r8,QWORD PTR[24+rsp]
	xor	rbp,rbp

	mulx	r14,rax,QWORD PTR[24+rsi]
	mov	rdx,r8
	lea	rsi,QWORD PTR[32+rsi]
	adcx	r13,rax
	adcx	r14,rbp

	mulx	r10,rax,QWORD PTR[rcx]
	adcx	rdi,rax
	adox	r10,r11
	mulx	r11,rax,QWORD PTR[8+rcx]
	adcx	r10,rax
	adox	r11,r12
DB	0c4h,062h,0fbh,0f6h,0a1h,010h,000h,000h,000h
	mov	rdi,QWORD PTR[48+rsp]
	mov	QWORD PTR[((-32))+rbx],r10
	adcx	r11,rax
	adox	r12,r13
	mulx	r15,rax,QWORD PTR[24+rcx]
	mov	rdx,r9
	mov	QWORD PTR[((-24))+rbx],r11
	adcx	r12,rax
	adox	r15,rbp
	lea	rcx,QWORD PTR[32+rcx]
	mov	QWORD PTR[((-16))+rbx],r12

	jmp	$L$mulx4x_1st

ALIGN	32
$L$mulx4x_1st::
	adcx	r15,rbp
	mulx	rax,r10,QWORD PTR[rsi]
	adcx	r10,r14
	mulx	r14,r11,QWORD PTR[8+rsi]
	adcx	r11,rax
	mulx	rax,r12,QWORD PTR[16+rsi]
	adcx	r12,r14
	mulx	r14,r13,QWORD PTR[24+rsi]
DB	067h,067h
	mov	rdx,r8
	adcx	r13,rax
	adcx	r14,rbp
	lea	rsi,QWORD PTR[32+rsi]
	lea	rbx,QWORD PTR[32+rbx]

	adox	r10,r15
	mulx	r15,rax,QWORD PTR[rcx]
	adcx	r10,rax
	adox	r11,r15
	mulx	r15,rax,QWORD PTR[8+rcx]
	adcx	r11,rax
	adox	r12,r15
	mulx	r15,rax,QWORD PTR[16+rcx]
	mov	QWORD PTR[((-40))+rbx],r10
	adcx	r12,rax
	mov	QWORD PTR[((-32))+rbx],r11
	adox	r13,r15
	mulx	r15,rax,QWORD PTR[24+rcx]
	mov	rdx,r9
	mov	QWORD PTR[((-24))+rbx],r12
	adcx	r13,rax
	adox	r15,rbp
	lea	rcx,QWORD PTR[32+rcx]
	mov	QWORD PTR[((-16))+rbx],r13

	dec	rdi
	jnz	$L$mulx4x_1st

	mov	rax,QWORD PTR[rsp]
	mov	rdi,QWORD PTR[8+rsp]
	adc	r15,rbp
	add	r14,r15
	sbb	r15,r15
	mov	QWORD PTR[((-8))+rbx],r14
	jmp	$L$mulx4x_outer

ALIGN	32
$L$mulx4x_outer::
	mov	rdx,QWORD PTR[rdi]
	lea	rdi,QWORD PTR[8+rdi]
	sub	rsi,rax
	mov	QWORD PTR[rbx],r15
	lea	rbx,QWORD PTR[((64+32))+rsp]
	sub	rcx,rax

	mulx	r11,r8,QWORD PTR[rsi]
	xor	ebp,ebp
	mov	r9,rdx
	mulx	r12,r14,QWORD PTR[8+rsi]
	adox	r8,QWORD PTR[((-32))+rbx]
	adcx	r11,r14
	mulx	r13,r15,QWORD PTR[16+rsi]
	adox	r11,QWORD PTR[((-24))+rbx]
	adcx	r12,r15
	adox	r12,rbp
	adcx	r13,rbp

	mov	QWORD PTR[8+rsp],rdi
DB	067h
	mov	r15,r8
	imul	r8,QWORD PTR[24+rsp]
	xor	ebp,ebp

	mulx	r14,rax,QWORD PTR[24+rsi]
	mov	rdx,r8
	adox	r12,QWORD PTR[((-16))+rbx]
	adcx	r13,rax
	adox	r13,QWORD PTR[((-8))+rbx]
	adcx	r14,rbp
	lea	rsi,QWORD PTR[32+rsi]
	adox	r14,rbp

	mulx	r10,rax,QWORD PTR[rcx]
	adcx	r15,rax
	adox	r10,r11
	mulx	r11,rax,QWORD PTR[8+rcx]
	adcx	r10,rax
	adox	r11,r12
	mulx	r12,rax,QWORD PTR[16+rcx]
	mov	QWORD PTR[((-32))+rbx],r10
	adcx	r11,rax
	adox	r12,r13
	mulx	r15,rax,QWORD PTR[24+rcx]
	mov	rdx,r9
	mov	QWORD PTR[((-24))+rbx],r11
	lea	rcx,QWORD PTR[32+rcx]
	adcx	r12,rax
	adox	r15,rbp
	mov	rdi,QWORD PTR[48+rsp]
	mov	QWORD PTR[((-16))+rbx],r12

	jmp	$L$mulx4x_inner

ALIGN	32
$L$mulx4x_inner::
	mulx	rax,r10,QWORD PTR[rsi]
	adcx	r15,rbp
	adox	r10,r14
	mulx	r14,r11,QWORD PTR[8+rsi]
	adcx	r10,QWORD PTR[rbx]
	adox	r11,rax
	mulx	rax,r12,QWORD PTR[16+rsi]
	adcx	r11,QWORD PTR[8+rbx]
	adox	r12,r14
	mulx	r14,r13,QWORD PTR[24+rsi]
	mov	rdx,r8
	adcx	r12,QWORD PTR[16+rbx]
	adox	r13,rax
	adcx	r13,QWORD PTR[24+rbx]
	adox	r14,rbp
	lea	rsi,QWORD PTR[32+rsi]
	lea	rbx,QWORD PTR[32+rbx]
	adcx	r14,rbp

	adox	r10,r15
	mulx	r15,rax,QWORD PTR[rcx]
	adcx	r10,rax
	adox	r11,r15
	mulx	r15,rax,QWORD PTR[8+rcx]
	adcx	r11,rax
	adox	r12,r15
	mulx	r15,rax,QWORD PTR[16+rcx]
	mov	QWORD PTR[((-40))+rbx],r10
	adcx	r12,rax
	adox	r13,r15
	mulx	r15,rax,QWORD PTR[24+rcx]
	mov	rdx,r9
	mov	QWORD PTR[((-32))+rbx],r11
	mov	QWORD PTR[((-24))+rbx],r12
	adcx	r13,rax
	adox	r15,rbp
	lea	rcx,QWORD PTR[32+rcx]
	mov	QWORD PTR[((-16))+rbx],r13

	dec	rdi
	jnz	$L$mulx4x_inner

	mov	rax,QWORD PTR[rsp]
	mov	rdi,QWORD PTR[8+rsp]
	adc	r15,rbp
	sub	rbp,QWORD PTR[rbx]
	adc	r14,r15
	sbb	r15,r15
	mov	QWORD PTR[((-8))+rbx],r14

	cmp	rdi,QWORD PTR[16+rsp]
	jne	$L$mulx4x_outer

	lea	rbx,QWORD PTR[64+rsp]
	sub	rcx,rax
	neg	r15
	mov	rdx,rax
	shr	rax,3+2
	mov	rdi,QWORD PTR[32+rsp]
	jmp	$L$mulx4x_sub

ALIGN	32
$L$mulx4x_sub::
	mov	r11,QWORD PTR[rbx]
	mov	r12,QWORD PTR[8+rbx]
	mov	r13,QWORD PTR[16+rbx]
	mov	r14,QWORD PTR[24+rbx]
	lea	rbx,QWORD PTR[32+rbx]
	sbb	r11,QWORD PTR[rcx]
	sbb	r12,QWORD PTR[8+rcx]
	sbb	r13,QWORD PTR[16+rcx]
	sbb	r14,QWORD PTR[24+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	mov	QWORD PTR[rdi],r11
	mov	QWORD PTR[8+rdi],r12
	mov	QWORD PTR[16+rdi],r13
	mov	QWORD PTR[24+rdi],r14
	lea	rdi,QWORD PTR[32+rdi]
	dec	rax
	jnz	$L$mulx4x_sub

	sbb	r15,0
	lea	rbx,QWORD PTR[64+rsp]
	sub	rdi,rdx

DB	102,73,15,110,207
	pxor	xmm0,xmm0
	pshufd	xmm1,xmm1,0
	mov	rsi,QWORD PTR[40+rsp]
	jmp	$L$mulx4x_cond_copy

ALIGN	32
$L$mulx4x_cond_copy::
	movdqa	xmm2,XMMWORD PTR[rbx]
	movdqa	xmm3,XMMWORD PTR[16+rbx]
	lea	rbx,QWORD PTR[32+rbx]
	movdqu	xmm4,XMMWORD PTR[rdi]
	movdqu	xmm5,XMMWORD PTR[16+rdi]
	lea	rdi,QWORD PTR[32+rdi]
	movdqa	XMMWORD PTR[(-32)+rbx],xmm0
	movdqa	XMMWORD PTR[(-16)+rbx],xmm0
	pcmpeqd	xmm0,xmm1
	pand	xmm2,xmm1
	pand	xmm3,xmm1
	pand	xmm4,xmm0
	pand	xmm5,xmm0
	pxor	xmm0,xmm0
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqu	XMMWORD PTR[(-32)+rdi],xmm4
	movdqu	XMMWORD PTR[(-16)+rdi],xmm5
	sub	rdx,32
	jnz	$L$mulx4x_cond_copy

	mov	QWORD PTR[rbx],rdx

	mov	rax,1
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$mulx4x_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mulx4x_mont::
bn_mulx4x_mont	ENDP
DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
DB	112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56
DB	54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
ALIGN	16
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

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	r10,QWORD PTR[192+r8]
	mov	rax,QWORD PTR[8+r10*8+rax]

	jmp	$L$common_pop_regs
mul_handler	ENDP


ALIGN	16
sqr_handler	PROC PRIVATE
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

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_pop_regs

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[8+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	mov	rax,QWORD PTR[40+rax]

$L$common_pop_regs::
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
sqr_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_bn_mul_mont
	DD	imagerel $L$SEH_end_bn_mul_mont
	DD	imagerel $L$SEH_info_bn_mul_mont

	DD	imagerel $L$SEH_begin_bn_mul4x_mont
	DD	imagerel $L$SEH_end_bn_mul4x_mont
	DD	imagerel $L$SEH_info_bn_mul4x_mont

	DD	imagerel $L$SEH_begin_bn_sqr8x_mont
	DD	imagerel $L$SEH_end_bn_sqr8x_mont
	DD	imagerel $L$SEH_info_bn_sqr8x_mont
	DD	imagerel $L$SEH_begin_bn_mulx4x_mont
	DD	imagerel $L$SEH_end_bn_mulx4x_mont
	DD	imagerel $L$SEH_info_bn_mulx4x_mont
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_bn_mul_mont::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul_body,imagerel $L$mul_epilogue
$L$SEH_info_bn_mul4x_mont::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul4x_body,imagerel $L$mul4x_epilogue
$L$SEH_info_bn_sqr8x_mont::
DB	9,0,0,0
	DD	imagerel sqr_handler
	DD	imagerel $L$sqr8x_prologue,imagerel $L$sqr8x_body,imagerel $L$sqr8x_epilogue
ALIGN	8
$L$SEH_info_bn_mulx4x_mont::
DB	9,0,0,0
	DD	imagerel sqr_handler
	DD	imagerel $L$mulx4x_prologue,imagerel $L$mulx4x_body,imagerel $L$mulx4x_epilogue
ALIGN	8

.xdata	ENDS
END
