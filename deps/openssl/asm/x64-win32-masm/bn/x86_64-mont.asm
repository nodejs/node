OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'

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


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	mov	r9d,r9d
	lea	r10,QWORD PTR[2+r9]
	mov	r11,rsp
	neg	r10
	lea	rsp,QWORD PTR[r10*8+rsp]
	and	rsp,-1024

	mov	QWORD PTR[8+r9*8+rsp],r11
$L$prologue::
	mov	r12,rdx

	mov	r8,QWORD PTR[r8]

	xor	r14,r14
	xor	r15,r15

	mov	rbx,QWORD PTR[r12]
	mov	rax,QWORD PTR[rsi]
	mul	rbx
	mov	r10,rax
	mov	r11,rdx

	imul	rax,r8
	mov	rbp,rax

	mul	QWORD PTR[rcx]
	add	rax,r10
	adc	rdx,0
	mov	r13,rdx

	lea	r15,QWORD PTR[1+r15]
$L$1st::
	mov	rax,QWORD PTR[r15*8+rsi]
	mul	rbx
	add	rax,r11
	adc	rdx,0
	mov	r10,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	mov	r11,rdx

	mul	rbp
	add	rax,r13
	lea	r15,QWORD PTR[1+r15]
	adc	rdx,0
	add	rax,r10
	adc	rdx,0
	mov	QWORD PTR[((-16))+r15*8+rsp],rax
	cmp	r15,r9
	mov	r13,rdx
	jl	$L$1st

	xor	rdx,rdx
	add	r13,r11
	adc	rdx,0
	mov	QWORD PTR[((-8))+r9*8+rsp],r13
	mov	QWORD PTR[r9*8+rsp],rdx

	lea	r14,QWORD PTR[1+r14]
ALIGN	4
$L$outer::
	xor	r15,r15

	mov	rbx,QWORD PTR[r14*8+r12]
	mov	rax,QWORD PTR[rsi]
	mul	rbx
	add	rax,QWORD PTR[rsp]
	adc	rdx,0
	mov	r10,rax
	mov	r11,rdx

	imul	rax,r8
	mov	rbp,rax

	mul	QWORD PTR[r15*8+rcx]
	add	rax,r10
	mov	r10,QWORD PTR[8+rsp]
	adc	rdx,0
	mov	r13,rdx

	lea	r15,QWORD PTR[1+r15]
ALIGN	4
$L$inner::
	mov	rax,QWORD PTR[r15*8+rsi]
	mul	rbx
	add	rax,r11
	adc	rdx,0
	add	r10,rax
	mov	rax,QWORD PTR[r15*8+rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	rax,r13
	lea	r15,QWORD PTR[1+r15]
	adc	rdx,0
	add	rax,r10
	adc	rdx,0
	mov	r10,QWORD PTR[r15*8+rsp]
	cmp	r15,r9
	mov	QWORD PTR[((-16))+r15*8+rsp],rax
	mov	r13,rdx
	jl	$L$inner

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

	lea	rsi,QWORD PTR[rsp]
	lea	r15,QWORD PTR[((-1))+r9]

	mov	rax,QWORD PTR[rsi]
	xor	r14,r14
	jmp	$L$sub
ALIGN	16
$L$sub::	sbb	rax,QWORD PTR[r14*8+rcx]
	mov	QWORD PTR[r14*8+rdi],rax
	dec	r15
	mov	rax,QWORD PTR[8+r14*8+rsi]
	lea	r14,QWORD PTR[1+r14]
	jge	$L$sub

	sbb	rax,0
	and	rsi,rax
	not	rax
	mov	rcx,rdi
	and	rcx,rax
	lea	r15,QWORD PTR[((-1))+r9]
	or	rsi,rcx
ALIGN	16
$L$copy::
	mov	rax,QWORD PTR[r15*8+rsi]
	mov	QWORD PTR[r15*8+rdi],rax
	mov	QWORD PTR[r15*8+rsp],r14
	dec	r15
	jge	$L$copy

	mov	rsi,QWORD PTR[8+r9*8+rsp]
	mov	rax,1
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_mul_mont::
bn_mul_mont	ENDP
DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
DB	112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56
DB	54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
ALIGN	16
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
se_handler	PROC PRIVATE
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

	lea	r10,QWORD PTR[$L$prologue]
	cmp	rbx,r10
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$epilogue]
	cmp	rbx,r10
	jae	$L$in_prologue

	mov	r10,QWORD PTR[192+r8]
	mov	rax,QWORD PTR[8+r10*8+rax]
	lea	rax,QWORD PTR[48+rax]

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

$L$in_prologue::
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
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_bn_mul_mont
	DD	imagerel $L$SEH_end_bn_mul_mont
	DD	imagerel $L$SEH_info_bn_mul_mont

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_bn_mul_mont::
DB	9,0,0,0
	DD	imagerel se_handler

.xdata	ENDS
END
