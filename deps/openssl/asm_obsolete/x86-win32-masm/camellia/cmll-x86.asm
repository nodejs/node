TITLE	cmll-586.asm
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
_Camellia_EncryptBlock_Rounds	PROC PUBLIC
$L_Camellia_EncryptBlock_Rounds_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	lea	ecx,DWORD PTR [edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	lea	eax,DWORD PTR [eax*1+edi]
	mov	DWORD PTR 20[esp],ebx
	mov	DWORD PTR 16[esp],eax
	call	$L000pic_point
$L000pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L000pic_point)[ebp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	bswap	eax
	mov	edx,DWORD PTR 12[esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esp,DWORD PTR 20[esp]
	bswap	eax
	mov	esi,DWORD PTR 32[esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_EncryptBlock_Rounds ENDP
ALIGN	16
_Camellia_EncryptBlock	PROC PUBLIC
$L_Camellia_EncryptBlock_begin::
	mov	eax,128
	sub	eax,DWORD PTR 4[esp]
	mov	eax,3
	adc	eax,0
	mov	DWORD PTR 4[esp],eax
	jmp	$L_Camellia_EncryptBlock_Rounds_begin
_Camellia_EncryptBlock ENDP
ALIGN	16
_Camellia_encrypt	PROC PUBLIC
$L_Camellia_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	mov	eax,DWORD PTR 272[edi]
	lea	ecx,DWORD PTR [edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	lea	eax,DWORD PTR [eax*1+edi]
	mov	DWORD PTR 20[esp],ebx
	mov	DWORD PTR 16[esp],eax
	call	$L001pic_point
$L001pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L001pic_point)[ebp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	bswap	eax
	mov	edx,DWORD PTR 12[esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esp,DWORD PTR 20[esp]
	bswap	eax
	mov	esi,DWORD PTR 24[esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_encrypt ENDP
ALIGN	16
__x86_Camellia_encrypt	PROC PRIVATE
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR 16[edi]
	mov	DWORD PTR 4[esp],eax
	mov	DWORD PTR 8[esp],ebx
	mov	DWORD PTR 12[esp],ecx
	mov	DWORD PTR 16[esp],edx
ALIGN	16
$L002loop:
	xor	eax,esi
	xor	ebx,DWORD PTR 20[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 24[edi]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 28[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 32[edi]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	xor	eax,esi
	xor	ebx,DWORD PTR 36[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 40[edi]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 44[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 48[edi]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	xor	eax,esi
	xor	ebx,DWORD PTR 52[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 56[edi]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 60[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 64[edi]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	add	edi,64
	cmp	edi,DWORD PTR 20[esp]
	je	$L003done
	and	esi,eax
	mov	edx,DWORD PTR 16[esp]
	rol	esi,1
	mov	ecx,edx
	xor	ebx,esi
	or	ecx,DWORD PTR 12[edi]
	mov	DWORD PTR 8[esp],ebx
	xor	ecx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 4[edi]
	mov	DWORD PTR 12[esp],ecx
	or	esi,ebx
	and	ecx,DWORD PTR 8[edi]
	xor	eax,esi
	rol	ecx,1
	mov	DWORD PTR 4[esp],eax
	xor	edx,ecx
	mov	esi,DWORD PTR 16[edi]
	mov	DWORD PTR 16[esp],edx
	jmp	$L002loop
ALIGN	8
$L003done:
	mov	ecx,eax
	mov	edx,ebx
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	xor	eax,esi
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	ret
__x86_Camellia_encrypt ENDP
ALIGN	16
_Camellia_DecryptBlock_Rounds	PROC PUBLIC
$L_Camellia_DecryptBlock_Rounds_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD PTR 20[esp]
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	lea	ecx,DWORD PTR [edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	mov	DWORD PTR 16[esp],edi
	lea	edi,DWORD PTR [eax*1+edi]
	mov	DWORD PTR 20[esp],ebx
	call	$L004pic_point
$L004pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L004pic_point)[ebp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	bswap	eax
	mov	edx,DWORD PTR 12[esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	esp,DWORD PTR 20[esp]
	bswap	eax
	mov	esi,DWORD PTR 32[esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_DecryptBlock_Rounds ENDP
ALIGN	16
_Camellia_DecryptBlock	PROC PUBLIC
$L_Camellia_DecryptBlock_begin::
	mov	eax,128
	sub	eax,DWORD PTR 4[esp]
	mov	eax,3
	adc	eax,0
	mov	DWORD PTR 4[esp],eax
	jmp	$L_Camellia_DecryptBlock_Rounds_begin
_Camellia_DecryptBlock ENDP
ALIGN	16
_Camellia_decrypt	PROC PUBLIC
$L_Camellia_decrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	mov	eax,DWORD PTR 272[edi]
	lea	ecx,DWORD PTR [edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	mov	DWORD PTR 16[esp],edi
	lea	edi,DWORD PTR [eax*1+edi]
	mov	DWORD PTR 20[esp],ebx
	call	$L005pic_point
$L005pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L005pic_point)[ebp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	bswap	eax
	mov	edx,DWORD PTR 12[esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	esp,DWORD PTR 20[esp]
	bswap	eax
	mov	esi,DWORD PTR 24[esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_decrypt ENDP
ALIGN	16
__x86_Camellia_decrypt	PROC PRIVATE
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	esi,DWORD PTR [edi-8]
	mov	DWORD PTR 4[esp],eax
	mov	DWORD PTR 8[esp],ebx
	mov	DWORD PTR 12[esp],ecx
	mov	DWORD PTR 16[esp],edx
ALIGN	16
$L006loop:
	xor	eax,esi
	xor	ebx,DWORD PTR [edi-4]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-16]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR [edi-12]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-24]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	xor	eax,esi
	xor	ebx,DWORD PTR [edi-20]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-32]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR [edi-28]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-40]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	xor	eax,esi
	xor	ebx,DWORD PTR [edi-36]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 16[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 12[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-48]
	xor	edx,ecx
	mov	DWORD PTR 16[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 12[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR [edi-44]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 8[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR 4[esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR [edi-56]
	xor	ebx,eax
	mov	DWORD PTR 8[esp],ebx
	xor	eax,edx
	mov	DWORD PTR 4[esp],eax
	sub	edi,64
	cmp	edi,DWORD PTR 20[esp]
	je	$L007done
	and	esi,eax
	mov	edx,DWORD PTR 16[esp]
	rol	esi,1
	mov	ecx,edx
	xor	ebx,esi
	or	ecx,DWORD PTR 4[edi]
	mov	DWORD PTR 8[esp],ebx
	xor	ecx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 12[edi]
	mov	DWORD PTR 12[esp],ecx
	or	esi,ebx
	and	ecx,DWORD PTR [edi]
	xor	eax,esi
	rol	ecx,1
	mov	DWORD PTR 4[esp],eax
	xor	edx,ecx
	mov	esi,DWORD PTR [edi-8]
	mov	DWORD PTR 16[esp],edx
	jmp	$L006loop
ALIGN	8
$L007done:
	mov	ecx,eax
	mov	edx,ebx
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	xor	ecx,esi
	xor	edx,DWORD PTR 12[edi]
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	ret
__x86_Camellia_decrypt ENDP
ALIGN	16
_Camellia_Ekeygen	PROC PUBLIC
$L_Camellia_Ekeygen_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,16
	mov	ebp,DWORD PTR 36[esp]
	mov	esi,DWORD PTR 40[esp]
	mov	edi,DWORD PTR 44[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	cmp	ebp,128
	je	$L0081st128
	mov	eax,DWORD PTR 16[esi]
	mov	ebx,DWORD PTR 20[esi]
	cmp	ebp,192
	je	$L0091st192
	mov	ecx,DWORD PTR 24[esi]
	mov	edx,DWORD PTR 28[esi]
	jmp	$L0101st256
ALIGN	4
$L0091st192:
	mov	ecx,eax
	mov	edx,ebx
	not	ecx
	not	edx
ALIGN	4
$L0101st256:
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD PTR 32[edi],eax
	mov	DWORD PTR 36[edi],ebx
	mov	DWORD PTR 40[edi],ecx
	mov	DWORD PTR 44[edi],edx
	xor	eax,DWORD PTR [edi]
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
ALIGN	4
$L0081st128:
	call	$L011pic_point
$L011pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L011pic_point)[ebp]
	lea	edi,DWORD PTR ($LCamellia_SIGMA-$LCamellia_SBOX)[ebp]
	mov	esi,DWORD PTR [edi]
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],edx
	xor	eax,esi
	xor	ebx,DWORD PTR 4[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 12[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 8[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 8[edi]
	xor	edx,ecx
	mov	DWORD PTR 12[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 8[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 12[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 4[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR [esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 16[edi]
	xor	ebx,eax
	mov	DWORD PTR 4[esp],ebx
	xor	eax,edx
	mov	DWORD PTR [esp],eax
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 44[esp]
	xor	eax,DWORD PTR [esi]
	xor	ebx,DWORD PTR 4[esi]
	xor	ecx,DWORD PTR 8[esi]
	xor	edx,DWORD PTR 12[esi]
	mov	esi,DWORD PTR 16[edi]
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],edx
	xor	eax,esi
	xor	ebx,DWORD PTR 20[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 12[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 8[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 24[edi]
	xor	edx,ecx
	mov	DWORD PTR 12[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 8[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 28[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 4[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR [esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 32[edi]
	xor	ebx,eax
	mov	DWORD PTR 4[esp],ebx
	xor	eax,edx
	mov	DWORD PTR [esp],eax
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	esi,DWORD PTR 36[esp]
	cmp	esi,128
	jne	$L0122nd256
	mov	edi,DWORD PTR 44[esp]
	lea	edi,DWORD PTR 128[edi]
	mov	DWORD PTR [edi-112],eax
	mov	DWORD PTR [edi-108],ebx
	mov	DWORD PTR [edi-104],ecx
	mov	DWORD PTR [edi-100],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD PTR [edi-80],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD PTR [edi-76],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR [edi-72],ecx
	mov	DWORD PTR [edi-68],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD PTR [edi-64],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD PTR [edi-60],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR [edi-56],ecx
	mov	DWORD PTR [edi-52],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD PTR [edi-32],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD PTR [edi-28],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD PTR [edi-16],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD PTR [edi-12],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR [edi-8],ecx
	mov	DWORD PTR [edi-4],edx
	mov	ebp,ebx
	shl	ebx,2
	mov	esi,ecx
	shr	esi,30
	shl	ecx,2
	or	ebx,esi
	mov	esi,edx
	shl	edx,2
	mov	DWORD PTR 32[edi],ebx
	shr	esi,30
	or	ecx,esi
	shr	ebp,30
	mov	esi,eax
	shr	esi,30
	mov	DWORD PTR 36[edi],ecx
	shl	eax,2
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 40[edi],edx
	mov	DWORD PTR 44[edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD PTR 64[edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD PTR 68[edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 72[edi],edx
	mov	DWORD PTR 76[edi],eax
	mov	ebx,DWORD PTR [edi-128]
	mov	ecx,DWORD PTR [edi-124]
	mov	edx,DWORD PTR [edi-120]
	mov	eax,DWORD PTR [edi-116]
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD PTR [edi-96],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD PTR [edi-92],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR [edi-88],edx
	mov	DWORD PTR [edi-84],eax
	mov	ebp,ebx
	shl	ebx,30
	mov	esi,ecx
	shr	esi,2
	shl	ecx,30
	or	ebx,esi
	mov	esi,edx
	shl	edx,30
	mov	DWORD PTR [edi-48],ebx
	shr	esi,2
	or	ecx,esi
	shr	ebp,2
	mov	esi,eax
	shr	esi,2
	mov	DWORD PTR [edi-44],ecx
	shl	eax,30
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR [edi-40],edx
	mov	DWORD PTR [edi-36],eax
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR [edi-24],edx
	mov	DWORD PTR [edi-20],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD PTR [edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD PTR 4[edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 8[edi],edx
	mov	DWORD PTR 12[edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD PTR 16[edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD PTR 20[edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 24[edi],edx
	mov	DWORD PTR 28[edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD PTR 48[edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD PTR 52[edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 56[edi],edx
	mov	DWORD PTR 60[edi],eax
	mov	eax,3
	jmp	$L013done
ALIGN	16
$L0122nd256:
	mov	esi,DWORD PTR 44[esp]
	mov	DWORD PTR 48[esi],eax
	mov	DWORD PTR 52[esi],ebx
	mov	DWORD PTR 56[esi],ecx
	mov	DWORD PTR 60[esi],edx
	xor	eax,DWORD PTR 32[esi]
	xor	ebx,DWORD PTR 36[esi]
	xor	ecx,DWORD PTR 40[esi]
	xor	edx,DWORD PTR 44[esi]
	mov	esi,DWORD PTR 32[edi]
	mov	DWORD PTR [esp],eax
	mov	DWORD PTR 4[esp],ebx
	mov	DWORD PTR 8[esp],ecx
	mov	DWORD PTR 12[esp],edx
	xor	eax,esi
	xor	ebx,DWORD PTR 36[edi]
	movzx	esi,ah
	mov	edx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD PTR 4[esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD PTR [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD PTR [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD PTR 4[esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD PTR 2048[eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD PTR 12[esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD PTR 8[esp]
	xor	edx,eax
	xor	ecx,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 40[edi]
	xor	edx,ecx
	mov	DWORD PTR 12[esp],edx
	xor	ecx,ebx
	mov	DWORD PTR 8[esp],ecx
	xor	ecx,esi
	xor	edx,DWORD PTR 44[edi]
	movzx	esi,ch
	mov	ebx,DWORD PTR 2052[esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD PTR 4[esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD PTR [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD PTR [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD PTR 4[esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD PTR 2048[ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD PTR 4[esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD PTR 2048[esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD PTR [esp]
	xor	ebx,ecx
	xor	eax,DWORD PTR 2052[esi*8+ebp]
	mov	esi,DWORD PTR 48[edi]
	xor	ebx,eax
	mov	DWORD PTR 4[esp],ebx
	xor	eax,edx
	mov	DWORD PTR [esp],eax
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	mov	edi,DWORD PTR 44[esp]
	lea	edi,DWORD PTR 128[edi]
	mov	DWORD PTR [edi-112],eax
	mov	DWORD PTR [edi-108],ebx
	mov	DWORD PTR [edi-104],ecx
	mov	DWORD PTR [edi-100],edx
	mov	ebp,eax
	shl	eax,30
	mov	esi,ebx
	shr	esi,2
	shl	ebx,30
	or	eax,esi
	mov	esi,ecx
	shl	ecx,30
	mov	DWORD PTR [edi-48],eax
	shr	esi,2
	or	ebx,esi
	shr	ebp,2
	mov	esi,edx
	shr	esi,2
	mov	DWORD PTR [edi-44],ebx
	shl	edx,30
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR [edi-40],ecx
	mov	DWORD PTR [edi-36],edx
	mov	ebp,eax
	shl	eax,30
	mov	esi,ebx
	shr	esi,2
	shl	ebx,30
	or	eax,esi
	mov	esi,ecx
	shl	ecx,30
	mov	DWORD PTR 32[edi],eax
	shr	esi,2
	or	ebx,esi
	shr	ebp,2
	mov	esi,edx
	shr	esi,2
	mov	DWORD PTR 36[edi],ebx
	shl	edx,30
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR 40[edi],ecx
	mov	DWORD PTR 44[edi],edx
	mov	ebp,ebx
	shl	ebx,19
	mov	esi,ecx
	shr	esi,13
	shl	ecx,19
	or	ebx,esi
	mov	esi,edx
	shl	edx,19
	mov	DWORD PTR 128[edi],ebx
	shr	esi,13
	or	ecx,esi
	shr	ebp,13
	mov	esi,eax
	shr	esi,13
	mov	DWORD PTR 132[edi],ecx
	shl	eax,19
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 136[edi],edx
	mov	DWORD PTR 140[edi],eax
	mov	ebx,DWORD PTR [edi-96]
	mov	ecx,DWORD PTR [edi-92]
	mov	edx,DWORD PTR [edi-88]
	mov	eax,DWORD PTR [edi-84]
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD PTR [edi-96],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD PTR [edi-92],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR [edi-88],edx
	mov	DWORD PTR [edi-84],eax
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD PTR [edi-64],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD PTR [edi-60],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR [edi-56],edx
	mov	DWORD PTR [edi-52],eax
	mov	ebp,ebx
	shl	ebx,30
	mov	esi,ecx
	shr	esi,2
	shl	ecx,30
	or	ebx,esi
	mov	esi,edx
	shl	edx,30
	mov	DWORD PTR 16[edi],ebx
	shr	esi,2
	or	ecx,esi
	shr	ebp,2
	mov	esi,eax
	shr	esi,2
	mov	DWORD PTR 20[edi],ecx
	shl	eax,30
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 24[edi],edx
	mov	DWORD PTR 28[edi],eax
	mov	ebp,ecx
	shl	ecx,2
	mov	esi,edx
	shr	esi,30
	shl	edx,2
	or	ecx,esi
	mov	esi,eax
	shl	eax,2
	mov	DWORD PTR 80[edi],ecx
	shr	esi,30
	or	edx,esi
	shr	ebp,30
	mov	esi,ebx
	shr	esi,30
	mov	DWORD PTR 84[edi],edx
	shl	ebx,2
	or	eax,esi
	or	ebx,ebp
	mov	DWORD PTR 88[edi],eax
	mov	DWORD PTR 92[edi],ebx
	mov	ecx,DWORD PTR [edi-80]
	mov	edx,DWORD PTR [edi-76]
	mov	eax,DWORD PTR [edi-72]
	mov	ebx,DWORD PTR [edi-68]
	mov	ebp,ecx
	shl	ecx,15
	mov	esi,edx
	shr	esi,17
	shl	edx,15
	or	ecx,esi
	mov	esi,eax
	shl	eax,15
	mov	DWORD PTR [edi-80],ecx
	shr	esi,17
	or	edx,esi
	shr	ebp,17
	mov	esi,ebx
	shr	esi,17
	mov	DWORD PTR [edi-76],edx
	shl	ebx,15
	or	eax,esi
	or	ebx,ebp
	mov	DWORD PTR [edi-72],eax
	mov	DWORD PTR [edi-68],ebx
	mov	ebp,ecx
	shl	ecx,30
	mov	esi,edx
	shr	esi,2
	shl	edx,30
	or	ecx,esi
	mov	esi,eax
	shl	eax,30
	mov	DWORD PTR [edi-16],ecx
	shr	esi,2
	or	edx,esi
	shr	ebp,2
	mov	esi,ebx
	shr	esi,2
	mov	DWORD PTR [edi-12],edx
	shl	ebx,30
	or	eax,esi
	or	ebx,ebp
	mov	DWORD PTR [edi-8],eax
	mov	DWORD PTR [edi-4],ebx
	mov	DWORD PTR 64[edi],edx
	mov	DWORD PTR 68[edi],eax
	mov	DWORD PTR 72[edi],ebx
	mov	DWORD PTR 76[edi],ecx
	mov	ebp,edx
	shl	edx,17
	mov	esi,eax
	shr	esi,15
	shl	eax,17
	or	edx,esi
	mov	esi,ebx
	shl	ebx,17
	mov	DWORD PTR 96[edi],edx
	shr	esi,15
	or	eax,esi
	shr	ebp,15
	mov	esi,ecx
	shr	esi,15
	mov	DWORD PTR 100[edi],eax
	shl	ecx,17
	or	ebx,esi
	or	ecx,ebp
	mov	DWORD PTR 104[edi],ebx
	mov	DWORD PTR 108[edi],ecx
	mov	edx,DWORD PTR [edi-128]
	mov	eax,DWORD PTR [edi-124]
	mov	ebx,DWORD PTR [edi-120]
	mov	ecx,DWORD PTR [edi-116]
	mov	ebp,eax
	shl	eax,13
	mov	esi,ebx
	shr	esi,19
	shl	ebx,13
	or	eax,esi
	mov	esi,ecx
	shl	ecx,13
	mov	DWORD PTR [edi-32],eax
	shr	esi,19
	or	ebx,esi
	shr	ebp,19
	mov	esi,edx
	shr	esi,19
	mov	DWORD PTR [edi-28],ebx
	shl	edx,13
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR [edi-24],ecx
	mov	DWORD PTR [edi-20],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD PTR [edi],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD PTR 4[edi],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ebp,eax
	shl	eax,17
	mov	esi,ebx
	shr	esi,15
	shl	ebx,17
	or	eax,esi
	mov	esi,ecx
	shl	ecx,17
	mov	DWORD PTR 48[edi],eax
	shr	esi,15
	or	ebx,esi
	shr	ebp,15
	mov	esi,edx
	shr	esi,15
	mov	DWORD PTR 52[edi],ebx
	shl	edx,17
	or	ecx,esi
	or	edx,ebp
	mov	DWORD PTR 56[edi],ecx
	mov	DWORD PTR 60[edi],edx
	mov	ebp,ebx
	shl	ebx,2
	mov	esi,ecx
	shr	esi,30
	shl	ecx,2
	or	ebx,esi
	mov	esi,edx
	shl	edx,2
	mov	DWORD PTR 112[edi],ebx
	shr	esi,30
	or	ecx,esi
	shr	ebp,30
	mov	esi,eax
	shr	esi,30
	mov	DWORD PTR 116[edi],ecx
	shl	eax,2
	or	edx,esi
	or	eax,ebp
	mov	DWORD PTR 120[edi],edx
	mov	DWORD PTR 124[edi],eax
	mov	eax,4
$L013done:
	lea	edx,DWORD PTR 144[edi]
	add	esp,16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_Ekeygen ENDP
ALIGN	16
_private_Camellia_set_key	PROC PUBLIC
$L_private_Camellia_set_key_begin::
	push	ebx
	mov	ecx,DWORD PTR 8[esp]
	mov	ebx,DWORD PTR 12[esp]
	mov	edx,DWORD PTR 16[esp]
	mov	eax,-1
	test	ecx,ecx
	jz	$L014done
	test	edx,edx
	jz	$L014done
	mov	eax,-2
	cmp	ebx,256
	je	$L015arg_ok
	cmp	ebx,192
	je	$L015arg_ok
	cmp	ebx,128
	jne	$L014done
ALIGN	4
$L015arg_ok:
	push	edx
	push	ecx
	push	ebx
	call	$L_Camellia_Ekeygen_begin
	add	esp,12
	mov	DWORD PTR [edx],eax
	xor	eax,eax
ALIGN	4
$L014done:
	pop	ebx
	ret
_private_Camellia_set_key ENDP
ALIGN	64
$LCamellia_SIGMA::
DD	2694735487,1003262091,3061508184,1286239154
DD	3337565999,3914302142,1426019237,4057165596
DD	283453434,3731369245,2958461122,3018244605
DD	0,0,0,0
ALIGN	64
$LCamellia_SBOX::
DD	1886416896,1886388336
DD	2189591040,741081132
DD	741092352,3014852787
DD	3974949888,3233808576
DD	3014898432,3840147684
DD	656877312,1465319511
DD	3233857536,3941204202
DD	3857048832,2930639022
DD	3840205824,589496355
DD	2240120064,1802174571
DD	1465341696,1162149957
DD	892679424,2779054245
DD	3941263872,3991732461
DD	202116096,1330577487
DD	2930683392,488439837
DD	1094795520,2459041938
DD	589505280,2256928902
DD	4025478912,2947481775
DD	1802201856,2088501372
DD	2475922176,522125343
DD	1162167552,1044250686
DD	421075200,3705405660
DD	2779096320,1583218782
DD	555819264,185270283
DD	3991792896,2795896998
DD	235802112,960036921
DD	1330597632,3587506389
DD	1313754624,1566376029
DD	488447232,3654877401
DD	1701143808,1515847770
DD	2459079168,1364262993
DD	3183328512,1819017324
DD	2256963072,2341142667
DD	3099113472,2593783962
DD	2947526400,4227531003
DD	2408550144,2964324528
DD	2088532992,1953759348
DD	3958106880,724238379
DD	522133248,4042260720
DD	3469659648,2223243396
DD	1044266496,3755933919
DD	808464384,3419078859
DD	3705461760,875823156
DD	1600085760,1987444854
DD	1583242752,1835860077
DD	3318072576,2846425257
DD	185273088,3520135377
DD	437918208,67371012
DD	2795939328,336855060
DD	3789676800,976879674
DD	960051456,3739091166
DD	3402287616,286326801
DD	3587560704,842137650
DD	1195853568,2627469468
DD	1566399744,1397948499
DD	1027423488,4075946226
DD	3654932736,4278059262
DD	16843008,3486449871
DD	1515870720,3284336835
DD	3604403712,2054815866
DD	1364283648,606339108
DD	1448498688,3907518696
DD	1819044864,1616904288
DD	1296911616,1768489065
DD	2341178112,2863268010
DD	218959104,2694840480
DD	2593823232,2711683233
DD	1717986816,1650589794
DD	4227595008,1414791252
DD	3435973632,505282590
DD	2964369408,3772776672
DD	757935360,1684275300
DD	1953788928,269484048
DD	303174144,0
DD	724249344,2745368739
DD	538976256,1970602101
DD	4042321920,2324299914
DD	2981212416,3873833190
DD	2223277056,151584777
DD	2576980224,3722248413
DD	3755990784,2273771655
DD	1280068608,2206400643
DD	3419130624,3452764365
DD	3267543552,2425356432
DD	875836416,1936916595
DD	2122219008,4143317238
DD	1987474944,2644312221
DD	84215040,3216965823
DD	1835887872,1381105746
DD	3082270464,3638034648
DD	2846468352,3368550600
DD	825307392,3334865094
DD	3520188672,2172715137
DD	387389184,1869545583
DD	67372032,320012307
DD	3621246720,1667432547
DD	336860160,3924361449
DD	1482184704,2812739751
DD	976894464,2677997727
DD	1633771776,3166437564
DD	3739147776,690552873
DD	454761216,4193845497
DD	286331136,791609391
DD	471604224,3031695540
DD	842150400,2021130360
DD	252645120,101056518
DD	2627509248,3890675943
DD	370546176,1903231089
DD	1397969664,3570663636
DD	404232192,2880110763
DD	4076007936,2290614408
DD	572662272,2374828173
DD	4278124032,1920073842
DD	1145324544,3115909305
DD	3486502656,4177002744
DD	2998055424,2896953516
DD	3284386560,909508662
DD	3048584448,707395626
DD	2054846976,1010565180
DD	2442236160,4059103473
DD	606348288,1077936192
DD	134744064,3553820883
DD	3907577856,3149594811
DD	2829625344,1128464451
DD	1616928768,353697813
DD	4244438016,2913796269
DD	1768515840,2004287607
DD	1347440640,2155872384
DD	2863311360,2189557890
DD	3503345664,3974889708
DD	2694881280,656867367
DD	2105376000,3856990437
DD	2711724288,2240086149
DD	2307492096,892665909
DD	1650614784,202113036
DD	2543294208,1094778945
DD	1414812672,4025417967
DD	1532713728,2475884691
DD	505290240,421068825
DD	2509608192,555810849
DD	3772833792,235798542
DD	4294967040,1313734734
DD	1684300800,1701118053
DD	3537031680,3183280317
DD	269488128,3099066552
DD	3301229568,2408513679
DD	0,3958046955
DD	1212696576,3469607118
DD	2745410304,808452144
DD	4160222976,1600061535
DD	1970631936,3318022341
DD	3688618752,437911578
DD	2324335104,3789619425
DD	50529024,3402236106
DD	3873891840,1195835463
DD	3671775744,1027407933
DD	151587072,16842753
DD	1061109504,3604349142
DD	3722304768,1448476758
DD	2492765184,1296891981
DD	2273806080,218955789
DD	1549556736,1717960806
DD	2206434048,3435921612
DD	33686016,757923885
DD	3452816640,303169554
DD	1246382592,538968096
DD	2425393152,2981167281
DD	858993408,2576941209
DD	1936945920,1280049228
DD	1734829824,3267494082
DD	4143379968,2122186878
DD	4092850944,84213765
DD	2644352256,3082223799
DD	2139062016,825294897
DD	3217014528,387383319
DD	3806519808,3621191895
DD	1381126656,1482162264
DD	2610666240,1633747041
DD	3638089728,454754331
DD	640034304,471597084
DD	3368601600,252641295
DD	926365440,370540566
DD	3334915584,404226072
DD	993737472,572653602
DD	2172748032,1145307204
DD	2526451200,2998010034
DD	1869573888,3048538293
DD	1263225600,2442199185
DD	320017152,134742024
DD	3200171520,2829582504
DD	1667457792,4244373756
DD	774778368,1347420240
DD	3924420864,3503292624
DD	2038003968,2105344125
DD	2812782336,2307457161
DD	2358021120,2543255703
DD	2678038272,1532690523
DD	1852730880,2509570197
DD	3166485504,4294902015
DD	2391707136,3536978130
DD	690563328,3301179588
DD	4126536960,1212678216
DD	4193908992,4160159991
DD	3065427456,3688562907
DD	791621376,50528259
DD	4261281024,3671720154
DD	3031741440,1061093439
DD	1499027712,2492727444
DD	2021160960,1549533276
DD	2560137216,33685506
DD	101058048,1246363722
DD	1785358848,858980403
DD	3890734848,1734803559
DD	1179010560,4092788979
DD	1903259904,2139029631
DD	3132799488,3806462178
DD	3570717696,2610626715
DD	623191296,640024614
DD	2880154368,926351415
DD	1111638528,993722427
DD	2290649088,2526412950
DD	2728567296,1263206475
DD	2374864128,3200123070
DD	4210752000,774766638
DD	1920102912,2037973113
DD	117901056,2357985420
DD	3115956480,1852702830
DD	1431655680,2391670926
DD	4177065984,4126474485
DD	4008635904,3065381046
DD	2896997376,4261216509
DD	168430080,1499005017
DD	909522432,2560098456
DD	1229539584,1785331818
DD	707406336,1178992710
DD	1751672832,3132752058
DD	1010580480,623181861
DD	943208448,1111621698
DD	4059164928,2728525986
DD	2762253312,4210688250
DD	1077952512,117899271
DD	673720320,1431634005
DD	3553874688,4008575214
DD	2071689984,168427530
DD	3149642496,1229520969
DD	3385444608,1751646312
DD	1128481536,943194168
DD	3250700544,2762211492
DD	353703168,673710120
DD	3823362816,2071658619
DD	2913840384,3385393353
DD	4109693952,3250651329
DD	2004317952,3823304931
DD	3351758592,4109631732
DD	2155905024,3351707847
DD	2661195264,2661154974
DD	14737632,939538488
DD	328965,1090535745
DD	5789784,369104406
DD	14277081,1979741814
DD	6776679,3640711641
DD	5131854,2466288531
DD	8487297,1610637408
DD	13355979,4060148466
DD	13224393,1912631922
DD	723723,3254829762
DD	11447982,2868947883
DD	6974058,2583730842
DD	14013909,1962964341
DD	1579032,100664838
DD	6118749,1459640151
DD	8553090,2684395680
DD	4605510,2432733585
DD	14671839,4144035831
DD	14079702,3036722613
DD	2565927,3372272073
DD	9079434,2717950626
DD	3289650,2348846220
DD	4934475,3523269330
DD	4342338,2415956112
DD	14408667,4127258358
DD	1842204,117442311
DD	10395294,2801837991
DD	10263708,654321447
DD	3815994,2382401166
DD	13290186,2986390194
DD	2434341,1224755529
DD	8092539,3724599006
DD	855309,1124090691
DD	7434609,1543527516
DD	6250335,3607156695
DD	2039583,3338717127
DD	16316664,1040203326
DD	14145495,4110480885
DD	4079166,2399178639
DD	10329501,1728079719
DD	8158332,520101663
DD	6316128,402659352
DD	12171705,1845522030
DD	12500670,2936057775
DD	12369084,788541231
DD	9145227,3791708898
DD	1447446,2231403909
DD	3421236,218107149
DD	5066061,1392530259
DD	12829635,4026593520
DD	7500402,2617285788
DD	9803157,1694524773
DD	11250603,3925928682
DD	9342606,2734728099
DD	12237498,2919280302
DD	8026746,2650840734
DD	11776947,3959483628
DD	131586,2147516544
DD	11842740,754986285
DD	11382189,1795189611
DD	10658466,2818615464
DD	11316396,721431339
DD	14211288,905983542
DD	10132122,2785060518
DD	1513239,3305162181
DD	1710618,2248181382
DD	3487029,1291865421
DD	13421772,855651123
DD	16250871,4244700669
DD	10066329,1711302246
DD	6381921,1476417624
DD	5921370,2516620950
DD	15263976,973093434
DD	2368548,150997257
DD	5658198,2499843477
DD	4210752,268439568
DD	14803425,2013296760
DD	6513507,3623934168
DD	592137,1107313218
DD	3355443,3422604492
DD	12566463,4009816047
DD	10000536,637543974
DD	9934743,3842041317
DD	8750469,1627414881
DD	6842472,436214298
DD	16579836,1056980799
DD	15527148,989870907
DD	657930,2181071490
DD	14342874,3053500086
DD	7303023,3674266587
DD	5460819,3556824276
DD	6447714,2550175896
DD	10724259,3892373736
DD	3026478,2332068747
DD	526344,33554946
DD	11513775,3942706155
DD	2631720,167774730
DD	11579568,738208812
DD	7631988,486546717
DD	12763842,2952835248
DD	12434877,1862299503
DD	3552822,2365623693
DD	2236962,2281736328
DD	3684408,234884622
DD	6579300,419436825
DD	1973790,2264958855
DD	3750201,1308642894
DD	2894892,184552203
DD	10921638,2835392937
DD	3158064,201329676
DD	15066597,2030074233
DD	4473924,285217041
DD	16645629,2130739071
DD	8947848,570434082
DD	10461087,3875596263
DD	6645093,1493195097
DD	8882055,3774931425
DD	7039851,3657489114
DD	16053492,1023425853
DD	2302755,3355494600
DD	4737096,301994514
DD	1052688,67109892
DD	13750737,1946186868
DD	5329233,1409307732
DD	12632256,805318704
DD	16382457,2113961598
DD	13816530,3019945140
DD	10526880,671098920
DD	5592405,1426085205
DD	10592673,1744857192
DD	4276545,1342197840
DD	16448250,3187719870
DD	4408131,3489714384
DD	1250067,3288384708
DD	12895428,822096177
DD	3092271,3405827019
DD	11053224,704653866
DD	11974326,2902502829
DD	3947580,251662095
DD	2829099,3389049546
DD	12698049,1879076976
DD	16777215,4278255615
DD	13158600,838873650
DD	10855845,1761634665
DD	2105376,134219784
DD	9013641,1644192354
DD	0,0
DD	9474192,603989028
DD	4671303,3506491857
DD	15724527,4211145723
DD	15395562,3120609978
DD	12040119,3976261101
DD	1381653,1157645637
DD	394758,2164294017
DD	13487565,1929409395
DD	11908533,1828744557
DD	1184274,2214626436
DD	8289918,2667618207
DD	12303291,3993038574
DD	2697513,1241533002
DD	986895,3271607235
DD	12105912,771763758
DD	460551,3238052289
DD	263172,16777473
DD	10197915,3858818790
DD	9737364,620766501
DD	2171169,1207978056
DD	6710886,2566953369
DD	15132390,3103832505
DD	13553358,3003167667
DD	15592941,2063629179
DD	15198183,4177590777
DD	3881787,3456159438
DD	16711422,3204497343
DD	8355711,3741376479
DD	12961221,1895854449
DD	10790052,687876393
DD	3618615,3439381965
DD	11645361,1811967084
DD	5000268,318771987
DD	9539985,1677747300
DD	7237230,2600508315
DD	9276813,1660969827
DD	7763574,2634063261
DD	197379,3221274816
DD	2960685,1258310475
DD	14606046,3070277559
DD	9868950,2768283045
DD	2500134,2298513801
DD	8224125,1593859935
DD	13027014,2969612721
DD	6052956,385881879
DD	13882323,4093703412
DD	15921906,3154164924
DD	5197647,3540046803
DD	1644825,1174423110
DD	4144959,3472936911
DD	14474460,922761015
DD	7960953,1577082462
DD	1907997,1191200583
DD	5395026,2483066004
DD	15461355,4194368250
DD	15987699,4227923196
DD	7171437,1526750043
DD	6184542,2533398423
DD	16514043,4261478142
DD	6908265,1509972570
DD	11711154,2885725356
DD	15790320,1006648380
DD	3223857,1275087948
DD	789516,50332419
DD	13948116,889206069
DD	13619151,4076925939
DD	9211020,587211555
DD	14869218,3087055032
DD	7697781,1560304989
DD	11119017,1778412138
DD	4868682,2449511058
DD	5723991,3573601749
DD	8684676,553656609
DD	1118481,1140868164
DD	4539717,1358975313
DD	1776411,3321939654
DD	16119285,2097184125
DD	15000804,956315961
DD	921102,2197848963
DD	7566195,3691044060
DD	11184810,2852170410
DD	15856113,2080406652
DD	14540253,1996519287
DD	5855577,1442862678
DD	1315860,83887365
DD	7105644,452991771
DD	9605778,2751505572
DD	5526612,352326933
DD	13684944,872428596
DD	7895160,503324190
DD	7368816,469769244
DD	14935011,4160813304
DD	4802889,1375752786
DD	8421504,536879136
DD	5263440,335549460
DD	10987431,3909151209
DD	16185078,3170942397
DD	7829367,3707821533
DD	9671571,3825263844
DD	8816262,2701173153
DD	8618883,3758153952
DD	2763306,2315291274
DD	13092807,4043370993
DD	5987163,3590379222
DD	15329769,2046851706
DD	15658734,3137387451
DD	9408399,3808486371
DD	65793,1073758272
DD	4013373,1325420367
ALIGN	16
_Camellia_cbc_encrypt	PROC PUBLIC
$L_Camellia_cbc_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ecx,DWORD PTR 28[esp]
	cmp	ecx,0
	je	$L016enc_out
	pushfd
	cld
	mov	eax,DWORD PTR 24[esp]
	mov	ebx,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 36[esp]
	mov	ebp,DWORD PTR 40[esp]
	lea	esi,DWORD PTR [esp-64]
	and	esi,-64
	lea	edi,DWORD PTR [edx-127]
	sub	edi,esi
	neg	edi
	and	edi,960
	sub	esi,edi
	mov	edi,DWORD PTR 44[esp]
	xchg	esp,esi
	add	esp,4
	mov	DWORD PTR 20[esp],esi
	mov	DWORD PTR 24[esp],eax
	mov	DWORD PTR 28[esp],ebx
	mov	DWORD PTR 32[esp],ecx
	mov	DWORD PTR 36[esp],edx
	mov	DWORD PTR 40[esp],ebp
	call	$L017pic_point
$L017pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($LCamellia_SBOX-$L017pic_point)[ebp]
	mov	esi,32
ALIGN	4
$L018prefetch_sbox:
	mov	eax,DWORD PTR [ebp]
	mov	ebx,DWORD PTR 32[ebp]
	mov	ecx,DWORD PTR 64[ebp]
	mov	edx,DWORD PTR 96[ebp]
	lea	ebp,DWORD PTR 128[ebp]
	dec	esi
	jnz	$L018prefetch_sbox
	mov	eax,DWORD PTR 36[esp]
	sub	ebp,4096
	mov	esi,DWORD PTR 24[esp]
	mov	edx,DWORD PTR 272[eax]
	cmp	edi,0
	je	$L019DECRYPT
	mov	ecx,DWORD PTR 32[esp]
	mov	edi,DWORD PTR 40[esp]
	shl	edx,6
	lea	edx,DWORD PTR [edx*1+eax]
	mov	DWORD PTR 16[esp],edx
	test	ecx,4294967280
	jz	$L020enc_tail
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
ALIGN	4
$L021enc_loop:
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	xor	eax,DWORD PTR [esi]
	xor	ebx,DWORD PTR 4[esi]
	xor	ecx,DWORD PTR 8[esi]
	bswap	eax
	xor	edx,DWORD PTR 12[esi]
	bswap	ebx
	mov	edi,DWORD PTR 36[esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	mov	DWORD PTR [edi],eax
	bswap	edx
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	ecx,DWORD PTR 32[esp]
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 24[esp],esi
	lea	edx,DWORD PTR 16[edi]
	mov	DWORD PTR 28[esp],edx
	sub	ecx,16
	test	ecx,4294967280
	mov	DWORD PTR 32[esp],ecx
	jnz	$L021enc_loop
	test	ecx,15
	jnz	$L020enc_tail
	mov	esi,DWORD PTR 40[esp]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	mov	esp,DWORD PTR 20[esp]
	popfd
$L016enc_out:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
ALIGN	4
$L020enc_tail:
	mov	eax,edi
	mov	edi,DWORD PTR 28[esp]
	push	eax
	mov	ebx,16
	sub	ebx,ecx
	cmp	edi,esi
	je	$L022enc_in_place
ALIGN	4
DD	2767451785
	jmp	$L023enc_skip_in_place
$L022enc_in_place:
	lea	edi,DWORD PTR [ecx*1+edi]
$L023enc_skip_in_place:
	mov	ecx,ebx
	xor	eax,eax
ALIGN	4
DD	2868115081
	pop	edi
	mov	esi,DWORD PTR 28[esp]
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	DWORD PTR 32[esp],16
	jmp	$L021enc_loop
ALIGN	16
$L019DECRYPT:
	shl	edx,6
	lea	edx,DWORD PTR [edx*1+eax]
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 36[esp],edx
	cmp	esi,DWORD PTR 28[esp]
	je	$L024dec_in_place
	mov	edi,DWORD PTR 40[esp]
	mov	DWORD PTR 44[esp],edi
ALIGN	4
$L025dec_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	bswap	eax
	mov	edx,DWORD PTR 12[esi]
	bswap	ebx
	mov	edi,DWORD PTR 36[esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	edi,DWORD PTR 44[esp]
	mov	esi,DWORD PTR 32[esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	xor	eax,DWORD PTR [edi]
	bswap	edx
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	sub	esi,16
	jc	$L026dec_partial
	mov	DWORD PTR 32[esp],esi
	mov	esi,DWORD PTR 24[esp]
	mov	edi,DWORD PTR 28[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	DWORD PTR 44[esp],esi
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 24[esp],esi
	lea	edi,DWORD PTR 16[edi]
	mov	DWORD PTR 28[esp],edi
	jnz	$L025dec_loop
	mov	edi,DWORD PTR 44[esp]
$L027dec_end:
	mov	esi,DWORD PTR 40[esp]
	mov	eax,DWORD PTR [edi]
	mov	ebx,DWORD PTR 4[edi]
	mov	ecx,DWORD PTR 8[edi]
	mov	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	jmp	$L028dec_out
ALIGN	4
$L026dec_partial:
	lea	edi,DWORD PTR 44[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	lea	ecx,DWORD PTR 16[esi]
	mov	esi,edi
	mov	edi,DWORD PTR 28[esp]
DD	2767451785
	mov	edi,DWORD PTR 24[esp]
	jmp	$L027dec_end
ALIGN	4
$L024dec_in_place:
$L029dec_in_place_loop:
	lea	edi,DWORD PTR 44[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	bswap	eax
	mov	DWORD PTR 12[edi],edx
	bswap	ebx
	mov	edi,DWORD PTR 36[esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	edi,DWORD PTR 40[esp]
	mov	esi,DWORD PTR 28[esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	xor	eax,DWORD PTR [edi]
	bswap	edx
	xor	ebx,DWORD PTR 4[edi]
	xor	ecx,DWORD PTR 8[edi]
	xor	edx,DWORD PTR 12[edi]
	mov	DWORD PTR [esi],eax
	mov	DWORD PTR 4[esi],ebx
	mov	DWORD PTR 8[esi],ecx
	mov	DWORD PTR 12[esi],edx
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 28[esp],esi
	lea	esi,DWORD PTR 44[esp]
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	ecx,DWORD PTR 8[esi]
	mov	edx,DWORD PTR 12[esi]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	mov	DWORD PTR 8[edi],ecx
	mov	DWORD PTR 12[edi],edx
	mov	esi,DWORD PTR 24[esp]
	lea	esi,DWORD PTR 16[esi]
	mov	DWORD PTR 24[esp],esi
	mov	ecx,DWORD PTR 32[esp]
	sub	ecx,16
	jc	$L030dec_in_place_partial
	mov	DWORD PTR 32[esp],ecx
	jnz	$L029dec_in_place_loop
	jmp	$L028dec_out
ALIGN	4
$L030dec_in_place_partial:
	mov	edi,DWORD PTR 28[esp]
	lea	esi,DWORD PTR 44[esp]
	lea	edi,DWORD PTR [ecx*1+edi]
	lea	esi,DWORD PTR 16[ecx*1+esi]
	neg	ecx
DD	2767451785
ALIGN	4
$L028dec_out:
	mov	esp,DWORD PTR 20[esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_Camellia_cbc_encrypt ENDP
DB	67,97,109,101,108,108,105,97,32,102,111,114,32,120,56,54
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
.text$	ENDS
END
