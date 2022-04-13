default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

global	sha256_multi_block

ALIGN	32
sha256_multi_block:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



	mov	rcx,QWORD[((OPENSSL_ia32cap_P+4))]
	bt	rcx,61
	jc	NEAR _shaext_shortcut
	test	ecx,268435456
	jnz	NEAR _avx_shortcut
	mov	rax,rsp

	push	rbx

	push	rbp

	lea	rsp,[((-168))+rsp]
	movaps	XMMWORD[rsp],xmm6
	movaps	XMMWORD[16+rsp],xmm7
	movaps	XMMWORD[32+rsp],xmm8
	movaps	XMMWORD[48+rsp],xmm9
	movaps	XMMWORD[(-120)+rax],xmm10
	movaps	XMMWORD[(-104)+rax],xmm11
	movaps	XMMWORD[(-88)+rax],xmm12
	movaps	XMMWORD[(-72)+rax],xmm13
	movaps	XMMWORD[(-56)+rax],xmm14
	movaps	XMMWORD[(-40)+rax],xmm15
	sub	rsp,288
	and	rsp,-256
	mov	QWORD[272+rsp],rax

$L$body:
	lea	rbp,[((K256+128))]
	lea	rbx,[256+rsp]
	lea	rdi,[128+rdi]

$L$oop_grande:
	mov	DWORD[280+rsp],edx
	xor	edx,edx

	mov	r8,QWORD[rsi]

	mov	ecx,DWORD[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[rbx],ecx
	cmovle	r8,rbp

	mov	r9,QWORD[16+rsi]

	mov	ecx,DWORD[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[4+rbx],ecx
	cmovle	r9,rbp

	mov	r10,QWORD[32+rsi]

	mov	ecx,DWORD[40+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[8+rbx],ecx
	cmovle	r10,rbp

	mov	r11,QWORD[48+rsi]

	mov	ecx,DWORD[56+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[12+rbx],ecx
	cmovle	r11,rbp
	test	edx,edx
	jz	NEAR $L$done

	movdqu	xmm8,XMMWORD[((0-128))+rdi]
	lea	rax,[128+rsp]
	movdqu	xmm9,XMMWORD[((32-128))+rdi]
	movdqu	xmm10,XMMWORD[((64-128))+rdi]
	movdqu	xmm11,XMMWORD[((96-128))+rdi]
	movdqu	xmm12,XMMWORD[((128-128))+rdi]
	movdqu	xmm13,XMMWORD[((160-128))+rdi]
	movdqu	xmm14,XMMWORD[((192-128))+rdi]
	movdqu	xmm15,XMMWORD[((224-128))+rdi]
	movdqu	xmm6,XMMWORD[$L$pbswap]
	jmp	NEAR $L$oop

ALIGN	32
$L$oop:
	movdqa	xmm4,xmm10
	pxor	xmm4,xmm9
	movd	xmm5,DWORD[r8]
	movd	xmm0,DWORD[r9]
	movd	xmm1,DWORD[r10]
	movd	xmm2,DWORD[r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm12
DB	102,15,56,0,238
	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD[(0-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-128))+rbp]
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
	movd	xmm5,DWORD[4+r8]
	movd	xmm0,DWORD[4+r9]
	movd	xmm1,DWORD[4+r10]
	movd	xmm2,DWORD[4+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD[(16-128)+rax],xmm5
	paddd	xmm5,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-96))+rbp]
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
	movd	xmm5,DWORD[8+r8]
	movd	xmm0,DWORD[8+r9]
	movd	xmm1,DWORD[8+r10]
	movd	xmm2,DWORD[8+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm10
DB	102,15,56,0,238
	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD[(32-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-64))+rbp]
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
	movd	xmm5,DWORD[12+r8]
	movd	xmm0,DWORD[12+r9]
	movd	xmm1,DWORD[12+r10]
	movd	xmm2,DWORD[12+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD[(48-128)+rax],xmm5
	paddd	xmm5,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-32))+rbp]
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
	movd	xmm5,DWORD[16+r8]
	movd	xmm0,DWORD[16+r9]
	movd	xmm1,DWORD[16+r10]
	movd	xmm2,DWORD[16+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm8
DB	102,15,56,0,238
	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD[(64-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[rbp]
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
	movd	xmm5,DWORD[20+r8]
	movd	xmm0,DWORD[20+r9]
	movd	xmm1,DWORD[20+r10]
	movd	xmm2,DWORD[20+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD[(80-128)+rax],xmm5
	paddd	xmm5,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[32+rbp]
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
	movd	xmm5,DWORD[24+r8]
	movd	xmm0,DWORD[24+r9]
	movd	xmm1,DWORD[24+r10]
	movd	xmm2,DWORD[24+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm14
DB	102,15,56,0,238
	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD[(96-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[64+rbp]
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
	movd	xmm5,DWORD[28+r8]
	movd	xmm0,DWORD[28+r9]
	movd	xmm1,DWORD[28+r10]
	movd	xmm2,DWORD[28+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD[(112-128)+rax],xmm5
	paddd	xmm5,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[96+rbp]
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
	lea	rbp,[256+rbp]
	movd	xmm5,DWORD[32+r8]
	movd	xmm0,DWORD[32+r9]
	movd	xmm1,DWORD[32+r10]
	movd	xmm2,DWORD[32+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm12
DB	102,15,56,0,238
	movdqa	xmm2,xmm12

	psrld	xmm7,6
	movdqa	xmm1,xmm12
	pslld	xmm2,7
	movdqa	XMMWORD[(128-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-128))+rbp]
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
	movd	xmm5,DWORD[36+r8]
	movd	xmm0,DWORD[36+r9]
	movd	xmm1,DWORD[36+r10]
	movd	xmm2,DWORD[36+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm11

	movdqa	xmm2,xmm11
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm11
	pslld	xmm2,7
	movdqa	XMMWORD[(144-128)+rax],xmm5
	paddd	xmm5,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-96))+rbp]
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
	movd	xmm5,DWORD[40+r8]
	movd	xmm0,DWORD[40+r9]
	movd	xmm1,DWORD[40+r10]
	movd	xmm2,DWORD[40+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm10
DB	102,15,56,0,238
	movdqa	xmm2,xmm10

	psrld	xmm7,6
	movdqa	xmm1,xmm10
	pslld	xmm2,7
	movdqa	XMMWORD[(160-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-64))+rbp]
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
	movd	xmm5,DWORD[44+r8]
	movd	xmm0,DWORD[44+r9]
	movd	xmm1,DWORD[44+r10]
	movd	xmm2,DWORD[44+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm9

	movdqa	xmm2,xmm9
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm9
	pslld	xmm2,7
	movdqa	XMMWORD[(176-128)+rax],xmm5
	paddd	xmm5,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-32))+rbp]
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
	movd	xmm5,DWORD[48+r8]
	movd	xmm0,DWORD[48+r9]
	movd	xmm1,DWORD[48+r10]
	movd	xmm2,DWORD[48+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm8
DB	102,15,56,0,238
	movdqa	xmm2,xmm8

	psrld	xmm7,6
	movdqa	xmm1,xmm8
	pslld	xmm2,7
	movdqa	XMMWORD[(192-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[rbp]
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
	movd	xmm5,DWORD[52+r8]
	movd	xmm0,DWORD[52+r9]
	movd	xmm1,DWORD[52+r10]
	movd	xmm2,DWORD[52+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm15

	movdqa	xmm2,xmm15
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm15
	pslld	xmm2,7
	movdqa	XMMWORD[(208-128)+rax],xmm5
	paddd	xmm5,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[32+rbp]
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
	movd	xmm5,DWORD[56+r8]
	movd	xmm0,DWORD[56+r9]
	movd	xmm1,DWORD[56+r10]
	movd	xmm2,DWORD[56+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm14
DB	102,15,56,0,238
	movdqa	xmm2,xmm14

	psrld	xmm7,6
	movdqa	xmm1,xmm14
	pslld	xmm2,7
	movdqa	XMMWORD[(224-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[64+rbp]
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
	movd	xmm5,DWORD[60+r8]
	lea	r8,[64+r8]
	movd	xmm0,DWORD[60+r9]
	lea	r9,[64+r9]
	movd	xmm1,DWORD[60+r10]
	lea	r10,[64+r10]
	movd	xmm2,DWORD[60+r11]
	lea	r11,[64+r11]
	punpckldq	xmm5,xmm1
	punpckldq	xmm0,xmm2
	punpckldq	xmm5,xmm0
	movdqa	xmm7,xmm13

	movdqa	xmm2,xmm13
DB	102,15,56,0,238
	psrld	xmm7,6
	movdqa	xmm1,xmm13
	pslld	xmm2,7
	movdqa	XMMWORD[(240-128)+rax],xmm5
	paddd	xmm5,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[96+rbp]
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
	lea	rbp,[256+rbp]
	movdqu	xmm5,XMMWORD[((0-128))+rax]
	mov	ecx,3
	jmp	NEAR $L$oop_16_xx
ALIGN	32
$L$oop_16_xx:
	movdqa	xmm6,XMMWORD[((16-128))+rax]
	paddd	xmm5,XMMWORD[((144-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((224-128))+rax]
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
	movdqa	XMMWORD[(0-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-128))+rbp]
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
	movdqa	xmm5,XMMWORD[((32-128))+rax]
	paddd	xmm6,XMMWORD[((160-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((240-128))+rax]
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
	movdqa	XMMWORD[(16-128)+rax],xmm6
	paddd	xmm6,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[((-96))+rbp]
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
	movdqa	xmm6,XMMWORD[((48-128))+rax]
	paddd	xmm5,XMMWORD[((176-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((0-128))+rax]
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
	movdqa	XMMWORD[(32-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-64))+rbp]
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
	movdqa	xmm5,XMMWORD[((64-128))+rax]
	paddd	xmm6,XMMWORD[((192-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((16-128))+rax]
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
	movdqa	XMMWORD[(48-128)+rax],xmm6
	paddd	xmm6,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[((-32))+rbp]
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
	movdqa	xmm6,XMMWORD[((80-128))+rax]
	paddd	xmm5,XMMWORD[((208-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((32-128))+rax]
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
	movdqa	XMMWORD[(64-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[rbp]
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
	movdqa	xmm5,XMMWORD[((96-128))+rax]
	paddd	xmm6,XMMWORD[((224-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((48-128))+rax]
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
	movdqa	XMMWORD[(80-128)+rax],xmm6
	paddd	xmm6,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[32+rbp]
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
	movdqa	xmm6,XMMWORD[((112-128))+rax]
	paddd	xmm5,XMMWORD[((240-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((64-128))+rax]
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
	movdqa	XMMWORD[(96-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[64+rbp]
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
	movdqa	xmm5,XMMWORD[((128-128))+rax]
	paddd	xmm6,XMMWORD[((0-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((80-128))+rax]
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
	movdqa	XMMWORD[(112-128)+rax],xmm6
	paddd	xmm6,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[96+rbp]
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
	lea	rbp,[256+rbp]
	movdqa	xmm6,XMMWORD[((144-128))+rax]
	paddd	xmm5,XMMWORD[((16-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((96-128))+rax]
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
	movdqa	XMMWORD[(128-128)+rax],xmm5
	paddd	xmm5,xmm15

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-128))+rbp]
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
	movdqa	xmm5,XMMWORD[((160-128))+rax]
	paddd	xmm6,XMMWORD[((32-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((112-128))+rax]
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
	movdqa	XMMWORD[(144-128)+rax],xmm6
	paddd	xmm6,xmm14

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[((-96))+rbp]
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
	movdqa	xmm6,XMMWORD[((176-128))+rax]
	paddd	xmm5,XMMWORD[((48-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((128-128))+rax]
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
	movdqa	XMMWORD[(160-128)+rax],xmm5
	paddd	xmm5,xmm13

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[((-64))+rbp]
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
	movdqa	xmm5,XMMWORD[((192-128))+rax]
	paddd	xmm6,XMMWORD[((64-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((144-128))+rax]
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
	movdqa	XMMWORD[(176-128)+rax],xmm6
	paddd	xmm6,xmm12

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[((-32))+rbp]
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
	movdqa	xmm6,XMMWORD[((208-128))+rax]
	paddd	xmm5,XMMWORD[((80-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((160-128))+rax]
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
	movdqa	XMMWORD[(192-128)+rax],xmm5
	paddd	xmm5,xmm11

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[rbp]
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
	movdqa	xmm5,XMMWORD[((224-128))+rax]
	paddd	xmm6,XMMWORD[((96-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((176-128))+rax]
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
	movdqa	XMMWORD[(208-128)+rax],xmm6
	paddd	xmm6,xmm10

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[32+rbp]
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
	movdqa	xmm6,XMMWORD[((240-128))+rax]
	paddd	xmm5,XMMWORD[((112-128))+rax]

	movdqa	xmm7,xmm6
	movdqa	xmm1,xmm6
	psrld	xmm7,3
	movdqa	xmm2,xmm6

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((192-128))+rax]
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
	movdqa	XMMWORD[(224-128)+rax],xmm5
	paddd	xmm5,xmm9

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm5,XMMWORD[64+rbp]
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
	movdqa	xmm5,XMMWORD[((0-128))+rax]
	paddd	xmm6,XMMWORD[((128-128))+rax]

	movdqa	xmm7,xmm5
	movdqa	xmm1,xmm5
	psrld	xmm7,3
	movdqa	xmm2,xmm5

	psrld	xmm1,7
	movdqa	xmm0,XMMWORD[((208-128))+rax]
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
	movdqa	XMMWORD[(240-128)+rax],xmm6
	paddd	xmm6,xmm8

	psrld	xmm1,11
	pxor	xmm7,xmm2
	pslld	xmm2,21-7
	paddd	xmm6,XMMWORD[96+rbp]
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
	lea	rbp,[256+rbp]
	dec	ecx
	jnz	NEAR $L$oop_16_xx

	mov	ecx,1
	lea	rbp,[((K256+128))]

	movdqa	xmm7,XMMWORD[rbx]
	cmp	ecx,DWORD[rbx]
	pxor	xmm0,xmm0
	cmovge	r8,rbp
	cmp	ecx,DWORD[4+rbx]
	movdqa	xmm6,xmm7
	cmovge	r9,rbp
	cmp	ecx,DWORD[8+rbx]
	pcmpgtd	xmm6,xmm0
	cmovge	r10,rbp
	cmp	ecx,DWORD[12+rbx]
	paddd	xmm7,xmm6
	cmovge	r11,rbp

	movdqu	xmm0,XMMWORD[((0-128))+rdi]
	pand	xmm8,xmm6
	movdqu	xmm1,XMMWORD[((32-128))+rdi]
	pand	xmm9,xmm6
	movdqu	xmm2,XMMWORD[((64-128))+rdi]
	pand	xmm10,xmm6
	movdqu	xmm5,XMMWORD[((96-128))+rdi]
	pand	xmm11,xmm6
	paddd	xmm8,xmm0
	movdqu	xmm0,XMMWORD[((128-128))+rdi]
	pand	xmm12,xmm6
	paddd	xmm9,xmm1
	movdqu	xmm1,XMMWORD[((160-128))+rdi]
	pand	xmm13,xmm6
	paddd	xmm10,xmm2
	movdqu	xmm2,XMMWORD[((192-128))+rdi]
	pand	xmm14,xmm6
	paddd	xmm11,xmm5
	movdqu	xmm5,XMMWORD[((224-128))+rdi]
	pand	xmm15,xmm6
	paddd	xmm12,xmm0
	paddd	xmm13,xmm1
	movdqu	XMMWORD[(0-128)+rdi],xmm8
	paddd	xmm14,xmm2
	movdqu	XMMWORD[(32-128)+rdi],xmm9
	paddd	xmm15,xmm5
	movdqu	XMMWORD[(64-128)+rdi],xmm10
	movdqu	XMMWORD[(96-128)+rdi],xmm11
	movdqu	XMMWORD[(128-128)+rdi],xmm12
	movdqu	XMMWORD[(160-128)+rdi],xmm13
	movdqu	XMMWORD[(192-128)+rdi],xmm14
	movdqu	XMMWORD[(224-128)+rdi],xmm15

	movdqa	XMMWORD[rbx],xmm7
	movdqa	xmm6,XMMWORD[$L$pbswap]
	dec	edx
	jnz	NEAR $L$oop

	mov	edx,DWORD[280+rsp]
	lea	rdi,[16+rdi]
	lea	rsi,[64+rsi]
	dec	edx
	jnz	NEAR $L$oop_grande

$L$done:
	mov	rax,QWORD[272+rsp]

	movaps	xmm6,XMMWORD[((-184))+rax]
	movaps	xmm7,XMMWORD[((-168))+rax]
	movaps	xmm8,XMMWORD[((-152))+rax]
	movaps	xmm9,XMMWORD[((-136))+rax]
	movaps	xmm10,XMMWORD[((-120))+rax]
	movaps	xmm11,XMMWORD[((-104))+rax]
	movaps	xmm12,XMMWORD[((-88))+rax]
	movaps	xmm13,XMMWORD[((-72))+rax]
	movaps	xmm14,XMMWORD[((-56))+rax]
	movaps	xmm15,XMMWORD[((-40))+rax]
	mov	rbp,QWORD[((-16))+rax]

	mov	rbx,QWORD[((-8))+rax]

	lea	rsp,[rax]

$L$epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha256_multi_block:

ALIGN	32
sha256_multi_block_shaext:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block_shaext:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_shaext_shortcut:
	mov	rax,rsp

	push	rbx

	push	rbp

	lea	rsp,[((-168))+rsp]
	movaps	XMMWORD[rsp],xmm6
	movaps	XMMWORD[16+rsp],xmm7
	movaps	XMMWORD[32+rsp],xmm8
	movaps	XMMWORD[48+rsp],xmm9
	movaps	XMMWORD[(-120)+rax],xmm10
	movaps	XMMWORD[(-104)+rax],xmm11
	movaps	XMMWORD[(-88)+rax],xmm12
	movaps	XMMWORD[(-72)+rax],xmm13
	movaps	XMMWORD[(-56)+rax],xmm14
	movaps	XMMWORD[(-40)+rax],xmm15
	sub	rsp,288
	shl	edx,1
	and	rsp,-256
	lea	rdi,[128+rdi]
	mov	QWORD[272+rsp],rax
$L$body_shaext:
	lea	rbx,[256+rsp]
	lea	rbp,[((K256_shaext+128))]

$L$oop_grande_shaext:
	mov	DWORD[280+rsp],edx
	xor	edx,edx

	mov	r8,QWORD[rsi]

	mov	ecx,DWORD[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[rbx],ecx
	cmovle	r8,rsp

	mov	r9,QWORD[16+rsi]

	mov	ecx,DWORD[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[4+rbx],ecx
	cmovle	r9,rsp
	test	edx,edx
	jz	NEAR $L$done_shaext

	movq	xmm12,QWORD[((0-128))+rdi]
	movq	xmm4,QWORD[((32-128))+rdi]
	movq	xmm13,QWORD[((64-128))+rdi]
	movq	xmm5,QWORD[((96-128))+rdi]
	movq	xmm8,QWORD[((128-128))+rdi]
	movq	xmm9,QWORD[((160-128))+rdi]
	movq	xmm10,QWORD[((192-128))+rdi]
	movq	xmm11,QWORD[((224-128))+rdi]

	punpckldq	xmm12,xmm4
	punpckldq	xmm13,xmm5
	punpckldq	xmm8,xmm9
	punpckldq	xmm10,xmm11
	movdqa	xmm3,XMMWORD[((K256_shaext-16))]

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
	jmp	NEAR $L$oop_shaext

ALIGN	32
$L$oop_shaext:
	movdqu	xmm4,XMMWORD[r8]
	movdqu	xmm8,XMMWORD[r9]
	movdqu	xmm5,XMMWORD[16+r8]
	movdqu	xmm9,XMMWORD[16+r9]
	movdqu	xmm6,XMMWORD[32+r8]
DB	102,15,56,0,227
	movdqu	xmm10,XMMWORD[32+r9]
DB	102,68,15,56,0,195
	movdqu	xmm7,XMMWORD[48+r8]
	lea	r8,[64+r8]
	movdqu	xmm11,XMMWORD[48+r9]
	lea	r9,[64+r9]

	movdqa	xmm0,XMMWORD[((0-128))+rbp]
DB	102,15,56,0,235
	paddd	xmm0,xmm4
	pxor	xmm4,xmm12
	movdqa	xmm1,xmm0
	movdqa	xmm2,XMMWORD[((0-128))+rbp]
DB	102,68,15,56,0,203
	paddd	xmm2,xmm8
	movdqa	XMMWORD[80+rsp],xmm13
DB	69,15,56,203,236
	pxor	xmm8,xmm14
	movdqa	xmm0,xmm2
	movdqa	XMMWORD[112+rsp],xmm15
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,0x0e
	pxor	xmm4,xmm12
	movdqa	XMMWORD[64+rsp],xmm12
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	pxor	xmm8,xmm14
	movdqa	XMMWORD[96+rsp],xmm14
	movdqa	xmm1,XMMWORD[((16-128))+rbp]
	paddd	xmm1,xmm5
DB	102,15,56,0,243
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((16-128))+rbp]
	paddd	xmm2,xmm9
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	prefetcht0	[127+r8]
DB	102,15,56,0,251
DB	102,68,15,56,0,211
	prefetcht0	[127+r9]
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,0x0e
DB	102,68,15,56,0,219
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((32-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((32-128))+rbp]
	paddd	xmm2,xmm10
DB	69,15,56,203,236
DB	69,15,56,204,193
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,0x0e
DB	102,15,58,15,222,4
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((48-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((48-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((64-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((64-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((80-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((80-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((96-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247
DB	69,15,56,204,193
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((96-128))+rbp]
	paddd	xmm11,xmm3
	paddd	xmm2,xmm10
DB	15,56,205,254
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	102,15,58,15,222,4
DB	69,15,56,203,254
DB	69,15,56,205,218
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((112-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((112-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((128-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((128-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((144-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((144-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
DB	15,56,204,229
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((160-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247
DB	69,15,56,204,193
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((160-128))+rbp]
	paddd	xmm11,xmm3
	paddd	xmm2,xmm10
DB	15,56,205,254
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm7
DB	102,15,58,15,222,4
DB	69,15,56,203,254
DB	69,15,56,205,218
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm4,xmm3
	movdqa	xmm3,xmm11
DB	102,65,15,58,15,218,4
DB	15,56,204,238
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((176-128))+rbp]
	paddd	xmm1,xmm7
DB	69,15,56,203,247
DB	69,15,56,204,202
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((176-128))+rbp]
	paddd	xmm8,xmm3
	paddd	xmm2,xmm11
DB	15,56,205,231
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm4
DB	102,15,58,15,223,4
DB	69,15,56,203,254
DB	69,15,56,205,195
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm5,xmm3
	movdqa	xmm3,xmm8
DB	102,65,15,58,15,219,4
DB	15,56,204,247
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((192-128))+rbp]
	paddd	xmm1,xmm4
DB	69,15,56,203,247
DB	69,15,56,204,211
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((192-128))+rbp]
	paddd	xmm9,xmm3
	paddd	xmm2,xmm8
DB	15,56,205,236
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm5
DB	102,15,58,15,220,4
DB	69,15,56,203,254
DB	69,15,56,205,200
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm6,xmm3
	movdqa	xmm3,xmm9
DB	102,65,15,58,15,216,4
DB	15,56,204,252
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((208-128))+rbp]
	paddd	xmm1,xmm5
DB	69,15,56,203,247
DB	69,15,56,204,216
	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((208-128))+rbp]
	paddd	xmm10,xmm3
	paddd	xmm2,xmm9
DB	15,56,205,245
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	movdqa	xmm3,xmm6
DB	102,15,58,15,221,4
DB	69,15,56,203,254
DB	69,15,56,205,209
	pshufd	xmm0,xmm1,0x0e
	paddd	xmm7,xmm3
	movdqa	xmm3,xmm10
DB	102,65,15,58,15,217,4
	nop
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm1,XMMWORD[((224-128))+rbp]
	paddd	xmm1,xmm6
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	movdqa	xmm2,XMMWORD[((224-128))+rbp]
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
	pshufd	xmm0,xmm1,0x0e
	movdqa	xmm1,XMMWORD[((240-128))+rbp]
	paddd	xmm1,xmm7
	movq	xmm7,QWORD[rbx]
	nop
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	movdqa	xmm2,XMMWORD[((240-128))+rbp]
	paddd	xmm2,xmm11
DB	69,15,56,203,247

	movdqa	xmm0,xmm1
	cmp	ecx,DWORD[rbx]
	cmovge	r8,rsp
	cmp	ecx,DWORD[4+rbx]
	cmovge	r9,rsp
	pshufd	xmm9,xmm7,0x00
DB	69,15,56,203,236
	movdqa	xmm0,xmm2
	pshufd	xmm10,xmm7,0x55
	movdqa	xmm11,xmm7
DB	69,15,56,203,254
	pshufd	xmm0,xmm1,0x0e
	pcmpgtd	xmm9,xmm6
	pcmpgtd	xmm10,xmm6
DB	69,15,56,203,229
	pshufd	xmm0,xmm2,0x0e
	pcmpgtd	xmm11,xmm6
	movdqa	xmm3,XMMWORD[((K256_shaext-16))]
DB	69,15,56,203,247

	pand	xmm13,xmm9
	pand	xmm15,xmm10
	pand	xmm12,xmm9
	pand	xmm14,xmm10
	paddd	xmm11,xmm7

	paddd	xmm13,XMMWORD[80+rsp]
	paddd	xmm15,XMMWORD[112+rsp]
	paddd	xmm12,XMMWORD[64+rsp]
	paddd	xmm14,XMMWORD[96+rsp]

	movq	QWORD[rbx],xmm11
	dec	edx
	jnz	NEAR $L$oop_shaext

	mov	edx,DWORD[280+rsp]

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

	movq	QWORD[(0-128)+rdi],xmm12
	psrldq	xmm12,8
	movq	QWORD[(128-128)+rdi],xmm5
	psrldq	xmm5,8
	movq	QWORD[(32-128)+rdi],xmm12
	movq	QWORD[(160-128)+rdi],xmm5

	movq	QWORD[(64-128)+rdi],xmm13
	psrldq	xmm13,8
	movq	QWORD[(192-128)+rdi],xmm6
	psrldq	xmm6,8
	movq	QWORD[(96-128)+rdi],xmm13
	movq	QWORD[(224-128)+rdi],xmm6

	lea	rdi,[8+rdi]
	lea	rsi,[32+rsi]
	dec	edx
	jnz	NEAR $L$oop_grande_shaext

$L$done_shaext:

	movaps	xmm6,XMMWORD[((-184))+rax]
	movaps	xmm7,XMMWORD[((-168))+rax]
	movaps	xmm8,XMMWORD[((-152))+rax]
	movaps	xmm9,XMMWORD[((-136))+rax]
	movaps	xmm10,XMMWORD[((-120))+rax]
	movaps	xmm11,XMMWORD[((-104))+rax]
	movaps	xmm12,XMMWORD[((-88))+rax]
	movaps	xmm13,XMMWORD[((-72))+rax]
	movaps	xmm14,XMMWORD[((-56))+rax]
	movaps	xmm15,XMMWORD[((-40))+rax]
	mov	rbp,QWORD[((-16))+rax]

	mov	rbx,QWORD[((-8))+rax]

	lea	rsp,[rax]

$L$epilogue_shaext:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha256_multi_block_shaext:

ALIGN	32
sha256_multi_block_avx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block_avx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_avx_shortcut:
	shr	rcx,32
	cmp	edx,2
	jb	NEAR $L$avx
	test	ecx,32
	jnz	NEAR _avx2_shortcut
	jmp	NEAR $L$avx
ALIGN	32
$L$avx:
	mov	rax,rsp

	push	rbx

	push	rbp

	lea	rsp,[((-168))+rsp]
	movaps	XMMWORD[rsp],xmm6
	movaps	XMMWORD[16+rsp],xmm7
	movaps	XMMWORD[32+rsp],xmm8
	movaps	XMMWORD[48+rsp],xmm9
	movaps	XMMWORD[(-120)+rax],xmm10
	movaps	XMMWORD[(-104)+rax],xmm11
	movaps	XMMWORD[(-88)+rax],xmm12
	movaps	XMMWORD[(-72)+rax],xmm13
	movaps	XMMWORD[(-56)+rax],xmm14
	movaps	XMMWORD[(-40)+rax],xmm15
	sub	rsp,288
	and	rsp,-256
	mov	QWORD[272+rsp],rax

$L$body_avx:
	lea	rbp,[((K256+128))]
	lea	rbx,[256+rsp]
	lea	rdi,[128+rdi]

$L$oop_grande_avx:
	mov	DWORD[280+rsp],edx
	xor	edx,edx

	mov	r8,QWORD[rsi]

	mov	ecx,DWORD[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[rbx],ecx
	cmovle	r8,rbp

	mov	r9,QWORD[16+rsi]

	mov	ecx,DWORD[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[4+rbx],ecx
	cmovle	r9,rbp

	mov	r10,QWORD[32+rsi]

	mov	ecx,DWORD[40+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[8+rbx],ecx
	cmovle	r10,rbp

	mov	r11,QWORD[48+rsi]

	mov	ecx,DWORD[56+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[12+rbx],ecx
	cmovle	r11,rbp
	test	edx,edx
	jz	NEAR $L$done_avx

	vmovdqu	xmm8,XMMWORD[((0-128))+rdi]
	lea	rax,[128+rsp]
	vmovdqu	xmm9,XMMWORD[((32-128))+rdi]
	vmovdqu	xmm10,XMMWORD[((64-128))+rdi]
	vmovdqu	xmm11,XMMWORD[((96-128))+rdi]
	vmovdqu	xmm12,XMMWORD[((128-128))+rdi]
	vmovdqu	xmm13,XMMWORD[((160-128))+rdi]
	vmovdqu	xmm14,XMMWORD[((192-128))+rdi]
	vmovdqu	xmm15,XMMWORD[((224-128))+rdi]
	vmovdqu	xmm6,XMMWORD[$L$pbswap]
	jmp	NEAR $L$oop_avx

ALIGN	32
$L$oop_avx:
	vpxor	xmm4,xmm10,xmm9
	vmovd	xmm5,DWORD[r8]
	vmovd	xmm0,DWORD[r9]
	vpinsrd	xmm5,xmm5,DWORD[r10],1
	vpinsrd	xmm0,xmm0,DWORD[r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm12,6
	vpslld	xmm2,xmm12,26
	vmovdqu	XMMWORD[(0-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm15

	vpsrld	xmm1,xmm12,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm12,21
	vpaddd	xmm5,xmm5,XMMWORD[((-128))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm12,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,7
	vpandn	xmm0,xmm12,xmm14
	vpand	xmm3,xmm12,xmm13

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm15,xmm8,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm8,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm9,xmm8

	vpxor	xmm15,xmm15,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm8,13

	vpslld	xmm2,xmm8,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm15,xmm1

	vpsrld	xmm1,xmm8,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,10
	vpxor	xmm15,xmm9,xmm4
	vpaddd	xmm11,xmm11,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm15,xmm15,xmm5
	vpaddd	xmm15,xmm15,xmm7
	vmovd	xmm5,DWORD[4+r8]
	vmovd	xmm0,DWORD[4+r9]
	vpinsrd	xmm5,xmm5,DWORD[4+r10],1
	vpinsrd	xmm0,xmm0,DWORD[4+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm11,6
	vpslld	xmm2,xmm11,26
	vmovdqu	XMMWORD[(16-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm14

	vpsrld	xmm1,xmm11,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm11,21
	vpaddd	xmm5,xmm5,XMMWORD[((-96))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm11,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,7
	vpandn	xmm0,xmm11,xmm13
	vpand	xmm4,xmm11,xmm12

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm14,xmm15,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm15,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm8,xmm15

	vpxor	xmm14,xmm14,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm15,13

	vpslld	xmm2,xmm15,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm14,xmm1

	vpsrld	xmm1,xmm15,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,10
	vpxor	xmm14,xmm8,xmm3
	vpaddd	xmm10,xmm10,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm14,xmm14,xmm5
	vpaddd	xmm14,xmm14,xmm7
	vmovd	xmm5,DWORD[8+r8]
	vmovd	xmm0,DWORD[8+r9]
	vpinsrd	xmm5,xmm5,DWORD[8+r10],1
	vpinsrd	xmm0,xmm0,DWORD[8+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm10,6
	vpslld	xmm2,xmm10,26
	vmovdqu	XMMWORD[(32-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm13

	vpsrld	xmm1,xmm10,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm10,21
	vpaddd	xmm5,xmm5,XMMWORD[((-64))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm10,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,7
	vpandn	xmm0,xmm10,xmm12
	vpand	xmm3,xmm10,xmm11

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm13,xmm14,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm14,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm15,xmm14

	vpxor	xmm13,xmm13,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm14,13

	vpslld	xmm2,xmm14,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm13,xmm1

	vpsrld	xmm1,xmm14,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,10
	vpxor	xmm13,xmm15,xmm4
	vpaddd	xmm9,xmm9,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm13,xmm13,xmm7
	vmovd	xmm5,DWORD[12+r8]
	vmovd	xmm0,DWORD[12+r9]
	vpinsrd	xmm5,xmm5,DWORD[12+r10],1
	vpinsrd	xmm0,xmm0,DWORD[12+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm9,6
	vpslld	xmm2,xmm9,26
	vmovdqu	XMMWORD[(48-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm12

	vpsrld	xmm1,xmm9,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm9,21
	vpaddd	xmm5,xmm5,XMMWORD[((-32))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm9,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,7
	vpandn	xmm0,xmm9,xmm11
	vpand	xmm4,xmm9,xmm10

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm12,xmm13,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm13,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm14,xmm13

	vpxor	xmm12,xmm12,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm13,13

	vpslld	xmm2,xmm13,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm12,xmm1

	vpsrld	xmm1,xmm13,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,10
	vpxor	xmm12,xmm14,xmm3
	vpaddd	xmm8,xmm8,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm12,xmm12,xmm5
	vpaddd	xmm12,xmm12,xmm7
	vmovd	xmm5,DWORD[16+r8]
	vmovd	xmm0,DWORD[16+r9]
	vpinsrd	xmm5,xmm5,DWORD[16+r10],1
	vpinsrd	xmm0,xmm0,DWORD[16+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm8,6
	vpslld	xmm2,xmm8,26
	vmovdqu	XMMWORD[(64-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm11

	vpsrld	xmm1,xmm8,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm8,21
	vpaddd	xmm5,xmm5,XMMWORD[rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm8,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,7
	vpandn	xmm0,xmm8,xmm10
	vpand	xmm3,xmm8,xmm9

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm11,xmm12,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm12,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm13,xmm12

	vpxor	xmm11,xmm11,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm12,13

	vpslld	xmm2,xmm12,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm11,xmm1

	vpsrld	xmm1,xmm12,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,10
	vpxor	xmm11,xmm13,xmm4
	vpaddd	xmm15,xmm15,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm11,xmm11,xmm5
	vpaddd	xmm11,xmm11,xmm7
	vmovd	xmm5,DWORD[20+r8]
	vmovd	xmm0,DWORD[20+r9]
	vpinsrd	xmm5,xmm5,DWORD[20+r10],1
	vpinsrd	xmm0,xmm0,DWORD[20+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm15,6
	vpslld	xmm2,xmm15,26
	vmovdqu	XMMWORD[(80-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm10

	vpsrld	xmm1,xmm15,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm15,21
	vpaddd	xmm5,xmm5,XMMWORD[32+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm15,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,7
	vpandn	xmm0,xmm15,xmm9
	vpand	xmm4,xmm15,xmm8

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm10,xmm11,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm11,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm12,xmm11

	vpxor	xmm10,xmm10,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm11,13

	vpslld	xmm2,xmm11,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm10,xmm1

	vpsrld	xmm1,xmm11,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,10
	vpxor	xmm10,xmm12,xmm3
	vpaddd	xmm14,xmm14,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm10,xmm10,xmm5
	vpaddd	xmm10,xmm10,xmm7
	vmovd	xmm5,DWORD[24+r8]
	vmovd	xmm0,DWORD[24+r9]
	vpinsrd	xmm5,xmm5,DWORD[24+r10],1
	vpinsrd	xmm0,xmm0,DWORD[24+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm14,6
	vpslld	xmm2,xmm14,26
	vmovdqu	XMMWORD[(96-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm9

	vpsrld	xmm1,xmm14,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm14,21
	vpaddd	xmm5,xmm5,XMMWORD[64+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm14,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,7
	vpandn	xmm0,xmm14,xmm8
	vpand	xmm3,xmm14,xmm15

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm9,xmm10,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm10,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm11,xmm10

	vpxor	xmm9,xmm9,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm10,13

	vpslld	xmm2,xmm10,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm9,xmm1

	vpsrld	xmm1,xmm10,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,10
	vpxor	xmm9,xmm11,xmm4
	vpaddd	xmm13,xmm13,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm9,xmm9,xmm5
	vpaddd	xmm9,xmm9,xmm7
	vmovd	xmm5,DWORD[28+r8]
	vmovd	xmm0,DWORD[28+r9]
	vpinsrd	xmm5,xmm5,DWORD[28+r10],1
	vpinsrd	xmm0,xmm0,DWORD[28+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm13,6
	vpslld	xmm2,xmm13,26
	vmovdqu	XMMWORD[(112-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm8

	vpsrld	xmm1,xmm13,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm13,21
	vpaddd	xmm5,xmm5,XMMWORD[96+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm13,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,7
	vpandn	xmm0,xmm13,xmm15
	vpand	xmm4,xmm13,xmm14

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm8,xmm9,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm9,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm10,xmm9

	vpxor	xmm8,xmm8,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm9,13

	vpslld	xmm2,xmm9,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm8,xmm1

	vpsrld	xmm1,xmm9,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,10
	vpxor	xmm8,xmm10,xmm3
	vpaddd	xmm12,xmm12,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm8,xmm8,xmm5
	vpaddd	xmm8,xmm8,xmm7
	add	rbp,256
	vmovd	xmm5,DWORD[32+r8]
	vmovd	xmm0,DWORD[32+r9]
	vpinsrd	xmm5,xmm5,DWORD[32+r10],1
	vpinsrd	xmm0,xmm0,DWORD[32+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm12,6
	vpslld	xmm2,xmm12,26
	vmovdqu	XMMWORD[(128-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm15

	vpsrld	xmm1,xmm12,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm12,21
	vpaddd	xmm5,xmm5,XMMWORD[((-128))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm12,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,7
	vpandn	xmm0,xmm12,xmm14
	vpand	xmm3,xmm12,xmm13

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm15,xmm8,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm8,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm9,xmm8

	vpxor	xmm15,xmm15,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm8,13

	vpslld	xmm2,xmm8,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm15,xmm1

	vpsrld	xmm1,xmm8,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,10
	vpxor	xmm15,xmm9,xmm4
	vpaddd	xmm11,xmm11,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm15,xmm15,xmm5
	vpaddd	xmm15,xmm15,xmm7
	vmovd	xmm5,DWORD[36+r8]
	vmovd	xmm0,DWORD[36+r9]
	vpinsrd	xmm5,xmm5,DWORD[36+r10],1
	vpinsrd	xmm0,xmm0,DWORD[36+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm11,6
	vpslld	xmm2,xmm11,26
	vmovdqu	XMMWORD[(144-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm14

	vpsrld	xmm1,xmm11,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm11,21
	vpaddd	xmm5,xmm5,XMMWORD[((-96))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm11,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,7
	vpandn	xmm0,xmm11,xmm13
	vpand	xmm4,xmm11,xmm12

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm14,xmm15,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm15,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm8,xmm15

	vpxor	xmm14,xmm14,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm15,13

	vpslld	xmm2,xmm15,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm14,xmm1

	vpsrld	xmm1,xmm15,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,10
	vpxor	xmm14,xmm8,xmm3
	vpaddd	xmm10,xmm10,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm14,xmm14,xmm5
	vpaddd	xmm14,xmm14,xmm7
	vmovd	xmm5,DWORD[40+r8]
	vmovd	xmm0,DWORD[40+r9]
	vpinsrd	xmm5,xmm5,DWORD[40+r10],1
	vpinsrd	xmm0,xmm0,DWORD[40+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm10,6
	vpslld	xmm2,xmm10,26
	vmovdqu	XMMWORD[(160-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm13

	vpsrld	xmm1,xmm10,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm10,21
	vpaddd	xmm5,xmm5,XMMWORD[((-64))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm10,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,7
	vpandn	xmm0,xmm10,xmm12
	vpand	xmm3,xmm10,xmm11

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm13,xmm14,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm14,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm15,xmm14

	vpxor	xmm13,xmm13,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm14,13

	vpslld	xmm2,xmm14,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm13,xmm1

	vpsrld	xmm1,xmm14,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,10
	vpxor	xmm13,xmm15,xmm4
	vpaddd	xmm9,xmm9,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm13,xmm13,xmm7
	vmovd	xmm5,DWORD[44+r8]
	vmovd	xmm0,DWORD[44+r9]
	vpinsrd	xmm5,xmm5,DWORD[44+r10],1
	vpinsrd	xmm0,xmm0,DWORD[44+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm9,6
	vpslld	xmm2,xmm9,26
	vmovdqu	XMMWORD[(176-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm12

	vpsrld	xmm1,xmm9,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm9,21
	vpaddd	xmm5,xmm5,XMMWORD[((-32))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm9,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,7
	vpandn	xmm0,xmm9,xmm11
	vpand	xmm4,xmm9,xmm10

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm12,xmm13,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm13,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm14,xmm13

	vpxor	xmm12,xmm12,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm13,13

	vpslld	xmm2,xmm13,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm12,xmm1

	vpsrld	xmm1,xmm13,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,10
	vpxor	xmm12,xmm14,xmm3
	vpaddd	xmm8,xmm8,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm12,xmm12,xmm5
	vpaddd	xmm12,xmm12,xmm7
	vmovd	xmm5,DWORD[48+r8]
	vmovd	xmm0,DWORD[48+r9]
	vpinsrd	xmm5,xmm5,DWORD[48+r10],1
	vpinsrd	xmm0,xmm0,DWORD[48+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm8,6
	vpslld	xmm2,xmm8,26
	vmovdqu	XMMWORD[(192-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm11

	vpsrld	xmm1,xmm8,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm8,21
	vpaddd	xmm5,xmm5,XMMWORD[rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm8,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,7
	vpandn	xmm0,xmm8,xmm10
	vpand	xmm3,xmm8,xmm9

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm11,xmm12,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm12,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm13,xmm12

	vpxor	xmm11,xmm11,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm12,13

	vpslld	xmm2,xmm12,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm11,xmm1

	vpsrld	xmm1,xmm12,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,10
	vpxor	xmm11,xmm13,xmm4
	vpaddd	xmm15,xmm15,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm11,xmm11,xmm5
	vpaddd	xmm11,xmm11,xmm7
	vmovd	xmm5,DWORD[52+r8]
	vmovd	xmm0,DWORD[52+r9]
	vpinsrd	xmm5,xmm5,DWORD[52+r10],1
	vpinsrd	xmm0,xmm0,DWORD[52+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm15,6
	vpslld	xmm2,xmm15,26
	vmovdqu	XMMWORD[(208-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm10

	vpsrld	xmm1,xmm15,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm15,21
	vpaddd	xmm5,xmm5,XMMWORD[32+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm15,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,7
	vpandn	xmm0,xmm15,xmm9
	vpand	xmm4,xmm15,xmm8

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm10,xmm11,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm11,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm12,xmm11

	vpxor	xmm10,xmm10,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm11,13

	vpslld	xmm2,xmm11,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm10,xmm1

	vpsrld	xmm1,xmm11,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,10
	vpxor	xmm10,xmm12,xmm3
	vpaddd	xmm14,xmm14,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm10,xmm10,xmm5
	vpaddd	xmm10,xmm10,xmm7
	vmovd	xmm5,DWORD[56+r8]
	vmovd	xmm0,DWORD[56+r9]
	vpinsrd	xmm5,xmm5,DWORD[56+r10],1
	vpinsrd	xmm0,xmm0,DWORD[56+r11],1
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm14,6
	vpslld	xmm2,xmm14,26
	vmovdqu	XMMWORD[(224-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm9

	vpsrld	xmm1,xmm14,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm14,21
	vpaddd	xmm5,xmm5,XMMWORD[64+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm14,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,7
	vpandn	xmm0,xmm14,xmm8
	vpand	xmm3,xmm14,xmm15

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm9,xmm10,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm10,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm11,xmm10

	vpxor	xmm9,xmm9,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm10,13

	vpslld	xmm2,xmm10,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm9,xmm1

	vpsrld	xmm1,xmm10,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,10
	vpxor	xmm9,xmm11,xmm4
	vpaddd	xmm13,xmm13,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm9,xmm9,xmm5
	vpaddd	xmm9,xmm9,xmm7
	vmovd	xmm5,DWORD[60+r8]
	lea	r8,[64+r8]
	vmovd	xmm0,DWORD[60+r9]
	lea	r9,[64+r9]
	vpinsrd	xmm5,xmm5,DWORD[60+r10],1
	lea	r10,[64+r10]
	vpinsrd	xmm0,xmm0,DWORD[60+r11],1
	lea	r11,[64+r11]
	vpunpckldq	xmm5,xmm5,xmm0
	vpshufb	xmm5,xmm5,xmm6
	vpsrld	xmm7,xmm13,6
	vpslld	xmm2,xmm13,26
	vmovdqu	XMMWORD[(240-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm8

	vpsrld	xmm1,xmm13,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm13,21
	vpaddd	xmm5,xmm5,XMMWORD[96+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm13,25
	vpxor	xmm7,xmm7,xmm2
	prefetcht0	[63+r8]
	vpslld	xmm2,xmm13,7
	vpandn	xmm0,xmm13,xmm15
	vpand	xmm4,xmm13,xmm14
	prefetcht0	[63+r9]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm8,xmm9,2
	vpxor	xmm7,xmm7,xmm2
	prefetcht0	[63+r10]
	vpslld	xmm1,xmm9,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm10,xmm9
	prefetcht0	[63+r11]
	vpxor	xmm8,xmm8,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm9,13

	vpslld	xmm2,xmm9,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm8,xmm1

	vpsrld	xmm1,xmm9,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,10
	vpxor	xmm8,xmm10,xmm3
	vpaddd	xmm12,xmm12,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm8,xmm8,xmm5
	vpaddd	xmm8,xmm8,xmm7
	add	rbp,256
	vmovdqu	xmm5,XMMWORD[((0-128))+rax]
	mov	ecx,3
	jmp	NEAR $L$oop_16_xx_avx
ALIGN	32
$L$oop_16_xx_avx:
	vmovdqu	xmm6,XMMWORD[((16-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((144-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((224-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm12,6
	vpslld	xmm2,xmm12,26
	vmovdqu	XMMWORD[(0-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm15

	vpsrld	xmm1,xmm12,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm12,21
	vpaddd	xmm5,xmm5,XMMWORD[((-128))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm12,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,7
	vpandn	xmm0,xmm12,xmm14
	vpand	xmm3,xmm12,xmm13

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm15,xmm8,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm8,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm9,xmm8

	vpxor	xmm15,xmm15,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm8,13

	vpslld	xmm2,xmm8,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm15,xmm1

	vpsrld	xmm1,xmm8,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,10
	vpxor	xmm15,xmm9,xmm4
	vpaddd	xmm11,xmm11,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm15,xmm15,xmm5
	vpaddd	xmm15,xmm15,xmm7
	vmovdqu	xmm5,XMMWORD[((32-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((160-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((240-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm11,6
	vpslld	xmm2,xmm11,26
	vmovdqu	XMMWORD[(16-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm14

	vpsrld	xmm1,xmm11,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm11,21
	vpaddd	xmm6,xmm6,XMMWORD[((-96))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm11,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,7
	vpandn	xmm0,xmm11,xmm13
	vpand	xmm4,xmm11,xmm12

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm14,xmm15,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm15,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm8,xmm15

	vpxor	xmm14,xmm14,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm15,13

	vpslld	xmm2,xmm15,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm14,xmm1

	vpsrld	xmm1,xmm15,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,10
	vpxor	xmm14,xmm8,xmm3
	vpaddd	xmm10,xmm10,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm14,xmm14,xmm6
	vpaddd	xmm14,xmm14,xmm7
	vmovdqu	xmm6,XMMWORD[((48-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((176-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((0-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm10,6
	vpslld	xmm2,xmm10,26
	vmovdqu	XMMWORD[(32-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm13

	vpsrld	xmm1,xmm10,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm10,21
	vpaddd	xmm5,xmm5,XMMWORD[((-64))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm10,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,7
	vpandn	xmm0,xmm10,xmm12
	vpand	xmm3,xmm10,xmm11

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm13,xmm14,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm14,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm15,xmm14

	vpxor	xmm13,xmm13,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm14,13

	vpslld	xmm2,xmm14,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm13,xmm1

	vpsrld	xmm1,xmm14,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,10
	vpxor	xmm13,xmm15,xmm4
	vpaddd	xmm9,xmm9,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm13,xmm13,xmm7
	vmovdqu	xmm5,XMMWORD[((64-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((192-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((16-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm9,6
	vpslld	xmm2,xmm9,26
	vmovdqu	XMMWORD[(48-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm12

	vpsrld	xmm1,xmm9,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm9,21
	vpaddd	xmm6,xmm6,XMMWORD[((-32))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm9,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,7
	vpandn	xmm0,xmm9,xmm11
	vpand	xmm4,xmm9,xmm10

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm12,xmm13,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm13,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm14,xmm13

	vpxor	xmm12,xmm12,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm13,13

	vpslld	xmm2,xmm13,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm12,xmm1

	vpsrld	xmm1,xmm13,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,10
	vpxor	xmm12,xmm14,xmm3
	vpaddd	xmm8,xmm8,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm12,xmm12,xmm6
	vpaddd	xmm12,xmm12,xmm7
	vmovdqu	xmm6,XMMWORD[((80-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((208-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((32-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm8,6
	vpslld	xmm2,xmm8,26
	vmovdqu	XMMWORD[(64-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm11

	vpsrld	xmm1,xmm8,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm8,21
	vpaddd	xmm5,xmm5,XMMWORD[rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm8,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,7
	vpandn	xmm0,xmm8,xmm10
	vpand	xmm3,xmm8,xmm9

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm11,xmm12,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm12,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm13,xmm12

	vpxor	xmm11,xmm11,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm12,13

	vpslld	xmm2,xmm12,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm11,xmm1

	vpsrld	xmm1,xmm12,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,10
	vpxor	xmm11,xmm13,xmm4
	vpaddd	xmm15,xmm15,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm11,xmm11,xmm5
	vpaddd	xmm11,xmm11,xmm7
	vmovdqu	xmm5,XMMWORD[((96-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((224-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((48-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm15,6
	vpslld	xmm2,xmm15,26
	vmovdqu	XMMWORD[(80-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm10

	vpsrld	xmm1,xmm15,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm15,21
	vpaddd	xmm6,xmm6,XMMWORD[32+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm15,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,7
	vpandn	xmm0,xmm15,xmm9
	vpand	xmm4,xmm15,xmm8

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm10,xmm11,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm11,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm12,xmm11

	vpxor	xmm10,xmm10,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm11,13

	vpslld	xmm2,xmm11,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm10,xmm1

	vpsrld	xmm1,xmm11,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,10
	vpxor	xmm10,xmm12,xmm3
	vpaddd	xmm14,xmm14,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm10,xmm10,xmm6
	vpaddd	xmm10,xmm10,xmm7
	vmovdqu	xmm6,XMMWORD[((112-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((240-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((64-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm14,6
	vpslld	xmm2,xmm14,26
	vmovdqu	XMMWORD[(96-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm9

	vpsrld	xmm1,xmm14,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm14,21
	vpaddd	xmm5,xmm5,XMMWORD[64+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm14,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,7
	vpandn	xmm0,xmm14,xmm8
	vpand	xmm3,xmm14,xmm15

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm9,xmm10,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm10,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm11,xmm10

	vpxor	xmm9,xmm9,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm10,13

	vpslld	xmm2,xmm10,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm9,xmm1

	vpsrld	xmm1,xmm10,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,10
	vpxor	xmm9,xmm11,xmm4
	vpaddd	xmm13,xmm13,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm9,xmm9,xmm5
	vpaddd	xmm9,xmm9,xmm7
	vmovdqu	xmm5,XMMWORD[((128-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((0-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((80-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm13,6
	vpslld	xmm2,xmm13,26
	vmovdqu	XMMWORD[(112-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm8

	vpsrld	xmm1,xmm13,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm13,21
	vpaddd	xmm6,xmm6,XMMWORD[96+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm13,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,7
	vpandn	xmm0,xmm13,xmm15
	vpand	xmm4,xmm13,xmm14

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm8,xmm9,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm9,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm10,xmm9

	vpxor	xmm8,xmm8,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm9,13

	vpslld	xmm2,xmm9,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm8,xmm1

	vpsrld	xmm1,xmm9,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,10
	vpxor	xmm8,xmm10,xmm3
	vpaddd	xmm12,xmm12,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm8,xmm8,xmm6
	vpaddd	xmm8,xmm8,xmm7
	add	rbp,256
	vmovdqu	xmm6,XMMWORD[((144-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((16-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((96-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm12,6
	vpslld	xmm2,xmm12,26
	vmovdqu	XMMWORD[(128-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm15

	vpsrld	xmm1,xmm12,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm12,21
	vpaddd	xmm5,xmm5,XMMWORD[((-128))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm12,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,7
	vpandn	xmm0,xmm12,xmm14
	vpand	xmm3,xmm12,xmm13

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm15,xmm8,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm8,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm9,xmm8

	vpxor	xmm15,xmm15,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm8,13

	vpslld	xmm2,xmm8,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm15,xmm1

	vpsrld	xmm1,xmm8,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,10
	vpxor	xmm15,xmm9,xmm4
	vpaddd	xmm11,xmm11,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm15,xmm15,xmm5
	vpaddd	xmm15,xmm15,xmm7
	vmovdqu	xmm5,XMMWORD[((160-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((32-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((112-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm11,6
	vpslld	xmm2,xmm11,26
	vmovdqu	XMMWORD[(144-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm14

	vpsrld	xmm1,xmm11,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm11,21
	vpaddd	xmm6,xmm6,XMMWORD[((-96))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm11,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,7
	vpandn	xmm0,xmm11,xmm13
	vpand	xmm4,xmm11,xmm12

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm14,xmm15,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm15,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm8,xmm15

	vpxor	xmm14,xmm14,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm15,13

	vpslld	xmm2,xmm15,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm14,xmm1

	vpsrld	xmm1,xmm15,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,10
	vpxor	xmm14,xmm8,xmm3
	vpaddd	xmm10,xmm10,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm14,xmm14,xmm6
	vpaddd	xmm14,xmm14,xmm7
	vmovdqu	xmm6,XMMWORD[((176-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((48-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((128-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm10,6
	vpslld	xmm2,xmm10,26
	vmovdqu	XMMWORD[(160-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm13

	vpsrld	xmm1,xmm10,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm10,21
	vpaddd	xmm5,xmm5,XMMWORD[((-64))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm10,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,7
	vpandn	xmm0,xmm10,xmm12
	vpand	xmm3,xmm10,xmm11

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm13,xmm14,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm14,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm15,xmm14

	vpxor	xmm13,xmm13,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm14,13

	vpslld	xmm2,xmm14,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm13,xmm1

	vpsrld	xmm1,xmm14,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,10
	vpxor	xmm13,xmm15,xmm4
	vpaddd	xmm9,xmm9,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm13,xmm13,xmm7
	vmovdqu	xmm5,XMMWORD[((192-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((64-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((144-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm9,6
	vpslld	xmm2,xmm9,26
	vmovdqu	XMMWORD[(176-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm12

	vpsrld	xmm1,xmm9,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm9,21
	vpaddd	xmm6,xmm6,XMMWORD[((-32))+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm9,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,7
	vpandn	xmm0,xmm9,xmm11
	vpand	xmm4,xmm9,xmm10

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm12,xmm13,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm13,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm14,xmm13

	vpxor	xmm12,xmm12,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm13,13

	vpslld	xmm2,xmm13,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm12,xmm1

	vpsrld	xmm1,xmm13,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,10
	vpxor	xmm12,xmm14,xmm3
	vpaddd	xmm8,xmm8,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm12,xmm12,xmm6
	vpaddd	xmm12,xmm12,xmm7
	vmovdqu	xmm6,XMMWORD[((208-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((80-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((160-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm8,6
	vpslld	xmm2,xmm8,26
	vmovdqu	XMMWORD[(192-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm11

	vpsrld	xmm1,xmm8,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm8,21
	vpaddd	xmm5,xmm5,XMMWORD[rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm8,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm8,7
	vpandn	xmm0,xmm8,xmm10
	vpand	xmm3,xmm8,xmm9

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm11,xmm12,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm12,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm13,xmm12

	vpxor	xmm11,xmm11,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm12,13

	vpslld	xmm2,xmm12,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm11,xmm1

	vpsrld	xmm1,xmm12,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm12,10
	vpxor	xmm11,xmm13,xmm4
	vpaddd	xmm15,xmm15,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm11,xmm11,xmm5
	vpaddd	xmm11,xmm11,xmm7
	vmovdqu	xmm5,XMMWORD[((224-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((96-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((176-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm15,6
	vpslld	xmm2,xmm15,26
	vmovdqu	XMMWORD[(208-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm10

	vpsrld	xmm1,xmm15,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm15,21
	vpaddd	xmm6,xmm6,XMMWORD[32+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm15,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm15,7
	vpandn	xmm0,xmm15,xmm9
	vpand	xmm4,xmm15,xmm8

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm10,xmm11,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm11,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm12,xmm11

	vpxor	xmm10,xmm10,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm11,13

	vpslld	xmm2,xmm11,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm10,xmm1

	vpsrld	xmm1,xmm11,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm11,10
	vpxor	xmm10,xmm12,xmm3
	vpaddd	xmm14,xmm14,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm10,xmm10,xmm6
	vpaddd	xmm10,xmm10,xmm7
	vmovdqu	xmm6,XMMWORD[((240-128))+rax]
	vpaddd	xmm5,xmm5,XMMWORD[((112-128))+rax]

	vpsrld	xmm7,xmm6,3
	vpsrld	xmm1,xmm6,7
	vpslld	xmm2,xmm6,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm6,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm6,14
	vmovdqu	xmm0,XMMWORD[((192-128))+rax]
	vpsrld	xmm3,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm5,xmm5,xmm7
	vpxor	xmm7,xmm3,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm5,xmm5,xmm7
	vpsrld	xmm7,xmm14,6
	vpslld	xmm2,xmm14,26
	vmovdqu	XMMWORD[(224-128)+rax],xmm5
	vpaddd	xmm5,xmm5,xmm9

	vpsrld	xmm1,xmm14,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm14,21
	vpaddd	xmm5,xmm5,XMMWORD[64+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm14,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm14,7
	vpandn	xmm0,xmm14,xmm8
	vpand	xmm3,xmm14,xmm15

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm9,xmm10,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm10,30
	vpxor	xmm0,xmm0,xmm3
	vpxor	xmm3,xmm11,xmm10

	vpxor	xmm9,xmm9,xmm1
	vpaddd	xmm5,xmm5,xmm7

	vpsrld	xmm1,xmm10,13

	vpslld	xmm2,xmm10,19
	vpaddd	xmm5,xmm5,xmm0
	vpand	xmm4,xmm4,xmm3

	vpxor	xmm7,xmm9,xmm1

	vpsrld	xmm1,xmm10,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm10,10
	vpxor	xmm9,xmm11,xmm4
	vpaddd	xmm13,xmm13,xmm5

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm9,xmm9,xmm5
	vpaddd	xmm9,xmm9,xmm7
	vmovdqu	xmm5,XMMWORD[((0-128))+rax]
	vpaddd	xmm6,xmm6,XMMWORD[((128-128))+rax]

	vpsrld	xmm7,xmm5,3
	vpsrld	xmm1,xmm5,7
	vpslld	xmm2,xmm5,25
	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm5,18
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm5,14
	vmovdqu	xmm0,XMMWORD[((208-128))+rax]
	vpsrld	xmm4,xmm0,10

	vpxor	xmm7,xmm7,xmm1
	vpsrld	xmm1,xmm0,17
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,15
	vpaddd	xmm6,xmm6,xmm7
	vpxor	xmm7,xmm4,xmm1
	vpsrld	xmm1,xmm0,19
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm0,13
	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2
	vpaddd	xmm6,xmm6,xmm7
	vpsrld	xmm7,xmm13,6
	vpslld	xmm2,xmm13,26
	vmovdqu	XMMWORD[(240-128)+rax],xmm6
	vpaddd	xmm6,xmm6,xmm8

	vpsrld	xmm1,xmm13,11
	vpxor	xmm7,xmm7,xmm2
	vpslld	xmm2,xmm13,21
	vpaddd	xmm6,xmm6,XMMWORD[96+rbp]
	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm1,xmm13,25
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm13,7
	vpandn	xmm0,xmm13,xmm15
	vpand	xmm4,xmm13,xmm14

	vpxor	xmm7,xmm7,xmm1

	vpsrld	xmm8,xmm9,2
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm1,xmm9,30
	vpxor	xmm0,xmm0,xmm4
	vpxor	xmm4,xmm10,xmm9

	vpxor	xmm8,xmm8,xmm1
	vpaddd	xmm6,xmm6,xmm7

	vpsrld	xmm1,xmm9,13

	vpslld	xmm2,xmm9,19
	vpaddd	xmm6,xmm6,xmm0
	vpand	xmm3,xmm3,xmm4

	vpxor	xmm7,xmm8,xmm1

	vpsrld	xmm1,xmm9,22
	vpxor	xmm7,xmm7,xmm2

	vpslld	xmm2,xmm9,10
	vpxor	xmm8,xmm10,xmm3
	vpaddd	xmm12,xmm12,xmm6

	vpxor	xmm7,xmm7,xmm1
	vpxor	xmm7,xmm7,xmm2

	vpaddd	xmm8,xmm8,xmm6
	vpaddd	xmm8,xmm8,xmm7
	add	rbp,256
	dec	ecx
	jnz	NEAR $L$oop_16_xx_avx

	mov	ecx,1
	lea	rbp,[((K256+128))]
	cmp	ecx,DWORD[rbx]
	cmovge	r8,rbp
	cmp	ecx,DWORD[4+rbx]
	cmovge	r9,rbp
	cmp	ecx,DWORD[8+rbx]
	cmovge	r10,rbp
	cmp	ecx,DWORD[12+rbx]
	cmovge	r11,rbp
	vmovdqa	xmm7,XMMWORD[rbx]
	vpxor	xmm0,xmm0,xmm0
	vmovdqa	xmm6,xmm7
	vpcmpgtd	xmm6,xmm6,xmm0
	vpaddd	xmm7,xmm7,xmm6

	vmovdqu	xmm0,XMMWORD[((0-128))+rdi]
	vpand	xmm8,xmm8,xmm6
	vmovdqu	xmm1,XMMWORD[((32-128))+rdi]
	vpand	xmm9,xmm9,xmm6
	vmovdqu	xmm2,XMMWORD[((64-128))+rdi]
	vpand	xmm10,xmm10,xmm6
	vmovdqu	xmm5,XMMWORD[((96-128))+rdi]
	vpand	xmm11,xmm11,xmm6
	vpaddd	xmm8,xmm8,xmm0
	vmovdqu	xmm0,XMMWORD[((128-128))+rdi]
	vpand	xmm12,xmm12,xmm6
	vpaddd	xmm9,xmm9,xmm1
	vmovdqu	xmm1,XMMWORD[((160-128))+rdi]
	vpand	xmm13,xmm13,xmm6
	vpaddd	xmm10,xmm10,xmm2
	vmovdqu	xmm2,XMMWORD[((192-128))+rdi]
	vpand	xmm14,xmm14,xmm6
	vpaddd	xmm11,xmm11,xmm5
	vmovdqu	xmm5,XMMWORD[((224-128))+rdi]
	vpand	xmm15,xmm15,xmm6
	vpaddd	xmm12,xmm12,xmm0
	vpaddd	xmm13,xmm13,xmm1
	vmovdqu	XMMWORD[(0-128)+rdi],xmm8
	vpaddd	xmm14,xmm14,xmm2
	vmovdqu	XMMWORD[(32-128)+rdi],xmm9
	vpaddd	xmm15,xmm15,xmm5
	vmovdqu	XMMWORD[(64-128)+rdi],xmm10
	vmovdqu	XMMWORD[(96-128)+rdi],xmm11
	vmovdqu	XMMWORD[(128-128)+rdi],xmm12
	vmovdqu	XMMWORD[(160-128)+rdi],xmm13
	vmovdqu	XMMWORD[(192-128)+rdi],xmm14
	vmovdqu	XMMWORD[(224-128)+rdi],xmm15

	vmovdqu	XMMWORD[rbx],xmm7
	vmovdqu	xmm6,XMMWORD[$L$pbswap]
	dec	edx
	jnz	NEAR $L$oop_avx

	mov	edx,DWORD[280+rsp]
	lea	rdi,[16+rdi]
	lea	rsi,[64+rsi]
	dec	edx
	jnz	NEAR $L$oop_grande_avx

$L$done_avx:
	mov	rax,QWORD[272+rsp]

	vzeroupper
	movaps	xmm6,XMMWORD[((-184))+rax]
	movaps	xmm7,XMMWORD[((-168))+rax]
	movaps	xmm8,XMMWORD[((-152))+rax]
	movaps	xmm9,XMMWORD[((-136))+rax]
	movaps	xmm10,XMMWORD[((-120))+rax]
	movaps	xmm11,XMMWORD[((-104))+rax]
	movaps	xmm12,XMMWORD[((-88))+rax]
	movaps	xmm13,XMMWORD[((-72))+rax]
	movaps	xmm14,XMMWORD[((-56))+rax]
	movaps	xmm15,XMMWORD[((-40))+rax]
	mov	rbp,QWORD[((-16))+rax]

	mov	rbx,QWORD[((-8))+rax]

	lea	rsp,[rax]

$L$epilogue_avx:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha256_multi_block_avx:

ALIGN	32
sha256_multi_block_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha256_multi_block_avx2:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_avx2_shortcut:
	mov	rax,rsp

	push	rbx

	push	rbp

	push	r12

	push	r13

	push	r14

	push	r15

	lea	rsp,[((-168))+rsp]
	movaps	XMMWORD[rsp],xmm6
	movaps	XMMWORD[16+rsp],xmm7
	movaps	XMMWORD[32+rsp],xmm8
	movaps	XMMWORD[48+rsp],xmm9
	movaps	XMMWORD[64+rsp],xmm10
	movaps	XMMWORD[80+rsp],xmm11
	movaps	XMMWORD[(-120)+rax],xmm12
	movaps	XMMWORD[(-104)+rax],xmm13
	movaps	XMMWORD[(-88)+rax],xmm14
	movaps	XMMWORD[(-72)+rax],xmm15
	sub	rsp,576
	and	rsp,-256
	mov	QWORD[544+rsp],rax

$L$body_avx2:
	lea	rbp,[((K256+128))]
	lea	rdi,[128+rdi]

$L$oop_grande_avx2:
	mov	DWORD[552+rsp],edx
	xor	edx,edx
	lea	rbx,[512+rsp]

	mov	r12,QWORD[rsi]

	mov	ecx,DWORD[8+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[rbx],ecx
	cmovle	r12,rbp

	mov	r13,QWORD[16+rsi]

	mov	ecx,DWORD[24+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[4+rbx],ecx
	cmovle	r13,rbp

	mov	r14,QWORD[32+rsi]

	mov	ecx,DWORD[40+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[8+rbx],ecx
	cmovle	r14,rbp

	mov	r15,QWORD[48+rsi]

	mov	ecx,DWORD[56+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[12+rbx],ecx
	cmovle	r15,rbp

	mov	r8,QWORD[64+rsi]

	mov	ecx,DWORD[72+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[16+rbx],ecx
	cmovle	r8,rbp

	mov	r9,QWORD[80+rsi]

	mov	ecx,DWORD[88+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[20+rbx],ecx
	cmovle	r9,rbp

	mov	r10,QWORD[96+rsi]

	mov	ecx,DWORD[104+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[24+rbx],ecx
	cmovle	r10,rbp

	mov	r11,QWORD[112+rsi]

	mov	ecx,DWORD[120+rsi]
	cmp	ecx,edx
	cmovg	edx,ecx
	test	ecx,ecx
	mov	DWORD[28+rbx],ecx
	cmovle	r11,rbp
	vmovdqu	ymm8,YMMWORD[((0-128))+rdi]
	lea	rax,[128+rsp]
	vmovdqu	ymm9,YMMWORD[((32-128))+rdi]
	lea	rbx,[((256+128))+rsp]
	vmovdqu	ymm10,YMMWORD[((64-128))+rdi]
	vmovdqu	ymm11,YMMWORD[((96-128))+rdi]
	vmovdqu	ymm12,YMMWORD[((128-128))+rdi]
	vmovdqu	ymm13,YMMWORD[((160-128))+rdi]
	vmovdqu	ymm14,YMMWORD[((192-128))+rdi]
	vmovdqu	ymm15,YMMWORD[((224-128))+rdi]
	vmovdqu	ymm6,YMMWORD[$L$pbswap]
	jmp	NEAR $L$oop_avx2

ALIGN	32
$L$oop_avx2:
	vpxor	ymm4,ymm10,ymm9
	vmovd	xmm5,DWORD[r12]
	vmovd	xmm0,DWORD[r8]
	vmovd	xmm1,DWORD[r13]
	vmovd	xmm2,DWORD[r9]
	vpinsrd	xmm5,xmm5,DWORD[r14],1
	vpinsrd	xmm0,xmm0,DWORD[r10],1
	vpinsrd	xmm1,xmm1,DWORD[r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm12,6
	vpslld	ymm2,ymm12,26
	vmovdqu	YMMWORD[(0-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm15

	vpsrld	ymm1,ymm12,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm12,21
	vpaddd	ymm5,ymm5,YMMWORD[((-128))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm12,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,7
	vpandn	ymm0,ymm12,ymm14
	vpand	ymm3,ymm12,ymm13

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm15,ymm8,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm8,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm9,ymm8

	vpxor	ymm15,ymm15,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm8,13

	vpslld	ymm2,ymm8,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm15,ymm1

	vpsrld	ymm1,ymm8,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,10
	vpxor	ymm15,ymm9,ymm4
	vpaddd	ymm11,ymm11,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm15,ymm15,ymm5
	vpaddd	ymm15,ymm15,ymm7
	vmovd	xmm5,DWORD[4+r12]
	vmovd	xmm0,DWORD[4+r8]
	vmovd	xmm1,DWORD[4+r13]
	vmovd	xmm2,DWORD[4+r9]
	vpinsrd	xmm5,xmm5,DWORD[4+r14],1
	vpinsrd	xmm0,xmm0,DWORD[4+r10],1
	vpinsrd	xmm1,xmm1,DWORD[4+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[4+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm11,6
	vpslld	ymm2,ymm11,26
	vmovdqu	YMMWORD[(32-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm14

	vpsrld	ymm1,ymm11,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm11,21
	vpaddd	ymm5,ymm5,YMMWORD[((-96))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm11,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,7
	vpandn	ymm0,ymm11,ymm13
	vpand	ymm4,ymm11,ymm12

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm14,ymm15,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm15,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm8,ymm15

	vpxor	ymm14,ymm14,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm15,13

	vpslld	ymm2,ymm15,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm14,ymm1

	vpsrld	ymm1,ymm15,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,10
	vpxor	ymm14,ymm8,ymm3
	vpaddd	ymm10,ymm10,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm14,ymm14,ymm5
	vpaddd	ymm14,ymm14,ymm7
	vmovd	xmm5,DWORD[8+r12]
	vmovd	xmm0,DWORD[8+r8]
	vmovd	xmm1,DWORD[8+r13]
	vmovd	xmm2,DWORD[8+r9]
	vpinsrd	xmm5,xmm5,DWORD[8+r14],1
	vpinsrd	xmm0,xmm0,DWORD[8+r10],1
	vpinsrd	xmm1,xmm1,DWORD[8+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[8+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm10,6
	vpslld	ymm2,ymm10,26
	vmovdqu	YMMWORD[(64-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm13

	vpsrld	ymm1,ymm10,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm10,21
	vpaddd	ymm5,ymm5,YMMWORD[((-64))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm10,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,7
	vpandn	ymm0,ymm10,ymm12
	vpand	ymm3,ymm10,ymm11

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm13,ymm14,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm14,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm15,ymm14

	vpxor	ymm13,ymm13,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm14,13

	vpslld	ymm2,ymm14,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm13,ymm1

	vpsrld	ymm1,ymm14,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,10
	vpxor	ymm13,ymm15,ymm4
	vpaddd	ymm9,ymm9,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm13,ymm13,ymm5
	vpaddd	ymm13,ymm13,ymm7
	vmovd	xmm5,DWORD[12+r12]
	vmovd	xmm0,DWORD[12+r8]
	vmovd	xmm1,DWORD[12+r13]
	vmovd	xmm2,DWORD[12+r9]
	vpinsrd	xmm5,xmm5,DWORD[12+r14],1
	vpinsrd	xmm0,xmm0,DWORD[12+r10],1
	vpinsrd	xmm1,xmm1,DWORD[12+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[12+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm9,6
	vpslld	ymm2,ymm9,26
	vmovdqu	YMMWORD[(96-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm12

	vpsrld	ymm1,ymm9,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm9,21
	vpaddd	ymm5,ymm5,YMMWORD[((-32))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm9,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,7
	vpandn	ymm0,ymm9,ymm11
	vpand	ymm4,ymm9,ymm10

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm12,ymm13,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm13,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm14,ymm13

	vpxor	ymm12,ymm12,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm13,13

	vpslld	ymm2,ymm13,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm12,ymm1

	vpsrld	ymm1,ymm13,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,10
	vpxor	ymm12,ymm14,ymm3
	vpaddd	ymm8,ymm8,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm12,ymm12,ymm5
	vpaddd	ymm12,ymm12,ymm7
	vmovd	xmm5,DWORD[16+r12]
	vmovd	xmm0,DWORD[16+r8]
	vmovd	xmm1,DWORD[16+r13]
	vmovd	xmm2,DWORD[16+r9]
	vpinsrd	xmm5,xmm5,DWORD[16+r14],1
	vpinsrd	xmm0,xmm0,DWORD[16+r10],1
	vpinsrd	xmm1,xmm1,DWORD[16+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[16+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm8,6
	vpslld	ymm2,ymm8,26
	vmovdqu	YMMWORD[(128-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm11

	vpsrld	ymm1,ymm8,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm8,21
	vpaddd	ymm5,ymm5,YMMWORD[rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm8,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,7
	vpandn	ymm0,ymm8,ymm10
	vpand	ymm3,ymm8,ymm9

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm11,ymm12,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm12,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm13,ymm12

	vpxor	ymm11,ymm11,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm12,13

	vpslld	ymm2,ymm12,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm11,ymm1

	vpsrld	ymm1,ymm12,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,10
	vpxor	ymm11,ymm13,ymm4
	vpaddd	ymm15,ymm15,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm11,ymm11,ymm5
	vpaddd	ymm11,ymm11,ymm7
	vmovd	xmm5,DWORD[20+r12]
	vmovd	xmm0,DWORD[20+r8]
	vmovd	xmm1,DWORD[20+r13]
	vmovd	xmm2,DWORD[20+r9]
	vpinsrd	xmm5,xmm5,DWORD[20+r14],1
	vpinsrd	xmm0,xmm0,DWORD[20+r10],1
	vpinsrd	xmm1,xmm1,DWORD[20+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[20+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm15,6
	vpslld	ymm2,ymm15,26
	vmovdqu	YMMWORD[(160-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm10

	vpsrld	ymm1,ymm15,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm15,21
	vpaddd	ymm5,ymm5,YMMWORD[32+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm15,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,7
	vpandn	ymm0,ymm15,ymm9
	vpand	ymm4,ymm15,ymm8

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm10,ymm11,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm11,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm12,ymm11

	vpxor	ymm10,ymm10,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm11,13

	vpslld	ymm2,ymm11,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm10,ymm1

	vpsrld	ymm1,ymm11,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,10
	vpxor	ymm10,ymm12,ymm3
	vpaddd	ymm14,ymm14,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm10,ymm10,ymm5
	vpaddd	ymm10,ymm10,ymm7
	vmovd	xmm5,DWORD[24+r12]
	vmovd	xmm0,DWORD[24+r8]
	vmovd	xmm1,DWORD[24+r13]
	vmovd	xmm2,DWORD[24+r9]
	vpinsrd	xmm5,xmm5,DWORD[24+r14],1
	vpinsrd	xmm0,xmm0,DWORD[24+r10],1
	vpinsrd	xmm1,xmm1,DWORD[24+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[24+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm14,6
	vpslld	ymm2,ymm14,26
	vmovdqu	YMMWORD[(192-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm9

	vpsrld	ymm1,ymm14,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm14,21
	vpaddd	ymm5,ymm5,YMMWORD[64+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm14,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,7
	vpandn	ymm0,ymm14,ymm8
	vpand	ymm3,ymm14,ymm15

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm9,ymm10,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm10,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm11,ymm10

	vpxor	ymm9,ymm9,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm10,13

	vpslld	ymm2,ymm10,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm9,ymm1

	vpsrld	ymm1,ymm10,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,10
	vpxor	ymm9,ymm11,ymm4
	vpaddd	ymm13,ymm13,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm9,ymm9,ymm5
	vpaddd	ymm9,ymm9,ymm7
	vmovd	xmm5,DWORD[28+r12]
	vmovd	xmm0,DWORD[28+r8]
	vmovd	xmm1,DWORD[28+r13]
	vmovd	xmm2,DWORD[28+r9]
	vpinsrd	xmm5,xmm5,DWORD[28+r14],1
	vpinsrd	xmm0,xmm0,DWORD[28+r10],1
	vpinsrd	xmm1,xmm1,DWORD[28+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[28+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm13,6
	vpslld	ymm2,ymm13,26
	vmovdqu	YMMWORD[(224-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm8

	vpsrld	ymm1,ymm13,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm13,21
	vpaddd	ymm5,ymm5,YMMWORD[96+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm13,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,7
	vpandn	ymm0,ymm13,ymm15
	vpand	ymm4,ymm13,ymm14

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm8,ymm9,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm9,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm10,ymm9

	vpxor	ymm8,ymm8,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm9,13

	vpslld	ymm2,ymm9,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm8,ymm1

	vpsrld	ymm1,ymm9,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,10
	vpxor	ymm8,ymm10,ymm3
	vpaddd	ymm12,ymm12,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm8,ymm8,ymm5
	vpaddd	ymm8,ymm8,ymm7
	add	rbp,256
	vmovd	xmm5,DWORD[32+r12]
	vmovd	xmm0,DWORD[32+r8]
	vmovd	xmm1,DWORD[32+r13]
	vmovd	xmm2,DWORD[32+r9]
	vpinsrd	xmm5,xmm5,DWORD[32+r14],1
	vpinsrd	xmm0,xmm0,DWORD[32+r10],1
	vpinsrd	xmm1,xmm1,DWORD[32+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[32+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm12,6
	vpslld	ymm2,ymm12,26
	vmovdqu	YMMWORD[(256-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm15

	vpsrld	ymm1,ymm12,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm12,21
	vpaddd	ymm5,ymm5,YMMWORD[((-128))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm12,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,7
	vpandn	ymm0,ymm12,ymm14
	vpand	ymm3,ymm12,ymm13

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm15,ymm8,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm8,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm9,ymm8

	vpxor	ymm15,ymm15,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm8,13

	vpslld	ymm2,ymm8,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm15,ymm1

	vpsrld	ymm1,ymm8,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,10
	vpxor	ymm15,ymm9,ymm4
	vpaddd	ymm11,ymm11,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm15,ymm15,ymm5
	vpaddd	ymm15,ymm15,ymm7
	vmovd	xmm5,DWORD[36+r12]
	vmovd	xmm0,DWORD[36+r8]
	vmovd	xmm1,DWORD[36+r13]
	vmovd	xmm2,DWORD[36+r9]
	vpinsrd	xmm5,xmm5,DWORD[36+r14],1
	vpinsrd	xmm0,xmm0,DWORD[36+r10],1
	vpinsrd	xmm1,xmm1,DWORD[36+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[36+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm11,6
	vpslld	ymm2,ymm11,26
	vmovdqu	YMMWORD[(288-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm14

	vpsrld	ymm1,ymm11,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm11,21
	vpaddd	ymm5,ymm5,YMMWORD[((-96))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm11,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,7
	vpandn	ymm0,ymm11,ymm13
	vpand	ymm4,ymm11,ymm12

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm14,ymm15,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm15,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm8,ymm15

	vpxor	ymm14,ymm14,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm15,13

	vpslld	ymm2,ymm15,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm14,ymm1

	vpsrld	ymm1,ymm15,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,10
	vpxor	ymm14,ymm8,ymm3
	vpaddd	ymm10,ymm10,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm14,ymm14,ymm5
	vpaddd	ymm14,ymm14,ymm7
	vmovd	xmm5,DWORD[40+r12]
	vmovd	xmm0,DWORD[40+r8]
	vmovd	xmm1,DWORD[40+r13]
	vmovd	xmm2,DWORD[40+r9]
	vpinsrd	xmm5,xmm5,DWORD[40+r14],1
	vpinsrd	xmm0,xmm0,DWORD[40+r10],1
	vpinsrd	xmm1,xmm1,DWORD[40+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[40+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm10,6
	vpslld	ymm2,ymm10,26
	vmovdqu	YMMWORD[(320-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm13

	vpsrld	ymm1,ymm10,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm10,21
	vpaddd	ymm5,ymm5,YMMWORD[((-64))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm10,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,7
	vpandn	ymm0,ymm10,ymm12
	vpand	ymm3,ymm10,ymm11

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm13,ymm14,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm14,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm15,ymm14

	vpxor	ymm13,ymm13,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm14,13

	vpslld	ymm2,ymm14,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm13,ymm1

	vpsrld	ymm1,ymm14,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,10
	vpxor	ymm13,ymm15,ymm4
	vpaddd	ymm9,ymm9,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm13,ymm13,ymm5
	vpaddd	ymm13,ymm13,ymm7
	vmovd	xmm5,DWORD[44+r12]
	vmovd	xmm0,DWORD[44+r8]
	vmovd	xmm1,DWORD[44+r13]
	vmovd	xmm2,DWORD[44+r9]
	vpinsrd	xmm5,xmm5,DWORD[44+r14],1
	vpinsrd	xmm0,xmm0,DWORD[44+r10],1
	vpinsrd	xmm1,xmm1,DWORD[44+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[44+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm9,6
	vpslld	ymm2,ymm9,26
	vmovdqu	YMMWORD[(352-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm12

	vpsrld	ymm1,ymm9,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm9,21
	vpaddd	ymm5,ymm5,YMMWORD[((-32))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm9,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,7
	vpandn	ymm0,ymm9,ymm11
	vpand	ymm4,ymm9,ymm10

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm12,ymm13,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm13,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm14,ymm13

	vpxor	ymm12,ymm12,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm13,13

	vpslld	ymm2,ymm13,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm12,ymm1

	vpsrld	ymm1,ymm13,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,10
	vpxor	ymm12,ymm14,ymm3
	vpaddd	ymm8,ymm8,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm12,ymm12,ymm5
	vpaddd	ymm12,ymm12,ymm7
	vmovd	xmm5,DWORD[48+r12]
	vmovd	xmm0,DWORD[48+r8]
	vmovd	xmm1,DWORD[48+r13]
	vmovd	xmm2,DWORD[48+r9]
	vpinsrd	xmm5,xmm5,DWORD[48+r14],1
	vpinsrd	xmm0,xmm0,DWORD[48+r10],1
	vpinsrd	xmm1,xmm1,DWORD[48+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[48+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm8,6
	vpslld	ymm2,ymm8,26
	vmovdqu	YMMWORD[(384-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm11

	vpsrld	ymm1,ymm8,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm8,21
	vpaddd	ymm5,ymm5,YMMWORD[rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm8,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,7
	vpandn	ymm0,ymm8,ymm10
	vpand	ymm3,ymm8,ymm9

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm11,ymm12,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm12,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm13,ymm12

	vpxor	ymm11,ymm11,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm12,13

	vpslld	ymm2,ymm12,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm11,ymm1

	vpsrld	ymm1,ymm12,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,10
	vpxor	ymm11,ymm13,ymm4
	vpaddd	ymm15,ymm15,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm11,ymm11,ymm5
	vpaddd	ymm11,ymm11,ymm7
	vmovd	xmm5,DWORD[52+r12]
	vmovd	xmm0,DWORD[52+r8]
	vmovd	xmm1,DWORD[52+r13]
	vmovd	xmm2,DWORD[52+r9]
	vpinsrd	xmm5,xmm5,DWORD[52+r14],1
	vpinsrd	xmm0,xmm0,DWORD[52+r10],1
	vpinsrd	xmm1,xmm1,DWORD[52+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[52+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm15,6
	vpslld	ymm2,ymm15,26
	vmovdqu	YMMWORD[(416-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm10

	vpsrld	ymm1,ymm15,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm15,21
	vpaddd	ymm5,ymm5,YMMWORD[32+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm15,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,7
	vpandn	ymm0,ymm15,ymm9
	vpand	ymm4,ymm15,ymm8

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm10,ymm11,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm11,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm12,ymm11

	vpxor	ymm10,ymm10,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm11,13

	vpslld	ymm2,ymm11,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm10,ymm1

	vpsrld	ymm1,ymm11,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,10
	vpxor	ymm10,ymm12,ymm3
	vpaddd	ymm14,ymm14,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm10,ymm10,ymm5
	vpaddd	ymm10,ymm10,ymm7
	vmovd	xmm5,DWORD[56+r12]
	vmovd	xmm0,DWORD[56+r8]
	vmovd	xmm1,DWORD[56+r13]
	vmovd	xmm2,DWORD[56+r9]
	vpinsrd	xmm5,xmm5,DWORD[56+r14],1
	vpinsrd	xmm0,xmm0,DWORD[56+r10],1
	vpinsrd	xmm1,xmm1,DWORD[56+r15],1
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[56+r11],1
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm14,6
	vpslld	ymm2,ymm14,26
	vmovdqu	YMMWORD[(448-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm9

	vpsrld	ymm1,ymm14,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm14,21
	vpaddd	ymm5,ymm5,YMMWORD[64+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm14,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,7
	vpandn	ymm0,ymm14,ymm8
	vpand	ymm3,ymm14,ymm15

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm9,ymm10,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm10,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm11,ymm10

	vpxor	ymm9,ymm9,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm10,13

	vpslld	ymm2,ymm10,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm9,ymm1

	vpsrld	ymm1,ymm10,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,10
	vpxor	ymm9,ymm11,ymm4
	vpaddd	ymm13,ymm13,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm9,ymm9,ymm5
	vpaddd	ymm9,ymm9,ymm7
	vmovd	xmm5,DWORD[60+r12]
	lea	r12,[64+r12]
	vmovd	xmm0,DWORD[60+r8]
	lea	r8,[64+r8]
	vmovd	xmm1,DWORD[60+r13]
	lea	r13,[64+r13]
	vmovd	xmm2,DWORD[60+r9]
	lea	r9,[64+r9]
	vpinsrd	xmm5,xmm5,DWORD[60+r14],1
	lea	r14,[64+r14]
	vpinsrd	xmm0,xmm0,DWORD[60+r10],1
	lea	r10,[64+r10]
	vpinsrd	xmm1,xmm1,DWORD[60+r15],1
	lea	r15,[64+r15]
	vpunpckldq	ymm5,ymm5,ymm1
	vpinsrd	xmm2,xmm2,DWORD[60+r11],1
	lea	r11,[64+r11]
	vpunpckldq	ymm0,ymm0,ymm2
	vinserti128	ymm5,ymm5,xmm0,1
	vpshufb	ymm5,ymm5,ymm6
	vpsrld	ymm7,ymm13,6
	vpslld	ymm2,ymm13,26
	vmovdqu	YMMWORD[(480-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm8

	vpsrld	ymm1,ymm13,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm13,21
	vpaddd	ymm5,ymm5,YMMWORD[96+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm13,25
	vpxor	ymm7,ymm7,ymm2
	prefetcht0	[63+r12]
	vpslld	ymm2,ymm13,7
	vpandn	ymm0,ymm13,ymm15
	vpand	ymm4,ymm13,ymm14
	prefetcht0	[63+r13]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm8,ymm9,2
	vpxor	ymm7,ymm7,ymm2
	prefetcht0	[63+r14]
	vpslld	ymm1,ymm9,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm10,ymm9
	prefetcht0	[63+r15]
	vpxor	ymm8,ymm8,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm9,13
	prefetcht0	[63+r8]
	vpslld	ymm2,ymm9,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm3,ymm3,ymm4
	prefetcht0	[63+r9]
	vpxor	ymm7,ymm8,ymm1

	vpsrld	ymm1,ymm9,22
	vpxor	ymm7,ymm7,ymm2
	prefetcht0	[63+r10]
	vpslld	ymm2,ymm9,10
	vpxor	ymm8,ymm10,ymm3
	vpaddd	ymm12,ymm12,ymm5
	prefetcht0	[63+r11]
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm8,ymm8,ymm5
	vpaddd	ymm8,ymm8,ymm7
	add	rbp,256
	vmovdqu	ymm5,YMMWORD[((0-128))+rax]
	mov	ecx,3
	jmp	NEAR $L$oop_16_xx_avx2
ALIGN	32
$L$oop_16_xx_avx2:
	vmovdqu	ymm6,YMMWORD[((32-128))+rax]
	vpaddd	ymm5,ymm5,YMMWORD[((288-256-128))+rbx]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((448-256-128))+rbx]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm12,6
	vpslld	ymm2,ymm12,26
	vmovdqu	YMMWORD[(0-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm15

	vpsrld	ymm1,ymm12,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm12,21
	vpaddd	ymm5,ymm5,YMMWORD[((-128))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm12,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,7
	vpandn	ymm0,ymm12,ymm14
	vpand	ymm3,ymm12,ymm13

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm15,ymm8,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm8,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm9,ymm8

	vpxor	ymm15,ymm15,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm8,13

	vpslld	ymm2,ymm8,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm15,ymm1

	vpsrld	ymm1,ymm8,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,10
	vpxor	ymm15,ymm9,ymm4
	vpaddd	ymm11,ymm11,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm15,ymm15,ymm5
	vpaddd	ymm15,ymm15,ymm7
	vmovdqu	ymm5,YMMWORD[((64-128))+rax]
	vpaddd	ymm6,ymm6,YMMWORD[((320-256-128))+rbx]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((480-256-128))+rbx]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm11,6
	vpslld	ymm2,ymm11,26
	vmovdqu	YMMWORD[(32-128)+rax],ymm6
	vpaddd	ymm6,ymm6,ymm14

	vpsrld	ymm1,ymm11,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm11,21
	vpaddd	ymm6,ymm6,YMMWORD[((-96))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm11,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,7
	vpandn	ymm0,ymm11,ymm13
	vpand	ymm4,ymm11,ymm12

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm14,ymm15,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm15,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm8,ymm15

	vpxor	ymm14,ymm14,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm15,13

	vpslld	ymm2,ymm15,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm14,ymm1

	vpsrld	ymm1,ymm15,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,10
	vpxor	ymm14,ymm8,ymm3
	vpaddd	ymm10,ymm10,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm14,ymm14,ymm6
	vpaddd	ymm14,ymm14,ymm7
	vmovdqu	ymm6,YMMWORD[((96-128))+rax]
	vpaddd	ymm5,ymm5,YMMWORD[((352-256-128))+rbx]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((0-128))+rax]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm10,6
	vpslld	ymm2,ymm10,26
	vmovdqu	YMMWORD[(64-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm13

	vpsrld	ymm1,ymm10,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm10,21
	vpaddd	ymm5,ymm5,YMMWORD[((-64))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm10,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,7
	vpandn	ymm0,ymm10,ymm12
	vpand	ymm3,ymm10,ymm11

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm13,ymm14,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm14,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm15,ymm14

	vpxor	ymm13,ymm13,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm14,13

	vpslld	ymm2,ymm14,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm13,ymm1

	vpsrld	ymm1,ymm14,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,10
	vpxor	ymm13,ymm15,ymm4
	vpaddd	ymm9,ymm9,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm13,ymm13,ymm5
	vpaddd	ymm13,ymm13,ymm7
	vmovdqu	ymm5,YMMWORD[((128-128))+rax]
	vpaddd	ymm6,ymm6,YMMWORD[((384-256-128))+rbx]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((32-128))+rax]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm9,6
	vpslld	ymm2,ymm9,26
	vmovdqu	YMMWORD[(96-128)+rax],ymm6
	vpaddd	ymm6,ymm6,ymm12

	vpsrld	ymm1,ymm9,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm9,21
	vpaddd	ymm6,ymm6,YMMWORD[((-32))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm9,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,7
	vpandn	ymm0,ymm9,ymm11
	vpand	ymm4,ymm9,ymm10

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm12,ymm13,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm13,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm14,ymm13

	vpxor	ymm12,ymm12,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm13,13

	vpslld	ymm2,ymm13,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm12,ymm1

	vpsrld	ymm1,ymm13,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,10
	vpxor	ymm12,ymm14,ymm3
	vpaddd	ymm8,ymm8,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm12,ymm12,ymm6
	vpaddd	ymm12,ymm12,ymm7
	vmovdqu	ymm6,YMMWORD[((160-128))+rax]
	vpaddd	ymm5,ymm5,YMMWORD[((416-256-128))+rbx]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((64-128))+rax]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm8,6
	vpslld	ymm2,ymm8,26
	vmovdqu	YMMWORD[(128-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm11

	vpsrld	ymm1,ymm8,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm8,21
	vpaddd	ymm5,ymm5,YMMWORD[rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm8,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,7
	vpandn	ymm0,ymm8,ymm10
	vpand	ymm3,ymm8,ymm9

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm11,ymm12,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm12,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm13,ymm12

	vpxor	ymm11,ymm11,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm12,13

	vpslld	ymm2,ymm12,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm11,ymm1

	vpsrld	ymm1,ymm12,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,10
	vpxor	ymm11,ymm13,ymm4
	vpaddd	ymm15,ymm15,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm11,ymm11,ymm5
	vpaddd	ymm11,ymm11,ymm7
	vmovdqu	ymm5,YMMWORD[((192-128))+rax]
	vpaddd	ymm6,ymm6,YMMWORD[((448-256-128))+rbx]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((96-128))+rax]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm15,6
	vpslld	ymm2,ymm15,26
	vmovdqu	YMMWORD[(160-128)+rax],ymm6
	vpaddd	ymm6,ymm6,ymm10

	vpsrld	ymm1,ymm15,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm15,21
	vpaddd	ymm6,ymm6,YMMWORD[32+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm15,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,7
	vpandn	ymm0,ymm15,ymm9
	vpand	ymm4,ymm15,ymm8

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm10,ymm11,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm11,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm12,ymm11

	vpxor	ymm10,ymm10,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm11,13

	vpslld	ymm2,ymm11,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm10,ymm1

	vpsrld	ymm1,ymm11,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,10
	vpxor	ymm10,ymm12,ymm3
	vpaddd	ymm14,ymm14,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm10,ymm10,ymm6
	vpaddd	ymm10,ymm10,ymm7
	vmovdqu	ymm6,YMMWORD[((224-128))+rax]
	vpaddd	ymm5,ymm5,YMMWORD[((480-256-128))+rbx]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((128-128))+rax]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm14,6
	vpslld	ymm2,ymm14,26
	vmovdqu	YMMWORD[(192-128)+rax],ymm5
	vpaddd	ymm5,ymm5,ymm9

	vpsrld	ymm1,ymm14,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm14,21
	vpaddd	ymm5,ymm5,YMMWORD[64+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm14,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,7
	vpandn	ymm0,ymm14,ymm8
	vpand	ymm3,ymm14,ymm15

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm9,ymm10,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm10,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm11,ymm10

	vpxor	ymm9,ymm9,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm10,13

	vpslld	ymm2,ymm10,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm9,ymm1

	vpsrld	ymm1,ymm10,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,10
	vpxor	ymm9,ymm11,ymm4
	vpaddd	ymm13,ymm13,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm9,ymm9,ymm5
	vpaddd	ymm9,ymm9,ymm7
	vmovdqu	ymm5,YMMWORD[((256-256-128))+rbx]
	vpaddd	ymm6,ymm6,YMMWORD[((0-128))+rax]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((160-128))+rax]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm13,6
	vpslld	ymm2,ymm13,26
	vmovdqu	YMMWORD[(224-128)+rax],ymm6
	vpaddd	ymm6,ymm6,ymm8

	vpsrld	ymm1,ymm13,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm13,21
	vpaddd	ymm6,ymm6,YMMWORD[96+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm13,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,7
	vpandn	ymm0,ymm13,ymm15
	vpand	ymm4,ymm13,ymm14

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm8,ymm9,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm9,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm10,ymm9

	vpxor	ymm8,ymm8,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm9,13

	vpslld	ymm2,ymm9,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm8,ymm1

	vpsrld	ymm1,ymm9,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,10
	vpxor	ymm8,ymm10,ymm3
	vpaddd	ymm12,ymm12,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm8,ymm8,ymm6
	vpaddd	ymm8,ymm8,ymm7
	add	rbp,256
	vmovdqu	ymm6,YMMWORD[((288-256-128))+rbx]
	vpaddd	ymm5,ymm5,YMMWORD[((32-128))+rax]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((192-128))+rax]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm12,6
	vpslld	ymm2,ymm12,26
	vmovdqu	YMMWORD[(256-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm15

	vpsrld	ymm1,ymm12,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm12,21
	vpaddd	ymm5,ymm5,YMMWORD[((-128))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm12,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,7
	vpandn	ymm0,ymm12,ymm14
	vpand	ymm3,ymm12,ymm13

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm15,ymm8,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm8,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm9,ymm8

	vpxor	ymm15,ymm15,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm8,13

	vpslld	ymm2,ymm8,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm15,ymm1

	vpsrld	ymm1,ymm8,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,10
	vpxor	ymm15,ymm9,ymm4
	vpaddd	ymm11,ymm11,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm15,ymm15,ymm5
	vpaddd	ymm15,ymm15,ymm7
	vmovdqu	ymm5,YMMWORD[((320-256-128))+rbx]
	vpaddd	ymm6,ymm6,YMMWORD[((64-128))+rax]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((224-128))+rax]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm11,6
	vpslld	ymm2,ymm11,26
	vmovdqu	YMMWORD[(288-256-128)+rbx],ymm6
	vpaddd	ymm6,ymm6,ymm14

	vpsrld	ymm1,ymm11,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm11,21
	vpaddd	ymm6,ymm6,YMMWORD[((-96))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm11,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,7
	vpandn	ymm0,ymm11,ymm13
	vpand	ymm4,ymm11,ymm12

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm14,ymm15,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm15,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm8,ymm15

	vpxor	ymm14,ymm14,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm15,13

	vpslld	ymm2,ymm15,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm14,ymm1

	vpsrld	ymm1,ymm15,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,10
	vpxor	ymm14,ymm8,ymm3
	vpaddd	ymm10,ymm10,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm14,ymm14,ymm6
	vpaddd	ymm14,ymm14,ymm7
	vmovdqu	ymm6,YMMWORD[((352-256-128))+rbx]
	vpaddd	ymm5,ymm5,YMMWORD[((96-128))+rax]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((256-256-128))+rbx]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm10,6
	vpslld	ymm2,ymm10,26
	vmovdqu	YMMWORD[(320-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm13

	vpsrld	ymm1,ymm10,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm10,21
	vpaddd	ymm5,ymm5,YMMWORD[((-64))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm10,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,7
	vpandn	ymm0,ymm10,ymm12
	vpand	ymm3,ymm10,ymm11

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm13,ymm14,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm14,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm15,ymm14

	vpxor	ymm13,ymm13,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm14,13

	vpslld	ymm2,ymm14,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm13,ymm1

	vpsrld	ymm1,ymm14,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,10
	vpxor	ymm13,ymm15,ymm4
	vpaddd	ymm9,ymm9,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm13,ymm13,ymm5
	vpaddd	ymm13,ymm13,ymm7
	vmovdqu	ymm5,YMMWORD[((384-256-128))+rbx]
	vpaddd	ymm6,ymm6,YMMWORD[((128-128))+rax]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((288-256-128))+rbx]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm9,6
	vpslld	ymm2,ymm9,26
	vmovdqu	YMMWORD[(352-256-128)+rbx],ymm6
	vpaddd	ymm6,ymm6,ymm12

	vpsrld	ymm1,ymm9,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm9,21
	vpaddd	ymm6,ymm6,YMMWORD[((-32))+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm9,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,7
	vpandn	ymm0,ymm9,ymm11
	vpand	ymm4,ymm9,ymm10

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm12,ymm13,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm13,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm14,ymm13

	vpxor	ymm12,ymm12,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm13,13

	vpslld	ymm2,ymm13,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm12,ymm1

	vpsrld	ymm1,ymm13,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,10
	vpxor	ymm12,ymm14,ymm3
	vpaddd	ymm8,ymm8,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm12,ymm12,ymm6
	vpaddd	ymm12,ymm12,ymm7
	vmovdqu	ymm6,YMMWORD[((416-256-128))+rbx]
	vpaddd	ymm5,ymm5,YMMWORD[((160-128))+rax]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((320-256-128))+rbx]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm8,6
	vpslld	ymm2,ymm8,26
	vmovdqu	YMMWORD[(384-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm11

	vpsrld	ymm1,ymm8,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm8,21
	vpaddd	ymm5,ymm5,YMMWORD[rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm8,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm8,7
	vpandn	ymm0,ymm8,ymm10
	vpand	ymm3,ymm8,ymm9

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm11,ymm12,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm12,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm13,ymm12

	vpxor	ymm11,ymm11,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm12,13

	vpslld	ymm2,ymm12,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm11,ymm1

	vpsrld	ymm1,ymm12,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm12,10
	vpxor	ymm11,ymm13,ymm4
	vpaddd	ymm15,ymm15,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm11,ymm11,ymm5
	vpaddd	ymm11,ymm11,ymm7
	vmovdqu	ymm5,YMMWORD[((448-256-128))+rbx]
	vpaddd	ymm6,ymm6,YMMWORD[((192-128))+rax]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((352-256-128))+rbx]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm15,6
	vpslld	ymm2,ymm15,26
	vmovdqu	YMMWORD[(416-256-128)+rbx],ymm6
	vpaddd	ymm6,ymm6,ymm10

	vpsrld	ymm1,ymm15,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm15,21
	vpaddd	ymm6,ymm6,YMMWORD[32+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm15,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm15,7
	vpandn	ymm0,ymm15,ymm9
	vpand	ymm4,ymm15,ymm8

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm10,ymm11,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm11,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm12,ymm11

	vpxor	ymm10,ymm10,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm11,13

	vpslld	ymm2,ymm11,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm10,ymm1

	vpsrld	ymm1,ymm11,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm11,10
	vpxor	ymm10,ymm12,ymm3
	vpaddd	ymm14,ymm14,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm10,ymm10,ymm6
	vpaddd	ymm10,ymm10,ymm7
	vmovdqu	ymm6,YMMWORD[((480-256-128))+rbx]
	vpaddd	ymm5,ymm5,YMMWORD[((224-128))+rax]

	vpsrld	ymm7,ymm6,3
	vpsrld	ymm1,ymm6,7
	vpslld	ymm2,ymm6,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm6,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm6,14
	vmovdqu	ymm0,YMMWORD[((384-256-128))+rbx]
	vpsrld	ymm3,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm5,ymm5,ymm7
	vpxor	ymm7,ymm3,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm5,ymm5,ymm7
	vpsrld	ymm7,ymm14,6
	vpslld	ymm2,ymm14,26
	vmovdqu	YMMWORD[(448-256-128)+rbx],ymm5
	vpaddd	ymm5,ymm5,ymm9

	vpsrld	ymm1,ymm14,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm14,21
	vpaddd	ymm5,ymm5,YMMWORD[64+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm14,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm14,7
	vpandn	ymm0,ymm14,ymm8
	vpand	ymm3,ymm14,ymm15

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm9,ymm10,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm10,30
	vpxor	ymm0,ymm0,ymm3
	vpxor	ymm3,ymm11,ymm10

	vpxor	ymm9,ymm9,ymm1
	vpaddd	ymm5,ymm5,ymm7

	vpsrld	ymm1,ymm10,13

	vpslld	ymm2,ymm10,19
	vpaddd	ymm5,ymm5,ymm0
	vpand	ymm4,ymm4,ymm3

	vpxor	ymm7,ymm9,ymm1

	vpsrld	ymm1,ymm10,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm10,10
	vpxor	ymm9,ymm11,ymm4
	vpaddd	ymm13,ymm13,ymm5

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm9,ymm9,ymm5
	vpaddd	ymm9,ymm9,ymm7
	vmovdqu	ymm5,YMMWORD[((0-128))+rax]
	vpaddd	ymm6,ymm6,YMMWORD[((256-256-128))+rbx]

	vpsrld	ymm7,ymm5,3
	vpsrld	ymm1,ymm5,7
	vpslld	ymm2,ymm5,25
	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm5,18
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm5,14
	vmovdqu	ymm0,YMMWORD[((416-256-128))+rbx]
	vpsrld	ymm4,ymm0,10

	vpxor	ymm7,ymm7,ymm1
	vpsrld	ymm1,ymm0,17
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,15
	vpaddd	ymm6,ymm6,ymm7
	vpxor	ymm7,ymm4,ymm1
	vpsrld	ymm1,ymm0,19
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm0,13
	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2
	vpaddd	ymm6,ymm6,ymm7
	vpsrld	ymm7,ymm13,6
	vpslld	ymm2,ymm13,26
	vmovdqu	YMMWORD[(480-256-128)+rbx],ymm6
	vpaddd	ymm6,ymm6,ymm8

	vpsrld	ymm1,ymm13,11
	vpxor	ymm7,ymm7,ymm2
	vpslld	ymm2,ymm13,21
	vpaddd	ymm6,ymm6,YMMWORD[96+rbp]
	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm1,ymm13,25
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm13,7
	vpandn	ymm0,ymm13,ymm15
	vpand	ymm4,ymm13,ymm14

	vpxor	ymm7,ymm7,ymm1

	vpsrld	ymm8,ymm9,2
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm1,ymm9,30
	vpxor	ymm0,ymm0,ymm4
	vpxor	ymm4,ymm10,ymm9

	vpxor	ymm8,ymm8,ymm1
	vpaddd	ymm6,ymm6,ymm7

	vpsrld	ymm1,ymm9,13

	vpslld	ymm2,ymm9,19
	vpaddd	ymm6,ymm6,ymm0
	vpand	ymm3,ymm3,ymm4

	vpxor	ymm7,ymm8,ymm1

	vpsrld	ymm1,ymm9,22
	vpxor	ymm7,ymm7,ymm2

	vpslld	ymm2,ymm9,10
	vpxor	ymm8,ymm10,ymm3
	vpaddd	ymm12,ymm12,ymm6

	vpxor	ymm7,ymm7,ymm1
	vpxor	ymm7,ymm7,ymm2

	vpaddd	ymm8,ymm8,ymm6
	vpaddd	ymm8,ymm8,ymm7
	add	rbp,256
	dec	ecx
	jnz	NEAR $L$oop_16_xx_avx2

	mov	ecx,1
	lea	rbx,[512+rsp]
	lea	rbp,[((K256+128))]
	cmp	ecx,DWORD[rbx]
	cmovge	r12,rbp
	cmp	ecx,DWORD[4+rbx]
	cmovge	r13,rbp
	cmp	ecx,DWORD[8+rbx]
	cmovge	r14,rbp
	cmp	ecx,DWORD[12+rbx]
	cmovge	r15,rbp
	cmp	ecx,DWORD[16+rbx]
	cmovge	r8,rbp
	cmp	ecx,DWORD[20+rbx]
	cmovge	r9,rbp
	cmp	ecx,DWORD[24+rbx]
	cmovge	r10,rbp
	cmp	ecx,DWORD[28+rbx]
	cmovge	r11,rbp
	vmovdqa	ymm7,YMMWORD[rbx]
	vpxor	ymm0,ymm0,ymm0
	vmovdqa	ymm6,ymm7
	vpcmpgtd	ymm6,ymm6,ymm0
	vpaddd	ymm7,ymm7,ymm6

	vmovdqu	ymm0,YMMWORD[((0-128))+rdi]
	vpand	ymm8,ymm8,ymm6
	vmovdqu	ymm1,YMMWORD[((32-128))+rdi]
	vpand	ymm9,ymm9,ymm6
	vmovdqu	ymm2,YMMWORD[((64-128))+rdi]
	vpand	ymm10,ymm10,ymm6
	vmovdqu	ymm5,YMMWORD[((96-128))+rdi]
	vpand	ymm11,ymm11,ymm6
	vpaddd	ymm8,ymm8,ymm0
	vmovdqu	ymm0,YMMWORD[((128-128))+rdi]
	vpand	ymm12,ymm12,ymm6
	vpaddd	ymm9,ymm9,ymm1
	vmovdqu	ymm1,YMMWORD[((160-128))+rdi]
	vpand	ymm13,ymm13,ymm6
	vpaddd	ymm10,ymm10,ymm2
	vmovdqu	ymm2,YMMWORD[((192-128))+rdi]
	vpand	ymm14,ymm14,ymm6
	vpaddd	ymm11,ymm11,ymm5
	vmovdqu	ymm5,YMMWORD[((224-128))+rdi]
	vpand	ymm15,ymm15,ymm6
	vpaddd	ymm12,ymm12,ymm0
	vpaddd	ymm13,ymm13,ymm1
	vmovdqu	YMMWORD[(0-128)+rdi],ymm8
	vpaddd	ymm14,ymm14,ymm2
	vmovdqu	YMMWORD[(32-128)+rdi],ymm9
	vpaddd	ymm15,ymm15,ymm5
	vmovdqu	YMMWORD[(64-128)+rdi],ymm10
	vmovdqu	YMMWORD[(96-128)+rdi],ymm11
	vmovdqu	YMMWORD[(128-128)+rdi],ymm12
	vmovdqu	YMMWORD[(160-128)+rdi],ymm13
	vmovdqu	YMMWORD[(192-128)+rdi],ymm14
	vmovdqu	YMMWORD[(224-128)+rdi],ymm15

	vmovdqu	YMMWORD[rbx],ymm7
	lea	rbx,[((256+128))+rsp]
	vmovdqu	ymm6,YMMWORD[$L$pbswap]
	dec	edx
	jnz	NEAR $L$oop_avx2







$L$done_avx2:
	mov	rax,QWORD[544+rsp]

	vzeroupper
	movaps	xmm6,XMMWORD[((-216))+rax]
	movaps	xmm7,XMMWORD[((-200))+rax]
	movaps	xmm8,XMMWORD[((-184))+rax]
	movaps	xmm9,XMMWORD[((-168))+rax]
	movaps	xmm10,XMMWORD[((-152))+rax]
	movaps	xmm11,XMMWORD[((-136))+rax]
	movaps	xmm12,XMMWORD[((-120))+rax]
	movaps	xmm13,XMMWORD[((-104))+rax]
	movaps	xmm14,XMMWORD[((-88))+rax]
	movaps	xmm15,XMMWORD[((-72))+rax]
	mov	r15,QWORD[((-48))+rax]

	mov	r14,QWORD[((-40))+rax]

	mov	r13,QWORD[((-32))+rax]

	mov	r12,QWORD[((-24))+rax]

	mov	rbp,QWORD[((-16))+rax]

	mov	rbx,QWORD[((-8))+rax]

	lea	rsp,[rax]

$L$epilogue_avx2:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_sha256_multi_block_avx2:
ALIGN	256
K256:
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
$L$pbswap:
	DD	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
	DD	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
K256_shaext:
	DD	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
	DD	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
	DD	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
	DD	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
	DD	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
	DD	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
	DD	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
	DD	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
	DD	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
	DD	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
	DD	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
	DD	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
	DD	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
	DD	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
	DD	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
	DD	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
DB	83,72,65,50,53,54,32,109,117,108,116,105,45,98,108,111
DB	99,107,32,116,114,97,110,115,102,111,114,109,32,102,111,114
DB	32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71
DB	65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112
DB	101,110,115,115,108,46,111,114,103,62,0
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
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$in_prologue

	mov	rax,QWORD[272+rax]

	mov	rbx,QWORD[((-8))+rax]
	mov	rbp,QWORD[((-16))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp

	lea	rsi,[((-24-160))+rax]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

$L$in_prologue:
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


ALIGN	16
avx2_handler:
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
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$in_prologue

	mov	rax,QWORD[544+r8]

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

	lea	rsi,[((-56-160))+rax]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

	jmp	NEAR $L$in_prologue

section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_sha256_multi_block wrt ..imagebase
	DD	$L$SEH_end_sha256_multi_block wrt ..imagebase
	DD	$L$SEH_info_sha256_multi_block wrt ..imagebase
	DD	$L$SEH_begin_sha256_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_end_sha256_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_info_sha256_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_begin_sha256_multi_block_avx wrt ..imagebase
	DD	$L$SEH_end_sha256_multi_block_avx wrt ..imagebase
	DD	$L$SEH_info_sha256_multi_block_avx wrt ..imagebase
	DD	$L$SEH_begin_sha256_multi_block_avx2 wrt ..imagebase
	DD	$L$SEH_end_sha256_multi_block_avx2 wrt ..imagebase
	DD	$L$SEH_info_sha256_multi_block_avx2 wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_sha256_multi_block:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body wrt ..imagebase,$L$epilogue wrt ..imagebase
$L$SEH_info_sha256_multi_block_shaext:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body_shaext wrt ..imagebase,$L$epilogue_shaext wrt ..imagebase
$L$SEH_info_sha256_multi_block_avx:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body_avx wrt ..imagebase,$L$epilogue_avx wrt ..imagebase
$L$SEH_info_sha256_multi_block_avx2:
DB	9,0,0,0
	DD	avx2_handler wrt ..imagebase
	DD	$L$body_avx2 wrt ..imagebase,$L$epilogue_avx2 wrt ..imagebase
