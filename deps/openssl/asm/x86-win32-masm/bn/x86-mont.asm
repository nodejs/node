TITLE	../openssl/crypto/bn/asm/x86-mont.asm
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
_bn_mul_mont	PROC PUBLIC
$L_bn_mul_mont_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	xor	eax,eax
	mov	edi,DWORD PTR 40[esp]
	cmp	edi,4
	jl	$L000just_leave
	lea	esi,DWORD PTR 20[esp]
	lea	edx,DWORD PTR 24[esp]
	mov	ebp,esp
	add	edi,2
	neg	edi
	lea	esp,DWORD PTR [edi*4+esp-32]
	neg	edi
	mov	eax,esp
	sub	eax,edx
	and	eax,2047
	sub	esp,eax
	xor	edx,esp
	and	edx,2048
	xor	edx,2048
	sub	esp,edx
	and	esp,-64
	mov	eax,ebp
	sub	eax,esp
	and	eax,-4096
$L001page_walk:
	mov	edx,DWORD PTR [eax*1+esp]
	sub	eax,4096
DB	46
	jnc	$L001page_walk
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	esi,DWORD PTR 16[esi]
	mov	esi,DWORD PTR [esi]
	mov	DWORD PTR 4[esp],eax
	mov	DWORD PTR 8[esp],ebx
	mov	DWORD PTR 12[esp],ecx
	mov	DWORD PTR 16[esp],edx
	mov	DWORD PTR 20[esp],esi
	lea	ebx,DWORD PTR [edi-3]
	mov	DWORD PTR 24[esp],ebp
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [eax],26
	jnc	$L002non_sse2
	mov	eax,-1
	movd	mm7,eax
	mov	esi,DWORD PTR 8[esp]
	mov	edi,DWORD PTR 12[esp]
	mov	ebp,DWORD PTR 16[esp]
	xor	edx,edx
	xor	ecx,ecx
	movd	mm4,DWORD PTR [edi]
	movd	mm5,DWORD PTR [esi]
	movd	mm3,DWORD PTR [ebp]
	pmuludq	mm5,mm4
	movq	mm2,mm5
	movq	mm0,mm5
	pand	mm0,mm7
	pmuludq	mm5,QWORD PTR 20[esp]
	pmuludq	mm3,mm5
	paddq	mm3,mm0
	movd	mm1,DWORD PTR 4[ebp]
	movd	mm0,DWORD PTR 4[esi]
	psrlq	mm2,32
	psrlq	mm3,32
	inc	ecx
ALIGN	16
$L0031st:
	pmuludq	mm0,mm4
	pmuludq	mm1,mm5
	paddq	mm2,mm0
	paddq	mm3,mm1
	movq	mm0,mm2
	pand	mm0,mm7
	movd	mm1,DWORD PTR 4[ecx*4+ebp]
	paddq	mm3,mm0
	movd	mm0,DWORD PTR 4[ecx*4+esi]
	psrlq	mm2,32
	movd	DWORD PTR 28[ecx*4+esp],mm3
	psrlq	mm3,32
	lea	ecx,DWORD PTR 1[ecx]
	cmp	ecx,ebx
	jl	$L0031st
	pmuludq	mm0,mm4
	pmuludq	mm1,mm5
	paddq	mm2,mm0
	paddq	mm3,mm1
	movq	mm0,mm2
	pand	mm0,mm7
	paddq	mm3,mm0
	movd	DWORD PTR 28[ecx*4+esp],mm3
	psrlq	mm2,32
	psrlq	mm3,32
	paddq	mm3,mm2
	movq	QWORD PTR 32[ebx*4+esp],mm3
	inc	edx
$L004outer:
	xor	ecx,ecx
	movd	mm4,DWORD PTR [edx*4+edi]
	movd	mm5,DWORD PTR [esi]
	movd	mm6,DWORD PTR 32[esp]
	movd	mm3,DWORD PTR [ebp]
	pmuludq	mm5,mm4
	paddq	mm5,mm6
	movq	mm0,mm5
	movq	mm2,mm5
	pand	mm0,mm7
	pmuludq	mm5,QWORD PTR 20[esp]
	pmuludq	mm3,mm5
	paddq	mm3,mm0
	movd	mm6,DWORD PTR 36[esp]
	movd	mm1,DWORD PTR 4[ebp]
	movd	mm0,DWORD PTR 4[esi]
	psrlq	mm2,32
	psrlq	mm3,32
	paddq	mm2,mm6
	inc	ecx
	dec	ebx
$L005inner:
	pmuludq	mm0,mm4
	pmuludq	mm1,mm5
	paddq	mm2,mm0
	paddq	mm3,mm1
	movq	mm0,mm2
	movd	mm6,DWORD PTR 36[ecx*4+esp]
	pand	mm0,mm7
	movd	mm1,DWORD PTR 4[ecx*4+ebp]
	paddq	mm3,mm0
	movd	mm0,DWORD PTR 4[ecx*4+esi]
	psrlq	mm2,32
	movd	DWORD PTR 28[ecx*4+esp],mm3
	psrlq	mm3,32
	paddq	mm2,mm6
	dec	ebx
	lea	ecx,DWORD PTR 1[ecx]
	jnz	$L005inner
	mov	ebx,ecx
	pmuludq	mm0,mm4
	pmuludq	mm1,mm5
	paddq	mm2,mm0
	paddq	mm3,mm1
	movq	mm0,mm2
	pand	mm0,mm7
	paddq	mm3,mm0
	movd	DWORD PTR 28[ecx*4+esp],mm3
	psrlq	mm2,32
	psrlq	mm3,32
	movd	mm6,DWORD PTR 36[ebx*4+esp]
	paddq	mm3,mm2
	paddq	mm3,mm6
	movq	QWORD PTR 32[ebx*4+esp],mm3
	lea	edx,DWORD PTR 1[edx]
	cmp	edx,ebx
	jle	$L004outer
	emms
	jmp	$L006common_tail
ALIGN	16
$L002non_sse2:
	mov	esi,DWORD PTR 8[esp]
	lea	ebp,DWORD PTR 1[ebx]
	mov	edi,DWORD PTR 12[esp]
	xor	ecx,ecx
	mov	edx,esi
	and	ebp,1
	sub	edx,edi
	lea	eax,DWORD PTR 4[ebx*4+edi]
	or	ebp,edx
	mov	edi,DWORD PTR [edi]
	jz	$L007bn_sqr_mont
	mov	DWORD PTR 28[esp],eax
	mov	eax,DWORD PTR [esi]
	xor	edx,edx
ALIGN	16
$L008mull:
	mov	ebp,edx
	mul	edi
	add	ebp,eax
	lea	ecx,DWORD PTR 1[ecx]
	adc	edx,0
	mov	eax,DWORD PTR [ecx*4+esi]
	cmp	ecx,ebx
	mov	DWORD PTR 28[ecx*4+esp],ebp
	jl	$L008mull
	mov	ebp,edx
	mul	edi
	mov	edi,DWORD PTR 20[esp]
	add	eax,ebp
	mov	esi,DWORD PTR 16[esp]
	adc	edx,0
	imul	edi,DWORD PTR 32[esp]
	mov	DWORD PTR 32[ebx*4+esp],eax
	xor	ecx,ecx
	mov	DWORD PTR 36[ebx*4+esp],edx
	mov	DWORD PTR 40[ebx*4+esp],ecx
	mov	eax,DWORD PTR [esi]
	mul	edi
	add	eax,DWORD PTR 32[esp]
	mov	eax,DWORD PTR 4[esi]
	adc	edx,0
	inc	ecx
	jmp	$L0092ndmadd
ALIGN	16
$L0101stmadd:
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 32[ecx*4+esp]
	lea	ecx,DWORD PTR 1[ecx]
	adc	edx,0
	add	ebp,eax
	mov	eax,DWORD PTR [ecx*4+esi]
	adc	edx,0
	cmp	ecx,ebx
	mov	DWORD PTR 28[ecx*4+esp],ebp
	jl	$L0101stmadd
	mov	ebp,edx
	mul	edi
	add	eax,DWORD PTR 32[ebx*4+esp]
	mov	edi,DWORD PTR 20[esp]
	adc	edx,0
	mov	esi,DWORD PTR 16[esp]
	add	ebp,eax
	adc	edx,0
	imul	edi,DWORD PTR 32[esp]
	xor	ecx,ecx
	add	edx,DWORD PTR 36[ebx*4+esp]
	mov	DWORD PTR 32[ebx*4+esp],ebp
	adc	ecx,0
	mov	eax,DWORD PTR [esi]
	mov	DWORD PTR 36[ebx*4+esp],edx
	mov	DWORD PTR 40[ebx*4+esp],ecx
	mul	edi
	add	eax,DWORD PTR 32[esp]
	mov	eax,DWORD PTR 4[esi]
	adc	edx,0
	mov	ecx,1
ALIGN	16
$L0092ndmadd:
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 32[ecx*4+esp]
	lea	ecx,DWORD PTR 1[ecx]
	adc	edx,0
	add	ebp,eax
	mov	eax,DWORD PTR [ecx*4+esi]
	adc	edx,0
	cmp	ecx,ebx
	mov	DWORD PTR 24[ecx*4+esp],ebp
	jl	$L0092ndmadd
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 32[ebx*4+esp]
	adc	edx,0
	add	ebp,eax
	adc	edx,0
	mov	DWORD PTR 28[ebx*4+esp],ebp
	xor	eax,eax
	mov	ecx,DWORD PTR 12[esp]
	add	edx,DWORD PTR 36[ebx*4+esp]
	adc	eax,DWORD PTR 40[ebx*4+esp]
	lea	ecx,DWORD PTR 4[ecx]
	mov	DWORD PTR 32[ebx*4+esp],edx
	cmp	ecx,DWORD PTR 28[esp]
	mov	DWORD PTR 36[ebx*4+esp],eax
	je	$L006common_tail
	mov	edi,DWORD PTR [ecx]
	mov	esi,DWORD PTR 8[esp]
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,ecx
	xor	edx,edx
	mov	eax,DWORD PTR [esi]
	jmp	$L0101stmadd
ALIGN	16
$L007bn_sqr_mont:
	mov	DWORD PTR [esp],ebx
	mov	DWORD PTR 12[esp],ecx
	mov	eax,edi
	mul	edi
	mov	DWORD PTR 32[esp],eax
	mov	ebx,edx
	shr	edx,1
	and	ebx,1
	inc	ecx
ALIGN	16
$L011sqr:
	mov	eax,DWORD PTR [ecx*4+esi]
	mov	ebp,edx
	mul	edi
	add	eax,ebp
	lea	ecx,DWORD PTR 1[ecx]
	adc	edx,0
	lea	ebp,DWORD PTR [eax*2+ebx]
	shr	eax,31
	cmp	ecx,DWORD PTR [esp]
	mov	ebx,eax
	mov	DWORD PTR 28[ecx*4+esp],ebp
	jl	$L011sqr
	mov	eax,DWORD PTR [ecx*4+esi]
	mov	ebp,edx
	mul	edi
	add	eax,ebp
	mov	edi,DWORD PTR 20[esp]
	adc	edx,0
	mov	esi,DWORD PTR 16[esp]
	lea	ebp,DWORD PTR [eax*2+ebx]
	imul	edi,DWORD PTR 32[esp]
	shr	eax,31
	mov	DWORD PTR 32[ecx*4+esp],ebp
	lea	ebp,DWORD PTR [edx*2+eax]
	mov	eax,DWORD PTR [esi]
	shr	edx,31
	mov	DWORD PTR 36[ecx*4+esp],ebp
	mov	DWORD PTR 40[ecx*4+esp],edx
	mul	edi
	add	eax,DWORD PTR 32[esp]
	mov	ebx,ecx
	adc	edx,0
	mov	eax,DWORD PTR 4[esi]
	mov	ecx,1
ALIGN	16
$L0123rdmadd:
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 32[ecx*4+esp]
	adc	edx,0
	add	ebp,eax
	mov	eax,DWORD PTR 4[ecx*4+esi]
	adc	edx,0
	mov	DWORD PTR 28[ecx*4+esp],ebp
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 36[ecx*4+esp]
	lea	ecx,DWORD PTR 2[ecx]
	adc	edx,0
	add	ebp,eax
	mov	eax,DWORD PTR [ecx*4+esi]
	adc	edx,0
	cmp	ecx,ebx
	mov	DWORD PTR 24[ecx*4+esp],ebp
	jl	$L0123rdmadd
	mov	ebp,edx
	mul	edi
	add	ebp,DWORD PTR 32[ebx*4+esp]
	adc	edx,0
	add	ebp,eax
	adc	edx,0
	mov	DWORD PTR 28[ebx*4+esp],ebp
	mov	ecx,DWORD PTR 12[esp]
	xor	eax,eax
	mov	esi,DWORD PTR 8[esp]
	add	edx,DWORD PTR 36[ebx*4+esp]
	adc	eax,DWORD PTR 40[ebx*4+esp]
	mov	DWORD PTR 32[ebx*4+esp],edx
	cmp	ecx,ebx
	mov	DWORD PTR 36[ebx*4+esp],eax
	je	$L006common_tail
	mov	edi,DWORD PTR 4[ecx*4+esi]
	lea	ecx,DWORD PTR 1[ecx]
	mov	eax,edi
	mov	DWORD PTR 12[esp],ecx
	mul	edi
	add	eax,DWORD PTR 32[ecx*4+esp]
	adc	edx,0
	mov	DWORD PTR 32[ecx*4+esp],eax
	xor	ebp,ebp
	cmp	ecx,ebx
	lea	ecx,DWORD PTR 1[ecx]
	je	$L013sqrlast
	mov	ebx,edx
	shr	edx,1
	and	ebx,1
ALIGN	16
$L014sqradd:
	mov	eax,DWORD PTR [ecx*4+esi]
	mov	ebp,edx
	mul	edi
	add	eax,ebp
	lea	ebp,DWORD PTR [eax*1+eax]
	adc	edx,0
	shr	eax,31
	add	ebp,DWORD PTR 32[ecx*4+esp]
	lea	ecx,DWORD PTR 1[ecx]
	adc	eax,0
	add	ebp,ebx
	adc	eax,0
	cmp	ecx,DWORD PTR [esp]
	mov	DWORD PTR 28[ecx*4+esp],ebp
	mov	ebx,eax
	jle	$L014sqradd
	mov	ebp,edx
	add	edx,edx
	shr	ebp,31
	add	edx,ebx
	adc	ebp,0
$L013sqrlast:
	mov	edi,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 16[esp]
	imul	edi,DWORD PTR 32[esp]
	add	edx,DWORD PTR 32[ecx*4+esp]
	mov	eax,DWORD PTR [esi]
	adc	ebp,0
	mov	DWORD PTR 32[ecx*4+esp],edx
	mov	DWORD PTR 36[ecx*4+esp],ebp
	mul	edi
	add	eax,DWORD PTR 32[esp]
	lea	ebx,DWORD PTR [ecx-1]
	adc	edx,0
	mov	ecx,1
	mov	eax,DWORD PTR 4[esi]
	jmp	$L0123rdmadd
ALIGN	16
$L006common_tail:
	mov	ebp,DWORD PTR 16[esp]
	mov	edi,DWORD PTR 4[esp]
	lea	esi,DWORD PTR 32[esp]
	mov	eax,DWORD PTR [esi]
	mov	ecx,ebx
	xor	edx,edx
ALIGN	16
$L015sub:
	sbb	eax,DWORD PTR [edx*4+ebp]
	mov	DWORD PTR [edx*4+edi],eax
	dec	ecx
	mov	eax,DWORD PTR 4[edx*4+esi]
	lea	edx,DWORD PTR 1[edx]
	jge	$L015sub
	sbb	eax,0
	and	esi,eax
	not	eax
	mov	ebp,edi
	and	ebp,eax
	or	esi,ebp
ALIGN	16
$L016copy:
	mov	eax,DWORD PTR [ebx*4+esi]
	mov	DWORD PTR [ebx*4+edi],eax
	mov	DWORD PTR 32[ebx*4+esp],ecx
	dec	ebx
	jge	$L016copy
	mov	esp,DWORD PTR 24[esp]
	mov	eax,1
$L000just_leave:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_mul_mont ENDP
DB	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105
DB	112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56
DB	54,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121
DB	32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46
DB	111,114,103,62,0
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
