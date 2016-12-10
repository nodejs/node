TITLE	sha512-586.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.XMM
IF @Version LT 800
XMMWORD STRUCT 16
DQ	2 dup (?)
XMMWORD	ENDS
ENDIF

.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
;EXTERN	_OPENSSL_ia32cap_P:NEAR
ALIGN	16
_sha512_block_data_order	PROC PUBLIC
$L_sha512_block_data_order_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	ebx,esp
	call	$L000pic_point
$L000pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($L001K512-$L000pic_point)[ebp]
	sub	esp,16
	and	esp,-64
	shl	eax,7
	add	eax,edi
	mov	DWORD PTR [esp],esi
	mov	DWORD PTR 4[esp],edi
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	lea	edx,DWORD PTR _OPENSSL_ia32cap_P
	mov	ecx,DWORD PTR [edx]
	test	ecx,67108864
	jz	$L002loop_x86
	mov	edx,DWORD PTR 4[edx]
	movq	mm0,QWORD PTR [esi]
	and	ecx,16777216
	movq	mm1,QWORD PTR 8[esi]
	and	edx,512
	movq	mm2,QWORD PTR 16[esi]
	or	ecx,edx
	movq	mm3,QWORD PTR 24[esi]
	movq	mm4,QWORD PTR 32[esi]
	movq	mm5,QWORD PTR 40[esi]
	movq	mm6,QWORD PTR 48[esi]
	movq	mm7,QWORD PTR 56[esi]
	cmp	ecx,16777728
	je	$L003SSSE3
	sub	esp,80
	jmp	$L004loop_sse2
ALIGN	16
$L004loop_sse2:
	movq	QWORD PTR 8[esp],mm1
	movq	QWORD PTR 16[esp],mm2
	movq	QWORD PTR 24[esp],mm3
	movq	QWORD PTR 40[esp],mm5
	movq	QWORD PTR 48[esp],mm6
	pxor	mm2,mm1
	movq	QWORD PTR 56[esp],mm7
	movq	mm3,mm0
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	add	edi,8
	mov	edx,15
	bswap	eax
	bswap	ebx
	jmp	$L00500_14_sse2
ALIGN	16
$L00500_14_sse2:
	movd	mm1,eax
	mov	eax,DWORD PTR [edi]
	movd	mm7,ebx
	mov	ebx,DWORD PTR 4[edi]
	add	edi,8
	bswap	eax
	bswap	ebx
	punpckldq	mm7,mm1
	movq	mm1,mm4
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	movq	mm0,mm3
	movq	QWORD PTR 72[esp],mm7
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	paddq	mm7,QWORD PTR [ebp]
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	sub	esp,8
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 40[esp]
	paddq	mm3,mm2
	movq	mm2,mm0
	add	ebp,8
	paddq	mm3,mm6
	movq	mm6,QWORD PTR 48[esp]
	dec	edx
	jnz	$L00500_14_sse2
	movd	mm1,eax
	movd	mm7,ebx
	punpckldq	mm7,mm1
	movq	mm1,mm4
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	movq	mm0,mm3
	movq	QWORD PTR 72[esp],mm7
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	paddq	mm7,QWORD PTR [ebp]
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	sub	esp,8
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm7,QWORD PTR 192[esp]
	paddq	mm3,mm2
	movq	mm2,mm0
	add	ebp,8
	paddq	mm3,mm6
	pxor	mm0,mm0
	mov	edx,32
	jmp	$L00616_79_sse2
ALIGN	16
$L00616_79_sse2:
	movq	mm5,QWORD PTR 88[esp]
	movq	mm1,mm7
	psrlq	mm7,1
	movq	mm6,mm5
	psrlq	mm5,6
	psllq	mm1,56
	paddq	mm0,mm3
	movq	mm3,mm7
	psrlq	mm7,6
	pxor	mm3,mm1
	psllq	mm1,7
	pxor	mm3,mm7
	psrlq	mm7,1
	pxor	mm3,mm1
	movq	mm1,mm5
	psrlq	mm5,13
	pxor	mm7,mm3
	psllq	mm6,3
	pxor	mm1,mm5
	paddq	mm7,QWORD PTR 200[esp]
	pxor	mm1,mm6
	psrlq	mm5,42
	paddq	mm7,QWORD PTR 128[esp]
	pxor	mm1,mm5
	psllq	mm6,42
	movq	mm5,QWORD PTR 40[esp]
	pxor	mm1,mm6
	movq	mm6,QWORD PTR 48[esp]
	paddq	mm7,mm1
	movq	mm1,mm4
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	movq	QWORD PTR 72[esp],mm7
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	paddq	mm7,QWORD PTR [ebp]
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	sub	esp,8
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm7,QWORD PTR 192[esp]
	paddq	mm2,mm6
	add	ebp,8
	movq	mm5,QWORD PTR 88[esp]
	movq	mm1,mm7
	psrlq	mm7,1
	movq	mm6,mm5
	psrlq	mm5,6
	psllq	mm1,56
	paddq	mm2,mm3
	movq	mm3,mm7
	psrlq	mm7,6
	pxor	mm3,mm1
	psllq	mm1,7
	pxor	mm3,mm7
	psrlq	mm7,1
	pxor	mm3,mm1
	movq	mm1,mm5
	psrlq	mm5,13
	pxor	mm7,mm3
	psllq	mm6,3
	pxor	mm1,mm5
	paddq	mm7,QWORD PTR 200[esp]
	pxor	mm1,mm6
	psrlq	mm5,42
	paddq	mm7,QWORD PTR 128[esp]
	pxor	mm1,mm5
	psllq	mm6,42
	movq	mm5,QWORD PTR 40[esp]
	pxor	mm1,mm6
	movq	mm6,QWORD PTR 48[esp]
	paddq	mm7,mm1
	movq	mm1,mm4
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	movq	QWORD PTR 72[esp],mm7
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	paddq	mm7,QWORD PTR [ebp]
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	sub	esp,8
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm7,QWORD PTR 192[esp]
	paddq	mm0,mm6
	add	ebp,8
	dec	edx
	jnz	$L00616_79_sse2
	paddq	mm0,mm3
	movq	mm1,QWORD PTR 8[esp]
	movq	mm3,QWORD PTR 24[esp]
	movq	mm5,QWORD PTR 40[esp]
	movq	mm6,QWORD PTR 48[esp]
	movq	mm7,QWORD PTR 56[esp]
	pxor	mm2,mm1
	paddq	mm0,QWORD PTR [esi]
	paddq	mm1,QWORD PTR 8[esi]
	paddq	mm2,QWORD PTR 16[esi]
	paddq	mm3,QWORD PTR 24[esi]
	paddq	mm4,QWORD PTR 32[esi]
	paddq	mm5,QWORD PTR 40[esi]
	paddq	mm6,QWORD PTR 48[esi]
	paddq	mm7,QWORD PTR 56[esi]
	mov	eax,640
	movq	QWORD PTR [esi],mm0
	movq	QWORD PTR 8[esi],mm1
	movq	QWORD PTR 16[esi],mm2
	movq	QWORD PTR 24[esi],mm3
	movq	QWORD PTR 32[esi],mm4
	movq	QWORD PTR 40[esi],mm5
	movq	QWORD PTR 48[esi],mm6
	movq	QWORD PTR 56[esi],mm7
	lea	esp,DWORD PTR [eax*1+esp]
	sub	ebp,eax
	cmp	edi,DWORD PTR 88[esp]
	jb	$L004loop_sse2
	mov	esp,DWORD PTR 92[esp]
	emms
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	32
$L003SSSE3:
	lea	edx,DWORD PTR [esp-64]
	sub	esp,256
	movdqa	xmm1,XMMWORD PTR 640[ebp]
	movdqu	xmm0,XMMWORD PTR [edi]
DB	102,15,56,0,193
	movdqa	xmm3,XMMWORD PTR [ebp]
	movdqa	xmm2,xmm1
	movdqu	xmm1,XMMWORD PTR 16[edi]
	paddq	xmm3,xmm0
DB	102,15,56,0,202
	movdqa	XMMWORD PTR [edx-128],xmm3
	movdqa	xmm4,XMMWORD PTR 16[ebp]
	movdqa	xmm3,xmm2
	movdqu	xmm2,XMMWORD PTR 32[edi]
	paddq	xmm4,xmm1
DB	102,15,56,0,211
	movdqa	XMMWORD PTR [edx-112],xmm4
	movdqa	xmm5,XMMWORD PTR 32[ebp]
	movdqa	xmm4,xmm3
	movdqu	xmm3,XMMWORD PTR 48[edi]
	paddq	xmm5,xmm2
DB	102,15,56,0,220
	movdqa	XMMWORD PTR [edx-96],xmm5
	movdqa	xmm6,XMMWORD PTR 48[ebp]
	movdqa	xmm5,xmm4
	movdqu	xmm4,XMMWORD PTR 64[edi]
	paddq	xmm6,xmm3
DB	102,15,56,0,229
	movdqa	XMMWORD PTR [edx-80],xmm6
	movdqa	xmm7,XMMWORD PTR 64[ebp]
	movdqa	xmm6,xmm5
	movdqu	xmm5,XMMWORD PTR 80[edi]
	paddq	xmm7,xmm4
DB	102,15,56,0,238
	movdqa	XMMWORD PTR [edx-64],xmm7
	movdqa	XMMWORD PTR [edx],xmm0
	movdqa	xmm0,XMMWORD PTR 80[ebp]
	movdqa	xmm7,xmm6
	movdqu	xmm6,XMMWORD PTR 96[edi]
	paddq	xmm0,xmm5
DB	102,15,56,0,247
	movdqa	XMMWORD PTR [edx-48],xmm0
	movdqa	XMMWORD PTR 16[edx],xmm1
	movdqa	xmm1,XMMWORD PTR 96[ebp]
	movdqa	xmm0,xmm7
	movdqu	xmm7,XMMWORD PTR 112[edi]
	paddq	xmm1,xmm6
DB	102,15,56,0,248
	movdqa	XMMWORD PTR [edx-32],xmm1
	movdqa	XMMWORD PTR 32[edx],xmm2
	movdqa	xmm2,XMMWORD PTR 112[ebp]
	movdqa	xmm0,XMMWORD PTR [edx]
	paddq	xmm2,xmm7
	movdqa	XMMWORD PTR [edx-16],xmm2
	nop
ALIGN	32
$L007loop_ssse3:
	movdqa	xmm2,XMMWORD PTR 16[edx]
	movdqa	XMMWORD PTR 48[edx],xmm3
	lea	ebp,DWORD PTR 128[ebp]
	movq	QWORD PTR 8[esp],mm1
	mov	ebx,edi
	movq	QWORD PTR 16[esp],mm2
	lea	edi,DWORD PTR 128[edi]
	movq	QWORD PTR 24[esp],mm3
	cmp	edi,eax
	movq	QWORD PTR 40[esp],mm5
	cmovb	ebx,edi
	movq	QWORD PTR 48[esp],mm6
	mov	ecx,4
	pxor	mm2,mm1
	movq	QWORD PTR 56[esp],mm7
	pxor	mm3,mm3
	jmp	$L00800_47_ssse3
ALIGN	32
$L00800_47_ssse3:
	movdqa	xmm3,xmm5
	movdqa	xmm1,xmm2
DB	102,15,58,15,208,8
	movdqa	XMMWORD PTR [edx],xmm4
DB	102,15,58,15,220,8
	movdqa	xmm4,xmm2
	psrlq	xmm2,7
	paddq	xmm0,xmm3
	movdqa	xmm3,xmm4
	psrlq	xmm4,1
	psllq	xmm3,56
	pxor	xmm2,xmm4
	psrlq	xmm4,7
	pxor	xmm2,xmm3
	psllq	xmm3,7
	pxor	xmm2,xmm4
	movdqa	xmm4,xmm7
	pxor	xmm2,xmm3
	movdqa	xmm3,xmm7
	psrlq	xmm4,6
	paddq	xmm0,xmm2
	movdqa	xmm2,xmm7
	psrlq	xmm3,19
	psllq	xmm2,3
	pxor	xmm4,xmm3
	psrlq	xmm3,42
	pxor	xmm4,xmm2
	psllq	xmm2,42
	pxor	xmm4,xmm3
	movdqa	xmm3,XMMWORD PTR 32[edx]
	pxor	xmm4,xmm2
	movdqa	xmm2,XMMWORD PTR [ebp]
	movq	mm1,mm4
	paddq	xmm0,xmm4
	movq	mm7,QWORD PTR [edx-128]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	paddq	xmm2,xmm0
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 32[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 40[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-120]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 24[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 56[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 48[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 16[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR [esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 24[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 32[esp]
	movdqa	XMMWORD PTR [edx-128],xmm2
	movdqa	xmm4,xmm6
	movdqa	xmm2,xmm3
DB	102,15,58,15,217,8
	movdqa	XMMWORD PTR 16[edx],xmm5
DB	102,15,58,15,229,8
	movdqa	xmm5,xmm3
	psrlq	xmm3,7
	paddq	xmm1,xmm4
	movdqa	xmm4,xmm5
	psrlq	xmm5,1
	psllq	xmm4,56
	pxor	xmm3,xmm5
	psrlq	xmm5,7
	pxor	xmm3,xmm4
	psllq	xmm4,7
	pxor	xmm3,xmm5
	movdqa	xmm5,xmm0
	pxor	xmm3,xmm4
	movdqa	xmm4,xmm0
	psrlq	xmm5,6
	paddq	xmm1,xmm3
	movdqa	xmm3,xmm0
	psrlq	xmm4,19
	psllq	xmm3,3
	pxor	xmm5,xmm4
	psrlq	xmm4,42
	pxor	xmm5,xmm3
	psllq	xmm3,42
	pxor	xmm5,xmm4
	movdqa	xmm4,XMMWORD PTR 48[edx]
	pxor	xmm5,xmm3
	movdqa	xmm3,XMMWORD PTR 16[ebp]
	movq	mm1,mm4
	paddq	xmm1,xmm5
	movq	mm7,QWORD PTR [edx-112]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 16[esp],mm4
	paddq	xmm3,xmm1
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 48[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 40[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 8[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 56[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 16[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 24[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-104]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 8[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 40[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 32[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR [esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 48[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 8[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 16[esp]
	movdqa	XMMWORD PTR [edx-112],xmm3
	movdqa	xmm5,xmm7
	movdqa	xmm3,xmm4
DB	102,15,58,15,226,8
	movdqa	XMMWORD PTR 32[edx],xmm6
DB	102,15,58,15,238,8
	movdqa	xmm6,xmm4
	psrlq	xmm4,7
	paddq	xmm2,xmm5
	movdqa	xmm5,xmm6
	psrlq	xmm6,1
	psllq	xmm5,56
	pxor	xmm4,xmm6
	psrlq	xmm6,7
	pxor	xmm4,xmm5
	psllq	xmm5,7
	pxor	xmm4,xmm6
	movdqa	xmm6,xmm1
	pxor	xmm4,xmm5
	movdqa	xmm5,xmm1
	psrlq	xmm6,6
	paddq	xmm2,xmm4
	movdqa	xmm4,xmm1
	psrlq	xmm5,19
	psllq	xmm4,3
	pxor	xmm6,xmm5
	psrlq	xmm5,42
	pxor	xmm6,xmm4
	psllq	xmm4,42
	pxor	xmm6,xmm5
	movdqa	xmm5,XMMWORD PTR [edx]
	pxor	xmm6,xmm4
	movdqa	xmm4,XMMWORD PTR 32[ebp]
	movq	mm1,mm4
	paddq	xmm2,xmm6
	movq	mm7,QWORD PTR [edx-96]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR [esp],mm4
	paddq	xmm4,xmm2
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 32[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 24[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 56[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 40[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR [esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 8[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-88]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 56[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 24[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 16[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 48[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 32[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 56[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR [esp]
	movdqa	XMMWORD PTR [edx-96],xmm4
	movdqa	xmm6,xmm0
	movdqa	xmm4,xmm5
DB	102,15,58,15,235,8
	movdqa	XMMWORD PTR 48[edx],xmm7
DB	102,15,58,15,247,8
	movdqa	xmm7,xmm5
	psrlq	xmm5,7
	paddq	xmm3,xmm6
	movdqa	xmm6,xmm7
	psrlq	xmm7,1
	psllq	xmm6,56
	pxor	xmm5,xmm7
	psrlq	xmm7,7
	pxor	xmm5,xmm6
	psllq	xmm6,7
	pxor	xmm5,xmm7
	movdqa	xmm7,xmm2
	pxor	xmm5,xmm6
	movdqa	xmm6,xmm2
	psrlq	xmm7,6
	paddq	xmm3,xmm5
	movdqa	xmm5,xmm2
	psrlq	xmm6,19
	psllq	xmm5,3
	pxor	xmm7,xmm6
	psrlq	xmm6,42
	pxor	xmm7,xmm5
	psllq	xmm5,42
	pxor	xmm7,xmm6
	movdqa	xmm6,XMMWORD PTR 16[edx]
	pxor	xmm7,xmm5
	movdqa	xmm5,XMMWORD PTR 48[ebp]
	movq	mm1,mm4
	paddq	xmm3,xmm7
	movq	mm7,QWORD PTR [edx-80]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 48[esp],mm4
	paddq	xmm5,xmm3
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 16[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 8[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 40[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 24[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 48[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 56[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-72]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 40[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 8[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR [esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 32[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 16[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 40[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 48[esp]
	movdqa	XMMWORD PTR [edx-80],xmm5
	movdqa	xmm7,xmm1
	movdqa	xmm5,xmm6
DB	102,15,58,15,244,8
	movdqa	XMMWORD PTR [edx],xmm0
DB	102,15,58,15,248,8
	movdqa	xmm0,xmm6
	psrlq	xmm6,7
	paddq	xmm4,xmm7
	movdqa	xmm7,xmm0
	psrlq	xmm0,1
	psllq	xmm7,56
	pxor	xmm6,xmm0
	psrlq	xmm0,7
	pxor	xmm6,xmm7
	psllq	xmm7,7
	pxor	xmm6,xmm0
	movdqa	xmm0,xmm3
	pxor	xmm6,xmm7
	movdqa	xmm7,xmm3
	psrlq	xmm0,6
	paddq	xmm4,xmm6
	movdqa	xmm6,xmm3
	psrlq	xmm7,19
	psllq	xmm6,3
	pxor	xmm0,xmm7
	psrlq	xmm7,42
	pxor	xmm0,xmm6
	psllq	xmm6,42
	pxor	xmm0,xmm7
	movdqa	xmm7,XMMWORD PTR 32[edx]
	pxor	xmm0,xmm6
	movdqa	xmm6,XMMWORD PTR 64[ebp]
	movq	mm1,mm4
	paddq	xmm4,xmm0
	movq	mm7,QWORD PTR [edx-64]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	paddq	xmm6,xmm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 32[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 40[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-56]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 24[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 56[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 48[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 16[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR [esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 24[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 32[esp]
	movdqa	XMMWORD PTR [edx-64],xmm6
	movdqa	xmm0,xmm2
	movdqa	xmm6,xmm7
DB	102,15,58,15,253,8
	movdqa	XMMWORD PTR 16[edx],xmm1
DB	102,15,58,15,193,8
	movdqa	xmm1,xmm7
	psrlq	xmm7,7
	paddq	xmm5,xmm0
	movdqa	xmm0,xmm1
	psrlq	xmm1,1
	psllq	xmm0,56
	pxor	xmm7,xmm1
	psrlq	xmm1,7
	pxor	xmm7,xmm0
	psllq	xmm0,7
	pxor	xmm7,xmm1
	movdqa	xmm1,xmm4
	pxor	xmm7,xmm0
	movdqa	xmm0,xmm4
	psrlq	xmm1,6
	paddq	xmm5,xmm7
	movdqa	xmm7,xmm4
	psrlq	xmm0,19
	psllq	xmm7,3
	pxor	xmm1,xmm0
	psrlq	xmm0,42
	pxor	xmm1,xmm7
	psllq	xmm7,42
	pxor	xmm1,xmm0
	movdqa	xmm0,XMMWORD PTR 48[edx]
	pxor	xmm1,xmm7
	movdqa	xmm7,XMMWORD PTR 80[ebp]
	movq	mm1,mm4
	paddq	xmm5,xmm1
	movq	mm7,QWORD PTR [edx-48]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 16[esp],mm4
	paddq	xmm7,xmm5
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 48[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 40[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 8[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 56[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 16[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 24[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-40]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 8[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 40[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 32[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR [esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 48[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 8[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 16[esp]
	movdqa	XMMWORD PTR [edx-48],xmm7
	movdqa	xmm1,xmm3
	movdqa	xmm7,xmm0
DB	102,15,58,15,198,8
	movdqa	XMMWORD PTR 32[edx],xmm2
DB	102,15,58,15,202,8
	movdqa	xmm2,xmm0
	psrlq	xmm0,7
	paddq	xmm6,xmm1
	movdqa	xmm1,xmm2
	psrlq	xmm2,1
	psllq	xmm1,56
	pxor	xmm0,xmm2
	psrlq	xmm2,7
	pxor	xmm0,xmm1
	psllq	xmm1,7
	pxor	xmm0,xmm2
	movdqa	xmm2,xmm5
	pxor	xmm0,xmm1
	movdqa	xmm1,xmm5
	psrlq	xmm2,6
	paddq	xmm6,xmm0
	movdqa	xmm0,xmm5
	psrlq	xmm1,19
	psllq	xmm0,3
	pxor	xmm2,xmm1
	psrlq	xmm1,42
	pxor	xmm2,xmm0
	psllq	xmm0,42
	pxor	xmm2,xmm1
	movdqa	xmm1,XMMWORD PTR [edx]
	pxor	xmm2,xmm0
	movdqa	xmm0,XMMWORD PTR 96[ebp]
	movq	mm1,mm4
	paddq	xmm6,xmm2
	movq	mm7,QWORD PTR [edx-32]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR [esp],mm4
	paddq	xmm0,xmm6
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 32[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 24[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 56[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 40[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR [esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 8[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-24]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 56[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 24[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 16[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 48[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 32[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 56[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR [esp]
	movdqa	XMMWORD PTR [edx-32],xmm0
	movdqa	xmm2,xmm4
	movdqa	xmm0,xmm1
DB	102,15,58,15,207,8
	movdqa	XMMWORD PTR 48[edx],xmm3
DB	102,15,58,15,211,8
	movdqa	xmm3,xmm1
	psrlq	xmm1,7
	paddq	xmm7,xmm2
	movdqa	xmm2,xmm3
	psrlq	xmm3,1
	psllq	xmm2,56
	pxor	xmm1,xmm3
	psrlq	xmm3,7
	pxor	xmm1,xmm2
	psllq	xmm2,7
	pxor	xmm1,xmm3
	movdqa	xmm3,xmm6
	pxor	xmm1,xmm2
	movdqa	xmm2,xmm6
	psrlq	xmm3,6
	paddq	xmm7,xmm1
	movdqa	xmm1,xmm6
	psrlq	xmm2,19
	psllq	xmm1,3
	pxor	xmm3,xmm2
	psrlq	xmm2,42
	pxor	xmm3,xmm1
	psllq	xmm1,42
	pxor	xmm3,xmm2
	movdqa	xmm2,XMMWORD PTR 16[edx]
	pxor	xmm3,xmm1
	movdqa	xmm1,XMMWORD PTR 112[ebp]
	movq	mm1,mm4
	paddq	xmm7,xmm3
	movq	mm7,QWORD PTR [edx-16]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 48[esp],mm4
	paddq	xmm1,xmm7
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 16[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 8[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 40[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 24[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 48[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 56[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-8]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 40[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 8[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR [esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 32[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 16[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 40[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 48[esp]
	movdqa	XMMWORD PTR [edx-16],xmm1
	lea	ebp,DWORD PTR 128[ebp]
	dec	ecx
	jnz	$L00800_47_ssse3
	movdqa	xmm1,XMMWORD PTR [ebp]
	lea	ebp,DWORD PTR [ebp-640]
	movdqu	xmm0,XMMWORD PTR [ebx]
DB	102,15,56,0,193
	movdqa	xmm3,XMMWORD PTR [ebp]
	movdqa	xmm2,xmm1
	movdqu	xmm1,XMMWORD PTR 16[ebx]
	paddq	xmm3,xmm0
DB	102,15,56,0,202
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-128]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 32[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 40[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-120]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 24[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 56[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 48[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 16[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR [esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 24[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 32[esp]
	movdqa	XMMWORD PTR [edx-128],xmm3
	movdqa	xmm4,XMMWORD PTR 16[ebp]
	movdqa	xmm3,xmm2
	movdqu	xmm2,XMMWORD PTR 32[ebx]
	paddq	xmm4,xmm1
DB	102,15,56,0,211
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-112]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 16[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 48[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 40[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 8[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 56[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 16[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 24[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-104]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 8[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 40[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 32[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR [esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 48[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 8[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 16[esp]
	movdqa	XMMWORD PTR [edx-112],xmm4
	movdqa	xmm5,XMMWORD PTR 32[ebp]
	movdqa	xmm4,xmm3
	movdqu	xmm3,XMMWORD PTR 48[ebx]
	paddq	xmm5,xmm2
DB	102,15,56,0,220
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-96]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR [esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 32[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 24[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 56[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 40[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR [esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 8[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-88]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 56[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 24[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 16[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 48[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 32[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 56[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR [esp]
	movdqa	XMMWORD PTR [edx-96],xmm5
	movdqa	xmm6,XMMWORD PTR 48[ebp]
	movdqa	xmm5,xmm4
	movdqu	xmm4,XMMWORD PTR 64[ebx]
	paddq	xmm6,xmm3
DB	102,15,56,0,229
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-80]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 48[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 16[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 8[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 40[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 24[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 48[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 56[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-72]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 40[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 8[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR [esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 32[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 16[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 40[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 48[esp]
	movdqa	XMMWORD PTR [edx-80],xmm6
	movdqa	xmm7,XMMWORD PTR 64[ebp]
	movdqa	xmm6,xmm5
	movdqu	xmm5,XMMWORD PTR 80[ebx]
	paddq	xmm7,xmm4
DB	102,15,56,0,238
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-64]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 32[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR [esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 56[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 24[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 8[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 32[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 40[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-56]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 24[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 56[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 48[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 16[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR [esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 24[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 32[esp]
	movdqa	XMMWORD PTR [edx-64],xmm7
	movdqa	XMMWORD PTR [edx],xmm0
	movdqa	xmm0,XMMWORD PTR 80[ebp]
	movdqa	xmm7,xmm6
	movdqu	xmm6,XMMWORD PTR 96[ebx]
	paddq	xmm0,xmm5
DB	102,15,56,0,247
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-48]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 16[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 48[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 40[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 8[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 56[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 16[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 24[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-40]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 8[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 40[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 32[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR [esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 48[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 8[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 16[esp]
	movdqa	XMMWORD PTR [edx-48],xmm0
	movdqa	XMMWORD PTR 16[edx],xmm1
	movdqa	xmm1,XMMWORD PTR 96[ebp]
	movdqa	xmm0,xmm7
	movdqu	xmm7,XMMWORD PTR 112[ebx]
	paddq	xmm1,xmm6
DB	102,15,56,0,248
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-32]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR [esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 32[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 24[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 56[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 40[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR [esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 8[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-24]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 56[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 24[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 16[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 48[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 32[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 56[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR [esp]
	movdqa	XMMWORD PTR [edx-32],xmm1
	movdqa	XMMWORD PTR 32[edx],xmm2
	movdqa	xmm2,XMMWORD PTR 112[ebp]
	movdqa	xmm0,XMMWORD PTR [edx]
	paddq	xmm2,xmm7
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-16]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 48[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm0,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 16[esp],mm0
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR 8[esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 40[esp]
	paddq	mm3,mm7
	movq	mm5,mm0
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm0
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 24[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm0,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm2,mm0
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm2,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 48[esp]
	paddq	mm2,mm6
	movq	mm6,QWORD PTR 56[esp]
	movq	mm1,mm4
	movq	mm7,QWORD PTR [edx-8]
	pxor	mm5,mm6
	psrlq	mm1,14
	movq	QWORD PTR 40[esp],mm4
	pand	mm5,mm4
	psllq	mm4,23
	paddq	mm2,mm3
	movq	mm3,mm1
	psrlq	mm1,4
	pxor	mm5,mm6
	pxor	mm3,mm4
	psllq	mm4,23
	pxor	mm3,mm1
	movq	QWORD PTR 8[esp],mm2
	paddq	mm7,mm5
	pxor	mm3,mm4
	psrlq	mm1,23
	paddq	mm7,QWORD PTR [esp]
	pxor	mm3,mm1
	psllq	mm4,4
	pxor	mm3,mm4
	movq	mm4,QWORD PTR 32[esp]
	paddq	mm3,mm7
	movq	mm5,mm2
	psrlq	mm5,28
	paddq	mm4,mm3
	movq	mm6,mm2
	movq	mm7,mm5
	psllq	mm6,25
	movq	mm1,QWORD PTR 16[esp]
	psrlq	mm5,6
	pxor	mm7,mm6
	psllq	mm6,5
	pxor	mm7,mm5
	pxor	mm2,mm1
	psrlq	mm5,5
	pxor	mm7,mm6
	pand	mm0,mm2
	psllq	mm6,6
	pxor	mm7,mm5
	pxor	mm0,mm1
	pxor	mm6,mm7
	movq	mm5,QWORD PTR 40[esp]
	paddq	mm0,mm6
	movq	mm6,QWORD PTR 48[esp]
	movdqa	XMMWORD PTR [edx-16],xmm2
	movq	mm1,QWORD PTR 8[esp]
	paddq	mm0,mm3
	movq	mm3,QWORD PTR 24[esp]
	movq	mm7,QWORD PTR 56[esp]
	pxor	mm2,mm1
	paddq	mm0,QWORD PTR [esi]
	paddq	mm1,QWORD PTR 8[esi]
	paddq	mm2,QWORD PTR 16[esi]
	paddq	mm3,QWORD PTR 24[esi]
	paddq	mm4,QWORD PTR 32[esi]
	paddq	mm5,QWORD PTR 40[esi]
	paddq	mm6,QWORD PTR 48[esi]
	paddq	mm7,QWORD PTR 56[esi]
	movq	QWORD PTR [esi],mm0
	movq	QWORD PTR 8[esi],mm1
	movq	QWORD PTR 16[esi],mm2
	movq	QWORD PTR 24[esi],mm3
	movq	QWORD PTR 32[esi],mm4
	movq	QWORD PTR 40[esi],mm5
	movq	QWORD PTR 48[esi],mm6
	movq	QWORD PTR 56[esi],mm7
	cmp	edi,eax
	jb	$L007loop_ssse3
	mov	esp,DWORD PTR 76[edx]
	emms
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	16
$L002loop_x86:
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 16[edi]
	mov	ebx,DWORD PTR 20[edi]
	mov	ecx,DWORD PTR 24[edi]
	mov	edx,DWORD PTR 28[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 32[edi]
	mov	ebx,DWORD PTR 36[edi]
	mov	ecx,DWORD PTR 40[edi]
	mov	edx,DWORD PTR 44[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 48[edi]
	mov	ebx,DWORD PTR 52[edi]
	mov	ecx,DWORD PTR 56[edi]
	mov	edx,DWORD PTR 60[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 64[edi]
	mov	ebx,DWORD PTR 68[edi]
	mov	ecx,DWORD PTR 72[edi]
	mov	edx,DWORD PTR 76[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 80[edi]
	mov	ebx,DWORD PTR 84[edi]
	mov	ecx,DWORD PTR 88[edi]
	mov	edx,DWORD PTR 92[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 96[edi]
	mov	ebx,DWORD PTR 100[edi]
	mov	ecx,DWORD PTR 104[edi]
	mov	edx,DWORD PTR 108[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	mov	eax,DWORD PTR 112[edi]
	mov	ebx,DWORD PTR 116[edi]
	mov	ecx,DWORD PTR 120[edi]
	mov	edx,DWORD PTR 124[edi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	push	eax
	push	ebx
	push	ecx
	push	edx
	add	edi,128
	sub	esp,72
	mov	DWORD PTR 204[esp],edi
	lea	edi,DWORD PTR 8[esp]
	mov	ecx,16
DD	2784229001
ALIGN	16
$L00900_15_x86:
	mov	ecx,DWORD PTR 40[esp]
	mov	edx,DWORD PTR 44[esp]
	mov	esi,ecx
	shr	ecx,9
	mov	edi,edx
	shr	edx,9
	mov	ebx,ecx
	shl	esi,14
	mov	eax,edx
	shl	edi,14
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	eax,ecx
	shl	esi,4
	xor	ebx,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,4
	xor	eax,edi
	shr	edx,4
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 48[esp]
	mov	edx,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 56[esp]
	mov	edi,DWORD PTR 60[esp]
	add	eax,DWORD PTR 64[esp]
	adc	ebx,DWORD PTR 68[esp]
	xor	ecx,esi
	xor	edx,edi
	and	ecx,DWORD PTR 40[esp]
	and	edx,DWORD PTR 44[esp]
	add	eax,DWORD PTR 192[esp]
	adc	ebx,DWORD PTR 196[esp]
	xor	ecx,esi
	xor	edx,edi
	mov	esi,DWORD PTR [ebp]
	mov	edi,DWORD PTR 4[ebp]
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 32[esp]
	mov	edx,DWORD PTR 36[esp]
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	esi,ecx
	shr	ecx,2
	mov	edi,edx
	shr	edx,2
	mov	ebx,ecx
	shl	esi,4
	mov	eax,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	ebx,ecx
	shl	esi,21
	xor	eax,edx
	shl	edi,21
	xor	eax,esi
	shr	ecx,21
	xor	ebx,edi
	shr	edx,21
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 20[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	or	ecx,esi
	or	edx,edi
	and	ecx,DWORD PTR 24[esp]
	and	edx,DWORD PTR 28[esp]
	and	esi,DWORD PTR 8[esp]
	and	edi,DWORD PTR 12[esp]
	or	ecx,esi
	or	edx,edi
	add	eax,ecx
	adc	ebx,edx
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	dl,BYTE PTR [ebp]
	sub	esp,8
	lea	ebp,DWORD PTR 8[ebp]
	cmp	dl,148
	jne	$L00900_15_x86
ALIGN	16
$L01016_79_x86:
	mov	ecx,DWORD PTR 312[esp]
	mov	edx,DWORD PTR 316[esp]
	mov	esi,ecx
	shr	ecx,1
	mov	edi,edx
	shr	edx,1
	mov	eax,ecx
	shl	esi,24
	mov	ebx,edx
	shl	edi,24
	xor	ebx,esi
	shr	ecx,6
	xor	eax,edi
	shr	edx,6
	xor	eax,ecx
	shl	esi,7
	xor	ebx,edx
	shl	edi,1
	xor	ebx,esi
	shr	ecx,1
	xor	eax,edi
	shr	edx,1
	xor	eax,ecx
	shl	edi,6
	xor	ebx,edx
	xor	eax,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	ecx,DWORD PTR 208[esp]
	mov	edx,DWORD PTR 212[esp]
	mov	esi,ecx
	shr	ecx,6
	mov	edi,edx
	shr	edx,6
	mov	eax,ecx
	shl	esi,3
	mov	ebx,edx
	shl	edi,3
	xor	eax,esi
	shr	ecx,13
	xor	ebx,edi
	shr	edx,13
	xor	eax,ecx
	shl	esi,10
	xor	ebx,edx
	shl	edi,10
	xor	ebx,esi
	shr	ecx,10
	xor	eax,edi
	shr	edx,10
	xor	ebx,ecx
	shl	edi,13
	xor	eax,edx
	xor	eax,edi
	mov	ecx,DWORD PTR 320[esp]
	mov	edx,DWORD PTR 324[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	mov	esi,DWORD PTR 248[esp]
	mov	edi,DWORD PTR 252[esp]
	add	eax,ecx
	adc	ebx,edx
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR 192[esp],eax
	mov	DWORD PTR 196[esp],ebx
	mov	ecx,DWORD PTR 40[esp]
	mov	edx,DWORD PTR 44[esp]
	mov	esi,ecx
	shr	ecx,9
	mov	edi,edx
	shr	edx,9
	mov	ebx,ecx
	shl	esi,14
	mov	eax,edx
	shl	edi,14
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	eax,ecx
	shl	esi,4
	xor	ebx,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,4
	xor	eax,edi
	shr	edx,4
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 48[esp]
	mov	edx,DWORD PTR 52[esp]
	mov	esi,DWORD PTR 56[esp]
	mov	edi,DWORD PTR 60[esp]
	add	eax,DWORD PTR 64[esp]
	adc	ebx,DWORD PTR 68[esp]
	xor	ecx,esi
	xor	edx,edi
	and	ecx,DWORD PTR 40[esp]
	and	edx,DWORD PTR 44[esp]
	add	eax,DWORD PTR 192[esp]
	adc	ebx,DWORD PTR 196[esp]
	xor	ecx,esi
	xor	edx,edi
	mov	esi,DWORD PTR [ebp]
	mov	edi,DWORD PTR 4[ebp]
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 32[esp]
	mov	edx,DWORD PTR 36[esp]
	add	eax,esi
	adc	ebx,edi
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	add	eax,ecx
	adc	ebx,edx
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	DWORD PTR 32[esp],eax
	mov	DWORD PTR 36[esp],ebx
	mov	esi,ecx
	shr	ecx,2
	mov	edi,edx
	shr	edx,2
	mov	ebx,ecx
	shl	esi,4
	mov	eax,edx
	shl	edi,4
	xor	ebx,esi
	shr	ecx,5
	xor	eax,edi
	shr	edx,5
	xor	ebx,ecx
	shl	esi,21
	xor	eax,edx
	shl	edi,21
	xor	eax,esi
	shr	ecx,21
	xor	ebx,edi
	shr	edx,21
	xor	eax,ecx
	shl	esi,5
	xor	ebx,edx
	shl	edi,5
	xor	eax,esi
	xor	ebx,edi
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 20[esp]
	add	eax,DWORD PTR [esp]
	adc	ebx,DWORD PTR 4[esp]
	or	ecx,esi
	or	edx,edi
	and	ecx,DWORD PTR 24[esp]
	and	edx,DWORD PTR 28[esp]
	and	esi,DWORD PTR 8[esp]
	and	edi,DWORD PTR 12[esp]
	or	ecx,esi
	or	edx,edi
	add	eax,ecx
	adc	ebx,edx
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	dl,BYTE PTR [ebp]
	sub	esp,8
	lea	ebp,DWORD PTR 8[ebp]
	cmp	dl,23
	jne	$L01016_79_x86
	mov	esi,DWORD PTR 840[esp]
	mov	edi,DWORD PTR 844[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	add	eax,DWORD PTR 8[esp]
	adc	ebx,DWORD PTR 12[esp]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	add	ecx,DWORD PTR 16[esp]
	adc	edx,DWORD PTR 20[esp]
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	mov	eax,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 28[esi]
	add	eax,DWORD PTR 24[esp]
	adc	ebx,DWORD PTR 28[esp]
	mov	DWORD PTR 16[esi],eax
	mov	DWORD PTR 20[esi],ebx
	add	ecx,DWORD PTR 32[esp]
	adc	edx,DWORD PTR 36[esp]
	mov	DWORD PTR 24[esi],ecx
	mov	DWORD PTR 28[esi],edx
	mov	eax,DWORD PTR 32[esi]
	mov	ebx,DWORD PTR 36[esi]
	mov	ecx,DWORD PTR 40[esi]
	mov	edx,DWORD PTR 44[esi]
	add	eax,DWORD PTR 40[esp]
	adc	ebx,DWORD PTR 44[esp]
	mov	DWORD PTR 32[esi],eax
	mov	DWORD PTR 36[esi],ebx
	add	ecx,DWORD PTR 48[esp]
	adc	edx,DWORD PTR 52[esp]
	mov	DWORD PTR 40[esi],ecx
	mov	DWORD PTR 44[esi],edx
	mov	eax,DWORD PTR 48[esi]
	mov	ebx,DWORD PTR 52[esi]
	mov	ecx,DWORD PTR 56[esi]
	mov	edx,DWORD PTR 60[esi]
	add	eax,DWORD PTR 56[esp]
	adc	ebx,DWORD PTR 60[esp]
	mov	DWORD PTR 48[esi],eax
	mov	DWORD PTR 52[esi],ebx
	add	ecx,DWORD PTR 64[esp]
	adc	edx,DWORD PTR 68[esp]
	mov	DWORD PTR 56[esi],ecx
	mov	DWORD PTR 60[esi],edx
	add	esp,840
	sub	ebp,640
	cmp	edi,DWORD PTR 8[esp]
	jb	$L002loop_x86
	mov	esp,DWORD PTR 12[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L001K512:
DD	3609767458,1116352408
DD	602891725,1899447441
DD	3964484399,3049323471
DD	2173295548,3921009573
DD	4081628472,961987163
DD	3053834265,1508970993
DD	2937671579,2453635748
DD	3664609560,2870763221
DD	2734883394,3624381080
DD	1164996542,310598401
DD	1323610764,607225278
DD	3590304994,1426881987
DD	4068182383,1925078388
DD	991336113,2162078206
DD	633803317,2614888103
DD	3479774868,3248222580
DD	2666613458,3835390401
DD	944711139,4022224774
DD	2341262773,264347078
DD	2007800933,604807628
DD	1495990901,770255983
DD	1856431235,1249150122
DD	3175218132,1555081692
DD	2198950837,1996064986
DD	3999719339,2554220882
DD	766784016,2821834349
DD	2566594879,2952996808
DD	3203337956,3210313671
DD	1034457026,3336571891
DD	2466948901,3584528711
DD	3758326383,113926993
DD	168717936,338241895
DD	1188179964,666307205
DD	1546045734,773529912
DD	1522805485,1294757372
DD	2643833823,1396182291
DD	2343527390,1695183700
DD	1014477480,1986661051
DD	1206759142,2177026350
DD	344077627,2456956037
DD	1290863460,2730485921
DD	3158454273,2820302411
DD	3505952657,3259730800
DD	106217008,3345764771
DD	3606008344,3516065817
DD	1432725776,3600352804
DD	1467031594,4094571909
DD	851169720,275423344
DD	3100823752,430227734
DD	1363258195,506948616
DD	3750685593,659060556
DD	3785050280,883997877
DD	3318307427,958139571
DD	3812723403,1322822218
DD	2003034995,1537002063
DD	3602036899,1747873779
DD	1575990012,1955562222
DD	1125592928,2024104815
DD	2716904306,2227730452
DD	442776044,2361852424
DD	593698344,2428436474
DD	3733110249,2756734187
DD	2999351573,3204031479
DD	3815920427,3329325298
DD	3928383900,3391569614
DD	566280711,3515267271
DD	3454069534,3940187606
DD	4000239992,4118630271
DD	1914138554,116418474
DD	2731055270,174292421
DD	3203993006,289380356
DD	320620315,460393269
DD	587496836,685471733
DD	1086792851,852142971
DD	365543100,1017036298
DD	2618297676,1126000580
DD	3409855158,1288033470
DD	4234509866,1501505948
DD	987167468,1607167915
DD	1246189591,1816402316
DD	67438087,66051
DD	202182159,134810123
_sha512_block_data_order ENDP
DB	83,72,65,53,49,50,32,98,108,111,99,107,32,116,114,97
DB	110,115,102,111,114,109,32,102,111,114,32,120,56,54,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
