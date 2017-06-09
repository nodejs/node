TITLE	bf-586.asm
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
	;
	push	ebp
	push	ebx
	mov	ebx,DWORD PTR 12[esp]
	mov	ebp,DWORD PTR 16[esp]
	push	esi
	push	edi
	; Load the 2 words
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	xor	eax,eax
	mov	ebx,DWORD PTR [ebp]
	xor	ecx,ecx
	xor	edi,ebx
	;
	; Round 0
	mov	edx,DWORD PTR 4[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 1
	mov	edx,DWORD PTR 8[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 2
	mov	edx,DWORD PTR 12[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 3
	mov	edx,DWORD PTR 16[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 4
	mov	edx,DWORD PTR 20[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 5
	mov	edx,DWORD PTR 24[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 6
	mov	edx,DWORD PTR 28[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 7
	mov	edx,DWORD PTR 32[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 8
	mov	edx,DWORD PTR 36[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 9
	mov	edx,DWORD PTR 40[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 10
	mov	edx,DWORD PTR 44[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 11
	mov	edx,DWORD PTR 48[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 12
	mov	edx,DWORD PTR 52[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 13
	mov	edx,DWORD PTR 56[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 14
	mov	edx,DWORD PTR 60[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 15
	mov	edx,DWORD PTR 64[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	; Load parameter 0 (16) enc=1
	mov	eax,DWORD PTR 20[esp]
	xor	edi,ebx
	mov	edx,DWORD PTR 68[ebp]
	xor	esi,edx
	mov	DWORD PTR 4[eax],edi
	mov	DWORD PTR [eax],esi
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_BF_encrypt ENDP
ALIGN	16
_BF_decrypt	PROC PUBLIC
$L_BF_decrypt_begin::
	;
	push	ebp
	push	ebx
	mov	ebx,DWORD PTR 12[esp]
	mov	ebp,DWORD PTR 16[esp]
	push	esi
	push	edi
	; Load the 2 words
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	xor	eax,eax
	mov	ebx,DWORD PTR 68[ebp]
	xor	ecx,ecx
	xor	edi,ebx
	;
	; Round 16
	mov	edx,DWORD PTR 64[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 15
	mov	edx,DWORD PTR 60[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 14
	mov	edx,DWORD PTR 56[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 13
	mov	edx,DWORD PTR 52[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 12
	mov	edx,DWORD PTR 48[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 11
	mov	edx,DWORD PTR 44[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 10
	mov	edx,DWORD PTR 40[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 9
	mov	edx,DWORD PTR 36[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 8
	mov	edx,DWORD PTR 32[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 7
	mov	edx,DWORD PTR 28[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 6
	mov	edx,DWORD PTR 24[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 5
	mov	edx,DWORD PTR 20[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 4
	mov	edx,DWORD PTR 16[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 3
	mov	edx,DWORD PTR 12[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	edi,ebx
	;
	; Round 2
	mov	edx,DWORD PTR 8[ebp]
	mov	ebx,edi
	xor	esi,edx
	shr	ebx,16
	mov	edx,edi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	xor	eax,eax
	xor	esi,ebx
	;
	; Round 1
	mov	edx,DWORD PTR 4[ebp]
	mov	ebx,esi
	xor	edi,edx
	shr	ebx,16
	mov	edx,esi
	mov	al,bh
	and	ebx,255
	mov	cl,dh
	and	edx,255
	mov	eax,DWORD PTR 72[eax*4+ebp]
	mov	ebx,DWORD PTR 1096[ebx*4+ebp]
	add	ebx,eax
	mov	eax,DWORD PTR 2120[ecx*4+ebp]
	xor	ebx,eax
	mov	edx,DWORD PTR 3144[edx*4+ebp]
	add	ebx,edx
	; Load parameter 0 (1) enc=0
	mov	eax,DWORD PTR 20[esp]
	xor	edi,ebx
	mov	edx,DWORD PTR [ebp]
	xor	esi,edx
	mov	DWORD PTR 4[eax],edi
	mov	DWORD PTR [eax],esi
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
