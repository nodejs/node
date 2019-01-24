default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

global	sha1_multi_block

ALIGN	32
sha1_multi_block:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block:
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
	lea	rbp,[K_XX_XX]
	lea	rbx,[256+rsp]

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

	movdqu	xmm10,XMMWORD[rdi]
	lea	rax,[128+rsp]
	movdqu	xmm11,XMMWORD[32+rdi]
	movdqu	xmm12,XMMWORD[64+rdi]
	movdqu	xmm13,XMMWORD[96+rdi]
	movdqu	xmm14,XMMWORD[128+rdi]
	movdqa	xmm5,XMMWORD[96+rbp]
	movdqa	xmm15,XMMWORD[((-32))+rbp]
	jmp	NEAR $L$oop

ALIGN	32
$L$oop:
	movd	xmm0,DWORD[r8]
	lea	r8,[64+r8]
	movd	xmm2,DWORD[r9]
	lea	r9,[64+r9]
	movd	xmm3,DWORD[r10]
	lea	r10,[64+r10]
	movd	xmm4,DWORD[r11]
	lea	r11,[64+r11]
	punpckldq	xmm0,xmm3
	movd	xmm1,DWORD[((-60))+r8]
	punpckldq	xmm2,xmm4
	movd	xmm9,DWORD[((-60))+r9]
	punpckldq	xmm0,xmm2
	movd	xmm8,DWORD[((-60))+r10]
DB	102,15,56,0,197
	movd	xmm7,DWORD[((-60))+r11]
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

	movdqa	XMMWORD[(0-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD[((-56))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-56))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD[((-56))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD[((-56))+r11]
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

	movdqa	XMMWORD[(16-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD[((-52))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-52))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD[((-52))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD[((-52))+r11]
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

	movdqa	XMMWORD[(32-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD[((-48))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-48))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD[((-48))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD[((-48))+r11]
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

	movdqa	XMMWORD[(48-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD[((-44))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-44))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD[((-44))+r10]
	por	xmm13,xmm7
	movd	xmm7,DWORD[((-44))+r11]
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

	movdqa	XMMWORD[(64-128)+rax],xmm4
	paddd	xmm10,xmm4
	movd	xmm1,DWORD[((-40))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm12

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-40))+r9]
	pslld	xmm7,30
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
DB	102,15,56,0,197
	movd	xmm8,DWORD[((-40))+r10]
	por	xmm12,xmm7
	movd	xmm7,DWORD[((-40))+r11]
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

	movdqa	XMMWORD[(80-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD[((-36))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-36))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD[((-36))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD[((-36))+r11]
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

	movdqa	XMMWORD[(96-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD[((-32))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-32))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD[((-32))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD[((-32))+r11]
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

	movdqa	XMMWORD[(112-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD[((-28))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-28))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD[((-28))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD[((-28))+r11]
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

	movdqa	XMMWORD[(128-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD[((-24))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-24))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD[((-24))+r10]
	por	xmm13,xmm7
	movd	xmm7,DWORD[((-24))+r11]
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

	movdqa	XMMWORD[(144-128)+rax],xmm4
	paddd	xmm10,xmm4
	movd	xmm1,DWORD[((-20))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm12

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-20))+r9]
	pslld	xmm7,30
	paddd	xmm10,xmm6

	psrld	xmm12,2
	paddd	xmm10,xmm8
DB	102,15,56,0,197
	movd	xmm8,DWORD[((-20))+r10]
	por	xmm12,xmm7
	movd	xmm7,DWORD[((-20))+r11]
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

	movdqa	XMMWORD[(160-128)+rax],xmm0
	paddd	xmm14,xmm0
	movd	xmm2,DWORD[((-16))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm11

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-16))+r9]
	pslld	xmm7,30
	paddd	xmm14,xmm6

	psrld	xmm11,2
	paddd	xmm14,xmm8
DB	102,15,56,0,205
	movd	xmm8,DWORD[((-16))+r10]
	por	xmm11,xmm7
	movd	xmm7,DWORD[((-16))+r11]
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

	movdqa	XMMWORD[(176-128)+rax],xmm1
	paddd	xmm13,xmm1
	movd	xmm3,DWORD[((-12))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm10

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-12))+r9]
	pslld	xmm7,30
	paddd	xmm13,xmm6

	psrld	xmm10,2
	paddd	xmm13,xmm8
DB	102,15,56,0,213
	movd	xmm8,DWORD[((-12))+r10]
	por	xmm10,xmm7
	movd	xmm7,DWORD[((-12))+r11]
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

	movdqa	XMMWORD[(192-128)+rax],xmm2
	paddd	xmm12,xmm2
	movd	xmm4,DWORD[((-8))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm14

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-8))+r9]
	pslld	xmm7,30
	paddd	xmm12,xmm6

	psrld	xmm14,2
	paddd	xmm12,xmm8
DB	102,15,56,0,221
	movd	xmm8,DWORD[((-8))+r10]
	por	xmm14,xmm7
	movd	xmm7,DWORD[((-8))+r11]
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

	movdqa	XMMWORD[(208-128)+rax],xmm3
	paddd	xmm11,xmm3
	movd	xmm0,DWORD[((-4))+r8]
	psrld	xmm9,27
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm13

	por	xmm8,xmm9
	movd	xmm9,DWORD[((-4))+r9]
	pslld	xmm7,30
	paddd	xmm11,xmm6

	psrld	xmm13,2
	paddd	xmm11,xmm8
DB	102,15,56,0,229
	movd	xmm8,DWORD[((-4))+r10]
	por	xmm13,xmm7
	movdqa	xmm1,XMMWORD[((0-128))+rax]
	movd	xmm7,DWORD[((-4))+r11]
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

	movdqa	XMMWORD[(224-128)+rax],xmm4
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
	movdqa	xmm2,XMMWORD[((16-128))+rax]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD[((32-128))+rax]

	movdqa	xmm8,xmm10
	pxor	xmm1,XMMWORD[((128-128))+rax]
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

	movdqa	XMMWORD[(240-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((48-128))+rax]

	movdqa	xmm8,xmm14
	pxor	xmm2,XMMWORD[((144-128))+rax]
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

	movdqa	XMMWORD[(0-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((64-128))+rax]

	movdqa	xmm8,xmm13
	pxor	xmm3,XMMWORD[((160-128))+rax]
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

	movdqa	XMMWORD[(16-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((80-128))+rax]

	movdqa	xmm8,xmm12
	pxor	xmm4,XMMWORD[((176-128))+rax]
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

	movdqa	XMMWORD[(32-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((96-128))+rax]

	movdqa	xmm8,xmm11
	pxor	xmm0,XMMWORD[((192-128))+rax]
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

	movdqa	XMMWORD[(48-128)+rax],xmm4
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
	movdqa	xmm15,XMMWORD[rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD[((112-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((208-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(64-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((128-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((224-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(80-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((144-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((240-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(96-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((160-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((0-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(112-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((176-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((16-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(128-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((192-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((32-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(144-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((208-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((48-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(160-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((224-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((64-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(176-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((240-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((80-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(192-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((0-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((96-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(208-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((16-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((112-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(224-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((32-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((128-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(240-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((48-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((144-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(0-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((64-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((160-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(16-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((80-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((176-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(32-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((96-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((192-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(48-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((112-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((208-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(64-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((128-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((224-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(80-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((144-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((240-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(96-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((160-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((0-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(112-128)+rax],xmm4
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
	movdqa	xmm15,XMMWORD[32+rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD[((176-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD[((16-128))+rax]
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

	movdqa	XMMWORD[(128-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((192-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD[((32-128))+rax]
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

	movdqa	XMMWORD[(144-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((208-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD[((48-128))+rax]
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

	movdqa	XMMWORD[(160-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((224-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD[((64-128))+rax]
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

	movdqa	XMMWORD[(176-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((240-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD[((80-128))+rax]
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

	movdqa	XMMWORD[(192-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((0-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD[((96-128))+rax]
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

	movdqa	XMMWORD[(208-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((16-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD[((112-128))+rax]
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

	movdqa	XMMWORD[(224-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((32-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD[((128-128))+rax]
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

	movdqa	XMMWORD[(240-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((48-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD[((144-128))+rax]
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

	movdqa	XMMWORD[(0-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((64-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD[((160-128))+rax]
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

	movdqa	XMMWORD[(16-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((80-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD[((176-128))+rax]
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

	movdqa	XMMWORD[(32-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((96-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD[((192-128))+rax]
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

	movdqa	XMMWORD[(48-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((112-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD[((208-128))+rax]
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

	movdqa	XMMWORD[(64-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((128-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD[((224-128))+rax]
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

	movdqa	XMMWORD[(80-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((144-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD[((240-128))+rax]
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

	movdqa	XMMWORD[(96-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((160-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm7,xmm13
	pxor	xmm1,XMMWORD[((0-128))+rax]
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

	movdqa	XMMWORD[(112-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((176-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm7,xmm12
	pxor	xmm2,XMMWORD[((16-128))+rax]
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

	movdqa	XMMWORD[(128-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((192-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm7,xmm11
	pxor	xmm3,XMMWORD[((32-128))+rax]
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

	movdqa	XMMWORD[(144-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((208-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm7,xmm10
	pxor	xmm4,XMMWORD[((48-128))+rax]
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

	movdqa	XMMWORD[(160-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((224-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm7,xmm14
	pxor	xmm0,XMMWORD[((64-128))+rax]
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

	movdqa	XMMWORD[(176-128)+rax],xmm4
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
	movdqa	xmm15,XMMWORD[64+rbp]
	pxor	xmm1,xmm3
	movdqa	xmm3,XMMWORD[((240-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((80-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(192-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((0-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((96-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(208-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((16-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((112-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(224-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((32-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((128-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(240-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((48-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((144-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(0-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((64-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((160-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(16-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((80-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((176-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(32-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((96-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((192-128))+rax]
	paddd	xmm12,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm14

	movdqa	xmm9,xmm13
	movdqa	XMMWORD[(48-128)+rax],xmm2
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
	movdqa	xmm1,XMMWORD[((112-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((208-128))+rax]
	paddd	xmm11,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm13

	movdqa	xmm9,xmm12
	movdqa	XMMWORD[(64-128)+rax],xmm3
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
	movdqa	xmm2,XMMWORD[((128-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((224-128))+rax]
	paddd	xmm10,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm12

	movdqa	xmm9,xmm11
	movdqa	XMMWORD[(80-128)+rax],xmm4
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
	movdqa	xmm3,XMMWORD[((144-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((240-128))+rax]
	paddd	xmm14,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm11

	movdqa	xmm9,xmm10
	movdqa	XMMWORD[(96-128)+rax],xmm0
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
	movdqa	xmm4,XMMWORD[((160-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((0-128))+rax]
	paddd	xmm13,xmm15
	pslld	xmm8,5
	pxor	xmm6,xmm10

	movdqa	xmm9,xmm14
	movdqa	XMMWORD[(112-128)+rax],xmm1
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
	movdqa	xmm0,XMMWORD[((176-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((16-128))+rax]
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
	movdqa	xmm1,XMMWORD[((192-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((32-128))+rax]
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
	movdqa	xmm2,XMMWORD[((208-128))+rax]

	movdqa	xmm8,xmm11
	movdqa	xmm6,xmm14
	pxor	xmm0,XMMWORD[((48-128))+rax]
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
	movdqa	xmm3,XMMWORD[((224-128))+rax]

	movdqa	xmm8,xmm10
	movdqa	xmm6,xmm13
	pxor	xmm1,XMMWORD[((64-128))+rax]
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
	movdqa	xmm4,XMMWORD[((240-128))+rax]

	movdqa	xmm8,xmm14
	movdqa	xmm6,xmm12
	pxor	xmm2,XMMWORD[((80-128))+rax]
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
	movdqa	xmm0,XMMWORD[((0-128))+rax]

	movdqa	xmm8,xmm13
	movdqa	xmm6,xmm11
	pxor	xmm3,XMMWORD[((96-128))+rax]
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
	movdqa	xmm1,XMMWORD[((16-128))+rax]

	movdqa	xmm8,xmm12
	movdqa	xmm6,xmm10
	pxor	xmm4,XMMWORD[((112-128))+rax]
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
	movdqa	xmm0,XMMWORD[rbx]
	mov	ecx,1
	cmp	ecx,DWORD[rbx]
	pxor	xmm8,xmm8
	cmovge	r8,rbp
	cmp	ecx,DWORD[4+rbx]
	movdqa	xmm1,xmm0
	cmovge	r9,rbp
	cmp	ecx,DWORD[8+rbx]
	pcmpgtd	xmm1,xmm8
	cmovge	r10,rbp
	cmp	ecx,DWORD[12+rbx]
	paddd	xmm0,xmm1
	cmovge	r11,rbp

	movdqu	xmm6,XMMWORD[rdi]
	pand	xmm10,xmm1
	movdqu	xmm7,XMMWORD[32+rdi]
	pand	xmm11,xmm1
	paddd	xmm10,xmm6
	movdqu	xmm8,XMMWORD[64+rdi]
	pand	xmm12,xmm1
	paddd	xmm11,xmm7
	movdqu	xmm9,XMMWORD[96+rdi]
	pand	xmm13,xmm1
	paddd	xmm12,xmm8
	movdqu	xmm5,XMMWORD[128+rdi]
	pand	xmm14,xmm1
	movdqu	XMMWORD[rdi],xmm10
	paddd	xmm13,xmm9
	movdqu	XMMWORD[32+rdi],xmm11
	paddd	xmm14,xmm5
	movdqu	XMMWORD[64+rdi],xmm12
	movdqu	XMMWORD[96+rdi],xmm13
	movdqu	XMMWORD[128+rdi],xmm14

	movdqa	XMMWORD[rbx],xmm0
	movdqa	xmm5,XMMWORD[96+rbp]
	movdqa	xmm15,XMMWORD[((-32))+rbp]
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

$L$SEH_end_sha1_multi_block:

ALIGN	32
sha1_multi_block_shaext:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block_shaext:
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
	lea	rdi,[64+rdi]
	mov	QWORD[272+rsp],rax
$L$body_shaext:
	lea	rbx,[256+rsp]
	movdqa	xmm3,XMMWORD[((K_XX_XX+128))]

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

	movq	xmm0,QWORD[((0-64))+rdi]
	movq	xmm4,QWORD[((32-64))+rdi]
	movq	xmm5,QWORD[((64-64))+rdi]
	movq	xmm6,QWORD[((96-64))+rdi]
	movq	xmm7,QWORD[((128-64))+rdi]

	punpckldq	xmm0,xmm4
	punpckldq	xmm5,xmm6

	movdqa	xmm8,xmm0
	punpcklqdq	xmm0,xmm5
	punpckhqdq	xmm8,xmm5

	pshufd	xmm1,xmm7,63
	pshufd	xmm9,xmm7,127
	pshufd	xmm0,xmm0,27
	pshufd	xmm8,xmm8,27
	jmp	NEAR $L$oop_shaext

ALIGN	32
$L$oop_shaext:
	movdqu	xmm4,XMMWORD[r8]
	movdqu	xmm11,XMMWORD[r9]
	movdqu	xmm5,XMMWORD[16+r8]
	movdqu	xmm12,XMMWORD[16+r9]
	movdqu	xmm6,XMMWORD[32+r8]
DB	102,15,56,0,227
	movdqu	xmm13,XMMWORD[32+r9]
DB	102,68,15,56,0,219
	movdqu	xmm7,XMMWORD[48+r8]
	lea	r8,[64+r8]
DB	102,15,56,0,235
	movdqu	xmm14,XMMWORD[48+r9]
	lea	r9,[64+r9]
DB	102,68,15,56,0,227

	movdqa	XMMWORD[80+rsp],xmm1
	paddd	xmm1,xmm4
	movdqa	XMMWORD[112+rsp],xmm9
	paddd	xmm9,xmm11
	movdqa	XMMWORD[64+rsp],xmm0
	movdqa	xmm2,xmm0
	movdqa	XMMWORD[96+rsp],xmm8
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
	cmp	ecx,DWORD[rbx]
	cmovge	r8,rsp

	movdqa	xmm1,xmm0
	movdqa	xmm9,xmm8
DB	15,58,204,194,3
DB	15,56,200,206
DB	69,15,58,204,194,3
DB	69,15,56,200,205
DB	15,56,202,254
DB	69,15,56,202,245

	cmp	ecx,DWORD[4+rbx]
	cmovge	r9,rsp
	movq	xmm6,QWORD[rbx]

	movdqa	xmm2,xmm0
	movdqa	xmm10,xmm8
DB	15,58,204,193,3
DB	15,56,200,215
DB	69,15,58,204,193,3
DB	69,15,56,200,214

	pshufd	xmm11,xmm6,0x00
	pshufd	xmm12,xmm6,0x55
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

	paddd	xmm0,XMMWORD[64+rsp]
	paddd	xmm1,XMMWORD[80+rsp]
	paddd	xmm8,XMMWORD[96+rsp]
	paddd	xmm9,XMMWORD[112+rsp]

	movq	QWORD[rbx],xmm6
	dec	edx
	jnz	NEAR $L$oop_shaext

	mov	edx,DWORD[280+rsp]

	pshufd	xmm0,xmm0,27
	pshufd	xmm8,xmm8,27

	movdqa	xmm6,xmm0
	punpckldq	xmm0,xmm8
	punpckhdq	xmm6,xmm8
	punpckhdq	xmm1,xmm9
	movq	QWORD[(0-64)+rdi],xmm0
	psrldq	xmm0,8
	movq	QWORD[(64-64)+rdi],xmm6
	psrldq	xmm6,8
	movq	QWORD[(32-64)+rdi],xmm0
	psrldq	xmm1,8
	movq	QWORD[(96-64)+rdi],xmm6
	movq	QWORD[(128-64)+rdi],xmm1

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

$L$SEH_end_sha1_multi_block_shaext:

ALIGN	32
sha1_multi_block_avx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block_avx:
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
	lea	rbp,[K_XX_XX]
	lea	rbx,[256+rsp]

	vzeroupper
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

	vmovdqu	xmm10,XMMWORD[rdi]
	lea	rax,[128+rsp]
	vmovdqu	xmm11,XMMWORD[32+rdi]
	vmovdqu	xmm12,XMMWORD[64+rdi]
	vmovdqu	xmm13,XMMWORD[96+rdi]
	vmovdqu	xmm14,XMMWORD[128+rdi]
	vmovdqu	xmm5,XMMWORD[96+rbp]
	jmp	NEAR $L$oop_avx

ALIGN	32
$L$oop_avx:
	vmovdqa	xmm15,XMMWORD[((-32))+rbp]
	vmovd	xmm0,DWORD[r8]
	lea	r8,[64+r8]
	vmovd	xmm2,DWORD[r9]
	lea	r9,[64+r9]
	vpinsrd	xmm0,xmm0,DWORD[r10],1
	lea	r10,[64+r10]
	vpinsrd	xmm2,xmm2,DWORD[r11],1
	lea	r11,[64+r11]
	vmovd	xmm1,DWORD[((-60))+r8]
	vpunpckldq	xmm0,xmm0,xmm2
	vmovd	xmm9,DWORD[((-60))+r9]
	vpshufb	xmm0,xmm0,xmm5
	vpinsrd	xmm1,xmm1,DWORD[((-60))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-60))+r11],1
	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpandn	xmm7,xmm11,xmm13
	vpand	xmm6,xmm11,xmm12

	vmovdqa	XMMWORD[(0-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpunpckldq	xmm1,xmm1,xmm9
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm2,DWORD[((-56))+r8]

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-56))+r9]
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpshufb	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpinsrd	xmm2,xmm2,DWORD[((-56))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-56))+r11],1
	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpandn	xmm7,xmm10,xmm12
	vpand	xmm6,xmm10,xmm11

	vmovdqa	XMMWORD[(16-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpunpckldq	xmm2,xmm2,xmm9
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm3,DWORD[((-52))+r8]

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-52))+r9]
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpshufb	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpinsrd	xmm3,xmm3,DWORD[((-52))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-52))+r11],1
	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpandn	xmm7,xmm14,xmm11
	vpand	xmm6,xmm14,xmm10

	vmovdqa	XMMWORD[(32-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpunpckldq	xmm3,xmm3,xmm9
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm4,DWORD[((-48))+r8]

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-48))+r9]
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpshufb	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpinsrd	xmm4,xmm4,DWORD[((-48))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-48))+r11],1
	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpandn	xmm7,xmm13,xmm10
	vpand	xmm6,xmm13,xmm14

	vmovdqa	XMMWORD[(48-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpunpckldq	xmm4,xmm4,xmm9
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm0,DWORD[((-44))+r8]

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-44))+r9]
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpshufb	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpinsrd	xmm0,xmm0,DWORD[((-44))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-44))+r11],1
	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpandn	xmm7,xmm12,xmm14
	vpand	xmm6,xmm12,xmm13

	vmovdqa	XMMWORD[(64-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpunpckldq	xmm0,xmm0,xmm9
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm1,DWORD[((-40))+r8]

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-40))+r9]
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpshufb	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpinsrd	xmm1,xmm1,DWORD[((-40))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-40))+r11],1
	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpandn	xmm7,xmm11,xmm13
	vpand	xmm6,xmm11,xmm12

	vmovdqa	XMMWORD[(80-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpunpckldq	xmm1,xmm1,xmm9
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm2,DWORD[((-36))+r8]

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-36))+r9]
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpshufb	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpinsrd	xmm2,xmm2,DWORD[((-36))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-36))+r11],1
	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpandn	xmm7,xmm10,xmm12
	vpand	xmm6,xmm10,xmm11

	vmovdqa	XMMWORD[(96-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpunpckldq	xmm2,xmm2,xmm9
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm3,DWORD[((-32))+r8]

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-32))+r9]
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpshufb	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpinsrd	xmm3,xmm3,DWORD[((-32))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-32))+r11],1
	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpandn	xmm7,xmm14,xmm11
	vpand	xmm6,xmm14,xmm10

	vmovdqa	XMMWORD[(112-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpunpckldq	xmm3,xmm3,xmm9
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm4,DWORD[((-28))+r8]

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-28))+r9]
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpshufb	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpinsrd	xmm4,xmm4,DWORD[((-28))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-28))+r11],1
	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpandn	xmm7,xmm13,xmm10
	vpand	xmm6,xmm13,xmm14

	vmovdqa	XMMWORD[(128-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpunpckldq	xmm4,xmm4,xmm9
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm0,DWORD[((-24))+r8]

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-24))+r9]
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpshufb	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpinsrd	xmm0,xmm0,DWORD[((-24))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-24))+r11],1
	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpandn	xmm7,xmm12,xmm14
	vpand	xmm6,xmm12,xmm13

	vmovdqa	XMMWORD[(144-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpunpckldq	xmm0,xmm0,xmm9
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm1,DWORD[((-20))+r8]

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-20))+r9]
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpshufb	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpinsrd	xmm1,xmm1,DWORD[((-20))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-20))+r11],1
	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpandn	xmm7,xmm11,xmm13
	vpand	xmm6,xmm11,xmm12

	vmovdqa	XMMWORD[(160-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpunpckldq	xmm1,xmm1,xmm9
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm2,DWORD[((-16))+r8]

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-16))+r9]
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpshufb	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpinsrd	xmm2,xmm2,DWORD[((-16))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-16))+r11],1
	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpandn	xmm7,xmm10,xmm12
	vpand	xmm6,xmm10,xmm11

	vmovdqa	XMMWORD[(176-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpunpckldq	xmm2,xmm2,xmm9
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm3,DWORD[((-12))+r8]

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-12))+r9]
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpshufb	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpinsrd	xmm3,xmm3,DWORD[((-12))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-12))+r11],1
	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpandn	xmm7,xmm14,xmm11
	vpand	xmm6,xmm14,xmm10

	vmovdqa	XMMWORD[(192-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpunpckldq	xmm3,xmm3,xmm9
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm4,DWORD[((-8))+r8]

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-8))+r9]
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpshufb	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpinsrd	xmm4,xmm4,DWORD[((-8))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-8))+r11],1
	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpandn	xmm7,xmm13,xmm10
	vpand	xmm6,xmm13,xmm14

	vmovdqa	XMMWORD[(208-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpunpckldq	xmm4,xmm4,xmm9
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm7
	vmovd	xmm0,DWORD[((-4))+r8]

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vmovd	xmm9,DWORD[((-4))+r9]
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpshufb	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vmovdqa	xmm1,XMMWORD[((0-128))+rax]
	vpinsrd	xmm0,xmm0,DWORD[((-4))+r10],1
	vpinsrd	xmm9,xmm9,DWORD[((-4))+r11],1
	vpaddd	xmm10,xmm10,xmm15
	prefetcht0	[63+r8]
	vpslld	xmm8,xmm11,5
	vpandn	xmm7,xmm12,xmm14
	vpand	xmm6,xmm12,xmm13

	vmovdqa	XMMWORD[(224-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpunpckldq	xmm0,xmm0,xmm9
	vpsrld	xmm9,xmm11,27
	prefetcht0	[63+r9]
	vpxor	xmm6,xmm6,xmm7

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	prefetcht0	[63+r10]
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	prefetcht0	[63+r11]
	vpshufb	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vmovdqa	xmm2,XMMWORD[((16-128))+rax]
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((32-128))+rax]

	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpandn	xmm7,xmm11,xmm13

	vpand	xmm6,xmm11,xmm12

	vmovdqa	XMMWORD[(240-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((128-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm7
	vpxor	xmm1,xmm1,xmm3


	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2

	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((48-128))+rax]

	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpandn	xmm7,xmm10,xmm12

	vpand	xmm6,xmm10,xmm11

	vmovdqa	XMMWORD[(0-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((144-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm7
	vpxor	xmm2,xmm2,xmm4


	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2

	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((64-128))+rax]

	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpandn	xmm7,xmm14,xmm11

	vpand	xmm6,xmm14,xmm10

	vmovdqa	XMMWORD[(16-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((160-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm7
	vpxor	xmm3,xmm3,xmm0


	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2

	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((80-128))+rax]

	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpandn	xmm7,xmm13,xmm10

	vpand	xmm6,xmm13,xmm14

	vmovdqa	XMMWORD[(32-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((176-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm7
	vpxor	xmm4,xmm4,xmm1


	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2

	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((96-128))+rax]

	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpandn	xmm7,xmm12,xmm14

	vpand	xmm6,xmm12,xmm13

	vmovdqa	XMMWORD[(48-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((192-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm7
	vpxor	xmm0,xmm0,xmm2


	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2

	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vmovdqa	xmm15,XMMWORD[rbp]
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((112-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(64-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((208-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((128-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(80-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((224-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((144-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(96-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((240-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((160-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(112-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((0-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((176-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(128-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((16-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((192-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(144-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((32-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((208-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(160-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((48-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((224-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(176-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((64-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((240-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(192-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((80-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((0-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(208-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((96-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((16-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(224-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((112-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((32-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(240-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((128-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((48-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(0-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((144-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((64-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(16-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((160-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((80-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(32-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((176-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((96-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(48-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((192-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((112-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(64-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((208-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((128-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(80-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((224-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((144-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(96-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((240-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((160-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(112-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((0-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vmovdqa	xmm15,XMMWORD[32+rbp]
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((176-128))+rax]

	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpand	xmm7,xmm13,xmm12
	vpxor	xmm1,xmm1,XMMWORD[((16-128))+rax]

	vpaddd	xmm14,xmm14,xmm7
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm13,xmm12
	vpxor	xmm1,xmm1,xmm3

	vmovdqu	XMMWORD[(128-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm1,31
	vpand	xmm6,xmm6,xmm11
	vpaddd	xmm1,xmm1,xmm1

	vpslld	xmm7,xmm11,30
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((192-128))+rax]

	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpand	xmm7,xmm12,xmm11
	vpxor	xmm2,xmm2,XMMWORD[((32-128))+rax]

	vpaddd	xmm13,xmm13,xmm7
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm12,xmm11
	vpxor	xmm2,xmm2,xmm4

	vmovdqu	XMMWORD[(144-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm2,31
	vpand	xmm6,xmm6,xmm10
	vpaddd	xmm2,xmm2,xmm2

	vpslld	xmm7,xmm10,30
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((208-128))+rax]

	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpand	xmm7,xmm11,xmm10
	vpxor	xmm3,xmm3,XMMWORD[((48-128))+rax]

	vpaddd	xmm12,xmm12,xmm7
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm11,xmm10
	vpxor	xmm3,xmm3,xmm0

	vmovdqu	XMMWORD[(160-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm3,31
	vpand	xmm6,xmm6,xmm14
	vpaddd	xmm3,xmm3,xmm3

	vpslld	xmm7,xmm14,30
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((224-128))+rax]

	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpand	xmm7,xmm10,xmm14
	vpxor	xmm4,xmm4,XMMWORD[((64-128))+rax]

	vpaddd	xmm11,xmm11,xmm7
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm10,xmm14
	vpxor	xmm4,xmm4,xmm1

	vmovdqu	XMMWORD[(176-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm4,31
	vpand	xmm6,xmm6,xmm13
	vpaddd	xmm4,xmm4,xmm4

	vpslld	xmm7,xmm13,30
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((240-128))+rax]

	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpand	xmm7,xmm14,xmm13
	vpxor	xmm0,xmm0,XMMWORD[((80-128))+rax]

	vpaddd	xmm10,xmm10,xmm7
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm14,xmm13
	vpxor	xmm0,xmm0,xmm2

	vmovdqu	XMMWORD[(192-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm0,31
	vpand	xmm6,xmm6,xmm12
	vpaddd	xmm0,xmm0,xmm0

	vpslld	xmm7,xmm12,30
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((0-128))+rax]

	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpand	xmm7,xmm13,xmm12
	vpxor	xmm1,xmm1,XMMWORD[((96-128))+rax]

	vpaddd	xmm14,xmm14,xmm7
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm13,xmm12
	vpxor	xmm1,xmm1,xmm3

	vmovdqu	XMMWORD[(208-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm1,31
	vpand	xmm6,xmm6,xmm11
	vpaddd	xmm1,xmm1,xmm1

	vpslld	xmm7,xmm11,30
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((16-128))+rax]

	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpand	xmm7,xmm12,xmm11
	vpxor	xmm2,xmm2,XMMWORD[((112-128))+rax]

	vpaddd	xmm13,xmm13,xmm7
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm12,xmm11
	vpxor	xmm2,xmm2,xmm4

	vmovdqu	XMMWORD[(224-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm2,31
	vpand	xmm6,xmm6,xmm10
	vpaddd	xmm2,xmm2,xmm2

	vpslld	xmm7,xmm10,30
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((32-128))+rax]

	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpand	xmm7,xmm11,xmm10
	vpxor	xmm3,xmm3,XMMWORD[((128-128))+rax]

	vpaddd	xmm12,xmm12,xmm7
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm11,xmm10
	vpxor	xmm3,xmm3,xmm0

	vmovdqu	XMMWORD[(240-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm3,31
	vpand	xmm6,xmm6,xmm14
	vpaddd	xmm3,xmm3,xmm3

	vpslld	xmm7,xmm14,30
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((48-128))+rax]

	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpand	xmm7,xmm10,xmm14
	vpxor	xmm4,xmm4,XMMWORD[((144-128))+rax]

	vpaddd	xmm11,xmm11,xmm7
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm10,xmm14
	vpxor	xmm4,xmm4,xmm1

	vmovdqu	XMMWORD[(0-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm4,31
	vpand	xmm6,xmm6,xmm13
	vpaddd	xmm4,xmm4,xmm4

	vpslld	xmm7,xmm13,30
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((64-128))+rax]

	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpand	xmm7,xmm14,xmm13
	vpxor	xmm0,xmm0,XMMWORD[((160-128))+rax]

	vpaddd	xmm10,xmm10,xmm7
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm14,xmm13
	vpxor	xmm0,xmm0,xmm2

	vmovdqu	XMMWORD[(16-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm0,31
	vpand	xmm6,xmm6,xmm12
	vpaddd	xmm0,xmm0,xmm0

	vpslld	xmm7,xmm12,30
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((80-128))+rax]

	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpand	xmm7,xmm13,xmm12
	vpxor	xmm1,xmm1,XMMWORD[((176-128))+rax]

	vpaddd	xmm14,xmm14,xmm7
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm13,xmm12
	vpxor	xmm1,xmm1,xmm3

	vmovdqu	XMMWORD[(32-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm1,31
	vpand	xmm6,xmm6,xmm11
	vpaddd	xmm1,xmm1,xmm1

	vpslld	xmm7,xmm11,30
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((96-128))+rax]

	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpand	xmm7,xmm12,xmm11
	vpxor	xmm2,xmm2,XMMWORD[((192-128))+rax]

	vpaddd	xmm13,xmm13,xmm7
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm12,xmm11
	vpxor	xmm2,xmm2,xmm4

	vmovdqu	XMMWORD[(48-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm2,31
	vpand	xmm6,xmm6,xmm10
	vpaddd	xmm2,xmm2,xmm2

	vpslld	xmm7,xmm10,30
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((112-128))+rax]

	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpand	xmm7,xmm11,xmm10
	vpxor	xmm3,xmm3,XMMWORD[((208-128))+rax]

	vpaddd	xmm12,xmm12,xmm7
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm11,xmm10
	vpxor	xmm3,xmm3,xmm0

	vmovdqu	XMMWORD[(64-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm3,31
	vpand	xmm6,xmm6,xmm14
	vpaddd	xmm3,xmm3,xmm3

	vpslld	xmm7,xmm14,30
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((128-128))+rax]

	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpand	xmm7,xmm10,xmm14
	vpxor	xmm4,xmm4,XMMWORD[((224-128))+rax]

	vpaddd	xmm11,xmm11,xmm7
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm10,xmm14
	vpxor	xmm4,xmm4,xmm1

	vmovdqu	XMMWORD[(80-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm4,31
	vpand	xmm6,xmm6,xmm13
	vpaddd	xmm4,xmm4,xmm4

	vpslld	xmm7,xmm13,30
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((144-128))+rax]

	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpand	xmm7,xmm14,xmm13
	vpxor	xmm0,xmm0,XMMWORD[((240-128))+rax]

	vpaddd	xmm10,xmm10,xmm7
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm14,xmm13
	vpxor	xmm0,xmm0,xmm2

	vmovdqu	XMMWORD[(96-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm0,31
	vpand	xmm6,xmm6,xmm12
	vpaddd	xmm0,xmm0,xmm0

	vpslld	xmm7,xmm12,30
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((160-128))+rax]

	vpaddd	xmm14,xmm14,xmm15
	vpslld	xmm8,xmm10,5
	vpand	xmm7,xmm13,xmm12
	vpxor	xmm1,xmm1,XMMWORD[((0-128))+rax]

	vpaddd	xmm14,xmm14,xmm7
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm13,xmm12
	vpxor	xmm1,xmm1,xmm3

	vmovdqu	XMMWORD[(112-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm1,31
	vpand	xmm6,xmm6,xmm11
	vpaddd	xmm1,xmm1,xmm1

	vpslld	xmm7,xmm11,30
	vpaddd	xmm14,xmm14,xmm6

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((176-128))+rax]

	vpaddd	xmm13,xmm13,xmm15
	vpslld	xmm8,xmm14,5
	vpand	xmm7,xmm12,xmm11
	vpxor	xmm2,xmm2,XMMWORD[((16-128))+rax]

	vpaddd	xmm13,xmm13,xmm7
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm12,xmm11
	vpxor	xmm2,xmm2,xmm4

	vmovdqu	XMMWORD[(128-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm2,31
	vpand	xmm6,xmm6,xmm10
	vpaddd	xmm2,xmm2,xmm2

	vpslld	xmm7,xmm10,30
	vpaddd	xmm13,xmm13,xmm6

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((192-128))+rax]

	vpaddd	xmm12,xmm12,xmm15
	vpslld	xmm8,xmm13,5
	vpand	xmm7,xmm11,xmm10
	vpxor	xmm3,xmm3,XMMWORD[((32-128))+rax]

	vpaddd	xmm12,xmm12,xmm7
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm11,xmm10
	vpxor	xmm3,xmm3,xmm0

	vmovdqu	XMMWORD[(144-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm3,31
	vpand	xmm6,xmm6,xmm14
	vpaddd	xmm3,xmm3,xmm3

	vpslld	xmm7,xmm14,30
	vpaddd	xmm12,xmm12,xmm6

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((208-128))+rax]

	vpaddd	xmm11,xmm11,xmm15
	vpslld	xmm8,xmm12,5
	vpand	xmm7,xmm10,xmm14
	vpxor	xmm4,xmm4,XMMWORD[((48-128))+rax]

	vpaddd	xmm11,xmm11,xmm7
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm10,xmm14
	vpxor	xmm4,xmm4,xmm1

	vmovdqu	XMMWORD[(160-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm4,31
	vpand	xmm6,xmm6,xmm13
	vpaddd	xmm4,xmm4,xmm4

	vpslld	xmm7,xmm13,30
	vpaddd	xmm11,xmm11,xmm6

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((224-128))+rax]

	vpaddd	xmm10,xmm10,xmm15
	vpslld	xmm8,xmm11,5
	vpand	xmm7,xmm14,xmm13
	vpxor	xmm0,xmm0,XMMWORD[((64-128))+rax]

	vpaddd	xmm10,xmm10,xmm7
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm14,xmm13
	vpxor	xmm0,xmm0,xmm2

	vmovdqu	XMMWORD[(176-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpor	xmm8,xmm8,xmm9
	vpsrld	xmm5,xmm0,31
	vpand	xmm6,xmm6,xmm12
	vpaddd	xmm0,xmm0,xmm0

	vpslld	xmm7,xmm12,30
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vmovdqa	xmm15,XMMWORD[64+rbp]
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((240-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(192-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((80-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((0-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(208-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((96-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((16-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(224-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((112-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((32-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(240-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((128-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((48-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(0-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((144-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((64-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(16-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((160-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((80-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(32-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((176-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((96-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vmovdqa	XMMWORD[(48-128)+rax],xmm2
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((192-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((112-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vmovdqa	XMMWORD[(64-128)+rax],xmm3
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((208-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((128-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vmovdqa	XMMWORD[(80-128)+rax],xmm4
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((224-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((144-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vmovdqa	XMMWORD[(96-128)+rax],xmm0
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((240-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((160-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vmovdqa	XMMWORD[(112-128)+rax],xmm1
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((0-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((176-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((16-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((192-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((32-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpxor	xmm0,xmm0,xmm2
	vmovdqa	xmm2,XMMWORD[((208-128))+rax]

	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm0,xmm0,XMMWORD[((48-128))+rax]
	vpsrld	xmm9,xmm11,27
	vpxor	xmm6,xmm6,xmm13
	vpxor	xmm0,xmm0,xmm2

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6
	vpsrld	xmm5,xmm0,31
	vpaddd	xmm0,xmm0,xmm0

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm0,xmm0,xmm5
	vpor	xmm12,xmm12,xmm7
	vpxor	xmm1,xmm1,xmm3
	vmovdqa	xmm3,XMMWORD[((224-128))+rax]

	vpslld	xmm8,xmm10,5
	vpaddd	xmm14,xmm14,xmm15
	vpxor	xmm6,xmm13,xmm11
	vpaddd	xmm14,xmm14,xmm0
	vpxor	xmm1,xmm1,XMMWORD[((64-128))+rax]
	vpsrld	xmm9,xmm10,27
	vpxor	xmm6,xmm6,xmm12
	vpxor	xmm1,xmm1,xmm3

	vpslld	xmm7,xmm11,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm14,xmm14,xmm6
	vpsrld	xmm5,xmm1,31
	vpaddd	xmm1,xmm1,xmm1

	vpsrld	xmm11,xmm11,2
	vpaddd	xmm14,xmm14,xmm8
	vpor	xmm1,xmm1,xmm5
	vpor	xmm11,xmm11,xmm7
	vpxor	xmm2,xmm2,xmm4
	vmovdqa	xmm4,XMMWORD[((240-128))+rax]

	vpslld	xmm8,xmm14,5
	vpaddd	xmm13,xmm13,xmm15
	vpxor	xmm6,xmm12,xmm10
	vpaddd	xmm13,xmm13,xmm1
	vpxor	xmm2,xmm2,XMMWORD[((80-128))+rax]
	vpsrld	xmm9,xmm14,27
	vpxor	xmm6,xmm6,xmm11
	vpxor	xmm2,xmm2,xmm4

	vpslld	xmm7,xmm10,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm13,xmm13,xmm6
	vpsrld	xmm5,xmm2,31
	vpaddd	xmm2,xmm2,xmm2

	vpsrld	xmm10,xmm10,2
	vpaddd	xmm13,xmm13,xmm8
	vpor	xmm2,xmm2,xmm5
	vpor	xmm10,xmm10,xmm7
	vpxor	xmm3,xmm3,xmm0
	vmovdqa	xmm0,XMMWORD[((0-128))+rax]

	vpslld	xmm8,xmm13,5
	vpaddd	xmm12,xmm12,xmm15
	vpxor	xmm6,xmm11,xmm14
	vpaddd	xmm12,xmm12,xmm2
	vpxor	xmm3,xmm3,XMMWORD[((96-128))+rax]
	vpsrld	xmm9,xmm13,27
	vpxor	xmm6,xmm6,xmm10
	vpxor	xmm3,xmm3,xmm0

	vpslld	xmm7,xmm14,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm12,xmm12,xmm6
	vpsrld	xmm5,xmm3,31
	vpaddd	xmm3,xmm3,xmm3

	vpsrld	xmm14,xmm14,2
	vpaddd	xmm12,xmm12,xmm8
	vpor	xmm3,xmm3,xmm5
	vpor	xmm14,xmm14,xmm7
	vpxor	xmm4,xmm4,xmm1
	vmovdqa	xmm1,XMMWORD[((16-128))+rax]

	vpslld	xmm8,xmm12,5
	vpaddd	xmm11,xmm11,xmm15
	vpxor	xmm6,xmm10,xmm13
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm4,XMMWORD[((112-128))+rax]
	vpsrld	xmm9,xmm12,27
	vpxor	xmm6,xmm6,xmm14
	vpxor	xmm4,xmm4,xmm1

	vpslld	xmm7,xmm13,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm11,xmm11,xmm6
	vpsrld	xmm5,xmm4,31
	vpaddd	xmm4,xmm4,xmm4

	vpsrld	xmm13,xmm13,2
	vpaddd	xmm11,xmm11,xmm8
	vpor	xmm4,xmm4,xmm5
	vpor	xmm13,xmm13,xmm7
	vpslld	xmm8,xmm11,5
	vpaddd	xmm10,xmm10,xmm15
	vpxor	xmm6,xmm14,xmm12

	vpsrld	xmm9,xmm11,27
	vpaddd	xmm10,xmm10,xmm4
	vpxor	xmm6,xmm6,xmm13

	vpslld	xmm7,xmm12,30
	vpor	xmm8,xmm8,xmm9
	vpaddd	xmm10,xmm10,xmm6

	vpsrld	xmm12,xmm12,2
	vpaddd	xmm10,xmm10,xmm8
	vpor	xmm12,xmm12,xmm7
	mov	ecx,1
	cmp	ecx,DWORD[rbx]
	cmovge	r8,rbp
	cmp	ecx,DWORD[4+rbx]
	cmovge	r9,rbp
	cmp	ecx,DWORD[8+rbx]
	cmovge	r10,rbp
	cmp	ecx,DWORD[12+rbx]
	cmovge	r11,rbp
	vmovdqu	xmm6,XMMWORD[rbx]
	vpxor	xmm8,xmm8,xmm8
	vmovdqa	xmm7,xmm6
	vpcmpgtd	xmm7,xmm7,xmm8
	vpaddd	xmm6,xmm6,xmm7

	vpand	xmm10,xmm10,xmm7
	vpand	xmm11,xmm11,xmm7
	vpaddd	xmm10,xmm10,XMMWORD[rdi]
	vpand	xmm12,xmm12,xmm7
	vpaddd	xmm11,xmm11,XMMWORD[32+rdi]
	vpand	xmm13,xmm13,xmm7
	vpaddd	xmm12,xmm12,XMMWORD[64+rdi]
	vpand	xmm14,xmm14,xmm7
	vpaddd	xmm13,xmm13,XMMWORD[96+rdi]
	vpaddd	xmm14,xmm14,XMMWORD[128+rdi]
	vmovdqu	XMMWORD[rdi],xmm10
	vmovdqu	XMMWORD[32+rdi],xmm11
	vmovdqu	XMMWORD[64+rdi],xmm12
	vmovdqu	XMMWORD[96+rdi],xmm13
	vmovdqu	XMMWORD[128+rdi],xmm14

	vmovdqu	XMMWORD[rbx],xmm6
	vmovdqu	xmm5,XMMWORD[96+rbp]
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

$L$SEH_end_sha1_multi_block_avx:

ALIGN	32
sha1_multi_block_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_sha1_multi_block_avx2:
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
	lea	rbp,[K_XX_XX]
	shr	edx,1

	vzeroupper
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
	vmovdqu	ymm0,YMMWORD[rdi]
	lea	rax,[128+rsp]
	vmovdqu	ymm1,YMMWORD[32+rdi]
	lea	rbx,[((256+128))+rsp]
	vmovdqu	ymm2,YMMWORD[64+rdi]
	vmovdqu	ymm3,YMMWORD[96+rdi]
	vmovdqu	ymm4,YMMWORD[128+rdi]
	vmovdqu	ymm9,YMMWORD[96+rbp]
	jmp	NEAR $L$oop_avx2

ALIGN	32
$L$oop_avx2:
	vmovdqa	ymm15,YMMWORD[((-32))+rbp]
	vmovd	xmm10,DWORD[r12]
	lea	r12,[64+r12]
	vmovd	xmm12,DWORD[r8]
	lea	r8,[64+r8]
	vmovd	xmm7,DWORD[r13]
	lea	r13,[64+r13]
	vmovd	xmm6,DWORD[r9]
	lea	r9,[64+r9]
	vpinsrd	xmm10,xmm10,DWORD[r14],1
	lea	r14,[64+r14]
	vpinsrd	xmm12,xmm12,DWORD[r10],1
	lea	r10,[64+r10]
	vpinsrd	xmm7,xmm7,DWORD[r15],1
	lea	r15,[64+r15]
	vpunpckldq	ymm10,ymm10,ymm7
	vpinsrd	xmm6,xmm6,DWORD[r11],1
	lea	r11,[64+r11]
	vpunpckldq	ymm12,ymm12,ymm6
	vmovd	xmm11,DWORD[((-60))+r12]
	vinserti128	ymm10,ymm10,xmm12,1
	vmovd	xmm8,DWORD[((-60))+r8]
	vpshufb	ymm10,ymm10,ymm9
	vmovd	xmm7,DWORD[((-60))+r13]
	vmovd	xmm6,DWORD[((-60))+r9]
	vpinsrd	xmm11,xmm11,DWORD[((-60))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-60))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-60))+r15],1
	vpunpckldq	ymm11,ymm11,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-60))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpandn	ymm6,ymm1,ymm3
	vpand	ymm5,ymm1,ymm2

	vmovdqa	YMMWORD[(0-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vinserti128	ymm11,ymm11,xmm8,1
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm12,DWORD[((-56))+r12]

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-56))+r8]
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpshufb	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vmovd	xmm7,DWORD[((-56))+r13]
	vmovd	xmm6,DWORD[((-56))+r9]
	vpinsrd	xmm12,xmm12,DWORD[((-56))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-56))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-56))+r15],1
	vpunpckldq	ymm12,ymm12,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-56))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpandn	ymm6,ymm0,ymm2
	vpand	ymm5,ymm0,ymm1

	vmovdqa	YMMWORD[(32-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vinserti128	ymm12,ymm12,xmm8,1
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm13,DWORD[((-52))+r12]

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-52))+r8]
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpshufb	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vmovd	xmm7,DWORD[((-52))+r13]
	vmovd	xmm6,DWORD[((-52))+r9]
	vpinsrd	xmm13,xmm13,DWORD[((-52))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-52))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-52))+r15],1
	vpunpckldq	ymm13,ymm13,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-52))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpandn	ymm6,ymm4,ymm1
	vpand	ymm5,ymm4,ymm0

	vmovdqa	YMMWORD[(64-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vinserti128	ymm13,ymm13,xmm8,1
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm14,DWORD[((-48))+r12]

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-48))+r8]
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpshufb	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vmovd	xmm7,DWORD[((-48))+r13]
	vmovd	xmm6,DWORD[((-48))+r9]
	vpinsrd	xmm14,xmm14,DWORD[((-48))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-48))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-48))+r15],1
	vpunpckldq	ymm14,ymm14,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-48))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpandn	ymm6,ymm3,ymm0
	vpand	ymm5,ymm3,ymm4

	vmovdqa	YMMWORD[(96-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vinserti128	ymm14,ymm14,xmm8,1
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm10,DWORD[((-44))+r12]

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-44))+r8]
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpshufb	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vmovd	xmm7,DWORD[((-44))+r13]
	vmovd	xmm6,DWORD[((-44))+r9]
	vpinsrd	xmm10,xmm10,DWORD[((-44))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-44))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-44))+r15],1
	vpunpckldq	ymm10,ymm10,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-44))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpandn	ymm6,ymm2,ymm4
	vpand	ymm5,ymm2,ymm3

	vmovdqa	YMMWORD[(128-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vinserti128	ymm10,ymm10,xmm8,1
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm11,DWORD[((-40))+r12]

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-40))+r8]
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpshufb	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovd	xmm7,DWORD[((-40))+r13]
	vmovd	xmm6,DWORD[((-40))+r9]
	vpinsrd	xmm11,xmm11,DWORD[((-40))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-40))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-40))+r15],1
	vpunpckldq	ymm11,ymm11,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-40))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpandn	ymm6,ymm1,ymm3
	vpand	ymm5,ymm1,ymm2

	vmovdqa	YMMWORD[(160-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vinserti128	ymm11,ymm11,xmm8,1
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm12,DWORD[((-36))+r12]

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-36))+r8]
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpshufb	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vmovd	xmm7,DWORD[((-36))+r13]
	vmovd	xmm6,DWORD[((-36))+r9]
	vpinsrd	xmm12,xmm12,DWORD[((-36))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-36))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-36))+r15],1
	vpunpckldq	ymm12,ymm12,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-36))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpandn	ymm6,ymm0,ymm2
	vpand	ymm5,ymm0,ymm1

	vmovdqa	YMMWORD[(192-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vinserti128	ymm12,ymm12,xmm8,1
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm13,DWORD[((-32))+r12]

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-32))+r8]
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpshufb	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vmovd	xmm7,DWORD[((-32))+r13]
	vmovd	xmm6,DWORD[((-32))+r9]
	vpinsrd	xmm13,xmm13,DWORD[((-32))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-32))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-32))+r15],1
	vpunpckldq	ymm13,ymm13,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-32))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpandn	ymm6,ymm4,ymm1
	vpand	ymm5,ymm4,ymm0

	vmovdqa	YMMWORD[(224-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vinserti128	ymm13,ymm13,xmm8,1
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm14,DWORD[((-28))+r12]

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-28))+r8]
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpshufb	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vmovd	xmm7,DWORD[((-28))+r13]
	vmovd	xmm6,DWORD[((-28))+r9]
	vpinsrd	xmm14,xmm14,DWORD[((-28))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-28))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-28))+r15],1
	vpunpckldq	ymm14,ymm14,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-28))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpandn	ymm6,ymm3,ymm0
	vpand	ymm5,ymm3,ymm4

	vmovdqa	YMMWORD[(256-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vinserti128	ymm14,ymm14,xmm8,1
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm10,DWORD[((-24))+r12]

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-24))+r8]
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpshufb	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vmovd	xmm7,DWORD[((-24))+r13]
	vmovd	xmm6,DWORD[((-24))+r9]
	vpinsrd	xmm10,xmm10,DWORD[((-24))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-24))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-24))+r15],1
	vpunpckldq	ymm10,ymm10,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-24))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpandn	ymm6,ymm2,ymm4
	vpand	ymm5,ymm2,ymm3

	vmovdqa	YMMWORD[(288-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vinserti128	ymm10,ymm10,xmm8,1
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm11,DWORD[((-20))+r12]

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-20))+r8]
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpshufb	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovd	xmm7,DWORD[((-20))+r13]
	vmovd	xmm6,DWORD[((-20))+r9]
	vpinsrd	xmm11,xmm11,DWORD[((-20))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-20))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-20))+r15],1
	vpunpckldq	ymm11,ymm11,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-20))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpandn	ymm6,ymm1,ymm3
	vpand	ymm5,ymm1,ymm2

	vmovdqa	YMMWORD[(320-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vinserti128	ymm11,ymm11,xmm8,1
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm12,DWORD[((-16))+r12]

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-16))+r8]
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpshufb	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vmovd	xmm7,DWORD[((-16))+r13]
	vmovd	xmm6,DWORD[((-16))+r9]
	vpinsrd	xmm12,xmm12,DWORD[((-16))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-16))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-16))+r15],1
	vpunpckldq	ymm12,ymm12,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-16))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpandn	ymm6,ymm0,ymm2
	vpand	ymm5,ymm0,ymm1

	vmovdqa	YMMWORD[(352-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vinserti128	ymm12,ymm12,xmm8,1
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm13,DWORD[((-12))+r12]

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-12))+r8]
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpshufb	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vmovd	xmm7,DWORD[((-12))+r13]
	vmovd	xmm6,DWORD[((-12))+r9]
	vpinsrd	xmm13,xmm13,DWORD[((-12))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-12))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-12))+r15],1
	vpunpckldq	ymm13,ymm13,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-12))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpandn	ymm6,ymm4,ymm1
	vpand	ymm5,ymm4,ymm0

	vmovdqa	YMMWORD[(384-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vinserti128	ymm13,ymm13,xmm8,1
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm14,DWORD[((-8))+r12]

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-8))+r8]
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpshufb	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vmovd	xmm7,DWORD[((-8))+r13]
	vmovd	xmm6,DWORD[((-8))+r9]
	vpinsrd	xmm14,xmm14,DWORD[((-8))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-8))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-8))+r15],1
	vpunpckldq	ymm14,ymm14,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-8))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpandn	ymm6,ymm3,ymm0
	vpand	ymm5,ymm3,ymm4

	vmovdqa	YMMWORD[(416-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vinserti128	ymm14,ymm14,xmm8,1
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm6
	vmovd	xmm10,DWORD[((-4))+r12]

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vmovd	xmm8,DWORD[((-4))+r8]
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpshufb	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vmovdqa	ymm11,YMMWORD[((0-128))+rax]
	vmovd	xmm7,DWORD[((-4))+r13]
	vmovd	xmm6,DWORD[((-4))+r9]
	vpinsrd	xmm10,xmm10,DWORD[((-4))+r14],1
	vpinsrd	xmm8,xmm8,DWORD[((-4))+r10],1
	vpinsrd	xmm7,xmm7,DWORD[((-4))+r15],1
	vpunpckldq	ymm10,ymm10,ymm7
	vpinsrd	xmm6,xmm6,DWORD[((-4))+r11],1
	vpunpckldq	ymm8,ymm8,ymm6
	vpaddd	ymm0,ymm0,ymm15
	prefetcht0	[63+r12]
	vpslld	ymm7,ymm1,5
	vpandn	ymm6,ymm2,ymm4
	vpand	ymm5,ymm2,ymm3

	vmovdqa	YMMWORD[(448-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vinserti128	ymm10,ymm10,xmm8,1
	vpsrld	ymm8,ymm1,27
	prefetcht0	[63+r13]
	vpxor	ymm5,ymm5,ymm6

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	prefetcht0	[63+r14]
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	prefetcht0	[63+r15]
	vpshufb	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovdqa	ymm12,YMMWORD[((32-128))+rax]
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((64-128))+rax]

	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpandn	ymm6,ymm1,ymm3
	prefetcht0	[63+r8]
	vpand	ymm5,ymm1,ymm2

	vmovdqa	YMMWORD[(480-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((256-256-128))+rbx]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm6
	vpxor	ymm11,ymm11,ymm13
	prefetcht0	[63+r9]

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	prefetcht0	[63+r10]
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	prefetcht0	[63+r11]
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((96-128))+rax]

	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpandn	ymm6,ymm0,ymm2

	vpand	ymm5,ymm0,ymm1

	vmovdqa	YMMWORD[(0-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((288-256-128))+rbx]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm6
	vpxor	ymm12,ymm12,ymm14


	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2

	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((128-128))+rax]

	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpandn	ymm6,ymm4,ymm1

	vpand	ymm5,ymm4,ymm0

	vmovdqa	YMMWORD[(32-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((320-256-128))+rbx]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm6
	vpxor	ymm13,ymm13,ymm10


	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2

	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((160-128))+rax]

	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpandn	ymm6,ymm3,ymm0

	vpand	ymm5,ymm3,ymm4

	vmovdqa	YMMWORD[(64-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((352-256-128))+rbx]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm6
	vpxor	ymm14,ymm14,ymm11


	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2

	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((192-128))+rax]

	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpandn	ymm6,ymm2,ymm4

	vpand	ymm5,ymm2,ymm3

	vmovdqa	YMMWORD[(96-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((384-256-128))+rbx]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm6
	vpxor	ymm10,ymm10,ymm12


	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2

	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovdqa	ymm15,YMMWORD[rbp]
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((224-128))+rax]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(128-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((416-256-128))+rbx]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((256-256-128))+rbx]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(160-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((448-256-128))+rbx]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((288-256-128))+rbx]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(192-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((480-256-128))+rbx]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((320-256-128))+rbx]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(224-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((0-128))+rax]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((352-256-128))+rbx]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(256-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((32-128))+rax]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((384-256-128))+rbx]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(288-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((64-128))+rax]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((416-256-128))+rbx]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(320-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((96-128))+rax]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((448-256-128))+rbx]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(352-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((128-128))+rax]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((480-256-128))+rbx]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(384-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((160-128))+rax]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((0-128))+rax]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(416-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((192-128))+rax]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((32-128))+rax]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(448-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((224-128))+rax]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((64-128))+rax]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(480-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((256-256-128))+rbx]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((96-128))+rax]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(0-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((288-256-128))+rbx]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((128-128))+rax]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(32-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((320-256-128))+rbx]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((160-128))+rax]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(64-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((352-256-128))+rbx]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((192-128))+rax]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(96-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((384-256-128))+rbx]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((224-128))+rax]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(128-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((416-256-128))+rbx]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((256-256-128))+rbx]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(160-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((448-256-128))+rbx]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((288-256-128))+rbx]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(192-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((480-256-128))+rbx]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((320-256-128))+rbx]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(224-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((0-128))+rax]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovdqa	ymm15,YMMWORD[32+rbp]
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((352-256-128))+rbx]

	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpand	ymm6,ymm3,ymm2
	vpxor	ymm11,ymm11,YMMWORD[((32-128))+rax]

	vpaddd	ymm4,ymm4,ymm6
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm3,ymm2
	vpxor	ymm11,ymm11,ymm13

	vmovdqu	YMMWORD[(256-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm11,31
	vpand	ymm5,ymm5,ymm1
	vpaddd	ymm11,ymm11,ymm11

	vpslld	ymm6,ymm1,30
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((384-256-128))+rbx]

	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpand	ymm6,ymm2,ymm1
	vpxor	ymm12,ymm12,YMMWORD[((64-128))+rax]

	vpaddd	ymm3,ymm3,ymm6
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm2,ymm1
	vpxor	ymm12,ymm12,ymm14

	vmovdqu	YMMWORD[(288-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm12,31
	vpand	ymm5,ymm5,ymm0
	vpaddd	ymm12,ymm12,ymm12

	vpslld	ymm6,ymm0,30
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((416-256-128))+rbx]

	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpand	ymm6,ymm1,ymm0
	vpxor	ymm13,ymm13,YMMWORD[((96-128))+rax]

	vpaddd	ymm2,ymm2,ymm6
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm1,ymm0
	vpxor	ymm13,ymm13,ymm10

	vmovdqu	YMMWORD[(320-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm13,31
	vpand	ymm5,ymm5,ymm4
	vpaddd	ymm13,ymm13,ymm13

	vpslld	ymm6,ymm4,30
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((448-256-128))+rbx]

	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpand	ymm6,ymm0,ymm4
	vpxor	ymm14,ymm14,YMMWORD[((128-128))+rax]

	vpaddd	ymm1,ymm1,ymm6
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm0,ymm4
	vpxor	ymm14,ymm14,ymm11

	vmovdqu	YMMWORD[(352-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm14,31
	vpand	ymm5,ymm5,ymm3
	vpaddd	ymm14,ymm14,ymm14

	vpslld	ymm6,ymm3,30
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((480-256-128))+rbx]

	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpand	ymm6,ymm4,ymm3
	vpxor	ymm10,ymm10,YMMWORD[((160-128))+rax]

	vpaddd	ymm0,ymm0,ymm6
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm4,ymm3
	vpxor	ymm10,ymm10,ymm12

	vmovdqu	YMMWORD[(384-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm10,31
	vpand	ymm5,ymm5,ymm2
	vpaddd	ymm10,ymm10,ymm10

	vpslld	ymm6,ymm2,30
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((0-128))+rax]

	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpand	ymm6,ymm3,ymm2
	vpxor	ymm11,ymm11,YMMWORD[((192-128))+rax]

	vpaddd	ymm4,ymm4,ymm6
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm3,ymm2
	vpxor	ymm11,ymm11,ymm13

	vmovdqu	YMMWORD[(416-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm11,31
	vpand	ymm5,ymm5,ymm1
	vpaddd	ymm11,ymm11,ymm11

	vpslld	ymm6,ymm1,30
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((32-128))+rax]

	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpand	ymm6,ymm2,ymm1
	vpxor	ymm12,ymm12,YMMWORD[((224-128))+rax]

	vpaddd	ymm3,ymm3,ymm6
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm2,ymm1
	vpxor	ymm12,ymm12,ymm14

	vmovdqu	YMMWORD[(448-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm12,31
	vpand	ymm5,ymm5,ymm0
	vpaddd	ymm12,ymm12,ymm12

	vpslld	ymm6,ymm0,30
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((64-128))+rax]

	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpand	ymm6,ymm1,ymm0
	vpxor	ymm13,ymm13,YMMWORD[((256-256-128))+rbx]

	vpaddd	ymm2,ymm2,ymm6
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm1,ymm0
	vpxor	ymm13,ymm13,ymm10

	vmovdqu	YMMWORD[(480-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm13,31
	vpand	ymm5,ymm5,ymm4
	vpaddd	ymm13,ymm13,ymm13

	vpslld	ymm6,ymm4,30
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((96-128))+rax]

	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpand	ymm6,ymm0,ymm4
	vpxor	ymm14,ymm14,YMMWORD[((288-256-128))+rbx]

	vpaddd	ymm1,ymm1,ymm6
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm0,ymm4
	vpxor	ymm14,ymm14,ymm11

	vmovdqu	YMMWORD[(0-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm14,31
	vpand	ymm5,ymm5,ymm3
	vpaddd	ymm14,ymm14,ymm14

	vpslld	ymm6,ymm3,30
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((128-128))+rax]

	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpand	ymm6,ymm4,ymm3
	vpxor	ymm10,ymm10,YMMWORD[((320-256-128))+rbx]

	vpaddd	ymm0,ymm0,ymm6
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm4,ymm3
	vpxor	ymm10,ymm10,ymm12

	vmovdqu	YMMWORD[(32-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm10,31
	vpand	ymm5,ymm5,ymm2
	vpaddd	ymm10,ymm10,ymm10

	vpslld	ymm6,ymm2,30
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((160-128))+rax]

	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpand	ymm6,ymm3,ymm2
	vpxor	ymm11,ymm11,YMMWORD[((352-256-128))+rbx]

	vpaddd	ymm4,ymm4,ymm6
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm3,ymm2
	vpxor	ymm11,ymm11,ymm13

	vmovdqu	YMMWORD[(64-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm11,31
	vpand	ymm5,ymm5,ymm1
	vpaddd	ymm11,ymm11,ymm11

	vpslld	ymm6,ymm1,30
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((192-128))+rax]

	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpand	ymm6,ymm2,ymm1
	vpxor	ymm12,ymm12,YMMWORD[((384-256-128))+rbx]

	vpaddd	ymm3,ymm3,ymm6
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm2,ymm1
	vpxor	ymm12,ymm12,ymm14

	vmovdqu	YMMWORD[(96-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm12,31
	vpand	ymm5,ymm5,ymm0
	vpaddd	ymm12,ymm12,ymm12

	vpslld	ymm6,ymm0,30
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((224-128))+rax]

	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpand	ymm6,ymm1,ymm0
	vpxor	ymm13,ymm13,YMMWORD[((416-256-128))+rbx]

	vpaddd	ymm2,ymm2,ymm6
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm1,ymm0
	vpxor	ymm13,ymm13,ymm10

	vmovdqu	YMMWORD[(128-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm13,31
	vpand	ymm5,ymm5,ymm4
	vpaddd	ymm13,ymm13,ymm13

	vpslld	ymm6,ymm4,30
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((256-256-128))+rbx]

	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpand	ymm6,ymm0,ymm4
	vpxor	ymm14,ymm14,YMMWORD[((448-256-128))+rbx]

	vpaddd	ymm1,ymm1,ymm6
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm0,ymm4
	vpxor	ymm14,ymm14,ymm11

	vmovdqu	YMMWORD[(160-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm14,31
	vpand	ymm5,ymm5,ymm3
	vpaddd	ymm14,ymm14,ymm14

	vpslld	ymm6,ymm3,30
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((288-256-128))+rbx]

	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpand	ymm6,ymm4,ymm3
	vpxor	ymm10,ymm10,YMMWORD[((480-256-128))+rbx]

	vpaddd	ymm0,ymm0,ymm6
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm4,ymm3
	vpxor	ymm10,ymm10,ymm12

	vmovdqu	YMMWORD[(192-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm10,31
	vpand	ymm5,ymm5,ymm2
	vpaddd	ymm10,ymm10,ymm10

	vpslld	ymm6,ymm2,30
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((320-256-128))+rbx]

	vpaddd	ymm4,ymm4,ymm15
	vpslld	ymm7,ymm0,5
	vpand	ymm6,ymm3,ymm2
	vpxor	ymm11,ymm11,YMMWORD[((0-128))+rax]

	vpaddd	ymm4,ymm4,ymm6
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm3,ymm2
	vpxor	ymm11,ymm11,ymm13

	vmovdqu	YMMWORD[(224-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm11,31
	vpand	ymm5,ymm5,ymm1
	vpaddd	ymm11,ymm11,ymm11

	vpslld	ymm6,ymm1,30
	vpaddd	ymm4,ymm4,ymm5

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((352-256-128))+rbx]

	vpaddd	ymm3,ymm3,ymm15
	vpslld	ymm7,ymm4,5
	vpand	ymm6,ymm2,ymm1
	vpxor	ymm12,ymm12,YMMWORD[((32-128))+rax]

	vpaddd	ymm3,ymm3,ymm6
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm2,ymm1
	vpxor	ymm12,ymm12,ymm14

	vmovdqu	YMMWORD[(256-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm12,31
	vpand	ymm5,ymm5,ymm0
	vpaddd	ymm12,ymm12,ymm12

	vpslld	ymm6,ymm0,30
	vpaddd	ymm3,ymm3,ymm5

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((384-256-128))+rbx]

	vpaddd	ymm2,ymm2,ymm15
	vpslld	ymm7,ymm3,5
	vpand	ymm6,ymm1,ymm0
	vpxor	ymm13,ymm13,YMMWORD[((64-128))+rax]

	vpaddd	ymm2,ymm2,ymm6
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm1,ymm0
	vpxor	ymm13,ymm13,ymm10

	vmovdqu	YMMWORD[(288-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm13,31
	vpand	ymm5,ymm5,ymm4
	vpaddd	ymm13,ymm13,ymm13

	vpslld	ymm6,ymm4,30
	vpaddd	ymm2,ymm2,ymm5

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((416-256-128))+rbx]

	vpaddd	ymm1,ymm1,ymm15
	vpslld	ymm7,ymm2,5
	vpand	ymm6,ymm0,ymm4
	vpxor	ymm14,ymm14,YMMWORD[((96-128))+rax]

	vpaddd	ymm1,ymm1,ymm6
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm0,ymm4
	vpxor	ymm14,ymm14,ymm11

	vmovdqu	YMMWORD[(320-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm14,31
	vpand	ymm5,ymm5,ymm3
	vpaddd	ymm14,ymm14,ymm14

	vpslld	ymm6,ymm3,30
	vpaddd	ymm1,ymm1,ymm5

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((448-256-128))+rbx]

	vpaddd	ymm0,ymm0,ymm15
	vpslld	ymm7,ymm1,5
	vpand	ymm6,ymm4,ymm3
	vpxor	ymm10,ymm10,YMMWORD[((128-128))+rax]

	vpaddd	ymm0,ymm0,ymm6
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm4,ymm3
	vpxor	ymm10,ymm10,ymm12

	vmovdqu	YMMWORD[(352-256-128)+rbx],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpor	ymm7,ymm7,ymm8
	vpsrld	ymm9,ymm10,31
	vpand	ymm5,ymm5,ymm2
	vpaddd	ymm10,ymm10,ymm10

	vpslld	ymm6,ymm2,30
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vmovdqa	ymm15,YMMWORD[64+rbp]
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((480-256-128))+rbx]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(384-256-128)+rbx],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((160-128))+rax]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((0-128))+rax]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(416-256-128)+rbx],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((192-128))+rax]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((32-128))+rax]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(448-256-128)+rbx],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((224-128))+rax]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((64-128))+rax]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(480-256-128)+rbx],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((256-256-128))+rbx]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((96-128))+rax]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(0-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((288-256-128))+rbx]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((128-128))+rax]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(32-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((320-256-128))+rbx]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((160-128))+rax]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(64-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((352-256-128))+rbx]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((192-128))+rax]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vmovdqa	YMMWORD[(96-128)+rax],ymm12
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((384-256-128))+rbx]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((224-128))+rax]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vmovdqa	YMMWORD[(128-128)+rax],ymm13
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((416-256-128))+rbx]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((256-256-128))+rbx]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vmovdqa	YMMWORD[(160-128)+rax],ymm14
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((448-256-128))+rbx]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((288-256-128))+rbx]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vmovdqa	YMMWORD[(192-128)+rax],ymm10
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((480-256-128))+rbx]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((320-256-128))+rbx]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vmovdqa	YMMWORD[(224-128)+rax],ymm11
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((0-128))+rax]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((352-256-128))+rbx]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((32-128))+rax]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((384-256-128))+rbx]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((64-128))+rax]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpxor	ymm10,ymm10,ymm12
	vmovdqa	ymm12,YMMWORD[((416-256-128))+rbx]

	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm10,ymm10,YMMWORD[((96-128))+rax]
	vpsrld	ymm8,ymm1,27
	vpxor	ymm5,ymm5,ymm3
	vpxor	ymm10,ymm10,ymm12

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5
	vpsrld	ymm9,ymm10,31
	vpaddd	ymm10,ymm10,ymm10

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm10,ymm10,ymm9
	vpor	ymm2,ymm2,ymm6
	vpxor	ymm11,ymm11,ymm13
	vmovdqa	ymm13,YMMWORD[((448-256-128))+rbx]

	vpslld	ymm7,ymm0,5
	vpaddd	ymm4,ymm4,ymm15
	vpxor	ymm5,ymm3,ymm1
	vpaddd	ymm4,ymm4,ymm10
	vpxor	ymm11,ymm11,YMMWORD[((128-128))+rax]
	vpsrld	ymm8,ymm0,27
	vpxor	ymm5,ymm5,ymm2
	vpxor	ymm11,ymm11,ymm13

	vpslld	ymm6,ymm1,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm4,ymm4,ymm5
	vpsrld	ymm9,ymm11,31
	vpaddd	ymm11,ymm11,ymm11

	vpsrld	ymm1,ymm1,2
	vpaddd	ymm4,ymm4,ymm7
	vpor	ymm11,ymm11,ymm9
	vpor	ymm1,ymm1,ymm6
	vpxor	ymm12,ymm12,ymm14
	vmovdqa	ymm14,YMMWORD[((480-256-128))+rbx]

	vpslld	ymm7,ymm4,5
	vpaddd	ymm3,ymm3,ymm15
	vpxor	ymm5,ymm2,ymm0
	vpaddd	ymm3,ymm3,ymm11
	vpxor	ymm12,ymm12,YMMWORD[((160-128))+rax]
	vpsrld	ymm8,ymm4,27
	vpxor	ymm5,ymm5,ymm1
	vpxor	ymm12,ymm12,ymm14

	vpslld	ymm6,ymm0,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm3,ymm3,ymm5
	vpsrld	ymm9,ymm12,31
	vpaddd	ymm12,ymm12,ymm12

	vpsrld	ymm0,ymm0,2
	vpaddd	ymm3,ymm3,ymm7
	vpor	ymm12,ymm12,ymm9
	vpor	ymm0,ymm0,ymm6
	vpxor	ymm13,ymm13,ymm10
	vmovdqa	ymm10,YMMWORD[((0-128))+rax]

	vpslld	ymm7,ymm3,5
	vpaddd	ymm2,ymm2,ymm15
	vpxor	ymm5,ymm1,ymm4
	vpaddd	ymm2,ymm2,ymm12
	vpxor	ymm13,ymm13,YMMWORD[((192-128))+rax]
	vpsrld	ymm8,ymm3,27
	vpxor	ymm5,ymm5,ymm0
	vpxor	ymm13,ymm13,ymm10

	vpslld	ymm6,ymm4,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm2,ymm2,ymm5
	vpsrld	ymm9,ymm13,31
	vpaddd	ymm13,ymm13,ymm13

	vpsrld	ymm4,ymm4,2
	vpaddd	ymm2,ymm2,ymm7
	vpor	ymm13,ymm13,ymm9
	vpor	ymm4,ymm4,ymm6
	vpxor	ymm14,ymm14,ymm11
	vmovdqa	ymm11,YMMWORD[((32-128))+rax]

	vpslld	ymm7,ymm2,5
	vpaddd	ymm1,ymm1,ymm15
	vpxor	ymm5,ymm0,ymm3
	vpaddd	ymm1,ymm1,ymm13
	vpxor	ymm14,ymm14,YMMWORD[((224-128))+rax]
	vpsrld	ymm8,ymm2,27
	vpxor	ymm5,ymm5,ymm4
	vpxor	ymm14,ymm14,ymm11

	vpslld	ymm6,ymm3,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm1,ymm1,ymm5
	vpsrld	ymm9,ymm14,31
	vpaddd	ymm14,ymm14,ymm14

	vpsrld	ymm3,ymm3,2
	vpaddd	ymm1,ymm1,ymm7
	vpor	ymm14,ymm14,ymm9
	vpor	ymm3,ymm3,ymm6
	vpslld	ymm7,ymm1,5
	vpaddd	ymm0,ymm0,ymm15
	vpxor	ymm5,ymm4,ymm2

	vpsrld	ymm8,ymm1,27
	vpaddd	ymm0,ymm0,ymm14
	vpxor	ymm5,ymm5,ymm3

	vpslld	ymm6,ymm2,30
	vpor	ymm7,ymm7,ymm8
	vpaddd	ymm0,ymm0,ymm5

	vpsrld	ymm2,ymm2,2
	vpaddd	ymm0,ymm0,ymm7
	vpor	ymm2,ymm2,ymm6
	mov	ecx,1
	lea	rbx,[512+rsp]
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
	vmovdqu	ymm5,YMMWORD[rbx]
	vpxor	ymm7,ymm7,ymm7
	vmovdqa	ymm6,ymm5
	vpcmpgtd	ymm6,ymm6,ymm7
	vpaddd	ymm5,ymm5,ymm6

	vpand	ymm0,ymm0,ymm6
	vpand	ymm1,ymm1,ymm6
	vpaddd	ymm0,ymm0,YMMWORD[rdi]
	vpand	ymm2,ymm2,ymm6
	vpaddd	ymm1,ymm1,YMMWORD[32+rdi]
	vpand	ymm3,ymm3,ymm6
	vpaddd	ymm2,ymm2,YMMWORD[64+rdi]
	vpand	ymm4,ymm4,ymm6
	vpaddd	ymm3,ymm3,YMMWORD[96+rdi]
	vpaddd	ymm4,ymm4,YMMWORD[128+rdi]
	vmovdqu	YMMWORD[rdi],ymm0
	vmovdqu	YMMWORD[32+rdi],ymm1
	vmovdqu	YMMWORD[64+rdi],ymm2
	vmovdqu	YMMWORD[96+rdi],ymm3
	vmovdqu	YMMWORD[128+rdi],ymm4

	vmovdqu	YMMWORD[rbx],ymm5
	lea	rbx,[((256+128))+rsp]
	vmovdqu	ymm9,YMMWORD[96+rbp]
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

$L$SEH_end_sha1_multi_block_avx2:

ALIGN	256
	DD	0x5a827999,0x5a827999,0x5a827999,0x5a827999
	DD	0x5a827999,0x5a827999,0x5a827999,0x5a827999
K_XX_XX:
	DD	0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1
	DD	0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1
	DD	0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc
	DD	0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc
	DD	0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6
	DD	0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6
	DD	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
	DD	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
DB	0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0
DB	83,72,65,49,32,109,117,108,116,105,45,98,108,111,99,107
DB	32,116,114,97,110,115,102,111,114,109,32,102,111,114,32,120
DB	56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77
DB	83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110
DB	115,115,108,46,111,114,103,62,0
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
	DD	$L$SEH_begin_sha1_multi_block wrt ..imagebase
	DD	$L$SEH_end_sha1_multi_block wrt ..imagebase
	DD	$L$SEH_info_sha1_multi_block wrt ..imagebase
	DD	$L$SEH_begin_sha1_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_end_sha1_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_info_sha1_multi_block_shaext wrt ..imagebase
	DD	$L$SEH_begin_sha1_multi_block_avx wrt ..imagebase
	DD	$L$SEH_end_sha1_multi_block_avx wrt ..imagebase
	DD	$L$SEH_info_sha1_multi_block_avx wrt ..imagebase
	DD	$L$SEH_begin_sha1_multi_block_avx2 wrt ..imagebase
	DD	$L$SEH_end_sha1_multi_block_avx2 wrt ..imagebase
	DD	$L$SEH_info_sha1_multi_block_avx2 wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_sha1_multi_block:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body wrt ..imagebase,$L$epilogue wrt ..imagebase
$L$SEH_info_sha1_multi_block_shaext:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body_shaext wrt ..imagebase,$L$epilogue_shaext wrt ..imagebase
$L$SEH_info_sha1_multi_block_avx:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
	DD	$L$body_avx wrt ..imagebase,$L$epilogue_avx wrt ..imagebase
$L$SEH_info_sha1_multi_block_avx2:
DB	9,0,0,0
	DD	avx2_handler wrt ..imagebase
	DD	$L$body_avx2 wrt ..imagebase,$L$epilogue_avx2 wrt ..imagebase
