TITLE	cast-586.asm
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
EXTERN	_CAST_S_table0:NEAR
EXTERN	_CAST_S_table1:NEAR
EXTERN	_CAST_S_table2:NEAR
EXTERN	_CAST_S_table3:NEAR
ALIGN	16
_CAST_encrypt	PROC PUBLIC
$L_CAST_encrypt_begin::
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
	; Get short key flag
	mov	eax,DWORD PTR 128[ebp]
	push	eax
	xor	eax,eax
	; round 0
	mov	edx,DWORD PTR [ebp]
	mov	ecx,DWORD PTR 4[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 1
	mov	edx,DWORD PTR 8[ebp]
	mov	ecx,DWORD PTR 12[ebp]
	xor	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	esi,ecx
	; round 2
	mov	edx,DWORD PTR 16[ebp]
	mov	ecx,DWORD PTR 20[ebp]
	sub	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	edi,ecx
	; round 3
	mov	edx,DWORD PTR 24[ebp]
	mov	ecx,DWORD PTR 28[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
	; round 4
	mov	edx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 36[ebp]
	xor	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	edi,ecx
	; round 5
	mov	edx,DWORD PTR 40[ebp]
	mov	ecx,DWORD PTR 44[ebp]
	sub	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	esi,ecx
	; round 6
	mov	edx,DWORD PTR 48[ebp]
	mov	ecx,DWORD PTR 52[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 7
	mov	edx,DWORD PTR 56[ebp]
	mov	ecx,DWORD PTR 60[ebp]
	xor	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	esi,ecx
	; round 8
	mov	edx,DWORD PTR 64[ebp]
	mov	ecx,DWORD PTR 68[ebp]
	sub	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	edi,ecx
	; round 9
	mov	edx,DWORD PTR 72[ebp]
	mov	ecx,DWORD PTR 76[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
	; round 10
	mov	edx,DWORD PTR 80[ebp]
	mov	ecx,DWORD PTR 84[ebp]
	xor	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	edi,ecx
	; round 11
	mov	edx,DWORD PTR 88[ebp]
	mov	ecx,DWORD PTR 92[ebp]
	sub	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	esi,ecx
	; test short key flag
	pop	edx
	or	edx,edx
	jnz	$L000cast_enc_done
	; round 12
	mov	edx,DWORD PTR 96[ebp]
	mov	ecx,DWORD PTR 100[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 13
	mov	edx,DWORD PTR 104[ebp]
	mov	ecx,DWORD PTR 108[ebp]
	xor	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	esi,ecx
	; round 14
	mov	edx,DWORD PTR 112[ebp]
	mov	ecx,DWORD PTR 116[ebp]
	sub	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	edi,ecx
	; round 15
	mov	edx,DWORD PTR 120[ebp]
	mov	ecx,DWORD PTR 124[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
$L000cast_enc_done:
	nop
	mov	eax,DWORD PTR 20[esp]
	mov	DWORD PTR 4[eax],edi
	mov	DWORD PTR [eax],esi
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_CAST_encrypt ENDP
EXTERN	_CAST_S_table0:NEAR
EXTERN	_CAST_S_table1:NEAR
EXTERN	_CAST_S_table2:NEAR
EXTERN	_CAST_S_table3:NEAR
ALIGN	16
_CAST_decrypt	PROC PUBLIC
$L_CAST_decrypt_begin::
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
	; Get short key flag
	mov	eax,DWORD PTR 128[ebp]
	or	eax,eax
	jnz	$L001cast_dec_skip
	xor	eax,eax
	; round 15
	mov	edx,DWORD PTR 120[ebp]
	mov	ecx,DWORD PTR 124[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 14
	mov	edx,DWORD PTR 112[ebp]
	mov	ecx,DWORD PTR 116[ebp]
	sub	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	esi,ecx
	; round 13
	mov	edx,DWORD PTR 104[ebp]
	mov	ecx,DWORD PTR 108[ebp]
	xor	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	edi,ecx
	; round 12
	mov	edx,DWORD PTR 96[ebp]
	mov	ecx,DWORD PTR 100[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
$L001cast_dec_skip:
	; round 11
	mov	edx,DWORD PTR 88[ebp]
	mov	ecx,DWORD PTR 92[ebp]
	sub	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	edi,ecx
	; round 10
	mov	edx,DWORD PTR 80[ebp]
	mov	ecx,DWORD PTR 84[ebp]
	xor	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	esi,ecx
	; round 9
	mov	edx,DWORD PTR 72[ebp]
	mov	ecx,DWORD PTR 76[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 8
	mov	edx,DWORD PTR 64[ebp]
	mov	ecx,DWORD PTR 68[ebp]
	sub	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	esi,ecx
	; round 7
	mov	edx,DWORD PTR 56[ebp]
	mov	ecx,DWORD PTR 60[ebp]
	xor	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	edi,ecx
	; round 6
	mov	edx,DWORD PTR 48[ebp]
	mov	ecx,DWORD PTR 52[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
	; round 5
	mov	edx,DWORD PTR 40[ebp]
	mov	ecx,DWORD PTR 44[ebp]
	sub	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	edi,ecx
	; round 4
	mov	edx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 36[ebp]
	xor	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	esi,ecx
	; round 3
	mov	edx,DWORD PTR 24[ebp]
	mov	ecx,DWORD PTR 28[ebp]
	add	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	edi,ecx
	; round 2
	mov	edx,DWORD PTR 16[ebp]
	mov	ecx,DWORD PTR 20[ebp]
	sub	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	sub	ecx,ebx
	xor	esi,ecx
	; round 1
	mov	edx,DWORD PTR 8[ebp]
	mov	ecx,DWORD PTR 12[ebp]
	xor	edx,esi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	add	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	xor	ecx,ebx
	xor	edi,ecx
	; round 0
	mov	edx,DWORD PTR [ebp]
	mov	ecx,DWORD PTR 4[ebp]
	add	edx,edi
	rol	edx,cl
	mov	ebx,edx
	xor	ecx,ecx
	mov	cl,dh
	and	ebx,255
	shr	edx,16
	xor	eax,eax
	mov	al,dh
	and	edx,255
	mov	ecx,DWORD PTR _CAST_S_table0[ecx*4]
	mov	ebx,DWORD PTR _CAST_S_table1[ebx*4]
	xor	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table2[eax*4]
	sub	ecx,ebx
	mov	ebx,DWORD PTR _CAST_S_table3[edx*4]
	add	ecx,ebx
	xor	esi,ecx
	nop
	mov	eax,DWORD PTR 20[esp]
	mov	DWORD PTR 4[eax],edi
	mov	DWORD PTR [eax],esi
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_CAST_decrypt ENDP
ALIGN	16
_CAST_cbc_encrypt	PROC PUBLIC
$L_CAST_cbc_encrypt_begin::
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
	jz	$L002decrypt
	and	ebp,4294967288
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	jz	$L003encrypt_finish
$L004encrypt_loop:
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR 4[esi]
	xor	eax,ecx
	xor	ebx,edx
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_CAST_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L004encrypt_loop
$L003encrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L005finish
	call	$L006PIC_point
$L006PIC_point:
	pop	edx
	lea	ecx,DWORD PTR ($L007cbc_enc_jmp_table-$L006PIC_point)[edx]
	mov	ebp,DWORD PTR [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
$L008ej7:
	mov	dh,BYTE PTR 6[esi]
	shl	edx,8
$L009ej6:
	mov	dh,BYTE PTR 5[esi]
$L010ej5:
	mov	dl,BYTE PTR 4[esi]
$L011ej4:
	mov	ecx,DWORD PTR [esi]
	jmp	$L012ejend
$L013ej3:
	mov	ch,BYTE PTR 2[esi]
	shl	ecx,8
$L014ej2:
	mov	ch,BYTE PTR 1[esi]
$L015ej1:
	mov	cl,BYTE PTR [esi]
$L012ejend:
	xor	eax,ecx
	xor	ebx,edx
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_CAST_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	jmp	$L005finish
$L002decrypt:
	and	ebp,4294967288
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	jz	$L016decrypt_finish
$L017decrypt_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_CAST_decrypt_begin
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
	jnz	$L017decrypt_loop
$L016decrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L005finish
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	bswap	eax
	bswap	ebx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_CAST_decrypt_begin
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
$L018dj7:
	ror	edx,16
	mov	BYTE PTR 6[edi],dl
	shr	edx,16
$L019dj6:
	mov	BYTE PTR 5[edi],dh
$L020dj5:
	mov	BYTE PTR 4[edi],dl
$L021dj4:
	mov	DWORD PTR [edi],ecx
	jmp	$L022djend
$L023dj3:
	ror	ecx,16
	mov	BYTE PTR 2[edi],cl
	shl	ecx,16
$L024dj2:
	mov	BYTE PTR 1[esi],ch
$L025dj1:
	mov	BYTE PTR [esi],cl
$L022djend:
	jmp	$L005finish
$L005finish:
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
$L007cbc_enc_jmp_table:
DD	0
DD	$L015ej1-$L006PIC_point
DD	$L014ej2-$L006PIC_point
DD	$L013ej3-$L006PIC_point
DD	$L011ej4-$L006PIC_point
DD	$L010ej5-$L006PIC_point
DD	$L009ej6-$L006PIC_point
DD	$L008ej7-$L006PIC_point
ALIGN	64
_CAST_cbc_encrypt ENDP
.text$	ENDS
END
