OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	rsaz_512_sqr

ALIGN	32
rsaz_512_sqr	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_512_sqr::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	sub	rsp,128+24
$L$sqr_body::
	mov	rbp,rdx
	mov	rdx,QWORD PTR[rsi]
	mov	rax,QWORD PTR[8+rsi]
	mov	QWORD PTR[128+rsp],rcx
	jmp	$L$oop_sqr

ALIGN	32
$L$oop_sqr::
	mov	DWORD PTR[((128+8))+rsp],r8d

	mov	rbx,rdx
	mul	rdx
	mov	r8,rax
	mov	rax,QWORD PTR[16+rsi]
	mov	r9,rdx

	mul	rbx
	add	r9,rax
	mov	rax,QWORD PTR[24+rsi]
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[32+rsi]
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[40+rsi]
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[48+rsi]
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD PTR[56+rsi]
	mov	r14,rdx
	adc	r14,0

	mul	rbx
	add	r14,rax
	mov	rax,rbx
	mov	r15,rdx
	adc	r15,0

	add	r8,r8
	mov	rcx,r9
	adc	r9,r9

	mul	rax
	mov	QWORD PTR[rsp],rax
	add	r8,rdx
	adc	r9,0

	mov	QWORD PTR[8+rsp],r8
	shr	rcx,63


	mov	r8,QWORD PTR[8+rsi]
	mov	rax,QWORD PTR[16+rsi]
	mul	r8
	add	r10,rax
	mov	rax,QWORD PTR[24+rsi]
	mov	rbx,rdx
	adc	rbx,0

	mul	r8
	add	r11,rax
	mov	rax,QWORD PTR[32+rsi]
	adc	rdx,0
	add	r11,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r8
	add	r12,rax
	mov	rax,QWORD PTR[40+rsi]
	adc	rdx,0
	add	r12,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r8
	add	r13,rax
	mov	rax,QWORD PTR[48+rsi]
	adc	rdx,0
	add	r13,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r8
	add	r14,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	add	r14,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r8
	add	r15,rax
	mov	rax,r8
	adc	rdx,0
	add	r15,rbx
	mov	r8,rdx
	mov	rdx,r10
	adc	r8,0

	add	rdx,rdx
	lea	r10,QWORD PTR[r10*2+rcx]
	mov	rbx,r11
	adc	r11,r11

	mul	rax
	add	r9,rax
	adc	r10,rdx
	adc	r11,0

	mov	QWORD PTR[16+rsp],r9
	mov	QWORD PTR[24+rsp],r10
	shr	rbx,63


	mov	r9,QWORD PTR[16+rsi]
	mov	rax,QWORD PTR[24+rsi]
	mul	r9
	add	r12,rax
	mov	rax,QWORD PTR[32+rsi]
	mov	rcx,rdx
	adc	rcx,0

	mul	r9
	add	r13,rax
	mov	rax,QWORD PTR[40+rsi]
	adc	rdx,0
	add	r13,rcx
	mov	rcx,rdx
	adc	rcx,0

	mul	r9
	add	r14,rax
	mov	rax,QWORD PTR[48+rsi]
	adc	rdx,0
	add	r14,rcx
	mov	rcx,rdx
	adc	rcx,0

	mul	r9
	mov	r10,r12
	lea	r12,QWORD PTR[r12*2+rbx]
	add	r15,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	add	r15,rcx
	mov	rcx,rdx
	adc	rcx,0

	mul	r9
	shr	r10,63
	add	r8,rax
	mov	rax,r9
	adc	rdx,0
	add	r8,rcx
	mov	r9,rdx
	adc	r9,0

	mov	rcx,r13
	lea	r13,QWORD PTR[r13*2+r10]

	mul	rax
	add	r11,rax
	adc	r12,rdx
	adc	r13,0

	mov	QWORD PTR[32+rsp],r11
	mov	QWORD PTR[40+rsp],r12
	shr	rcx,63


	mov	r10,QWORD PTR[24+rsi]
	mov	rax,QWORD PTR[32+rsi]
	mul	r10
	add	r14,rax
	mov	rax,QWORD PTR[40+rsi]
	mov	rbx,rdx
	adc	rbx,0

	mul	r10
	add	r15,rax
	mov	rax,QWORD PTR[48+rsi]
	adc	rdx,0
	add	r15,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r10
	mov	r12,r14
	lea	r14,QWORD PTR[r14*2+rcx]
	add	r8,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	add	r8,rbx
	mov	rbx,rdx
	adc	rbx,0

	mul	r10
	shr	r12,63
	add	r9,rax
	mov	rax,r10
	adc	rdx,0
	add	r9,rbx
	mov	r10,rdx
	adc	r10,0

	mov	rbx,r15
	lea	r15,QWORD PTR[r15*2+r12]

	mul	rax
	add	r13,rax
	adc	r14,rdx
	adc	r15,0

	mov	QWORD PTR[48+rsp],r13
	mov	QWORD PTR[56+rsp],r14
	shr	rbx,63


	mov	r11,QWORD PTR[32+rsi]
	mov	rax,QWORD PTR[40+rsi]
	mul	r11
	add	r8,rax
	mov	rax,QWORD PTR[48+rsi]
	mov	rcx,rdx
	adc	rcx,0

	mul	r11
	add	r9,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	mov	r12,r8
	lea	r8,QWORD PTR[r8*2+rbx]
	add	r9,rcx
	mov	rcx,rdx
	adc	rcx,0

	mul	r11
	shr	r12,63
	add	r10,rax
	mov	rax,r11
	adc	rdx,0
	add	r10,rcx
	mov	r11,rdx
	adc	r11,0

	mov	rcx,r9
	lea	r9,QWORD PTR[r9*2+r12]

	mul	rax
	add	r15,rax
	adc	r8,rdx
	adc	r9,0

	mov	QWORD PTR[64+rsp],r15
	mov	QWORD PTR[72+rsp],r8
	shr	rcx,63


	mov	r12,QWORD PTR[40+rsi]
	mov	rax,QWORD PTR[48+rsi]
	mul	r12
	add	r10,rax
	mov	rax,QWORD PTR[56+rsi]
	mov	rbx,rdx
	adc	rbx,0

	mul	r12
	add	r11,rax
	mov	rax,r12
	mov	r15,r10
	lea	r10,QWORD PTR[r10*2+rcx]
	adc	rdx,0
	shr	r15,63
	add	r11,rbx
	mov	r12,rdx
	adc	r12,0

	mov	rbx,r11
	lea	r11,QWORD PTR[r11*2+r15]

	mul	rax
	add	r9,rax
	adc	r10,rdx
	adc	r11,0

	mov	QWORD PTR[80+rsp],r9
	mov	QWORD PTR[88+rsp],r10


	mov	r13,QWORD PTR[48+rsi]
	mov	rax,QWORD PTR[56+rsi]
	mul	r13
	add	r12,rax
	mov	rax,r13
	mov	r13,rdx
	adc	r13,0

	xor	r14,r14
	shl	rbx,1
	adc	r12,r12
	adc	r13,r13
	adc	r14,r14

	mul	rax
	add	r11,rax
	adc	r12,rdx
	adc	r13,0

	mov	QWORD PTR[96+rsp],r11
	mov	QWORD PTR[104+rsp],r12


	mov	rax,QWORD PTR[56+rsi]
	mul	rax
	add	r13,rax
	adc	rdx,0

	add	r14,rdx

	mov	QWORD PTR[112+rsp],r13
	mov	QWORD PTR[120+rsp],r14

	mov	r8,QWORD PTR[rsp]
	mov	r9,QWORD PTR[8+rsp]
	mov	r10,QWORD PTR[16+rsp]
	mov	r11,QWORD PTR[24+rsp]
	mov	r12,QWORD PTR[32+rsp]
	mov	r13,QWORD PTR[40+rsp]
	mov	r14,QWORD PTR[48+rsp]
	mov	r15,QWORD PTR[56+rsp]

	call	__rsaz_512_reduce

	add	r8,QWORD PTR[64+rsp]
	adc	r9,QWORD PTR[72+rsp]
	adc	r10,QWORD PTR[80+rsp]
	adc	r11,QWORD PTR[88+rsp]
	adc	r12,QWORD PTR[96+rsp]
	adc	r13,QWORD PTR[104+rsp]
	adc	r14,QWORD PTR[112+rsp]
	adc	r15,QWORD PTR[120+rsp]
	sbb	rcx,rcx

	call	__rsaz_512_subtract

	mov	rdx,r8
	mov	rax,r9
	mov	r8d,DWORD PTR[((128+8))+rsp]
	mov	rsi,rdi

	dec	r8d
	jnz	$L$oop_sqr

	lea	rax,QWORD PTR[((128+24+48))+rsp]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$sqr_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_sqr::
rsaz_512_sqr	ENDP
PUBLIC	rsaz_512_mul

ALIGN	32
rsaz_512_mul	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_512_mul::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	sub	rsp,128+24
$L$mul_body::
DB	102,72,15,110,199
DB	102,72,15,110,201
	mov	QWORD PTR[128+rsp],r8
	mov	rbx,QWORD PTR[rdx]
	mov	rbp,rdx
	call	__rsaz_512_mul

DB	102,72,15,126,199
DB	102,72,15,126,205

	mov	r8,QWORD PTR[rsp]
	mov	r9,QWORD PTR[8+rsp]
	mov	r10,QWORD PTR[16+rsp]
	mov	r11,QWORD PTR[24+rsp]
	mov	r12,QWORD PTR[32+rsp]
	mov	r13,QWORD PTR[40+rsp]
	mov	r14,QWORD PTR[48+rsp]
	mov	r15,QWORD PTR[56+rsp]

	call	__rsaz_512_reduce
	add	r8,QWORD PTR[64+rsp]
	adc	r9,QWORD PTR[72+rsp]
	adc	r10,QWORD PTR[80+rsp]
	adc	r11,QWORD PTR[88+rsp]
	adc	r12,QWORD PTR[96+rsp]
	adc	r13,QWORD PTR[104+rsp]
	adc	r14,QWORD PTR[112+rsp]
	adc	r15,QWORD PTR[120+rsp]
	sbb	rcx,rcx

	call	__rsaz_512_subtract

	lea	rax,QWORD PTR[((128+24+48))+rsp]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$mul_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_mul::
rsaz_512_mul	ENDP
PUBLIC	rsaz_512_mul_gather4

ALIGN	32
rsaz_512_mul_gather4	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_512_mul_gather4::
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
	sub	rsp,128+24
$L$mul_gather4_body::
	mov	eax,DWORD PTR[64+r9*4+rdx]
DB	102,72,15,110,199
	mov	ebx,DWORD PTR[r9*4+rdx]
DB	102,72,15,110,201
	mov	QWORD PTR[128+rsp],r8

	shl	rax,32
	or	rbx,rax
	mov	rax,QWORD PTR[rsi]
	mov	rcx,QWORD PTR[8+rsi]
	lea	rbp,QWORD PTR[128+r9*4+rdx]
	mul	rbx
	mov	QWORD PTR[rsp],rax
	mov	rax,rcx
	mov	r8,rdx

	mul	rbx
	movd	xmm4,DWORD PTR[rbp]
	add	r8,rax
	mov	rax,QWORD PTR[16+rsi]
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	movd	xmm5,DWORD PTR[64+rbp]
	add	r9,rax
	mov	rax,QWORD PTR[24+rsi]
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	pslldq	xmm5,4
	add	r10,rax
	mov	rax,QWORD PTR[32+rsi]
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	por	xmm4,xmm5
	add	r11,rax
	mov	rax,QWORD PTR[40+rsi]
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[48+rsi]
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	lea	rbp,QWORD PTR[128+rbp]
	add	r13,rax
	mov	rax,QWORD PTR[56+rsi]
	mov	r14,rdx
	adc	r14,0

	mul	rbx
DB	102,72,15,126,227
	add	r14,rax
	mov	rax,QWORD PTR[rsi]
	mov	r15,rdx
	adc	r15,0

	lea	rdi,QWORD PTR[8+rsp]
	mov	ecx,7
	jmp	$L$oop_mul_gather

ALIGN	32
$L$oop_mul_gather::
	mul	rbx
	add	r8,rax
	mov	rax,QWORD PTR[8+rsi]
	mov	QWORD PTR[rdi],r8
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	movd	xmm4,DWORD PTR[rbp]
	add	r9,rax
	mov	rax,QWORD PTR[16+rsi]
	adc	rdx,0
	add	r8,r9
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	movd	xmm5,DWORD PTR[64+rbp]
	add	r10,rax
	mov	rax,QWORD PTR[24+rsi]
	adc	rdx,0
	add	r9,r10
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	pslldq	xmm5,4
	add	r11,rax
	mov	rax,QWORD PTR[32+rsi]
	adc	rdx,0
	add	r10,r11
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	por	xmm4,xmm5
	add	r12,rax
	mov	rax,QWORD PTR[40+rsi]
	adc	rdx,0
	add	r11,r12
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD PTR[48+rsi]
	adc	rdx,0
	add	r12,r13
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	add	r13,r14
	mov	r14,rdx
	adc	r14,0

	mul	rbx
DB	102,72,15,126,227
	add	r15,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	r14,r15
	mov	r15,rdx
	adc	r15,0

	lea	rbp,QWORD PTR[128+rbp]
	lea	rdi,QWORD PTR[8+rdi]

	dec	ecx
	jnz	$L$oop_mul_gather

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

DB	102,72,15,126,199
DB	102,72,15,126,205

	mov	r8,QWORD PTR[rsp]
	mov	r9,QWORD PTR[8+rsp]
	mov	r10,QWORD PTR[16+rsp]
	mov	r11,QWORD PTR[24+rsp]
	mov	r12,QWORD PTR[32+rsp]
	mov	r13,QWORD PTR[40+rsp]
	mov	r14,QWORD PTR[48+rsp]
	mov	r15,QWORD PTR[56+rsp]

	call	__rsaz_512_reduce
	add	r8,QWORD PTR[64+rsp]
	adc	r9,QWORD PTR[72+rsp]
	adc	r10,QWORD PTR[80+rsp]
	adc	r11,QWORD PTR[88+rsp]
	adc	r12,QWORD PTR[96+rsp]
	adc	r13,QWORD PTR[104+rsp]
	adc	r14,QWORD PTR[112+rsp]
	adc	r15,QWORD PTR[120+rsp]
	sbb	rcx,rcx

	call	__rsaz_512_subtract

	lea	rax,QWORD PTR[((128+24+48))+rsp]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$mul_gather4_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_mul_gather4::
rsaz_512_mul_gather4	ENDP
PUBLIC	rsaz_512_mul_scatter4

ALIGN	32
rsaz_512_mul_scatter4	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_512_mul_scatter4::
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
	sub	rsp,128+24
$L$mul_scatter4_body::
	lea	r8,QWORD PTR[r9*4+r8]
DB	102,72,15,110,199
DB	102,72,15,110,202
DB	102,73,15,110,208
	mov	QWORD PTR[128+rsp],rcx

	mov	rbp,rdi
	mov	rbx,QWORD PTR[rdi]
	call	__rsaz_512_mul

DB	102,72,15,126,199
DB	102,72,15,126,205

	mov	r8,QWORD PTR[rsp]
	mov	r9,QWORD PTR[8+rsp]
	mov	r10,QWORD PTR[16+rsp]
	mov	r11,QWORD PTR[24+rsp]
	mov	r12,QWORD PTR[32+rsp]
	mov	r13,QWORD PTR[40+rsp]
	mov	r14,QWORD PTR[48+rsp]
	mov	r15,QWORD PTR[56+rsp]

	call	__rsaz_512_reduce
	add	r8,QWORD PTR[64+rsp]
	adc	r9,QWORD PTR[72+rsp]
	adc	r10,QWORD PTR[80+rsp]
	adc	r11,QWORD PTR[88+rsp]
	adc	r12,QWORD PTR[96+rsp]
	adc	r13,QWORD PTR[104+rsp]
	adc	r14,QWORD PTR[112+rsp]
	adc	r15,QWORD PTR[120+rsp]
DB	102,72,15,126,214
	sbb	rcx,rcx

	call	__rsaz_512_subtract

	mov	DWORD PTR[rsi],r8d
	shr	r8,32
	mov	DWORD PTR[128+rsi],r9d
	shr	r9,32
	mov	DWORD PTR[256+rsi],r10d
	shr	r10,32
	mov	DWORD PTR[384+rsi],r11d
	shr	r11,32
	mov	DWORD PTR[512+rsi],r12d
	shr	r12,32
	mov	DWORD PTR[640+rsi],r13d
	shr	r13,32
	mov	DWORD PTR[768+rsi],r14d
	shr	r14,32
	mov	DWORD PTR[896+rsi],r15d
	shr	r15,32
	mov	DWORD PTR[64+rsi],r8d
	mov	DWORD PTR[192+rsi],r9d
	mov	DWORD PTR[320+rsi],r10d
	mov	DWORD PTR[448+rsi],r11d
	mov	DWORD PTR[576+rsi],r12d
	mov	DWORD PTR[704+rsi],r13d
	mov	DWORD PTR[832+rsi],r14d
	mov	DWORD PTR[960+rsi],r15d

	lea	rax,QWORD PTR[((128+24+48))+rsp]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$mul_scatter4_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_mul_scatter4::
rsaz_512_mul_scatter4	ENDP
PUBLIC	rsaz_512_mul_by_one

ALIGN	32
rsaz_512_mul_by_one	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_rsaz_512_mul_by_one::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	sub	rsp,128+24
$L$mul_by_one_body::
	mov	rbp,rdx
	mov	QWORD PTR[128+rsp],rcx

	mov	r8,QWORD PTR[rsi]
	pxor	xmm0,xmm0
	mov	r9,QWORD PTR[8+rsi]
	mov	r10,QWORD PTR[16+rsi]
	mov	r11,QWORD PTR[24+rsi]
	mov	r12,QWORD PTR[32+rsi]
	mov	r13,QWORD PTR[40+rsi]
	mov	r14,QWORD PTR[48+rsi]
	mov	r15,QWORD PTR[56+rsi]

	movdqa	XMMWORD PTR[rsp],xmm0
	movdqa	XMMWORD PTR[16+rsp],xmm0
	movdqa	XMMWORD PTR[32+rsp],xmm0
	movdqa	XMMWORD PTR[48+rsp],xmm0
	movdqa	XMMWORD PTR[64+rsp],xmm0
	movdqa	XMMWORD PTR[80+rsp],xmm0
	movdqa	XMMWORD PTR[96+rsp],xmm0
	call	__rsaz_512_reduce
	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

	lea	rax,QWORD PTR[((128+24+48))+rsp]
	mov	r15,QWORD PTR[((-48))+rax]
	mov	r14,QWORD PTR[((-40))+rax]
	mov	r13,QWORD PTR[((-32))+rax]
	mov	r12,QWORD PTR[((-24))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$mul_by_one_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_mul_by_one::
rsaz_512_mul_by_one	ENDP

ALIGN	32
__rsaz_512_reduce	PROC PRIVATE
	mov	rbx,r8
	imul	rbx,QWORD PTR[((128+8))+rsp]
	mov	rax,QWORD PTR[rbp]
	mov	ecx,8
	jmp	$L$reduction_loop

ALIGN	32
$L$reduction_loop::
	mul	rbx
	mov	rax,QWORD PTR[8+rbp]
	neg	r8
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD PTR[16+rbp]
	adc	rdx,0
	add	r8,r9
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[24+rbp]
	adc	rdx,0
	add	r9,r10
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[32+rbp]
	adc	rdx,0
	add	r10,r11
	mov	rsi,QWORD PTR[((128+8))+rsp]


	adc	rdx,0
	mov	r11,rdx

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[40+rbp]
	adc	rdx,0
	imul	rsi,r8
	add	r11,r12
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD PTR[48+rbp]
	adc	rdx,0
	add	r12,r13
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD PTR[56+rbp]
	adc	rdx,0
	add	r13,r14
	mov	r14,rdx
	adc	r14,0

	mul	rbx
	mov	rbx,rsi
	add	r15,rax
	mov	rax,QWORD PTR[rbp]
	adc	rdx,0
	add	r14,r15
	mov	r15,rdx
	adc	r15,0

	dec	ecx
	jne	$L$reduction_loop

	DB	0F3h,0C3h		;repret
__rsaz_512_reduce	ENDP

ALIGN	32
__rsaz_512_subtract	PROC PRIVATE
	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

	mov	r8,QWORD PTR[rbp]
	mov	r9,QWORD PTR[8+rbp]
	neg	r8
	not	r9
	and	r8,rcx
	mov	r10,QWORD PTR[16+rbp]
	and	r9,rcx
	not	r10
	mov	r11,QWORD PTR[24+rbp]
	and	r10,rcx
	not	r11
	mov	r12,QWORD PTR[32+rbp]
	and	r11,rcx
	not	r12
	mov	r13,QWORD PTR[40+rbp]
	and	r12,rcx
	not	r13
	mov	r14,QWORD PTR[48+rbp]
	and	r13,rcx
	not	r14
	mov	r15,QWORD PTR[56+rbp]
	and	r14,rcx
	not	r15
	and	r15,rcx

	add	r8,QWORD PTR[rdi]
	adc	r9,QWORD PTR[8+rdi]
	adc	r10,QWORD PTR[16+rdi]
	adc	r11,QWORD PTR[24+rdi]
	adc	r12,QWORD PTR[32+rdi]
	adc	r13,QWORD PTR[40+rdi]
	adc	r14,QWORD PTR[48+rdi]
	adc	r15,QWORD PTR[56+rdi]

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

	DB	0F3h,0C3h		;repret
__rsaz_512_subtract	ENDP

ALIGN	32
__rsaz_512_mul	PROC PRIVATE
	lea	rdi,QWORD PTR[8+rsp]

	mov	rax,QWORD PTR[rsi]
	mul	rbx
	mov	QWORD PTR[rdi],rax
	mov	rax,QWORD PTR[8+rsi]
	mov	r8,rdx

	mul	rbx
	add	r8,rax
	mov	rax,QWORD PTR[16+rsi]
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD PTR[24+rsi]
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[32+rsi]
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[40+rsi]
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[48+rsi]
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD PTR[56+rsi]
	mov	r14,rdx
	adc	r14,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD PTR[rsi]
	mov	r15,rdx
	adc	r15,0

	lea	rbp,QWORD PTR[8+rbp]
	lea	rdi,QWORD PTR[8+rdi]

	mov	ecx,7
	jmp	$L$oop_mul

ALIGN	32
$L$oop_mul::
	mov	rbx,QWORD PTR[rbp]
	mul	rbx
	add	r8,rax
	mov	rax,QWORD PTR[8+rsi]
	mov	QWORD PTR[rdi],r8
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD PTR[16+rsi]
	adc	rdx,0
	add	r8,r9
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[24+rsi]
	adc	rdx,0
	add	r9,r10
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[32+rsi]
	adc	rdx,0
	add	r10,r11
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[40+rsi]
	adc	rdx,0
	add	r11,r12
	mov	r12,rdx
	adc	r12,0

	mul	rbx
	add	r13,rax
	mov	rax,QWORD PTR[48+rsi]
	adc	rdx,0
	add	r12,r13
	mov	r13,rdx
	adc	r13,0

	mul	rbx
	add	r14,rax
	mov	rax,QWORD PTR[56+rsi]
	adc	rdx,0
	add	r13,r14
	mov	r14,rdx
	lea	rbp,QWORD PTR[8+rbp]
	adc	r14,0

	mul	rbx
	add	r15,rax
	mov	rax,QWORD PTR[rsi]
	adc	rdx,0
	add	r14,r15
	mov	r15,rdx
	adc	r15,0

	lea	rdi,QWORD PTR[8+rdi]

	dec	ecx
	jnz	$L$oop_mul

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

	DB	0F3h,0C3h		;repret
__rsaz_512_mul	ENDP
PUBLIC	rsaz_512_scatter4

ALIGN	16
rsaz_512_scatter4	PROC PUBLIC
	lea	rcx,QWORD PTR[r8*4+rcx]
	mov	r9d,8
	jmp	$L$oop_scatter
ALIGN	16
$L$oop_scatter::
	mov	rax,QWORD PTR[rdx]
	lea	rdx,QWORD PTR[8+rdx]
	mov	DWORD PTR[rcx],eax
	shr	rax,32
	mov	DWORD PTR[64+rcx],eax
	lea	rcx,QWORD PTR[128+rcx]
	dec	r9d
	jnz	$L$oop_scatter
	DB	0F3h,0C3h		;repret
rsaz_512_scatter4	ENDP

PUBLIC	rsaz_512_gather4

ALIGN	16
rsaz_512_gather4	PROC PUBLIC
	lea	rdx,QWORD PTR[r8*4+rdx]
	mov	r9d,8
	jmp	$L$oop_gather
ALIGN	16
$L$oop_gather::
	mov	eax,DWORD PTR[rdx]
	mov	r8d,DWORD PTR[64+rdx]
	lea	rdx,QWORD PTR[128+rdx]
	shl	r8,32
	or	rax,r8
	mov	QWORD PTR[rcx],rax
	lea	rcx,QWORD PTR[8+rcx]
	dec	r9d
	jnz	$L$oop_gather
	DB	0F3h,0C3h		;repret
rsaz_512_gather4	ENDP
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

	lea	rax,QWORD PTR[((128+24+48))+rax]

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
se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_rsaz_512_sqr
	DD	imagerel $L$SEH_end_rsaz_512_sqr
	DD	imagerel $L$SEH_info_rsaz_512_sqr

	DD	imagerel $L$SEH_begin_rsaz_512_mul
	DD	imagerel $L$SEH_end_rsaz_512_mul
	DD	imagerel $L$SEH_info_rsaz_512_mul

	DD	imagerel $L$SEH_begin_rsaz_512_mul_gather4
	DD	imagerel $L$SEH_end_rsaz_512_mul_gather4
	DD	imagerel $L$SEH_info_rsaz_512_mul_gather4

	DD	imagerel $L$SEH_begin_rsaz_512_mul_scatter4
	DD	imagerel $L$SEH_end_rsaz_512_mul_scatter4
	DD	imagerel $L$SEH_info_rsaz_512_mul_scatter4

	DD	imagerel $L$SEH_begin_rsaz_512_mul_by_one
	DD	imagerel $L$SEH_end_rsaz_512_mul_by_one
	DD	imagerel $L$SEH_info_rsaz_512_mul_by_one

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_rsaz_512_sqr::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$sqr_body,imagerel $L$sqr_epilogue
$L$SEH_info_rsaz_512_mul::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$mul_body,imagerel $L$mul_epilogue
$L$SEH_info_rsaz_512_mul_gather4::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$mul_gather4_body,imagerel $L$mul_gather4_epilogue
$L$SEH_info_rsaz_512_mul_scatter4::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$mul_scatter4_body,imagerel $L$mul_scatter4_epilogue
$L$SEH_info_rsaz_512_mul_by_one::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$mul_by_one_body,imagerel $L$mul_by_one_epilogue

.xdata	ENDS
END
