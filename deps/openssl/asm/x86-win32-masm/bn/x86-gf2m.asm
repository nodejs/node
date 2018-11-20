TITLE	../openssl/crypto/bn/asm/x86-gf2m.asm
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
__mul_1x1_mmx	PROC PRIVATE
	sub	esp,36
	mov	ecx,eax
	lea	edx,DWORD PTR [eax*1+eax]
	and	ecx,1073741823
	lea	ebp,DWORD PTR [edx*1+edx]
	mov	DWORD PTR [esp],0
	and	edx,2147483647
	movd	mm2,eax
	movd	mm3,ebx
	mov	DWORD PTR 4[esp],ecx
	xor	ecx,edx
	pxor	mm5,mm5
	pxor	mm4,mm4
	mov	DWORD PTR 8[esp],edx
	xor	edx,ebp
	mov	DWORD PTR 12[esp],ecx
	pcmpgtd	mm5,mm2
	paddd	mm2,mm2
	xor	ecx,edx
	mov	DWORD PTR 16[esp],ebp
	xor	ebp,edx
	pand	mm5,mm3
	pcmpgtd	mm4,mm2
	mov	DWORD PTR 20[esp],ecx
	xor	ebp,ecx
	psllq	mm5,31
	pand	mm4,mm3
	mov	DWORD PTR 24[esp],edx
	mov	esi,7
	mov	DWORD PTR 28[esp],ebp
	mov	ebp,esi
	and	esi,ebx
	shr	ebx,3
	mov	edi,ebp
	psllq	mm4,30
	and	edi,ebx
	shr	ebx,3
	movd	mm0,DWORD PTR [esi*4+esp]
	mov	esi,ebp
	and	esi,ebx
	shr	ebx,3
	movd	mm2,DWORD PTR [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,3
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD PTR [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,6
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD PTR [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,9
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD PTR [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,12
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD PTR [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,15
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD PTR [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,18
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD PTR [edi*4+esp]
	mov	edi,ebp
	psllq	mm2,21
	and	edi,ebx
	shr	ebx,3
	pxor	mm0,mm2
	movd	mm1,DWORD PTR [esi*4+esp]
	mov	esi,ebp
	psllq	mm1,24
	and	esi,ebx
	shr	ebx,3
	pxor	mm0,mm1
	movd	mm2,DWORD PTR [edi*4+esp]
	pxor	mm0,mm4
	psllq	mm2,27
	pxor	mm0,mm2
	movd	mm1,DWORD PTR [esi*4+esp]
	pxor	mm0,mm5
	psllq	mm1,30
	add	esp,36
	pxor	mm0,mm1
	ret
__mul_1x1_mmx ENDP
ALIGN	16
__mul_1x1_ialu	PROC PRIVATE
	sub	esp,36
	mov	ecx,eax
	lea	edx,DWORD PTR [eax*1+eax]
	lea	ebp,DWORD PTR [eax*4]
	and	ecx,1073741823
	lea	edi,DWORD PTR [eax*1+eax]
	sar	eax,31
	mov	DWORD PTR [esp],0
	and	edx,2147483647
	mov	DWORD PTR 4[esp],ecx
	xor	ecx,edx
	mov	DWORD PTR 8[esp],edx
	xor	edx,ebp
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,edx
	mov	DWORD PTR 16[esp],ebp
	xor	ebp,edx
	mov	DWORD PTR 20[esp],ecx
	xor	ebp,ecx
	sar	edi,31
	and	eax,ebx
	mov	DWORD PTR 24[esp],edx
	and	edi,ebx
	mov	DWORD PTR 28[esp],ebp
	mov	edx,eax
	shl	eax,31
	mov	ecx,edi
	shr	edx,1
	mov	esi,7
	shl	edi,30
	and	esi,ebx
	shr	ecx,2
	xor	eax,edi
	shr	ebx,3
	mov	edi,7
	and	edi,ebx
	shr	ebx,3
	xor	edx,ecx
	xor	eax,DWORD PTR [esi*4+esp]
	mov	esi,7
	and	esi,ebx
	shr	ebx,3
	mov	ebp,DWORD PTR [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,3
	and	edi,ebx
	shr	ecx,29
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD PTR [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,6
	and	esi,ebx
	shr	ebp,26
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD PTR [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,9
	and	edi,ebx
	shr	ecx,23
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD PTR [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,12
	and	esi,ebx
	shr	ebp,20
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD PTR [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,15
	and	edi,ebx
	shr	ecx,17
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD PTR [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,18
	and	esi,ebx
	shr	ebp,14
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD PTR [edi*4+esp]
	mov	edi,7
	mov	ecx,ebp
	shl	ebp,21
	and	edi,ebx
	shr	ecx,11
	xor	eax,ebp
	shr	ebx,3
	xor	edx,ecx
	mov	ecx,DWORD PTR [esi*4+esp]
	mov	esi,7
	mov	ebp,ecx
	shl	ecx,24
	and	esi,ebx
	shr	ebp,8
	xor	eax,ecx
	shr	ebx,3
	xor	edx,ebp
	mov	ebp,DWORD PTR [edi*4+esp]
	mov	ecx,ebp
	shl	ebp,27
	mov	edi,DWORD PTR [esi*4+esp]
	shr	ecx,5
	mov	esi,edi
	xor	eax,ebp
	shl	edi,30
	xor	edx,ecx
	shr	esi,2
	xor	eax,edi
	xor	edx,esi
	add	esp,36
	ret
__mul_1x1_ialu ENDP
ALIGN	16
_bn_GF2m_mul_2x2	PROC PUBLIC
$L_bn_GF2m_mul_2x2_begin::
	lea	edx,DWORD PTR _OPENSSL_ia32cap_P
	mov	eax,DWORD PTR [edx]
	mov	edx,DWORD PTR 4[edx]
	test	eax,8388608
	jz	$L000ialu
	test	eax,16777216
	jz	$L001mmx
	test	edx,2
	jz	$L001mmx
	movups	xmm0,XMMWORD PTR 8[esp]
	shufps	xmm0,xmm0,177
DB	102,15,58,68,192,1
	mov	eax,DWORD PTR 4[esp]
	movups	XMMWORD PTR [eax],xmm0
	ret
ALIGN	16
$L001mmx:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD PTR 24[esp]
	mov	ebx,DWORD PTR 32[esp]
	call	__mul_1x1_mmx
	movq	mm7,mm0
	mov	eax,DWORD PTR 28[esp]
	mov	ebx,DWORD PTR 36[esp]
	call	__mul_1x1_mmx
	movq	mm6,mm0
	mov	eax,DWORD PTR 24[esp]
	mov	ebx,DWORD PTR 32[esp]
	xor	eax,DWORD PTR 28[esp]
	xor	ebx,DWORD PTR 36[esp]
	call	__mul_1x1_mmx
	pxor	mm0,mm7
	mov	eax,DWORD PTR 20[esp]
	pxor	mm0,mm6
	movq	mm2,mm0
	psllq	mm0,32
	pop	edi
	psrlq	mm2,32
	pop	esi
	pxor	mm0,mm6
	pop	ebx
	pxor	mm2,mm7
	movq	QWORD PTR [eax],mm0
	pop	ebp
	movq	QWORD PTR 8[eax],mm2
	emms
	ret
ALIGN	16
$L000ialu:
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,20
	mov	eax,DWORD PTR 44[esp]
	mov	ebx,DWORD PTR 52[esp]
	call	__mul_1x1_ialu
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],edx
	mov	eax,DWORD PTR 48[esp]
	mov	ebx,DWORD PTR 56[esp]
	call	__mul_1x1_ialu
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],edx
	mov	eax,DWORD PTR 44[esp]
	mov	ebx,DWORD PTR 52[esp]
	xor	eax,DWORD PTR 48[esp]
	xor	ebx,DWORD PTR 56[esp]
	call	__mul_1x1_ialu
	mov	ebp,DWORD PTR 40[esp]
	mov	ebx,DWORD PTR [esp]
	mov	ecx,DWORD PTR 4[esp]
	mov	edi,DWORD PTR 8[esp]
	mov	esi,DWORD PTR 12[esp]
	xor	eax,edx
	xor	edx,ecx
	xor	eax,ebx
	mov	DWORD PTR [ebp],ebx
	xor	edx,edi
	mov	DWORD PTR 12[ebp],esi
	xor	eax,esi
	add	esp,20
	xor	edx,esi
	pop	edi
	xor	eax,edx
	pop	esi
	mov	DWORD PTR 8[ebp],edx
	pop	ebx
	mov	DWORD PTR 4[ebp],eax
	pop	ebp
	ret
_bn_GF2m_mul_2x2 ENDP
DB	71,70,40,50,94,109,41,32,77,117,108,116,105,112,108,105
DB	99,97,116,105,111,110,32,102,111,114,32,120,56,54,44,32
DB	67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB	112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB	62,0
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
