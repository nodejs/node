default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

ALIGN	64
$L$zero:
	DD	0,0,0,0
$L$one:
	DD	1,0,0,0
$L$inc:
	DD	0,1,2,3
$L$four:
	DD	4,4,4,4
$L$incy:
	DD	0,2,4,6,1,3,5,7
$L$eight:
	DD	8,8,8,8,8,8,8,8
$L$rot16:
DB	0x2,0x3,0x0,0x1,0x6,0x7,0x4,0x5,0xa,0xb,0x8,0x9,0xe,0xf,0xc,0xd
$L$rot24:
DB	0x3,0x0,0x1,0x2,0x7,0x4,0x5,0x6,0xb,0x8,0x9,0xa,0xf,0xc,0xd,0xe
$L$sigma:
DB	101,120,112,97,110,100,32,51,50,45,98,121,116,101,32,107
DB	0
DB	67,104,97,67,104,97,50,48,32,102,111,114,32,120,56,54
DB	95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32
DB	98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115
DB	108,46,111,114,103,62,0
global	ChaCha20_ctr32

ALIGN	64
ChaCha20_ctr32:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ChaCha20_ctr32:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]


	cmp	rdx,0
	je	NEAR $L$no_data
	mov	r10,QWORD[((OPENSSL_ia32cap_P+4))]
	test	r10d,512
	jnz	NEAR $L$ChaCha20_ssse3

	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	sub	rsp,64+24


	movdqu	xmm1,XMMWORD[rcx]
	movdqu	xmm2,XMMWORD[16+rcx]
	movdqu	xmm3,XMMWORD[r8]
	movdqa	xmm4,XMMWORD[$L$one]


	movdqa	XMMWORD[16+rsp],xmm1
	movdqa	XMMWORD[32+rsp],xmm2
	movdqa	XMMWORD[48+rsp],xmm3
	mov	rbp,rdx
	jmp	NEAR $L$oop_outer

ALIGN	32
$L$oop_outer:
	mov	eax,0x61707865
	mov	ebx,0x3320646e
	mov	ecx,0x79622d32
	mov	edx,0x6b206574
	mov	r8d,DWORD[16+rsp]
	mov	r9d,DWORD[20+rsp]
	mov	r10d,DWORD[24+rsp]
	mov	r11d,DWORD[28+rsp]
	movd	r12d,xmm3
	mov	r13d,DWORD[52+rsp]
	mov	r14d,DWORD[56+rsp]
	mov	r15d,DWORD[60+rsp]

	mov	QWORD[((64+0))+rsp],rbp
	mov	ebp,10
	mov	QWORD[((64+8))+rsp],rsi
DB	102,72,15,126,214
	mov	QWORD[((64+16))+rsp],rdi
	mov	rdi,rsi
	shr	rdi,32
	jmp	NEAR $L$oop

ALIGN	32
$L$oop:
	add	eax,r8d
	xor	r12d,eax
	rol	r12d,16
	add	ebx,r9d
	xor	r13d,ebx
	rol	r13d,16
	add	esi,r12d
	xor	r8d,esi
	rol	r8d,12
	add	edi,r13d
	xor	r9d,edi
	rol	r9d,12
	add	eax,r8d
	xor	r12d,eax
	rol	r12d,8
	add	ebx,r9d
	xor	r13d,ebx
	rol	r13d,8
	add	esi,r12d
	xor	r8d,esi
	rol	r8d,7
	add	edi,r13d
	xor	r9d,edi
	rol	r9d,7
	mov	DWORD[32+rsp],esi
	mov	DWORD[36+rsp],edi
	mov	esi,DWORD[40+rsp]
	mov	edi,DWORD[44+rsp]
	add	ecx,r10d
	xor	r14d,ecx
	rol	r14d,16
	add	edx,r11d
	xor	r15d,edx
	rol	r15d,16
	add	esi,r14d
	xor	r10d,esi
	rol	r10d,12
	add	edi,r15d
	xor	r11d,edi
	rol	r11d,12
	add	ecx,r10d
	xor	r14d,ecx
	rol	r14d,8
	add	edx,r11d
	xor	r15d,edx
	rol	r15d,8
	add	esi,r14d
	xor	r10d,esi
	rol	r10d,7
	add	edi,r15d
	xor	r11d,edi
	rol	r11d,7
	add	eax,r9d
	xor	r15d,eax
	rol	r15d,16
	add	ebx,r10d
	xor	r12d,ebx
	rol	r12d,16
	add	esi,r15d
	xor	r9d,esi
	rol	r9d,12
	add	edi,r12d
	xor	r10d,edi
	rol	r10d,12
	add	eax,r9d
	xor	r15d,eax
	rol	r15d,8
	add	ebx,r10d
	xor	r12d,ebx
	rol	r12d,8
	add	esi,r15d
	xor	r9d,esi
	rol	r9d,7
	add	edi,r12d
	xor	r10d,edi
	rol	r10d,7
	mov	DWORD[40+rsp],esi
	mov	DWORD[44+rsp],edi
	mov	esi,DWORD[32+rsp]
	mov	edi,DWORD[36+rsp]
	add	ecx,r11d
	xor	r13d,ecx
	rol	r13d,16
	add	edx,r8d
	xor	r14d,edx
	rol	r14d,16
	add	esi,r13d
	xor	r11d,esi
	rol	r11d,12
	add	edi,r14d
	xor	r8d,edi
	rol	r8d,12
	add	ecx,r11d
	xor	r13d,ecx
	rol	r13d,8
	add	edx,r8d
	xor	r14d,edx
	rol	r14d,8
	add	esi,r13d
	xor	r11d,esi
	rol	r11d,7
	add	edi,r14d
	xor	r8d,edi
	rol	r8d,7
	dec	ebp
	jnz	NEAR $L$oop
	mov	DWORD[36+rsp],edi
	mov	DWORD[32+rsp],esi
	mov	rbp,QWORD[64+rsp]
	movdqa	xmm1,xmm2
	mov	rsi,QWORD[((64+8))+rsp]
	paddd	xmm3,xmm4
	mov	rdi,QWORD[((64+16))+rsp]

	add	eax,0x61707865
	add	ebx,0x3320646e
	add	ecx,0x79622d32
	add	edx,0x6b206574
	add	r8d,DWORD[16+rsp]
	add	r9d,DWORD[20+rsp]
	add	r10d,DWORD[24+rsp]
	add	r11d,DWORD[28+rsp]
	add	r12d,DWORD[48+rsp]
	add	r13d,DWORD[52+rsp]
	add	r14d,DWORD[56+rsp]
	add	r15d,DWORD[60+rsp]
	paddd	xmm1,XMMWORD[32+rsp]

	cmp	rbp,64
	jb	NEAR $L$tail

	xor	eax,DWORD[rsi]
	xor	ebx,DWORD[4+rsi]
	xor	ecx,DWORD[8+rsi]
	xor	edx,DWORD[12+rsi]
	xor	r8d,DWORD[16+rsi]
	xor	r9d,DWORD[20+rsi]
	xor	r10d,DWORD[24+rsi]
	xor	r11d,DWORD[28+rsi]
	movdqu	xmm0,XMMWORD[32+rsi]
	xor	r12d,DWORD[48+rsi]
	xor	r13d,DWORD[52+rsi]
	xor	r14d,DWORD[56+rsi]
	xor	r15d,DWORD[60+rsi]
	lea	rsi,[64+rsi]
	pxor	xmm0,xmm1

	movdqa	XMMWORD[32+rsp],xmm2
	movd	DWORD[48+rsp],xmm3

	mov	DWORD[rdi],eax
	mov	DWORD[4+rdi],ebx
	mov	DWORD[8+rdi],ecx
	mov	DWORD[12+rdi],edx
	mov	DWORD[16+rdi],r8d
	mov	DWORD[20+rdi],r9d
	mov	DWORD[24+rdi],r10d
	mov	DWORD[28+rdi],r11d
	movdqu	XMMWORD[32+rdi],xmm0
	mov	DWORD[48+rdi],r12d
	mov	DWORD[52+rdi],r13d
	mov	DWORD[56+rdi],r14d
	mov	DWORD[60+rdi],r15d
	lea	rdi,[64+rdi]

	sub	rbp,64
	jnz	NEAR $L$oop_outer

	jmp	NEAR $L$done

ALIGN	16
$L$tail:
	mov	DWORD[rsp],eax
	mov	DWORD[4+rsp],ebx
	xor	rbx,rbx
	mov	DWORD[8+rsp],ecx
	mov	DWORD[12+rsp],edx
	mov	DWORD[16+rsp],r8d
	mov	DWORD[20+rsp],r9d
	mov	DWORD[24+rsp],r10d
	mov	DWORD[28+rsp],r11d
	movdqa	XMMWORD[32+rsp],xmm1
	mov	DWORD[48+rsp],r12d
	mov	DWORD[52+rsp],r13d
	mov	DWORD[56+rsp],r14d
	mov	DWORD[60+rsp],r15d

$L$oop_tail:
	movzx	eax,BYTE[rbx*1+rsi]
	movzx	edx,BYTE[rbx*1+rsp]
	lea	rbx,[1+rbx]
	xor	eax,edx
	mov	BYTE[((-1))+rbx*1+rdi],al
	dec	rbp
	jnz	NEAR $L$oop_tail

$L$done:
	add	rsp,64+24
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
$L$no_data:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ChaCha20_ctr32:

ALIGN	32
ChaCha20_ssse3:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ChaCha20_ssse3:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]


$L$ChaCha20_ssse3:
	test	r10d,2048
	jnz	NEAR $L$ChaCha20_4xop
	cmp	rdx,128
	ja	NEAR $L$ChaCha20_4x

$L$do_sse3_after_all:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15

	sub	rsp,64+72
	movaps	XMMWORD[(64+32)+rsp],xmm6
	movaps	XMMWORD[(64+48)+rsp],xmm7
	movdqa	xmm0,XMMWORD[$L$sigma]
	movdqu	xmm1,XMMWORD[rcx]
	movdqu	xmm2,XMMWORD[16+rcx]
	movdqu	xmm3,XMMWORD[r8]
	movdqa	xmm6,XMMWORD[$L$rot16]
	movdqa	xmm7,XMMWORD[$L$rot24]

	movdqa	XMMWORD[rsp],xmm0
	movdqa	XMMWORD[16+rsp],xmm1
	movdqa	XMMWORD[32+rsp],xmm2
	movdqa	XMMWORD[48+rsp],xmm3
	mov	ebp,10
	jmp	NEAR $L$oop_ssse3

ALIGN	32
$L$oop_outer_ssse3:
	movdqa	xmm3,XMMWORD[$L$one]
	movdqa	xmm0,XMMWORD[rsp]
	movdqa	xmm1,XMMWORD[16+rsp]
	movdqa	xmm2,XMMWORD[32+rsp]
	paddd	xmm3,XMMWORD[48+rsp]
	mov	ebp,10
	movdqa	XMMWORD[48+rsp],xmm3
	jmp	NEAR $L$oop_ssse3

ALIGN	32
$L$oop_ssse3:
	paddd	xmm0,xmm1
	pxor	xmm3,xmm0
DB	102,15,56,0,222
	paddd	xmm2,xmm3
	pxor	xmm1,xmm2
	movdqa	xmm4,xmm1
	psrld	xmm1,20
	pslld	xmm4,12
	por	xmm1,xmm4
	paddd	xmm0,xmm1
	pxor	xmm3,xmm0
DB	102,15,56,0,223
	paddd	xmm2,xmm3
	pxor	xmm1,xmm2
	movdqa	xmm4,xmm1
	psrld	xmm1,25
	pslld	xmm4,7
	por	xmm1,xmm4
	pshufd	xmm2,xmm2,78
	pshufd	xmm1,xmm1,57
	pshufd	xmm3,xmm3,147
	nop
	paddd	xmm0,xmm1
	pxor	xmm3,xmm0
DB	102,15,56,0,222
	paddd	xmm2,xmm3
	pxor	xmm1,xmm2
	movdqa	xmm4,xmm1
	psrld	xmm1,20
	pslld	xmm4,12
	por	xmm1,xmm4
	paddd	xmm0,xmm1
	pxor	xmm3,xmm0
DB	102,15,56,0,223
	paddd	xmm2,xmm3
	pxor	xmm1,xmm2
	movdqa	xmm4,xmm1
	psrld	xmm1,25
	pslld	xmm4,7
	por	xmm1,xmm4
	pshufd	xmm2,xmm2,78
	pshufd	xmm1,xmm1,147
	pshufd	xmm3,xmm3,57
	dec	ebp
	jnz	NEAR $L$oop_ssse3
	paddd	xmm0,XMMWORD[rsp]
	paddd	xmm1,XMMWORD[16+rsp]
	paddd	xmm2,XMMWORD[32+rsp]
	paddd	xmm3,XMMWORD[48+rsp]

	cmp	rdx,64
	jb	NEAR $L$tail_ssse3

	movdqu	xmm4,XMMWORD[rsi]
	movdqu	xmm5,XMMWORD[16+rsi]
	pxor	xmm0,xmm4
	movdqu	xmm4,XMMWORD[32+rsi]
	pxor	xmm1,xmm5
	movdqu	xmm5,XMMWORD[48+rsi]
	lea	rsi,[64+rsi]
	pxor	xmm2,xmm4
	pxor	xmm3,xmm5

	movdqu	XMMWORD[rdi],xmm0
	movdqu	XMMWORD[16+rdi],xmm1
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	XMMWORD[48+rdi],xmm3
	lea	rdi,[64+rdi]

	sub	rdx,64
	jnz	NEAR $L$oop_outer_ssse3

	jmp	NEAR $L$done_ssse3

ALIGN	16
$L$tail_ssse3:
	movdqa	XMMWORD[rsp],xmm0
	movdqa	XMMWORD[16+rsp],xmm1
	movdqa	XMMWORD[32+rsp],xmm2
	movdqa	XMMWORD[48+rsp],xmm3
	xor	rbx,rbx

$L$oop_tail_ssse3:
	movzx	eax,BYTE[rbx*1+rsi]
	movzx	ecx,BYTE[rbx*1+rsp]
	lea	rbx,[1+rbx]
	xor	eax,ecx
	mov	BYTE[((-1))+rbx*1+rdi],al
	dec	rdx
	jnz	NEAR $L$oop_tail_ssse3

$L$done_ssse3:
	movaps	xmm6,XMMWORD[((64+32))+rsp]
	movaps	xmm7,XMMWORD[((64+48))+rsp]
	add	rsp,64+72
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ChaCha20_ssse3:

ALIGN	32
ChaCha20_4x:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ChaCha20_4x:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]


$L$ChaCha20_4x:
	mov	r11,r10
	shr	r10,32
	test	r10,32
	jnz	NEAR $L$ChaCha20_8x
	cmp	rdx,192
	ja	NEAR $L$proceed4x

	and	r11,71303168
	cmp	r11,4194304
	je	NEAR $L$do_sse3_after_all

$L$proceed4x:
	lea	r11,[((-120))+rsp]
	sub	rsp,0x148+160
	movaps	XMMWORD[(-48)+r11],xmm6
	movaps	XMMWORD[(-32)+r11],xmm7
	movaps	XMMWORD[(-16)+r11],xmm8
	movaps	XMMWORD[r11],xmm9
	movaps	XMMWORD[16+r11],xmm10
	movaps	XMMWORD[32+r11],xmm11
	movaps	XMMWORD[48+r11],xmm12
	movaps	XMMWORD[64+r11],xmm13
	movaps	XMMWORD[80+r11],xmm14
	movaps	XMMWORD[96+r11],xmm15
	movdqa	xmm11,XMMWORD[$L$sigma]
	movdqu	xmm15,XMMWORD[rcx]
	movdqu	xmm7,XMMWORD[16+rcx]
	movdqu	xmm3,XMMWORD[r8]
	lea	rcx,[256+rsp]
	lea	r10,[$L$rot16]
	lea	r11,[$L$rot24]

	pshufd	xmm8,xmm11,0x00
	pshufd	xmm9,xmm11,0x55
	movdqa	XMMWORD[64+rsp],xmm8
	pshufd	xmm10,xmm11,0xaa
	movdqa	XMMWORD[80+rsp],xmm9
	pshufd	xmm11,xmm11,0xff
	movdqa	XMMWORD[96+rsp],xmm10
	movdqa	XMMWORD[112+rsp],xmm11

	pshufd	xmm12,xmm15,0x00
	pshufd	xmm13,xmm15,0x55
	movdqa	XMMWORD[(128-256)+rcx],xmm12
	pshufd	xmm14,xmm15,0xaa
	movdqa	XMMWORD[(144-256)+rcx],xmm13
	pshufd	xmm15,xmm15,0xff
	movdqa	XMMWORD[(160-256)+rcx],xmm14
	movdqa	XMMWORD[(176-256)+rcx],xmm15

	pshufd	xmm4,xmm7,0x00
	pshufd	xmm5,xmm7,0x55
	movdqa	XMMWORD[(192-256)+rcx],xmm4
	pshufd	xmm6,xmm7,0xaa
	movdqa	XMMWORD[(208-256)+rcx],xmm5
	pshufd	xmm7,xmm7,0xff
	movdqa	XMMWORD[(224-256)+rcx],xmm6
	movdqa	XMMWORD[(240-256)+rcx],xmm7

	pshufd	xmm0,xmm3,0x00
	pshufd	xmm1,xmm3,0x55
	paddd	xmm0,XMMWORD[$L$inc]
	pshufd	xmm2,xmm3,0xaa
	movdqa	XMMWORD[(272-256)+rcx],xmm1
	pshufd	xmm3,xmm3,0xff
	movdqa	XMMWORD[(288-256)+rcx],xmm2
	movdqa	XMMWORD[(304-256)+rcx],xmm3

	jmp	NEAR $L$oop_enter4x

ALIGN	32
$L$oop_outer4x:
	movdqa	xmm8,XMMWORD[64+rsp]
	movdqa	xmm9,XMMWORD[80+rsp]
	movdqa	xmm10,XMMWORD[96+rsp]
	movdqa	xmm11,XMMWORD[112+rsp]
	movdqa	xmm12,XMMWORD[((128-256))+rcx]
	movdqa	xmm13,XMMWORD[((144-256))+rcx]
	movdqa	xmm14,XMMWORD[((160-256))+rcx]
	movdqa	xmm15,XMMWORD[((176-256))+rcx]
	movdqa	xmm4,XMMWORD[((192-256))+rcx]
	movdqa	xmm5,XMMWORD[((208-256))+rcx]
	movdqa	xmm6,XMMWORD[((224-256))+rcx]
	movdqa	xmm7,XMMWORD[((240-256))+rcx]
	movdqa	xmm0,XMMWORD[((256-256))+rcx]
	movdqa	xmm1,XMMWORD[((272-256))+rcx]
	movdqa	xmm2,XMMWORD[((288-256))+rcx]
	movdqa	xmm3,XMMWORD[((304-256))+rcx]
	paddd	xmm0,XMMWORD[$L$four]

$L$oop_enter4x:
	movdqa	XMMWORD[32+rsp],xmm6
	movdqa	XMMWORD[48+rsp],xmm7
	movdqa	xmm7,XMMWORD[r10]
	mov	eax,10
	movdqa	XMMWORD[(256-256)+rcx],xmm0
	jmp	NEAR $L$oop4x

ALIGN	32
$L$oop4x:
	paddd	xmm8,xmm12
	paddd	xmm9,xmm13
	pxor	xmm0,xmm8
	pxor	xmm1,xmm9
DB	102,15,56,0,199
DB	102,15,56,0,207
	paddd	xmm4,xmm0
	paddd	xmm5,xmm1
	pxor	xmm12,xmm4
	pxor	xmm13,xmm5
	movdqa	xmm6,xmm12
	pslld	xmm12,12
	psrld	xmm6,20
	movdqa	xmm7,xmm13
	pslld	xmm13,12
	por	xmm12,xmm6
	psrld	xmm7,20
	movdqa	xmm6,XMMWORD[r11]
	por	xmm13,xmm7
	paddd	xmm8,xmm12
	paddd	xmm9,xmm13
	pxor	xmm0,xmm8
	pxor	xmm1,xmm9
DB	102,15,56,0,198
DB	102,15,56,0,206
	paddd	xmm4,xmm0
	paddd	xmm5,xmm1
	pxor	xmm12,xmm4
	pxor	xmm13,xmm5
	movdqa	xmm7,xmm12
	pslld	xmm12,7
	psrld	xmm7,25
	movdqa	xmm6,xmm13
	pslld	xmm13,7
	por	xmm12,xmm7
	psrld	xmm6,25
	movdqa	xmm7,XMMWORD[r10]
	por	xmm13,xmm6
	movdqa	XMMWORD[rsp],xmm4
	movdqa	XMMWORD[16+rsp],xmm5
	movdqa	xmm4,XMMWORD[32+rsp]
	movdqa	xmm5,XMMWORD[48+rsp]
	paddd	xmm10,xmm14
	paddd	xmm11,xmm15
	pxor	xmm2,xmm10
	pxor	xmm3,xmm11
DB	102,15,56,0,215
DB	102,15,56,0,223
	paddd	xmm4,xmm2
	paddd	xmm5,xmm3
	pxor	xmm14,xmm4
	pxor	xmm15,xmm5
	movdqa	xmm6,xmm14
	pslld	xmm14,12
	psrld	xmm6,20
	movdqa	xmm7,xmm15
	pslld	xmm15,12
	por	xmm14,xmm6
	psrld	xmm7,20
	movdqa	xmm6,XMMWORD[r11]
	por	xmm15,xmm7
	paddd	xmm10,xmm14
	paddd	xmm11,xmm15
	pxor	xmm2,xmm10
	pxor	xmm3,xmm11
DB	102,15,56,0,214
DB	102,15,56,0,222
	paddd	xmm4,xmm2
	paddd	xmm5,xmm3
	pxor	xmm14,xmm4
	pxor	xmm15,xmm5
	movdqa	xmm7,xmm14
	pslld	xmm14,7
	psrld	xmm7,25
	movdqa	xmm6,xmm15
	pslld	xmm15,7
	por	xmm14,xmm7
	psrld	xmm6,25
	movdqa	xmm7,XMMWORD[r10]
	por	xmm15,xmm6
	paddd	xmm8,xmm13
	paddd	xmm9,xmm14
	pxor	xmm3,xmm8
	pxor	xmm0,xmm9
DB	102,15,56,0,223
DB	102,15,56,0,199
	paddd	xmm4,xmm3
	paddd	xmm5,xmm0
	pxor	xmm13,xmm4
	pxor	xmm14,xmm5
	movdqa	xmm6,xmm13
	pslld	xmm13,12
	psrld	xmm6,20
	movdqa	xmm7,xmm14
	pslld	xmm14,12
	por	xmm13,xmm6
	psrld	xmm7,20
	movdqa	xmm6,XMMWORD[r11]
	por	xmm14,xmm7
	paddd	xmm8,xmm13
	paddd	xmm9,xmm14
	pxor	xmm3,xmm8
	pxor	xmm0,xmm9
DB	102,15,56,0,222
DB	102,15,56,0,198
	paddd	xmm4,xmm3
	paddd	xmm5,xmm0
	pxor	xmm13,xmm4
	pxor	xmm14,xmm5
	movdqa	xmm7,xmm13
	pslld	xmm13,7
	psrld	xmm7,25
	movdqa	xmm6,xmm14
	pslld	xmm14,7
	por	xmm13,xmm7
	psrld	xmm6,25
	movdqa	xmm7,XMMWORD[r10]
	por	xmm14,xmm6
	movdqa	XMMWORD[32+rsp],xmm4
	movdqa	XMMWORD[48+rsp],xmm5
	movdqa	xmm4,XMMWORD[rsp]
	movdqa	xmm5,XMMWORD[16+rsp]
	paddd	xmm10,xmm15
	paddd	xmm11,xmm12
	pxor	xmm1,xmm10
	pxor	xmm2,xmm11
DB	102,15,56,0,207
DB	102,15,56,0,215
	paddd	xmm4,xmm1
	paddd	xmm5,xmm2
	pxor	xmm15,xmm4
	pxor	xmm12,xmm5
	movdqa	xmm6,xmm15
	pslld	xmm15,12
	psrld	xmm6,20
	movdqa	xmm7,xmm12
	pslld	xmm12,12
	por	xmm15,xmm6
	psrld	xmm7,20
	movdqa	xmm6,XMMWORD[r11]
	por	xmm12,xmm7
	paddd	xmm10,xmm15
	paddd	xmm11,xmm12
	pxor	xmm1,xmm10
	pxor	xmm2,xmm11
DB	102,15,56,0,206
DB	102,15,56,0,214
	paddd	xmm4,xmm1
	paddd	xmm5,xmm2
	pxor	xmm15,xmm4
	pxor	xmm12,xmm5
	movdqa	xmm7,xmm15
	pslld	xmm15,7
	psrld	xmm7,25
	movdqa	xmm6,xmm12
	pslld	xmm12,7
	por	xmm15,xmm7
	psrld	xmm6,25
	movdqa	xmm7,XMMWORD[r10]
	por	xmm12,xmm6
	dec	eax
	jnz	NEAR $L$oop4x

	paddd	xmm8,XMMWORD[64+rsp]
	paddd	xmm9,XMMWORD[80+rsp]
	paddd	xmm10,XMMWORD[96+rsp]
	paddd	xmm11,XMMWORD[112+rsp]

	movdqa	xmm6,xmm8
	punpckldq	xmm8,xmm9
	movdqa	xmm7,xmm10
	punpckldq	xmm10,xmm11
	punpckhdq	xmm6,xmm9
	punpckhdq	xmm7,xmm11
	movdqa	xmm9,xmm8
	punpcklqdq	xmm8,xmm10
	movdqa	xmm11,xmm6
	punpcklqdq	xmm6,xmm7
	punpckhqdq	xmm9,xmm10
	punpckhqdq	xmm11,xmm7
	paddd	xmm12,XMMWORD[((128-256))+rcx]
	paddd	xmm13,XMMWORD[((144-256))+rcx]
	paddd	xmm14,XMMWORD[((160-256))+rcx]
	paddd	xmm15,XMMWORD[((176-256))+rcx]

	movdqa	XMMWORD[rsp],xmm8
	movdqa	XMMWORD[16+rsp],xmm9
	movdqa	xmm8,XMMWORD[32+rsp]
	movdqa	xmm9,XMMWORD[48+rsp]

	movdqa	xmm10,xmm12
	punpckldq	xmm12,xmm13
	movdqa	xmm7,xmm14
	punpckldq	xmm14,xmm15
	punpckhdq	xmm10,xmm13
	punpckhdq	xmm7,xmm15
	movdqa	xmm13,xmm12
	punpcklqdq	xmm12,xmm14
	movdqa	xmm15,xmm10
	punpcklqdq	xmm10,xmm7
	punpckhqdq	xmm13,xmm14
	punpckhqdq	xmm15,xmm7
	paddd	xmm4,XMMWORD[((192-256))+rcx]
	paddd	xmm5,XMMWORD[((208-256))+rcx]
	paddd	xmm8,XMMWORD[((224-256))+rcx]
	paddd	xmm9,XMMWORD[((240-256))+rcx]

	movdqa	XMMWORD[32+rsp],xmm6
	movdqa	XMMWORD[48+rsp],xmm11

	movdqa	xmm14,xmm4
	punpckldq	xmm4,xmm5
	movdqa	xmm7,xmm8
	punpckldq	xmm8,xmm9
	punpckhdq	xmm14,xmm5
	punpckhdq	xmm7,xmm9
	movdqa	xmm5,xmm4
	punpcklqdq	xmm4,xmm8
	movdqa	xmm9,xmm14
	punpcklqdq	xmm14,xmm7
	punpckhqdq	xmm5,xmm8
	punpckhqdq	xmm9,xmm7
	paddd	xmm0,XMMWORD[((256-256))+rcx]
	paddd	xmm1,XMMWORD[((272-256))+rcx]
	paddd	xmm2,XMMWORD[((288-256))+rcx]
	paddd	xmm3,XMMWORD[((304-256))+rcx]

	movdqa	xmm8,xmm0
	punpckldq	xmm0,xmm1
	movdqa	xmm7,xmm2
	punpckldq	xmm2,xmm3
	punpckhdq	xmm8,xmm1
	punpckhdq	xmm7,xmm3
	movdqa	xmm1,xmm0
	punpcklqdq	xmm0,xmm2
	movdqa	xmm3,xmm8
	punpcklqdq	xmm8,xmm7
	punpckhqdq	xmm1,xmm2
	punpckhqdq	xmm3,xmm7
	cmp	rdx,64*4
	jb	NEAR $L$tail4x

	movdqu	xmm6,XMMWORD[rsi]
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[rsp]
	pxor	xmm11,xmm12
	pxor	xmm2,xmm4
	pxor	xmm7,xmm0

	movdqu	XMMWORD[rdi],xmm6
	movdqu	xmm6,XMMWORD[64+rsi]
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	xmm11,XMMWORD[80+rsi]
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	xmm2,XMMWORD[96+rsi]
	movdqu	XMMWORD[48+rdi],xmm7
	movdqu	xmm7,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]
	pxor	xmm6,XMMWORD[16+rsp]
	pxor	xmm11,xmm13
	pxor	xmm2,xmm5
	pxor	xmm7,xmm1

	movdqu	XMMWORD[64+rdi],xmm6
	movdqu	xmm6,XMMWORD[rsi]
	movdqu	XMMWORD[80+rdi],xmm11
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	XMMWORD[96+rdi],xmm2
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	XMMWORD[112+rdi],xmm7
	lea	rdi,[128+rdi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[32+rsp]
	pxor	xmm11,xmm10
	pxor	xmm2,xmm14
	pxor	xmm7,xmm8

	movdqu	XMMWORD[rdi],xmm6
	movdqu	xmm6,XMMWORD[64+rsi]
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	xmm11,XMMWORD[80+rsi]
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	xmm2,XMMWORD[96+rsi]
	movdqu	XMMWORD[48+rdi],xmm7
	movdqu	xmm7,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]
	pxor	xmm6,XMMWORD[48+rsp]
	pxor	xmm11,xmm15
	pxor	xmm2,xmm9
	pxor	xmm7,xmm3
	movdqu	XMMWORD[64+rdi],xmm6
	movdqu	XMMWORD[80+rdi],xmm11
	movdqu	XMMWORD[96+rdi],xmm2
	movdqu	XMMWORD[112+rdi],xmm7
	lea	rdi,[128+rdi]

	sub	rdx,64*4
	jnz	NEAR $L$oop_outer4x

	jmp	NEAR $L$done4x

$L$tail4x:
	cmp	rdx,192
	jae	NEAR $L$192_or_more4x
	cmp	rdx,128
	jae	NEAR $L$128_or_more4x
	cmp	rdx,64
	jae	NEAR $L$64_or_more4x


	xor	r10,r10

	movdqa	XMMWORD[16+rsp],xmm12
	movdqa	XMMWORD[32+rsp],xmm4
	movdqa	XMMWORD[48+rsp],xmm0
	jmp	NEAR $L$oop_tail4x

ALIGN	32
$L$64_or_more4x:
	movdqu	xmm6,XMMWORD[rsi]
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[rsp]
	pxor	xmm11,xmm12
	pxor	xmm2,xmm4
	pxor	xmm7,xmm0
	movdqu	XMMWORD[rdi],xmm6
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	XMMWORD[48+rdi],xmm7
	je	NEAR $L$done4x

	movdqa	xmm6,XMMWORD[16+rsp]
	lea	rsi,[64+rsi]
	xor	r10,r10
	movdqa	XMMWORD[rsp],xmm6
	movdqa	XMMWORD[16+rsp],xmm13
	lea	rdi,[64+rdi]
	movdqa	XMMWORD[32+rsp],xmm5
	sub	rdx,64
	movdqa	XMMWORD[48+rsp],xmm1
	jmp	NEAR $L$oop_tail4x

ALIGN	32
$L$128_or_more4x:
	movdqu	xmm6,XMMWORD[rsi]
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[rsp]
	pxor	xmm11,xmm12
	pxor	xmm2,xmm4
	pxor	xmm7,xmm0

	movdqu	XMMWORD[rdi],xmm6
	movdqu	xmm6,XMMWORD[64+rsi]
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	xmm11,XMMWORD[80+rsi]
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	xmm2,XMMWORD[96+rsi]
	movdqu	XMMWORD[48+rdi],xmm7
	movdqu	xmm7,XMMWORD[112+rsi]
	pxor	xmm6,XMMWORD[16+rsp]
	pxor	xmm11,xmm13
	pxor	xmm2,xmm5
	pxor	xmm7,xmm1
	movdqu	XMMWORD[64+rdi],xmm6
	movdqu	XMMWORD[80+rdi],xmm11
	movdqu	XMMWORD[96+rdi],xmm2
	movdqu	XMMWORD[112+rdi],xmm7
	je	NEAR $L$done4x

	movdqa	xmm6,XMMWORD[32+rsp]
	lea	rsi,[128+rsi]
	xor	r10,r10
	movdqa	XMMWORD[rsp],xmm6
	movdqa	XMMWORD[16+rsp],xmm10
	lea	rdi,[128+rdi]
	movdqa	XMMWORD[32+rsp],xmm14
	sub	rdx,128
	movdqa	XMMWORD[48+rsp],xmm8
	jmp	NEAR $L$oop_tail4x

ALIGN	32
$L$192_or_more4x:
	movdqu	xmm6,XMMWORD[rsi]
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[rsp]
	pxor	xmm11,xmm12
	pxor	xmm2,xmm4
	pxor	xmm7,xmm0

	movdqu	XMMWORD[rdi],xmm6
	movdqu	xmm6,XMMWORD[64+rsi]
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	xmm11,XMMWORD[80+rsi]
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	xmm2,XMMWORD[96+rsi]
	movdqu	XMMWORD[48+rdi],xmm7
	movdqu	xmm7,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]
	pxor	xmm6,XMMWORD[16+rsp]
	pxor	xmm11,xmm13
	pxor	xmm2,xmm5
	pxor	xmm7,xmm1

	movdqu	XMMWORD[64+rdi],xmm6
	movdqu	xmm6,XMMWORD[rsi]
	movdqu	XMMWORD[80+rdi],xmm11
	movdqu	xmm11,XMMWORD[16+rsi]
	movdqu	XMMWORD[96+rdi],xmm2
	movdqu	xmm2,XMMWORD[32+rsi]
	movdqu	XMMWORD[112+rdi],xmm7
	lea	rdi,[128+rdi]
	movdqu	xmm7,XMMWORD[48+rsi]
	pxor	xmm6,XMMWORD[32+rsp]
	pxor	xmm11,xmm10
	pxor	xmm2,xmm14
	pxor	xmm7,xmm8
	movdqu	XMMWORD[rdi],xmm6
	movdqu	XMMWORD[16+rdi],xmm11
	movdqu	XMMWORD[32+rdi],xmm2
	movdqu	XMMWORD[48+rdi],xmm7
	je	NEAR $L$done4x

	movdqa	xmm6,XMMWORD[48+rsp]
	lea	rsi,[64+rsi]
	xor	r10,r10
	movdqa	XMMWORD[rsp],xmm6
	movdqa	XMMWORD[16+rsp],xmm15
	lea	rdi,[64+rdi]
	movdqa	XMMWORD[32+rsp],xmm9
	sub	rdx,192
	movdqa	XMMWORD[48+rsp],xmm3

$L$oop_tail4x:
	movzx	eax,BYTE[r10*1+rsi]
	movzx	ecx,BYTE[r10*1+rsp]
	lea	r10,[1+r10]
	xor	eax,ecx
	mov	BYTE[((-1))+r10*1+rdi],al
	dec	rdx
	jnz	NEAR $L$oop_tail4x

$L$done4x:
	lea	r11,[((320+48))+rsp]
	movaps	xmm6,XMMWORD[((-48))+r11]
	movaps	xmm7,XMMWORD[((-32))+r11]
	movaps	xmm8,XMMWORD[((-16))+r11]
	movaps	xmm9,XMMWORD[r11]
	movaps	xmm10,XMMWORD[16+r11]
	movaps	xmm11,XMMWORD[32+r11]
	movaps	xmm12,XMMWORD[48+r11]
	movaps	xmm13,XMMWORD[64+r11]
	movaps	xmm14,XMMWORD[80+r11]
	movaps	xmm15,XMMWORD[96+r11]
	add	rsp,0x148+160
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ChaCha20_4x:

ALIGN	32
ChaCha20_4xop:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ChaCha20_4xop:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]


$L$ChaCha20_4xop:
	lea	r11,[((-120))+rsp]
	sub	rsp,0x148+160
	movaps	XMMWORD[(-48)+r11],xmm6
	movaps	XMMWORD[(-32)+r11],xmm7
	movaps	XMMWORD[(-16)+r11],xmm8
	movaps	XMMWORD[r11],xmm9
	movaps	XMMWORD[16+r11],xmm10
	movaps	XMMWORD[32+r11],xmm11
	movaps	XMMWORD[48+r11],xmm12
	movaps	XMMWORD[64+r11],xmm13
	movaps	XMMWORD[80+r11],xmm14
	movaps	XMMWORD[96+r11],xmm15
	vzeroupper

	vmovdqa	xmm11,XMMWORD[$L$sigma]
	vmovdqu	xmm3,XMMWORD[rcx]
	vmovdqu	xmm15,XMMWORD[16+rcx]
	vmovdqu	xmm7,XMMWORD[r8]
	lea	rcx,[256+rsp]

	vpshufd	xmm8,xmm11,0x00
	vpshufd	xmm9,xmm11,0x55
	vmovdqa	XMMWORD[64+rsp],xmm8
	vpshufd	xmm10,xmm11,0xaa
	vmovdqa	XMMWORD[80+rsp],xmm9
	vpshufd	xmm11,xmm11,0xff
	vmovdqa	XMMWORD[96+rsp],xmm10
	vmovdqa	XMMWORD[112+rsp],xmm11

	vpshufd	xmm0,xmm3,0x00
	vpshufd	xmm1,xmm3,0x55
	vmovdqa	XMMWORD[(128-256)+rcx],xmm0
	vpshufd	xmm2,xmm3,0xaa
	vmovdqa	XMMWORD[(144-256)+rcx],xmm1
	vpshufd	xmm3,xmm3,0xff
	vmovdqa	XMMWORD[(160-256)+rcx],xmm2
	vmovdqa	XMMWORD[(176-256)+rcx],xmm3

	vpshufd	xmm12,xmm15,0x00
	vpshufd	xmm13,xmm15,0x55
	vmovdqa	XMMWORD[(192-256)+rcx],xmm12
	vpshufd	xmm14,xmm15,0xaa
	vmovdqa	XMMWORD[(208-256)+rcx],xmm13
	vpshufd	xmm15,xmm15,0xff
	vmovdqa	XMMWORD[(224-256)+rcx],xmm14
	vmovdqa	XMMWORD[(240-256)+rcx],xmm15

	vpshufd	xmm4,xmm7,0x00
	vpshufd	xmm5,xmm7,0x55
	vpaddd	xmm4,xmm4,XMMWORD[$L$inc]
	vpshufd	xmm6,xmm7,0xaa
	vmovdqa	XMMWORD[(272-256)+rcx],xmm5
	vpshufd	xmm7,xmm7,0xff
	vmovdqa	XMMWORD[(288-256)+rcx],xmm6
	vmovdqa	XMMWORD[(304-256)+rcx],xmm7

	jmp	NEAR $L$oop_enter4xop

ALIGN	32
$L$oop_outer4xop:
	vmovdqa	xmm8,XMMWORD[64+rsp]
	vmovdqa	xmm9,XMMWORD[80+rsp]
	vmovdqa	xmm10,XMMWORD[96+rsp]
	vmovdqa	xmm11,XMMWORD[112+rsp]
	vmovdqa	xmm0,XMMWORD[((128-256))+rcx]
	vmovdqa	xmm1,XMMWORD[((144-256))+rcx]
	vmovdqa	xmm2,XMMWORD[((160-256))+rcx]
	vmovdqa	xmm3,XMMWORD[((176-256))+rcx]
	vmovdqa	xmm12,XMMWORD[((192-256))+rcx]
	vmovdqa	xmm13,XMMWORD[((208-256))+rcx]
	vmovdqa	xmm14,XMMWORD[((224-256))+rcx]
	vmovdqa	xmm15,XMMWORD[((240-256))+rcx]
	vmovdqa	xmm4,XMMWORD[((256-256))+rcx]
	vmovdqa	xmm5,XMMWORD[((272-256))+rcx]
	vmovdqa	xmm6,XMMWORD[((288-256))+rcx]
	vmovdqa	xmm7,XMMWORD[((304-256))+rcx]
	vpaddd	xmm4,xmm4,XMMWORD[$L$four]

$L$oop_enter4xop:
	mov	eax,10
	vmovdqa	XMMWORD[(256-256)+rcx],xmm4
	jmp	NEAR $L$oop4xop

ALIGN	32
$L$oop4xop:
	vpaddd	xmm8,xmm8,xmm0
	vpaddd	xmm9,xmm9,xmm1
	vpaddd	xmm10,xmm10,xmm2
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm8,xmm4
	vpxor	xmm5,xmm9,xmm5
	vpxor	xmm6,xmm10,xmm6
	vpxor	xmm7,xmm11,xmm7
DB	143,232,120,194,228,16
DB	143,232,120,194,237,16
DB	143,232,120,194,246,16
DB	143,232,120,194,255,16
	vpaddd	xmm12,xmm12,xmm4
	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm14,xmm14,xmm6
	vpaddd	xmm15,xmm15,xmm7
	vpxor	xmm0,xmm12,xmm0
	vpxor	xmm1,xmm13,xmm1
	vpxor	xmm2,xmm2,xmm14
	vpxor	xmm3,xmm3,xmm15
DB	143,232,120,194,192,12
DB	143,232,120,194,201,12
DB	143,232,120,194,210,12
DB	143,232,120,194,219,12
	vpaddd	xmm8,xmm0,xmm8
	vpaddd	xmm9,xmm1,xmm9
	vpaddd	xmm10,xmm10,xmm2
	vpaddd	xmm11,xmm11,xmm3
	vpxor	xmm4,xmm8,xmm4
	vpxor	xmm5,xmm9,xmm5
	vpxor	xmm6,xmm10,xmm6
	vpxor	xmm7,xmm11,xmm7
DB	143,232,120,194,228,8
DB	143,232,120,194,237,8
DB	143,232,120,194,246,8
DB	143,232,120,194,255,8
	vpaddd	xmm12,xmm12,xmm4
	vpaddd	xmm13,xmm13,xmm5
	vpaddd	xmm14,xmm14,xmm6
	vpaddd	xmm15,xmm15,xmm7
	vpxor	xmm0,xmm12,xmm0
	vpxor	xmm1,xmm13,xmm1
	vpxor	xmm2,xmm2,xmm14
	vpxor	xmm3,xmm3,xmm15
DB	143,232,120,194,192,7
DB	143,232,120,194,201,7
DB	143,232,120,194,210,7
DB	143,232,120,194,219,7
	vpaddd	xmm8,xmm8,xmm1
	vpaddd	xmm9,xmm9,xmm2
	vpaddd	xmm10,xmm10,xmm3
	vpaddd	xmm11,xmm11,xmm0
	vpxor	xmm7,xmm8,xmm7
	vpxor	xmm4,xmm9,xmm4
	vpxor	xmm5,xmm10,xmm5
	vpxor	xmm6,xmm11,xmm6
DB	143,232,120,194,255,16
DB	143,232,120,194,228,16
DB	143,232,120,194,237,16
DB	143,232,120,194,246,16
	vpaddd	xmm14,xmm14,xmm7
	vpaddd	xmm15,xmm15,xmm4
	vpaddd	xmm12,xmm12,xmm5
	vpaddd	xmm13,xmm13,xmm6
	vpxor	xmm1,xmm14,xmm1
	vpxor	xmm2,xmm15,xmm2
	vpxor	xmm3,xmm3,xmm12
	vpxor	xmm0,xmm0,xmm13
DB	143,232,120,194,201,12
DB	143,232,120,194,210,12
DB	143,232,120,194,219,12
DB	143,232,120,194,192,12
	vpaddd	xmm8,xmm1,xmm8
	vpaddd	xmm9,xmm2,xmm9
	vpaddd	xmm10,xmm10,xmm3
	vpaddd	xmm11,xmm11,xmm0
	vpxor	xmm7,xmm8,xmm7
	vpxor	xmm4,xmm9,xmm4
	vpxor	xmm5,xmm10,xmm5
	vpxor	xmm6,xmm11,xmm6
DB	143,232,120,194,255,8
DB	143,232,120,194,228,8
DB	143,232,120,194,237,8
DB	143,232,120,194,246,8
	vpaddd	xmm14,xmm14,xmm7
	vpaddd	xmm15,xmm15,xmm4
	vpaddd	xmm12,xmm12,xmm5
	vpaddd	xmm13,xmm13,xmm6
	vpxor	xmm1,xmm14,xmm1
	vpxor	xmm2,xmm15,xmm2
	vpxor	xmm3,xmm3,xmm12
	vpxor	xmm0,xmm0,xmm13
DB	143,232,120,194,201,7
DB	143,232,120,194,210,7
DB	143,232,120,194,219,7
DB	143,232,120,194,192,7
	dec	eax
	jnz	NEAR $L$oop4xop

	vpaddd	xmm8,xmm8,XMMWORD[64+rsp]
	vpaddd	xmm9,xmm9,XMMWORD[80+rsp]
	vpaddd	xmm10,xmm10,XMMWORD[96+rsp]
	vpaddd	xmm11,xmm11,XMMWORD[112+rsp]

	vmovdqa	XMMWORD[32+rsp],xmm14
	vmovdqa	XMMWORD[48+rsp],xmm15

	vpunpckldq	xmm14,xmm8,xmm9
	vpunpckldq	xmm15,xmm10,xmm11
	vpunpckhdq	xmm8,xmm8,xmm9
	vpunpckhdq	xmm10,xmm10,xmm11
	vpunpcklqdq	xmm9,xmm14,xmm15
	vpunpckhqdq	xmm14,xmm14,xmm15
	vpunpcklqdq	xmm11,xmm8,xmm10
	vpunpckhqdq	xmm8,xmm8,xmm10
	vpaddd	xmm0,xmm0,XMMWORD[((128-256))+rcx]
	vpaddd	xmm1,xmm1,XMMWORD[((144-256))+rcx]
	vpaddd	xmm2,xmm2,XMMWORD[((160-256))+rcx]
	vpaddd	xmm3,xmm3,XMMWORD[((176-256))+rcx]

	vmovdqa	XMMWORD[rsp],xmm9
	vmovdqa	XMMWORD[16+rsp],xmm14
	vmovdqa	xmm9,XMMWORD[32+rsp]
	vmovdqa	xmm14,XMMWORD[48+rsp]

	vpunpckldq	xmm10,xmm0,xmm1
	vpunpckldq	xmm15,xmm2,xmm3
	vpunpckhdq	xmm0,xmm0,xmm1
	vpunpckhdq	xmm2,xmm2,xmm3
	vpunpcklqdq	xmm1,xmm10,xmm15
	vpunpckhqdq	xmm10,xmm10,xmm15
	vpunpcklqdq	xmm3,xmm0,xmm2
	vpunpckhqdq	xmm0,xmm0,xmm2
	vpaddd	xmm12,xmm12,XMMWORD[((192-256))+rcx]
	vpaddd	xmm13,xmm13,XMMWORD[((208-256))+rcx]
	vpaddd	xmm9,xmm9,XMMWORD[((224-256))+rcx]
	vpaddd	xmm14,xmm14,XMMWORD[((240-256))+rcx]

	vpunpckldq	xmm2,xmm12,xmm13
	vpunpckldq	xmm15,xmm9,xmm14
	vpunpckhdq	xmm12,xmm12,xmm13
	vpunpckhdq	xmm9,xmm9,xmm14
	vpunpcklqdq	xmm13,xmm2,xmm15
	vpunpckhqdq	xmm2,xmm2,xmm15
	vpunpcklqdq	xmm14,xmm12,xmm9
	vpunpckhqdq	xmm12,xmm12,xmm9
	vpaddd	xmm4,xmm4,XMMWORD[((256-256))+rcx]
	vpaddd	xmm5,xmm5,XMMWORD[((272-256))+rcx]
	vpaddd	xmm6,xmm6,XMMWORD[((288-256))+rcx]
	vpaddd	xmm7,xmm7,XMMWORD[((304-256))+rcx]

	vpunpckldq	xmm9,xmm4,xmm5
	vpunpckldq	xmm15,xmm6,xmm7
	vpunpckhdq	xmm4,xmm4,xmm5
	vpunpckhdq	xmm6,xmm6,xmm7
	vpunpcklqdq	xmm5,xmm9,xmm15
	vpunpckhqdq	xmm9,xmm9,xmm15
	vpunpcklqdq	xmm7,xmm4,xmm6
	vpunpckhqdq	xmm4,xmm4,xmm6
	vmovdqa	xmm6,XMMWORD[rsp]
	vmovdqa	xmm15,XMMWORD[16+rsp]

	cmp	rdx,64*4
	jb	NEAR $L$tail4xop

	vpxor	xmm6,xmm6,XMMWORD[rsi]
	vpxor	xmm1,xmm1,XMMWORD[16+rsi]
	vpxor	xmm13,xmm13,XMMWORD[32+rsi]
	vpxor	xmm5,xmm5,XMMWORD[48+rsi]
	vpxor	xmm15,xmm15,XMMWORD[64+rsi]
	vpxor	xmm10,xmm10,XMMWORD[80+rsi]
	vpxor	xmm2,xmm2,XMMWORD[96+rsi]
	vpxor	xmm9,xmm9,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]
	vpxor	xmm11,xmm11,XMMWORD[rsi]
	vpxor	xmm3,xmm3,XMMWORD[16+rsi]
	vpxor	xmm14,xmm14,XMMWORD[32+rsi]
	vpxor	xmm7,xmm7,XMMWORD[48+rsi]
	vpxor	xmm8,xmm8,XMMWORD[64+rsi]
	vpxor	xmm0,xmm0,XMMWORD[80+rsi]
	vpxor	xmm12,xmm12,XMMWORD[96+rsi]
	vpxor	xmm4,xmm4,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]

	vmovdqu	XMMWORD[rdi],xmm6
	vmovdqu	XMMWORD[16+rdi],xmm1
	vmovdqu	XMMWORD[32+rdi],xmm13
	vmovdqu	XMMWORD[48+rdi],xmm5
	vmovdqu	XMMWORD[64+rdi],xmm15
	vmovdqu	XMMWORD[80+rdi],xmm10
	vmovdqu	XMMWORD[96+rdi],xmm2
	vmovdqu	XMMWORD[112+rdi],xmm9
	lea	rdi,[128+rdi]
	vmovdqu	XMMWORD[rdi],xmm11
	vmovdqu	XMMWORD[16+rdi],xmm3
	vmovdqu	XMMWORD[32+rdi],xmm14
	vmovdqu	XMMWORD[48+rdi],xmm7
	vmovdqu	XMMWORD[64+rdi],xmm8
	vmovdqu	XMMWORD[80+rdi],xmm0
	vmovdqu	XMMWORD[96+rdi],xmm12
	vmovdqu	XMMWORD[112+rdi],xmm4
	lea	rdi,[128+rdi]

	sub	rdx,64*4
	jnz	NEAR $L$oop_outer4xop

	jmp	NEAR $L$done4xop

ALIGN	32
$L$tail4xop:
	cmp	rdx,192
	jae	NEAR $L$192_or_more4xop
	cmp	rdx,128
	jae	NEAR $L$128_or_more4xop
	cmp	rdx,64
	jae	NEAR $L$64_or_more4xop

	xor	r10,r10
	vmovdqa	XMMWORD[rsp],xmm6
	vmovdqa	XMMWORD[16+rsp],xmm1
	vmovdqa	XMMWORD[32+rsp],xmm13
	vmovdqa	XMMWORD[48+rsp],xmm5
	jmp	NEAR $L$oop_tail4xop

ALIGN	32
$L$64_or_more4xop:
	vpxor	xmm6,xmm6,XMMWORD[rsi]
	vpxor	xmm1,xmm1,XMMWORD[16+rsi]
	vpxor	xmm13,xmm13,XMMWORD[32+rsi]
	vpxor	xmm5,xmm5,XMMWORD[48+rsi]
	vmovdqu	XMMWORD[rdi],xmm6
	vmovdqu	XMMWORD[16+rdi],xmm1
	vmovdqu	XMMWORD[32+rdi],xmm13
	vmovdqu	XMMWORD[48+rdi],xmm5
	je	NEAR $L$done4xop

	lea	rsi,[64+rsi]
	vmovdqa	XMMWORD[rsp],xmm15
	xor	r10,r10
	vmovdqa	XMMWORD[16+rsp],xmm10
	lea	rdi,[64+rdi]
	vmovdqa	XMMWORD[32+rsp],xmm2
	sub	rdx,64
	vmovdqa	XMMWORD[48+rsp],xmm9
	jmp	NEAR $L$oop_tail4xop

ALIGN	32
$L$128_or_more4xop:
	vpxor	xmm6,xmm6,XMMWORD[rsi]
	vpxor	xmm1,xmm1,XMMWORD[16+rsi]
	vpxor	xmm13,xmm13,XMMWORD[32+rsi]
	vpxor	xmm5,xmm5,XMMWORD[48+rsi]
	vpxor	xmm15,xmm15,XMMWORD[64+rsi]
	vpxor	xmm10,xmm10,XMMWORD[80+rsi]
	vpxor	xmm2,xmm2,XMMWORD[96+rsi]
	vpxor	xmm9,xmm9,XMMWORD[112+rsi]

	vmovdqu	XMMWORD[rdi],xmm6
	vmovdqu	XMMWORD[16+rdi],xmm1
	vmovdqu	XMMWORD[32+rdi],xmm13
	vmovdqu	XMMWORD[48+rdi],xmm5
	vmovdqu	XMMWORD[64+rdi],xmm15
	vmovdqu	XMMWORD[80+rdi],xmm10
	vmovdqu	XMMWORD[96+rdi],xmm2
	vmovdqu	XMMWORD[112+rdi],xmm9
	je	NEAR $L$done4xop

	lea	rsi,[128+rsi]
	vmovdqa	XMMWORD[rsp],xmm11
	xor	r10,r10
	vmovdqa	XMMWORD[16+rsp],xmm3
	lea	rdi,[128+rdi]
	vmovdqa	XMMWORD[32+rsp],xmm14
	sub	rdx,128
	vmovdqa	XMMWORD[48+rsp],xmm7
	jmp	NEAR $L$oop_tail4xop

ALIGN	32
$L$192_or_more4xop:
	vpxor	xmm6,xmm6,XMMWORD[rsi]
	vpxor	xmm1,xmm1,XMMWORD[16+rsi]
	vpxor	xmm13,xmm13,XMMWORD[32+rsi]
	vpxor	xmm5,xmm5,XMMWORD[48+rsi]
	vpxor	xmm15,xmm15,XMMWORD[64+rsi]
	vpxor	xmm10,xmm10,XMMWORD[80+rsi]
	vpxor	xmm2,xmm2,XMMWORD[96+rsi]
	vpxor	xmm9,xmm9,XMMWORD[112+rsi]
	lea	rsi,[128+rsi]
	vpxor	xmm11,xmm11,XMMWORD[rsi]
	vpxor	xmm3,xmm3,XMMWORD[16+rsi]
	vpxor	xmm14,xmm14,XMMWORD[32+rsi]
	vpxor	xmm7,xmm7,XMMWORD[48+rsi]

	vmovdqu	XMMWORD[rdi],xmm6
	vmovdqu	XMMWORD[16+rdi],xmm1
	vmovdqu	XMMWORD[32+rdi],xmm13
	vmovdqu	XMMWORD[48+rdi],xmm5
	vmovdqu	XMMWORD[64+rdi],xmm15
	vmovdqu	XMMWORD[80+rdi],xmm10
	vmovdqu	XMMWORD[96+rdi],xmm2
	vmovdqu	XMMWORD[112+rdi],xmm9
	lea	rdi,[128+rdi]
	vmovdqu	XMMWORD[rdi],xmm11
	vmovdqu	XMMWORD[16+rdi],xmm3
	vmovdqu	XMMWORD[32+rdi],xmm14
	vmovdqu	XMMWORD[48+rdi],xmm7
	je	NEAR $L$done4xop

	lea	rsi,[64+rsi]
	vmovdqa	XMMWORD[rsp],xmm8
	xor	r10,r10
	vmovdqa	XMMWORD[16+rsp],xmm0
	lea	rdi,[64+rdi]
	vmovdqa	XMMWORD[32+rsp],xmm12
	sub	rdx,192
	vmovdqa	XMMWORD[48+rsp],xmm4

$L$oop_tail4xop:
	movzx	eax,BYTE[r10*1+rsi]
	movzx	ecx,BYTE[r10*1+rsp]
	lea	r10,[1+r10]
	xor	eax,ecx
	mov	BYTE[((-1))+r10*1+rdi],al
	dec	rdx
	jnz	NEAR $L$oop_tail4xop

$L$done4xop:
	vzeroupper
	lea	r11,[((320+48))+rsp]
	movaps	xmm6,XMMWORD[((-48))+r11]
	movaps	xmm7,XMMWORD[((-32))+r11]
	movaps	xmm8,XMMWORD[((-16))+r11]
	movaps	xmm9,XMMWORD[r11]
	movaps	xmm10,XMMWORD[16+r11]
	movaps	xmm11,XMMWORD[32+r11]
	movaps	xmm12,XMMWORD[48+r11]
	movaps	xmm13,XMMWORD[64+r11]
	movaps	xmm14,XMMWORD[80+r11]
	movaps	xmm15,XMMWORD[96+r11]
	add	rsp,0x148+160
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ChaCha20_4xop:

ALIGN	32
ChaCha20_8x:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ChaCha20_8x:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD[40+rsp]


$L$ChaCha20_8x:
	mov	r10,rsp
	sub	rsp,0x280+176
	and	rsp,-32
	lea	r11,[((656+48))+rsp]
	movaps	XMMWORD[(-48)+r11],xmm6
	movaps	XMMWORD[(-32)+r11],xmm7
	movaps	XMMWORD[(-16)+r11],xmm8
	movaps	XMMWORD[r11],xmm9
	movaps	XMMWORD[16+r11],xmm10
	movaps	XMMWORD[32+r11],xmm11
	movaps	XMMWORD[48+r11],xmm12
	movaps	XMMWORD[64+r11],xmm13
	movaps	XMMWORD[80+r11],xmm14
	movaps	XMMWORD[96+r11],xmm15
	vzeroupper
	mov	QWORD[640+rsp],r10










	vbroadcasti128	ymm11,XMMWORD[$L$sigma]
	vbroadcasti128	ymm3,XMMWORD[rcx]
	vbroadcasti128	ymm15,XMMWORD[16+rcx]
	vbroadcasti128	ymm7,XMMWORD[r8]
	lea	rcx,[256+rsp]
	lea	rax,[512+rsp]
	lea	r10,[$L$rot16]
	lea	r11,[$L$rot24]

	vpshufd	ymm8,ymm11,0x00
	vpshufd	ymm9,ymm11,0x55
	vmovdqa	YMMWORD[(128-256)+rcx],ymm8
	vpshufd	ymm10,ymm11,0xaa
	vmovdqa	YMMWORD[(160-256)+rcx],ymm9
	vpshufd	ymm11,ymm11,0xff
	vmovdqa	YMMWORD[(192-256)+rcx],ymm10
	vmovdqa	YMMWORD[(224-256)+rcx],ymm11

	vpshufd	ymm0,ymm3,0x00
	vpshufd	ymm1,ymm3,0x55
	vmovdqa	YMMWORD[(256-256)+rcx],ymm0
	vpshufd	ymm2,ymm3,0xaa
	vmovdqa	YMMWORD[(288-256)+rcx],ymm1
	vpshufd	ymm3,ymm3,0xff
	vmovdqa	YMMWORD[(320-256)+rcx],ymm2
	vmovdqa	YMMWORD[(352-256)+rcx],ymm3

	vpshufd	ymm12,ymm15,0x00
	vpshufd	ymm13,ymm15,0x55
	vmovdqa	YMMWORD[(384-512)+rax],ymm12
	vpshufd	ymm14,ymm15,0xaa
	vmovdqa	YMMWORD[(416-512)+rax],ymm13
	vpshufd	ymm15,ymm15,0xff
	vmovdqa	YMMWORD[(448-512)+rax],ymm14
	vmovdqa	YMMWORD[(480-512)+rax],ymm15

	vpshufd	ymm4,ymm7,0x00
	vpshufd	ymm5,ymm7,0x55
	vpaddd	ymm4,ymm4,YMMWORD[$L$incy]
	vpshufd	ymm6,ymm7,0xaa
	vmovdqa	YMMWORD[(544-512)+rax],ymm5
	vpshufd	ymm7,ymm7,0xff
	vmovdqa	YMMWORD[(576-512)+rax],ymm6
	vmovdqa	YMMWORD[(608-512)+rax],ymm7

	jmp	NEAR $L$oop_enter8x

ALIGN	32
$L$oop_outer8x:
	vmovdqa	ymm8,YMMWORD[((128-256))+rcx]
	vmovdqa	ymm9,YMMWORD[((160-256))+rcx]
	vmovdqa	ymm10,YMMWORD[((192-256))+rcx]
	vmovdqa	ymm11,YMMWORD[((224-256))+rcx]
	vmovdqa	ymm0,YMMWORD[((256-256))+rcx]
	vmovdqa	ymm1,YMMWORD[((288-256))+rcx]
	vmovdqa	ymm2,YMMWORD[((320-256))+rcx]
	vmovdqa	ymm3,YMMWORD[((352-256))+rcx]
	vmovdqa	ymm12,YMMWORD[((384-512))+rax]
	vmovdqa	ymm13,YMMWORD[((416-512))+rax]
	vmovdqa	ymm14,YMMWORD[((448-512))+rax]
	vmovdqa	ymm15,YMMWORD[((480-512))+rax]
	vmovdqa	ymm4,YMMWORD[((512-512))+rax]
	vmovdqa	ymm5,YMMWORD[((544-512))+rax]
	vmovdqa	ymm6,YMMWORD[((576-512))+rax]
	vmovdqa	ymm7,YMMWORD[((608-512))+rax]
	vpaddd	ymm4,ymm4,YMMWORD[$L$eight]

$L$oop_enter8x:
	vmovdqa	YMMWORD[64+rsp],ymm14
	vmovdqa	YMMWORD[96+rsp],ymm15
	vbroadcasti128	ymm15,XMMWORD[r10]
	vmovdqa	YMMWORD[(512-512)+rax],ymm4
	mov	eax,10
	jmp	NEAR $L$oop8x

ALIGN	32
$L$oop8x:
	vpaddd	ymm8,ymm8,ymm0
	vpxor	ymm4,ymm8,ymm4
	vpshufb	ymm4,ymm4,ymm15
	vpaddd	ymm9,ymm9,ymm1
	vpxor	ymm5,ymm9,ymm5
	vpshufb	ymm5,ymm5,ymm15
	vpaddd	ymm12,ymm12,ymm4
	vpxor	ymm0,ymm12,ymm0
	vpslld	ymm14,ymm0,12
	vpsrld	ymm0,ymm0,20
	vpor	ymm0,ymm14,ymm0
	vbroadcasti128	ymm14,XMMWORD[r11]
	vpaddd	ymm13,ymm13,ymm5
	vpxor	ymm1,ymm13,ymm1
	vpslld	ymm15,ymm1,12
	vpsrld	ymm1,ymm1,20
	vpor	ymm1,ymm15,ymm1
	vpaddd	ymm8,ymm8,ymm0
	vpxor	ymm4,ymm8,ymm4
	vpshufb	ymm4,ymm4,ymm14
	vpaddd	ymm9,ymm9,ymm1
	vpxor	ymm5,ymm9,ymm5
	vpshufb	ymm5,ymm5,ymm14
	vpaddd	ymm12,ymm12,ymm4
	vpxor	ymm0,ymm12,ymm0
	vpslld	ymm15,ymm0,7
	vpsrld	ymm0,ymm0,25
	vpor	ymm0,ymm15,ymm0
	vbroadcasti128	ymm15,XMMWORD[r10]
	vpaddd	ymm13,ymm13,ymm5
	vpxor	ymm1,ymm13,ymm1
	vpslld	ymm14,ymm1,7
	vpsrld	ymm1,ymm1,25
	vpor	ymm1,ymm14,ymm1
	vmovdqa	YMMWORD[rsp],ymm12
	vmovdqa	YMMWORD[32+rsp],ymm13
	vmovdqa	ymm12,YMMWORD[64+rsp]
	vmovdqa	ymm13,YMMWORD[96+rsp]
	vpaddd	ymm10,ymm10,ymm2
	vpxor	ymm6,ymm10,ymm6
	vpshufb	ymm6,ymm6,ymm15
	vpaddd	ymm11,ymm11,ymm3
	vpxor	ymm7,ymm11,ymm7
	vpshufb	ymm7,ymm7,ymm15
	vpaddd	ymm12,ymm12,ymm6
	vpxor	ymm2,ymm12,ymm2
	vpslld	ymm14,ymm2,12
	vpsrld	ymm2,ymm2,20
	vpor	ymm2,ymm14,ymm2
	vbroadcasti128	ymm14,XMMWORD[r11]
	vpaddd	ymm13,ymm13,ymm7
	vpxor	ymm3,ymm13,ymm3
	vpslld	ymm15,ymm3,12
	vpsrld	ymm3,ymm3,20
	vpor	ymm3,ymm15,ymm3
	vpaddd	ymm10,ymm10,ymm2
	vpxor	ymm6,ymm10,ymm6
	vpshufb	ymm6,ymm6,ymm14
	vpaddd	ymm11,ymm11,ymm3
	vpxor	ymm7,ymm11,ymm7
	vpshufb	ymm7,ymm7,ymm14
	vpaddd	ymm12,ymm12,ymm6
	vpxor	ymm2,ymm12,ymm2
	vpslld	ymm15,ymm2,7
	vpsrld	ymm2,ymm2,25
	vpor	ymm2,ymm15,ymm2
	vbroadcasti128	ymm15,XMMWORD[r10]
	vpaddd	ymm13,ymm13,ymm7
	vpxor	ymm3,ymm13,ymm3
	vpslld	ymm14,ymm3,7
	vpsrld	ymm3,ymm3,25
	vpor	ymm3,ymm14,ymm3
	vpaddd	ymm8,ymm8,ymm1
	vpxor	ymm7,ymm8,ymm7
	vpshufb	ymm7,ymm7,ymm15
	vpaddd	ymm9,ymm9,ymm2
	vpxor	ymm4,ymm9,ymm4
	vpshufb	ymm4,ymm4,ymm15
	vpaddd	ymm12,ymm12,ymm7
	vpxor	ymm1,ymm12,ymm1
	vpslld	ymm14,ymm1,12
	vpsrld	ymm1,ymm1,20
	vpor	ymm1,ymm14,ymm1
	vbroadcasti128	ymm14,XMMWORD[r11]
	vpaddd	ymm13,ymm13,ymm4
	vpxor	ymm2,ymm13,ymm2
	vpslld	ymm15,ymm2,12
	vpsrld	ymm2,ymm2,20
	vpor	ymm2,ymm15,ymm2
	vpaddd	ymm8,ymm8,ymm1
	vpxor	ymm7,ymm8,ymm7
	vpshufb	ymm7,ymm7,ymm14
	vpaddd	ymm9,ymm9,ymm2
	vpxor	ymm4,ymm9,ymm4
	vpshufb	ymm4,ymm4,ymm14
	vpaddd	ymm12,ymm12,ymm7
	vpxor	ymm1,ymm12,ymm1
	vpslld	ymm15,ymm1,7
	vpsrld	ymm1,ymm1,25
	vpor	ymm1,ymm15,ymm1
	vbroadcasti128	ymm15,XMMWORD[r10]
	vpaddd	ymm13,ymm13,ymm4
	vpxor	ymm2,ymm13,ymm2
	vpslld	ymm14,ymm2,7
	vpsrld	ymm2,ymm2,25
	vpor	ymm2,ymm14,ymm2
	vmovdqa	YMMWORD[64+rsp],ymm12
	vmovdqa	YMMWORD[96+rsp],ymm13
	vmovdqa	ymm12,YMMWORD[rsp]
	vmovdqa	ymm13,YMMWORD[32+rsp]
	vpaddd	ymm10,ymm10,ymm3
	vpxor	ymm5,ymm10,ymm5
	vpshufb	ymm5,ymm5,ymm15
	vpaddd	ymm11,ymm11,ymm0
	vpxor	ymm6,ymm11,ymm6
	vpshufb	ymm6,ymm6,ymm15
	vpaddd	ymm12,ymm12,ymm5
	vpxor	ymm3,ymm12,ymm3
	vpslld	ymm14,ymm3,12
	vpsrld	ymm3,ymm3,20
	vpor	ymm3,ymm14,ymm3
	vbroadcasti128	ymm14,XMMWORD[r11]
	vpaddd	ymm13,ymm13,ymm6
	vpxor	ymm0,ymm13,ymm0
	vpslld	ymm15,ymm0,12
	vpsrld	ymm0,ymm0,20
	vpor	ymm0,ymm15,ymm0
	vpaddd	ymm10,ymm10,ymm3
	vpxor	ymm5,ymm10,ymm5
	vpshufb	ymm5,ymm5,ymm14
	vpaddd	ymm11,ymm11,ymm0
	vpxor	ymm6,ymm11,ymm6
	vpshufb	ymm6,ymm6,ymm14
	vpaddd	ymm12,ymm12,ymm5
	vpxor	ymm3,ymm12,ymm3
	vpslld	ymm15,ymm3,7
	vpsrld	ymm3,ymm3,25
	vpor	ymm3,ymm15,ymm3
	vbroadcasti128	ymm15,XMMWORD[r10]
	vpaddd	ymm13,ymm13,ymm6
	vpxor	ymm0,ymm13,ymm0
	vpslld	ymm14,ymm0,7
	vpsrld	ymm0,ymm0,25
	vpor	ymm0,ymm14,ymm0
	dec	eax
	jnz	NEAR $L$oop8x

	lea	rax,[512+rsp]
	vpaddd	ymm8,ymm8,YMMWORD[((128-256))+rcx]
	vpaddd	ymm9,ymm9,YMMWORD[((160-256))+rcx]
	vpaddd	ymm10,ymm10,YMMWORD[((192-256))+rcx]
	vpaddd	ymm11,ymm11,YMMWORD[((224-256))+rcx]

	vpunpckldq	ymm14,ymm8,ymm9
	vpunpckldq	ymm15,ymm10,ymm11
	vpunpckhdq	ymm8,ymm8,ymm9
	vpunpckhdq	ymm10,ymm10,ymm11
	vpunpcklqdq	ymm9,ymm14,ymm15
	vpunpckhqdq	ymm14,ymm14,ymm15
	vpunpcklqdq	ymm11,ymm8,ymm10
	vpunpckhqdq	ymm8,ymm8,ymm10
	vpaddd	ymm0,ymm0,YMMWORD[((256-256))+rcx]
	vpaddd	ymm1,ymm1,YMMWORD[((288-256))+rcx]
	vpaddd	ymm2,ymm2,YMMWORD[((320-256))+rcx]
	vpaddd	ymm3,ymm3,YMMWORD[((352-256))+rcx]

	vpunpckldq	ymm10,ymm0,ymm1
	vpunpckldq	ymm15,ymm2,ymm3
	vpunpckhdq	ymm0,ymm0,ymm1
	vpunpckhdq	ymm2,ymm2,ymm3
	vpunpcklqdq	ymm1,ymm10,ymm15
	vpunpckhqdq	ymm10,ymm10,ymm15
	vpunpcklqdq	ymm3,ymm0,ymm2
	vpunpckhqdq	ymm0,ymm0,ymm2
	vperm2i128	ymm15,ymm9,ymm1,0x20
	vperm2i128	ymm1,ymm9,ymm1,0x31
	vperm2i128	ymm9,ymm14,ymm10,0x20
	vperm2i128	ymm10,ymm14,ymm10,0x31
	vperm2i128	ymm14,ymm11,ymm3,0x20
	vperm2i128	ymm3,ymm11,ymm3,0x31
	vperm2i128	ymm11,ymm8,ymm0,0x20
	vperm2i128	ymm0,ymm8,ymm0,0x31
	vmovdqa	YMMWORD[rsp],ymm15
	vmovdqa	YMMWORD[32+rsp],ymm9
	vmovdqa	ymm15,YMMWORD[64+rsp]
	vmovdqa	ymm9,YMMWORD[96+rsp]

	vpaddd	ymm12,ymm12,YMMWORD[((384-512))+rax]
	vpaddd	ymm13,ymm13,YMMWORD[((416-512))+rax]
	vpaddd	ymm15,ymm15,YMMWORD[((448-512))+rax]
	vpaddd	ymm9,ymm9,YMMWORD[((480-512))+rax]

	vpunpckldq	ymm2,ymm12,ymm13
	vpunpckldq	ymm8,ymm15,ymm9
	vpunpckhdq	ymm12,ymm12,ymm13
	vpunpckhdq	ymm15,ymm15,ymm9
	vpunpcklqdq	ymm13,ymm2,ymm8
	vpunpckhqdq	ymm2,ymm2,ymm8
	vpunpcklqdq	ymm9,ymm12,ymm15
	vpunpckhqdq	ymm12,ymm12,ymm15
	vpaddd	ymm4,ymm4,YMMWORD[((512-512))+rax]
	vpaddd	ymm5,ymm5,YMMWORD[((544-512))+rax]
	vpaddd	ymm6,ymm6,YMMWORD[((576-512))+rax]
	vpaddd	ymm7,ymm7,YMMWORD[((608-512))+rax]

	vpunpckldq	ymm15,ymm4,ymm5
	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm4,ymm4,ymm5
	vpunpckhdq	ymm6,ymm6,ymm7
	vpunpcklqdq	ymm5,ymm15,ymm8
	vpunpckhqdq	ymm15,ymm15,ymm8
	vpunpcklqdq	ymm7,ymm4,ymm6
	vpunpckhqdq	ymm4,ymm4,ymm6
	vperm2i128	ymm8,ymm13,ymm5,0x20
	vperm2i128	ymm5,ymm13,ymm5,0x31
	vperm2i128	ymm13,ymm2,ymm15,0x20
	vperm2i128	ymm15,ymm2,ymm15,0x31
	vperm2i128	ymm2,ymm9,ymm7,0x20
	vperm2i128	ymm7,ymm9,ymm7,0x31
	vperm2i128	ymm9,ymm12,ymm4,0x20
	vperm2i128	ymm4,ymm12,ymm4,0x31
	vmovdqa	ymm6,YMMWORD[rsp]
	vmovdqa	ymm12,YMMWORD[32+rsp]

	cmp	rdx,64*8
	jb	NEAR $L$tail8x

	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	lea	rsi,[128+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	lea	rdi,[128+rdi]

	vpxor	ymm12,ymm12,YMMWORD[rsi]
	vpxor	ymm13,ymm13,YMMWORD[32+rsi]
	vpxor	ymm10,ymm10,YMMWORD[64+rsi]
	vpxor	ymm15,ymm15,YMMWORD[96+rsi]
	lea	rsi,[128+rsi]
	vmovdqu	YMMWORD[rdi],ymm12
	vmovdqu	YMMWORD[32+rdi],ymm13
	vmovdqu	YMMWORD[64+rdi],ymm10
	vmovdqu	YMMWORD[96+rdi],ymm15
	lea	rdi,[128+rdi]

	vpxor	ymm14,ymm14,YMMWORD[rsi]
	vpxor	ymm2,ymm2,YMMWORD[32+rsi]
	vpxor	ymm3,ymm3,YMMWORD[64+rsi]
	vpxor	ymm7,ymm7,YMMWORD[96+rsi]
	lea	rsi,[128+rsi]
	vmovdqu	YMMWORD[rdi],ymm14
	vmovdqu	YMMWORD[32+rdi],ymm2
	vmovdqu	YMMWORD[64+rdi],ymm3
	vmovdqu	YMMWORD[96+rdi],ymm7
	lea	rdi,[128+rdi]

	vpxor	ymm11,ymm11,YMMWORD[rsi]
	vpxor	ymm9,ymm9,YMMWORD[32+rsi]
	vpxor	ymm0,ymm0,YMMWORD[64+rsi]
	vpxor	ymm4,ymm4,YMMWORD[96+rsi]
	lea	rsi,[128+rsi]
	vmovdqu	YMMWORD[rdi],ymm11
	vmovdqu	YMMWORD[32+rdi],ymm9
	vmovdqu	YMMWORD[64+rdi],ymm0
	vmovdqu	YMMWORD[96+rdi],ymm4
	lea	rdi,[128+rdi]

	sub	rdx,64*8
	jnz	NEAR $L$oop_outer8x

	jmp	NEAR $L$done8x

$L$tail8x:
	cmp	rdx,448
	jae	NEAR $L$448_or_more8x
	cmp	rdx,384
	jae	NEAR $L$384_or_more8x
	cmp	rdx,320
	jae	NEAR $L$320_or_more8x
	cmp	rdx,256
	jae	NEAR $L$256_or_more8x
	cmp	rdx,192
	jae	NEAR $L$192_or_more8x
	cmp	rdx,128
	jae	NEAR $L$128_or_more8x
	cmp	rdx,64
	jae	NEAR $L$64_or_more8x

	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm6
	vmovdqa	YMMWORD[32+rsp],ymm8
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$64_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	je	NEAR $L$done8x

	lea	rsi,[64+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm1
	lea	rdi,[64+rdi]
	sub	rdx,64
	vmovdqa	YMMWORD[32+rsp],ymm5
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$128_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	je	NEAR $L$done8x

	lea	rsi,[128+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm12
	lea	rdi,[128+rdi]
	sub	rdx,128
	vmovdqa	YMMWORD[32+rsp],ymm13
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$192_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vpxor	ymm12,ymm12,YMMWORD[128+rsi]
	vpxor	ymm13,ymm13,YMMWORD[160+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	vmovdqu	YMMWORD[128+rdi],ymm12
	vmovdqu	YMMWORD[160+rdi],ymm13
	je	NEAR $L$done8x

	lea	rsi,[192+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm10
	lea	rdi,[192+rdi]
	sub	rdx,192
	vmovdqa	YMMWORD[32+rsp],ymm15
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$256_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vpxor	ymm12,ymm12,YMMWORD[128+rsi]
	vpxor	ymm13,ymm13,YMMWORD[160+rsi]
	vpxor	ymm10,ymm10,YMMWORD[192+rsi]
	vpxor	ymm15,ymm15,YMMWORD[224+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	vmovdqu	YMMWORD[128+rdi],ymm12
	vmovdqu	YMMWORD[160+rdi],ymm13
	vmovdqu	YMMWORD[192+rdi],ymm10
	vmovdqu	YMMWORD[224+rdi],ymm15
	je	NEAR $L$done8x

	lea	rsi,[256+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm14
	lea	rdi,[256+rdi]
	sub	rdx,256
	vmovdqa	YMMWORD[32+rsp],ymm2
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$320_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vpxor	ymm12,ymm12,YMMWORD[128+rsi]
	vpxor	ymm13,ymm13,YMMWORD[160+rsi]
	vpxor	ymm10,ymm10,YMMWORD[192+rsi]
	vpxor	ymm15,ymm15,YMMWORD[224+rsi]
	vpxor	ymm14,ymm14,YMMWORD[256+rsi]
	vpxor	ymm2,ymm2,YMMWORD[288+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	vmovdqu	YMMWORD[128+rdi],ymm12
	vmovdqu	YMMWORD[160+rdi],ymm13
	vmovdqu	YMMWORD[192+rdi],ymm10
	vmovdqu	YMMWORD[224+rdi],ymm15
	vmovdqu	YMMWORD[256+rdi],ymm14
	vmovdqu	YMMWORD[288+rdi],ymm2
	je	NEAR $L$done8x

	lea	rsi,[320+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm3
	lea	rdi,[320+rdi]
	sub	rdx,320
	vmovdqa	YMMWORD[32+rsp],ymm7
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$384_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vpxor	ymm12,ymm12,YMMWORD[128+rsi]
	vpxor	ymm13,ymm13,YMMWORD[160+rsi]
	vpxor	ymm10,ymm10,YMMWORD[192+rsi]
	vpxor	ymm15,ymm15,YMMWORD[224+rsi]
	vpxor	ymm14,ymm14,YMMWORD[256+rsi]
	vpxor	ymm2,ymm2,YMMWORD[288+rsi]
	vpxor	ymm3,ymm3,YMMWORD[320+rsi]
	vpxor	ymm7,ymm7,YMMWORD[352+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	vmovdqu	YMMWORD[128+rdi],ymm12
	vmovdqu	YMMWORD[160+rdi],ymm13
	vmovdqu	YMMWORD[192+rdi],ymm10
	vmovdqu	YMMWORD[224+rdi],ymm15
	vmovdqu	YMMWORD[256+rdi],ymm14
	vmovdqu	YMMWORD[288+rdi],ymm2
	vmovdqu	YMMWORD[320+rdi],ymm3
	vmovdqu	YMMWORD[352+rdi],ymm7
	je	NEAR $L$done8x

	lea	rsi,[384+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm11
	lea	rdi,[384+rdi]
	sub	rdx,384
	vmovdqa	YMMWORD[32+rsp],ymm9
	jmp	NEAR $L$oop_tail8x

ALIGN	32
$L$448_or_more8x:
	vpxor	ymm6,ymm6,YMMWORD[rsi]
	vpxor	ymm8,ymm8,YMMWORD[32+rsi]
	vpxor	ymm1,ymm1,YMMWORD[64+rsi]
	vpxor	ymm5,ymm5,YMMWORD[96+rsi]
	vpxor	ymm12,ymm12,YMMWORD[128+rsi]
	vpxor	ymm13,ymm13,YMMWORD[160+rsi]
	vpxor	ymm10,ymm10,YMMWORD[192+rsi]
	vpxor	ymm15,ymm15,YMMWORD[224+rsi]
	vpxor	ymm14,ymm14,YMMWORD[256+rsi]
	vpxor	ymm2,ymm2,YMMWORD[288+rsi]
	vpxor	ymm3,ymm3,YMMWORD[320+rsi]
	vpxor	ymm7,ymm7,YMMWORD[352+rsi]
	vpxor	ymm11,ymm11,YMMWORD[384+rsi]
	vpxor	ymm9,ymm9,YMMWORD[416+rsi]
	vmovdqu	YMMWORD[rdi],ymm6
	vmovdqu	YMMWORD[32+rdi],ymm8
	vmovdqu	YMMWORD[64+rdi],ymm1
	vmovdqu	YMMWORD[96+rdi],ymm5
	vmovdqu	YMMWORD[128+rdi],ymm12
	vmovdqu	YMMWORD[160+rdi],ymm13
	vmovdqu	YMMWORD[192+rdi],ymm10
	vmovdqu	YMMWORD[224+rdi],ymm15
	vmovdqu	YMMWORD[256+rdi],ymm14
	vmovdqu	YMMWORD[288+rdi],ymm2
	vmovdqu	YMMWORD[320+rdi],ymm3
	vmovdqu	YMMWORD[352+rdi],ymm7
	vmovdqu	YMMWORD[384+rdi],ymm11
	vmovdqu	YMMWORD[416+rdi],ymm9
	je	NEAR $L$done8x

	lea	rsi,[448+rsi]
	xor	r10,r10
	vmovdqa	YMMWORD[rsp],ymm0
	lea	rdi,[448+rdi]
	sub	rdx,448
	vmovdqa	YMMWORD[32+rsp],ymm4

$L$oop_tail8x:
	movzx	eax,BYTE[r10*1+rsi]
	movzx	ecx,BYTE[r10*1+rsp]
	lea	r10,[1+r10]
	xor	eax,ecx
	mov	BYTE[((-1))+r10*1+rdi],al
	dec	rdx
	jnz	NEAR $L$oop_tail8x

$L$done8x:
	vzeroall
	lea	r11,[((656+48))+rsp]
	movaps	xmm6,XMMWORD[((-48))+r11]
	movaps	xmm7,XMMWORD[((-32))+r11]
	movaps	xmm8,XMMWORD[((-16))+r11]
	movaps	xmm9,XMMWORD[r11]
	movaps	xmm10,XMMWORD[16+r11]
	movaps	xmm11,XMMWORD[32+r11]
	movaps	xmm12,XMMWORD[48+r11]
	movaps	xmm13,XMMWORD[64+r11]
	movaps	xmm14,XMMWORD[80+r11]
	movaps	xmm15,XMMWORD[96+r11]
	mov	rsp,QWORD[640+rsp]
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_ChaCha20_8x:
