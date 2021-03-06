%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
global	_Camellia_EncryptBlock_Rounds
align	16
_Camellia_EncryptBlock_Rounds:
L$_Camellia_EncryptBlock_Rounds_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edi,DWORD [28+esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	lea	ecx,[edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	lea	eax,[eax*1+edi]
	mov	DWORD [20+esp],ebx
	mov	DWORD [16+esp],eax
	call	L$000pic_point
L$000pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$000pic_point)+ebp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	bswap	eax
	mov	edx,DWORD [12+esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esp,DWORD [20+esp]
	bswap	eax
	mov	esi,DWORD [32+esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_Camellia_EncryptBlock
align	16
_Camellia_EncryptBlock:
L$_Camellia_EncryptBlock_begin:
	mov	eax,128
	sub	eax,DWORD [4+esp]
	mov	eax,3
	adc	eax,0
	mov	DWORD [4+esp],eax
	jmp	NEAR L$_Camellia_EncryptBlock_Rounds_begin
global	_Camellia_encrypt
align	16
_Camellia_encrypt:
L$_Camellia_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [28+esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	mov	eax,DWORD [272+edi]
	lea	ecx,[edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	lea	eax,[eax*1+edi]
	mov	DWORD [20+esp],ebx
	mov	DWORD [16+esp],eax
	call	L$001pic_point
L$001pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$001pic_point)+ebp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	bswap	eax
	mov	edx,DWORD [12+esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esp,DWORD [20+esp]
	bswap	eax
	mov	esi,DWORD [24+esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	16
__x86_Camellia_encrypt:
	xor	eax,DWORD [edi]
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
	mov	esi,DWORD [16+edi]
	mov	DWORD [4+esp],eax
	mov	DWORD [8+esp],ebx
	mov	DWORD [12+esp],ecx
	mov	DWORD [16+esp],edx
align	16
L$002loop:
	xor	eax,esi
	xor	ebx,DWORD [20+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [24+edi]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [28+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [32+edi]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	xor	eax,esi
	xor	ebx,DWORD [36+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [40+edi]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [44+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [48+edi]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	xor	eax,esi
	xor	ebx,DWORD [52+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [56+edi]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [60+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [64+edi]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	add	edi,64
	cmp	edi,DWORD [20+esp]
	je	NEAR L$003done
	and	esi,eax
	mov	edx,DWORD [16+esp]
	rol	esi,1
	mov	ecx,edx
	xor	ebx,esi
	or	ecx,DWORD [12+edi]
	mov	DWORD [8+esp],ebx
	xor	ecx,DWORD [12+esp]
	mov	esi,DWORD [4+edi]
	mov	DWORD [12+esp],ecx
	or	esi,ebx
	and	ecx,DWORD [8+edi]
	xor	eax,esi
	rol	ecx,1
	mov	DWORD [4+esp],eax
	xor	edx,ecx
	mov	esi,DWORD [16+edi]
	mov	DWORD [16+esp],edx
	jmp	NEAR L$002loop
align	8
L$003done:
	mov	ecx,eax
	mov	edx,ebx
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	xor	eax,esi
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
	ret
global	_Camellia_DecryptBlock_Rounds
align	16
_Camellia_DecryptBlock_Rounds:
L$_Camellia_DecryptBlock_Rounds_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	eax,DWORD [20+esp]
	mov	esi,DWORD [24+esp]
	mov	edi,DWORD [28+esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	lea	ecx,[edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	mov	DWORD [16+esp],edi
	lea	edi,[eax*1+edi]
	mov	DWORD [20+esp],ebx
	call	L$004pic_point
L$004pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$004pic_point)+ebp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	bswap	eax
	mov	edx,DWORD [12+esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	esp,DWORD [20+esp]
	bswap	eax
	mov	esi,DWORD [32+esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_Camellia_DecryptBlock
align	16
_Camellia_DecryptBlock:
L$_Camellia_DecryptBlock_begin:
	mov	eax,128
	sub	eax,DWORD [4+esp]
	mov	eax,3
	adc	eax,0
	mov	DWORD [4+esp],eax
	jmp	NEAR L$_Camellia_DecryptBlock_Rounds_begin
global	_Camellia_decrypt
align	16
_Camellia_decrypt:
L$_Camellia_decrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [28+esp]
	mov	ebx,esp
	sub	esp,28
	and	esp,-64
	mov	eax,DWORD [272+edi]
	lea	ecx,[edi-127]
	sub	ecx,esp
	neg	ecx
	and	ecx,960
	sub	esp,ecx
	add	esp,4
	shl	eax,6
	mov	DWORD [16+esp],edi
	lea	edi,[eax*1+edi]
	mov	DWORD [20+esp],ebx
	call	L$005pic_point
L$005pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$005pic_point)+ebp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	bswap	eax
	mov	edx,DWORD [12+esi]
	bswap	ebx
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	esp,DWORD [20+esp]
	bswap	eax
	mov	esi,DWORD [24+esp]
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	16
__x86_Camellia_decrypt:
	xor	eax,DWORD [edi]
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
	mov	esi,DWORD [edi-8]
	mov	DWORD [4+esp],eax
	mov	DWORD [8+esp],ebx
	mov	DWORD [12+esp],ecx
	mov	DWORD [16+esp],edx
align	16
L$006loop:
	xor	eax,esi
	xor	ebx,DWORD [edi-4]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-16]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [edi-12]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-24]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	xor	eax,esi
	xor	ebx,DWORD [edi-20]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-32]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [edi-28]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-40]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	xor	eax,esi
	xor	ebx,DWORD [edi-36]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [16+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [12+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-48]
	xor	edx,ecx
	mov	DWORD [16+esp],edx
	xor	ecx,ebx
	mov	DWORD [12+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [edi-44]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [8+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [4+esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [edi-56]
	xor	ebx,eax
	mov	DWORD [8+esp],ebx
	xor	eax,edx
	mov	DWORD [4+esp],eax
	sub	edi,64
	cmp	edi,DWORD [20+esp]
	je	NEAR L$007done
	and	esi,eax
	mov	edx,DWORD [16+esp]
	rol	esi,1
	mov	ecx,edx
	xor	ebx,esi
	or	ecx,DWORD [4+edi]
	mov	DWORD [8+esp],ebx
	xor	ecx,DWORD [12+esp]
	mov	esi,DWORD [12+edi]
	mov	DWORD [12+esp],ecx
	or	esi,ebx
	and	ecx,DWORD [edi]
	xor	eax,esi
	rol	ecx,1
	mov	DWORD [4+esp],eax
	xor	edx,ecx
	mov	esi,DWORD [edi-8]
	mov	DWORD [16+esp],edx
	jmp	NEAR L$006loop
align	8
L$007done:
	mov	ecx,eax
	mov	edx,ebx
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	xor	ecx,esi
	xor	edx,DWORD [12+edi]
	xor	eax,DWORD [edi]
	xor	ebx,DWORD [4+edi]
	ret
global	_Camellia_Ekeygen
align	16
_Camellia_Ekeygen:
L$_Camellia_Ekeygen_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	sub	esp,16
	mov	ebp,DWORD [36+esp]
	mov	esi,DWORD [40+esp]
	mov	edi,DWORD [44+esp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	mov	edx,DWORD [12+esi]
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	cmp	ebp,128
	je	NEAR L$0081st128
	mov	eax,DWORD [16+esi]
	mov	ebx,DWORD [20+esi]
	cmp	ebp,192
	je	NEAR L$0091st192
	mov	ecx,DWORD [24+esi]
	mov	edx,DWORD [28+esi]
	jmp	NEAR L$0101st256
align	4
L$0091st192:
	mov	ecx,eax
	mov	edx,ebx
	not	ecx
	not	edx
align	4
L$0101st256:
	bswap	eax
	bswap	ebx
	bswap	ecx
	bswap	edx
	mov	DWORD [32+edi],eax
	mov	DWORD [36+edi],ebx
	mov	DWORD [40+edi],ecx
	mov	DWORD [44+edi],edx
	xor	eax,DWORD [edi]
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
align	4
L$0081st128:
	call	L$011pic_point
L$011pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$011pic_point)+ebp]
	lea	edi,[(L$Camellia_SIGMA-L$Camellia_SBOX)+ebp]
	mov	esi,DWORD [edi]
	mov	DWORD [esp],eax
	mov	DWORD [4+esp],ebx
	mov	DWORD [8+esp],ecx
	mov	DWORD [12+esp],edx
	xor	eax,esi
	xor	ebx,DWORD [4+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [12+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [8+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [8+edi]
	xor	edx,ecx
	mov	DWORD [12+esp],edx
	xor	ecx,ebx
	mov	DWORD [8+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [12+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [4+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [16+edi]
	xor	ebx,eax
	mov	DWORD [4+esp],ebx
	xor	eax,edx
	mov	DWORD [esp],eax
	mov	ecx,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
	mov	esi,DWORD [44+esp]
	xor	eax,DWORD [esi]
	xor	ebx,DWORD [4+esi]
	xor	ecx,DWORD [8+esi]
	xor	edx,DWORD [12+esi]
	mov	esi,DWORD [16+edi]
	mov	DWORD [esp],eax
	mov	DWORD [4+esp],ebx
	mov	DWORD [8+esp],ecx
	mov	DWORD [12+esp],edx
	xor	eax,esi
	xor	ebx,DWORD [20+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [12+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [8+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [24+edi]
	xor	edx,ecx
	mov	DWORD [12+esp],edx
	xor	ecx,ebx
	mov	DWORD [8+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [28+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [4+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [32+edi]
	xor	ebx,eax
	mov	DWORD [4+esp],ebx
	xor	eax,edx
	mov	DWORD [esp],eax
	mov	ecx,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
	mov	esi,DWORD [36+esp]
	cmp	esi,128
	jne	NEAR L$0122nd256
	mov	edi,DWORD [44+esp]
	lea	edi,[128+edi]
	mov	DWORD [edi-112],eax
	mov	DWORD [edi-108],ebx
	mov	DWORD [edi-104],ecx
	mov	DWORD [edi-100],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD [edi-80],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD [edi-76],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [edi-72],ecx
	mov	DWORD [edi-68],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD [edi-64],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD [edi-60],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [edi-56],ecx
	mov	DWORD [edi-52],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD [edi-32],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD [edi-28],ebx
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
	mov	DWORD [edi-16],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD [edi-12],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [edi-8],ecx
	mov	DWORD [edi-4],edx
	mov	ebp,ebx
	shl	ebx,2
	mov	esi,ecx
	shr	esi,30
	shl	ecx,2
	or	ebx,esi
	mov	esi,edx
	shl	edx,2
	mov	DWORD [32+edi],ebx
	shr	esi,30
	or	ecx,esi
	shr	ebp,30
	mov	esi,eax
	shr	esi,30
	mov	DWORD [36+edi],ecx
	shl	eax,2
	or	edx,esi
	or	eax,ebp
	mov	DWORD [40+edi],edx
	mov	DWORD [44+edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD [64+edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD [68+edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD [72+edi],edx
	mov	DWORD [76+edi],eax
	mov	ebx,DWORD [edi-128]
	mov	ecx,DWORD [edi-124]
	mov	edx,DWORD [edi-120]
	mov	eax,DWORD [edi-116]
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD [edi-96],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD [edi-92],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD [edi-88],edx
	mov	DWORD [edi-84],eax
	mov	ebp,ebx
	shl	ebx,30
	mov	esi,ecx
	shr	esi,2
	shl	ecx,30
	or	ebx,esi
	mov	esi,edx
	shl	edx,30
	mov	DWORD [edi-48],ebx
	shr	esi,2
	or	ecx,esi
	shr	ebp,2
	mov	esi,eax
	shr	esi,2
	mov	DWORD [edi-44],ecx
	shl	eax,30
	or	edx,esi
	or	eax,ebp
	mov	DWORD [edi-40],edx
	mov	DWORD [edi-36],eax
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
	mov	DWORD [edi-24],edx
	mov	DWORD [edi-20],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD [edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD [4+edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD [8+edi],edx
	mov	DWORD [12+edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD [16+edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD [20+edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD [24+edi],edx
	mov	DWORD [28+edi],eax
	mov	ebp,ebx
	shl	ebx,17
	mov	esi,ecx
	shr	esi,15
	shl	ecx,17
	or	ebx,esi
	mov	esi,edx
	shl	edx,17
	mov	DWORD [48+edi],ebx
	shr	esi,15
	or	ecx,esi
	shr	ebp,15
	mov	esi,eax
	shr	esi,15
	mov	DWORD [52+edi],ecx
	shl	eax,17
	or	edx,esi
	or	eax,ebp
	mov	DWORD [56+edi],edx
	mov	DWORD [60+edi],eax
	mov	eax,3
	jmp	NEAR L$013done
align	16
L$0122nd256:
	mov	esi,DWORD [44+esp]
	mov	DWORD [48+esi],eax
	mov	DWORD [52+esi],ebx
	mov	DWORD [56+esi],ecx
	mov	DWORD [60+esi],edx
	xor	eax,DWORD [32+esi]
	xor	ebx,DWORD [36+esi]
	xor	ecx,DWORD [40+esi]
	xor	edx,DWORD [44+esi]
	mov	esi,DWORD [32+edi]
	mov	DWORD [esp],eax
	mov	DWORD [4+esp],ebx
	mov	DWORD [8+esp],ecx
	mov	DWORD [12+esp],edx
	xor	eax,esi
	xor	ebx,DWORD [36+edi]
	movzx	esi,ah
	mov	edx,DWORD [2052+esi*8+ebp]
	movzx	esi,al
	xor	edx,DWORD [4+esi*8+ebp]
	shr	eax,16
	movzx	esi,bl
	mov	ecx,DWORD [esi*8+ebp]
	movzx	esi,ah
	xor	edx,DWORD [esi*8+ebp]
	movzx	esi,bh
	xor	ecx,DWORD [4+esi*8+ebp]
	shr	ebx,16
	movzx	eax,al
	xor	edx,DWORD [2048+eax*8+ebp]
	movzx	esi,bh
	mov	eax,DWORD [12+esp]
	xor	ecx,edx
	ror	edx,8
	xor	ecx,DWORD [2048+esi*8+ebp]
	movzx	esi,bl
	mov	ebx,DWORD [8+esp]
	xor	edx,eax
	xor	ecx,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [40+edi]
	xor	edx,ecx
	mov	DWORD [12+esp],edx
	xor	ecx,ebx
	mov	DWORD [8+esp],ecx
	xor	ecx,esi
	xor	edx,DWORD [44+edi]
	movzx	esi,ch
	mov	ebx,DWORD [2052+esi*8+ebp]
	movzx	esi,cl
	xor	ebx,DWORD [4+esi*8+ebp]
	shr	ecx,16
	movzx	esi,dl
	mov	eax,DWORD [esi*8+ebp]
	movzx	esi,ch
	xor	ebx,DWORD [esi*8+ebp]
	movzx	esi,dh
	xor	eax,DWORD [4+esi*8+ebp]
	shr	edx,16
	movzx	ecx,cl
	xor	ebx,DWORD [2048+ecx*8+ebp]
	movzx	esi,dh
	mov	ecx,DWORD [4+esp]
	xor	eax,ebx
	ror	ebx,8
	xor	eax,DWORD [2048+esi*8+ebp]
	movzx	esi,dl
	mov	edx,DWORD [esp]
	xor	ebx,ecx
	xor	eax,DWORD [2052+esi*8+ebp]
	mov	esi,DWORD [48+edi]
	xor	ebx,eax
	mov	DWORD [4+esp],ebx
	xor	eax,edx
	mov	DWORD [esp],eax
	mov	ecx,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
	mov	edi,DWORD [44+esp]
	lea	edi,[128+edi]
	mov	DWORD [edi-112],eax
	mov	DWORD [edi-108],ebx
	mov	DWORD [edi-104],ecx
	mov	DWORD [edi-100],edx
	mov	ebp,eax
	shl	eax,30
	mov	esi,ebx
	shr	esi,2
	shl	ebx,30
	or	eax,esi
	mov	esi,ecx
	shl	ecx,30
	mov	DWORD [edi-48],eax
	shr	esi,2
	or	ebx,esi
	shr	ebp,2
	mov	esi,edx
	shr	esi,2
	mov	DWORD [edi-44],ebx
	shl	edx,30
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [edi-40],ecx
	mov	DWORD [edi-36],edx
	mov	ebp,eax
	shl	eax,30
	mov	esi,ebx
	shr	esi,2
	shl	ebx,30
	or	eax,esi
	mov	esi,ecx
	shl	ecx,30
	mov	DWORD [32+edi],eax
	shr	esi,2
	or	ebx,esi
	shr	ebp,2
	mov	esi,edx
	shr	esi,2
	mov	DWORD [36+edi],ebx
	shl	edx,30
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [40+edi],ecx
	mov	DWORD [44+edi],edx
	mov	ebp,ebx
	shl	ebx,19
	mov	esi,ecx
	shr	esi,13
	shl	ecx,19
	or	ebx,esi
	mov	esi,edx
	shl	edx,19
	mov	DWORD [128+edi],ebx
	shr	esi,13
	or	ecx,esi
	shr	ebp,13
	mov	esi,eax
	shr	esi,13
	mov	DWORD [132+edi],ecx
	shl	eax,19
	or	edx,esi
	or	eax,ebp
	mov	DWORD [136+edi],edx
	mov	DWORD [140+edi],eax
	mov	ebx,DWORD [edi-96]
	mov	ecx,DWORD [edi-92]
	mov	edx,DWORD [edi-88]
	mov	eax,DWORD [edi-84]
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD [edi-96],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD [edi-92],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD [edi-88],edx
	mov	DWORD [edi-84],eax
	mov	ebp,ebx
	shl	ebx,15
	mov	esi,ecx
	shr	esi,17
	shl	ecx,15
	or	ebx,esi
	mov	esi,edx
	shl	edx,15
	mov	DWORD [edi-64],ebx
	shr	esi,17
	or	ecx,esi
	shr	ebp,17
	mov	esi,eax
	shr	esi,17
	mov	DWORD [edi-60],ecx
	shl	eax,15
	or	edx,esi
	or	eax,ebp
	mov	DWORD [edi-56],edx
	mov	DWORD [edi-52],eax
	mov	ebp,ebx
	shl	ebx,30
	mov	esi,ecx
	shr	esi,2
	shl	ecx,30
	or	ebx,esi
	mov	esi,edx
	shl	edx,30
	mov	DWORD [16+edi],ebx
	shr	esi,2
	or	ecx,esi
	shr	ebp,2
	mov	esi,eax
	shr	esi,2
	mov	DWORD [20+edi],ecx
	shl	eax,30
	or	edx,esi
	or	eax,ebp
	mov	DWORD [24+edi],edx
	mov	DWORD [28+edi],eax
	mov	ebp,ecx
	shl	ecx,2
	mov	esi,edx
	shr	esi,30
	shl	edx,2
	or	ecx,esi
	mov	esi,eax
	shl	eax,2
	mov	DWORD [80+edi],ecx
	shr	esi,30
	or	edx,esi
	shr	ebp,30
	mov	esi,ebx
	shr	esi,30
	mov	DWORD [84+edi],edx
	shl	ebx,2
	or	eax,esi
	or	ebx,ebp
	mov	DWORD [88+edi],eax
	mov	DWORD [92+edi],ebx
	mov	ecx,DWORD [edi-80]
	mov	edx,DWORD [edi-76]
	mov	eax,DWORD [edi-72]
	mov	ebx,DWORD [edi-68]
	mov	ebp,ecx
	shl	ecx,15
	mov	esi,edx
	shr	esi,17
	shl	edx,15
	or	ecx,esi
	mov	esi,eax
	shl	eax,15
	mov	DWORD [edi-80],ecx
	shr	esi,17
	or	edx,esi
	shr	ebp,17
	mov	esi,ebx
	shr	esi,17
	mov	DWORD [edi-76],edx
	shl	ebx,15
	or	eax,esi
	or	ebx,ebp
	mov	DWORD [edi-72],eax
	mov	DWORD [edi-68],ebx
	mov	ebp,ecx
	shl	ecx,30
	mov	esi,edx
	shr	esi,2
	shl	edx,30
	or	ecx,esi
	mov	esi,eax
	shl	eax,30
	mov	DWORD [edi-16],ecx
	shr	esi,2
	or	edx,esi
	shr	ebp,2
	mov	esi,ebx
	shr	esi,2
	mov	DWORD [edi-12],edx
	shl	ebx,30
	or	eax,esi
	or	ebx,ebp
	mov	DWORD [edi-8],eax
	mov	DWORD [edi-4],ebx
	mov	DWORD [64+edi],edx
	mov	DWORD [68+edi],eax
	mov	DWORD [72+edi],ebx
	mov	DWORD [76+edi],ecx
	mov	ebp,edx
	shl	edx,17
	mov	esi,eax
	shr	esi,15
	shl	eax,17
	or	edx,esi
	mov	esi,ebx
	shl	ebx,17
	mov	DWORD [96+edi],edx
	shr	esi,15
	or	eax,esi
	shr	ebp,15
	mov	esi,ecx
	shr	esi,15
	mov	DWORD [100+edi],eax
	shl	ecx,17
	or	ebx,esi
	or	ecx,ebp
	mov	DWORD [104+edi],ebx
	mov	DWORD [108+edi],ecx
	mov	edx,DWORD [edi-128]
	mov	eax,DWORD [edi-124]
	mov	ebx,DWORD [edi-120]
	mov	ecx,DWORD [edi-116]
	mov	ebp,eax
	shl	eax,13
	mov	esi,ebx
	shr	esi,19
	shl	ebx,13
	or	eax,esi
	mov	esi,ecx
	shl	ecx,13
	mov	DWORD [edi-32],eax
	shr	esi,19
	or	ebx,esi
	shr	ebp,19
	mov	esi,edx
	shr	esi,19
	mov	DWORD [edi-28],ebx
	shl	edx,13
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [edi-24],ecx
	mov	DWORD [edi-20],edx
	mov	ebp,eax
	shl	eax,15
	mov	esi,ebx
	shr	esi,17
	shl	ebx,15
	or	eax,esi
	mov	esi,ecx
	shl	ecx,15
	mov	DWORD [edi],eax
	shr	esi,17
	or	ebx,esi
	shr	ebp,17
	mov	esi,edx
	shr	esi,17
	mov	DWORD [4+edi],ebx
	shl	edx,15
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	mov	ebp,eax
	shl	eax,17
	mov	esi,ebx
	shr	esi,15
	shl	ebx,17
	or	eax,esi
	mov	esi,ecx
	shl	ecx,17
	mov	DWORD [48+edi],eax
	shr	esi,15
	or	ebx,esi
	shr	ebp,15
	mov	esi,edx
	shr	esi,15
	mov	DWORD [52+edi],ebx
	shl	edx,17
	or	ecx,esi
	or	edx,ebp
	mov	DWORD [56+edi],ecx
	mov	DWORD [60+edi],edx
	mov	ebp,ebx
	shl	ebx,2
	mov	esi,ecx
	shr	esi,30
	shl	ecx,2
	or	ebx,esi
	mov	esi,edx
	shl	edx,2
	mov	DWORD [112+edi],ebx
	shr	esi,30
	or	ecx,esi
	shr	ebp,30
	mov	esi,eax
	shr	esi,30
	mov	DWORD [116+edi],ecx
	shl	eax,2
	or	edx,esi
	or	eax,ebp
	mov	DWORD [120+edi],edx
	mov	DWORD [124+edi],eax
	mov	eax,4
L$013done:
	lea	edx,[144+edi]
	add	esp,16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_Camellia_set_key
align	16
_Camellia_set_key:
L$_Camellia_set_key_begin:
	push	ebx
	mov	ecx,DWORD [8+esp]
	mov	ebx,DWORD [12+esp]
	mov	edx,DWORD [16+esp]
	mov	eax,-1
	test	ecx,ecx
	jz	NEAR L$014done
	test	edx,edx
	jz	NEAR L$014done
	mov	eax,-2
	cmp	ebx,256
	je	NEAR L$015arg_ok
	cmp	ebx,192
	je	NEAR L$015arg_ok
	cmp	ebx,128
	jne	NEAR L$014done
align	4
L$015arg_ok:
	push	edx
	push	ecx
	push	ebx
	call	L$_Camellia_Ekeygen_begin
	add	esp,12
	mov	DWORD [edx],eax
	xor	eax,eax
align	4
L$014done:
	pop	ebx
	ret
align	64
L$Camellia_SIGMA:
dd	2694735487,1003262091,3061508184,1286239154,3337565999,3914302142,1426019237,4057165596,283453434,3731369245,2958461122,3018244605,0,0,0,0
align	64
L$Camellia_SBOX:
dd	1886416896,1886388336
dd	2189591040,741081132
dd	741092352,3014852787
dd	3974949888,3233808576
dd	3014898432,3840147684
dd	656877312,1465319511
dd	3233857536,3941204202
dd	3857048832,2930639022
dd	3840205824,589496355
dd	2240120064,1802174571
dd	1465341696,1162149957
dd	892679424,2779054245
dd	3941263872,3991732461
dd	202116096,1330577487
dd	2930683392,488439837
dd	1094795520,2459041938
dd	589505280,2256928902
dd	4025478912,2947481775
dd	1802201856,2088501372
dd	2475922176,522125343
dd	1162167552,1044250686
dd	421075200,3705405660
dd	2779096320,1583218782
dd	555819264,185270283
dd	3991792896,2795896998
dd	235802112,960036921
dd	1330597632,3587506389
dd	1313754624,1566376029
dd	488447232,3654877401
dd	1701143808,1515847770
dd	2459079168,1364262993
dd	3183328512,1819017324
dd	2256963072,2341142667
dd	3099113472,2593783962
dd	2947526400,4227531003
dd	2408550144,2964324528
dd	2088532992,1953759348
dd	3958106880,724238379
dd	522133248,4042260720
dd	3469659648,2223243396
dd	1044266496,3755933919
dd	808464384,3419078859
dd	3705461760,875823156
dd	1600085760,1987444854
dd	1583242752,1835860077
dd	3318072576,2846425257
dd	185273088,3520135377
dd	437918208,67371012
dd	2795939328,336855060
dd	3789676800,976879674
dd	960051456,3739091166
dd	3402287616,286326801
dd	3587560704,842137650
dd	1195853568,2627469468
dd	1566399744,1397948499
dd	1027423488,4075946226
dd	3654932736,4278059262
dd	16843008,3486449871
dd	1515870720,3284336835
dd	3604403712,2054815866
dd	1364283648,606339108
dd	1448498688,3907518696
dd	1819044864,1616904288
dd	1296911616,1768489065
dd	2341178112,2863268010
dd	218959104,2694840480
dd	2593823232,2711683233
dd	1717986816,1650589794
dd	4227595008,1414791252
dd	3435973632,505282590
dd	2964369408,3772776672
dd	757935360,1684275300
dd	1953788928,269484048
dd	303174144,0
dd	724249344,2745368739
dd	538976256,1970602101
dd	4042321920,2324299914
dd	2981212416,3873833190
dd	2223277056,151584777
dd	2576980224,3722248413
dd	3755990784,2273771655
dd	1280068608,2206400643
dd	3419130624,3452764365
dd	3267543552,2425356432
dd	875836416,1936916595
dd	2122219008,4143317238
dd	1987474944,2644312221
dd	84215040,3216965823
dd	1835887872,1381105746
dd	3082270464,3638034648
dd	2846468352,3368550600
dd	825307392,3334865094
dd	3520188672,2172715137
dd	387389184,1869545583
dd	67372032,320012307
dd	3621246720,1667432547
dd	336860160,3924361449
dd	1482184704,2812739751
dd	976894464,2677997727
dd	1633771776,3166437564
dd	3739147776,690552873
dd	454761216,4193845497
dd	286331136,791609391
dd	471604224,3031695540
dd	842150400,2021130360
dd	252645120,101056518
dd	2627509248,3890675943
dd	370546176,1903231089
dd	1397969664,3570663636
dd	404232192,2880110763
dd	4076007936,2290614408
dd	572662272,2374828173
dd	4278124032,1920073842
dd	1145324544,3115909305
dd	3486502656,4177002744
dd	2998055424,2896953516
dd	3284386560,909508662
dd	3048584448,707395626
dd	2054846976,1010565180
dd	2442236160,4059103473
dd	606348288,1077936192
dd	134744064,3553820883
dd	3907577856,3149594811
dd	2829625344,1128464451
dd	1616928768,353697813
dd	4244438016,2913796269
dd	1768515840,2004287607
dd	1347440640,2155872384
dd	2863311360,2189557890
dd	3503345664,3974889708
dd	2694881280,656867367
dd	2105376000,3856990437
dd	2711724288,2240086149
dd	2307492096,892665909
dd	1650614784,202113036
dd	2543294208,1094778945
dd	1414812672,4025417967
dd	1532713728,2475884691
dd	505290240,421068825
dd	2509608192,555810849
dd	3772833792,235798542
dd	4294967040,1313734734
dd	1684300800,1701118053
dd	3537031680,3183280317
dd	269488128,3099066552
dd	3301229568,2408513679
dd	0,3958046955
dd	1212696576,3469607118
dd	2745410304,808452144
dd	4160222976,1600061535
dd	1970631936,3318022341
dd	3688618752,437911578
dd	2324335104,3789619425
dd	50529024,3402236106
dd	3873891840,1195835463
dd	3671775744,1027407933
dd	151587072,16842753
dd	1061109504,3604349142
dd	3722304768,1448476758
dd	2492765184,1296891981
dd	2273806080,218955789
dd	1549556736,1717960806
dd	2206434048,3435921612
dd	33686016,757923885
dd	3452816640,303169554
dd	1246382592,538968096
dd	2425393152,2981167281
dd	858993408,2576941209
dd	1936945920,1280049228
dd	1734829824,3267494082
dd	4143379968,2122186878
dd	4092850944,84213765
dd	2644352256,3082223799
dd	2139062016,825294897
dd	3217014528,387383319
dd	3806519808,3621191895
dd	1381126656,1482162264
dd	2610666240,1633747041
dd	3638089728,454754331
dd	640034304,471597084
dd	3368601600,252641295
dd	926365440,370540566
dd	3334915584,404226072
dd	993737472,572653602
dd	2172748032,1145307204
dd	2526451200,2998010034
dd	1869573888,3048538293
dd	1263225600,2442199185
dd	320017152,134742024
dd	3200171520,2829582504
dd	1667457792,4244373756
dd	774778368,1347420240
dd	3924420864,3503292624
dd	2038003968,2105344125
dd	2812782336,2307457161
dd	2358021120,2543255703
dd	2678038272,1532690523
dd	1852730880,2509570197
dd	3166485504,4294902015
dd	2391707136,3536978130
dd	690563328,3301179588
dd	4126536960,1212678216
dd	4193908992,4160159991
dd	3065427456,3688562907
dd	791621376,50528259
dd	4261281024,3671720154
dd	3031741440,1061093439
dd	1499027712,2492727444
dd	2021160960,1549533276
dd	2560137216,33685506
dd	101058048,1246363722
dd	1785358848,858980403
dd	3890734848,1734803559
dd	1179010560,4092788979
dd	1903259904,2139029631
dd	3132799488,3806462178
dd	3570717696,2610626715
dd	623191296,640024614
dd	2880154368,926351415
dd	1111638528,993722427
dd	2290649088,2526412950
dd	2728567296,1263206475
dd	2374864128,3200123070
dd	4210752000,774766638
dd	1920102912,2037973113
dd	117901056,2357985420
dd	3115956480,1852702830
dd	1431655680,2391670926
dd	4177065984,4126474485
dd	4008635904,3065381046
dd	2896997376,4261216509
dd	168430080,1499005017
dd	909522432,2560098456
dd	1229539584,1785331818
dd	707406336,1178992710
dd	1751672832,3132752058
dd	1010580480,623181861
dd	943208448,1111621698
dd	4059164928,2728525986
dd	2762253312,4210688250
dd	1077952512,117899271
dd	673720320,1431634005
dd	3553874688,4008575214
dd	2071689984,168427530
dd	3149642496,1229520969
dd	3385444608,1751646312
dd	1128481536,943194168
dd	3250700544,2762211492
dd	353703168,673710120
dd	3823362816,2071658619
dd	2913840384,3385393353
dd	4109693952,3250651329
dd	2004317952,3823304931
dd	3351758592,4109631732
dd	2155905024,3351707847
dd	2661195264,2661154974
dd	14737632,939538488
dd	328965,1090535745
dd	5789784,369104406
dd	14277081,1979741814
dd	6776679,3640711641
dd	5131854,2466288531
dd	8487297,1610637408
dd	13355979,4060148466
dd	13224393,1912631922
dd	723723,3254829762
dd	11447982,2868947883
dd	6974058,2583730842
dd	14013909,1962964341
dd	1579032,100664838
dd	6118749,1459640151
dd	8553090,2684395680
dd	4605510,2432733585
dd	14671839,4144035831
dd	14079702,3036722613
dd	2565927,3372272073
dd	9079434,2717950626
dd	3289650,2348846220
dd	4934475,3523269330
dd	4342338,2415956112
dd	14408667,4127258358
dd	1842204,117442311
dd	10395294,2801837991
dd	10263708,654321447
dd	3815994,2382401166
dd	13290186,2986390194
dd	2434341,1224755529
dd	8092539,3724599006
dd	855309,1124090691
dd	7434609,1543527516
dd	6250335,3607156695
dd	2039583,3338717127
dd	16316664,1040203326
dd	14145495,4110480885
dd	4079166,2399178639
dd	10329501,1728079719
dd	8158332,520101663
dd	6316128,402659352
dd	12171705,1845522030
dd	12500670,2936057775
dd	12369084,788541231
dd	9145227,3791708898
dd	1447446,2231403909
dd	3421236,218107149
dd	5066061,1392530259
dd	12829635,4026593520
dd	7500402,2617285788
dd	9803157,1694524773
dd	11250603,3925928682
dd	9342606,2734728099
dd	12237498,2919280302
dd	8026746,2650840734
dd	11776947,3959483628
dd	131586,2147516544
dd	11842740,754986285
dd	11382189,1795189611
dd	10658466,2818615464
dd	11316396,721431339
dd	14211288,905983542
dd	10132122,2785060518
dd	1513239,3305162181
dd	1710618,2248181382
dd	3487029,1291865421
dd	13421772,855651123
dd	16250871,4244700669
dd	10066329,1711302246
dd	6381921,1476417624
dd	5921370,2516620950
dd	15263976,973093434
dd	2368548,150997257
dd	5658198,2499843477
dd	4210752,268439568
dd	14803425,2013296760
dd	6513507,3623934168
dd	592137,1107313218
dd	3355443,3422604492
dd	12566463,4009816047
dd	10000536,637543974
dd	9934743,3842041317
dd	8750469,1627414881
dd	6842472,436214298
dd	16579836,1056980799
dd	15527148,989870907
dd	657930,2181071490
dd	14342874,3053500086
dd	7303023,3674266587
dd	5460819,3556824276
dd	6447714,2550175896
dd	10724259,3892373736
dd	3026478,2332068747
dd	526344,33554946
dd	11513775,3942706155
dd	2631720,167774730
dd	11579568,738208812
dd	7631988,486546717
dd	12763842,2952835248
dd	12434877,1862299503
dd	3552822,2365623693
dd	2236962,2281736328
dd	3684408,234884622
dd	6579300,419436825
dd	1973790,2264958855
dd	3750201,1308642894
dd	2894892,184552203
dd	10921638,2835392937
dd	3158064,201329676
dd	15066597,2030074233
dd	4473924,285217041
dd	16645629,2130739071
dd	8947848,570434082
dd	10461087,3875596263
dd	6645093,1493195097
dd	8882055,3774931425
dd	7039851,3657489114
dd	16053492,1023425853
dd	2302755,3355494600
dd	4737096,301994514
dd	1052688,67109892
dd	13750737,1946186868
dd	5329233,1409307732
dd	12632256,805318704
dd	16382457,2113961598
dd	13816530,3019945140
dd	10526880,671098920
dd	5592405,1426085205
dd	10592673,1744857192
dd	4276545,1342197840
dd	16448250,3187719870
dd	4408131,3489714384
dd	1250067,3288384708
dd	12895428,822096177
dd	3092271,3405827019
dd	11053224,704653866
dd	11974326,2902502829
dd	3947580,251662095
dd	2829099,3389049546
dd	12698049,1879076976
dd	16777215,4278255615
dd	13158600,838873650
dd	10855845,1761634665
dd	2105376,134219784
dd	9013641,1644192354
dd	0,0
dd	9474192,603989028
dd	4671303,3506491857
dd	15724527,4211145723
dd	15395562,3120609978
dd	12040119,3976261101
dd	1381653,1157645637
dd	394758,2164294017
dd	13487565,1929409395
dd	11908533,1828744557
dd	1184274,2214626436
dd	8289918,2667618207
dd	12303291,3993038574
dd	2697513,1241533002
dd	986895,3271607235
dd	12105912,771763758
dd	460551,3238052289
dd	263172,16777473
dd	10197915,3858818790
dd	9737364,620766501
dd	2171169,1207978056
dd	6710886,2566953369
dd	15132390,3103832505
dd	13553358,3003167667
dd	15592941,2063629179
dd	15198183,4177590777
dd	3881787,3456159438
dd	16711422,3204497343
dd	8355711,3741376479
dd	12961221,1895854449
dd	10790052,687876393
dd	3618615,3439381965
dd	11645361,1811967084
dd	5000268,318771987
dd	9539985,1677747300
dd	7237230,2600508315
dd	9276813,1660969827
dd	7763574,2634063261
dd	197379,3221274816
dd	2960685,1258310475
dd	14606046,3070277559
dd	9868950,2768283045
dd	2500134,2298513801
dd	8224125,1593859935
dd	13027014,2969612721
dd	6052956,385881879
dd	13882323,4093703412
dd	15921906,3154164924
dd	5197647,3540046803
dd	1644825,1174423110
dd	4144959,3472936911
dd	14474460,922761015
dd	7960953,1577082462
dd	1907997,1191200583
dd	5395026,2483066004
dd	15461355,4194368250
dd	15987699,4227923196
dd	7171437,1526750043
dd	6184542,2533398423
dd	16514043,4261478142
dd	6908265,1509972570
dd	11711154,2885725356
dd	15790320,1006648380
dd	3223857,1275087948
dd	789516,50332419
dd	13948116,889206069
dd	13619151,4076925939
dd	9211020,587211555
dd	14869218,3087055032
dd	7697781,1560304989
dd	11119017,1778412138
dd	4868682,2449511058
dd	5723991,3573601749
dd	8684676,553656609
dd	1118481,1140868164
dd	4539717,1358975313
dd	1776411,3321939654
dd	16119285,2097184125
dd	15000804,956315961
dd	921102,2197848963
dd	7566195,3691044060
dd	11184810,2852170410
dd	15856113,2080406652
dd	14540253,1996519287
dd	5855577,1442862678
dd	1315860,83887365
dd	7105644,452991771
dd	9605778,2751505572
dd	5526612,352326933
dd	13684944,872428596
dd	7895160,503324190
dd	7368816,469769244
dd	14935011,4160813304
dd	4802889,1375752786
dd	8421504,536879136
dd	5263440,335549460
dd	10987431,3909151209
dd	16185078,3170942397
dd	7829367,3707821533
dd	9671571,3825263844
dd	8816262,2701173153
dd	8618883,3758153952
dd	2763306,2315291274
dd	13092807,4043370993
dd	5987163,3590379222
dd	15329769,2046851706
dd	15658734,3137387451
dd	9408399,3808486371
dd	65793,1073758272
dd	4013373,1325420367
global	_Camellia_cbc_encrypt
align	16
_Camellia_cbc_encrypt:
L$_Camellia_cbc_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ecx,DWORD [28+esp]
	cmp	ecx,0
	je	NEAR L$016enc_out
	pushfd
	cld
	mov	eax,DWORD [24+esp]
	mov	ebx,DWORD [28+esp]
	mov	edx,DWORD [36+esp]
	mov	ebp,DWORD [40+esp]
	lea	esi,[esp-64]
	and	esi,-64
	lea	edi,[edx-127]
	sub	edi,esi
	neg	edi
	and	edi,960
	sub	esi,edi
	mov	edi,DWORD [44+esp]
	xchg	esp,esi
	add	esp,4
	mov	DWORD [20+esp],esi
	mov	DWORD [24+esp],eax
	mov	DWORD [28+esp],ebx
	mov	DWORD [32+esp],ecx
	mov	DWORD [36+esp],edx
	mov	DWORD [40+esp],ebp
	call	L$017pic_point
L$017pic_point:
	pop	ebp
	lea	ebp,[(L$Camellia_SBOX-L$017pic_point)+ebp]
	mov	esi,32
align	4
L$018prefetch_sbox:
	mov	eax,DWORD [ebp]
	mov	ebx,DWORD [32+ebp]
	mov	ecx,DWORD [64+ebp]
	mov	edx,DWORD [96+ebp]
	lea	ebp,[128+ebp]
	dec	esi
	jnz	NEAR L$018prefetch_sbox
	mov	eax,DWORD [36+esp]
	sub	ebp,4096
	mov	esi,DWORD [24+esp]
	mov	edx,DWORD [272+eax]
	cmp	edi,0
	je	NEAR L$019DECRYPT
	mov	ecx,DWORD [32+esp]
	mov	edi,DWORD [40+esp]
	shl	edx,6
	lea	edx,[edx*1+eax]
	mov	DWORD [16+esp],edx
	test	ecx,4294967280
	jz	NEAR L$020enc_tail
	mov	eax,DWORD [edi]
	mov	ebx,DWORD [4+edi]
align	4
L$021enc_loop:
	mov	ecx,DWORD [8+edi]
	mov	edx,DWORD [12+edi]
	xor	eax,DWORD [esi]
	xor	ebx,DWORD [4+esi]
	xor	ecx,DWORD [8+esi]
	bswap	eax
	xor	edx,DWORD [12+esi]
	bswap	ebx
	mov	edi,DWORD [36+esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_encrypt
	mov	esi,DWORD [24+esp]
	mov	edi,DWORD [28+esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	mov	DWORD [edi],eax
	bswap	edx
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	mov	ecx,DWORD [32+esp]
	lea	esi,[16+esi]
	mov	DWORD [24+esp],esi
	lea	edx,[16+edi]
	mov	DWORD [28+esp],edx
	sub	ecx,16
	test	ecx,4294967280
	mov	DWORD [32+esp],ecx
	jnz	NEAR L$021enc_loop
	test	ecx,15
	jnz	NEAR L$020enc_tail
	mov	esi,DWORD [40+esp]
	mov	ecx,DWORD [8+edi]
	mov	edx,DWORD [12+edi]
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	mov	esp,DWORD [20+esp]
	popfd
L$016enc_out:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
	pushfd
align	4
L$020enc_tail:
	mov	eax,edi
	mov	edi,DWORD [28+esp]
	push	eax
	mov	ebx,16
	sub	ebx,ecx
	cmp	edi,esi
	je	NEAR L$022enc_in_place
align	4
dd	2767451785
	jmp	NEAR L$023enc_skip_in_place
L$022enc_in_place:
	lea	edi,[ecx*1+edi]
L$023enc_skip_in_place:
	mov	ecx,ebx
	xor	eax,eax
align	4
dd	2868115081
	pop	edi
	mov	esi,DWORD [28+esp]
	mov	eax,DWORD [edi]
	mov	ebx,DWORD [4+edi]
	mov	DWORD [32+esp],16
	jmp	NEAR L$021enc_loop
align	16
L$019DECRYPT:
	shl	edx,6
	lea	edx,[edx*1+eax]
	mov	DWORD [16+esp],eax
	mov	DWORD [36+esp],edx
	cmp	esi,DWORD [28+esp]
	je	NEAR L$024dec_in_place
	mov	edi,DWORD [40+esp]
	mov	DWORD [44+esp],edi
align	4
L$025dec_loop:
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	bswap	eax
	mov	edx,DWORD [12+esi]
	bswap	ebx
	mov	edi,DWORD [36+esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	edi,DWORD [44+esp]
	mov	esi,DWORD [32+esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	xor	eax,DWORD [edi]
	bswap	edx
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
	sub	esi,16
	jc	NEAR L$026dec_partial
	mov	DWORD [32+esp],esi
	mov	esi,DWORD [24+esp]
	mov	edi,DWORD [28+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	mov	DWORD [44+esp],esi
	lea	esi,[16+esi]
	mov	DWORD [24+esp],esi
	lea	edi,[16+edi]
	mov	DWORD [28+esp],edi
	jnz	NEAR L$025dec_loop
	mov	edi,DWORD [44+esp]
L$027dec_end:
	mov	esi,DWORD [40+esp]
	mov	eax,DWORD [edi]
	mov	ebx,DWORD [4+edi]
	mov	ecx,DWORD [8+edi]
	mov	edx,DWORD [12+edi]
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	jmp	NEAR L$028dec_out
align	4
L$026dec_partial:
	lea	edi,[44+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	lea	ecx,[16+esi]
	mov	esi,edi
	mov	edi,DWORD [28+esp]
dd	2767451785
	mov	edi,DWORD [24+esp]
	jmp	NEAR L$027dec_end
align	4
L$024dec_in_place:
L$029dec_in_place_loop:
	lea	edi,[44+esp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	mov	edx,DWORD [12+esi]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	bswap	eax
	mov	DWORD [12+edi],edx
	bswap	ebx
	mov	edi,DWORD [36+esp]
	bswap	ecx
	bswap	edx
	call	__x86_Camellia_decrypt
	mov	edi,DWORD [40+esp]
	mov	esi,DWORD [28+esp]
	bswap	eax
	bswap	ebx
	bswap	ecx
	xor	eax,DWORD [edi]
	bswap	edx
	xor	ebx,DWORD [4+edi]
	xor	ecx,DWORD [8+edi]
	xor	edx,DWORD [12+edi]
	mov	DWORD [esi],eax
	mov	DWORD [4+esi],ebx
	mov	DWORD [8+esi],ecx
	mov	DWORD [12+esi],edx
	lea	esi,[16+esi]
	mov	DWORD [28+esp],esi
	lea	esi,[44+esp]
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	ecx,DWORD [8+esi]
	mov	edx,DWORD [12+esi]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	mov	DWORD [8+edi],ecx
	mov	DWORD [12+edi],edx
	mov	esi,DWORD [24+esp]
	lea	esi,[16+esi]
	mov	DWORD [24+esp],esi
	mov	ecx,DWORD [32+esp]
	sub	ecx,16
	jc	NEAR L$030dec_in_place_partial
	mov	DWORD [32+esp],ecx
	jnz	NEAR L$029dec_in_place_loop
	jmp	NEAR L$028dec_out
align	4
L$030dec_in_place_partial:
	mov	edi,DWORD [28+esp]
	lea	esi,[44+esp]
	lea	edi,[ecx*1+edi]
	lea	esi,[16+ecx*1+esi]
	neg	ecx
dd	2767451785
align	4
L$028dec_out:
	mov	esp,DWORD [20+esp]
	popfd
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
db	67,97,109,101,108,108,105,97,32,102,111,114,32,120,56,54
db	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
db	115,108,46,111,114,103,62,0
