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
	movd	xmm5,DWORD PTR[56+rsp]
	lea	r10,QWORD PTR[$L$inc]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

$L$mul_alloca::
	mov	rax,rsp
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
	movd	xmm5,DWORD PTR[56+rsp]
	lea	r10,QWORD PTR[$L$inc]
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

$L$mul4x_alloca::
	mov	rax,rsp
	lea	r11,QWORD PTR[4+r9]
	neg	r11
	lea	rsp,QWORD PTR[((-256))+r11*8+rsp]
	and	rsp,-1024

	mov	QWORD PTR[8+r9*8+rsp],rax
$L$mul4x_body::
	sub	rax,rsp
	and	rax,-4096
$L$mul4x_page_walk::
	mov	r11,QWORD PTR[rax*1+rsp]
	sub	rax,4096
DB	02eh

	jnc	$L$mul4x_page_walk

	mov	QWORD PTR[16+r9*8+rsp],rdi
	lea	r12,QWORD PTR[128+rdx]
	movdqa	xmm0,XMMWORD PTR[r10]
	movdqa	xmm1,XMMWORD PTR[16+r10]
	lea	r10,QWORD PTR[((32-112))+r9*8+rsp]

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

	xor	rdi,rdi
	add	r13,r10
	adc	rdi,0
	mov	QWORD PTR[((-8))+r15*8+rsp],r13
	mov	QWORD PTR[r15*8+rsp],rdi

	lea	r14,QWORD PTR[1+r14]
ALIGN	4
$L$outer4x::
	lea	rdx,QWORD PTR[((32+128))+r9*8+rsp]
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
	sub	rdx,1
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

	lea	rax,QWORD PTR[48+rax]

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
DB	001h,00bh,003h,00ah
DB	00bh,001h,021h,000h

DB	004h,0a3h,000h,000h

ALIGN	8

.xdata	ENDS
END
