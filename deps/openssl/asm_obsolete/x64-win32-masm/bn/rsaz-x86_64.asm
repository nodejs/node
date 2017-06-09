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

	sub	rsp,328
	movaps	XMMWORD PTR[160+rsp],xmm6
	movaps	XMMWORD PTR[176+rsp],xmm7
	movaps	XMMWORD PTR[192+rsp],xmm8
	movaps	XMMWORD PTR[208+rsp],xmm9
	movaps	XMMWORD PTR[224+rsp],xmm10
	movaps	XMMWORD PTR[240+rsp],xmm11
	movaps	XMMWORD PTR[256+rsp],xmm12
	movaps	XMMWORD PTR[272+rsp],xmm13
	movaps	XMMWORD PTR[288+rsp],xmm14
	movaps	XMMWORD PTR[304+rsp],xmm15
$L$mul_gather4_body::
	movd	xmm8,r9d
	movdqa	xmm1,XMMWORD PTR[(($L$inc+16))]
	movdqa	xmm0,XMMWORD PTR[$L$inc]

	pshufd	xmm8,xmm8,0
	movdqa	xmm7,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm8
	movdqa	xmm3,xmm7
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm8
	movdqa	xmm4,xmm7
	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm8
	movdqa	xmm5,xmm7
	paddd	xmm4,xmm3
	pcmpeqd	xmm3,xmm8
	movdqa	xmm6,xmm7
	paddd	xmm5,xmm4
	pcmpeqd	xmm4,xmm8
	paddd	xmm6,xmm5
	pcmpeqd	xmm5,xmm8
	paddd	xmm7,xmm6
	pcmpeqd	xmm6,xmm8
	pcmpeqd	xmm7,xmm8

	movdqa	xmm8,XMMWORD PTR[rdx]
	movdqa	xmm9,XMMWORD PTR[16+rdx]
	movdqa	xmm10,XMMWORD PTR[32+rdx]
	movdqa	xmm11,XMMWORD PTR[48+rdx]
	pand	xmm8,xmm0
	movdqa	xmm12,XMMWORD PTR[64+rdx]
	pand	xmm9,xmm1
	movdqa	xmm13,XMMWORD PTR[80+rdx]
	pand	xmm10,xmm2
	movdqa	xmm14,XMMWORD PTR[96+rdx]
	pand	xmm11,xmm3
	movdqa	xmm15,XMMWORD PTR[112+rdx]
	lea	rbp,QWORD PTR[128+rdx]
	pand	xmm12,xmm4
	pand	xmm13,xmm5
	pand	xmm14,xmm6
	pand	xmm15,xmm7
	por	xmm8,xmm10
	por	xmm9,xmm11
	por	xmm8,xmm12
	por	xmm9,xmm13
	por	xmm8,xmm14
	por	xmm9,xmm15

	por	xmm8,xmm9
	pshufd	xmm9,xmm8,04eh
	por	xmm8,xmm9
DB	102,76,15,126,195

	mov	QWORD PTR[128+rsp],r8
	mov	QWORD PTR[((128+8))+rsp],rdi
	mov	QWORD PTR[((128+16))+rsp],rcx

	mov	rax,QWORD PTR[rsi]
	mov	rcx,QWORD PTR[8+rsi]
	mul	rbx
	mov	QWORD PTR[rsp],rax
	mov	rax,rcx
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

	lea	rdi,QWORD PTR[8+rsp]
	mov	ecx,7
	jmp	$L$oop_mul_gather

ALIGN	32
$L$oop_mul_gather::
	movdqa	xmm8,XMMWORD PTR[rbp]
	movdqa	xmm9,XMMWORD PTR[16+rbp]
	movdqa	xmm10,XMMWORD PTR[32+rbp]
	movdqa	xmm11,XMMWORD PTR[48+rbp]
	pand	xmm8,xmm0
	movdqa	xmm12,XMMWORD PTR[64+rbp]
	pand	xmm9,xmm1
	movdqa	xmm13,XMMWORD PTR[80+rbp]
	pand	xmm10,xmm2
	movdqa	xmm14,XMMWORD PTR[96+rbp]
	pand	xmm11,xmm3
	movdqa	xmm15,XMMWORD PTR[112+rbp]
	lea	rbp,QWORD PTR[128+rbp]
	pand	xmm12,xmm4
	pand	xmm13,xmm5
	pand	xmm14,xmm6
	pand	xmm15,xmm7
	por	xmm8,xmm10
	por	xmm9,xmm11
	por	xmm8,xmm12
	por	xmm9,xmm13
	por	xmm8,xmm14
	por	xmm9,xmm15

	por	xmm8,xmm9
	pshufd	xmm9,xmm8,04eh
	por	xmm8,xmm9
DB	102,76,15,126,195

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
	jnz	$L$oop_mul_gather

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15

	mov	rdi,QWORD PTR[((128+8))+rsp]
	mov	rbp,QWORD PTR[((128+16))+rsp]

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
	movaps	xmm6,XMMWORD PTR[((160-200))+rax]
	movaps	xmm7,XMMWORD PTR[((176-200))+rax]
	movaps	xmm8,XMMWORD PTR[((192-200))+rax]
	movaps	xmm9,XMMWORD PTR[((208-200))+rax]
	movaps	xmm10,XMMWORD PTR[((224-200))+rax]
	movaps	xmm11,XMMWORD PTR[((240-200))+rax]
	movaps	xmm12,XMMWORD PTR[((256-200))+rax]
	movaps	xmm13,XMMWORD PTR[((272-200))+rax]
	movaps	xmm14,XMMWORD PTR[((288-200))+rax]
	movaps	xmm15,XMMWORD PTR[((304-200))+rax]
	lea	rax,QWORD PTR[176+rax]
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
	lea	r8,QWORD PTR[r9*8+r8]
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

	mov	QWORD PTR[rsi],r8
	mov	QWORD PTR[128+rsi],r9
	mov	QWORD PTR[256+rsi],r10
	mov	QWORD PTR[384+rsi],r11
	mov	QWORD PTR[512+rsi],r12
	mov	QWORD PTR[640+rsi],r13
	mov	QWORD PTR[768+rsi],r14
	mov	QWORD PTR[896+rsi],r15

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
	lea	rcx,QWORD PTR[r8*8+rcx]
	mov	r9d,8
	jmp	$L$oop_scatter
ALIGN	16
$L$oop_scatter::
	mov	rax,QWORD PTR[rdx]
	lea	rdx,QWORD PTR[8+rdx]
	mov	QWORD PTR[rcx],rax
	lea	rcx,QWORD PTR[128+rcx]
	dec	r9d
	jnz	$L$oop_scatter
	DB	0F3h,0C3h		;repret
rsaz_512_scatter4	ENDP

PUBLIC	rsaz_512_gather4

ALIGN	16
rsaz_512_gather4	PROC PUBLIC
$L$SEH_begin_rsaz_512_gather4::
DB	048h,081h,0ech,0a8h,000h,000h,000h
DB	00fh,029h,034h,024h
DB	00fh,029h,07ch,024h,010h
DB	044h,00fh,029h,044h,024h,020h
DB	044h,00fh,029h,04ch,024h,030h
DB	044h,00fh,029h,054h,024h,040h
DB	044h,00fh,029h,05ch,024h,050h
DB	044h,00fh,029h,064h,024h,060h
DB	044h,00fh,029h,06ch,024h,070h
DB	044h,00fh,029h,0b4h,024h,080h,0,0,0
DB	044h,00fh,029h,0bch,024h,090h,0,0,0
	movd	xmm8,r8d
	movdqa	xmm1,XMMWORD PTR[(($L$inc+16))]
	movdqa	xmm0,XMMWORD PTR[$L$inc]

	pshufd	xmm8,xmm8,0
	movdqa	xmm7,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm8
	movdqa	xmm3,xmm7
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm8
	movdqa	xmm4,xmm7
	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm8
	movdqa	xmm5,xmm7
	paddd	xmm4,xmm3
	pcmpeqd	xmm3,xmm8
	movdqa	xmm6,xmm7
	paddd	xmm5,xmm4
	pcmpeqd	xmm4,xmm8
	paddd	xmm6,xmm5
	pcmpeqd	xmm5,xmm8
	paddd	xmm7,xmm6
	pcmpeqd	xmm6,xmm8
	pcmpeqd	xmm7,xmm8
	mov	r9d,8
	jmp	$L$oop_gather
ALIGN	16
$L$oop_gather::
	movdqa	xmm8,XMMWORD PTR[rdx]
	movdqa	xmm9,XMMWORD PTR[16+rdx]
	movdqa	xmm10,XMMWORD PTR[32+rdx]
	movdqa	xmm11,XMMWORD PTR[48+rdx]
	pand	xmm8,xmm0
	movdqa	xmm12,XMMWORD PTR[64+rdx]
	pand	xmm9,xmm1
	movdqa	xmm13,XMMWORD PTR[80+rdx]
	pand	xmm10,xmm2
	movdqa	xmm14,XMMWORD PTR[96+rdx]
	pand	xmm11,xmm3
	movdqa	xmm15,XMMWORD PTR[112+rdx]
	lea	rdx,QWORD PTR[128+rdx]
	pand	xmm12,xmm4
	pand	xmm13,xmm5
	pand	xmm14,xmm6
	pand	xmm15,xmm7
	por	xmm8,xmm10
	por	xmm9,xmm11
	por	xmm8,xmm12
	por	xmm9,xmm13
	por	xmm8,xmm14
	por	xmm9,xmm15

	por	xmm8,xmm9
	pshufd	xmm9,xmm8,04eh
	por	xmm8,xmm9
	movq	QWORD PTR[rcx],xmm8
	lea	rcx,QWORD PTR[8+rcx]
	dec	r9d
	jnz	$L$oop_gather
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	xmm10,XMMWORD PTR[64+rsp]
	movaps	xmm11,XMMWORD PTR[80+rsp]
	movaps	xmm12,XMMWORD PTR[96+rsp]
	movaps	xmm13,XMMWORD PTR[112+rsp]
	movaps	xmm14,XMMWORD PTR[128+rsp]
	movaps	xmm15,XMMWORD PTR[144+rsp]
	add	rsp,0a8h
	DB	0F3h,0C3h		;repret
$L$SEH_end_rsaz_512_gather4::
rsaz_512_gather4	ENDP

ALIGN	64
$L$inc::
	DD	0,0,1,1
	DD	2,2,2,2
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

	lea	rbx,QWORD PTR[$L$mul_gather4_epilogue]
	cmp	rbx,r10
	jne	$L$se_not_in_mul_gather4

	lea	rax,QWORD PTR[176+rax]

	lea	rsi,QWORD PTR[((-48-168))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$se_not_in_mul_gather4::
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

	DD	imagerel $L$SEH_begin_rsaz_512_gather4
	DD	imagerel $L$SEH_end_rsaz_512_gather4
	DD	imagerel $L$SEH_info_rsaz_512_gather4

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
$L$SEH_info_rsaz_512_gather4::
DB	001h,046h,016h,000h
DB	046h,0f8h,009h,000h
DB	03dh,0e8h,008h,000h
DB	034h,0d8h,007h,000h
DB	02eh,0c8h,006h,000h
DB	028h,0b8h,005h,000h
DB	022h,0a8h,004h,000h
DB	01ch,098h,003h,000h
DB	016h,088h,002h,000h
DB	010h,078h,001h,000h
DB	00bh,068h,000h,000h
DB	007h,001h,015h,000h

.xdata	ENDS
END
