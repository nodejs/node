OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	sha256_multi_block

ALIGN	32
sha256_multi_block	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block::
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
	lea	rbp,QWORD PTR[((K256+128))]
	lea	rbx,QWORD PTR[256+rsp]
	lea	rdi,QWORD PTR[128+rdi]

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

	movdqu	xmm8,XMMWORD PTR[((0-128))+rdi]
	lea	rax,QWORD PTR[128+rsp]
	movdqu	xmm9,XMMWORD PTR[((32-128))+rdi]
	movdqu	xmm10,XMMWORD PTR[((64-128))+rdi]
	movdqu	xmm11,XMMWORD PTR[((96-128))+rdi]
	movdqu	xmm12,XMMWORD PTR[((128-128))+rdi]
	movdqu	xmm13,XMMWORD PTR[((160-128))+rdi]
	movdqu	xmm14,XMMWORD PTR[((192-128))+rdi]
	movdqu	xmm15,XMMWORD PTR[((224-128))+rdi]
	movdqu	xmm6,XMMWORD PTR[$L$pbswap]
	jmp	$L$oop

ALIGN	32
$L$oop::
	movdqa	xmm4,xmm10
	pxor	xmm4,xmm9
	movd	xmm5,DWORD PTR[r8]
	movd	xmm0,DWORD PTR[r9]
	movd	xmm1,DWORD PTR[r10]
	movd	xmm2,DWORD PTR[r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm12
DB	102,15,56,0,238
	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(0-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-128))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm12

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm12
	pslld	xmm2,26-21
	pandn	xmm0,xmm14
	pand	xmm3,xmm13
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm8
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm8
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm9
	movdqa	xmm7,xmm8
	pslld	xmm2,10
	pxor	xmm3,xmm8


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm15,xmm9
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm15,xmm4
	paddd	xmm11,xmm5
	pxor	xmm7,xmm2

	paddd	xmm15,xmm5
	paddd	xmm15,xmm7
	movd	xmm5,DWORD PTR[4+r8]
	movd	xmm0,DWORD PTR[4+r9]
	movd	xmm1,DWORD PTR[4+r10]
	movd	xmm2,DWORD PTR[4+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(16-128)+rax],xmm5
	paddd	xmm5,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-96))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm11

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm11
	pslld	xmm2,26-21
	pandn	xmm0,xmm13
	pand	xmm4,xmm12
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm15
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm15
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm8
	movdqa	xmm7,xmm15
	pslld	xmm2,10
	pxor	xmm4,xmm15


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm14,xmm8
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm14,xmm3
	paddd	xmm10,xmm5
	pxor	xmm7,xmm2

	paddd	xmm14,xmm5
	paddd	xmm14,xmm7
	movd	xmm5,DWORD PTR[8+r8]
	movd	xmm0,DWORD PTR[8+r9]
	movd	xmm1,DWORD PTR[8+r10]
	movd	xmm2,DWORD PTR[8+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm10
DB	102,15,56,0,238
	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(32-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-64))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm10

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm10
	pslld	xmm2,26-21
	pandn	xmm0,xmm12
	pand	xmm3,xmm11
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm14
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm14
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm15
	movdqa	xmm7,xmm14
	pslld	xmm2,10
	pxor	xmm3,xmm14


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm13,xmm15
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm13,xmm4
	paddd	xmm9,xmm5
	pxor	xmm7,xmm2

	paddd	xmm13,xmm5
	paddd	xmm13,xmm7
	movd	xmm5,DWORD PTR[12+r8]
	movd	xmm0,DWORD PTR[12+r9]
	movd	xmm1,DWORD PTR[12+r10]
	movd	xmm2,DWORD PTR[12+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(48-128)+rax],xmm5
	paddd	xmm5,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-32))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm9

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm9
	pslld	xmm2,26-21
	pandn	xmm0,xmm11
	pand	xmm4,xmm10
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm13
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm13
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm14
	movdqa	xmm7,xmm13
	pslld	xmm2,10
	pxor	xmm4,xmm13


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm12,xmm14
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm12,xmm3
	paddd	xmm8,xmm5
	pxor	xmm7,xmm2

	paddd	xmm12,xmm5
	paddd	xmm12,xmm7
	movd	xmm5,DWORD PTR[16+r8]
	movd	xmm0,DWORD PTR[16+r9]
	movd	xmm1,DWORD PTR[16+r10]
	movd	xmm2,DWORD PTR[16+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm8
DB	102,15,56,0,238
	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(64-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm8

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm8
	pslld	xmm2,26-21
	pandn	xmm0,xmm10
	pand	xmm3,xmm9
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm12
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm12
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm13
	movdqa	xmm7,xmm12
	pslld	xmm2,10
	pxor	xmm3,xmm12


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm11,xmm13
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm11,xmm4
	paddd	xmm15,xmm5
	pxor	xmm7,xmm2

	paddd	xmm11,xmm5
	paddd	xmm11,xmm7
	movd	xmm5,DWORD PTR[20+r8]
	movd	xmm0,DWORD PTR[20+r9]
	movd	xmm1,DWORD PTR[20+r10]
	movd	xmm2,DWORD PTR[20+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(80-128)+rax],xmm5
	paddd	xmm5,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[32+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm15

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm15
	pslld	xmm2,26-21
	pandn	xmm0,xmm9
	pand	xmm4,xmm8
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm11
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm11
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm12
	movdqa	xmm7,xmm11
	pslld	xmm2,10
	pxor	xmm4,xmm11


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm10,xmm12
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm10,xmm3
	paddd	xmm14,xmm5
	pxor	xmm7,xmm2

	paddd	xmm10,xmm5
	paddd	xmm10,xmm7
	movd	xmm5,DWORD PTR[24+r8]
	movd	xmm0,DWORD PTR[24+r9]
	movd	xmm1,DWORD PTR[24+r10]
	movd	xmm2,DWORD PTR[24+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm14
DB	102,15,56,0,238
	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(96-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[64+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm14

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm14
	pslld	xmm2,26-21
	pandn	xmm0,xmm8
	pand	xmm3,xmm15
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm10
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm10
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm11
	movdqa	xmm7,xmm10
	pslld	xmm2,10
	pxor	xmm3,xmm10


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm9,xmm11
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm9,xmm4
	paddd	xmm13,xmm5
	pxor	xmm7,xmm2

	paddd	xmm9,xmm5
	paddd	xmm9,xmm7
	movd	xmm5,DWORD PTR[28+r8]
	movd	xmm0,DWORD PTR[28+r9]
	movd	xmm1,DWORD PTR[28+r10]
	movd	xmm2,DWORD PTR[28+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(112-128)+rax],xmm5
	paddd	xmm5,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[96+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm13

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm13
	pslld	xmm2,26-21
	pandn	xmm0,xmm15
	pand	xmm4,xmm14
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm9
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm9
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm10
	movdqa	xmm7,xmm9
	pslld	xmm2,10
	pxor	xmm4,xmm9


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm8,xmm10
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm8,xmm3
	paddd	xmm12,xmm5
	pxor	xmm7,xmm2

	paddd	xmm8,xmm5
	paddd	xmm8,xmm7
	lea	rbp,QWORD PTR[256+rbp]
	movd	xmm5,DWORD PTR[32+r8]
	movd	xmm0,DWORD PTR[32+r9]
	movd	xmm1,DWORD PTR[32+r10]
	movd	xmm2,DWORD PTR[32+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm12
DB	102,15,56,0,238
	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(128-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-128))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm12

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm12
	pslld	xmm2,26-21
	pandn	xmm0,xmm14
	pand	xmm3,xmm13
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm8
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm8
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm9
	movdqa	xmm7,xmm8
	pslld	xmm2,10
	pxor	xmm3,xmm8


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm15,xmm9
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm15,xmm4
	paddd	xmm11,xmm5
	pxor	xmm7,xmm2

	paddd	xmm15,xmm5
	paddd	xmm15,xmm7
	movd	xmm5,DWORD PTR[36+r8]
	movd	xmm0,DWORD PTR[36+r9]
	movd	xmm1,DWORD PTR[36+r10]
	movd	xmm2,DWORD PTR[36+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(144-128)+rax],xmm5
	paddd	xmm5,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-96))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm11

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm11
	pslld	xmm2,26-21
	pandn	xmm0,xmm13
	pand	xmm4,xmm12
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm15
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm15
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm8
	movdqa	xmm7,xmm15
	pslld	xmm2,10
	pxor	xmm4,xmm15


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm14,xmm8
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm14,xmm3
	paddd	xmm10,xmm5
	pxor	xmm7,xmm2

	paddd	xmm14,xmm5
	paddd	xmm14,xmm7
	movd	xmm5,DWORD PTR[40+r8]
	movd	xmm0,DWORD PTR[40+r9]
	movd	xmm1,DWORD PTR[40+r10]
	movd	xmm2,DWORD PTR[40+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm10
DB	102,15,56,0,238
	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(160-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-64))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm10

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm10
	pslld	xmm2,26-21
	pandn	xmm0,xmm12
	pand	xmm3,xmm11
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm14
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm14
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm15
	movdqa	xmm7,xmm14
	pslld	xmm2,10
	pxor	xmm3,xmm14


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm13,xmm15
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm13,xmm4
	paddd	xmm9,xmm5
	pxor	xmm7,xmm2

	paddd	xmm13,xmm5
	paddd	xmm13,xmm7
	movd	xmm5,DWORD PTR[44+r8]
	movd	xmm0,DWORD PTR[44+r9]
	movd	xmm1,DWORD PTR[44+r10]
	movd	xmm2,DWORD PTR[44+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(176-128)+rax],xmm5
	paddd	xmm5,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-32))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm9

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm9
	pslld	xmm2,26-21
	pandn	xmm0,xmm11
	pand	xmm4,xmm10
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm13
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm13
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm14
	movdqa	xmm7,xmm13
	pslld	xmm2,10
	pxor	xmm4,xmm13


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm12,xmm14
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm12,xmm3
	paddd	xmm8,xmm5
	pxor	xmm7,xmm2

	paddd	xmm12,xmm5
	paddd	xmm12,xmm7
	movd	xmm5,DWORD PTR[48+r8]
	movd	xmm0,DWORD PTR[48+r9]
	movd	xmm1,DWORD PTR[48+r10]
	movd	xmm2,DWORD PTR[48+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm8
DB	102,15,56,0,238
	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(192-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm8

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm8
	pslld	xmm2,26-21
	pandn	xmm0,xmm10
	pand	xmm3,xmm9
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm12
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm12
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm13
	movdqa	xmm7,xmm12
	pslld	xmm2,10
	pxor	xmm3,xmm12


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm11,xmm13
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm11,xmm4
	paddd	xmm15,xmm5
	pxor	xmm7,xmm2

	paddd	xmm11,xmm5
	paddd	xmm11,xmm7
	movd	xmm5,DWORD PTR[52+r8]
	movd	xmm0,DWORD PTR[52+r9]
	movd	xmm1,DWORD PTR[52+r10]
	movd	xmm2,DWORD PTR[52+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(208-128)+rax],xmm5
	paddd	xmm5,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[32+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm15

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm15
	pslld	xmm2,26-21
	pandn	xmm0,xmm9
	pand	xmm4,xmm8
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm11
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm11
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm12
	movdqa	xmm7,xmm11
	pslld	xmm2,10
	pxor	xmm4,xmm11


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm10,xmm12
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm10,xmm3
	paddd	xmm14,xmm5
	pxor	xmm7,xmm2

	paddd	xmm10,xmm5
	paddd	xmm10,xmm7
	movd	xmm5,DWORD PTR[56+r8]
	movd	xmm0,DWORD PTR[56+r9]
	movd	xmm1,DWORD PTR[56+r10]
	movd	xmm2,DWORD PTR[56+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm14
DB	102,15,56,0,238
	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(224-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[64+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm14

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm14
	pslld	xmm2,26-21
	pandn	xmm0,xmm8
	pand	xmm3,xmm15
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm10
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm10
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm11
	movdqa	xmm7,xmm10
	pslld	xmm2,10
	pxor	xmm3,xmm10


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm9,xmm11
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm9,xmm4
	paddd	xmm13,xmm5
	pxor	xmm7,xmm2

	paddd	xmm9,xmm5
	paddd	xmm9,xmm7
	movd	xmm5,DWORD PTR[60+r8]
	lea	r8,QWORD PTR[64+r8]
	movd	xmm0,DWORD PTR[60+r9]
	lea	r9,QWORD PTR[64+r9]
	movd	xmm1,DWORD PTR[60+r10]
	lea	r10,QWORD PTR[64+r10]
	movd	xmm2,DWORD PTR[60+r11]
	lea	r11,QWORD PTR[64+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(240-128)+rax],xmm5
	paddd	xmm5,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[96+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm13
	prefetcht0	[63+r8]
	pxor	xmm7,xmm2
	movdqa	xmm4,xmm13
	pslld	xmm2,26-21
	pandn	xmm0,xmm15
	pand	xmm4,xmm14
	pxor	xmm7,xmm1

	prefetcht0	[63+r9]
	movdqa	xmm1,xmm9
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm9
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm10
	movdqa	xmm7,xmm9
	pslld	xmm2,10
	pxor	xmm4,xmm9

	prefetcht0	[63+r10]
	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7

	prefetcht0	[63+r11]
	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm8,xmm10
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm8,xmm3
	paddd	xmm12,xmm5
	pxor	xmm7,xmm2

	paddd	xmm8,xmm5
	paddd	xmm8,xmm7
	lea	rbp,QWORD PTR[256+rbp]
	movdqu	xmm5,XMMWORD PTR[((0-128))+rax]
	mov	ecx,3
	jmp	$L$oop_16_xx
ALIGN	32
$L$oop_16_xx::
	movdqa	xmm6,XMMWORD PTR[((16-128))+rax]
	paddd	xmm5,XMMWORD PTR[((144-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((224-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm12

	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(0-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-128))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm12

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm12
	pslld	xmm2,26-21
	pandn	xmm0,xmm14
	pand	xmm3,xmm13
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm8
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm8
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm9
	movdqa	xmm7,xmm8
	pslld	xmm2,10
	pxor	xmm3,xmm8


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm15,xmm9
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm15,xmm4
	paddd	xmm11,xmm5
	pxor	xmm7,xmm2

	paddd	xmm15,xmm5
	paddd	xmm15,xmm7
	movdqa	xmm5,XMMWORD PTR[((32-128))+rax]
	paddd	xmm6,XMMWORD PTR[((160-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((240-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11

	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(16-128)+rax],xmm6
	paddd	xmm6,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[((-96))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm11

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm11
	pslld	xmm2,26-21
	pandn	xmm0,xmm13
	pand	xmm4,xmm12
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm15
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm15
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm8
	movdqa	xmm7,xmm15
	pslld	xmm2,10
	pxor	xmm4,xmm15


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm14,xmm8
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm14,xmm3
	paddd	xmm10,xmm6
	pxor	xmm7,xmm2

	paddd	xmm14,xmm6
	paddd	xmm14,xmm7
	movdqa	xmm6,XMMWORD PTR[((48-128))+rax]
	paddd	xmm5,XMMWORD PTR[((176-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((0-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm10

	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(32-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-64))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm10

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm10
	pslld	xmm2,26-21
	pandn	xmm0,xmm12
	pand	xmm3,xmm11
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm14
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm14
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm15
	movdqa	xmm7,xmm14
	pslld	xmm2,10
	pxor	xmm3,xmm14


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm13,xmm15
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm13,xmm4
	paddd	xmm9,xmm5
	pxor	xmm7,xmm2

	paddd	xmm13,xmm5
	paddd	xmm13,xmm7
	movdqa	xmm5,XMMWORD PTR[((64-128))+rax]
	paddd	xmm6,XMMWORD PTR[((192-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((16-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9

	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(48-128)+rax],xmm6
	paddd	xmm6,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[((-32))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm9

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm9
	pslld	xmm2,26-21
	pandn	xmm0,xmm11
	pand	xmm4,xmm10
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm13
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm13
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm14
	movdqa	xmm7,xmm13
	pslld	xmm2,10
	pxor	xmm4,xmm13


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm12,xmm14
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm12,xmm3
	paddd	xmm8,xmm6
	pxor	xmm7,xmm2

	paddd	xmm12,xmm6
	paddd	xmm12,xmm7
	movdqa	xmm6,XMMWORD PTR[((80-128))+rax]
	paddd	xmm5,XMMWORD PTR[((208-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((32-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm8

	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(64-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm8

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm8
	pslld	xmm2,26-21
	pandn	xmm0,xmm10
	pand	xmm3,xmm9
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm12
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm12
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm13
	movdqa	xmm7,xmm12
	pslld	xmm2,10
	pxor	xmm3,xmm12


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm11,xmm13
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm11,xmm4
	paddd	xmm15,xmm5
	pxor	xmm7,xmm2

	paddd	xmm11,xmm5
	paddd	xmm11,xmm7
	movdqa	xmm5,XMMWORD PTR[((96-128))+rax]
	paddd	xmm6,XMMWORD PTR[((224-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((48-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15

	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(80-128)+rax],xmm6
	paddd	xmm6,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[32+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm15

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm15
	pslld	xmm2,26-21
	pandn	xmm0,xmm9
	pand	xmm4,xmm8
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm11
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm11
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm12
	movdqa	xmm7,xmm11
	pslld	xmm2,10
	pxor	xmm4,xmm11


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm10,xmm12
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm10,xmm3
	paddd	xmm14,xmm6
	pxor	xmm7,xmm2

	paddd	xmm10,xmm6
	paddd	xmm10,xmm7
	movdqa	xmm6,XMMWORD PTR[((112-128))+rax]
	paddd	xmm5,XMMWORD PTR[((240-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((64-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm14

	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(96-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[64+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm14

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm14
	pslld	xmm2,26-21
	pandn	xmm0,xmm8
	pand	xmm3,xmm15
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm10
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm10
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm11
	movdqa	xmm7,xmm10
	pslld	xmm2,10
	pxor	xmm3,xmm10


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm9,xmm11
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm9,xmm4
	paddd	xmm13,xmm5
	pxor	xmm7,xmm2

	paddd	xmm9,xmm5
	paddd	xmm9,xmm7
	movdqa	xmm5,XMMWORD PTR[((128-128))+rax]
	paddd	xmm6,XMMWORD PTR[((0-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((80-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13

	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(112-128)+rax],xmm6
	paddd	xmm6,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[96+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm13

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm13
	pslld	xmm2,26-21
	pandn	xmm0,xmm15
	pand	xmm4,xmm14
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm9
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm9
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm10
	movdqa	xmm7,xmm9
	pslld	xmm2,10
	pxor	xmm4,xmm9


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm8,xmm10
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm8,xmm3
	paddd	xmm12,xmm6
	pxor	xmm7,xmm2

	paddd	xmm8,xmm6
	paddd	xmm8,xmm7
	lea	rbp,QWORD PTR[256+rbp]
	movdqa	xmm6,XMMWORD PTR[((144-128))+rax]
	paddd	xmm5,XMMWORD PTR[((16-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((96-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm12

	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(128-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-128))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm12

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm12
	pslld	xmm2,26-21
	pandn	xmm0,xmm14
	pand	xmm3,xmm13
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm8
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm8
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm9
	movdqa	xmm7,xmm8
	pslld	xmm2,10
	pxor	xmm3,xmm8


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm15,xmm9
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm15,xmm4
	paddd	xmm11,xmm5
	pxor	xmm7,xmm2

	paddd	xmm15,xmm5
	paddd	xmm15,xmm7
	movdqa	xmm5,XMMWORD PTR[((160-128))+rax]
	paddd	xmm6,XMMWORD PTR[((32-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((112-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11

	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(144-128)+rax],xmm6
	paddd	xmm6,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[((-96))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm11

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm11
	pslld	xmm2,26-21
	pandn	xmm0,xmm13
	pand	xmm4,xmm12
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm15
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm15
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm8
	movdqa	xmm7,xmm15
	pslld	xmm2,10
	pxor	xmm4,xmm15


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm14,xmm8
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm14,xmm3
	paddd	xmm10,xmm6
	pxor	xmm7,xmm2

	paddd	xmm14,xmm6
	paddd	xmm14,xmm7
	movdqa	xmm6,XMMWORD PTR[((176-128))+rax]
	paddd	xmm5,XMMWORD PTR[((48-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((128-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm10

	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(160-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[((-64))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm10

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm10
	pslld	xmm2,26-21
	pandn	xmm0,xmm12
	pand	xmm3,xmm11
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm14
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm14
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm15
	movdqa	xmm7,xmm14
	pslld	xmm2,10
	pxor	xmm3,xmm14


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm13,xmm15
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm13,xmm4
	paddd	xmm9,xmm5
	pxor	xmm7,xmm2

	paddd	xmm13,xmm5
	paddd	xmm13,xmm7
	movdqa	xmm5,XMMWORD PTR[((192-128))+rax]
	paddd	xmm6,XMMWORD PTR[((64-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((144-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9

	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(176-128)+rax],xmm6
	paddd	xmm6,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[((-32))+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm9

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm9
	pslld	xmm2,26-21
	pandn	xmm0,xmm11
	pand	xmm4,xmm10
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm13
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm13
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm14
	movdqa	xmm7,xmm13
	pslld	xmm2,10
	pxor	xmm4,xmm13


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm12,xmm14
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm12,xmm3
	paddd	xmm8,xmm6
	pxor	xmm7,xmm2

	paddd	xmm12,xmm6
	paddd	xmm12,xmm7
	movdqa	xmm6,XMMWORD PTR[((208-128))+rax]
	paddd	xmm5,XMMWORD PTR[((80-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((160-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm8

	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(192-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm8

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm8
	pslld	xmm2,26-21
	pandn	xmm0,xmm10
	pand	xmm3,xmm9
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm12
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm12
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm13
	movdqa	xmm7,xmm12
	pslld	xmm2,10
	pxor	xmm3,xmm12


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm11,xmm13
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm11,xmm4
	paddd	xmm15,xmm5
	pxor	xmm7,xmm2

	paddd	xmm11,xmm5
	paddd	xmm11,xmm7
	movdqa	xmm5,XMMWORD PTR[((224-128))+rax]
	paddd	xmm6,XMMWORD PTR[((96-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((176-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15

	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(208-128)+rax],xmm6
	paddd	xmm6,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[32+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm15

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm15
	pslld	xmm2,26-21
	pandn	xmm0,xmm9
	pand	xmm4,xmm8
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm11
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm11
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm12
	movdqa	xmm7,xmm11
	pslld	xmm2,10
	pxor	xmm4,xmm11


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm10,xmm12
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm10,xmm3
	paddd	xmm14,xmm6
	pxor	xmm7,xmm2

	paddd	xmm10,xmm6
	paddd	xmm10,xmm7
	movdqa	xmm6,XMMWORD PTR[((240-128))+rax]
	paddd	xmm5,XMMWORD PTR[((112-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((192-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm3,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm3

	psrld	xmm3,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	psrld	xmm3,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm3
	pxor	xmm0,xmm1
	paddd	xmm5,xmm0
	movdqa	xmm7,xmm14

	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(224-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD PTR[64+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm14

	pxor	xmm7,xmm2
	movdqa	xmm3,xmm14
	pslld	xmm2,26-21
	pandn	xmm0,xmm8
	pand	xmm3,xmm15
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm10
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm10
	psrld	xmm1,2
	paddd	xmm5,xmm7
	pxor	xmm0,xmm3
	movdqa	xmm3,xmm11
	movdqa	xmm7,xmm10
	pslld	xmm2,10
	pxor	xmm3,xmm10


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm5,xmm0
	pslld	xmm2,19-10
	pand	xmm4,xmm3
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm9,xmm11
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm9,xmm4
	paddd	xmm13,xmm5
	pxor	xmm7,xmm2

	paddd	xmm9,xmm5
	paddd	xmm9,xmm7
	movdqa	xmm5,XMMWORD PTR[((0-128))+rax]
	paddd	xmm6,XMMWORD PTR[((128-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD PTR[((208-128))+rax]
	pslld	xmm2,14
	pxor	xmm7,xmm1
	psrld	xmm1,18-7
	movdqa	xmm4,xmm0
	pxor	xmm7,xmm2
	pslld	xmm2,25-14
	pxor	xmm7,xmm1
	psrld	xmm0,10
	movdqa	xmm1,xmm4

	psrld	xmm4,17
	pxor	xmm7,xmm2
	pslld	xmm1,13
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	psrld	xmm4,19-17
	pxor	xmm0,xmm1
	pslld	xmm1,15-13
	pxor	xmm0,xmm4
	pxor	xmm0,xmm1
	paddd	xmm6,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13

	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD PTR[(240-128)+rax],xmm6
	paddd	xmm6,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD PTR[96+rbp]
	pxor	xmm7,xmm1

	psrld	xmm1,25-11
	movdqa	xmm0,xmm13

	pxor	xmm7,xmm2
	movdqa	xmm4,xmm13
	pslld	xmm2,26-21
	pandn	xmm0,xmm15
	pand	xmm4,xmm14
	pxor	xmm7,xmm1


	movdqa	xmm1,xmm9
	pxor	xmm7,xmm2
	movdqa	xmm2,xmm9
	psrld	xmm1,2
	paddd	xmm6,xmm7
	pxor	xmm0,xmm4
	movdqa	xmm4,xmm10
	movdqa	xmm7,xmm9
	pslld	xmm2,10
	pxor	xmm4,xmm9


	psrld	xmm7,13
	pxor	xmm1,xmm2
	paddd	xmm6,xmm0
	pslld	xmm2,19-10
	pand	xmm3,xmm4
	pxor	xmm1,xmm7


	psrld	xmm7,22-13
	pxor	xmm1,xmm2
	movdqa	xmm8,xmm10
	pslld	xmm2,30-19
	pxor	xmm7,xmm1
	pxor	xmm8,xmm3
	paddd	xmm12,xmm6
	pxor	xmm7,xmm2

	paddd	xmm8,xmm6
	paddd	xmm8,xmm7
	lea	rbp,QWORD PTR[256+rbp]
	dec	ecx
	jnz	$L$oop_16_xx

	mov	ecx,1
	lea	rbp,QWORD PTR[((K256+128))]

	movdqa	xmm7,XMMWORD PTR[rbx]
	cmp	ecx,DWORD PTR[rbx]
	pxor	xmm0,xmm0
	cmovge	r8,rbp
	cmp	ecx,DWORD PTR[4+rbx]
	movdqa	xmm6,xmm7
	cmovge	r9,rbp
	cmp	ecx,DWORD PTR[8+rbx]
	pcmpgtd	xmm6,xmm0
	cmovge	r10,rbp
	cmp	ecx,DWORD PTR[12+rbx]
	paddd	xmm7,xmm6
	cmovge	r11,rbp

	movdqu	xmm0,XMMWORD PTR[((0-128))+rdi]
	pand	xmm8,xmm6
	movdqu	xmm1,XMMWORD PTR[((32-128))+rdi]
	pand	xmm9,xmm6
	movdqu	xmm2,XMMWORD PTR[((64-128))+rdi]
	pand	xmm10,xmm6
	movdqu	xmm5,XMMWORD PTR[((96-128))+rdi]
	pand	xmm11,xmm6
	paddd	xmm8,xmm0
	movdqu	xmm0,XMMWORD PTR[((128-128))+rdi]
	pand	xmm12,xmm6
	paddd	xmm9,xmm1
	movdqu	xmm1,XMMWORD PTR[((160-128))+rdi]
	pand	xmm13,xmm6
	paddd	xmm10,xmm2
	movdqu	xmm2,XMMWORD PTR[((192-128))+rdi]
	pand	xmm14,xmm6
	paddd	xmm11,xmm5
	movdqu	xmm5,XMMWORD PTR[((224-128))+rdi]
	pand	xmm15,xmm6
	paddd	xmm12,xmm0
	paddd	xmm13,xmm1
	movdqu	XMMWORD PTR[(0-128)+rdi],xmm8
	paddd	xmm14,xmm2
	movdqu	XMMWORD PTR[(32-128)+rdi],xmm9
	paddd	xmm15,xmm5
	movdqu	XMMWORD PTR[(64-128)+rdi],xmm10
	movdqu	XMMWORD PTR[(96-128)+rdi],xmm11
	movdqu	XMMWORD PTR[(128-128)+rdi],xmm12
	movdqu	XMMWORD PTR[(160-128)+rdi],xmm13
	movdqu	XMMWORD PTR[(192-128)+rdi],xmm14
	movdqu	XMMWORD PTR[(224-128)+rdi],xmm15

	movdqa	XMMWORD PTR[rbx],xmm7
	movdqa	xmm6,XMMWORD PTR[$L$pbswap]
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
$L$SEH_end_sha256_multi_block::
sha256_multi_block	ENDP

ALIGN	32
sha256_multi_block_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block_shaext::
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
	lea	rdi,QWORD PTR[128+rdi]
	mov	QWORD PTR[272+rsp],rax
$L$body_shaext::
	lea	rbx,QWORD PTR[256+rsp]
	lea	rbp,QWORD PTR[((K256_shaext+128))]

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

	movq	xmm12,QWORD PTR[((0-128))+rdi]
	movq	xmm4,QWORD PTR[((32-128))+rdi]
	movq	xmm13,QWORD PTR[((64-128))+rdi]
	movq	xmm5,QWORD PTR[((96-128))+rdi]
	movq	xmm8,QWORD PTR[((128-128))+rdi]
	movq	xmm9,QWORD PTR[((160-128))+rdi]
	movq	xmm10,QWORD PTR[((192-128))+rdi]
	movq	xmm11,QWORD PTR[((224-128))+rdi]

	punpckldq	xmm12,xmm4
	punpckldq	xmm13,xmm5
	punpckldq	xmm8,xmm9
	punpckldq	xmm10,xmm11
	movdqa	xmm3,XMMWORD PTR[((K256_shaext-16))]

	movdqa	xmm14,xmm12
	movdqa	xmm15,xmm13
	punpcklqdq	xmm12,xmm8
	punpcklqdq	xmm13,xmm10
	punpckhqdq	xmm14,xmm8
	punpckhqdq	xmm15,xmm10

	pshufd	xmm12,xmm12,27
	pshufd	xmm13,xmm13,27
	pshufd	xmm14,xmm14,27
	pshufd	xmm15,xmm15,27
	jmp	$L$oop_shaext

ALIGN	32
$L$oop_shaext::
	movdqu	xmm4,XMMWORD PTR[r8]
	movdqu	xmm8,XMMWORD PTR[r9]
	movdqu	xmm5,XMMWORD PTR[16+r8]
	movdqu	xmm9,XMMWORD PTR[16+r9]
	movdqu	xmm6,XMMWORD PTR[32+r8]
DB	102,15,56,0,227
	movdqu	xmm10,XMMWORD PTR[32+r9]
DB	102,68,15,56,0,195
	movdqu	xmm7,XMMWORD PTR[48+r8]
	lea	r8,QWORD PTR[64+r8]
	movdqu	xmm11,XMMWORD PTR[48+r9]
	lea	r9,QWORD PTR[64+r9]

	movdqa	xmm0,XMMWORD PTR[((0-128))+rbp]
DB	102,15,56,0,235
	paddd	xmm0,xmm4
	pxor	xmm4,xmm12
	movdqa	xmm1,xmm0
	movdqa	xmm2,XMMWORD PTR[((0-128))+rbp]
DB	102,68,15,56,0,203
	paddd	xmm2,xmm8
	movdqa	XMMWORD PTR[80+rsp],xmm13
DB	69,15,56,203,236
	pxor	xmm8,xmm14
	movdqa	xmm0,xmm2
	movdqa	XMMWORD PTR[112+rsp],xmm15
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,00eh
	pxor	xmm4,xmm12
	movdqa	XMMWORD PTR[64+rsp],xmm12
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	pxor	xmm8,xmm14
	movdqa	XMMWORD PTR[96+rsp],xmm14
	movdqa	xmm1,XMMWORD PTR[((16-128))+rbp]
	paddd	xmm1,xmm5
DB	102,15,56,0,243
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((16-128))+rbp]
	paddd	xmm2,xmm9
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	prefetcht0	[127+r8]
DB	102,15,56,0,251
DB	102,68,15,56,0,211
	prefetcht0	[127+r9]
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,00eh
DB	102,68,15,56,0,219
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((32-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((32-128))+rbp]
	paddd	xmm2,xmm10
DB	69,15,56,203,236
DB	69,15,56,204,193
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,00eh
DB	102,15,58,15,222,4
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((48-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((48-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,00eh
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((64-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((64-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,00eh
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((80-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((80-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,00eh
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((96-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247
DB	69,15,56,204,193
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((96-128))+rbp]
	paddd	xmm11,xmm3
	paddd	xmm2,xmm10
DB	15,56,205,254
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	102,15,58,15,222,4
DB	69,15,56,203,254
DB	69,15,56,205,218
	pshufd	xmm0,xmm1,00eh
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((112-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((112-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,00eh
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((128-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((128-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,00eh
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((144-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((144-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,00eh
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((160-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247
DB	69,15,56,204,193
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((160-128))+rbp]
	paddd	xmm11,xmm3
	paddd	xmm2,xmm10
DB	15,56,205,254
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	102,15,58,15,222,4
DB	69,15,56,203,254
DB	69,15,56,205,218
	pshufd	xmm0,xmm1,00eh
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((176-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((176-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,00eh
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((192-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((192-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,00eh
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((208-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((208-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,00eh
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
	nop
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm1,XMMWORD PTR[((224-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD PTR[((224-128))+rbp]
	paddd	xmm11,xmm3
	paddd	xmm2,xmm10
DB	15,56,205,254
	nop
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	mov	ecx,1
	pxor	xmm6,xmm6
DB	69,15,56,203,254
DB	69,15,56,205,218
	pshufd	xmm0,xmm1,00eh
	movdqa	xmm1,XMMWORD PTR[((240-128))+rbp]
	paddd	xmm1,xmm7
	movq	xmm7,QWORD PTR[rbx]
	nop
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	movdqa	xmm2,XMMWORD PTR[((240-128))+rbp]
	paddd	xmm2,xmm11
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	cmp	ecx,DWORD PTR[rbx]
	cmovge	r8,rsp
	cmp	ecx,DWORD PTR[4+rbx]
	cmovge	r9,rsp
	pshufd	xmm9,xmm7,000h
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	pshufd	xmm10,xmm7,055h
	movdqa	xmm11,xmm7
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,00eh
	pcmpgtd	xmm9,xmm6
	pcmpgtd	xmm10,xmm6
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,00eh
	pcmpgtd	xmm11,xmm6
	movdqa	xmm3,XMMWORD PTR[((K256_shaext-16))]
DB	69,15,56,203,247

	pand	xmm13,xmm9
	pand	xmm15,xmm10
	pand	xmm12,xmm9
	pand	xmm14,xmm10
	paddd	xmm11,xmm7

	paddd	xmm13,XMMWORD PTR[80+rsp]
	paddd	xmm15,XMMWORD PTR[112+rsp]
	paddd	xmm12,XMMWORD PTR[64+rsp]
	paddd	xmm14,XMMWORD PTR[96+rsp]

	movq	QWORD PTR[rbx],xmm11
	dec	edx
	jnz	$L$oop_shaext

	mov	edx,DWORD PTR[280+rsp]

	pshufd	xmm12,xmm12,27
	pshufd	xmm13,xmm13,27
	pshufd	xmm14,xmm14,27
	pshufd	xmm15,xmm15,27

	movdqa	xmm5,xmm12
	movdqa	xmm6,xmm13
	punpckldq	xmm12,xmm14
	punpckhdq	xmm5,xmm14
	punpckldq	xmm13,xmm15
	punpckhdq	xmm6,xmm15

	movq	QWORD PTR[(0-128)+rdi],xmm12
	psrldq	xmm12,8
	movq	QWORD PTR[(128-128)+rdi],xmm5
	psrldq	xmm5,8
	movq	QWORD PTR[(32-128)+rdi],xmm12
	movq	QWORD PTR[(160-128)+rdi],xmm5

	movq	QWORD PTR[(64-128)+rdi],xmm13
	psrldq	xmm13,8
	movq	QWORD PTR[(192-128)+rdi],xmm6
	psrldq	xmm6,8
	movq	QWORD PTR[(96-128)+rdi],xmm13
	movq	QWORD PTR[(224-128)+rdi],xmm6

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
$L$SEH_end_sha256_multi_block_shaext::
sha256_multi_block_shaext	ENDP
ALIGN	256
K256::
	DD	1116352408,1116352408,1116352408,1116352408
	DD	1116352408,1116352408,1116352408,1116352408
	DD	1899447441,1899447441,1899447441,1899447441
	DD	1899447441,1899447441,1899447441,1899447441
	DD	3049323471,3049323471,3049323471,3049323471
	DD	3049323471,3049323471,3049323471,3049323471
	DD	3921009573,3921009573,3921009573,3921009573
	DD	3921009573,3921009573,3921009573,3921009573
	DD	961987163,961987163,961987163,961987163
	DD	961987163,961987163,961987163,961987163
	DD	1508970993,1508970993,1508970993,1508970993
	DD	1508970993,1508970993,1508970993,1508970993
	DD	2453635748,2453635748,2453635748,2453635748
	DD	2453635748,2453635748,2453635748,2453635748
	DD	2870763221,2870763221,2870763221,2870763221
	DD	2870763221,2870763221,2870763221,2870763221
	DD	3624381080,3624381080,3624381080,3624381080
	DD	3624381080,3624381080,3624381080,3624381080
	DD	310598401,310598401,310598401,310598401
	DD	310598401,310598401,310598401,310598401
	DD	607225278,607225278,607225278,607225278
	DD	607225278,607225278,607225278,607225278
	DD	1426881987,1426881987,1426881987,1426881987
	DD	1426881987,1426881987,1426881987,1426881987
	DD	1925078388,1925078388,1925078388,1925078388
	DD	1925078388,1925078388,1925078388,1925078388
	DD	2162078206,2162078206,2162078206,2162078206
	DD	2162078206,2162078206,2162078206,2162078206
	DD	2614888103,2614888103,2614888103,2614888103
	DD	2614888103,2614888103,2614888103,2614888103
	DD	3248222580,3248222580,3248222580,3248222580
	DD	3248222580,3248222580,3248222580,3248222580
	DD	3835390401,3835390401,3835390401,3835390401
	DD	3835390401,3835390401,3835390401,3835390401
	DD	4022224774,4022224774,4022224774,4022224774
	DD	4022224774,4022224774,4022224774,4022224774
	DD	264347078,264347078,264347078,264347078
	DD	264347078,264347078,264347078,264347078
	DD	604807628,604807628,604807628,604807628
	DD	604807628,604807628,604807628,604807628
	DD	770255983,770255983,770255983,770255983
	DD	770255983,770255983,770255983,770255983
	DD	1249150122,1249150122,1249150122,1249150122
	DD	1249150122,1249150122,1249150122,1249150122
	DD	1555081692,1555081692,1555081692,1555081692
	DD	1555081692,1555081692,1555081692,1555081692
	DD	1996064986,1996064986,1996064986,1996064986
	DD	1996064986,1996064986,1996064986,1996064986
	DD	2554220882,2554220882,2554220882,2554220882
	DD	2554220882,2554220882,2554220882,2554220882
	DD	2821834349,2821834349,2821834349,2821834349
	DD	2821834349,2821834349,2821834349,2821834349
	DD	2952996808,2952996808,2952996808,2952996808
	DD	2952996808,2952996808,2952996808,2952996808
	DD	3210313671,3210313671,3210313671,3210313671
	DD	3210313671,3210313671,3210313671,3210313671
	DD	3336571891,3336571891,3336571891,3336571891
	DD	3336571891,3336571891,3336571891,3336571891
	DD	3584528711,3584528711,3584528711,3584528711
	DD	3584528711,3584528711,3584528711,3584528711
	DD	113926993,113926993,113926993,113926993
	DD	113926993,113926993,113926993,113926993
	DD	338241895,338241895,338241895,338241895
	DD	338241895,338241895,338241895,338241895
	DD	666307205,666307205,666307205,666307205
	DD	666307205,666307205,666307205,666307205
	DD	773529912,773529912,773529912,773529912
	DD	773529912,773529912,773529912,773529912
	DD	1294757372,1294757372,1294757372,1294757372
	DD	1294757372,1294757372,1294757372,1294757372
	DD	1396182291,1396182291,1396182291,1396182291
	DD	1396182291,1396182291,1396182291,1396182291
	DD	1695183700,1695183700,1695183700,1695183700
	DD	1695183700,1695183700,1695183700,1695183700
	DD	1986661051,1986661051,1986661051,1986661051
	DD	1986661051,1986661051,1986661051,1986661051
	DD	2177026350,2177026350,2177026350,2177026350
	DD	2177026350,2177026350,2177026350,2177026350
	DD	2456956037,2456956037,2456956037,2456956037
	DD	2456956037,2456956037,2456956037,2456956037
	DD	2730485921,2730485921,2730485921,2730485921
	DD	2730485921,2730485921,2730485921,2730485921
	DD	2820302411,2820302411,2820302411,2820302411
	DD	2820302411,2820302411,2820302411,2820302411
	DD	3259730800,3259730800,3259730800,3259730800
	DD	3259730800,3259730800,3259730800,3259730800
	DD	3345764771,3345764771,3345764771,3345764771
	DD	3345764771,3345764771,3345764771,3345764771
	DD	3516065817,3516065817,3516065817,3516065817
	DD	3516065817,3516065817,3516065817,3516065817
	DD	3600352804,3600352804,3600352804,3600352804
	DD	3600352804,3600352804,3600352804,3600352804
	DD	4094571909,4094571909,4094571909,4094571909
	DD	4094571909,4094571909,4094571909,4094571909
	DD	275423344,275423344,275423344,275423344
	DD	275423344,275423344,275423344,275423344
	DD	430227734,430227734,430227734,430227734
	DD	430227734,430227734,430227734,430227734
	DD	506948616,506948616,506948616,506948616
	DD	506948616,506948616,506948616,506948616
	DD	659060556,659060556,659060556,659060556
	DD	659060556,659060556,659060556,659060556
	DD	883997877,883997877,883997877,883997877
	DD	883997877,883997877,883997877,883997877
	DD	958139571,958139571,958139571,958139571
	DD	958139571,958139571,958139571,958139571
	DD	1322822218,1322822218,1322822218,1322822218
	DD	1322822218,1322822218,1322822218,1322822218
	DD	1537002063,1537002063,1537002063,1537002063
	DD	1537002063,1537002063,1537002063,1537002063
	DD	1747873779,1747873779,1747873779,1747873779
	DD	1747873779,1747873779,1747873779,1747873779
	DD	1955562222,1955562222,1955562222,1955562222
	DD	1955562222,1955562222,1955562222,1955562222
	DD	2024104815,2024104815,2024104815,2024104815
	DD	2024104815,2024104815,2024104815,2024104815
	DD	2227730452,2227730452,2227730452,2227730452
	DD	2227730452,2227730452,2227730452,2227730452
	DD	2361852424,2361852424,2361852424,2361852424
	DD	2361852424,2361852424,2361852424,2361852424
	DD	2428436474,2428436474,2428436474,2428436474
	DD	2428436474,2428436474,2428436474,2428436474
	DD	2756734187,2756734187,2756734187,2756734187
	DD	2756734187,2756734187,2756734187,2756734187
	DD	3204031479,3204031479,3204031479,3204031479
	DD	3204031479,3204031479,3204031479,3204031479
	DD	3329325298,3329325298,3329325298,3329325298
	DD	3329325298,3329325298,3329325298,3329325298
$L$pbswap::
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
K256_shaext::
	DD	0428a2f98h,071374491h,0b5c0fbcfh,0e9b5dba5h
	DD	03956c25bh,059f111f1h,0923f82a4h,0ab1c5ed5h
	DD	0d807aa98h,012835b01h,0243185beh,0550c7dc3h
	DD	072be5d74h,080deb1feh,09bdc06a7h,0c19bf174h
	DD	0e49b69c1h,0efbe4786h,00fc19dc6h,0240ca1cch
	DD	02de92c6fh,04a7484aah,05cb0a9dch,076f988dah
	DD	0983e5152h,0a831c66dh,0b00327c8h,0bf597fc7h
	DD	0c6e00bf3h,0d5a79147h,006ca6351h,014292967h
	DD	027b70a85h,02e1b2138h,04d2c6dfch,053380d13h
	DD	0650a7354h,0766a0abbh,081c2c92eh,092722c85h
	DD	0a2bfe8a1h,0a81a664bh,0c24b8b70h,0c76c51a3h
	DD	0d192e819h,0d6990624h,0f40e3585h,0106aa070h
	DD	019a4c116h,01e376c08h,02748774ch,034b0bcb5h
	DD	0391c0cb3h,04ed8aa4ah,05b9cca4fh,0682e6ff3h
	DD	0748f82eeh,078a5636fh,084c87814h,08cc70208h
	DD	090befffah,0a4506cebh,0bef9a3f7h,0c67178f2h
DB	83,72,65,50,53,54,32,109,117,108,116,105,45,98,108,111
DB	99,107,32,116,114,97,110,115,102,111,114,109,32,102,111,114
DB	32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71
DB	65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112
DB	101,110,115,115,108,46,111,114,103,62,0
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
	DD	imagerel $L$SEH_begin_sha256_multi_block
	DD	imagerel $L$SEH_end_sha256_multi_block
	DD	imagerel $L$SEH_info_sha256_multi_block
	DD	imagerel $L$SEH_begin_sha256_multi_block_shaext
	DD	imagerel $L$SEH_end_sha256_multi_block_shaext
	DD	imagerel $L$SEH_info_sha256_multi_block_shaext
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_sha256_multi_block::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$body,imagerel $L$epilogue
$L$SEH_info_sha256_multi_block_shaext::
DB	9,0,0,0
	DD	imagerel se_handler
	DD	imagerel $L$body_shaext,imagerel $L$epilogue_shaext

.xdata	ENDS
END
