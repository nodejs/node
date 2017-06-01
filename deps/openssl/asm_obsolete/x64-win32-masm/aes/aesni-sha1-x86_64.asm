OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR

PUBLIC	aesni_cbc_sha1_enc

ALIGN	32
aesni_cbc_sha1_enc	PROC PUBLIC

	mov	r10d,DWORD PTR[((OPENSSL_ia32cap_P+0))]
	mov	r11,QWORD PTR[((OPENSSL_ia32cap_P+4))]
	bt	r11,61
	jc	aesni_cbc_sha1_enc_shaext
	jmp	aesni_cbc_sha1_enc_ssse3
	DB	0F3h,0C3h		;repret
aesni_cbc_sha1_enc	ENDP

ALIGN	32
aesni_cbc_sha1_enc_ssse3	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha1_enc_ssse3::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	r10,QWORD PTR[56+rsp]


	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	lea	rsp,QWORD PTR[((-264))+rsp]


	movaps	XMMWORD PTR[(96+0)+rsp],xmm6
	movaps	XMMWORD PTR[(96+16)+rsp],xmm7
	movaps	XMMWORD PTR[(96+32)+rsp],xmm8
	movaps	XMMWORD PTR[(96+48)+rsp],xmm9
	movaps	XMMWORD PTR[(96+64)+rsp],xmm10
	movaps	XMMWORD PTR[(96+80)+rsp],xmm11
	movaps	XMMWORD PTR[(96+96)+rsp],xmm12
	movaps	XMMWORD PTR[(96+112)+rsp],xmm13
	movaps	XMMWORD PTR[(96+128)+rsp],xmm14
	movaps	XMMWORD PTR[(96+144)+rsp],xmm15
$L$prologue_ssse3::
	mov	r12,rdi
	mov	r13,rsi
	mov	r14,rdx
	lea	r15,QWORD PTR[112+rcx]
	movdqu	xmm2,XMMWORD PTR[r8]
	mov	QWORD PTR[88+rsp],r8
	shl	r14,6
	sub	r13,r12
	mov	r8d,DWORD PTR[((240-112))+r15]
	add	r14,r10

	lea	r11,QWORD PTR[K_XX_XX]
	mov	eax,DWORD PTR[r9]
	mov	ebx,DWORD PTR[4+r9]
	mov	ecx,DWORD PTR[8+r9]
	mov	edx,DWORD PTR[12+r9]
	mov	esi,ebx
	mov	ebp,DWORD PTR[16+r9]
	mov	edi,ecx
	xor	edi,edx
	and	esi,edi

	movdqa	xmm3,XMMWORD PTR[64+r11]
	movdqa	xmm13,XMMWORD PTR[r11]
	movdqu	xmm4,XMMWORD PTR[r10]
	movdqu	xmm5,XMMWORD PTR[16+r10]
	movdqu	xmm6,XMMWORD PTR[32+r10]
	movdqu	xmm7,XMMWORD PTR[48+r10]
DB	102,15,56,0,227
DB	102,15,56,0,235
DB	102,15,56,0,243
	add	r10,64
	paddd	xmm4,xmm13
DB	102,15,56,0,251
	paddd	xmm5,xmm13
	paddd	xmm6,xmm13
	movdqa	XMMWORD PTR[rsp],xmm4
	psubd	xmm4,xmm13
	movdqa	XMMWORD PTR[16+rsp],xmm5
	psubd	xmm5,xmm13
	movdqa	XMMWORD PTR[32+rsp],xmm6
	psubd	xmm6,xmm13
	movups	xmm15,XMMWORD PTR[((-112))+r15]
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	jmp	$L$oop_ssse3
ALIGN	32
$L$oop_ssse3::
	ror	ebx,2
	movups	xmm14,XMMWORD PTR[r12]
	xorps	xmm14,xmm15
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+r15]
DB	102,15,56,220,208
	pshufd	xmm8,xmm4,238
	xor	esi,edx
	movdqa	xmm12,xmm7
	paddd	xmm13,xmm7
	mov	edi,eax
	add	ebp,DWORD PTR[rsp]
	punpcklqdq	xmm8,xmm5
	xor	ebx,ecx
	rol	eax,5
	add	ebp,esi
	psrldq	xmm12,4
	and	edi,ebx
	xor	ebx,ecx
	pxor	xmm8,xmm4
	add	ebp,eax
	ror	eax,7
	pxor	xmm12,xmm6
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[4+rsp]
	pxor	xmm8,xmm12
	xor	eax,ebx
	rol	ebp,5
	movdqa	XMMWORD PTR[48+rsp],xmm13
	add	edx,edi
	movups	xmm0,XMMWORD PTR[((-64))+r15]
DB	102,15,56,220,209
	and	esi,eax
	movdqa	xmm3,xmm8
	xor	eax,ebx
	add	edx,ebp
	ror	ebp,7
	movdqa	xmm12,xmm8
	xor	esi,ebx
	pslldq	xmm3,12
	paddd	xmm8,xmm8
	mov	edi,edx
	add	ecx,DWORD PTR[8+rsp]
	psrld	xmm12,31
	xor	ebp,eax
	rol	edx,5
	add	ecx,esi
	movdqa	xmm13,xmm3
	and	edi,ebp
	xor	ebp,eax
	psrld	xmm3,30
	add	ecx,edx
	ror	edx,7
	por	xmm8,xmm12
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[12+rsp]
	movups	xmm1,XMMWORD PTR[((-48))+r15]
DB	102,15,56,220,208
	pslld	xmm13,2
	pxor	xmm8,xmm3
	xor	edx,ebp
	movdqa	xmm3,XMMWORD PTR[r11]
	rol	ecx,5
	add	ebx,edi
	and	esi,edx
	pxor	xmm8,xmm13
	xor	edx,ebp
	add	ebx,ecx
	ror	ecx,7
	pshufd	xmm9,xmm5,238
	xor	esi,ebp
	movdqa	xmm13,xmm8
	paddd	xmm3,xmm8
	mov	edi,ebx
	add	eax,DWORD PTR[16+rsp]
	punpcklqdq	xmm9,xmm6
	xor	ecx,edx
	rol	ebx,5
	add	eax,esi
	psrldq	xmm13,4
	and	edi,ecx
	xor	ecx,edx
	pxor	xmm9,xmm5
	add	eax,ebx
	ror	ebx,7
	movups	xmm0,XMMWORD PTR[((-32))+r15]
DB	102,15,56,220,209
	pxor	xmm13,xmm7
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[20+rsp]
	pxor	xmm9,xmm13
	xor	ebx,ecx
	rol	eax,5
	movdqa	XMMWORD PTR[rsp],xmm3
	add	ebp,edi
	and	esi,ebx
	movdqa	xmm12,xmm9
	xor	ebx,ecx
	add	ebp,eax
	ror	eax,7
	movdqa	xmm13,xmm9
	xor	esi,ecx
	pslldq	xmm12,12
	paddd	xmm9,xmm9
	mov	edi,ebp
	add	edx,DWORD PTR[24+rsp]
	psrld	xmm13,31
	xor	eax,ebx
	rol	ebp,5
	add	edx,esi
	movups	xmm1,XMMWORD PTR[((-16))+r15]
DB	102,15,56,220,208
	movdqa	xmm3,xmm12
	and	edi,eax
	xor	eax,ebx
	psrld	xmm12,30
	add	edx,ebp
	ror	ebp,7
	por	xmm9,xmm13
	xor	edi,ebx
	mov	esi,edx
	add	ecx,DWORD PTR[28+rsp]
	pslld	xmm3,2
	pxor	xmm9,xmm12
	xor	ebp,eax
	movdqa	xmm12,XMMWORD PTR[16+r11]
	rol	edx,5
	add	ecx,edi
	and	esi,ebp
	pxor	xmm9,xmm3
	xor	ebp,eax
	add	ecx,edx
	ror	edx,7
	pshufd	xmm10,xmm6,238
	xor	esi,eax
	movdqa	xmm3,xmm9
	paddd	xmm12,xmm9
	mov	edi,ecx
	add	ebx,DWORD PTR[32+rsp]
	movups	xmm0,XMMWORD PTR[r15]
DB	102,15,56,220,209
	punpcklqdq	xmm10,xmm7
	xor	edx,ebp
	rol	ecx,5
	add	ebx,esi
	psrldq	xmm3,4
	and	edi,edx
	xor	edx,ebp
	pxor	xmm10,xmm6
	add	ebx,ecx
	ror	ecx,7
	pxor	xmm3,xmm8
	xor	edi,ebp
	mov	esi,ebx
	add	eax,DWORD PTR[36+rsp]
	pxor	xmm10,xmm3
	xor	ecx,edx
	rol	ebx,5
	movdqa	XMMWORD PTR[16+rsp],xmm12
	add	eax,edi
	and	esi,ecx
	movdqa	xmm13,xmm10
	xor	ecx,edx
	add	eax,ebx
	ror	ebx,7
	movups	xmm1,XMMWORD PTR[16+r15]
DB	102,15,56,220,208
	movdqa	xmm3,xmm10
	xor	esi,edx
	pslldq	xmm13,12
	paddd	xmm10,xmm10
	mov	edi,eax
	add	ebp,DWORD PTR[40+rsp]
	psrld	xmm3,31
	xor	ebx,ecx
	rol	eax,5
	add	ebp,esi
	movdqa	xmm12,xmm13
	and	edi,ebx
	xor	ebx,ecx
	psrld	xmm13,30
	add	ebp,eax
	ror	eax,7
	por	xmm10,xmm3
	xor	edi,ecx
	mov	esi,ebp
	add	edx,DWORD PTR[44+rsp]
	pslld	xmm12,2
	pxor	xmm10,xmm13
	xor	eax,ebx
	movdqa	xmm13,XMMWORD PTR[16+r11]
	rol	ebp,5
	add	edx,edi
	movups	xmm0,XMMWORD PTR[32+r15]
DB	102,15,56,220,209
	and	esi,eax
	pxor	xmm10,xmm12
	xor	eax,ebx
	add	edx,ebp
	ror	ebp,7
	pshufd	xmm11,xmm7,238
	xor	esi,ebx
	movdqa	xmm12,xmm10
	paddd	xmm13,xmm10
	mov	edi,edx
	add	ecx,DWORD PTR[48+rsp]
	punpcklqdq	xmm11,xmm8
	xor	ebp,eax
	rol	edx,5
	add	ecx,esi
	psrldq	xmm12,4
	and	edi,ebp
	xor	ebp,eax
	pxor	xmm11,xmm7
	add	ecx,edx
	ror	edx,7
	pxor	xmm12,xmm9
	xor	edi,eax
	mov	esi,ecx
	add	ebx,DWORD PTR[52+rsp]
	movups	xmm1,XMMWORD PTR[48+r15]
DB	102,15,56,220,208
	pxor	xmm11,xmm12
	xor	edx,ebp
	rol	ecx,5
	movdqa	XMMWORD PTR[32+rsp],xmm13
	add	ebx,edi
	and	esi,edx
	movdqa	xmm3,xmm11
	xor	edx,ebp
	add	ebx,ecx
	ror	ecx,7
	movdqa	xmm12,xmm11
	xor	esi,ebp
	pslldq	xmm3,12
	paddd	xmm11,xmm11
	mov	edi,ebx
	add	eax,DWORD PTR[56+rsp]
	psrld	xmm12,31
	xor	ecx,edx
	rol	ebx,5
	add	eax,esi
	movdqa	xmm13,xmm3
	and	edi,ecx
	xor	ecx,edx
	psrld	xmm3,30
	add	eax,ebx
	ror	ebx,7
	cmp	r8d,11
	jb	$L$aesenclast1
	movups	xmm0,XMMWORD PTR[64+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+r15]
DB	102,15,56,220,208
	je	$L$aesenclast1
	movups	xmm0,XMMWORD PTR[96+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+r15]
DB	102,15,56,220,208
$L$aesenclast1::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	por	xmm11,xmm12
	xor	edi,edx
	mov	esi,eax
	add	ebp,DWORD PTR[60+rsp]
	pslld	xmm13,2
	pxor	xmm11,xmm3
	xor	ebx,ecx
	movdqa	xmm3,XMMWORD PTR[16+r11]
	rol	eax,5
	add	ebp,edi
	and	esi,ebx
	pxor	xmm11,xmm13
	pshufd	xmm13,xmm10,238
	xor	ebx,ecx
	add	ebp,eax
	ror	eax,7
	pxor	xmm4,xmm8
	xor	esi,ecx
	mov	edi,ebp
	add	edx,DWORD PTR[rsp]
	punpcklqdq	xmm13,xmm11
	xor	eax,ebx
	rol	ebp,5
	pxor	xmm4,xmm5
	add	edx,esi
	movups	xmm14,XMMWORD PTR[16+r12]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[r13*1+r12],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+r15]
DB	102,15,56,220,208
	and	edi,eax
	movdqa	xmm12,xmm3
	xor	eax,ebx
	paddd	xmm3,xmm11
	add	edx,ebp
	pxor	xmm4,xmm13
	ror	ebp,7
	xor	edi,ebx
	mov	esi,edx
	add	ecx,DWORD PTR[4+rsp]
	movdqa	xmm13,xmm4
	xor	ebp,eax
	rol	edx,5
	movdqa	XMMWORD PTR[48+rsp],xmm3
	add	ecx,edi
	and	esi,ebp
	xor	ebp,eax
	pslld	xmm4,2
	add	ecx,edx
	ror	edx,7
	psrld	xmm13,30
	xor	esi,eax
	mov	edi,ecx
	add	ebx,DWORD PTR[8+rsp]
	movups	xmm0,XMMWORD PTR[((-64))+r15]
DB	102,15,56,220,209
	por	xmm4,xmm13
	xor	edx,ebp
	rol	ecx,5
	pshufd	xmm3,xmm11,238
	add	ebx,esi
	and	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[12+rsp]
	xor	edi,ebp
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	add	eax,ebx
	pxor	xmm5,xmm9
	add	ebp,DWORD PTR[16+rsp]
	movups	xmm1,XMMWORD PTR[((-48))+r15]
DB	102,15,56,220,208
	xor	esi,ecx
	punpcklqdq	xmm3,xmm4
	mov	edi,eax
	rol	eax,5
	pxor	xmm5,xmm6
	add	ebp,esi
	xor	edi,ecx
	movdqa	xmm13,xmm12
	ror	ebx,7
	paddd	xmm12,xmm4
	add	ebp,eax
	pxor	xmm5,xmm3
	add	edx,DWORD PTR[20+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm3,xmm5
	add	edx,edi
	xor	esi,ebx
	movdqa	XMMWORD PTR[rsp],xmm12
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[24+rsp]
	pslld	xmm5,2
	xor	esi,eax
	mov	edi,edx
	psrld	xmm3,30
	rol	edx,5
	add	ecx,esi
	movups	xmm0,XMMWORD PTR[((-32))+r15]
DB	102,15,56,220,209
	xor	edi,eax
	ror	ebp,7
	por	xmm5,xmm3
	add	ecx,edx
	add	ebx,DWORD PTR[28+rsp]
	pshufd	xmm12,xmm4,238
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	pxor	xmm6,xmm10
	add	eax,DWORD PTR[32+rsp]
	xor	esi,edx
	punpcklqdq	xmm12,xmm5
	mov	edi,ebx
	rol	ebx,5
	pxor	xmm6,xmm7
	add	eax,esi
	xor	edi,edx
	movdqa	xmm3,XMMWORD PTR[32+r11]
	ror	ecx,7
	paddd	xmm13,xmm5
	add	eax,ebx
	pxor	xmm6,xmm12
	add	ebp,DWORD PTR[36+rsp]
	movups	xmm1,XMMWORD PTR[((-16))+r15]
DB	102,15,56,220,208
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	movdqa	xmm12,xmm6
	add	ebp,edi
	xor	esi,ecx
	movdqa	XMMWORD PTR[16+rsp],xmm13
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[40+rsp]
	pslld	xmm6,2
	xor	esi,ebx
	mov	edi,ebp
	psrld	xmm12,30
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	por	xmm6,xmm12
	add	edx,ebp
	add	ecx,DWORD PTR[44+rsp]
	pshufd	xmm13,xmm5,238
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	movups	xmm0,XMMWORD PTR[r15]
DB	102,15,56,220,209
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	pxor	xmm7,xmm11
	add	ebx,DWORD PTR[48+rsp]
	xor	esi,ebp
	punpcklqdq	xmm13,xmm6
	mov	edi,ecx
	rol	ecx,5
	pxor	xmm7,xmm8
	add	ebx,esi
	xor	edi,ebp
	movdqa	xmm12,xmm3
	ror	edx,7
	paddd	xmm3,xmm6
	add	ebx,ecx
	pxor	xmm7,xmm13
	add	eax,DWORD PTR[52+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	movdqa	xmm13,xmm7
	add	eax,edi
	xor	esi,edx
	movdqa	XMMWORD PTR[32+rsp],xmm3
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[56+rsp]
	movups	xmm1,XMMWORD PTR[16+r15]
DB	102,15,56,220,208
	pslld	xmm7,2
	xor	esi,ecx
	mov	edi,eax
	psrld	xmm13,30
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	por	xmm7,xmm13
	add	ebp,eax
	add	edx,DWORD PTR[60+rsp]
	pshufd	xmm3,xmm6,238
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	pxor	xmm8,xmm4
	add	ecx,DWORD PTR[rsp]
	xor	esi,eax
	punpcklqdq	xmm3,xmm7
	mov	edi,edx
	rol	edx,5
	pxor	xmm8,xmm9
	add	ecx,esi
	movups	xmm0,XMMWORD PTR[32+r15]
DB	102,15,56,220,209
	xor	edi,eax
	movdqa	xmm13,xmm12
	ror	ebp,7
	paddd	xmm12,xmm7
	add	ecx,edx
	pxor	xmm8,xmm3
	add	ebx,DWORD PTR[4+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	movdqa	xmm3,xmm8
	add	ebx,edi
	xor	esi,ebp
	movdqa	XMMWORD PTR[48+rsp],xmm12
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[8+rsp]
	pslld	xmm8,2
	xor	esi,edx
	mov	edi,ebx
	psrld	xmm3,30
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	por	xmm8,xmm3
	add	eax,ebx
	add	ebp,DWORD PTR[12+rsp]
	movups	xmm1,XMMWORD PTR[48+r15]
DB	102,15,56,220,208
	pshufd	xmm12,xmm7,238
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	pxor	xmm9,xmm5
	add	edx,DWORD PTR[16+rsp]
	xor	esi,ebx
	punpcklqdq	xmm12,xmm8
	mov	edi,ebp
	rol	ebp,5
	pxor	xmm9,xmm10
	add	edx,esi
	xor	edi,ebx
	movdqa	xmm3,xmm13
	ror	eax,7
	paddd	xmm13,xmm8
	add	edx,ebp
	pxor	xmm9,xmm12
	add	ecx,DWORD PTR[20+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	movdqa	xmm12,xmm9
	add	ecx,edi
	cmp	r8d,11
	jb	$L$aesenclast2
	movups	xmm0,XMMWORD PTR[64+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+r15]
DB	102,15,56,220,208
	je	$L$aesenclast2
	movups	xmm0,XMMWORD PTR[96+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+r15]
DB	102,15,56,220,208
$L$aesenclast2::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	xor	esi,eax
	movdqa	XMMWORD PTR[rsp],xmm13
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[24+rsp]
	pslld	xmm9,2
	xor	esi,ebp
	mov	edi,ecx
	psrld	xmm12,30
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	por	xmm9,xmm12
	add	ebx,ecx
	add	eax,DWORD PTR[28+rsp]
	pshufd	xmm13,xmm8,238
	ror	ecx,7
	mov	esi,ebx
	xor	edi,edx
	rol	ebx,5
	add	eax,edi
	xor	esi,ecx
	xor	ecx,edx
	add	eax,ebx
	pxor	xmm10,xmm6
	add	ebp,DWORD PTR[32+rsp]
	movups	xmm14,XMMWORD PTR[32+r12]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[16+r12*1+r13],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+r15]
DB	102,15,56,220,208
	and	esi,ecx
	xor	ecx,edx
	ror	ebx,7
	punpcklqdq	xmm13,xmm9
	mov	edi,eax
	xor	esi,ecx
	pxor	xmm10,xmm11
	rol	eax,5
	add	ebp,esi
	movdqa	xmm12,xmm3
	xor	edi,ebx
	paddd	xmm3,xmm9
	xor	ebx,ecx
	pxor	xmm10,xmm13
	add	ebp,eax
	add	edx,DWORD PTR[36+rsp]
	and	edi,ebx
	xor	ebx,ecx
	ror	eax,7
	movdqa	xmm13,xmm10
	mov	esi,ebp
	xor	edi,ebx
	movdqa	XMMWORD PTR[16+rsp],xmm3
	rol	ebp,5
	add	edx,edi
	movups	xmm0,XMMWORD PTR[((-64))+r15]
DB	102,15,56,220,209
	xor	esi,eax
	pslld	xmm10,2
	xor	eax,ebx
	add	edx,ebp
	psrld	xmm13,30
	add	ecx,DWORD PTR[40+rsp]
	and	esi,eax
	xor	eax,ebx
	por	xmm10,xmm13
	ror	ebp,7
	mov	edi,edx
	xor	esi,eax
	rol	edx,5
	pshufd	xmm3,xmm9,238
	add	ecx,esi
	xor	edi,ebp
	xor	ebp,eax
	add	ecx,edx
	add	ebx,DWORD PTR[44+rsp]
	and	edi,ebp
	xor	ebp,eax
	ror	edx,7
	movups	xmm1,XMMWORD PTR[((-48))+r15]
DB	102,15,56,220,208
	mov	esi,ecx
	xor	edi,ebp
	rol	ecx,5
	add	ebx,edi
	xor	esi,edx
	xor	edx,ebp
	add	ebx,ecx
	pxor	xmm11,xmm7
	add	eax,DWORD PTR[48+rsp]
	and	esi,edx
	xor	edx,ebp
	ror	ecx,7
	punpcklqdq	xmm3,xmm10
	mov	edi,ebx
	xor	esi,edx
	pxor	xmm11,xmm4
	rol	ebx,5
	add	eax,esi
	movdqa	xmm13,XMMWORD PTR[48+r11]
	xor	edi,ecx
	paddd	xmm12,xmm10
	xor	ecx,edx
	pxor	xmm11,xmm3
	add	eax,ebx
	add	ebp,DWORD PTR[52+rsp]
	movups	xmm0,XMMWORD PTR[((-32))+r15]
DB	102,15,56,220,209
	and	edi,ecx
	xor	ecx,edx
	ror	ebx,7
	movdqa	xmm3,xmm11
	mov	esi,eax
	xor	edi,ecx
	movdqa	XMMWORD PTR[32+rsp],xmm12
	rol	eax,5
	add	ebp,edi
	xor	esi,ebx
	pslld	xmm11,2
	xor	ebx,ecx
	add	ebp,eax
	psrld	xmm3,30
	add	edx,DWORD PTR[56+rsp]
	and	esi,ebx
	xor	ebx,ecx
	por	xmm11,xmm3
	ror	eax,7
	mov	edi,ebp
	xor	esi,ebx
	rol	ebp,5
	pshufd	xmm12,xmm10,238
	add	edx,esi
	movups	xmm1,XMMWORD PTR[((-16))+r15]
DB	102,15,56,220,208
	xor	edi,eax
	xor	eax,ebx
	add	edx,ebp
	add	ecx,DWORD PTR[60+rsp]
	and	edi,eax
	xor	eax,ebx
	ror	ebp,7
	mov	esi,edx
	xor	edi,eax
	rol	edx,5
	add	ecx,edi
	xor	esi,ebp
	xor	ebp,eax
	add	ecx,edx
	pxor	xmm4,xmm8
	add	ebx,DWORD PTR[rsp]
	and	esi,ebp
	xor	ebp,eax
	ror	edx,7
	movups	xmm0,XMMWORD PTR[r15]
DB	102,15,56,220,209
	punpcklqdq	xmm12,xmm11
	mov	edi,ecx
	xor	esi,ebp
	pxor	xmm4,xmm5
	rol	ecx,5
	add	ebx,esi
	movdqa	xmm3,xmm13
	xor	edi,edx
	paddd	xmm13,xmm11
	xor	edx,ebp
	pxor	xmm4,xmm12
	add	ebx,ecx
	add	eax,DWORD PTR[4+rsp]
	and	edi,edx
	xor	edx,ebp
	ror	ecx,7
	movdqa	xmm12,xmm4
	mov	esi,ebx
	xor	edi,edx
	movdqa	XMMWORD PTR[48+rsp],xmm13
	rol	ebx,5
	add	eax,edi
	xor	esi,ecx
	pslld	xmm4,2
	xor	ecx,edx
	add	eax,ebx
	psrld	xmm12,30
	add	ebp,DWORD PTR[8+rsp]
	movups	xmm1,XMMWORD PTR[16+r15]
DB	102,15,56,220,208
	and	esi,ecx
	xor	ecx,edx
	por	xmm4,xmm12
	ror	ebx,7
	mov	edi,eax
	xor	esi,ecx
	rol	eax,5
	pshufd	xmm13,xmm11,238
	add	ebp,esi
	xor	edi,ebx
	xor	ebx,ecx
	add	ebp,eax
	add	edx,DWORD PTR[12+rsp]
	and	edi,ebx
	xor	ebx,ecx
	ror	eax,7
	mov	esi,ebp
	xor	edi,ebx
	rol	ebp,5
	add	edx,edi
	movups	xmm0,XMMWORD PTR[32+r15]
DB	102,15,56,220,209
	xor	esi,eax
	xor	eax,ebx
	add	edx,ebp
	pxor	xmm5,xmm9
	add	ecx,DWORD PTR[16+rsp]
	and	esi,eax
	xor	eax,ebx
	ror	ebp,7
	punpcklqdq	xmm13,xmm4
	mov	edi,edx
	xor	esi,eax
	pxor	xmm5,xmm6
	rol	edx,5
	add	ecx,esi
	movdqa	xmm12,xmm3
	xor	edi,ebp
	paddd	xmm3,xmm4
	xor	ebp,eax
	pxor	xmm5,xmm13
	add	ecx,edx
	add	ebx,DWORD PTR[20+rsp]
	and	edi,ebp
	xor	ebp,eax
	ror	edx,7
	movups	xmm1,XMMWORD PTR[48+r15]
DB	102,15,56,220,208
	movdqa	xmm13,xmm5
	mov	esi,ecx
	xor	edi,ebp
	movdqa	XMMWORD PTR[rsp],xmm3
	rol	ecx,5
	add	ebx,edi
	xor	esi,edx
	pslld	xmm5,2
	xor	edx,ebp
	add	ebx,ecx
	psrld	xmm13,30
	add	eax,DWORD PTR[24+rsp]
	and	esi,edx
	xor	edx,ebp
	por	xmm5,xmm13
	ror	ecx,7
	mov	edi,ebx
	xor	esi,edx
	rol	ebx,5
	pshufd	xmm3,xmm4,238
	add	eax,esi
	xor	edi,ecx
	xor	ecx,edx
	add	eax,ebx
	add	ebp,DWORD PTR[28+rsp]
	cmp	r8d,11
	jb	$L$aesenclast3
	movups	xmm0,XMMWORD PTR[64+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+r15]
DB	102,15,56,220,208
	je	$L$aesenclast3
	movups	xmm0,XMMWORD PTR[96+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+r15]
DB	102,15,56,220,208
$L$aesenclast3::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	and	edi,ecx
	xor	ecx,edx
	ror	ebx,7
	mov	esi,eax
	xor	edi,ecx
	rol	eax,5
	add	ebp,edi
	xor	esi,ebx
	xor	ebx,ecx
	add	ebp,eax
	pxor	xmm6,xmm10
	add	edx,DWORD PTR[32+rsp]
	and	esi,ebx
	xor	ebx,ecx
	ror	eax,7
	punpcklqdq	xmm3,xmm5
	mov	edi,ebp
	xor	esi,ebx
	pxor	xmm6,xmm7
	rol	ebp,5
	add	edx,esi
	movups	xmm14,XMMWORD PTR[48+r12]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[32+r12*1+r13],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+r15]
DB	102,15,56,220,208
	movdqa	xmm13,xmm12
	xor	edi,eax
	paddd	xmm12,xmm5
	xor	eax,ebx
	pxor	xmm6,xmm3
	add	edx,ebp
	add	ecx,DWORD PTR[36+rsp]
	and	edi,eax
	xor	eax,ebx
	ror	ebp,7
	movdqa	xmm3,xmm6
	mov	esi,edx
	xor	edi,eax
	movdqa	XMMWORD PTR[16+rsp],xmm12
	rol	edx,5
	add	ecx,edi
	xor	esi,ebp
	pslld	xmm6,2
	xor	ebp,eax
	add	ecx,edx
	psrld	xmm3,30
	add	ebx,DWORD PTR[40+rsp]
	and	esi,ebp
	xor	ebp,eax
	por	xmm6,xmm3
	ror	edx,7
	movups	xmm0,XMMWORD PTR[((-64))+r15]
DB	102,15,56,220,209
	mov	edi,ecx
	xor	esi,ebp
	rol	ecx,5
	pshufd	xmm12,xmm5,238
	add	ebx,esi
	xor	edi,edx
	xor	edx,ebp
	add	ebx,ecx
	add	eax,DWORD PTR[44+rsp]
	and	edi,edx
	xor	edx,ebp
	ror	ecx,7
	mov	esi,ebx
	xor	edi,edx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	add	eax,ebx
	pxor	xmm7,xmm11
	add	ebp,DWORD PTR[48+rsp]
	movups	xmm1,XMMWORD PTR[((-48))+r15]
DB	102,15,56,220,208
	xor	esi,ecx
	punpcklqdq	xmm12,xmm6
	mov	edi,eax
	rol	eax,5
	pxor	xmm7,xmm8
	add	ebp,esi
	xor	edi,ecx
	movdqa	xmm3,xmm13
	ror	ebx,7
	paddd	xmm13,xmm6
	add	ebp,eax
	pxor	xmm7,xmm12
	add	edx,DWORD PTR[52+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	movdqa	xmm12,xmm7
	add	edx,edi
	xor	esi,ebx
	movdqa	XMMWORD PTR[32+rsp],xmm13
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[56+rsp]
	pslld	xmm7,2
	xor	esi,eax
	mov	edi,edx
	psrld	xmm12,30
	rol	edx,5
	add	ecx,esi
	movups	xmm0,XMMWORD PTR[((-32))+r15]
DB	102,15,56,220,209
	xor	edi,eax
	ror	ebp,7
	por	xmm7,xmm12
	add	ecx,edx
	add	ebx,DWORD PTR[60+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	paddd	xmm3,xmm7
	add	eax,esi
	xor	edi,edx
	movdqa	XMMWORD PTR[48+rsp],xmm3
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[4+rsp]
	movups	xmm1,XMMWORD PTR[((-16))+r15]
DB	102,15,56,220,208
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[8+rsp]
	xor	esi,ebx
	mov	edi,ebp
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[12+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	movups	xmm0,XMMWORD PTR[r15]
DB	102,15,56,220,209
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	cmp	r10,r14
	je	$L$done_ssse3
	movdqa	xmm3,XMMWORD PTR[64+r11]
	movdqa	xmm13,XMMWORD PTR[r11]
	movdqu	xmm4,XMMWORD PTR[r10]
	movdqu	xmm5,XMMWORD PTR[16+r10]
	movdqu	xmm6,XMMWORD PTR[32+r10]
	movdqu	xmm7,XMMWORD PTR[48+r10]
DB	102,15,56,0,227
	add	r10,64
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	mov	edi,ecx
DB	102,15,56,0,235
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	paddd	xmm4,xmm13
	add	ebx,ecx
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	movdqa	XMMWORD PTR[rsp],xmm4
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	psubd	xmm4,xmm13
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	movups	xmm1,XMMWORD PTR[16+r15]
DB	102,15,56,220,208
	xor	esi,ecx
	mov	edi,eax
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	mov	edi,edx
DB	102,15,56,0,243
	rol	edx,5
	add	ecx,esi
	movups	xmm0,XMMWORD PTR[32+r15]
DB	102,15,56,220,209
	xor	edi,eax
	ror	ebp,7
	paddd	xmm5,xmm13
	add	ecx,edx
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	movdqa	XMMWORD PTR[16+rsp],xmm5
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	psubd	xmm5,xmm13
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	movups	xmm1,XMMWORD PTR[48+r15]
DB	102,15,56,220,208
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	mov	edi,ebp
DB	102,15,56,0,251
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	paddd	xmm6,xmm13
	add	edx,ebp
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	movdqa	XMMWORD PTR[32+rsp],xmm6
	rol	edx,5
	add	ecx,edi
	cmp	r8d,11
	jb	$L$aesenclast4
	movups	xmm0,XMMWORD PTR[64+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+r15]
DB	102,15,56,220,208
	je	$L$aesenclast4
	movups	xmm0,XMMWORD PTR[96+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+r15]
DB	102,15,56,220,208
$L$aesenclast4::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	xor	esi,eax
	ror	ebp,7
	psubd	xmm6,xmm13
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	ror	ecx,7
	add	eax,ebx
	movups	XMMWORD PTR[48+r12*1+r13],xmm2
	lea	r12,QWORD PTR[64+r12]

	add	eax,DWORD PTR[r9]
	add	esi,DWORD PTR[4+r9]
	add	ecx,DWORD PTR[8+r9]
	add	edx,DWORD PTR[12+r9]
	mov	DWORD PTR[r9],eax
	add	ebp,DWORD PTR[16+r9]
	mov	DWORD PTR[4+r9],esi
	mov	ebx,esi
	mov	DWORD PTR[8+r9],ecx
	mov	edi,ecx
	mov	DWORD PTR[12+r9],edx
	xor	edi,edx
	mov	DWORD PTR[16+r9],ebp
	and	esi,edi
	jmp	$L$oop_ssse3

$L$done_ssse3::
	add	ebx,DWORD PTR[16+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[20+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	xor	esi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[24+rsp]
	movups	xmm1,XMMWORD PTR[16+r15]
DB	102,15,56,220,208
	xor	esi,ecx
	mov	edi,eax
	rol	eax,5
	add	ebp,esi
	xor	edi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[28+rsp]
	xor	edi,ebx
	mov	esi,ebp
	rol	ebp,5
	add	edx,edi
	xor	esi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[32+rsp]
	xor	esi,eax
	mov	edi,edx
	rol	edx,5
	add	ecx,esi
	movups	xmm0,XMMWORD PTR[32+r15]
DB	102,15,56,220,209
	xor	edi,eax
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[36+rsp]
	xor	edi,ebp
	mov	esi,ecx
	rol	ecx,5
	add	ebx,edi
	xor	esi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[40+rsp]
	xor	esi,edx
	mov	edi,ebx
	rol	ebx,5
	add	eax,esi
	xor	edi,edx
	ror	ecx,7
	add	eax,ebx
	add	ebp,DWORD PTR[44+rsp]
	movups	xmm1,XMMWORD PTR[48+r15]
DB	102,15,56,220,208
	xor	edi,ecx
	mov	esi,eax
	rol	eax,5
	add	ebp,edi
	xor	esi,ecx
	ror	ebx,7
	add	ebp,eax
	add	edx,DWORD PTR[48+rsp]
	xor	esi,ebx
	mov	edi,ebp
	rol	ebp,5
	add	edx,esi
	xor	edi,ebx
	ror	eax,7
	add	edx,ebp
	add	ecx,DWORD PTR[52+rsp]
	xor	edi,eax
	mov	esi,edx
	rol	edx,5
	add	ecx,edi
	cmp	r8d,11
	jb	$L$aesenclast5
	movups	xmm0,XMMWORD PTR[64+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+r15]
DB	102,15,56,220,208
	je	$L$aesenclast5
	movups	xmm0,XMMWORD PTR[96+r15]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+r15]
DB	102,15,56,220,208
$L$aesenclast5::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+r15]
	xor	esi,eax
	ror	ebp,7
	add	ecx,edx
	add	ebx,DWORD PTR[56+rsp]
	xor	esi,ebp
	mov	edi,ecx
	rol	ecx,5
	add	ebx,esi
	xor	edi,ebp
	ror	edx,7
	add	ebx,ecx
	add	eax,DWORD PTR[60+rsp]
	xor	edi,edx
	mov	esi,ebx
	rol	ebx,5
	add	eax,edi
	ror	ecx,7
	add	eax,ebx
	movups	XMMWORD PTR[48+r12*1+r13],xmm2
	mov	r8,QWORD PTR[88+rsp]

	add	eax,DWORD PTR[r9]
	add	esi,DWORD PTR[4+r9]
	add	ecx,DWORD PTR[8+r9]
	mov	DWORD PTR[r9],eax
	add	edx,DWORD PTR[12+r9]
	mov	DWORD PTR[4+r9],esi
	add	ebp,DWORD PTR[16+r9]
	mov	DWORD PTR[8+r9],ecx
	mov	DWORD PTR[12+r9],edx
	mov	DWORD PTR[16+r9],ebp
	movups	XMMWORD PTR[r8],xmm2
	movaps	xmm6,XMMWORD PTR[((96+0))+rsp]
	movaps	xmm7,XMMWORD PTR[((96+16))+rsp]
	movaps	xmm8,XMMWORD PTR[((96+32))+rsp]
	movaps	xmm9,XMMWORD PTR[((96+48))+rsp]
	movaps	xmm10,XMMWORD PTR[((96+64))+rsp]
	movaps	xmm11,XMMWORD PTR[((96+80))+rsp]
	movaps	xmm12,XMMWORD PTR[((96+96))+rsp]
	movaps	xmm13,XMMWORD PTR[((96+112))+rsp]
	movaps	xmm14,XMMWORD PTR[((96+128))+rsp]
	movaps	xmm15,XMMWORD PTR[((96+144))+rsp]
	lea	rsi,QWORD PTR[264+rsp]
	mov	r15,QWORD PTR[rsi]
	mov	r14,QWORD PTR[8+rsi]
	mov	r13,QWORD PTR[16+rsi]
	mov	r12,QWORD PTR[24+rsi]
	mov	rbp,QWORD PTR[32+rsi]
	mov	rbx,QWORD PTR[40+rsi]
	lea	rsp,QWORD PTR[48+rsi]
$L$epilogue_ssse3::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha1_enc_ssse3::
aesni_cbc_sha1_enc_ssse3	ENDP
ALIGN	64
K_XX_XX::
	DD	05a827999h,05a827999h,05a827999h,05a827999h
	DD	06ed9eba1h,06ed9eba1h,06ed9eba1h,06ed9eba1h
	DD	08f1bbcdch,08f1bbcdch,08f1bbcdch,08f1bbcdch
	DD	0ca62c1d6h,0ca62c1d6h,0ca62c1d6h,0ca62c1d6h
	DD	000010203h,004050607h,008090a0bh,00c0d0e0fh
DB	0fh,0eh,0dh,0ch,0bh,0ah,09h,08h,07h,06h,05h,04h,03h,02h,01h,00h

DB	65,69,83,78,73,45,67,66,67,43,83,72,65,49,32,115
DB	116,105,116,99,104,32,102,111,114,32,120,56,54,95,54,52
DB	44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32
DB	60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111
DB	114,103,62,0
ALIGN	64

ALIGN	32
aesni_cbc_sha1_enc_shaext	PROC PRIVATE
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_sha1_enc_shaext::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	mov	r10,QWORD PTR[56+rsp]
	lea	rsp,QWORD PTR[((-168))+rsp]
	movaps	XMMWORD PTR[(-8-160)+rax],xmm6
	movaps	XMMWORD PTR[(-8-144)+rax],xmm7
	movaps	XMMWORD PTR[(-8-128)+rax],xmm8
	movaps	XMMWORD PTR[(-8-112)+rax],xmm9
	movaps	XMMWORD PTR[(-8-96)+rax],xmm10
	movaps	XMMWORD PTR[(-8-80)+rax],xmm11
	movaps	XMMWORD PTR[(-8-64)+rax],xmm12
	movaps	XMMWORD PTR[(-8-48)+rax],xmm13
	movaps	XMMWORD PTR[(-8-32)+rax],xmm14
	movaps	XMMWORD PTR[(-8-16)+rax],xmm15
$L$prologue_shaext::
	movdqu	xmm8,XMMWORD PTR[r9]
	movd	xmm9,DWORD PTR[16+r9]
	movdqa	xmm7,XMMWORD PTR[((K_XX_XX+80))]

	mov	r11d,DWORD PTR[240+rcx]
	sub	rsi,rdi
	movups	xmm15,XMMWORD PTR[rcx]
	movups	xmm2,XMMWORD PTR[r8]
	movups	xmm0,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[112+rcx]

	pshufd	xmm8,xmm8,27
	pshufd	xmm9,xmm9,27
	jmp	$L$oop_shaext

ALIGN	16
$L$oop_shaext::
	movups	xmm14,XMMWORD PTR[rdi]
	xorps	xmm14,xmm15
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+rcx]
DB	102,15,56,220,208
	movdqu	xmm3,XMMWORD PTR[r10]
	movdqa	xmm12,xmm9
DB	102,15,56,0,223
	movdqu	xmm4,XMMWORD PTR[16+r10]
	movdqa	xmm11,xmm8
	movups	xmm0,XMMWORD PTR[((-64))+rcx]
DB	102,15,56,220,209
DB	102,15,56,0,231

	paddd	xmm9,xmm3
	movdqu	xmm5,XMMWORD PTR[32+r10]
	lea	r10,QWORD PTR[64+r10]
	pxor	xmm3,xmm12
	movups	xmm1,XMMWORD PTR[((-48))+rcx]
DB	102,15,56,220,208
	pxor	xmm3,xmm12
	movdqa	xmm10,xmm8
DB	102,15,56,0,239
DB	69,15,58,204,193,0
DB	68,15,56,200,212
	movups	xmm0,XMMWORD PTR[((-32))+rcx]
DB	102,15,56,220,209
DB	15,56,201,220
	movdqu	xmm6,XMMWORD PTR[((-16))+r10]
	movdqa	xmm9,xmm8
DB	102,15,56,0,247
	movups	xmm1,XMMWORD PTR[((-16))+rcx]
DB	102,15,56,220,208
DB	69,15,58,204,194,0
DB	68,15,56,200,205
	pxor	xmm3,xmm5
DB	15,56,201,229
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,0
DB	68,15,56,200,214
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	15,56,202,222
	pxor	xmm4,xmm6
DB	15,56,201,238
	movups	xmm0,XMMWORD PTR[32+rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,0
DB	68,15,56,200,203
	movups	xmm1,XMMWORD PTR[48+rcx]
DB	102,15,56,220,208
DB	15,56,202,227
	pxor	xmm5,xmm3
DB	15,56,201,243
	cmp	r11d,11
	jb	$L$aesenclast6
	movups	xmm0,XMMWORD PTR[64+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+rcx]
DB	102,15,56,220,208
	je	$L$aesenclast6
	movups	xmm0,XMMWORD PTR[96+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+rcx]
DB	102,15,56,220,208
$L$aesenclast6::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+rcx]
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,0
DB	68,15,56,200,212
	movups	xmm14,XMMWORD PTR[16+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[rdi*1+rsi],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+rcx]
DB	102,15,56,220,208
DB	15,56,202,236
	pxor	xmm6,xmm4
DB	15,56,201,220
	movups	xmm0,XMMWORD PTR[((-64))+rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,1
DB	68,15,56,200,205
	movups	xmm1,XMMWORD PTR[((-48))+rcx]
DB	102,15,56,220,208
DB	15,56,202,245
	pxor	xmm3,xmm5
DB	15,56,201,229
	movups	xmm0,XMMWORD PTR[((-32))+rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,1
DB	68,15,56,200,214
	movups	xmm1,XMMWORD PTR[((-16))+rcx]
DB	102,15,56,220,208
DB	15,56,202,222
	pxor	xmm4,xmm6
DB	15,56,201,238
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,1
DB	68,15,56,200,203
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	15,56,202,227
	pxor	xmm5,xmm3
DB	15,56,201,243
	movups	xmm0,XMMWORD PTR[32+rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,1
DB	68,15,56,200,212
	movups	xmm1,XMMWORD PTR[48+rcx]
DB	102,15,56,220,208
DB	15,56,202,236
	pxor	xmm6,xmm4
DB	15,56,201,220
	cmp	r11d,11
	jb	$L$aesenclast7
	movups	xmm0,XMMWORD PTR[64+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+rcx]
DB	102,15,56,220,208
	je	$L$aesenclast7
	movups	xmm0,XMMWORD PTR[96+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+rcx]
DB	102,15,56,220,208
$L$aesenclast7::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+rcx]
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,1
DB	68,15,56,200,205
	movups	xmm14,XMMWORD PTR[32+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[16+rdi*1+rsi],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+rcx]
DB	102,15,56,220,208
DB	15,56,202,245
	pxor	xmm3,xmm5
DB	15,56,201,229
	movups	xmm0,XMMWORD PTR[((-64))+rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,2
DB	68,15,56,200,214
	movups	xmm1,XMMWORD PTR[((-48))+rcx]
DB	102,15,56,220,208
DB	15,56,202,222
	pxor	xmm4,xmm6
DB	15,56,201,238
	movups	xmm0,XMMWORD PTR[((-32))+rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,2
DB	68,15,56,200,203
	movups	xmm1,XMMWORD PTR[((-16))+rcx]
DB	102,15,56,220,208
DB	15,56,202,227
	pxor	xmm5,xmm3
DB	15,56,201,243
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,2
DB	68,15,56,200,212
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	15,56,202,236
	pxor	xmm6,xmm4
DB	15,56,201,220
	movups	xmm0,XMMWORD PTR[32+rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,2
DB	68,15,56,200,205
	movups	xmm1,XMMWORD PTR[48+rcx]
DB	102,15,56,220,208
DB	15,56,202,245
	pxor	xmm3,xmm5
DB	15,56,201,229
	cmp	r11d,11
	jb	$L$aesenclast8
	movups	xmm0,XMMWORD PTR[64+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+rcx]
DB	102,15,56,220,208
	je	$L$aesenclast8
	movups	xmm0,XMMWORD PTR[96+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+rcx]
DB	102,15,56,220,208
$L$aesenclast8::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+rcx]
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,2
DB	68,15,56,200,214
	movups	xmm14,XMMWORD PTR[48+rdi]
	xorps	xmm14,xmm15
	movups	XMMWORD PTR[32+rdi*1+rsi],xmm2
	xorps	xmm2,xmm14
	movups	xmm1,XMMWORD PTR[((-80))+rcx]
DB	102,15,56,220,208
DB	15,56,202,222
	pxor	xmm4,xmm6
DB	15,56,201,238
	movups	xmm0,XMMWORD PTR[((-64))+rcx]
DB	102,15,56,220,209
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,3
DB	68,15,56,200,203
	movups	xmm1,XMMWORD PTR[((-48))+rcx]
DB	102,15,56,220,208
DB	15,56,202,227
	pxor	xmm5,xmm3
DB	15,56,201,243
	movups	xmm0,XMMWORD PTR[((-32))+rcx]
DB	102,15,56,220,209
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,3
DB	68,15,56,200,212
DB	15,56,202,236
	pxor	xmm6,xmm4
	movups	xmm1,XMMWORD PTR[((-16))+rcx]
DB	102,15,56,220,208
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,3
DB	68,15,56,200,205
DB	15,56,202,245
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,220,209
	movdqa	xmm5,xmm12
	movdqa	xmm10,xmm8
DB	69,15,58,204,193,3
DB	68,15,56,200,214
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
	movdqa	xmm9,xmm8
DB	69,15,58,204,194,3
DB	68,15,56,200,205
	movups	xmm0,XMMWORD PTR[32+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[48+rcx]
DB	102,15,56,220,208
	cmp	r11d,11
	jb	$L$aesenclast9
	movups	xmm0,XMMWORD PTR[64+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[80+rcx]
DB	102,15,56,220,208
	je	$L$aesenclast9
	movups	xmm0,XMMWORD PTR[96+rcx]
DB	102,15,56,220,209
	movups	xmm1,XMMWORD PTR[112+rcx]
DB	102,15,56,220,208
$L$aesenclast9::
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[((16-112))+rcx]
	dec	rdx

	paddd	xmm8,xmm11
	movups	XMMWORD PTR[48+rdi*1+rsi],xmm2
	lea	rdi,QWORD PTR[64+rdi]
	jnz	$L$oop_shaext

	pshufd	xmm8,xmm8,27
	pshufd	xmm9,xmm9,27
	movups	XMMWORD PTR[r8],xmm2
	movdqu	XMMWORD PTR[r9],xmm8
	movd	DWORD PTR[16+r9],xmm9
	movaps	xmm6,XMMWORD PTR[((-8-160))+rax]
	movaps	xmm7,XMMWORD PTR[((-8-144))+rax]
	movaps	xmm8,XMMWORD PTR[((-8-128))+rax]
	movaps	xmm9,XMMWORD PTR[((-8-112))+rax]
	movaps	xmm10,XMMWORD PTR[((-8-96))+rax]
	movaps	xmm11,XMMWORD PTR[((-8-80))+rax]
	movaps	xmm12,XMMWORD PTR[((-8-64))+rax]
	movaps	xmm13,XMMWORD PTR[((-8-48))+rax]
	movaps	xmm14,XMMWORD PTR[((-8-32))+rax]
	movaps	xmm15,XMMWORD PTR[((-8-16))+rax]
	mov	rsp,rax
$L$epilogue_shaext::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_sha1_enc_shaext::
aesni_cbc_sha1_enc_shaext	ENDP
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
ssse3_handler	PROC PRIVATE
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
	lea	r10,QWORD PTR[aesni_cbc_sha1_enc_shaext]
	cmp	rbx,r10
	jb	$L$seh_no_shaext

	lea	rsi,QWORD PTR[rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch
	lea	rax,QWORD PTR[168+rax]
	jmp	$L$common_seh_tail
$L$seh_no_shaext::
	lea	rsi,QWORD PTR[96+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch
	lea	rax,QWORD PTR[264+rax]

	mov	r15,QWORD PTR[rax]
	mov	r14,QWORD PTR[8+rax]
	mov	r13,QWORD PTR[16+rax]
	mov	r12,QWORD PTR[24+rax]
	mov	rbp,QWORD PTR[32+rax]
	mov	rbx,QWORD PTR[40+rax]
	lea	rax,QWORD PTR[48+rax]
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
ssse3_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_aesni_cbc_sha1_enc_ssse3
	DD	imagerel $L$SEH_end_aesni_cbc_sha1_enc_ssse3
	DD	imagerel $L$SEH_info_aesni_cbc_sha1_enc_ssse3
	DD	imagerel $L$SEH_begin_aesni_cbc_sha1_enc_shaext
	DD	imagerel $L$SEH_end_aesni_cbc_sha1_enc_shaext
	DD	imagerel $L$SEH_info_aesni_cbc_sha1_enc_shaext
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_aesni_cbc_sha1_enc_ssse3::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_ssse3,imagerel $L$epilogue_ssse3
$L$SEH_info_aesni_cbc_sha1_enc_shaext::
DB	9,0,0,0
	DD	imagerel ssse3_handler
	DD	imagerel $L$prologue_shaext,imagerel $L$epilogue_shaext

.xdata	ENDS
END
