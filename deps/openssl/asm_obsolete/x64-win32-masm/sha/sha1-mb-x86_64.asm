OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	sha1_multi_block

ALIGN	32
sha1_multi_block	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


	mov	rcx,QWORD PTR[((OPENSSL_ia32cap_P+4))]
	bt	rcx,61
	jc	_shaext_shortcut
	mov	rax,rsp
	push	rbx
	push	rbp
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
	movaps	XMMWORD PTR[(-120)+rax],xmm10
	movaps	XMMWORD PTR[(-104)+rax],xmm11
	movaps	XMMWORD PTR[(-88)+rax],xmm12
	movaps	XMMWORD PTR[(-72)+rax],xmm13
	movaps	XMMWORD PTR[(-56)+rax],xmm14
	movaps	XMMWORD PTR[(-40)+rax],xmm15
	sub	rsp,288
	and	rsp,-256
	mov	QWORD PTR[272+rsp],rax
$L$body::
	lea	rbp,QWORD PTR[K_XX_XX]
	lea	rbx,QWORD PTR[256+rsp]

$L$oop_grande::
	mov	DWORD PTR[280+rsp],edx
	xor	edx,edx
	mov	r8,QWORD PTR[rsi]
	mov	ecx,DWORD PTR[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[rbx],ecx
	cmovle	r8,rbp
	mov	r9,QWORD PTR[16+rsi]
	mov	ecx,DWORD PTR[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[4+rbx],ecx
	cmovle	r9,rbp
	mov	r10,QWORD PTR[32+rsi]
	mov	ecx,DWORD PTR[40+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[8+rbx],ecx
	cmovle	r10,rbp
	mov	r11,QWORD PTR[48+rsi]
	mov	ecx,DWORD PTR[56+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[12+rbx],ecx
	cmovle	r11,rbp
	test	edx,edx
	jz	$L$done

	movdqu	xmm10,XMMWORD PTR[rdi]
	lea	rax,QWORD PTR[128+rsp]
	movdqu	xmm11,XMMWORD PTR[32+rdi]
	movdqu	xmm12,XMMWORD PTR[64+rdi]
	movdqu	xmm13,XMMWORD PTR[96+rdi]
	movdqu	xmm14,XMMWORD PTR[128+rdi]
	movdqa	xmm5,XMMWORD PTR[96+rbp]
	movdqa	xmm15,XMMWORD PTR[((-32))+rbp]
	jmp	$L$oop

ALIGN	32
$L$oop::
	movd	xmm0,DWORD PTR[r8]
	lea	r8,QWORD PTR[64+r8]
	movd	xmm2,DWORD PTR[r9]
	lea	r9,QWORD PTR[64+r9]
	movd	xmm3,DWORD PTR[r10]
	lea	r10,QWORD PTR[64+r10]
	movd	xmm4,DWORD PTR[r11]
	lea	r11,QWORD PTR[64+r11]
	punpckldq	xmm0,xmm3
	movd	xmm1,DWORD PTR[((-60))+r8]
	punpckldq	xmm2,xmm4
	movd	xmm9,DWORD PTR[((-60))+r9]
	punpckldq	xmm0,xmm2
	movd	xmm8,DWORD PTR[((-60))+r10]
DB	102,15,56,0,197
	movd	xmm7,DWORD PTR[((-60))+r11]
	punpckldq	xmm1,xmm8
	movdqa	xmm8,xmm10
	paddd	xmm14,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm11
	movdqa	xmm6,xmm11
	pslld	xmm8,5
	pandn	xmm7,xmm13
	pand	xmm6,xmm12
	punpckldq	xmm1,xmm9
	movdqa	xmm9,xmm10

	movdqa	XMMWORD PTR[(0-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD PTR[((-56))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-56))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD PTR[((-56))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD PTR[((-56))+r11]
	punpckldq	xmm2,xmm8
	movdqa	xmm8,xmm14
	paddd	xmm13,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm10
	movdqa	xmm6,xmm10
	pslld	xmm8,5
	pandn	xmm7,xmm12
	pand	xmm6,xmm11
	punpckldq	xmm2,xmm9
	movdqa	xmm9,xmm14

	movdqa	XMMWORD PTR[(16-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD PTR[((-52))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-52))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD PTR[((-52))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD PTR[((-52))+r11]
	punpckldq	xmm3,xmm8
	movdqa	xmm8,xmm13
	paddd	xmm12,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm14
	movdqa	xmm6,xmm14
	pslld	xmm8,5
	pandn	xmm7,xmm11
	pand	xmm6,xmm10
	punpckldq	xmm3,xmm9
	movdqa	xmm9,xmm13

	movdqa	XMMWORD PTR[(32-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD PTR[((-48))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-48))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD PTR[((-48))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD PTR[((-48))+r11]
	punpckldq	xmm4,xmm8
	movdqa	xmm8,xmm12
	paddd	xmm11,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm13
	movdqa	xmm6,xmm13
	pslld	xmm8,5
	pandn	xmm7,xmm10
	pand	xmm6,xmm14
	punpckldq	xmm4,xmm9
	movdqa	xmm9,xmm12

	movdqa	XMMWORD PTR[(48-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD PTR[((-44))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-44))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD PTR[((-44))+r10]
	por	xmm13,xmm7
	movd	xmm7,DWORD PTR[((-44))+r11]
	punpckldq	xmm0,xmm8
	movdqa	xmm8,xmm11
	paddd	xmm10,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm12
	movdqa	xmm6,xmm12
	pslld	xmm8,5
	pandn	xmm7,xmm14
	pand	xmm6,xmm13
	punpckldq	xmm0,xmm9
	movdqa	xmm9,xmm11

	movdqa	XMMWORD PTR[(64-128)+rax],xmm4
	paddd	xmm10,xmm4
	movd	xmm1,DWORD PTR[((-40))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm12

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-40))+r9]
	pslld	xmm7,30
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
DB	102,15,56,0,197
	movd	xmm8,DWORD PTR[((-40))+r10]
	por	xmm12,xmm7
	movd	xmm7,DWORD PTR[((-40))+r11]
	punpckldq	xmm1,xmm8
	movdqa	xmm8,xmm10
	paddd	xmm14,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm11
	movdqa	xmm6,xmm11
	pslld	xmm8,5
	pandn	xmm7,xmm13
	pand	xmm6,xmm12
	punpckldq	xmm1,xmm9
	movdqa	xmm9,xmm10

	movdqa	XMMWORD PTR[(80-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD PTR[((-36))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-36))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD PTR[((-36))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD PTR[((-36))+r11]
	punpckldq	xmm2,xmm8
	movdqa	xmm8,xmm14
	paddd	xmm13,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm10
	movdqa	xmm6,xmm10
	pslld	xmm8,5
	pandn	xmm7,xmm12
	pand	xmm6,xmm11
	punpckldq	xmm2,xmm9
	movdqa	xmm9,xmm14

	movdqa	XMMWORD PTR[(96-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD PTR[((-32))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-32))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD PTR[((-32))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD PTR[((-32))+r11]
	punpckldq	xmm3,xmm8
	movdqa	xmm8,xmm13
	paddd	xmm12,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm14
	movdqa	xmm6,xmm14
	pslld	xmm8,5
	pandn	xmm7,xmm11
	pand	xmm6,xmm10
	punpckldq	xmm3,xmm9
	movdqa	xmm9,xmm13

	movdqa	XMMWORD PTR[(112-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD PTR[((-28))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-28))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD PTR[((-28))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD PTR[((-28))+r11]
	punpckldq	xmm4,xmm8
	movdqa	xmm8,xmm12
	paddd	xmm11,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm13
	movdqa	xmm6,xmm13
	pslld	xmm8,5
	pandn	xmm7,xmm10
	pand	xmm6,xmm14
	punpckldq	xmm4,xmm9
	movdqa	xmm9,xmm12

	movdqa	XMMWORD PTR[(128-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD PTR[((-24))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-24))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD PTR[((-24))+r10]
	por	xmm13,xmm7
	movd	xmm7,DWORD PTR[((-24))+r11]
	punpckldq	xmm0,xmm8
	movdqa	xmm8,xmm11
	paddd	xmm10,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm12
	movdqa	xmm6,xmm12
	pslld	xmm8,5
	pandn	xmm7,xmm14
	pand	xmm6,xmm13
	punpckldq	xmm0,xmm9
	movdqa	xmm9,xmm11

	movdqa	XMMWORD PTR[(144-128)+rax],xmm4
	paddd	xmm10,xmm4
	movd	xmm1,DWORD PTR[((-20))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm12

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-20))+r9]
	pslld	xmm7,30
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
DB	102,15,56,0,197
	movd	xmm8,DWORD PTR[((-20))+r10]
	por	xmm12,xmm7
	movd	xmm7,DWORD PTR[((-20))+r11]
	punpckldq	xmm1,xmm8
	movdqa	xmm8,xmm10
	paddd	xmm14,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm11
	movdqa	xmm6,xmm11
	pslld	xmm8,5
	pandn	xmm7,xmm13
	pand	xmm6,xmm12
	punpckldq	xmm1,xmm9
	movdqa	xmm9,xmm10

	movdqa	XMMWORD PTR[(160-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD PTR[((-16))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-16))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD PTR[((-16))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD PTR[((-16))+r11]
	punpckldq	xmm2,xmm8
	movdqa	xmm8,xmm14
	paddd	xmm13,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm10
	movdqa	xmm6,xmm10
	pslld	xmm8,5
	pandn	xmm7,xmm12
	pand	xmm6,xmm11
	punpckldq	xmm2,xmm9
	movdqa	xmm9,xmm14

	movdqa	XMMWORD PTR[(176-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD PTR[((-12))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-12))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD PTR[((-12))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD PTR[((-12))+r11]
	punpckldq	xmm3,xmm8
	movdqa	xmm8,xmm13
	paddd	xmm12,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm14
	movdqa	xmm6,xmm14
	pslld	xmm8,5
	pandn	xmm7,xmm11
	pand	xmm6,xmm10
	punpckldq	xmm3,xmm9
	movdqa	xmm9,xmm13

	movdqa	XMMWORD PTR[(192-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD PTR[((-8))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-8))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD PTR[((-8))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD PTR[((-8))+r11]
	punpckldq	xmm4,xmm8
	movdqa	xmm8,xmm12
	paddd	xmm11,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm13
	movdqa	xmm6,xmm13
	pslld	xmm8,5
	pandn	xmm7,xmm10
	pand	xmm6,xmm14
	punpckldq	xmm4,xmm9
	movdqa	xmm9,xmm12

	movdqa	XMMWORD PTR[(208-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD PTR[((-4))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD PTR[((-4))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD PTR[((-4))+r10]
	por	xmm13,xmm7
	movdqa	xmm1,XMMWORD PTR[((0-128))+rax]
	movd	xmm7,DWORD PTR[((-4))+r11]
	punpckldq	xmm0,xmm8
	movdqa	xmm8,xmm11
	paddd	xmm10,xmm15
	punpckldq	xmm9,xmm7
	movdqa	xmm7,xmm12
	movdqa	xmm6,xmm12
	pslld	xmm8,5
	prefetcht0	[63+r8]
	pandn	xmm7,xmm14
	pand	xmm6,xmm13
	punpckldq	xmm0,xmm9
	movdqa	xmm9,xmm11

	movdqa	XMMWORD PTR[(224-128)+rax],xmm4
	paddd	xmm10,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm12
	prefetcht0	[63+r9]

	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm10,xmm6
	prefetcht0	[63+r10]

	psrld	xmm12,2
	paddd	xmm10,xmm8
DB	102,15,56,0,197
	prefetcht0	[63+r11]
	por	xmm12,xmm7
	movdqa	xmm2,XMMWORD PTR[((16-128))+rax]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((32-128))+rax]

	movdqa	xmm8,xmm10
	pxor	xmm1,XMMWORD PTR[((128-128))+rax]
	paddd	xmm14,xmm15
	movdqa	xmm7,xmm11
	pslld	xmm8,5
	pxor	xmm1,xmm3
	movdqa	xmm6,xmm11
	pandn	xmm7,xmm13
	movdqa	xmm5,xmm1
	pand	xmm6,xmm12
	movdqa	xmm9,xmm10
	psrld	xmm5,31
	paddd	xmm1,xmm1

	movdqa	XMMWORD PTR[(240-128)+rax],xmm0
	paddd	xmm14,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm7

	movdqa	xmm7,xmm11
	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((48-128))+rax]

	movdqa	xmm8,xmm14
	pxor	xmm2,XMMWORD PTR[((144-128))+rax]
	paddd	xmm13,xmm15
	movdqa	xmm7,xmm10
	pslld	xmm8,5
	pxor	xmm2,xmm4
	movdqa	xmm6,xmm10
	pandn	xmm7,xmm12
	movdqa	xmm5,xmm2
	pand	xmm6,xmm11
	movdqa	xmm9,xmm14
	psrld	xmm5,31
	paddd	xmm2,xmm2

	movdqa	XMMWORD PTR[(0-128)+rax],xmm1
	paddd	xmm13,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm7

	movdqa	xmm7,xmm10
	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((64-128))+rax]

	movdqa	xmm8,xmm13
	pxor	xmm3,XMMWORD PTR[((160-128))+rax]
	paddd	xmm12,xmm15
	movdqa	xmm7,xmm14
	pslld	xmm8,5
	pxor	xmm3,xmm0
	movdqa	xmm6,xmm14
	pandn	xmm7,xmm11
	movdqa	xmm5,xmm3
	pand	xmm6,xmm10
	movdqa	xmm9,xmm13
	psrld	xmm5,31
	paddd	xmm3,xmm3

	movdqa	XMMWORD PTR[(16-128)+rax],xmm2
	paddd	xmm12,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm7

	movdqa	xmm7,xmm14
	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((80-128))+rax]

	movdqa	xmm8,xmm12
	pxor	xmm4,XMMWORD PTR[((176-128))+rax]
	paddd	xmm11,xmm15
	movdqa	xmm7,xmm13
	pslld	xmm8,5
	pxor	xmm4,xmm1
	movdqa	xmm6,xmm13
	pandn	xmm7,xmm10
	movdqa	xmm5,xmm4
	pand	xmm6,xmm14
	movdqa	xmm9,xmm12
	psrld	xmm5,31
	paddd	xmm4,xmm4

	movdqa	XMMWORD PTR[(32-128)+rax],xmm3
	paddd	xmm11,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm7

	movdqa	xmm7,xmm13
	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((96-128))+rax]

	movdqa	xmm8,xmm11
	pxor	xmm0,XMMWORD PTR[((192-128))+rax]
	paddd	xmm10,xmm15
	movdqa	xmm7,xmm12
	pslld	xmm8,5
	pxor	xmm0,xmm2
	movdqa	xmm6,xmm12
	pandn	xmm7,xmm14
	movdqa	xmm5,xmm0
	pand	xmm6,xmm13
	movdqa	xmm9,xmm11
	psrld	xmm5,31
	paddd	xmm0,xmm0

	movdqa	XMMWORD PTR[(48-128)+rax],xmm4
	paddd	xmm10,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm7

	movdqa	xmm7,xmm12
	por	xmm8,xmm9
	pslld	xmm7,30
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	movdqa	xmm15,XMMWORD PTR[rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((112-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((208-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(64-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((128-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((224-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(80-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((144-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((240-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(96-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((160-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((0-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(112-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((176-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((16-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(128-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((192-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((32-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(144-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((208-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((48-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(160-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((224-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((64-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(176-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((240-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((80-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(192-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((0-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((96-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(208-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((16-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((112-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(224-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((32-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((128-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(240-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((48-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((144-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(0-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((64-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((160-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(16-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((80-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((176-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(32-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((96-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((192-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(48-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((112-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((208-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(64-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((128-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((224-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(80-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((144-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((240-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(96-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((160-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((0-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(112-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	movdqa	xmm15,XMMWORD PTR[32+rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((176-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD PTR[((16-128))+rax]
	pxor	xmm1,xmm3
	paddd	xmm14,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm10
	pand	xmm7,xmm12

	movdqa	xmm6,xmm13
	movdqa	xmm5,xmm1
	psrld	xmm9,27
	paddd	xmm14,xmm7
	pxor	xmm6,xmm12

	movdqa	XMMWORD PTR[(128-128)+rax],xmm0
	paddd	xmm14,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm11
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	paddd	xmm1,xmm1
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((192-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD PTR[((32-128))+rax]
	pxor	xmm2,xmm4
	paddd	xmm13,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm14
	pand	xmm7,xmm11

	movdqa	xmm6,xmm12
	movdqa	xmm5,xmm2
	psrld	xmm9,27
	paddd	xmm13,xmm7
	pxor	xmm6,xmm11

	movdqa	XMMWORD PTR[(144-128)+rax],xmm1
	paddd	xmm13,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm10
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	paddd	xmm2,xmm2
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((208-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD PTR[((48-128))+rax]
	pxor	xmm3,xmm0
	paddd	xmm12,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm13
	pand	xmm7,xmm10

	movdqa	xmm6,xmm11
	movdqa	xmm5,xmm3
	psrld	xmm9,27
	paddd	xmm12,xmm7
	pxor	xmm6,xmm10

	movdqa	XMMWORD PTR[(160-128)+rax],xmm2
	paddd	xmm12,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm14
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	paddd	xmm3,xmm3
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((224-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD PTR[((64-128))+rax]
	pxor	xmm4,xmm1
	paddd	xmm11,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm12
	pand	xmm7,xmm14

	movdqa	xmm6,xmm10
	movdqa	xmm5,xmm4
	psrld	xmm9,27
	paddd	xmm11,xmm7
	pxor	xmm6,xmm14

	movdqa	XMMWORD PTR[(176-128)+rax],xmm3
	paddd	xmm11,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm13
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	paddd	xmm4,xmm4
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((240-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD PTR[((80-128))+rax]
	pxor	xmm0,xmm2
	paddd	xmm10,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm11
	pand	xmm7,xmm13

	movdqa	xmm6,xmm14
	movdqa	xmm5,xmm0
	psrld	xmm9,27
	paddd	xmm10,xmm7
	pxor	xmm6,xmm13

	movdqa	XMMWORD PTR[(192-128)+rax],xmm4
	paddd	xmm10,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm12
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	paddd	xmm0,xmm0
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((0-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD PTR[((96-128))+rax]
	pxor	xmm1,xmm3
	paddd	xmm14,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm10
	pand	xmm7,xmm12

	movdqa	xmm6,xmm13
	movdqa	xmm5,xmm1
	psrld	xmm9,27
	paddd	xmm14,xmm7
	pxor	xmm6,xmm12

	movdqa	XMMWORD PTR[(208-128)+rax],xmm0
	paddd	xmm14,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm11
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	paddd	xmm1,xmm1
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((16-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD PTR[((112-128))+rax]
	pxor	xmm2,xmm4
	paddd	xmm13,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm14
	pand	xmm7,xmm11

	movdqa	xmm6,xmm12
	movdqa	xmm5,xmm2
	psrld	xmm9,27
	paddd	xmm13,xmm7
	pxor	xmm6,xmm11

	movdqa	XMMWORD PTR[(224-128)+rax],xmm1
	paddd	xmm13,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm10
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	paddd	xmm2,xmm2
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((32-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD PTR[((128-128))+rax]
	pxor	xmm3,xmm0
	paddd	xmm12,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm13
	pand	xmm7,xmm10

	movdqa	xmm6,xmm11
	movdqa	xmm5,xmm3
	psrld	xmm9,27
	paddd	xmm12,xmm7
	pxor	xmm6,xmm10

	movdqa	XMMWORD PTR[(240-128)+rax],xmm2
	paddd	xmm12,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm14
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	paddd	xmm3,xmm3
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((48-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD PTR[((144-128))+rax]
	pxor	xmm4,xmm1
	paddd	xmm11,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm12
	pand	xmm7,xmm14

	movdqa	xmm6,xmm10
	movdqa	xmm5,xmm4
	psrld	xmm9,27
	paddd	xmm11,xmm7
	pxor	xmm6,xmm14

	movdqa	XMMWORD PTR[(0-128)+rax],xmm3
	paddd	xmm11,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm13
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	paddd	xmm4,xmm4
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((64-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD PTR[((160-128))+rax]
	pxor	xmm0,xmm2
	paddd	xmm10,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm11
	pand	xmm7,xmm13

	movdqa	xmm6,xmm14
	movdqa	xmm5,xmm0
	psrld	xmm9,27
	paddd	xmm10,xmm7
	pxor	xmm6,xmm13

	movdqa	XMMWORD PTR[(16-128)+rax],xmm4
	paddd	xmm10,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm12
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	paddd	xmm0,xmm0
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((80-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD PTR[((176-128))+rax]
	pxor	xmm1,xmm3
	paddd	xmm14,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm10
	pand	xmm7,xmm12

	movdqa	xmm6,xmm13
	movdqa	xmm5,xmm1
	psrld	xmm9,27
	paddd	xmm14,xmm7
	pxor	xmm6,xmm12

	movdqa	XMMWORD PTR[(32-128)+rax],xmm0
	paddd	xmm14,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm11
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	paddd	xmm1,xmm1
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((96-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD PTR[((192-128))+rax]
	pxor	xmm2,xmm4
	paddd	xmm13,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm14
	pand	xmm7,xmm11

	movdqa	xmm6,xmm12
	movdqa	xmm5,xmm2
	psrld	xmm9,27
	paddd	xmm13,xmm7
	pxor	xmm6,xmm11

	movdqa	XMMWORD PTR[(48-128)+rax],xmm1
	paddd	xmm13,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm10
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	paddd	xmm2,xmm2
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((112-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD PTR[((208-128))+rax]
	pxor	xmm3,xmm0
	paddd	xmm12,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm13
	pand	xmm7,xmm10

	movdqa	xmm6,xmm11
	movdqa	xmm5,xmm3
	psrld	xmm9,27
	paddd	xmm12,xmm7
	pxor	xmm6,xmm10

	movdqa	XMMWORD PTR[(64-128)+rax],xmm2
	paddd	xmm12,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm14
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	paddd	xmm3,xmm3
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((128-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD PTR[((224-128))+rax]
	pxor	xmm4,xmm1
	paddd	xmm11,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm12
	pand	xmm7,xmm14

	movdqa	xmm6,xmm10
	movdqa	xmm5,xmm4
	psrld	xmm9,27
	paddd	xmm11,xmm7
	pxor	xmm6,xmm14

	movdqa	XMMWORD PTR[(80-128)+rax],xmm3
	paddd	xmm11,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm13
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	paddd	xmm4,xmm4
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((144-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD PTR[((240-128))+rax]
	pxor	xmm0,xmm2
	paddd	xmm10,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm11
	pand	xmm7,xmm13

	movdqa	xmm6,xmm14
	movdqa	xmm5,xmm0
	psrld	xmm9,27
	paddd	xmm10,xmm7
	pxor	xmm6,xmm13

	movdqa	XMMWORD PTR[(96-128)+rax],xmm4
	paddd	xmm10,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm12
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	paddd	xmm0,xmm0
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((160-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD PTR[((0-128))+rax]
	pxor	xmm1,xmm3
	paddd	xmm14,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm10
	pand	xmm7,xmm12

	movdqa	xmm6,xmm13
	movdqa	xmm5,xmm1
	psrld	xmm9,27
	paddd	xmm14,xmm7
	pxor	xmm6,xmm12

	movdqa	XMMWORD PTR[(112-128)+rax],xmm0
	paddd	xmm14,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm11
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	paddd	xmm1,xmm1
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((176-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD PTR[((16-128))+rax]
	pxor	xmm2,xmm4
	paddd	xmm13,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm14
	pand	xmm7,xmm11

	movdqa	xmm6,xmm12
	movdqa	xmm5,xmm2
	psrld	xmm9,27
	paddd	xmm13,xmm7
	pxor	xmm6,xmm11

	movdqa	XMMWORD PTR[(128-128)+rax],xmm1
	paddd	xmm13,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm10
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	paddd	xmm2,xmm2
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((192-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD PTR[((32-128))+rax]
	pxor	xmm3,xmm0
	paddd	xmm12,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm13
	pand	xmm7,xmm10

	movdqa	xmm6,xmm11
	movdqa	xmm5,xmm3
	psrld	xmm9,27
	paddd	xmm12,xmm7
	pxor	xmm6,xmm10

	movdqa	XMMWORD PTR[(144-128)+rax],xmm2
	paddd	xmm12,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm14
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	paddd	xmm3,xmm3
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((208-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD PTR[((48-128))+rax]
	pxor	xmm4,xmm1
	paddd	xmm11,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm12
	pand	xmm7,xmm14

	movdqa	xmm6,xmm10
	movdqa	xmm5,xmm4
	psrld	xmm9,27
	paddd	xmm11,xmm7
	pxor	xmm6,xmm14

	movdqa	XMMWORD PTR[(160-128)+rax],xmm3
	paddd	xmm11,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm13
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	paddd	xmm4,xmm4
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((224-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD PTR[((64-128))+rax]
	pxor	xmm0,xmm2
	paddd	xmm10,xmm15
	pslld	xmm8,5
	movdqa	xmm9,xmm11
	pand	xmm7,xmm13

	movdqa	xmm6,xmm14
	movdqa	xmm5,xmm0
	psrld	xmm9,27
	paddd	xmm10,xmm7
	pxor	xmm6,xmm13

	movdqa	XMMWORD PTR[(176-128)+rax],xmm4
	paddd	xmm10,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	pand	xmm6,xmm12
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	paddd	xmm0,xmm0
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	movdqa	xmm15,XMMWORD PTR[64+rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((240-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((80-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(192-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((0-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((96-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(208-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((16-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((112-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(224-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((32-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((128-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(240-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((48-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((144-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(0-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((64-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((160-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(16-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((80-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((176-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(32-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((96-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((192-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD PTR[(48-128)+rax],xmm2
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((112-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((208-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD PTR[(64-128)+rax],xmm3
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((128-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((224-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD PTR[(80-128)+rax],xmm4
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((144-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((240-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD PTR[(96-128)+rax],xmm0
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((160-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((0-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD PTR[(112-128)+rax],xmm1
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((176-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((16-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((192-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((32-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	pxor	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR[((208-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD PTR[((48-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	paddd	xmm10,xmm4
	pxor	xmm0,xmm2
	psrld	xmm9,27
	pxor	xmm6,xmm13
	movdqa	xmm7,xmm12

	pslld	xmm7,30
	movdqa	xmm5,xmm0
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm10,xmm6
	paddd	xmm0,xmm0

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm0,xmm5
	por	xmm12,xmm7
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD PTR[((224-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD PTR[((64-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	paddd	xmm14,xmm0
	pxor	xmm1,xmm3
	psrld	xmm9,27
	pxor	xmm6,xmm12
	movdqa	xmm7,xmm11

	pslld	xmm7,30
	movdqa	xmm5,xmm1
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm14,xmm6
	paddd	xmm1,xmm1

	psrld	xmm11,2
	paddd	xmm14,xmm8
	por	xmm1,xmm5
	por	xmm11,xmm7
	pxor	xmm2,xmm4
	movdqa	xmm4,XMMWORD PTR[((240-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD PTR[((80-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	paddd	xmm13,xmm1
	pxor	xmm2,xmm4
	psrld	xmm9,27
	pxor	xmm6,xmm11
	movdqa	xmm7,xmm10

	pslld	xmm7,30
	movdqa	xmm5,xmm2
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm13,xmm6
	paddd	xmm2,xmm2

	psrld	xmm10,2
	paddd	xmm13,xmm8
	por	xmm2,xmm5
	por	xmm10,xmm7
	pxor	xmm3,xmm0
	movdqa	xmm0,XMMWORD PTR[((0-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD PTR[((96-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	paddd	xmm12,xmm2
	pxor	xmm3,xmm0
	psrld	xmm9,27
	pxor	xmm6,xmm10
	movdqa	xmm7,xmm14

	pslld	xmm7,30
	movdqa	xmm5,xmm3
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm12,xmm6
	paddd	xmm3,xmm3

	psrld	xmm14,2
	paddd	xmm12,xmm8
	por	xmm3,xmm5
	por	xmm14,xmm7
	pxor	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR[((16-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD PTR[((112-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	paddd	xmm11,xmm3
	pxor	xmm4,xmm1
	psrld	xmm9,27
	pxor	xmm6,xmm14
	movdqa	xmm7,xmm13

	pslld	xmm7,30
	movdqa	xmm5,xmm4
	por	xmm8,xmm9
	psrld	xmm5,31
	paddd	xmm11,xmm6
	paddd	xmm4,xmm4

	psrld	xmm13,2
	paddd	xmm11,xmm8
	por	xmm4,xmm5
	por	xmm13,xmm7
	movdqa	xmm8,xmm11
	paddd	xmm10,xmm15
	movdqa	xmm6,xmm14
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	paddd	xmm10,xmm4
	psrld	xmm9,27
	movdqa	xmm7,xmm12
	pxor	xmm6,xmm13

	pslld	xmm7,30
	por	xmm8,xmm9
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
	por	xmm12,xmm7
	movdqa	xmm0,XMMWORD PTR[rbx]
	mov	ecx,1
	cmp	ecx,DWORD PTR[rbx]
	pxor	xmm8,xmm8
	cmovge	r8,rbp
	cmp	ecx,DWORD PTR[4+rbx]
	movdqa	xmm1,xmm0
	cmovge	r9,rbp
	cmp	ecx,DWORD PTR[8+rbx]
	pcmpgtd	xmm1,xmm8
	cmovge	r10,rbp
	cmp	ecx,DWORD PTR[12+rbx]
	paddd	xmm0,xmm1
	cmovge	r11,rbp

	movdqu	xmm6,XMMWORD PTR[rdi]
	pand	xmm10,xmm1
	movdqu	xmm7,XMMWORD PTR[32+rdi]
	pand	xmm11,xmm1
	paddd	xmm10,xmm6
	movdqu	xmm8,XMMWORD PTR[64+rdi]
	pand	xmm12,xmm1
	paddd	xmm11,xmm7
	movdqu	xmm9,XMMWORD PTR[96+rdi]
	pand	xmm13,xmm1
	paddd	xmm12,xmm8
	movdqu	xmm5,XMMWORD PTR[128+rdi]
	pand	xmm14,xmm1
	movdqu	XMMWORD PTR[rdi],xmm10
	paddd	xmm13,xmm9
	movdqu	XMMWORD PTR[32+rdi],xmm11
	paddd	xmm14,xmm5
	movdqu	XMMWORD PTR[64+rdi],xmm12
	movdqu	XMMWORD PTR[96+rdi],xmm13
	movdqu	XMMWORD PTR[128+rdi],xmm14

	movdqa	XMMWORD PTR[rbx],xmm0
	movdqa	xmm5,XMMWORD PTR[96+rbp]
	movdqa	xmm15,XMMWORD PTR[((-32))+rbp]
	dec	edx
	jnz	$L$oop

	mov	edx,DWORD PTR[280+rsp]
	lea	rdi,QWORD PTR[16+rdi]
	lea	rsi,QWORD PTR[64+rsi]
	dec	edx
	jnz	$L$oop_grande

$L$done::
	mov	rax,QWORD PTR[272+rsp]
	movaps	xmm6,XMMWORD PTR[((-184))+rax]
	movaps	xmm7,XMMWORD PTR[((-168))+rax]
	movaps	xmm8,XMMWORD PTR[((-152))+rax]
	movaps	xmm9,XMMWORD PTR[((-136))+rax]
	movaps	xmm10,XMMWORD PTR[((-120))+rax]
	movaps	xmm11,XMMWORD PTR[((-104))+rax]
	movaps	xmm12,XMMWORD PTR[((-88))+rax]
	movaps	xmm13,XMMWORD PTR[((-72))+rax]
	movaps	xmm14,XMMWORD PTR[((-56))+rax]
	movaps	xmm15,XMMWORD PTR[((-40))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_multi_block::
sha1_multi_block	ENDP

ALIGN	32
sha1_multi_block_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block_shaext::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8


_shaext_shortcut::
	mov	rax,rsp
	push	rbx
	push	rbp
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
	movaps	XMMWORD PTR[(-120)+rax],xmm10
	movaps	XMMWORD PTR[(-104)+rax],xmm11
	movaps	XMMWORD PTR[(-88)+rax],xmm12
	movaps	XMMWORD PTR[(-72)+rax],xmm13
	movaps	XMMWORD PTR[(-56)+rax],xmm14
	movaps	XMMWORD PTR[(-40)+rax],xmm15
	sub	rsp,288
	shl	edx,1
	and	rsp,-256
	lea	rdi,QWORD PTR[64+rdi]
	mov	QWORD PTR[272+rsp],rax
$L$body_shaext::
	lea	rbx,QWORD PTR[256+rsp]
	movdqa	xmm3,XMMWORD PTR[((K_XX_XX+128))]

$L$oop_grande_shaext::
	mov	DWORD PTR[280+rsp],edx
	xor	edx,edx
	mov	r8,QWORD PTR[rsi]
	mov	ecx,DWORD PTR[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[rbx],ecx
	cmovle	r8,rsp
	mov	r9,QWORD PTR[16+rsi]
	mov	ecx,DWORD PTR[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD PTR[4+rbx],ecx
	cmovle	r9,rsp
	test	edx,edx
	jz	$L$done_shaext

	movq	xmm0,QWORD PTR[((0-64))+rdi]
	movq	xmm4,QWORD PTR[((32-64))+rdi]
	movq	xmm5,QWORD PTR[((64-64))+rdi]
	movq	xmm6,QWORD PTR[((96-64))+rdi]
	movq	xmm7,QWORD PTR[((128-64))+rdi]

	punpckldq	xmm0,xmm4
	punpckldq	xmm5,xmm6

	movdqa	xmm8,xmm0
	punpcklqdq	xmm0,xmm5
	punpckhqdq	xmm8,xmm5

	pshufd	xmm1,xmm7,63
	pshufd	xmm9,xmm7,127
	pshufd	xmm0,xmm0,27
	pshufd	xmm8,xmm8,27
	jmp	$L$oop_shaext

ALIGN	32
$L$oop_shaext::
	movdqu	xmm4,XMMWORD PTR[r8]
	movdqu	xmm11,XMMWORD PTR[r9]
	movdqu	xmm5,XMMWORD PTR[16+r8]
	movdqu	xmm12,XMMWORD PTR[16+r9]
	movdqu	xmm6,XMMWORD PTR[32+r8]
DB	102,15,56,0,227
	movdqu	xmm13,XMMWORD PTR[32+r9]
DB	102,68,15,56,0,219
	movdqu	xmm7,XMMWORD PTR[48+r8]
	lea	r8,QWORD PTR[64+r8]
DB	102,15,56,0,235
	movdqu	xmm14,XMMWORD PTR[48+r9]
	lea	r9,QWORD PTR[64+r9]
DB	102,68,15,56,0,227

	movdqa	XMMWORD PTR[80+rsp],xmm1
	paddd	xmm1,xmm4
	movdqa	XMMWORD PTR[112+rsp],xmm9
	paddd	xmm9,xmm11
	movdqa	XMMWORD PTR[64+rsp],xmm0
	movdqa	xmm2,xmm0
	movdqa	XMMWORD PTR[96+rsp],xmm8
	movdqa	xmm10,xmm8
DB	15,58,204,193,0
DB	15,56,200,213
DB	69,15,58,204,193,0
DB	69,15,56,200,212
DB	102,15,56,0,243
	prefetcht0	[127+r8]
DB	15,56,201,229
DB	102,68,15,56,0,235
	prefetcht0	[127+r9]
DB	69,15,56,201,220

DB	102,15,56,0,251
	movdqa	xmm1,xmm0
DB	102,68,15,56,0,243
	movdqa	xmm9,xmm8
DB	15,58,204,194,0
DB	15,56,200,206
DB	69,15,58,204,194,0
DB	69,15,56,200,205
	pxor	xmm4,xmm6
DB	15,56,201,238
	pxor	xmm11,xmm13
DB	69,15,56,201,229
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,0
DB	15,56,200,215
DB	69,15,58,204,193,0
DB	69,15,56,200,214
DB	15,56,202,231
DB	69,15,56,202,222
	pxor	xmm5,xmm7
DB	15,56,201,247
	pxor	xmm12,xmm14
DB	69,15,56,201,238
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,0
DB	15,56,200,204
DB	69,15,58,204,194,0
DB	69,15,56,200,203
DB	15,56,202,236
DB	69,15,56,202,227
	pxor	xmm6,xmm4
DB	15,56,201,252
	pxor	xmm13,xmm11
DB	69,15,56,201,243
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,0
DB	15,56,200,213
DB	69,15,58,204,193,0
DB	69,15,56,200,212
DB	15,56,202,245
DB	69,15,56,202,236
	pxor	xmm7,xmm5
DB	15,56,201,229
	pxor	xmm14,xmm12
DB	69,15,56,201,220
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,1
DB	15,56,200,206
DB	69,15,58,204,194,1
DB	69,15,56,200,205
DB	15,56,202,254
DB	69,15,56,202,245
	pxor	xmm4,xmm6
DB	15,56,201,238
	pxor	xmm11,xmm13
DB	69,15,56,201,229
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,1
DB	15,56,200,215
DB	69,15,58,204,193,1
DB	69,15,56,200,214
DB	15,56,202,231
DB	69,15,56,202,222
	pxor	xmm5,xmm7
DB	15,56,201,247
	pxor	xmm12,xmm14
DB	69,15,56,201,238
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,1
DB	15,56,200,204
DB	69,15,58,204,194,1
DB	69,15,56,200,203
DB	15,56,202,236
DB	69,15,56,202,227
	pxor	xmm6,xmm4
DB	15,56,201,252
	pxor	xmm13,xmm11
DB	69,15,56,201,243
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,1
DB	15,56,200,213
DB	69,15,58,204,193,1
DB	69,15,56,200,212
DB	15,56,202,245
DB	69,15,56,202,236
	pxor	xmm7,xmm5
DB	15,56,201,229
	pxor	xmm14,xmm12
DB	69,15,56,201,220
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,1
DB	15,56,200,206
DB	69,15,58,204,194,1
DB	69,15,56,200,205
DB	15,56,202,254
DB	69,15,56,202,245
	pxor	xmm4,xmm6
DB	15,56,201,238
	pxor	xmm11,xmm13
DB	69,15,56,201,229
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,2
DB	15,56,200,215
DB	69,15,58,204,193,2
DB	69,15,56,200,214
DB	15,56,202,231
DB	69,15,56,202,222
	pxor	xmm5,xmm7
DB	15,56,201,247
	pxor	xmm12,xmm14
DB	69,15,56,201,238
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,2
DB	15,56,200,204
DB	69,15,58,204,194,2
DB	69,15,56,200,203
DB	15,56,202,236
DB	69,15,56,202,227
	pxor	xmm6,xmm4
DB	15,56,201,252
	pxor	xmm13,xmm11
DB	69,15,56,201,243
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,2
DB	15,56,200,213
DB	69,15,58,204,193,2
DB	69,15,56,200,212
DB	15,56,202,245
DB	69,15,56,202,236
	pxor	xmm7,xmm5
DB	15,56,201,229
	pxor	xmm14,xmm12
DB	69,15,56,201,220
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,2
DB	15,56,200,206
DB	69,15,58,204,194,2
DB	69,15,56,200,205
DB	15,56,202,254
DB	69,15,56,202,245
	pxor	xmm4,xmm6
DB	15,56,201,238
	pxor	xmm11,xmm13
DB	69,15,56,201,229
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,2
DB	15,56,200,215
DB	69,15,58,204,193,2
DB	69,15,56,200,214
DB	15,56,202,231
DB	69,15,56,202,222
	pxor	xmm5,xmm7
DB	15,56,201,247
	pxor	xmm12,xmm14
DB	69,15,56,201,238
	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,3
DB	15,56,200,204
DB	69,15,58,204,194,3
DB	69,15,56,200,203
DB	15,56,202,236
DB	69,15,56,202,227
	pxor	xmm6,xmm4
DB	15,56,201,252
	pxor	xmm13,xmm11
DB	69,15,56,201,243
	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,3
DB	15,56,200,213
DB	69,15,58,204,193,3
DB	69,15,56,200,212
DB	15,56,202,245
DB	69,15,56,202,236
	pxor	xmm7,xmm5
	pxor	xmm14,xmm12

	mov	ecx,1
	pxor	xmm4,xmm4
	cmp	ecx,DWORD PTR[rbx]
	cmovge	r8,rsp

	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,3
DB	15,56,200,206
DB	69,15,58,204,194,3
DB	69,15,56,200,205
DB	15,56,202,254
DB	69,15,56,202,245

	cmp	ecx,DWORD PTR[4+rbx]
	cmovge	r9,rsp
	movq	xmm6,QWORD PTR[rbx]

	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,3
DB	15,56,200,215
DB	69,15,58,204,193,3
DB	69,15,56,200,214

	pshufd	xmm11,xmm6,000h
	pshufd	xmm12,xmm6,055h
	movdqa	xmm7,xmm6
	pcmpgtd	xmm11,xmm4
	pcmpgtd	xmm12,xmm4

	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,3
DB	15,56,200,204
DB	69,15,58,204,194,3
DB	68,15,56,200,204

	pcmpgtd	xmm7,xmm4
	pand	xmm0,xmm11
	pand	xmm1,xmm11
	pand	xmm8,xmm12
	pand	xmm9,xmm12
	paddd	xmm6,xmm7

	paddd	xmm0,XMMWORD PTR[64+rsp]
	paddd	xmm1,XMMWORD PTR[80+rsp]
	paddd	xmm8,XMMWORD PTR[96+rsp]
	paddd	xmm9,XMMWORD PTR[112+rsp]

	movq	QWORD PTR[rbx],xmm6
	dec	edx
	jnz	$L$oop_shaext

	mov	edx,DWORD PTR[280+rsp]

	pshufd	xmm0,xmm0,27
	pshufd	xmm8,xmm8,27

	movdqa	xmm6,xmm0
	punpckldq	xmm0,xmm8
	punpckhdq	xmm6,xmm8
	punpckhdq	xmm1,xmm9
	movq	QWORD PTR[(0-64)+rdi],xmm0
	psrldq	xmm0,8
	movq	QWORD PTR[(64-64)+rdi],xmm6
	psrldq	xmm6,8
	movq	QWORD PTR[(32-64)+rdi],xmm0
	psrldq	xmm1,8
	movq	QWORD PTR[(96-64)+rdi],xmm6
	movq	QWORD PTR[(128-64)+rdi],xmm1

	lea	rdi,QWORD PTR[8+rdi]
	lea	rsi,QWORD PTR[32+rsi]
	dec	edx
	jnz	$L$oop_grande_shaext

$L$done_shaext::

	movaps	xmm6,XMMWORD PTR[((-184))+rax]
	movaps	xmm7,XMMWORD PTR[((-168))+rax]
	movaps	xmm8,XMMWORD PTR[((-152))+rax]
	movaps	xmm9,XMMWORD PTR[((-136))+rax]
	movaps	xmm10,XMMWORD PTR[((-120))+rax]
	movaps	xmm11,XMMWORD PTR[((-104))+rax]
	movaps	xmm12,XMMWORD PTR[((-88))+rax]
	movaps	xmm13,XMMWORD PTR[((-72))+rax]
	movaps	xmm14,XMMWORD PTR[((-56))+rax]
	movaps	xmm15,XMMWORD PTR[((-40))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	rbx,QWORD PTR[((-8))+rax]
	lea	rsp,QWORD PTR[rax]
$L$epilogue_shaext::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_sha1_multi_block_shaext::
sha1_multi_block_shaext	ENDP

ALIGN	256
	DD	05a827999h,05a827999h,05a827999h,05a827999h
	DD	05a827999h,05a827999h,05a827999h,05a827999h
K_XX_XX::
	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h
	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h
	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch
	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch
	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h
	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
DB	0fh,0eh,0dh,0ch,0bh,0ah,09h,08h,07h,06h,05h,04h,03h,02h,01h,00h
DB	83,72,65,49,32,109,117,108,116,105,45,98,108,111,99,107
DB	32,116,114,97,110,115,102,111,114,109,32,102,111,114,32,120
DB	56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77
DB	83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110
DB	115,115,108,46,111,114,103,62,0
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
	jb	$L$in_prologue

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$in_prologue

	mov	rax,QWORD PTR[272+rax]

	mov	rbx,QWORD PTR[((-8))+rax]
	mov	rbp,QWORD PTR[((-16))+rax]
	mov	QWORD PTR[144+r8],rbx
	mov	QWORD PTR[160+r8],rbp

	lea	rsi,QWORD PTR[((-24-160))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

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
	DD	imagerel $L$SEH_begin_sha1_multi_block
	DD	imagerel $L$SEH_end_sha1_multi_block
	DD	imagerel $L$SEH_info_sha1_multi_block
	DD	imagerel $L$SEH_begin_sha1_multi_block_shaext
	DD	imagerel $L$SEH_end_sha1_multi_block_shaext
	DD	imagerel $L$SEH_info_sha1_multi_block_shaext
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha1_multi_block::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$body,imagerel $L$epilogue
$L$SEH_info_sha1_multi_block_shaext::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$body_shaext,imagerel $L$epilogue_shaext

.xdata	ENDS
END
