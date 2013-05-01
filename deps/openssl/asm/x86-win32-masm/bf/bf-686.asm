TITLE	bf-686.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
ALIGN	16
_BF_encrypt	PROC PUBLIC
$L_BF_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;

	; Load the 2 words
	mov	eax,DWORD PTR 20[esp]
	mov	ecx,DWORD PTR [eax]
	mov	edx,DWORD PTR 4[eax]
	;

	; P pointer, s and enc flag
	mov	edi,DWORD PTR 24[esp]
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,DWORD PTR [edi]
	;

	; Round 0
	ror	ecx,16
	mov	esi,DWORD PTR 4[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 1
	ror	edx,16
	mov	esi,DWORD PTR 8[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 2
	ror	ecx,16
	mov	esi,DWORD PTR 12[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 3
	ror	edx,16
	mov	esi,DWORD PTR 16[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 4
	ror	ecx,16
	mov	esi,DWORD PTR 20[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 5
	ror	edx,16
	mov	esi,DWORD PTR 24[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 6
	ror	ecx,16
	mov	esi,DWORD PTR 28[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 7
	ror	edx,16
	mov	esi,DWORD PTR 32[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 8
	ror	ecx,16
	mov	esi,DWORD PTR 36[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 9
	ror	edx,16
	mov	esi,DWORD PTR 40[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 10
	ror	ecx,16
	mov	esi,DWORD PTR 44[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 11
	ror	edx,16
	mov	esi,DWORD PTR 48[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 12
	ror	ecx,16
	mov	esi,DWORD PTR 52[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 13
	ror	edx,16
	mov	esi,DWORD PTR 56[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 14
	ror	ecx,16
	mov	esi,DWORD PTR 60[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 15
	ror	edx,16
	mov	esi,DWORD PTR 64[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	xor	edx,DWORD PTR 68[edi]
	mov	eax,DWORD PTR 20[esp]
	mov	DWORD PTR [eax],edx
	mov	DWORD PTR 4[eax],ecx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_BF_encrypt ENDP
ALIGN	16
_BF_decrypt	PROC PUBLIC
$L_BF_decrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;

	; Load the 2 words
	mov	eax,DWORD PTR 20[esp]
	mov	ecx,DWORD PTR [eax]
	mov	edx,DWORD PTR 4[eax]
	;

	; P pointer, s and enc flag
	mov	edi,DWORD PTR 24[esp]
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,DWORD PTR 68[edi]
	;

	; Round 16
	ror	ecx,16
	mov	esi,DWORD PTR 64[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 15
	ror	edx,16
	mov	esi,DWORD PTR 60[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 14
	ror	ecx,16
	mov	esi,DWORD PTR 56[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 13
	ror	edx,16
	mov	esi,DWORD PTR 52[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 12
	ror	ecx,16
	mov	esi,DWORD PTR 48[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 11
	ror	edx,16
	mov	esi,DWORD PTR 44[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 10
	ror	ecx,16
	mov	esi,DWORD PTR 40[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 9
	ror	edx,16
	mov	esi,DWORD PTR 36[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 8
	ror	ecx,16
	mov	esi,DWORD PTR 32[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 7
	ror	edx,16
	mov	esi,DWORD PTR 28[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 6
	ror	ecx,16
	mov	esi,DWORD PTR 24[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 5
	ror	edx,16
	mov	esi,DWORD PTR 20[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 4
	ror	ecx,16
	mov	esi,DWORD PTR 16[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 3
	ror	edx,16
	mov	esi,DWORD PTR 12[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	;

	; Round 2
	ror	ecx,16
	mov	esi,DWORD PTR 8[edi]
	mov	al,ch
	mov	bl,cl
	ror	ecx,16
	xor	edx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,ch
	mov	bl,cl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	edx,esi
	;

	; Round 1
	ror	edx,16
	mov	esi,DWORD PTR 4[edi]
	mov	al,dh
	mov	bl,dl
	ror	edx,16
	xor	ecx,esi
	mov	esi,DWORD PTR 72[eax*4+edi]
	mov	ebp,DWORD PTR 1096[ebx*4+edi]
	mov	al,dh
	mov	bl,dl
	add	esi,ebp
	mov	eax,DWORD PTR 2120[eax*4+edi]
	xor	esi,eax
	mov	ebp,DWORD PTR 3144[ebx*4+edi]
	add	esi,ebp
	xor	eax,eax
	xor	ecx,esi
	xor	edx,DWORD PTR [edi]
	mov	eax,DWORD PTR 20[esp]
	mov	DWORD PTR [eax],edx
	mov	DWORD PTR 4[eax],ecx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_BF_decrypt ENDP
ALIGN	16
_BF_cbc_encrypt	PROC PUBLIC
$L_BF_cbc_encrypt_begin::
	;

	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD PTR 28[esp]
	; getting iv ptr from parameter 4
	mov	ebx,DWORD PTR 36[esp]
	mov	esi,DWORD PTR [ebx]
	mov	edi,DWORD PTR 4[ebx]
	push	edi
	push	esi
	push	edi
	push	esi
	mov	ebx,esp
	mov	esi,DWORD PTR 36[esp]
	mov	edi,DWORD PTR 40[esp]
	; getting encrypt flag from parameter 5
	mov	ecx,DWORD PTR 56[esp]
	; get and push parameter 3
	mov	eax,DWORD PTR 48[esp]
	push	eax
	push	ebx
	cmp	ecx,0
	jz	$L000decrypt
	and	ebp,4294967288
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	jz	$L001encrypt_finish
$L002encrypt_loop:
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR 4[esi]
	xor	eax,ecx
	xor	ebx,edx
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_BF_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L002encrypt_loop
$L001encrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L003finish
	call	$L004PIC_point
$L004PIC_point:
	pop	edx
	lea	ecx,DWORD PTR ($L005cbc_enc_jmp_table-$L004PIC_point)[edx]
	mov	ebp,DWORD PTR [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
$L006ej7:
	mov	dh,BYTE PTR 6[esi]
	shl	edx,8
$L007ej6:
	mov	dh,BYTE PTR 5[esi]
$L008ej5:
	mov	dl,BYTE PTR 4[esi]
$L009ej4:
	mov	ecx,DWORD PTR [esi]
	jmp	$L010ejend
$L011ej3:
	mov	ch,BYTE PTR 2[esi]
	shl	ecx,8
$L012ej2:
	mov	ch,BYTE PTR 1[esi]
$L013ej1:
	mov	cl,BYTE PTR [esi]
$L010ejend:
	xor	eax,ecx
	xor	ebx,edx
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_BF_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	jmp	$L003finish
$L000decrypt:
	and	ebp,4294967288
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	jz	$L014decrypt_finish
$L015decrypt_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_BF_decrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	ecx,DWORD PTR 16[esp]
	mov	edx,DWORD PTR 20[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR [edi],ecx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L015decrypt_loop
$L014decrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L003finish
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_BF_decrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	ecx,DWORD PTR 16[esp]
	mov	edx,DWORD PTR 20[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
$L016dj7:
	ror	edx,16
	mov	BYTE PTR 6[edi],dl
	shr	edx,16
$L017dj6:
	mov	BYTE PTR 5[edi],dh
$L018dj5:
	mov	BYTE PTR 4[edi],dl
$L019dj4:
	mov	DWORD PTR [edi],ecx
	jmp	$L020djend
$L021dj3:
	ror	ecx,16
	mov	BYTE PTR 2[edi],cl
	shl	ecx,16
$L022dj2:
	mov	BYTE PTR 1[esi],ch
$L023dj1:
	mov	BYTE PTR [esi],cl
$L020djend:
	jmp	$L003finish
$L003finish:
	mov	ecx,DWORD PTR 60[esp]
	add	esp,24
	mov	DWORD PTR [ecx],eax
	mov	DWORD PTR 4[ecx],ebx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L005cbc_enc_jmp_table:
DD	0
DD	$L013ej1-$L004PIC_point
DD	$L012ej2-$L004PIC_point
DD	$L011ej3-$L004PIC_point
DD	$L009ej4-$L004PIC_point
DD	$L008ej5-$L004PIC_point
DD	$L007ej6-$L004PIC_point
DD	$L006ej7-$L004PIC_point
ALIGN	64
_BF_cbc_encrypt ENDP
.text$	ENDS
END
