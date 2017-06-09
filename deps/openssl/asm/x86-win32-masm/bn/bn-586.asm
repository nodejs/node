TITLE	../openssl/crypto/bn/asm/bn-586.asm
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
_bn_mul_add_words	PROC PUBLIC
$L_bn_mul_add_words_begin::
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [eax],26
	jnc	$L000maw_non_sse2
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 8[esp]
	mov	ecx,DWORD PTR 12[esp]
	movd	mm0,DWORD PTR 16[esp]
	pxor	mm1,mm1
	jmp	$L001maw_sse2_entry
ALIGN	16
$L002maw_sse2_unrolled:
	movd	mm3,DWORD PTR [eax]
	paddq	mm1,mm3
	movd	mm2,DWORD PTR [edx]
	pmuludq	mm2,mm0
	movd	mm4,DWORD PTR 4[edx]
	pmuludq	mm4,mm0
	movd	mm6,DWORD PTR 8[edx]
	pmuludq	mm6,mm0
	movd	mm7,DWORD PTR 12[edx]
	pmuludq	mm7,mm0
	paddq	mm1,mm2
	movd	mm3,DWORD PTR 4[eax]
	paddq	mm3,mm4
	movd	mm5,DWORD PTR 8[eax]
	paddq	mm5,mm6
	movd	mm4,DWORD PTR 12[eax]
	paddq	mm7,mm4
	movd	DWORD PTR [eax],mm1
	movd	mm2,DWORD PTR 16[edx]
	pmuludq	mm2,mm0
	psrlq	mm1,32
	movd	mm4,DWORD PTR 20[edx]
	pmuludq	mm4,mm0
	paddq	mm1,mm3
	movd	mm6,DWORD PTR 24[edx]
	pmuludq	mm6,mm0
	movd	DWORD PTR 4[eax],mm1
	psrlq	mm1,32
	movd	mm3,DWORD PTR 28[edx]
	add	edx,32
	pmuludq	mm3,mm0
	paddq	mm1,mm5
	movd	mm5,DWORD PTR 16[eax]
	paddq	mm2,mm5
	movd	DWORD PTR 8[eax],mm1
	psrlq	mm1,32
	paddq	mm1,mm7
	movd	mm5,DWORD PTR 20[eax]
	paddq	mm4,mm5
	movd	DWORD PTR 12[eax],mm1
	psrlq	mm1,32
	paddq	mm1,mm2
	movd	mm5,DWORD PTR 24[eax]
	paddq	mm6,mm5
	movd	DWORD PTR 16[eax],mm1
	psrlq	mm1,32
	paddq	mm1,mm4
	movd	mm5,DWORD PTR 28[eax]
	paddq	mm3,mm5
	movd	DWORD PTR 20[eax],mm1
	psrlq	mm1,32
	paddq	mm1,mm6
	movd	DWORD PTR 24[eax],mm1
	psrlq	mm1,32
	paddq	mm1,mm3
	movd	DWORD PTR 28[eax],mm1
	lea	eax,DWORD PTR 32[eax]
	psrlq	mm1,32
	sub	ecx,8
	jz	$L003maw_sse2_exit
$L001maw_sse2_entry:
	test	ecx,4294967288
	jnz	$L002maw_sse2_unrolled
ALIGN	4
$L004maw_sse2_loop:
	movd	mm2,DWORD PTR [edx]
	movd	mm3,DWORD PTR [eax]
	pmuludq	mm2,mm0
	lea	edx,DWORD PTR 4[edx]
	paddq	mm1,mm3
	paddq	mm1,mm2
	movd	DWORD PTR [eax],mm1
	sub	ecx,1
	psrlq	mm1,32
	lea	eax,DWORD PTR 4[eax]
	jnz	$L004maw_sse2_loop
$L003maw_sse2_exit:
	movd	eax,mm1
	emms
	ret
ALIGN	16
$L000maw_non_sse2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	xor	esi,esi
	mov	edi,DWORD PTR 20[esp]
	mov	ecx,DWORD PTR 28[esp]
	mov	ebx,DWORD PTR 24[esp]
	and	ecx,4294967288
	mov	ebp,DWORD PTR 32[esp]
	push	ecx
	jz	$L005maw_finish
ALIGN	16
$L006maw_loop:
	; Round 0
	mov	eax,DWORD PTR [ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR [edi]
	adc	edx,0
	mov	DWORD PTR [edi],eax
	mov	esi,edx
	; Round 4
	mov	eax,DWORD PTR 4[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 4[edi]
	adc	edx,0
	mov	DWORD PTR 4[edi],eax
	mov	esi,edx
	; Round 8
	mov	eax,DWORD PTR 8[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 8[edi]
	adc	edx,0
	mov	DWORD PTR 8[edi],eax
	mov	esi,edx
	; Round 12
	mov	eax,DWORD PTR 12[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 12[edi]
	adc	edx,0
	mov	DWORD PTR 12[edi],eax
	mov	esi,edx
	; Round 16
	mov	eax,DWORD PTR 16[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 16[edi]
	adc	edx,0
	mov	DWORD PTR 16[edi],eax
	mov	esi,edx
	; Round 20
	mov	eax,DWORD PTR 20[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 20[edi]
	adc	edx,0
	mov	DWORD PTR 20[edi],eax
	mov	esi,edx
	; Round 24
	mov	eax,DWORD PTR 24[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 24[edi]
	adc	edx,0
	mov	DWORD PTR 24[edi],eax
	mov	esi,edx
	; Round 28
	mov	eax,DWORD PTR 28[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 28[edi]
	adc	edx,0
	mov	DWORD PTR 28[edi],eax
	mov	esi,edx
	;
	sub	ecx,8
	lea	ebx,DWORD PTR 32[ebx]
	lea	edi,DWORD PTR 32[edi]
	jnz	$L006maw_loop
$L005maw_finish:
	mov	ecx,DWORD PTR 32[esp]
	and	ecx,7
	jnz	$L007maw_finish2
	jmp	$L008maw_end
$L007maw_finish2:
	; Tail Round 0
	mov	eax,DWORD PTR [ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR [edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR [edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 1
	mov	eax,DWORD PTR 4[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 4[edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR 4[edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 2
	mov	eax,DWORD PTR 8[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 8[edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR 8[edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 3
	mov	eax,DWORD PTR 12[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 12[edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR 12[edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 4
	mov	eax,DWORD PTR 16[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 16[edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR 16[edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 5
	mov	eax,DWORD PTR 20[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 20[edi]
	adc	edx,0
	dec	ecx
	mov	DWORD PTR 20[edi],eax
	mov	esi,edx
	jz	$L008maw_end
	; Tail Round 6
	mov	eax,DWORD PTR 24[ebx]
	mul	ebp
	add	eax,esi
	adc	edx,0
	add	eax,DWORD PTR 24[edi]
	adc	edx,0
	mov	DWORD PTR 24[edi],eax
	mov	esi,edx
$L008maw_end:
	mov	eax,esi
	pop	ecx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_mul_add_words ENDP
ALIGN	16
_bn_mul_words	PROC PUBLIC
$L_bn_mul_words_begin::
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [eax],26
	jnc	$L009mw_non_sse2
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 8[esp]
	mov	ecx,DWORD PTR 12[esp]
	movd	mm0,DWORD PTR 16[esp]
	pxor	mm1,mm1
ALIGN	16
$L010mw_sse2_loop:
	movd	mm2,DWORD PTR [edx]
	pmuludq	mm2,mm0
	lea	edx,DWORD PTR 4[edx]
	paddq	mm1,mm2
	movd	DWORD PTR [eax],mm1
	sub	ecx,1
	psrlq	mm1,32
	lea	eax,DWORD PTR 4[eax]
	jnz	$L010mw_sse2_loop
	movd	eax,mm1
	emms
	ret
ALIGN	16
$L009mw_non_sse2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	xor	esi,esi
	mov	edi,DWORD PTR 20[esp]
	mov	ebx,DWORD PTR 24[esp]
	mov	ebp,DWORD PTR 28[esp]
	mov	ecx,DWORD PTR 32[esp]
	and	ebp,4294967288
	jz	$L011mw_finish
$L012mw_loop:
	; Round 0
	mov	eax,DWORD PTR [ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR [edi],eax
	mov	esi,edx
	; Round 4
	mov	eax,DWORD PTR 4[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 4[edi],eax
	mov	esi,edx
	; Round 8
	mov	eax,DWORD PTR 8[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 8[edi],eax
	mov	esi,edx
	; Round 12
	mov	eax,DWORD PTR 12[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 12[edi],eax
	mov	esi,edx
	; Round 16
	mov	eax,DWORD PTR 16[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 16[edi],eax
	mov	esi,edx
	; Round 20
	mov	eax,DWORD PTR 20[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 20[edi],eax
	mov	esi,edx
	; Round 24
	mov	eax,DWORD PTR 24[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 24[edi],eax
	mov	esi,edx
	; Round 28
	mov	eax,DWORD PTR 28[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 28[edi],eax
	mov	esi,edx
	;
	add	ebx,32
	add	edi,32
	sub	ebp,8
	jz	$L011mw_finish
	jmp	$L012mw_loop
$L011mw_finish:
	mov	ebp,DWORD PTR 28[esp]
	and	ebp,7
	jnz	$L013mw_finish2
	jmp	$L014mw_end
$L013mw_finish2:
	; Tail Round 0
	mov	eax,DWORD PTR [ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR [edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 1
	mov	eax,DWORD PTR 4[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 4[edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 2
	mov	eax,DWORD PTR 8[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 8[edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 3
	mov	eax,DWORD PTR 12[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 12[edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 4
	mov	eax,DWORD PTR 16[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 16[edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 5
	mov	eax,DWORD PTR 20[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 20[edi],eax
	mov	esi,edx
	dec	ebp
	jz	$L014mw_end
	; Tail Round 6
	mov	eax,DWORD PTR 24[ebx]
	mul	ecx
	add	eax,esi
	adc	edx,0
	mov	DWORD PTR 24[edi],eax
	mov	esi,edx
$L014mw_end:
	mov	eax,esi
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_mul_words ENDP
ALIGN	16
_bn_sqr_words	PROC PUBLIC
$L_bn_sqr_words_begin::
	lea	eax,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [eax],26
	jnc	$L015sqr_non_sse2
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 8[esp]
	mov	ecx,DWORD PTR 12[esp]
ALIGN	16
$L016sqr_sse2_loop:
	movd	mm0,DWORD PTR [edx]
	pmuludq	mm0,mm0
	lea	edx,DWORD PTR 4[edx]
	movq	QWORD PTR [eax],mm0
	sub	ecx,1
	lea	eax,DWORD PTR 8[eax]
	jnz	$L016sqr_sse2_loop
	emms
	ret
ALIGN	16
$L015sqr_non_sse2:
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	ebx,DWORD PTR 28[esp]
	and	ebx,4294967288
	jz	$L017sw_finish
$L018sw_loop:
	; Round 0
	mov	eax,DWORD PTR [edi]
	mul	eax
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],edx
	; Round 4
	mov	eax,DWORD PTR 4[edi]
	mul	eax
	mov	DWORD PTR 8[esi],eax
	mov	DWORD PTR 12[esi],edx
	; Round 8
	mov	eax,DWORD PTR 8[edi]
	mul	eax
	mov	DWORD PTR 16[esi],eax
	mov	DWORD PTR 20[esi],edx
	; Round 12
	mov	eax,DWORD PTR 12[edi]
	mul	eax
	mov	DWORD PTR 24[esi],eax
	mov	DWORD PTR 28[esi],edx
	; Round 16
	mov	eax,DWORD PTR 16[edi]
	mul	eax
	mov	DWORD PTR 32[esi],eax
	mov	DWORD PTR 36[esi],edx
	; Round 20
	mov	eax,DWORD PTR 20[edi]
	mul	eax
	mov	DWORD PTR 40[esi],eax
	mov	DWORD PTR 44[esi],edx
	; Round 24
	mov	eax,DWORD PTR 24[edi]
	mul	eax
	mov	DWORD PTR 48[esi],eax
	mov	DWORD PTR 52[esi],edx
	; Round 28
	mov	eax,DWORD PTR 28[edi]
	mul	eax
	mov	DWORD PTR 56[esi],eax
	mov	DWORD PTR 60[esi],edx
	;
	add	edi,32
	add	esi,64
	sub	ebx,8
	jnz	$L018sw_loop
$L017sw_finish:
	mov	ebx,DWORD PTR 28[esp]
	and	ebx,7
	jz	$L019sw_end
	; Tail Round 0
	mov	eax,DWORD PTR [edi]
	mul	eax
	mov	DWORD PTR [esi],eax
	dec	ebx
	mov	DWORD PTR 4[esi],edx
	jz	$L019sw_end
	; Tail Round 1
	mov	eax,DWORD PTR 4[edi]
	mul	eax
	mov	DWORD PTR 8[esi],eax
	dec	ebx
	mov	DWORD PTR 12[esi],edx
	jz	$L019sw_end
	; Tail Round 2
	mov	eax,DWORD PTR 8[edi]
	mul	eax
	mov	DWORD PTR 16[esi],eax
	dec	ebx
	mov	DWORD PTR 20[esi],edx
	jz	$L019sw_end
	; Tail Round 3
	mov	eax,DWORD PTR 12[edi]
	mul	eax
	mov	DWORD PTR 24[esi],eax
	dec	ebx
	mov	DWORD PTR 28[esi],edx
	jz	$L019sw_end
	; Tail Round 4
	mov	eax,DWORD PTR 16[edi]
	mul	eax
	mov	DWORD PTR 32[esi],eax
	dec	ebx
	mov	DWORD PTR 36[esi],edx
	jz	$L019sw_end
	; Tail Round 5
	mov	eax,DWORD PTR 20[edi]
	mul	eax
	mov	DWORD PTR 40[esi],eax
	dec	ebx
	mov	DWORD PTR 44[esi],edx
	jz	$L019sw_end
	; Tail Round 6
	mov	eax,DWORD PTR 24[edi]
	mul	eax
	mov	DWORD PTR 48[esi],eax
	mov	DWORD PTR 52[esi],edx
$L019sw_end:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_sqr_words ENDP
ALIGN	16
_bn_div_words	PROC PUBLIC
$L_bn_div_words_begin::
	mov	edx,DWORD PTR 4[esp]
	mov	eax,DWORD PTR 8[esp]
	mov	ecx,DWORD PTR 12[esp]
	div	ecx
	ret
_bn_div_words ENDP
ALIGN	16
_bn_add_words	PROC PUBLIC
$L_bn_add_words_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	mov	ebx,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebp,DWORD PTR 32[esp]
	xor	eax,eax
	and	ebp,4294967288
	jz	$L020aw_finish
$L021aw_loop:
	; Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	; Round 1
	mov	ecx,DWORD PTR 4[esi]
	mov	edx,DWORD PTR 4[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 4[ebx],ecx
	; Round 2
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 8[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 8[ebx],ecx
	; Round 3
	mov	ecx,DWORD PTR 12[esi]
	mov	edx,DWORD PTR 12[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 12[ebx],ecx
	; Round 4
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 16[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 16[ebx],ecx
	; Round 5
	mov	ecx,DWORD PTR 20[esi]
	mov	edx,DWORD PTR 20[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 20[ebx],ecx
	; Round 6
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 24[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
	; Round 7
	mov	ecx,DWORD PTR 28[esi]
	mov	edx,DWORD PTR 28[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 28[ebx],ecx
	;
	add	esi,32
	add	edi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L021aw_loop
$L020aw_finish:
	mov	ebp,DWORD PTR 32[esp]
	and	ebp,7
	jz	$L022aw_end
	; Tail Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR [ebx],ecx
	jz	$L022aw_end
	; Tail Round 1
	mov	ecx,DWORD PTR 4[esi]
	mov	edx,DWORD PTR 4[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 4[ebx],ecx
	jz	$L022aw_end
	; Tail Round 2
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 8[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 8[ebx],ecx
	jz	$L022aw_end
	; Tail Round 3
	mov	ecx,DWORD PTR 12[esi]
	mov	edx,DWORD PTR 12[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 12[ebx],ecx
	jz	$L022aw_end
	; Tail Round 4
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 16[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 16[ebx],ecx
	jz	$L022aw_end
	; Tail Round 5
	mov	ecx,DWORD PTR 20[esi]
	mov	edx,DWORD PTR 20[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 20[ebx],ecx
	jz	$L022aw_end
	; Tail Round 6
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 24[edi]
	add	ecx,eax
	mov	eax,0
	adc	eax,eax
	add	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
$L022aw_end:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_add_words ENDP
ALIGN	16
_bn_sub_words	PROC PUBLIC
$L_bn_sub_words_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	mov	ebx,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebp,DWORD PTR 32[esp]
	xor	eax,eax
	and	ebp,4294967288
	jz	$L023aw_finish
$L024aw_loop:
	; Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	; Round 1
	mov	ecx,DWORD PTR 4[esi]
	mov	edx,DWORD PTR 4[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 4[ebx],ecx
	; Round 2
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 8[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 8[ebx],ecx
	; Round 3
	mov	ecx,DWORD PTR 12[esi]
	mov	edx,DWORD PTR 12[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 12[ebx],ecx
	; Round 4
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 16[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 16[ebx],ecx
	; Round 5
	mov	ecx,DWORD PTR 20[esi]
	mov	edx,DWORD PTR 20[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 20[ebx],ecx
	; Round 6
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 24[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
	; Round 7
	mov	ecx,DWORD PTR 28[esi]
	mov	edx,DWORD PTR 28[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 28[ebx],ecx
	;
	add	esi,32
	add	edi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L024aw_loop
$L023aw_finish:
	mov	ebp,DWORD PTR 32[esp]
	and	ebp,7
	jz	$L025aw_end
	; Tail Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR [ebx],ecx
	jz	$L025aw_end
	; Tail Round 1
	mov	ecx,DWORD PTR 4[esi]
	mov	edx,DWORD PTR 4[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 4[ebx],ecx
	jz	$L025aw_end
	; Tail Round 2
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 8[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 8[ebx],ecx
	jz	$L025aw_end
	; Tail Round 3
	mov	ecx,DWORD PTR 12[esi]
	mov	edx,DWORD PTR 12[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 12[ebx],ecx
	jz	$L025aw_end
	; Tail Round 4
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 16[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 16[ebx],ecx
	jz	$L025aw_end
	; Tail Round 5
	mov	ecx,DWORD PTR 20[esi]
	mov	edx,DWORD PTR 20[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 20[ebx],ecx
	jz	$L025aw_end
	; Tail Round 6
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 24[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
$L025aw_end:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_sub_words ENDP
ALIGN	16
_bn_sub_part_words	PROC PUBLIC
$L_bn_sub_part_words_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	mov	ebx,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebp,DWORD PTR 32[esp]
	xor	eax,eax
	and	ebp,4294967288
	jz	$L026aw_finish
$L027aw_loop:
	; Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	; Round 1
	mov	ecx,DWORD PTR 4[esi]
	mov	edx,DWORD PTR 4[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 4[ebx],ecx
	; Round 2
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 8[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 8[ebx],ecx
	; Round 3
	mov	ecx,DWORD PTR 12[esi]
	mov	edx,DWORD PTR 12[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 12[ebx],ecx
	; Round 4
	mov	ecx,DWORD PTR 16[esi]
	mov	edx,DWORD PTR 16[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 16[ebx],ecx
	; Round 5
	mov	ecx,DWORD PTR 20[esi]
	mov	edx,DWORD PTR 20[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 20[ebx],ecx
	; Round 6
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 24[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
	; Round 7
	mov	ecx,DWORD PTR 28[esi]
	mov	edx,DWORD PTR 28[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 28[ebx],ecx
	;
	add	esi,32
	add	edi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L027aw_loop
$L026aw_finish:
	mov	ebp,DWORD PTR 32[esp]
	and	ebp,7
	jz	$L028aw_end
	; Tail Round 0
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 1
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 2
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 3
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 4
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 5
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
	dec	ebp
	jz	$L028aw_end
	; Tail Round 6
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	add	esi,4
	add	edi,4
	add	ebx,4
$L028aw_end:
	cmp	DWORD PTR 36[esp],0
	je	$L029pw_end
	mov	ebp,DWORD PTR 36[esp]
	cmp	ebp,0
	je	$L029pw_end
	jge	$L030pw_pos
	; pw_neg
	mov	edx,0
	sub	edx,ebp
	mov	ebp,edx
	and	ebp,4294967288
	jz	$L031pw_neg_finish
$L032pw_neg_loop:
	; dl<0 Round 0
	mov	ecx,0
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR [ebx],ecx
	; dl<0 Round 1
	mov	ecx,0
	mov	edx,DWORD PTR 4[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 4[ebx],ecx
	; dl<0 Round 2
	mov	ecx,0
	mov	edx,DWORD PTR 8[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 8[ebx],ecx
	; dl<0 Round 3
	mov	ecx,0
	mov	edx,DWORD PTR 12[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 12[ebx],ecx
	; dl<0 Round 4
	mov	ecx,0
	mov	edx,DWORD PTR 16[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 16[ebx],ecx
	; dl<0 Round 5
	mov	ecx,0
	mov	edx,DWORD PTR 20[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 20[ebx],ecx
	; dl<0 Round 6
	mov	ecx,0
	mov	edx,DWORD PTR 24[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
	; dl<0 Round 7
	mov	ecx,0
	mov	edx,DWORD PTR 28[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 28[ebx],ecx
	;
	add	edi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L032pw_neg_loop
$L031pw_neg_finish:
	mov	edx,DWORD PTR 36[esp]
	mov	ebp,0
	sub	ebp,edx
	and	ebp,7
	jz	$L029pw_end
	; dl<0 Tail Round 0
	mov	ecx,0
	mov	edx,DWORD PTR [edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR [ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 1
	mov	ecx,0
	mov	edx,DWORD PTR 4[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 4[ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 2
	mov	ecx,0
	mov	edx,DWORD PTR 8[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 8[ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 3
	mov	ecx,0
	mov	edx,DWORD PTR 12[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 12[ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 4
	mov	ecx,0
	mov	edx,DWORD PTR 16[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 16[ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 5
	mov	ecx,0
	mov	edx,DWORD PTR 20[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	dec	ebp
	mov	DWORD PTR 20[ebx],ecx
	jz	$L029pw_end
	; dl<0 Tail Round 6
	mov	ecx,0
	mov	edx,DWORD PTR 24[edi]
	sub	ecx,eax
	mov	eax,0
	adc	eax,eax
	sub	ecx,edx
	adc	eax,0
	mov	DWORD PTR 24[ebx],ecx
	jmp	$L029pw_end
$L030pw_pos:
	and	ebp,4294967288
	jz	$L033pw_pos_finish
$L034pw_pos_loop:
	; dl>0 Round 0
	mov	ecx,DWORD PTR [esi]
	sub	ecx,eax
	mov	DWORD PTR [ebx],ecx
	jnc	$L035pw_nc0
	; dl>0 Round 1
	mov	ecx,DWORD PTR 4[esi]
	sub	ecx,eax
	mov	DWORD PTR 4[ebx],ecx
	jnc	$L036pw_nc1
	; dl>0 Round 2
	mov	ecx,DWORD PTR 8[esi]
	sub	ecx,eax
	mov	DWORD PTR 8[ebx],ecx
	jnc	$L037pw_nc2
	; dl>0 Round 3
	mov	ecx,DWORD PTR 12[esi]
	sub	ecx,eax
	mov	DWORD PTR 12[ebx],ecx
	jnc	$L038pw_nc3
	; dl>0 Round 4
	mov	ecx,DWORD PTR 16[esi]
	sub	ecx,eax
	mov	DWORD PTR 16[ebx],ecx
	jnc	$L039pw_nc4
	; dl>0 Round 5
	mov	ecx,DWORD PTR 20[esi]
	sub	ecx,eax
	mov	DWORD PTR 20[ebx],ecx
	jnc	$L040pw_nc5
	; dl>0 Round 6
	mov	ecx,DWORD PTR 24[esi]
	sub	ecx,eax
	mov	DWORD PTR 24[ebx],ecx
	jnc	$L041pw_nc6
	; dl>0 Round 7
	mov	ecx,DWORD PTR 28[esi]
	sub	ecx,eax
	mov	DWORD PTR 28[ebx],ecx
	jnc	$L042pw_nc7
	;
	add	esi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L034pw_pos_loop
$L033pw_pos_finish:
	mov	ebp,DWORD PTR 36[esp]
	and	ebp,7
	jz	$L029pw_end
	; dl>0 Tail Round 0
	mov	ecx,DWORD PTR [esi]
	sub	ecx,eax
	mov	DWORD PTR [ebx],ecx
	jnc	$L043pw_tail_nc0
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 1
	mov	ecx,DWORD PTR 4[esi]
	sub	ecx,eax
	mov	DWORD PTR 4[ebx],ecx
	jnc	$L044pw_tail_nc1
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 2
	mov	ecx,DWORD PTR 8[esi]
	sub	ecx,eax
	mov	DWORD PTR 8[ebx],ecx
	jnc	$L045pw_tail_nc2
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 3
	mov	ecx,DWORD PTR 12[esi]
	sub	ecx,eax
	mov	DWORD PTR 12[ebx],ecx
	jnc	$L046pw_tail_nc3
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 4
	mov	ecx,DWORD PTR 16[esi]
	sub	ecx,eax
	mov	DWORD PTR 16[ebx],ecx
	jnc	$L047pw_tail_nc4
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 5
	mov	ecx,DWORD PTR 20[esi]
	sub	ecx,eax
	mov	DWORD PTR 20[ebx],ecx
	jnc	$L048pw_tail_nc5
	dec	ebp
	jz	$L029pw_end
	; dl>0 Tail Round 6
	mov	ecx,DWORD PTR 24[esi]
	sub	ecx,eax
	mov	DWORD PTR 24[ebx],ecx
	jnc	$L049pw_tail_nc6
	mov	eax,1
	jmp	$L029pw_end
$L050pw_nc_loop:
	mov	ecx,DWORD PTR [esi]
	mov	DWORD PTR [ebx],ecx
$L035pw_nc0:
	mov	ecx,DWORD PTR 4[esi]
	mov	DWORD PTR 4[ebx],ecx
$L036pw_nc1:
	mov	ecx,DWORD PTR 8[esi]
	mov	DWORD PTR 8[ebx],ecx
$L037pw_nc2:
	mov	ecx,DWORD PTR 12[esi]
	mov	DWORD PTR 12[ebx],ecx
$L038pw_nc3:
	mov	ecx,DWORD PTR 16[esi]
	mov	DWORD PTR 16[ebx],ecx
$L039pw_nc4:
	mov	ecx,DWORD PTR 20[esi]
	mov	DWORD PTR 20[ebx],ecx
$L040pw_nc5:
	mov	ecx,DWORD PTR 24[esi]
	mov	DWORD PTR 24[ebx],ecx
$L041pw_nc6:
	mov	ecx,DWORD PTR 28[esi]
	mov	DWORD PTR 28[ebx],ecx
$L042pw_nc7:
	;
	add	esi,32
	add	ebx,32
	sub	ebp,8
	jnz	$L050pw_nc_loop
	mov	ebp,DWORD PTR 36[esp]
	and	ebp,7
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR [esi]
	mov	DWORD PTR [ebx],ecx
$L043pw_tail_nc0:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 4[esi]
	mov	DWORD PTR 4[ebx],ecx
$L044pw_tail_nc1:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 8[esi]
	mov	DWORD PTR 8[ebx],ecx
$L045pw_tail_nc2:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 12[esi]
	mov	DWORD PTR 12[ebx],ecx
$L046pw_tail_nc3:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 16[esi]
	mov	DWORD PTR 16[ebx],ecx
$L047pw_tail_nc4:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 20[esi]
	mov	DWORD PTR 20[ebx],ecx
$L048pw_tail_nc5:
	dec	ebp
	jz	$L051pw_nc_end
	mov	ecx,DWORD PTR 24[esi]
	mov	DWORD PTR 24[ebx],ecx
$L049pw_tail_nc6:
$L051pw_nc_end:
	mov	eax,0
$L029pw_end:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_bn_sub_part_words ENDP
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
