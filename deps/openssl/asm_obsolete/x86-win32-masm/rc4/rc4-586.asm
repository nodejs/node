TITLE	rc4-586.asm
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
_RC4	PROC PUBLIC
$L_RC4_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD PTR 20[esp]
	mov	edx,DWORD PTR 24[esp]
	mov	esi,DWORD PTR 28[esp]
	mov	ebp,DWORD PTR 32[esp]
	xor	eax,eax
	xor	ebx,ebx
	cmp	edx,0
	je	$L000abort
	mov	al,BYTE PTR [edi]
	mov	bl,BYTE PTR 4[edi]
	add	edi,8
	lea	ecx,DWORD PTR [edx*1+esi]
	sub	ebp,esi
	mov	DWORD PTR 24[esp],ecx
	inc	al
	cmp	DWORD PTR 256[edi],-1
	je	$L001RC4_CHAR
	mov	ecx,DWORD PTR [eax*4+edi]
	and	edx,-4
	jz	$L002loop1
	mov	DWORD PTR 32[esp],ebp
	test	edx,-8
	jz	$L003go4loop4
	lea	ebp,DWORD PTR _OPENSSL_ia32cap_P
	bt	DWORD PTR [ebp],26
	jnc	$L003go4loop4
	mov	ebp,DWORD PTR 32[esp]
	and	edx,-8
	lea	edx,DWORD PTR [edx*1+esi-8]
	mov	DWORD PTR [edi-4],edx
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	movq	mm0,QWORD PTR [esi]
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm2,DWORD PTR [edx*4+edi]
	jmp	$L004loop_mmx_enter
ALIGN	16
$L005loop_mmx:
	add	bl,cl
	psllq	mm1,56
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	movq	mm0,QWORD PTR [esi]
	movq	QWORD PTR [esi*1+ebp-8],mm2
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm2,DWORD PTR [edx*4+edi]
$L004loop_mmx_enter:
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm0
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,8
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,16
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,24
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,32
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,40
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	add	bl,cl
	psllq	mm1,48
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	inc	eax
	add	edx,ecx
	movzx	eax,al
	movzx	edx,dl
	pxor	mm2,mm1
	mov	ecx,DWORD PTR [eax*4+edi]
	movd	mm1,DWORD PTR [edx*4+edi]
	mov	edx,ebx
	xor	ebx,ebx
	mov	bl,dl
	cmp	esi,DWORD PTR [edi-4]
	lea	esi,DWORD PTR 8[esi]
	jb	$L005loop_mmx
	psllq	mm1,56
	pxor	mm2,mm1
	movq	QWORD PTR [esi*1+ebp-8],mm2
	emms
	cmp	esi,DWORD PTR 24[esp]
	je	$L006done
	jmp	$L002loop1
ALIGN	16
$L003go4loop4:
	lea	edx,DWORD PTR [edx*1+esi-4]
	mov	DWORD PTR 28[esp],edx
$L007loop4:
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	add	edx,ecx
	inc	al
	and	edx,255
	mov	ecx,DWORD PTR [eax*4+edi]
	mov	ebp,DWORD PTR [edx*4+edi]
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	add	edx,ecx
	inc	al
	and	edx,255
	ror	ebp,8
	mov	ecx,DWORD PTR [eax*4+edi]
	or	ebp,DWORD PTR [edx*4+edi]
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	add	edx,ecx
	inc	al
	and	edx,255
	ror	ebp,8
	mov	ecx,DWORD PTR [eax*4+edi]
	or	ebp,DWORD PTR [edx*4+edi]
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	add	edx,ecx
	inc	al
	and	edx,255
	ror	ebp,8
	mov	ecx,DWORD PTR 32[esp]
	or	ebp,DWORD PTR [edx*4+edi]
	ror	ebp,8
	xor	ebp,DWORD PTR [esi]
	cmp	esi,DWORD PTR 28[esp]
	mov	DWORD PTR [esi*1+ecx],ebp
	lea	esi,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR [eax*4+edi]
	jb	$L007loop4
	cmp	esi,DWORD PTR 24[esp]
	je	$L006done
	mov	ebp,DWORD PTR 32[esp]
ALIGN	16
$L002loop1:
	add	bl,cl
	mov	edx,DWORD PTR [ebx*4+edi]
	mov	DWORD PTR [ebx*4+edi],ecx
	mov	DWORD PTR [eax*4+edi],edx
	add	edx,ecx
	inc	al
	and	edx,255
	mov	edx,DWORD PTR [edx*4+edi]
	xor	dl,BYTE PTR [esi]
	lea	esi,DWORD PTR 1[esi]
	mov	ecx,DWORD PTR [eax*4+edi]
	cmp	esi,DWORD PTR 24[esp]
	mov	BYTE PTR [esi*1+ebp-1],dl
	jb	$L002loop1
	jmp	$L006done
ALIGN	16
$L001RC4_CHAR:
	movzx	ecx,BYTE PTR [eax*1+edi]
$L008cloop1:
	add	bl,cl
	movzx	edx,BYTE PTR [ebx*1+edi]
	mov	BYTE PTR [ebx*1+edi],cl
	mov	BYTE PTR [eax*1+edi],dl
	add	dl,cl
	movzx	edx,BYTE PTR [edx*1+edi]
	add	al,1
	xor	dl,BYTE PTR [esi]
	lea	esi,DWORD PTR 1[esi]
	movzx	ecx,BYTE PTR [eax*1+edi]
	cmp	esi,DWORD PTR 24[esp]
	mov	BYTE PTR [esi*1+ebp-1],dl
	jb	$L008cloop1
$L006done:
	dec	al
	mov	DWORD PTR [edi-4],ebx
	mov	BYTE PTR [edi-8],al
$L000abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_RC4 ENDP
ALIGN	16
_private_RC4_set_key	PROC PUBLIC
$L_private_RC4_set_key_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edi,DWORD PTR 20[esp]
	mov	ebp,DWORD PTR 24[esp]
	mov	esi,DWORD PTR 28[esp]
	lea	edx,DWORD PTR _OPENSSL_ia32cap_P
	lea	edi,DWORD PTR 8[edi]
	lea	esi,DWORD PTR [ebp*1+esi]
	neg	ebp
	xor	eax,eax
	mov	DWORD PTR [edi-4],ebp
	bt	DWORD PTR [edx],20
	jc	$L009c1stloop
ALIGN	16
$L010w1stloop:
	mov	DWORD PTR [eax*4+edi],eax
	add	al,1
	jnc	$L010w1stloop
	xor	ecx,ecx
	xor	edx,edx
ALIGN	16
$L011w2ndloop:
	mov	eax,DWORD PTR [ecx*4+edi]
	add	dl,BYTE PTR [ebp*1+esi]
	add	dl,al
	add	ebp,1
	mov	ebx,DWORD PTR [edx*4+edi]
	jnz	$L012wnowrap
	mov	ebp,DWORD PTR [edi-4]
$L012wnowrap:
	mov	DWORD PTR [edx*4+edi],eax
	mov	DWORD PTR [ecx*4+edi],ebx
	add	cl,1
	jnc	$L011w2ndloop
	jmp	$L013exit
ALIGN	16
$L009c1stloop:
	mov	BYTE PTR [eax*1+edi],al
	add	al,1
	jnc	$L009c1stloop
	xor	ecx,ecx
	xor	edx,edx
	xor	ebx,ebx
ALIGN	16
$L014c2ndloop:
	mov	al,BYTE PTR [ecx*1+edi]
	add	dl,BYTE PTR [ebp*1+esi]
	add	dl,al
	add	ebp,1
	mov	bl,BYTE PTR [edx*1+edi]
	jnz	$L015cnowrap
	mov	ebp,DWORD PTR [edi-4]
$L015cnowrap:
	mov	BYTE PTR [edx*1+edi],al
	mov	BYTE PTR [ecx*1+edi],bl
	add	cl,1
	jnc	$L014c2ndloop
	mov	DWORD PTR 256[edi],-1
$L013exit:
	xor	eax,eax
	mov	DWORD PTR [edi-8],eax
	mov	DWORD PTR [edi-4],eax
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_private_RC4_set_key ENDP
ALIGN	16
_RC4_options	PROC PUBLIC
$L_RC4_options_begin::
	call	$L016pic_point
$L016pic_point:
	pop	eax
	lea	eax,DWORD PTR ($L017opts-$L016pic_point)[eax]
	lea	edx,DWORD PTR _OPENSSL_ia32cap_P
	mov	edx,DWORD PTR [edx]
	bt	edx,20
	jc	$L0181xchar
	bt	edx,26
	jnc	$L019ret
	add	eax,25
	ret
$L0181xchar:
	add	eax,12
$L019ret:
	ret
ALIGN	64
$L017opts:
DB	114,99,52,40,52,120,44,105,110,116,41,0
DB	114,99,52,40,49,120,44,99,104,97,114,41,0
DB	114,99,52,40,56,120,44,109,109,120,41,0
DB	82,67,52,32,102,111,114,32,120,56,54,44,32,67,82,89
DB	80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114
DB	111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
ALIGN	64
_RC4_options ENDP
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
