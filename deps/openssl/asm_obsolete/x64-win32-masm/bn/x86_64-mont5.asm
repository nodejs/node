OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

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


	test	r9d,7
	jnz	$L$mul_enter
	jmp	$L$mul4x_enter

ALIGN	16
$L$mul_enter::
	mov	r9d,r9d
	mov	rax,rsp
	movd	xmm5,DWORD PTR[56+rsp]
	lea	r10,QWORD PTR[$L$inc]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	lea	r11,QWORD PTR[2+r9]
	neg	r11
	lea	rsp,QWORD PTR[((-264))+r11*8+rsp]
	and	rsp,-1024

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul_body::






	sub	rax,rsp
	and	rax,-4096
$L$mul_page_walk::
	mov	r11,QWORD PTR[rax*1+rsp]
	sub	rax,4096
DB	02eh
	jnc	$L$mul_page_walk

	lea	r12,QWORD PTR[128+rdx]
	movdqa	xmm0,XMMWORD PTR[r10]
	movdqa	xmm1,XMMWORD PTR[16+r10]
	lea	r10,QWORD PTR[((24-112))+r9*8+rsp]
	and	r10,-16

	pshufd	xmm5,xmm5,0
	movdqa	xmm4,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
DB	067h
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[112+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[128+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[144+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[160+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[176+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[192+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[208+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[224+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[240+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[256+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[272+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[288+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[304+r10],xmm0

	paddd	xmm3,xmm2
DB	067h
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[320+r10],xmm1

	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[336+r10],xmm2
	pand	xmm0,XMMWORD PTR[64+r12]

	pand	xmm1,XMMWORD PTR[80+r12]
	pand	xmm2,XMMWORD PTR[96+r12]
	movdqa	XMMWORD PTR[352+r10],xmm3
	pand	xmm3,XMMWORD PTR[112+r12]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[((-128))+r12]
	movdqa	xmm5,XMMWORD PTR[((-112))+r12]
	movdqa	xmm2,XMMWORD PTR[((-96))+r12]
	pand	xmm4,XMMWORD PTR[112+r10]
	movdqa	xmm3,XMMWORD PTR[((-80))+r12]
	pand	xmm5,XMMWORD PTR[128+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[144+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[160+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[((-64))+r12]
	movdqa	xmm5,XMMWORD PTR[((-48))+r12]
	movdqa	xmm2,XMMWORD PTR[((-32))+r12]
	pand	xmm4,XMMWORD PTR[176+r10]
	movdqa	xmm3,XMMWORD PTR[((-16))+r12]
	pand	xmm5,XMMWORD PTR[192+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[208+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[224+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[r12]
	movdqa	xmm5,XMMWORD PTR[16+r12]
	movdqa	xmm2,XMMWORD PTR[32+r12]
	pand	xmm4,XMMWORD PTR[240+r10]
	movdqa	xmm3,XMMWORD PTR[48+r12]
	pand	xmm5,XMMWORD PTR[256+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[272+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[288+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	por	xmm0,xmm1
	pshufd	xmm1,xmm0,04eh
	por	xmm0,xmm1
	lea	r12,QWORD PTR[256+r12]
DB	102,72,15,126,195

	mov	r8,QWORD PTR[r8]
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
	adc	rdx,0
	add	r13,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r9*8+rsp],r13
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
	lea	rdx,QWORD PTR[((24+128))+r9*8+rsp]
	and	rdx,-16
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD PTR[((-128))+r12]
	movdqa	xmm1,XMMWORD PTR[((-112))+r12]
	movdqa	xmm2,XMMWORD PTR[((-96))+r12]
	movdqa	xmm3,XMMWORD PTR[((-80))+r12]
	pand	xmm0,XMMWORD PTR[((-128))+rdx]
	pand	xmm1,XMMWORD PTR[((-112))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-96))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-80))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[((-64))+r12]
	movdqa	xmm1,XMMWORD PTR[((-48))+r12]
	movdqa	xmm2,XMMWORD PTR[((-32))+r12]
	movdqa	xmm3,XMMWORD PTR[((-16))+r12]
	pand	xmm0,XMMWORD PTR[((-64))+rdx]
	pand	xmm1,XMMWORD PTR[((-48))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-32))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-16))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[r12]
	movdqa	xmm1,XMMWORD PTR[16+r12]
	movdqa	xmm2,XMMWORD PTR[32+r12]
	movdqa	xmm3,XMMWORD PTR[48+r12]
	pand	xmm0,XMMWORD PTR[rdx]
	pand	xmm1,XMMWORD PTR[16+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[32+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[48+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[64+r12]
	movdqa	xmm1,XMMWORD PTR[80+r12]
	movdqa	xmm2,XMMWORD PTR[96+r12]
	movdqa	xmm3,XMMWORD PTR[112+r12]
	pand	xmm0,XMMWORD PTR[64+rdx]
	pand	xmm1,XMMWORD PTR[80+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[96+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[112+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
	pshufd	xmm0,xmm4,04eh
	por	xmm0,xmm4
	lea	r12,QWORD PTR[256+r12]

	mov	rax,QWORD PTR[rsi]
DB	102,72,15,126,195

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
	adc	rdx,0
	add	r13,r10
	mov	r10,QWORD PTR[r9*8+rsp]
	adc	rdx,0
	mov	QWORD PTR[((-16))+r9*8+rsp],r13
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
$L$SEH_end_bn_mul_mont_gather5::
bn_mul_mont_gather5	ENDP

ALIGN	32
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
DB	067h
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

DB	067h
	shl	r9d,3
	lea	r10,QWORD PTR[r9*2+r9]
	neg	r9










	lea	r11,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	$L$mul4xsp_alt
	sub	rsp,r11
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	jmp	$L$mul4xsp_done

ALIGN	32
$L$mul4xsp_alt::
	lea	r10,QWORD PTR[((4096-320))+r9*2]
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rsp,r11
$L$mul4xsp_done::
	and	rsp,-64
	mov	r11,rax
	sub	r11,rsp
	and	r11,-4096
$L$mul4x_page_walk::
	mov	r10,QWORD PTR[r11*1+rsp]
	sub	r11,4096
DB	02eh
	jnc	$L$mul4x_page_walk

	neg	r9

	mov	QWORD PTR[40+rsp],rax
$L$mul4x_body::

	call	mul4x_internal

	mov	rsi,QWORD PTR[40+rsp]
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
$L$SEH_end_bn_mul4x_mont_gather5::
bn_mul4x_mont_gather5	ENDP


ALIGN	32
mul4x_internal	PROC PRIVATE
	shl	r9,5
	movd	xmm5,DWORD PTR[56+rax]
	lea	rax,QWORD PTR[$L$inc]
	lea	r13,QWORD PTR[128+r9*1+rdx]
	shr	r9,5
	movdqa	xmm0,XMMWORD PTR[rax]
	movdqa	xmm1,XMMWORD PTR[16+rax]
	lea	r10,QWORD PTR[((88-112))+r9*1+rsp]
	lea	r12,QWORD PTR[128+rdx]

	pshufd	xmm5,xmm5,0
	movdqa	xmm4,xmm1
DB	067h,067h
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
DB	067h
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[112+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[128+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[144+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[160+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[176+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[192+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[208+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[224+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[240+r10],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[256+r10],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[272+r10],xmm2
	movdqa	xmm2,xmm4

	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[288+r10],xmm3
	movdqa	xmm3,xmm4
	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[304+r10],xmm0

	paddd	xmm3,xmm2
DB	067h
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[320+r10],xmm1

	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[336+r10],xmm2
	pand	xmm0,XMMWORD PTR[64+r12]

	pand	xmm1,XMMWORD PTR[80+r12]
	pand	xmm2,XMMWORD PTR[96+r12]
	movdqa	XMMWORD PTR[352+r10],xmm3
	pand	xmm3,XMMWORD PTR[112+r12]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[((-128))+r12]
	movdqa	xmm5,XMMWORD PTR[((-112))+r12]
	movdqa	xmm2,XMMWORD PTR[((-96))+r12]
	pand	xmm4,XMMWORD PTR[112+r10]
	movdqa	xmm3,XMMWORD PTR[((-80))+r12]
	pand	xmm5,XMMWORD PTR[128+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[144+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[160+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[((-64))+r12]
	movdqa	xmm5,XMMWORD PTR[((-48))+r12]
	movdqa	xmm2,XMMWORD PTR[((-32))+r12]
	pand	xmm4,XMMWORD PTR[176+r10]
	movdqa	xmm3,XMMWORD PTR[((-16))+r12]
	pand	xmm5,XMMWORD PTR[192+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[208+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[224+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	movdqa	xmm4,XMMWORD PTR[r12]
	movdqa	xmm5,XMMWORD PTR[16+r12]
	movdqa	xmm2,XMMWORD PTR[32+r12]
	pand	xmm4,XMMWORD PTR[240+r10]
	movdqa	xmm3,XMMWORD PTR[48+r12]
	pand	xmm5,XMMWORD PTR[256+r10]
	por	xmm0,xmm4
	pand	xmm2,XMMWORD PTR[272+r10]
	por	xmm1,xmm5
	pand	xmm3,XMMWORD PTR[288+r10]
	por	xmm0,xmm2
	por	xmm1,xmm3
	por	xmm0,xmm1
	pshufd	xmm1,xmm0,04eh
	por	xmm0,xmm1
	lea	r12,QWORD PTR[256+r12]
DB	102,72,15,126,195

	mov	QWORD PTR[((16+8))+rsp],r13
	mov	QWORD PTR[((56+8))+rsp],rdi

	mov	r8,QWORD PTR[r8]
	mov	rax,QWORD PTR[rsi]
	lea	rsi,QWORD PTR[r9*1+rsi]
	neg	r9

	mov	rbp,r8
	mul	rbx
	mov	r10,rax
	mov	rax,QWORD PTR[rcx]

	imul	rbp,r10
	lea	r14,QWORD PTR[((64+8))+rsp]
	mov	r11,rdx

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+r9*1+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+r9*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,QWORD PTR[32+r9]
	lea	rcx,QWORD PTR[32+rcx]
	adc	rdx,0
	mov	QWORD PTR[r14],rdi
	mov	r13,rdx
	jmp	$L$1st4x

ALIGN	32
$L$1st4x::
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+rcx]
	lea	r14,QWORD PTR[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-24))+r14],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r14],rdi
	mov	r13,rdx

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[8+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-8))+r14],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	rcx,QWORD PTR[32+rcx]
	adc	rdx,0
	mov	QWORD PTR[r14],rdi
	mov	r13,rdx

	add	r15,32
	jnz	$L$1st4x

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+rcx]
	lea	r14,QWORD PTR[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-24))+r14],r13
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+rcx]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r9*1+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-16))+r14],rdi
	mov	r13,rdx

	lea	rcx,QWORD PTR[r9*1+rcx]

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD PTR[((-8))+r14],r13

	jmp	$L$outer4x

ALIGN	32
$L$outer4x::
	lea	rdx,QWORD PTR[((16+128))+r14]
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD PTR[((-128))+r12]
	movdqa	xmm1,XMMWORD PTR[((-112))+r12]
	movdqa	xmm2,XMMWORD PTR[((-96))+r12]
	movdqa	xmm3,XMMWORD PTR[((-80))+r12]
	pand	xmm0,XMMWORD PTR[((-128))+rdx]
	pand	xmm1,XMMWORD PTR[((-112))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-96))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-80))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[((-64))+r12]
	movdqa	xmm1,XMMWORD PTR[((-48))+r12]
	movdqa	xmm2,XMMWORD PTR[((-32))+r12]
	movdqa	xmm3,XMMWORD PTR[((-16))+r12]
	pand	xmm0,XMMWORD PTR[((-64))+rdx]
	pand	xmm1,XMMWORD PTR[((-48))+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-32))+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-16))+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[r12]
	movdqa	xmm1,XMMWORD PTR[16+r12]
	movdqa	xmm2,XMMWORD PTR[32+r12]
	movdqa	xmm3,XMMWORD PTR[48+r12]
	pand	xmm0,XMMWORD PTR[rdx]
	pand	xmm1,XMMWORD PTR[16+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[32+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[48+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[64+r12]
	movdqa	xmm1,XMMWORD PTR[80+r12]
	movdqa	xmm2,XMMWORD PTR[96+r12]
	movdqa	xmm3,XMMWORD PTR[112+r12]
	pand	xmm0,XMMWORD PTR[64+rdx]
	pand	xmm1,XMMWORD PTR[80+rdx]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[96+rdx]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[112+rdx]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
	pshufd	xmm0,xmm4,04eh
	por	xmm0,xmm4
	lea	r12,QWORD PTR[256+r12]
DB	102,72,15,126,195

	mov	r10,QWORD PTR[r9*1+r14]
	mov	rbp,r8
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0

	imul	rbp,r10
	mov	r11,rdx
	mov	QWORD PTR[r14],rdi

	lea	r14,QWORD PTR[r9*1+r14]

	mul	rbp
	add	r10,rax
	mov	rax,QWORD PTR[8+r9*1+rsi]
	adc	rdx,0
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[8+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+r9*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	r15,QWORD PTR[32+r9]
	lea	rcx,QWORD PTR[32+rcx]
	adc	rdx,0
	mov	r13,rdx
	jmp	$L$inner4x

ALIGN	32
$L$inner4x::
	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+rcx]
	adc	rdx,0
	add	r10,QWORD PTR[16+r14]
	lea	r14,QWORD PTR[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-32))+r14],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[((-8))+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[((-8))+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-24))+r14],r13
	mov	r13,rdx

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[rcx]
	adc	rdx,0
	add	r10,QWORD PTR[r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[8+r15*1+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-16))+r14],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[8+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[8+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[16+r15*1+rsi]
	adc	rdx,0
	add	rdi,r11
	lea	rcx,QWORD PTR[32+rcx]
	adc	rdx,0
	mov	QWORD PTR[((-8))+r14],r13
	mov	r13,rdx

	add	r15,32
	jnz	$L$inner4x

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[((-16))+rcx]
	adc	rdx,0
	add	r10,QWORD PTR[16+r14]
	lea	r14,QWORD PTR[32+r14]
	adc	rdx,0
	mov	r11,rdx

	mul	rbp
	add	r13,rax
	mov	rax,QWORD PTR[((-8))+rsi]
	adc	rdx,0
	add	r13,r10
	adc	rdx,0
	mov	QWORD PTR[((-32))+r14],rdi
	mov	rdi,rdx

	mul	rbx
	add	r11,rax
	mov	rax,rbp
	mov	rbp,QWORD PTR[((-8))+rcx]
	adc	rdx,0
	add	r11,QWORD PTR[((-8))+r14]
	adc	rdx,0
	mov	r10,rdx

	mul	rbp
	add	rdi,rax
	mov	rax,QWORD PTR[r9*1+rsi]
	adc	rdx,0
	add	rdi,r11
	adc	rdx,0
	mov	QWORD PTR[((-24))+r14],r13
	mov	r13,rdx

	mov	QWORD PTR[((-16))+r14],rdi
	lea	rcx,QWORD PTR[r9*1+rcx]

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	add	r13,QWORD PTR[r14]
	adc	rdi,0
	mov	QWORD PTR[((-8))+r14],r13

	cmp	r12,QWORD PTR[((16+8))+rsp]
	jb	$L$outer4x
	xor	rax,rax
	sub	rbp,r13
	adc	r15,r15
	or	rdi,r15
	sub	rax,rdi
	lea	rbx,QWORD PTR[r9*1+r14]
	mov	r12,QWORD PTR[rcx]
	lea	rbp,QWORD PTR[rcx]
	mov	rcx,r9
	sar	rcx,3+2
	mov	rdi,QWORD PTR[((56+8))+rsp]
	dec	r12
	xor	r10,r10
	mov	r13,QWORD PTR[8+rbp]
	mov	r14,QWORD PTR[16+rbp]
	mov	r15,QWORD PTR[24+rbp]
	jmp	$L$sqr4x_sub_entry
mul4x_internal	ENDP
PUBLIC	bn_power5

ALIGN	32
bn_power5	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_power5::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	shl	r9d,3
	lea	r10d,DWORD PTR[r9*2+r9]
	neg	r9
	mov	r8,QWORD PTR[r8]








	lea	r11,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	$L$pwr_sp_alt
	sub	rsp,r11
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	jmp	$L$pwr_sp_done

ALIGN	32
$L$pwr_sp_alt::
	lea	r10,QWORD PTR[((4096-320))+r9*2]
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rsp,r11
$L$pwr_sp_done::
	and	rsp,-64
	mov	r11,rax
	sub	r11,rsp
	and	r11,-4096
$L$pwr_page_walk::
	mov	r10,QWORD PTR[r11*1+rsp]
	sub	r11,4096
DB	02eh
	jnc	$L$pwr_page_walk

	mov	r10,r9
	neg	r9










	mov	QWORD PTR[32+rsp],r8
	mov	QWORD PTR[40+rsp],rax
$L$power5_body::
DB	102,72,15,110,207
DB	102,72,15,110,209
DB	102,73,15,110,218
DB	102,72,15,110,226

	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal
	call	__bn_sqr8x_internal
	call	__bn_post4x_internal

DB	102,72,15,126,209
DB	102,72,15,126,226
	mov	rdi,rsi
	mov	rax,QWORD PTR[40+rsp]
	lea	r8,QWORD PTR[32+rsp]

	call	mul4x_internal

	mov	rsi,QWORD PTR[40+rsp]
	mov	rax,1
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$power5_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_power5::
bn_power5	ENDP

PUBLIC	bn_sqr8x_internal


ALIGN	32
bn_sqr8x_internal	PROC PUBLIC
__bn_sqr8x_internal::









































































	lea	rbp,QWORD PTR[32+r10]
	lea	rsi,QWORD PTR[r9*1+rsi]

	mov	rcx,r9


	mov	r14,QWORD PTR[((-32))+rbp*1+rsi]
	lea	rdi,QWORD PTR[((48+8))+r9*2+rsp]
	mov	rax,QWORD PTR[((-24))+rbp*1+rsi]
	lea	rdi,QWORD PTR[((-32))+rbp*1+rdi]
	mov	rbx,QWORD PTR[((-16))+rbp*1+rsi]
	mov	r15,rax

	mul	r14
	mov	r10,rax
	mov	rax,rbx
	mov	r11,rdx
	mov	QWORD PTR[((-24))+rbp*1+rdi],r10

	mul	r14
	add	r11,rax
	mov	rax,rbx
	adc	rdx,0
	mov	QWORD PTR[((-16))+rbp*1+rdi],r11
	mov	r10,rdx


	mov	rbx,QWORD PTR[((-8))+rbp*1+rsi]
	mul	r15
	mov	r12,rax
	mov	rax,rbx
	mov	r13,rdx

	lea	rcx,QWORD PTR[rbp]
	mul	r14
	add	r10,rax
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD PTR[((-8))+rcx*1+rdi],r10
	jmp	$L$sqr4x_1st

ALIGN	32
$L$sqr4x_1st::
	mov	rbx,QWORD PTR[rcx*1+rsi]
	mul	r15
	add	r13,rax
	mov	rax,rbx
	mov	r12,rdx
	adc	r12,0

	mul	r14
	add	r11,rax
	mov	rax,rbx
	mov	rbx,QWORD PTR[8+rcx*1+rsi]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0


	mul	r15
	add	r12,rax
	mov	rax,rbx
	mov	QWORD PTR[rcx*1+rdi],r11
	mov	r13,rdx
	adc	r13,0

	mul	r14
	add	r10,rax
	mov	rax,rbx
	mov	rbx,QWORD PTR[16+rcx*1+rsi]
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0

	mul	r15
	add	r13,rax
	mov	rax,rbx
	mov	QWORD PTR[8+rcx*1+rdi],r10
	mov	r12,rdx
	adc	r12,0

	mul	r14
	add	r11,rax
	mov	rax,rbx
	mov	rbx,QWORD PTR[24+rcx*1+rsi]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0


	mul	r15
	add	r12,rax
	mov	rax,rbx
	mov	QWORD PTR[16+rcx*1+rdi],r11
	mov	r13,rdx
	adc	r13,0
	lea	rcx,QWORD PTR[32+rcx]

	mul	r14
	add	r10,rax
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD PTR[((-8))+rcx*1+rdi],r10

	cmp	rcx,0
	jne	$L$sqr4x_1st

	mul	r15
	add	r13,rax
	lea	rbp,QWORD PTR[16+rbp]
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD PTR[rdi],r13
	mov	r12,rdx
	mov	QWORD PTR[8+rdi],rdx
	jmp	$L$sqr4x_outer

ALIGN	32
$L$sqr4x_outer::
	mov	r14,QWORD PTR[((-32))+rbp*1+rsi]
	lea	rdi,QWORD PTR[((48+8))+r9*2+rsp]
	mov	rax,QWORD PTR[((-24))+rbp*1+rsi]
	lea	rdi,QWORD PTR[((-32))+rbp*1+rdi]
	mov	rbx,QWORD PTR[((-16))+rbp*1+rsi]
	mov	r15,rax

	mul	r14
	mov	r10,QWORD PTR[((-24))+rbp*1+rdi]
	add	r10,rax
	mov	rax,rbx
	adc	rdx,0
	mov	QWORD PTR[((-24))+rbp*1+rdi],r10
	mov	r11,rdx

	mul	r14
	add	r11,rax
	mov	rax,rbx
	adc	rdx,0
	add	r11,QWORD PTR[((-16))+rbp*1+rdi]
	mov	r10,rdx
	adc	r10,0
	mov	QWORD PTR[((-16))+rbp*1+rdi],r11

	xor	r12,r12

	mov	rbx,QWORD PTR[((-8))+rbp*1+rsi]
	mul	r15
	add	r12,rax
	mov	rax,rbx
	adc	rdx,0
	add	r12,QWORD PTR[((-8))+rbp*1+rdi]
	mov	r13,rdx
	adc	r13,0

	mul	r14
	add	r10,rax
	mov	rax,rbx
	adc	rdx,0
	add	r10,r12
	mov	r11,rdx
	adc	r11,0
	mov	QWORD PTR[((-8))+rbp*1+rdi],r10

	lea	rcx,QWORD PTR[rbp]
	jmp	$L$sqr4x_inner

ALIGN	32
$L$sqr4x_inner::
	mov	rbx,QWORD PTR[rcx*1+rsi]
	mul	r15
	add	r13,rax
	mov	rax,rbx
	mov	r12,rdx
	adc	r12,0
	add	r13,QWORD PTR[rcx*1+rdi]
	adc	r12,0

DB	067h
	mul	r14
	add	r11,rax
	mov	rax,rbx
	mov	rbx,QWORD PTR[8+rcx*1+rsi]
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	adc	r10,0

	mul	r15
	add	r12,rax
	mov	QWORD PTR[rcx*1+rdi],r11
	mov	rax,rbx
	mov	r13,rdx
	adc	r13,0
	add	r12,QWORD PTR[8+rcx*1+rdi]
	lea	rcx,QWORD PTR[16+rcx]
	adc	r13,0

	mul	r14
	add	r10,rax
	mov	rax,rbx
	adc	rdx,0
	add	r10,r12
	mov	r11,rdx
	adc	r11,0
	mov	QWORD PTR[((-8))+rcx*1+rdi],r10

	cmp	rcx,0
	jne	$L$sqr4x_inner

DB	067h
	mul	r15
	add	r13,rax
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD PTR[rdi],r13
	mov	r12,rdx
	mov	QWORD PTR[8+rdi],rdx

	add	rbp,16
	jnz	$L$sqr4x_outer


	mov	r14,QWORD PTR[((-32))+rsi]
	lea	rdi,QWORD PTR[((48+8))+r9*2+rsp]
	mov	rax,QWORD PTR[((-24))+rsi]
	lea	rdi,QWORD PTR[((-32))+rbp*1+rdi]
	mov	rbx,QWORD PTR[((-16))+rsi]
	mov	r15,rax

	mul	r14
	add	r10,rax
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0

	mul	r14
	add	r11,rax
	mov	rax,rbx
	mov	QWORD PTR[((-24))+rdi],r10
	mov	r10,rdx
	adc	r10,0
	add	r11,r13
	mov	rbx,QWORD PTR[((-8))+rsi]
	adc	r10,0

	mul	r15
	add	r12,rax
	mov	rax,rbx
	mov	QWORD PTR[((-16))+rdi],r11
	mov	r13,rdx
	adc	r13,0

	mul	r14
	add	r10,rax
	mov	rax,rbx
	mov	r11,rdx
	adc	r11,0
	add	r10,r12
	adc	r11,0
	mov	QWORD PTR[((-8))+rdi],r10

	mul	r15
	add	r13,rax
	mov	rax,QWORD PTR[((-16))+rsi]
	adc	rdx,0
	add	r13,r11
	adc	rdx,0

	mov	QWORD PTR[rdi],r13
	mov	r12,rdx
	mov	QWORD PTR[8+rdi],rdx

	mul	rbx
	add	rbp,16
	xor	r14,r14
	sub	rbp,r9
	xor	r15,r15

	add	rax,r12
	adc	rdx,0
	mov	QWORD PTR[8+rdi],rax
	mov	QWORD PTR[16+rdi],rdx
	mov	QWORD PTR[24+rdi],r15

	mov	rax,QWORD PTR[((-16))+rbp*1+rsi]
	lea	rdi,QWORD PTR[((48+8))+rsp]
	xor	r10,r10
	mov	r11,QWORD PTR[8+rdi]

	lea	r12,QWORD PTR[r10*2+r14]
	shr	r10,63
	lea	r13,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r13,r10
	mov	r10,QWORD PTR[16+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[24+rdi]
	adc	r12,rax
	mov	rax,QWORD PTR[((-8))+rbp*1+rsi]
	mov	QWORD PTR[rdi],r12
	adc	r13,rdx

	lea	rbx,QWORD PTR[r10*2+r14]
	mov	QWORD PTR[8+rdi],r13
	sbb	r15,r15
	shr	r10,63
	lea	r8,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r8,r10
	mov	r10,QWORD PTR[32+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[40+rdi]
	adc	rbx,rax
	mov	rax,QWORD PTR[rbp*1+rsi]
	mov	QWORD PTR[16+rdi],rbx
	adc	r8,rdx
	lea	rbp,QWORD PTR[16+rbp]
	mov	QWORD PTR[24+rdi],r8
	sbb	r15,r15
	lea	rdi,QWORD PTR[64+rdi]
	jmp	$L$sqr4x_shift_n_add

ALIGN	32
$L$sqr4x_shift_n_add::
	lea	r12,QWORD PTR[r10*2+r14]
	shr	r10,63
	lea	r13,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r13,r10
	mov	r10,QWORD PTR[((-16))+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[((-8))+rdi]
	adc	r12,rax
	mov	rax,QWORD PTR[((-8))+rbp*1+rsi]
	mov	QWORD PTR[((-32))+rdi],r12
	adc	r13,rdx

	lea	rbx,QWORD PTR[r10*2+r14]
	mov	QWORD PTR[((-24))+rdi],r13
	sbb	r15,r15
	shr	r10,63
	lea	r8,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r8,r10
	mov	r10,QWORD PTR[rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[8+rdi]
	adc	rbx,rax
	mov	rax,QWORD PTR[rbp*1+rsi]
	mov	QWORD PTR[((-16))+rdi],rbx
	adc	r8,rdx

	lea	r12,QWORD PTR[r10*2+r14]
	mov	QWORD PTR[((-8))+rdi],r8
	sbb	r15,r15
	shr	r10,63
	lea	r13,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r13,r10
	mov	r10,QWORD PTR[16+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[24+rdi]
	adc	r12,rax
	mov	rax,QWORD PTR[8+rbp*1+rsi]
	mov	QWORD PTR[rdi],r12
	adc	r13,rdx

	lea	rbx,QWORD PTR[r10*2+r14]
	mov	QWORD PTR[8+rdi],r13
	sbb	r15,r15
	shr	r10,63
	lea	r8,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r8,r10
	mov	r10,QWORD PTR[32+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[40+rdi]
	adc	rbx,rax
	mov	rax,QWORD PTR[16+rbp*1+rsi]
	mov	QWORD PTR[16+rdi],rbx
	adc	r8,rdx
	mov	QWORD PTR[24+rdi],r8
	sbb	r15,r15
	lea	rdi,QWORD PTR[64+rdi]
	add	rbp,32
	jnz	$L$sqr4x_shift_n_add

	lea	r12,QWORD PTR[r10*2+r14]
DB	067h
	shr	r10,63
	lea	r13,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r13,r10
	mov	r10,QWORD PTR[((-16))+rdi]
	mov	r14,r11
	mul	rax
	neg	r15
	mov	r11,QWORD PTR[((-8))+rdi]
	adc	r12,rax
	mov	rax,QWORD PTR[((-8))+rsi]
	mov	QWORD PTR[((-32))+rdi],r12
	adc	r13,rdx

	lea	rbx,QWORD PTR[r10*2+r14]
	mov	QWORD PTR[((-24))+rdi],r13
	sbb	r15,r15
	shr	r10,63
	lea	r8,QWORD PTR[r11*2+rcx]
	shr	r11,63
	or	r8,r10
	mul	rax
	neg	r15
	adc	rbx,rax
	adc	r8,rdx
	mov	QWORD PTR[((-16))+rdi],rbx
	mov	QWORD PTR[((-8))+rdi],r8
DB	102,72,15,126,213
__bn_sqr8x_reduction::
	xor	rax,rax
	lea	rcx,QWORD PTR[rbp*1+r9]
	lea	rdx,QWORD PTR[((48+8))+r9*2+rsp]
	mov	QWORD PTR[((0+8))+rsp],rcx
	lea	rdi,QWORD PTR[((48+8))+r9*1+rsp]
	mov	QWORD PTR[((8+8))+rsp],rdx
	neg	r9
	jmp	$L$8x_reduction_loop

ALIGN	32
$L$8x_reduction_loop::
	lea	rdi,QWORD PTR[r9*1+rdi]
DB	066h
	mov	rbx,QWORD PTR[rdi]
	mov	r9,QWORD PTR[8+rdi]
	mov	r10,QWORD PTR[16+rdi]
	mov	r11,QWORD PTR[24+rdi]
	mov	r12,QWORD PTR[32+rdi]
	mov	r13,QWORD PTR[40+rdi]
	mov	r14,QWORD PTR[48+rdi]
	mov	r15,QWORD PTR[56+rdi]
	mov	QWORD PTR[rdx],rax
	lea	rdi,QWORD PTR[64+rdi]

DB	067h
	mov	r8,rbx
	imul	rbx,QWORD PTR[((32+8))+rsp]
	mov	rax,QWORD PTR[rbp]
	mov	ecx,8
	jmp	$L$8x_reduce

ALIGN	32
$L$8x_reduce::
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
	mov	QWORD PTR[((48-8+8))+rcx*8+rsp],rbx
	mov	r9,rdx
	adc	r9,0

	mul	rbx
	add	r10,rax
	mov	rax,QWORD PTR[24+rbp]
	adc	rdx,0
	add	r9,r10
	mov	rsi,QWORD PTR[((32+8))+rsp]
	mov	r10,rdx
	adc	r10,0

	mul	rbx
	add	r11,rax
	mov	rax,QWORD PTR[32+rbp]
	adc	rdx,0
	imul	rsi,r8
	add	r10,r11
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[40+rbp]
	adc	rdx,0
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
	jnz	$L$8x_reduce

	lea	rbp,QWORD PTR[64+rbp]
	xor	rax,rax
	mov	rdx,QWORD PTR[((8+8))+rsp]
	cmp	rbp,QWORD PTR[((0+8))+rsp]
	jae	$L$8x_no_tail

DB	066h
	add	r8,QWORD PTR[rdi]
	adc	r9,QWORD PTR[8+rdi]
	adc	r10,QWORD PTR[16+rdi]
	adc	r11,QWORD PTR[24+rdi]
	adc	r12,QWORD PTR[32+rdi]
	adc	r13,QWORD PTR[40+rdi]
	adc	r14,QWORD PTR[48+rdi]
	adc	r15,QWORD PTR[56+rdi]
	sbb	rsi,rsi

	mov	rbx,QWORD PTR[((48+56+8))+rsp]
	mov	ecx,8
	mov	rax,QWORD PTR[rbp]
	jmp	$L$8x_tail

ALIGN	32
$L$8x_tail::
	mul	rbx
	add	r8,rax
	mov	rax,QWORD PTR[8+rbp]
	mov	QWORD PTR[rdi],r8
	mov	r8,rdx
	adc	r8,0

	mul	rbx
	add	r9,rax
	mov	rax,QWORD PTR[16+rbp]
	adc	rdx,0
	add	r8,r9
	lea	rdi,QWORD PTR[8+rdi]
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
	mov	r11,rdx
	adc	r11,0

	mul	rbx
	add	r12,rax
	mov	rax,QWORD PTR[40+rbp]
	adc	rdx,0
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
	mov	rbx,QWORD PTR[((48-16+8))+rcx*8+rsp]
	add	r15,rax
	adc	rdx,0
	add	r14,r15
	mov	rax,QWORD PTR[rbp]
	mov	r15,rdx
	adc	r15,0

	dec	ecx
	jnz	$L$8x_tail

	lea	rbp,QWORD PTR[64+rbp]
	mov	rdx,QWORD PTR[((8+8))+rsp]
	cmp	rbp,QWORD PTR[((0+8))+rsp]
	jae	$L$8x_tail_done

	mov	rbx,QWORD PTR[((48+56+8))+rsp]
	neg	rsi
	mov	rax,QWORD PTR[rbp]
	adc	r8,QWORD PTR[rdi]
	adc	r9,QWORD PTR[8+rdi]
	adc	r10,QWORD PTR[16+rdi]
	adc	r11,QWORD PTR[24+rdi]
	adc	r12,QWORD PTR[32+rdi]
	adc	r13,QWORD PTR[40+rdi]
	adc	r14,QWORD PTR[48+rdi]
	adc	r15,QWORD PTR[56+rdi]
	sbb	rsi,rsi

	mov	ecx,8
	jmp	$L$8x_tail

ALIGN	32
$L$8x_tail_done::
	add	r8,QWORD PTR[rdx]
	adc	r9,0
	adc	r10,0
	adc	r11,0
	adc	r12,0
	adc	r13,0
	adc	r14,0
	adc	r15,0


	xor	rax,rax

	neg	rsi
$L$8x_no_tail::
	adc	r8,QWORD PTR[rdi]
	adc	r9,QWORD PTR[8+rdi]
	adc	r10,QWORD PTR[16+rdi]
	adc	r11,QWORD PTR[24+rdi]
	adc	r12,QWORD PTR[32+rdi]
	adc	r13,QWORD PTR[40+rdi]
	adc	r14,QWORD PTR[48+rdi]
	adc	r15,QWORD PTR[56+rdi]
	adc	rax,0
	mov	rcx,QWORD PTR[((-8))+rbp]
	xor	rsi,rsi

DB	102,72,15,126,213

	mov	QWORD PTR[rdi],r8
	mov	QWORD PTR[8+rdi],r9
DB	102,73,15,126,217
	mov	QWORD PTR[16+rdi],r10
	mov	QWORD PTR[24+rdi],r11
	mov	QWORD PTR[32+rdi],r12
	mov	QWORD PTR[40+rdi],r13
	mov	QWORD PTR[48+rdi],r14
	mov	QWORD PTR[56+rdi],r15
	lea	rdi,QWORD PTR[64+rdi]

	cmp	rdi,rdx
	jb	$L$8x_reduction_loop
	DB	0F3h,0C3h		;repret
bn_sqr8x_internal	ENDP

ALIGN	32
__bn_post4x_internal	PROC PRIVATE
	mov	r12,QWORD PTR[rbp]
	lea	rbx,QWORD PTR[r9*1+rdi]
	mov	rcx,r9
DB	102,72,15,126,207
	neg	rax
DB	102,72,15,126,206
	sar	rcx,3+2
	dec	r12
	xor	r10,r10
	mov	r13,QWORD PTR[8+rbp]
	mov	r14,QWORD PTR[16+rbp]
	mov	r15,QWORD PTR[24+rbp]
	jmp	$L$sqr4x_sub_entry

ALIGN	16
$L$sqr4x_sub::
	mov	r12,QWORD PTR[rbp]
	mov	r13,QWORD PTR[8+rbp]
	mov	r14,QWORD PTR[16+rbp]
	mov	r15,QWORD PTR[24+rbp]
$L$sqr4x_sub_entry::
	lea	rbp,QWORD PTR[32+rbp]
	not	r12
	not	r13
	not	r14
	not	r15
	and	r12,rax
	and	r13,rax
	and	r14,rax
	and	r15,rax

	neg	r10
	adc	r12,QWORD PTR[rbx]
	adc	r13,QWORD PTR[8+rbx]
	adc	r14,QWORD PTR[16+rbx]
	adc	r15,QWORD PTR[24+rbx]
	mov	QWORD PTR[rdi],r12
	lea	rbx,QWORD PTR[32+rbx]
	mov	QWORD PTR[8+rdi],r13
	sbb	r10,r10
	mov	QWORD PTR[16+rdi],r14
	mov	QWORD PTR[24+rdi],r15
	lea	rdi,QWORD PTR[32+rdi]

	inc	rcx
	jnz	$L$sqr4x_sub

	mov	r10,r9
	neg	r9
	DB	0F3h,0C3h		;repret
__bn_post4x_internal	ENDP
PUBLIC	bn_from_montgomery

ALIGN	32
bn_from_montgomery	PROC PUBLIC
	test	DWORD PTR[48+rsp],7
	jz	bn_from_mont8x
	xor	eax,eax
	DB	0F3h,0C3h		;repret
bn_from_montgomery	ENDP


ALIGN	32
bn_from_mont8x	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_bn_from_mont8x::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


DB	067h
	mov	rax,rsp
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	shl	r9d,3
	lea	r10,QWORD PTR[r9*2+r9]
	neg	r9
	mov	r8,QWORD PTR[r8]








	lea	r11,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,rdi
	and	r11,4095
	cmp	r10,r11
	jb	$L$from_sp_alt
	sub	rsp,r11
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	jmp	$L$from_sp_done

ALIGN	32
$L$from_sp_alt::
	lea	r10,QWORD PTR[((4096-320))+r9*2]
	lea	rsp,QWORD PTR[((-320))+r9*2+rsp]
	sub	r11,r10
	mov	r10,0
	cmovc	r11,r10
	sub	rsp,r11
$L$from_sp_done::
	and	rsp,-64
	mov	r11,rax
	sub	r11,rsp
	and	r11,-4096
$L$from_page_walk::
	mov	r10,QWORD PTR[r11*1+rsp]
	sub	r11,4096
DB	02eh
	jnc	$L$from_page_walk

	mov	r10,r9
	neg	r9










	mov	QWORD PTR[32+rsp],r8
	mov	QWORD PTR[40+rsp],rax
$L$from_body::
	mov	r11,r9
	lea	rax,QWORD PTR[48+rsp]
	pxor	xmm0,xmm0
	jmp	$L$mul_by_1

ALIGN	32
$L$mul_by_1::
	movdqu	xmm1,XMMWORD PTR[rsi]
	movdqu	xmm2,XMMWORD PTR[16+rsi]
	movdqu	xmm3,XMMWORD PTR[32+rsi]
	movdqa	XMMWORD PTR[r9*1+rax],xmm0
	movdqu	xmm4,XMMWORD PTR[48+rsi]
	movdqa	XMMWORD PTR[16+r9*1+rax],xmm0
DB	048h,08dh,0b6h,040h,000h,000h,000h
	movdqa	XMMWORD PTR[rax],xmm1
	movdqa	XMMWORD PTR[32+r9*1+rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm2
	movdqa	XMMWORD PTR[48+r9*1+rax],xmm0
	movdqa	XMMWORD PTR[32+rax],xmm3
	movdqa	XMMWORD PTR[48+rax],xmm4
	lea	rax,QWORD PTR[64+rax]
	sub	r11,64
	jnz	$L$mul_by_1

DB	102,72,15,110,207
DB	102,72,15,110,209
DB	067h
	mov	rbp,rcx
DB	102,73,15,110,218
	call	__bn_sqr8x_reduction
	call	__bn_post4x_internal

	pxor	xmm0,xmm0
	lea	rax,QWORD PTR[48+rsp]
	mov	rsi,QWORD PTR[40+rsp]
	jmp	$L$from_mont_zero

ALIGN	32
$L$from_mont_zero::
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	XMMWORD PTR[16+rax],xmm0
	movdqa	XMMWORD PTR[32+rax],xmm0
	movdqa	XMMWORD PTR[48+rax],xmm0
	lea	rax,QWORD PTR[64+rax]
	sub	r9,32
	jnz	$L$from_mont_zero

	mov	rax,1
	mov	r15,QWORD PTR[((-48))+rsi]
	mov	r14,QWORD PTR[((-40))+rsi]
	mov	r13,QWORD PTR[((-32))+rsi]
	mov	r12,QWORD PTR[((-24))+rsi]
	mov	rbp,QWORD PTR[((-16))+rsi]
	mov	rbx,QWORD PTR[((-8))+rsi]
	lea	rsp,QWORD PTR[rsi]
$L$from_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_from_mont8x::
bn_from_mont8x	ENDP
PUBLIC	bn_get_bits5

ALIGN	16
bn_get_bits5	PROC PUBLIC
	lea	r10,QWORD PTR[rcx]
	lea	r11,QWORD PTR[1+rcx]
	mov	ecx,edx
	shr	edx,4
	and	ecx,15
	lea	eax,DWORD PTR[((-8))+rcx]
	cmp	ecx,11
	cmova	r10,r11
	cmova	ecx,eax
	movzx	eax,WORD PTR[rdx*2+r10]
	shr	eax,cl
	and	eax,31
	DB	0F3h,0C3h		;repret
bn_get_bits5	ENDP

PUBLIC	bn_scatter5

ALIGN	16
bn_scatter5	PROC PUBLIC
	cmp	edx,0
	jz	$L$scatter_epilogue
	lea	r8,QWORD PTR[r9*8+r8]
$L$scatter::
	mov	rax,QWORD PTR[rcx]
	lea	rcx,QWORD PTR[8+rcx]
	mov	QWORD PTR[r8],rax
	lea	r8,QWORD PTR[256+r8]
	sub	edx,1
	jnz	$L$scatter
$L$scatter_epilogue::
	DB	0F3h,0C3h		;repret
bn_scatter5	ENDP

PUBLIC	bn_gather5

ALIGN	32
bn_gather5	PROC PUBLIC
$L$SEH_begin_bn_gather5::

DB	04ch,08dh,014h,024h
DB	048h,081h,0ech,008h,001h,000h,000h
	lea	rax,QWORD PTR[$L$inc]
	and	rsp,-16

	movd	xmm5,r9d
	movdqa	xmm0,XMMWORD PTR[rax]
	movdqa	xmm1,XMMWORD PTR[16+rax]
	lea	r11,QWORD PTR[128+r8]
	lea	rax,QWORD PTR[128+rsp]

	pshufd	xmm5,xmm5,0
	movdqa	xmm4,xmm1
	movdqa	xmm2,xmm1
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[(-128)+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[(-112)+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[(-96)+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[(-80)+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[(-64)+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[(-48)+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[(-32)+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[(-16)+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[16+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[32+rax],xmm2
	movdqa	xmm2,xmm4
	paddd	xmm1,xmm0
	pcmpeqd	xmm0,xmm5
	movdqa	XMMWORD PTR[48+rax],xmm3
	movdqa	xmm3,xmm4

	paddd	xmm2,xmm1
	pcmpeqd	xmm1,xmm5
	movdqa	XMMWORD PTR[64+rax],xmm0
	movdqa	xmm0,xmm4

	paddd	xmm3,xmm2
	pcmpeqd	xmm2,xmm5
	movdqa	XMMWORD PTR[80+rax],xmm1
	movdqa	xmm1,xmm4

	paddd	xmm0,xmm3
	pcmpeqd	xmm3,xmm5
	movdqa	XMMWORD PTR[96+rax],xmm2
	movdqa	xmm2,xmm4
	movdqa	XMMWORD PTR[112+rax],xmm3
	jmp	$L$gather

ALIGN	32
$L$gather::
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movdqa	xmm0,XMMWORD PTR[((-128))+r11]
	movdqa	xmm1,XMMWORD PTR[((-112))+r11]
	movdqa	xmm2,XMMWORD PTR[((-96))+r11]
	pand	xmm0,XMMWORD PTR[((-128))+rax]
	movdqa	xmm3,XMMWORD PTR[((-80))+r11]
	pand	xmm1,XMMWORD PTR[((-112))+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-96))+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-80))+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[((-64))+r11]
	movdqa	xmm1,XMMWORD PTR[((-48))+r11]
	movdqa	xmm2,XMMWORD PTR[((-32))+r11]
	pand	xmm0,XMMWORD PTR[((-64))+rax]
	movdqa	xmm3,XMMWORD PTR[((-16))+r11]
	pand	xmm1,XMMWORD PTR[((-48))+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[((-32))+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[((-16))+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[r11]
	movdqa	xmm1,XMMWORD PTR[16+r11]
	movdqa	xmm2,XMMWORD PTR[32+r11]
	pand	xmm0,XMMWORD PTR[rax]
	movdqa	xmm3,XMMWORD PTR[48+r11]
	pand	xmm1,XMMWORD PTR[16+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[32+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[48+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	movdqa	xmm0,XMMWORD PTR[64+r11]
	movdqa	xmm1,XMMWORD PTR[80+r11]
	movdqa	xmm2,XMMWORD PTR[96+r11]
	pand	xmm0,XMMWORD PTR[64+rax]
	movdqa	xmm3,XMMWORD PTR[112+r11]
	pand	xmm1,XMMWORD PTR[80+rax]
	por	xmm4,xmm0
	pand	xmm2,XMMWORD PTR[96+rax]
	por	xmm5,xmm1
	pand	xmm3,XMMWORD PTR[112+rax]
	por	xmm4,xmm2
	por	xmm5,xmm3
	por	xmm4,xmm5
	lea	r11,QWORD PTR[256+r11]
	pshufd	xmm0,xmm4,04eh
	por	xmm0,xmm4
	movq	QWORD PTR[rcx],xmm0
	lea	rcx,QWORD PTR[8+rcx]
	sub	edx,1
	jnz	$L$gather

	lea	rsp,QWORD PTR[r10]
	DB	0F3h,0C3h		;repret
$L$SEH_end_bn_gather5::
bn_gather5	ENDP
ALIGN	64
$L$inc::
	DD	0,0,1,1
	DD	2,2,2,2
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

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	r10,QWORD PTR[$L$mul_epilogue]
	cmp	rbx,r10
	ja	$L$body_40

	mov	r10,QWORD PTR[192+r8]
	mov	rax,QWORD PTR[8+r10*8+rax]

	jmp	$L$body_proceed

$L$body_40::
	mov	rax,QWORD PTR[40+rax]
$L$body_proceed::
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

	DD	imagerel $L$SEH_begin_bn_power5
	DD	imagerel $L$SEH_end_bn_power5
	DD	imagerel $L$SEH_info_bn_power5

	DD	imagerel $L$SEH_begin_bn_from_mont8x
	DD	imagerel $L$SEH_end_bn_from_mont8x
	DD	imagerel $L$SEH_info_bn_from_mont8x
	DD	imagerel $L$SEH_begin_bn_gather5
	DD	imagerel $L$SEH_end_bn_gather5
	DD	imagerel $L$SEH_info_bn_gather5

.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_bn_mul_mont_gather5::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul_body,imagerel $L$mul_epilogue
ALIGN	8
$L$SEH_info_bn_mul4x_mont_gather5::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$mul4x_body,imagerel $L$mul4x_epilogue
ALIGN	8
$L$SEH_info_bn_power5::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$power5_body,imagerel $L$power5_epilogue
ALIGN	8
$L$SEH_info_bn_from_mont8x::
DB	9,0,0,0
	DD	imagerel mul_handler
	DD	imagerel $L$from_body,imagerel $L$from_epilogue
ALIGN	8
$L$SEH_info_bn_gather5::
DB	001h,00bh,003h,00ah
DB	00bh,001h,021h,000h
DB	004h,0a3h,000h,000h
ALIGN	8

.xdata	ENDS
END
