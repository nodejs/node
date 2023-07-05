default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64



ALIGN	16
_mul_1x1:

	sub	rsp,128+8

	mov	r9,-1
	lea	rsi,[rax*1+rax]
	shr	r9,3
	lea	rdi,[rax*4]
	and	r9,rax
	lea	r12,[rax*8]
	sar	rax,63
	lea	r10,[r9*1+r9]
	sar	rsi,63
	lea	r11,[r9*4]
	and	rax,rbp
	sar	rdi,63
	mov	rdx,rax
	shl	rax,63
	and	rsi,rbp
	shr	rdx,1
	mov	rcx,rsi
	shl	rsi,62
	and	rdi,rbp
	shr	rcx,2
	xor	rax,rsi
	mov	rbx,rdi
	shl	rdi,61
	xor	rdx,rcx
	shr	rbx,3
	xor	rax,rdi
	xor	rdx,rbx

	mov	r13,r9
	mov	QWORD[rsp],0
	xor	r13,r10
	mov	QWORD[8+rsp],r9
	mov	r14,r11
	mov	QWORD[16+rsp],r10
	xor	r14,r12
	mov	QWORD[24+rsp],r13

	xor	r9,r11
	mov	QWORD[32+rsp],r11
	xor	r10,r11
	mov	QWORD[40+rsp],r9
	xor	r13,r11
	mov	QWORD[48+rsp],r10
	xor	r9,r14
	mov	QWORD[56+rsp],r13
	xor	r10,r14

	mov	QWORD[64+rsp],r12
	xor	r13,r14
	mov	QWORD[72+rsp],r9
	xor	r9,r11
	mov	QWORD[80+rsp],r10
	xor	r10,r11
	mov	QWORD[88+rsp],r13

	xor	r13,r11
	mov	QWORD[96+rsp],r14
	mov	rsi,r8
	mov	QWORD[104+rsp],r9
	and	rsi,rbp
	mov	QWORD[112+rsp],r10
	shr	rbp,4
	mov	QWORD[120+rsp],r13
	mov	rdi,r8
	and	rdi,rbp
	shr	rbp,4

	movq	xmm0,QWORD[rsi*8+rsp]
	mov	rsi,r8
	and	rsi,rbp
	shr	rbp,4
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,4
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,60
	xor	rax,rcx
	pslldq	xmm1,1
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,12
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,52
	xor	rax,rcx
	pslldq	xmm1,2
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,20
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,44
	xor	rax,rcx
	pslldq	xmm1,3
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,28
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,36
	xor	rax,rcx
	pslldq	xmm1,4
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,36
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,28
	xor	rax,rcx
	pslldq	xmm1,5
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,44
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,20
	xor	rax,rcx
	pslldq	xmm1,6
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rdi,r8
	mov	rbx,rcx
	shl	rcx,52
	and	rdi,rbp
	movq	xmm1,QWORD[rsi*8+rsp]
	shr	rbx,12
	xor	rax,rcx
	pslldq	xmm1,7
	mov	rsi,r8
	shr	rbp,4
	xor	rdx,rbx
	and	rsi,rbp
	shr	rbp,4
	pxor	xmm0,xmm1
	mov	rcx,QWORD[rdi*8+rsp]
	mov	rbx,rcx
	shl	rcx,60
DB	102,72,15,126,198
	shr	rbx,4
	xor	rax,rcx
	psrldq	xmm0,8
	xor	rdx,rbx
DB	102,72,15,126,199
	xor	rax,rsi
	xor	rdx,rdi

	add	rsp,128+8

	DB	0F3h,0C3h		;repret
$L$end_mul_1x1:


EXTERN	OPENSSL_ia32cap_P
global	bn_GF2m_mul_2x2

ALIGN	16
bn_GF2m_mul_2x2:

	mov	rax,rsp
	mov	r10,QWORD[OPENSSL_ia32cap_P]
	bt	r10,33
	jnc	NEAR $L$vanilla_mul_2x2

DB	102,72,15,110,194
DB	102,73,15,110,201
DB	102,73,15,110,208
	movq	xmm3,QWORD[40+rsp]
	movdqa	xmm4,xmm0
	movdqa	xmm5,xmm1
DB	102,15,58,68,193,0
	pxor	xmm4,xmm2
	pxor	xmm5,xmm3
DB	102,15,58,68,211,0
DB	102,15,58,68,229,0
	xorps	xmm4,xmm0
	xorps	xmm4,xmm2
	movdqa	xmm5,xmm4
	pslldq	xmm4,8
	psrldq	xmm5,8
	pxor	xmm2,xmm4
	pxor	xmm0,xmm5
	movdqu	XMMWORD[rcx],xmm2
	movdqu	XMMWORD[16+rcx],xmm0
	DB	0F3h,0C3h		;repret

ALIGN	16
$L$vanilla_mul_2x2:
	lea	rsp,[((-136))+rsp]

	mov	r10,QWORD[176+rsp]
	mov	QWORD[120+rsp],rdi
	mov	QWORD[128+rsp],rsi
	mov	QWORD[80+rsp],r14

	mov	QWORD[88+rsp],r13

	mov	QWORD[96+rsp],r12

	mov	QWORD[104+rsp],rbp

	mov	QWORD[112+rsp],rbx

$L$body_mul_2x2:
	mov	QWORD[32+rsp],rcx
	mov	QWORD[40+rsp],rdx
	mov	QWORD[48+rsp],r8
	mov	QWORD[56+rsp],r9
	mov	QWORD[64+rsp],r10

	mov	r8,0xf
	mov	rax,rdx
	mov	rbp,r9
	call	_mul_1x1
	mov	QWORD[16+rsp],rax
	mov	QWORD[24+rsp],rdx

	mov	rax,QWORD[48+rsp]
	mov	rbp,QWORD[64+rsp]
	call	_mul_1x1
	mov	QWORD[rsp],rax
	mov	QWORD[8+rsp],rdx

	mov	rax,QWORD[40+rsp]
	mov	rbp,QWORD[56+rsp]
	xor	rax,QWORD[48+rsp]
	xor	rbp,QWORD[64+rsp]
	call	_mul_1x1
	mov	rbx,QWORD[rsp]
	mov	rcx,QWORD[8+rsp]
	mov	rdi,QWORD[16+rsp]
	mov	rsi,QWORD[24+rsp]
	mov	rbp,QWORD[32+rsp]

	xor	rax,rdx
	xor	rdx,rcx
	xor	rax,rbx
	mov	QWORD[rbp],rbx
	xor	rdx,rdi
	mov	QWORD[24+rbp],rsi
	xor	rax,rsi
	xor	rdx,rsi
	xor	rax,rdx
	mov	QWORD[16+rbp],rdx
	mov	QWORD[8+rbp],rax

	mov	r14,QWORD[80+rsp]

	mov	r13,QWORD[88+rsp]

	mov	r12,QWORD[96+rsp]

	mov	rbp,QWORD[104+rsp]

	mov	rbx,QWORD[112+rsp]

	mov	rdi,QWORD[120+rsp]
	mov	rsi,QWORD[128+rsp]
	lea	rsp,[136+rsp]

$L$epilogue_mul_2x2:
	DB	0F3h,0C3h		;repret
$L$end_mul_2x2:


DB	71,70,40,50,94,109,41,32,77,117,108,116,105,112,108,105
DB	99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54
DB	52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121
DB	32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46
DB	111,114,103,62,0
ALIGN	16
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

	lea	r10,[$L$body_mul_2x2]
	cmp	rbx,r10
	jb	NEAR $L$in_prologue

	mov	rax,QWORD[152+r8]

	lea	r10,[$L$epilogue_mul_2x2]
	cmp	rbx,r10
	jae	NEAR $L$in_prologue

	mov	r14,QWORD[80+rax]
	mov	r13,QWORD[88+rax]
	mov	r12,QWORD[96+rax]
	mov	rbp,QWORD[104+rax]
	mov	rbx,QWORD[112+rax]
	mov	rdi,QWORD[120+rax]
	mov	rsi,QWORD[128+rax]

	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14

	lea	rax,[136+rax]

$L$in_prologue:
	mov	QWORD[152+r8],rax

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


section	.pdata rdata align=4
ALIGN	4
	DD	_mul_1x1 wrt ..imagebase
	DD	$L$end_mul_1x1 wrt ..imagebase
	DD	$L$SEH_info_1x1 wrt ..imagebase

	DD	$L$vanilla_mul_2x2 wrt ..imagebase
	DD	$L$end_mul_2x2 wrt ..imagebase
	DD	$L$SEH_info_2x2 wrt ..imagebase
section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_1x1:
DB	0x01,0x07,0x02,0x00
DB	0x07,0x01,0x11,0x00
$L$SEH_info_2x2:
DB	9,0,0,0
	DD	se_handler wrt ..imagebase
