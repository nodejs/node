TITLE	rc5-586.asm
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
_RC5_32_encrypt	PROC PUBLIC
$L_RC5_32_encrypt_begin::
	;

	push	ebp
	push	esi
	push	edi
	mov	edx,DWORD PTR 16[esp]
	mov	ebp,DWORD PTR 20[esp]
	; Load the 2 words
	mov	edi,DWORD PTR [edx]
	mov	esi,DWORD PTR 4[edx]
	push	ebx
	mov	ebx,DWORD PTR [ebp]
	add	edi,DWORD PTR 4[ebp]
	add	esi,DWORD PTR 8[ebp]
	xor	edi,esi
	mov	eax,DWORD PTR 12[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 16[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 20[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 24[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 28[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 32[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 36[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 40[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 44[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 48[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 52[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 56[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 60[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 64[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 68[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 72[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	cmp	ebx,8
	je	$L000rc5_exit
	xor	edi,esi
	mov	eax,DWORD PTR 76[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 80[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 84[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 88[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 92[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 96[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 100[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 104[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	cmp	ebx,12
	je	$L000rc5_exit
	xor	edi,esi
	mov	eax,DWORD PTR 108[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 112[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 116[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 120[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 124[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 128[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
	xor	edi,esi
	mov	eax,DWORD PTR 132[ebp]
	mov	ecx,esi
	rol	edi,cl
	add	edi,eax
	xor	esi,edi
	mov	eax,DWORD PTR 136[ebp]
	mov	ecx,edi
	rol	esi,cl
	add	esi,eax
$L000rc5_exit:
	mov	DWORD PTR [edx],edi
	mov	DWORD PTR 4[edx],esi
	pop	ebx
	pop	edi
	pop	esi
	pop	ebp
	ret
_RC5_32_encrypt ENDP
ALIGN	16
_RC5_32_decrypt	PROC PUBLIC
$L_RC5_32_decrypt_begin::
	;

	push	ebp
	push	esi
	push	edi
	mov	edx,DWORD PTR 16[esp]
	mov	ebp,DWORD PTR 20[esp]
	; Load the 2 words
	mov	edi,DWORD PTR [edx]
	mov	esi,DWORD PTR 4[edx]
	push	ebx
	mov	ebx,DWORD PTR [ebp]
	cmp	ebx,12
	je	$L001rc5_dec_12
	cmp	ebx,8
	je	$L002rc5_dec_8
	mov	eax,DWORD PTR 136[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 132[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 128[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 124[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 120[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 116[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 112[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 108[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
$L001rc5_dec_12:
	mov	eax,DWORD PTR 104[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 100[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 96[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 92[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 88[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 84[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 80[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 76[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
$L002rc5_dec_8:
	mov	eax,DWORD PTR 72[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 68[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 64[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 60[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 56[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 52[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 48[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 44[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 40[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 36[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 32[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 28[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 24[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 20[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	mov	eax,DWORD PTR 16[ebp]
	sub	esi,eax
	mov	ecx,edi
	ror	esi,cl
	xor	esi,edi
	mov	eax,DWORD PTR 12[ebp]
	sub	edi,eax
	mov	ecx,esi
	ror	edi,cl
	xor	edi,esi
	sub	esi,DWORD PTR 8[ebp]
	sub	edi,DWORD PTR 4[ebp]
$L003rc5_exit:
	mov	DWORD PTR [edx],edi
	mov	DWORD PTR 4[edx],esi
	pop	ebx
	pop	edi
	pop	esi
	pop	ebp
	ret
_RC5_32_decrypt ENDP
ALIGN	16
_RC5_32_cbc_encrypt	PROC PUBLIC
$L_RC5_32_cbc_encrypt_begin::
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
	jz	$L004decrypt
	and	ebp,4294967288
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	jz	$L005encrypt_finish
$L006encrypt_loop:
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR 4[esi]
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_RC5_32_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L006encrypt_loop
$L005encrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L007finish
	call	$L008PIC_point
$L008PIC_point:
	pop	edx
	lea	ecx,DWORD PTR ($L009cbc_enc_jmp_table-$L008PIC_point)[edx]
	mov	ebp,DWORD PTR [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
$L010ej7:
	mov	dh,BYTE PTR 6[esi]
	shl	edx,8
$L011ej6:
	mov	dh,BYTE PTR 5[esi]
$L012ej5:
	mov	dl,BYTE PTR 4[esi]
$L013ej4:
	mov	ecx,DWORD PTR [esi]
	jmp	$L014ejend
$L015ej3:
	mov	ch,BYTE PTR 2[esi]
	shl	ecx,8
$L016ej2:
	mov	ch,BYTE PTR 1[esi]
$L017ej1:
	mov	cl,BYTE PTR [esi]
$L014ejend:
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_RC5_32_encrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	jmp	$L007finish
$L004decrypt:
	and	ebp,4294967288
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	jz	$L018decrypt_finish
$L019decrypt_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_RC5_32_decrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
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
	jnz	$L019decrypt_loop
$L018decrypt_finish:
	mov	ebp,DWORD PTR 52[esp]
	and	ebp,7
	jz	$L007finish
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 8[esp],eax
	mov	DWORD PTR 12[esp],ebx
	call	$L_RC5_32_decrypt_begin
	mov	eax,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	mov	ecx,DWORD PTR 16[esp]
	mov	edx,DWORD PTR 20[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
$L020dj7:
	ror	edx,16
	mov	BYTE PTR 6[edi],dl
	shr	edx,16
$L021dj6:
	mov	BYTE PTR 5[edi],dh
$L022dj5:
	mov	BYTE PTR 4[edi],dl
$L023dj4:
	mov	DWORD PTR [edi],ecx
	jmp	$L024djend
$L025dj3:
	ror	ecx,16
	mov	BYTE PTR 2[edi],cl
	shl	ecx,16
$L026dj2:
	mov	BYTE PTR 1[esi],ch
$L027dj1:
	mov	BYTE PTR [esi],cl
$L024djend:
	jmp	$L007finish
$L007finish:
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
$L009cbc_enc_jmp_table:
DD	0
DD	$L017ej1-$L008PIC_point
DD	$L016ej2-$L008PIC_point
DD	$L015ej3-$L008PIC_point
DD	$L013ej4-$L008PIC_point
DD	$L012ej5-$L008PIC_point
DD	$L011ej6-$L008PIC_point
DD	$L010ej7-$L008PIC_point
ALIGN	64
_RC5_32_cbc_encrypt ENDP
.text$	ENDS
END
