%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
;extern	_OPENSSL_ia32cap_P
global	_aesni_encrypt
align	16
_aesni_encrypt:
L$_aesni_encrypt_begin:
	mov	eax,DWORD [4+esp]
	mov	edx,DWORD [12+esp]
	movups	xmm2,[eax]
	mov	ecx,DWORD [240+edx]
	mov	eax,DWORD [8+esp]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$000enc1_loop_1:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$000enc1_loop_1
db	102,15,56,221,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	[eax],xmm2
	pxor	xmm2,xmm2
	ret
global	_aesni_decrypt
align	16
_aesni_decrypt:
L$_aesni_decrypt_begin:
	mov	eax,DWORD [4+esp]
	mov	edx,DWORD [12+esp]
	movups	xmm2,[eax]
	mov	ecx,DWORD [240+edx]
	mov	eax,DWORD [8+esp]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$001dec1_loop_2:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$001dec1_loop_2
db	102,15,56,223,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	[eax],xmm2
	pxor	xmm2,xmm2
	ret
align	16
__aesni_encrypt2:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
	add	ecx,16
L$002enc2_loop:
db	102,15,56,220,209
db	102,15,56,220,217
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$002enc2_loop
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,221,208
db	102,15,56,221,216
	ret
align	16
__aesni_decrypt2:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
	add	ecx,16
L$003dec2_loop:
db	102,15,56,222,209
db	102,15,56,222,217
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,222,208
db	102,15,56,222,216
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$003dec2_loop
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,223,208
db	102,15,56,223,216
	ret
align	16
__aesni_encrypt3:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
	add	ecx,16
L$004enc3_loop:
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
db	102,15,56,220,224
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$004enc3_loop
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,221,208
db	102,15,56,221,216
db	102,15,56,221,224
	ret
align	16
__aesni_decrypt3:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
	add	ecx,16
L$005dec3_loop:
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,222,208
db	102,15,56,222,216
db	102,15,56,222,224
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$005dec3_loop
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,223,208
db	102,15,56,223,216
db	102,15,56,223,224
	ret
align	16
__aesni_encrypt4:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	shl	ecx,4
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
db	15,31,64,0
	add	ecx,16
L$006enc4_loop:
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,220,233
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
db	102,15,56,220,224
db	102,15,56,220,232
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$006enc4_loop
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,221,208
db	102,15,56,221,216
db	102,15,56,221,224
db	102,15,56,221,232
	ret
align	16
__aesni_decrypt4:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	shl	ecx,4
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	movups	xmm0,[32+edx]
	lea	edx,[32+ecx*1+edx]
	neg	ecx
db	15,31,64,0
	add	ecx,16
L$007dec4_loop:
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,222,233
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,222,208
db	102,15,56,222,216
db	102,15,56,222,224
db	102,15,56,222,232
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$007dec4_loop
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,222,233
db	102,15,56,223,208
db	102,15,56,223,216
db	102,15,56,223,224
db	102,15,56,223,232
	ret
align	16
__aesni_encrypt6:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
db	102,15,56,220,209
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
db	102,15,56,220,217
	lea	edx,[32+ecx*1+edx]
	neg	ecx
db	102,15,56,220,225
	pxor	xmm7,xmm0
	movups	xmm0,[ecx*1+edx]
	add	ecx,16
	jmp	NEAR L$008_aesni_encrypt6_inner
align	16
L$009enc6_loop:
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
L$008_aesni_encrypt6_inner:
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
L$_aesni_encrypt6_enter:
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
db	102,15,56,220,224
db	102,15,56,220,232
db	102,15,56,220,240
db	102,15,56,220,248
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$009enc6_loop
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
db	102,15,56,221,208
db	102,15,56,221,216
db	102,15,56,221,224
db	102,15,56,221,232
db	102,15,56,221,240
db	102,15,56,221,248
	ret
align	16
__aesni_decrypt6:
	movups	xmm0,[edx]
	shl	ecx,4
	movups	xmm1,[16+edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
db	102,15,56,222,209
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
db	102,15,56,222,217
	lea	edx,[32+ecx*1+edx]
	neg	ecx
db	102,15,56,222,225
	pxor	xmm7,xmm0
	movups	xmm0,[ecx*1+edx]
	add	ecx,16
	jmp	NEAR L$010_aesni_decrypt6_inner
align	16
L$011dec6_loop:
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
L$010_aesni_decrypt6_inner:
db	102,15,56,222,233
db	102,15,56,222,241
db	102,15,56,222,249
L$_aesni_decrypt6_enter:
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,222,208
db	102,15,56,222,216
db	102,15,56,222,224
db	102,15,56,222,232
db	102,15,56,222,240
db	102,15,56,222,248
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$011dec6_loop
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,222,233
db	102,15,56,222,241
db	102,15,56,222,249
db	102,15,56,223,208
db	102,15,56,223,216
db	102,15,56,223,224
db	102,15,56,223,232
db	102,15,56,223,240
db	102,15,56,223,248
	ret
global	_aesni_ecb_encrypt
align	16
_aesni_ecb_encrypt:
L$_aesni_ecb_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebx,DWORD [36+esp]
	and	eax,-16
	jz	NEAR L$012ecb_ret
	mov	ecx,DWORD [240+edx]
	test	ebx,ebx
	jz	NEAR L$013ecb_decrypt
	mov	ebp,edx
	mov	ebx,ecx
	cmp	eax,96
	jb	NEAR L$014ecb_enc_tail
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
	sub	eax,96
	jmp	NEAR L$015ecb_enc_loop6_enter
align	16
L$016ecb_enc_loop6:
	movups	[edi],xmm2
	movdqu	xmm2,[esi]
	movups	[16+edi],xmm3
	movdqu	xmm3,[16+esi]
	movups	[32+edi],xmm4
	movdqu	xmm4,[32+esi]
	movups	[48+edi],xmm5
	movdqu	xmm5,[48+esi]
	movups	[64+edi],xmm6
	movdqu	xmm6,[64+esi]
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
L$015ecb_enc_loop6_enter:
	call	__aesni_encrypt6
	mov	edx,ebp
	mov	ecx,ebx
	sub	eax,96
	jnc	NEAR L$016ecb_enc_loop6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	add	eax,96
	jz	NEAR L$012ecb_ret
L$014ecb_enc_tail:
	movups	xmm2,[esi]
	cmp	eax,32
	jb	NEAR L$017ecb_enc_one
	movups	xmm3,[16+esi]
	je	NEAR L$018ecb_enc_two
	movups	xmm4,[32+esi]
	cmp	eax,64
	jb	NEAR L$019ecb_enc_three
	movups	xmm5,[48+esi]
	je	NEAR L$020ecb_enc_four
	movups	xmm6,[64+esi]
	xorps	xmm7,xmm7
	call	__aesni_encrypt6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	jmp	NEAR L$012ecb_ret
align	16
L$017ecb_enc_one:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$021enc1_loop_3:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$021enc1_loop_3
db	102,15,56,221,209
	movups	[edi],xmm2
	jmp	NEAR L$012ecb_ret
align	16
L$018ecb_enc_two:
	call	__aesni_encrypt2
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	jmp	NEAR L$012ecb_ret
align	16
L$019ecb_enc_three:
	call	__aesni_encrypt3
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	jmp	NEAR L$012ecb_ret
align	16
L$020ecb_enc_four:
	call	__aesni_encrypt4
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	jmp	NEAR L$012ecb_ret
align	16
L$013ecb_decrypt:
	mov	ebp,edx
	mov	ebx,ecx
	cmp	eax,96
	jb	NEAR L$022ecb_dec_tail
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
	sub	eax,96
	jmp	NEAR L$023ecb_dec_loop6_enter
align	16
L$024ecb_dec_loop6:
	movups	[edi],xmm2
	movdqu	xmm2,[esi]
	movups	[16+edi],xmm3
	movdqu	xmm3,[16+esi]
	movups	[32+edi],xmm4
	movdqu	xmm4,[32+esi]
	movups	[48+edi],xmm5
	movdqu	xmm5,[48+esi]
	movups	[64+edi],xmm6
	movdqu	xmm6,[64+esi]
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
L$023ecb_dec_loop6_enter:
	call	__aesni_decrypt6
	mov	edx,ebp
	mov	ecx,ebx
	sub	eax,96
	jnc	NEAR L$024ecb_dec_loop6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	add	eax,96
	jz	NEAR L$012ecb_ret
L$022ecb_dec_tail:
	movups	xmm2,[esi]
	cmp	eax,32
	jb	NEAR L$025ecb_dec_one
	movups	xmm3,[16+esi]
	je	NEAR L$026ecb_dec_two
	movups	xmm4,[32+esi]
	cmp	eax,64
	jb	NEAR L$027ecb_dec_three
	movups	xmm5,[48+esi]
	je	NEAR L$028ecb_dec_four
	movups	xmm6,[64+esi]
	xorps	xmm7,xmm7
	call	__aesni_decrypt6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	jmp	NEAR L$012ecb_ret
align	16
L$025ecb_dec_one:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$029dec1_loop_4:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$029dec1_loop_4
db	102,15,56,223,209
	movups	[edi],xmm2
	jmp	NEAR L$012ecb_ret
align	16
L$026ecb_dec_two:
	call	__aesni_decrypt2
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	jmp	NEAR L$012ecb_ret
align	16
L$027ecb_dec_three:
	call	__aesni_decrypt3
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	jmp	NEAR L$012ecb_ret
align	16
L$028ecb_dec_four:
	call	__aesni_decrypt4
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
L$012ecb_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_ccm64_encrypt_blocks
align	16
_aesni_ccm64_encrypt_blocks:
L$_aesni_ccm64_encrypt_blocks_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebx,DWORD [36+esp]
	mov	ecx,DWORD [40+esp]
	mov	ebp,esp
	sub	esp,60
	and	esp,-16
	mov	DWORD [48+esp],ebp
	movdqu	xmm7,[ebx]
	movdqu	xmm3,[ecx]
	mov	ecx,DWORD [240+edx]
	mov	DWORD [esp],202182159
	mov	DWORD [4+esp],134810123
	mov	DWORD [8+esp],67438087
	mov	DWORD [12+esp],66051
	mov	ebx,1
	xor	ebp,ebp
	mov	DWORD [16+esp],ebx
	mov	DWORD [20+esp],ebp
	mov	DWORD [24+esp],ebp
	mov	DWORD [28+esp],ebp
	shl	ecx,4
	mov	ebx,16
	lea	ebp,[edx]
	movdqa	xmm5,[esp]
	movdqa	xmm2,xmm7
	lea	edx,[32+ecx*1+edx]
	sub	ebx,ecx
db	102,15,56,0,253
L$030ccm64_enc_outer:
	movups	xmm0,[ebp]
	mov	ecx,ebx
	movups	xmm6,[esi]
	xorps	xmm2,xmm0
	movups	xmm1,[16+ebp]
	xorps	xmm0,xmm6
	xorps	xmm3,xmm0
	movups	xmm0,[32+ebp]
L$031ccm64_enc2_loop:
db	102,15,56,220,209
db	102,15,56,220,217
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$031ccm64_enc2_loop
db	102,15,56,220,209
db	102,15,56,220,217
	paddq	xmm7,[16+esp]
	dec	eax
db	102,15,56,221,208
db	102,15,56,221,216
	lea	esi,[16+esi]
	xorps	xmm6,xmm2
	movdqa	xmm2,xmm7
	movups	[edi],xmm6
db	102,15,56,0,213
	lea	edi,[16+edi]
	jnz	NEAR L$030ccm64_enc_outer
	mov	esp,DWORD [48+esp]
	mov	edi,DWORD [40+esp]
	movups	[edi],xmm3
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_ccm64_decrypt_blocks
align	16
_aesni_ccm64_decrypt_blocks:
L$_aesni_ccm64_decrypt_blocks_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebx,DWORD [36+esp]
	mov	ecx,DWORD [40+esp]
	mov	ebp,esp
	sub	esp,60
	and	esp,-16
	mov	DWORD [48+esp],ebp
	movdqu	xmm7,[ebx]
	movdqu	xmm3,[ecx]
	mov	ecx,DWORD [240+edx]
	mov	DWORD [esp],202182159
	mov	DWORD [4+esp],134810123
	mov	DWORD [8+esp],67438087
	mov	DWORD [12+esp],66051
	mov	ebx,1
	xor	ebp,ebp
	mov	DWORD [16+esp],ebx
	mov	DWORD [20+esp],ebp
	mov	DWORD [24+esp],ebp
	mov	DWORD [28+esp],ebp
	movdqa	xmm5,[esp]
	movdqa	xmm2,xmm7
	mov	ebp,edx
	mov	ebx,ecx
db	102,15,56,0,253
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$032enc1_loop_5:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$032enc1_loop_5
db	102,15,56,221,209
	shl	ebx,4
	mov	ecx,16
	movups	xmm6,[esi]
	paddq	xmm7,[16+esp]
	lea	esi,[16+esi]
	sub	ecx,ebx
	lea	edx,[32+ebx*1+ebp]
	mov	ebx,ecx
	jmp	NEAR L$033ccm64_dec_outer
align	16
L$033ccm64_dec_outer:
	xorps	xmm6,xmm2
	movdqa	xmm2,xmm7
	movups	[edi],xmm6
	lea	edi,[16+edi]
db	102,15,56,0,213
	sub	eax,1
	jz	NEAR L$034ccm64_dec_break
	movups	xmm0,[ebp]
	mov	ecx,ebx
	movups	xmm1,[16+ebp]
	xorps	xmm6,xmm0
	xorps	xmm2,xmm0
	xorps	xmm3,xmm6
	movups	xmm0,[32+ebp]
L$035ccm64_dec2_loop:
db	102,15,56,220,209
db	102,15,56,220,217
	movups	xmm1,[ecx*1+edx]
	add	ecx,32
db	102,15,56,220,208
db	102,15,56,220,216
	movups	xmm0,[ecx*1+edx-16]
	jnz	NEAR L$035ccm64_dec2_loop
	movups	xmm6,[esi]
	paddq	xmm7,[16+esp]
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,221,208
db	102,15,56,221,216
	lea	esi,[16+esi]
	jmp	NEAR L$033ccm64_dec_outer
align	16
L$034ccm64_dec_break:
	mov	ecx,DWORD [240+ebp]
	mov	edx,ebp
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	xorps	xmm6,xmm0
	lea	edx,[32+edx]
	xorps	xmm3,xmm6
L$036enc1_loop_6:
db	102,15,56,220,217
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$036enc1_loop_6
db	102,15,56,221,217
	mov	esp,DWORD [48+esp]
	mov	edi,DWORD [40+esp]
	movups	[edi],xmm3
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_ctr32_encrypt_blocks
align	16
_aesni_ctr32_encrypt_blocks:
L$_aesni_ctr32_encrypt_blocks_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebx,DWORD [36+esp]
	mov	ebp,esp
	sub	esp,88
	and	esp,-16
	mov	DWORD [80+esp],ebp
	cmp	eax,1
	je	NEAR L$037ctr32_one_shortcut
	movdqu	xmm7,[ebx]
	mov	DWORD [esp],202182159
	mov	DWORD [4+esp],134810123
	mov	DWORD [8+esp],67438087
	mov	DWORD [12+esp],66051
	mov	ecx,6
	xor	ebp,ebp
	mov	DWORD [16+esp],ecx
	mov	DWORD [20+esp],ecx
	mov	DWORD [24+esp],ecx
	mov	DWORD [28+esp],ebp
db	102,15,58,22,251,3
db	102,15,58,34,253,3
	mov	ecx,DWORD [240+edx]
	bswap	ebx
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movdqa	xmm2,[esp]
db	102,15,58,34,195,0
	lea	ebp,[3+ebx]
db	102,15,58,34,205,0
	inc	ebx
db	102,15,58,34,195,1
	inc	ebp
db	102,15,58,34,205,1
	inc	ebx
db	102,15,58,34,195,2
	inc	ebp
db	102,15,58,34,205,2
	movdqa	[48+esp],xmm0
db	102,15,56,0,194
	movdqu	xmm6,[edx]
	movdqa	[64+esp],xmm1
db	102,15,56,0,202
	pshufd	xmm2,xmm0,192
	pshufd	xmm3,xmm0,128
	cmp	eax,6
	jb	NEAR L$038ctr32_tail
	pxor	xmm7,xmm6
	shl	ecx,4
	mov	ebx,16
	movdqa	[32+esp],xmm7
	mov	ebp,edx
	sub	ebx,ecx
	lea	edx,[32+ecx*1+edx]
	sub	eax,6
	jmp	NEAR L$039ctr32_loop6
align	16
L$039ctr32_loop6:
	pshufd	xmm4,xmm0,64
	movdqa	xmm0,[32+esp]
	pshufd	xmm5,xmm1,192
	pxor	xmm2,xmm0
	pshufd	xmm6,xmm1,128
	pxor	xmm3,xmm0
	pshufd	xmm7,xmm1,64
	movups	xmm1,[16+ebp]
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
db	102,15,56,220,209
	pxor	xmm6,xmm0
	pxor	xmm7,xmm0
db	102,15,56,220,217
	movups	xmm0,[32+ebp]
	mov	ecx,ebx
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
	call	L$_aesni_encrypt6_enter
	movups	xmm1,[esi]
	movups	xmm0,[16+esi]
	xorps	xmm2,xmm1
	movups	xmm1,[32+esi]
	xorps	xmm3,xmm0
	movups	[edi],xmm2
	movdqa	xmm0,[16+esp]
	xorps	xmm4,xmm1
	movdqa	xmm1,[64+esp]
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	paddd	xmm1,xmm0
	paddd	xmm0,[48+esp]
	movdqa	xmm2,[esp]
	movups	xmm3,[48+esi]
	movups	xmm4,[64+esi]
	xorps	xmm5,xmm3
	movups	xmm3,[80+esi]
	lea	esi,[96+esi]
	movdqa	[48+esp],xmm0
db	102,15,56,0,194
	xorps	xmm6,xmm4
	movups	[48+edi],xmm5
	xorps	xmm7,xmm3
	movdqa	[64+esp],xmm1
db	102,15,56,0,202
	movups	[64+edi],xmm6
	pshufd	xmm2,xmm0,192
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	pshufd	xmm3,xmm0,128
	sub	eax,6
	jnc	NEAR L$039ctr32_loop6
	add	eax,6
	jz	NEAR L$040ctr32_ret
	movdqu	xmm7,[ebp]
	mov	edx,ebp
	pxor	xmm7,[32+esp]
	mov	ecx,DWORD [240+ebp]
L$038ctr32_tail:
	por	xmm2,xmm7
	cmp	eax,2
	jb	NEAR L$041ctr32_one
	pshufd	xmm4,xmm0,64
	por	xmm3,xmm7
	je	NEAR L$042ctr32_two
	pshufd	xmm5,xmm1,192
	por	xmm4,xmm7
	cmp	eax,4
	jb	NEAR L$043ctr32_three
	pshufd	xmm6,xmm1,128
	por	xmm5,xmm7
	je	NEAR L$044ctr32_four
	por	xmm6,xmm7
	call	__aesni_encrypt6
	movups	xmm1,[esi]
	movups	xmm0,[16+esi]
	xorps	xmm2,xmm1
	movups	xmm1,[32+esi]
	xorps	xmm3,xmm0
	movups	xmm0,[48+esi]
	xorps	xmm4,xmm1
	movups	xmm1,[64+esi]
	xorps	xmm5,xmm0
	movups	[edi],xmm2
	xorps	xmm6,xmm1
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	jmp	NEAR L$040ctr32_ret
align	16
L$037ctr32_one_shortcut:
	movups	xmm2,[ebx]
	mov	ecx,DWORD [240+edx]
L$041ctr32_one:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$045enc1_loop_7:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$045enc1_loop_7
db	102,15,56,221,209
	movups	xmm6,[esi]
	xorps	xmm6,xmm2
	movups	[edi],xmm6
	jmp	NEAR L$040ctr32_ret
align	16
L$042ctr32_two:
	call	__aesni_encrypt2
	movups	xmm5,[esi]
	movups	xmm6,[16+esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	jmp	NEAR L$040ctr32_ret
align	16
L$043ctr32_three:
	call	__aesni_encrypt3
	movups	xmm5,[esi]
	movups	xmm6,[16+esi]
	xorps	xmm2,xmm5
	movups	xmm7,[32+esi]
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	xorps	xmm4,xmm7
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	jmp	NEAR L$040ctr32_ret
align	16
L$044ctr32_four:
	call	__aesni_encrypt4
	movups	xmm6,[esi]
	movups	xmm7,[16+esi]
	movups	xmm1,[32+esi]
	xorps	xmm2,xmm6
	movups	xmm0,[48+esi]
	xorps	xmm3,xmm7
	movups	[edi],xmm2
	xorps	xmm4,xmm1
	movups	[16+edi],xmm3
	xorps	xmm5,xmm0
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
L$040ctr32_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	movdqa	[32+esp],xmm0
	pxor	xmm5,xmm5
	movdqa	[48+esp],xmm0
	pxor	xmm6,xmm6
	movdqa	[64+esp],xmm0
	pxor	xmm7,xmm7
	mov	esp,DWORD [80+esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_xts_encrypt
align	16
_aesni_xts_encrypt:
L$_aesni_xts_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edx,DWORD [36+esp]
	mov	esi,DWORD [40+esp]
	mov	ecx,DWORD [240+edx]
	movups	xmm2,[esi]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$046enc1_loop_8:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$046enc1_loop_8
db	102,15,56,221,209
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebp,esp
	sub	esp,120
	mov	ecx,DWORD [240+edx]
	and	esp,-16
	mov	DWORD [96+esp],135
	mov	DWORD [100+esp],0
	mov	DWORD [104+esp],1
	mov	DWORD [108+esp],0
	mov	DWORD [112+esp],eax
	mov	DWORD [116+esp],ebp
	movdqa	xmm1,xmm2
	pxor	xmm0,xmm0
	movdqa	xmm3,[96+esp]
	pcmpgtd	xmm0,xmm1
	and	eax,-16
	mov	ebp,edx
	mov	ebx,ecx
	sub	eax,96
	jc	NEAR L$047xts_enc_short
	shl	ecx,4
	mov	ebx,16
	sub	ebx,ecx
	lea	edx,[32+ecx*1+edx]
	jmp	NEAR L$048xts_enc_loop6
align	16
L$048xts_enc_loop6:
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[16+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[32+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[48+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm7,xmm0,19
	movdqa	[64+esp],xmm1
	paddq	xmm1,xmm1
	movups	xmm0,[ebp]
	pand	xmm7,xmm3
	movups	xmm2,[esi]
	pxor	xmm7,xmm1
	mov	ecx,ebx
	movdqu	xmm3,[16+esi]
	xorps	xmm2,xmm0
	movdqu	xmm4,[32+esi]
	pxor	xmm3,xmm0
	movdqu	xmm5,[48+esi]
	pxor	xmm4,xmm0
	movdqu	xmm6,[64+esi]
	pxor	xmm5,xmm0
	movdqu	xmm1,[80+esi]
	pxor	xmm6,xmm0
	lea	esi,[96+esi]
	pxor	xmm2,[esp]
	movdqa	[80+esp],xmm7
	pxor	xmm7,xmm1
	movups	xmm1,[16+ebp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
db	102,15,56,220,209
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
db	102,15,56,220,217
	pxor	xmm7,xmm0
	movups	xmm0,[32+ebp]
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
	call	L$_aesni_encrypt6_enter
	movdqa	xmm1,[80+esp]
	pxor	xmm0,xmm0
	xorps	xmm2,[esp]
	pcmpgtd	xmm0,xmm1
	xorps	xmm3,[16+esp]
	movups	[edi],xmm2
	xorps	xmm4,[32+esp]
	movups	[16+edi],xmm3
	xorps	xmm5,[48+esp]
	movups	[32+edi],xmm4
	xorps	xmm6,[64+esp]
	movups	[48+edi],xmm5
	xorps	xmm7,xmm1
	movups	[64+edi],xmm6
	pshufd	xmm2,xmm0,19
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	movdqa	xmm3,[96+esp]
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	sub	eax,96
	jnc	NEAR L$048xts_enc_loop6
	mov	ecx,DWORD [240+ebp]
	mov	edx,ebp
	mov	ebx,ecx
L$047xts_enc_short:
	add	eax,96
	jz	NEAR L$049xts_enc_done6x
	movdqa	xmm5,xmm1
	cmp	eax,32
	jb	NEAR L$050xts_enc_one
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	je	NEAR L$051xts_enc_two
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	cmp	eax,64
	jb	NEAR L$052xts_enc_three
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm7,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	movdqa	[esp],xmm5
	movdqa	[16+esp],xmm6
	je	NEAR L$053xts_enc_four
	movdqa	[32+esp],xmm7
	pshufd	xmm7,xmm0,19
	movdqa	[48+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm7,xmm3
	pxor	xmm7,xmm1
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	pxor	xmm2,[esp]
	movdqu	xmm5,[48+esi]
	pxor	xmm3,[16+esp]
	movdqu	xmm6,[64+esi]
	pxor	xmm4,[32+esp]
	lea	esi,[80+esi]
	pxor	xmm5,[48+esp]
	movdqa	[64+esp],xmm7
	pxor	xmm6,xmm7
	call	__aesni_encrypt6
	movaps	xmm1,[64+esp]
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,[32+esp]
	movups	[edi],xmm2
	xorps	xmm5,[48+esp]
	movups	[16+edi],xmm3
	xorps	xmm6,xmm1
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	lea	edi,[80+edi]
	jmp	NEAR L$054xts_enc_done
align	16
L$050xts_enc_one:
	movups	xmm2,[esi]
	lea	esi,[16+esi]
	xorps	xmm2,xmm5
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$055enc1_loop_9:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$055enc1_loop_9
db	102,15,56,221,209
	xorps	xmm2,xmm5
	movups	[edi],xmm2
	lea	edi,[16+edi]
	movdqa	xmm1,xmm5
	jmp	NEAR L$054xts_enc_done
align	16
L$051xts_enc_two:
	movaps	xmm6,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	lea	esi,[32+esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	call	__aesni_encrypt2
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	lea	edi,[32+edi]
	movdqa	xmm1,xmm6
	jmp	NEAR L$054xts_enc_done
align	16
L$052xts_enc_three:
	movaps	xmm7,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	movups	xmm4,[32+esi]
	lea	esi,[48+esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	call	__aesni_encrypt3
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	lea	edi,[48+edi]
	movdqa	xmm1,xmm7
	jmp	NEAR L$054xts_enc_done
align	16
L$053xts_enc_four:
	movaps	xmm6,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	movups	xmm4,[32+esi]
	xorps	xmm2,[esp]
	movups	xmm5,[48+esi]
	lea	esi,[64+esi]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm7
	xorps	xmm5,xmm6
	call	__aesni_encrypt4
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm7
	movups	[edi],xmm2
	xorps	xmm5,xmm6
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	lea	edi,[64+edi]
	movdqa	xmm1,xmm6
	jmp	NEAR L$054xts_enc_done
align	16
L$049xts_enc_done6x:
	mov	eax,DWORD [112+esp]
	and	eax,15
	jz	NEAR L$056xts_enc_ret
	movdqa	xmm5,xmm1
	mov	DWORD [112+esp],eax
	jmp	NEAR L$057xts_enc_steal
align	16
L$054xts_enc_done:
	mov	eax,DWORD [112+esp]
	pxor	xmm0,xmm0
	and	eax,15
	jz	NEAR L$056xts_enc_ret
	pcmpgtd	xmm0,xmm1
	mov	DWORD [112+esp],eax
	pshufd	xmm5,xmm0,19
	paddq	xmm1,xmm1
	pand	xmm5,[96+esp]
	pxor	xmm5,xmm1
L$057xts_enc_steal:
	movzx	ecx,BYTE [esi]
	movzx	edx,BYTE [edi-16]
	lea	esi,[1+esi]
	mov	BYTE [edi-16],cl
	mov	BYTE [edi],dl
	lea	edi,[1+edi]
	sub	eax,1
	jnz	NEAR L$057xts_enc_steal
	sub	edi,DWORD [112+esp]
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,[edi-16]
	xorps	xmm2,xmm5
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$058enc1_loop_10:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$058enc1_loop_10
db	102,15,56,221,209
	xorps	xmm2,xmm5
	movups	[edi-16],xmm2
L$056xts_enc_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movdqa	[esp],xmm0
	pxor	xmm3,xmm3
	movdqa	[16+esp],xmm0
	pxor	xmm4,xmm4
	movdqa	[32+esp],xmm0
	pxor	xmm5,xmm5
	movdqa	[48+esp],xmm0
	pxor	xmm6,xmm6
	movdqa	[64+esp],xmm0
	pxor	xmm7,xmm7
	movdqa	[80+esp],xmm0
	mov	esp,DWORD [116+esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_xts_decrypt
align	16
_aesni_xts_decrypt:
L$_aesni_xts_decrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edx,DWORD [36+esp]
	mov	esi,DWORD [40+esp]
	mov	ecx,DWORD [240+edx]
	movups	xmm2,[esi]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$059enc1_loop_11:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$059enc1_loop_11
db	102,15,56,221,209
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	mov	ebp,esp
	sub	esp,120
	and	esp,-16
	xor	ebx,ebx
	test	eax,15
	setnz	bl
	shl	ebx,4
	sub	eax,ebx
	mov	DWORD [96+esp],135
	mov	DWORD [100+esp],0
	mov	DWORD [104+esp],1
	mov	DWORD [108+esp],0
	mov	DWORD [112+esp],eax
	mov	DWORD [116+esp],ebp
	mov	ecx,DWORD [240+edx]
	mov	ebp,edx
	mov	ebx,ecx
	movdqa	xmm1,xmm2
	pxor	xmm0,xmm0
	movdqa	xmm3,[96+esp]
	pcmpgtd	xmm0,xmm1
	and	eax,-16
	sub	eax,96
	jc	NEAR L$060xts_dec_short
	shl	ecx,4
	mov	ebx,16
	sub	ebx,ecx
	lea	edx,[32+ecx*1+edx]
	jmp	NEAR L$061xts_dec_loop6
align	16
L$061xts_dec_loop6:
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[16+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[32+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	[48+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm7,xmm0,19
	movdqa	[64+esp],xmm1
	paddq	xmm1,xmm1
	movups	xmm0,[ebp]
	pand	xmm7,xmm3
	movups	xmm2,[esi]
	pxor	xmm7,xmm1
	mov	ecx,ebx
	movdqu	xmm3,[16+esi]
	xorps	xmm2,xmm0
	movdqu	xmm4,[32+esi]
	pxor	xmm3,xmm0
	movdqu	xmm5,[48+esi]
	pxor	xmm4,xmm0
	movdqu	xmm6,[64+esi]
	pxor	xmm5,xmm0
	movdqu	xmm1,[80+esi]
	pxor	xmm6,xmm0
	lea	esi,[96+esi]
	pxor	xmm2,[esp]
	movdqa	[80+esp],xmm7
	pxor	xmm7,xmm1
	movups	xmm1,[16+ebp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
db	102,15,56,222,209
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
db	102,15,56,222,217
	pxor	xmm7,xmm0
	movups	xmm0,[32+ebp]
db	102,15,56,222,225
db	102,15,56,222,233
db	102,15,56,222,241
db	102,15,56,222,249
	call	L$_aesni_decrypt6_enter
	movdqa	xmm1,[80+esp]
	pxor	xmm0,xmm0
	xorps	xmm2,[esp]
	pcmpgtd	xmm0,xmm1
	xorps	xmm3,[16+esp]
	movups	[edi],xmm2
	xorps	xmm4,[32+esp]
	movups	[16+edi],xmm3
	xorps	xmm5,[48+esp]
	movups	[32+edi],xmm4
	xorps	xmm6,[64+esp]
	movups	[48+edi],xmm5
	xorps	xmm7,xmm1
	movups	[64+edi],xmm6
	pshufd	xmm2,xmm0,19
	movups	[80+edi],xmm7
	lea	edi,[96+edi]
	movdqa	xmm3,[96+esp]
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	sub	eax,96
	jnc	NEAR L$061xts_dec_loop6
	mov	ecx,DWORD [240+ebp]
	mov	edx,ebp
	mov	ebx,ecx
L$060xts_dec_short:
	add	eax,96
	jz	NEAR L$062xts_dec_done6x
	movdqa	xmm5,xmm1
	cmp	eax,32
	jb	NEAR L$063xts_dec_one
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	je	NEAR L$064xts_dec_two
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	cmp	eax,64
	jb	NEAR L$065xts_dec_three
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm7,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	movdqa	[esp],xmm5
	movdqa	[16+esp],xmm6
	je	NEAR L$066xts_dec_four
	movdqa	[32+esp],xmm7
	pshufd	xmm7,xmm0,19
	movdqa	[48+esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm7,xmm3
	pxor	xmm7,xmm1
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	pxor	xmm2,[esp]
	movdqu	xmm5,[48+esi]
	pxor	xmm3,[16+esp]
	movdqu	xmm6,[64+esi]
	pxor	xmm4,[32+esp]
	lea	esi,[80+esi]
	pxor	xmm5,[48+esp]
	movdqa	[64+esp],xmm7
	pxor	xmm6,xmm7
	call	__aesni_decrypt6
	movaps	xmm1,[64+esp]
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,[32+esp]
	movups	[edi],xmm2
	xorps	xmm5,[48+esp]
	movups	[16+edi],xmm3
	xorps	xmm6,xmm1
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	movups	[64+edi],xmm6
	lea	edi,[80+edi]
	jmp	NEAR L$067xts_dec_done
align	16
L$063xts_dec_one:
	movups	xmm2,[esi]
	lea	esi,[16+esi]
	xorps	xmm2,xmm5
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$068dec1_loop_12:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$068dec1_loop_12
db	102,15,56,223,209
	xorps	xmm2,xmm5
	movups	[edi],xmm2
	lea	edi,[16+edi]
	movdqa	xmm1,xmm5
	jmp	NEAR L$067xts_dec_done
align	16
L$064xts_dec_two:
	movaps	xmm6,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	lea	esi,[32+esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	call	__aesni_decrypt2
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	lea	edi,[32+edi]
	movdqa	xmm1,xmm6
	jmp	NEAR L$067xts_dec_done
align	16
L$065xts_dec_three:
	movaps	xmm7,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	movups	xmm4,[32+esi]
	lea	esi,[48+esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	call	__aesni_decrypt3
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	lea	edi,[48+edi]
	movdqa	xmm1,xmm7
	jmp	NEAR L$067xts_dec_done
align	16
L$066xts_dec_four:
	movaps	xmm6,xmm1
	movups	xmm2,[esi]
	movups	xmm3,[16+esi]
	movups	xmm4,[32+esi]
	xorps	xmm2,[esp]
	movups	xmm5,[48+esi]
	lea	esi,[64+esi]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm7
	xorps	xmm5,xmm6
	call	__aesni_decrypt4
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm7
	movups	[edi],xmm2
	xorps	xmm5,xmm6
	movups	[16+edi],xmm3
	movups	[32+edi],xmm4
	movups	[48+edi],xmm5
	lea	edi,[64+edi]
	movdqa	xmm1,xmm6
	jmp	NEAR L$067xts_dec_done
align	16
L$062xts_dec_done6x:
	mov	eax,DWORD [112+esp]
	and	eax,15
	jz	NEAR L$069xts_dec_ret
	mov	DWORD [112+esp],eax
	jmp	NEAR L$070xts_dec_only_one_more
align	16
L$067xts_dec_done:
	mov	eax,DWORD [112+esp]
	pxor	xmm0,xmm0
	and	eax,15
	jz	NEAR L$069xts_dec_ret
	pcmpgtd	xmm0,xmm1
	mov	DWORD [112+esp],eax
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm3,[96+esp]
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
L$070xts_dec_only_one_more:
	pshufd	xmm5,xmm0,19
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm5,xmm3
	pxor	xmm5,xmm1
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,[esi]
	xorps	xmm2,xmm5
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$071dec1_loop_13:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$071dec1_loop_13
db	102,15,56,223,209
	xorps	xmm2,xmm5
	movups	[edi],xmm2
L$072xts_dec_steal:
	movzx	ecx,BYTE [16+esi]
	movzx	edx,BYTE [edi]
	lea	esi,[1+esi]
	mov	BYTE [edi],cl
	mov	BYTE [16+edi],dl
	lea	edi,[1+edi]
	sub	eax,1
	jnz	NEAR L$072xts_dec_steal
	sub	edi,DWORD [112+esp]
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,[edi]
	xorps	xmm2,xmm6
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$073dec1_loop_14:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$073dec1_loop_14
db	102,15,56,223,209
	xorps	xmm2,xmm6
	movups	[edi],xmm2
L$069xts_dec_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movdqa	[esp],xmm0
	pxor	xmm3,xmm3
	movdqa	[16+esp],xmm0
	pxor	xmm4,xmm4
	movdqa	[32+esp],xmm0
	pxor	xmm5,xmm5
	movdqa	[48+esp],xmm0
	pxor	xmm6,xmm6
	movdqa	[64+esp],xmm0
	pxor	xmm7,xmm7
	movdqa	[80+esp],xmm0
	mov	esp,DWORD [116+esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_ocb_encrypt
align	16
_aesni_ocb_encrypt:
L$_aesni_ocb_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ecx,DWORD [40+esp]
	mov	ebx,DWORD [48+esp]
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	movdqu	xmm0,[ecx]
	mov	ebp,DWORD [36+esp]
	movdqu	xmm1,[ebx]
	mov	ebx,DWORD [44+esp]
	mov	ecx,esp
	sub	esp,132
	and	esp,-16
	sub	edi,esi
	shl	eax,4
	lea	eax,[eax*1+esi-96]
	mov	DWORD [120+esp],edi
	mov	DWORD [124+esp],eax
	mov	DWORD [128+esp],ecx
	mov	ecx,DWORD [240+edx]
	test	ebp,1
	jnz	NEAR L$074odd
	bsf	eax,ebp
	add	ebp,1
	shl	eax,4
	movdqu	xmm7,[eax*1+ebx]
	mov	eax,edx
	movdqu	xmm2,[esi]
	lea	esi,[16+esi]
	pxor	xmm7,xmm0
	pxor	xmm1,xmm2
	pxor	xmm2,xmm7
	movdqa	xmm6,xmm1
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$075enc1_loop_15:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$075enc1_loop_15
db	102,15,56,221,209
	xorps	xmm2,xmm7
	movdqa	xmm0,xmm7
	movdqa	xmm1,xmm6
	movups	[esi*1+edi-16],xmm2
	mov	ecx,DWORD [240+eax]
	mov	edx,eax
	mov	eax,DWORD [124+esp]
L$074odd:
	shl	ecx,4
	mov	edi,16
	sub	edi,ecx
	mov	DWORD [112+esp],edx
	lea	edx,[32+ecx*1+edx]
	mov	DWORD [116+esp],edi
	cmp	esi,eax
	ja	NEAR L$076short
	jmp	NEAR L$077grandloop
align	32
L$077grandloop:
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	lea	edi,[5+ebp]
	add	ebp,6
	bsf	ecx,ecx
	bsf	eax,eax
	bsf	edi,edi
	shl	ecx,4
	shl	eax,4
	shl	edi,4
	movdqu	xmm2,[ebx]
	movdqu	xmm3,[ecx*1+ebx]
	mov	ecx,DWORD [116+esp]
	movdqa	xmm4,xmm2
	movdqu	xmm5,[eax*1+ebx]
	movdqa	xmm6,xmm2
	movdqu	xmm7,[edi*1+ebx]
	pxor	xmm2,xmm0
	pxor	xmm3,xmm2
	movdqa	[esp],xmm2
	pxor	xmm4,xmm3
	movdqa	[16+esp],xmm3
	pxor	xmm5,xmm4
	movdqa	[32+esp],xmm4
	pxor	xmm6,xmm5
	movdqa	[48+esp],xmm5
	pxor	xmm7,xmm6
	movdqa	[64+esp],xmm6
	movdqa	[80+esp],xmm7
	movups	xmm0,[ecx*1+edx-48]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
	pxor	xmm1,xmm2
	pxor	xmm2,xmm0
	pxor	xmm1,xmm3
	pxor	xmm3,xmm0
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	pxor	xmm1,xmm5
	pxor	xmm5,xmm0
	pxor	xmm1,xmm6
	pxor	xmm6,xmm0
	pxor	xmm1,xmm7
	pxor	xmm7,xmm0
	movdqa	[96+esp],xmm1
	movups	xmm1,[ecx*1+edx-32]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	pxor	xmm7,[80+esp]
	movups	xmm0,[ecx*1+edx-16]
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
	mov	edi,DWORD [120+esp]
	mov	eax,DWORD [124+esp]
	call	L$_aesni_encrypt6_enter
	movdqa	xmm0,[80+esp]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	pxor	xmm7,xmm0
	movdqa	xmm1,[96+esp]
	movdqu	[esi*1+edi-96],xmm2
	movdqu	[esi*1+edi-80],xmm3
	movdqu	[esi*1+edi-64],xmm4
	movdqu	[esi*1+edi-48],xmm5
	movdqu	[esi*1+edi-32],xmm6
	movdqu	[esi*1+edi-16],xmm7
	cmp	esi,eax
	jbe	NEAR L$077grandloop
L$076short:
	add	eax,96
	sub	eax,esi
	jz	NEAR L$078done
	cmp	eax,32
	jb	NEAR L$079one
	je	NEAR L$080two
	cmp	eax,64
	jb	NEAR L$081three
	je	NEAR L$082four
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	bsf	ecx,ecx
	bsf	eax,eax
	shl	ecx,4
	shl	eax,4
	movdqu	xmm2,[ebx]
	movdqu	xmm3,[ecx*1+ebx]
	mov	ecx,DWORD [116+esp]
	movdqa	xmm4,xmm2
	movdqu	xmm5,[eax*1+ebx]
	movdqa	xmm6,xmm2
	pxor	xmm2,xmm0
	pxor	xmm3,xmm2
	movdqa	[esp],xmm2
	pxor	xmm4,xmm3
	movdqa	[16+esp],xmm3
	pxor	xmm5,xmm4
	movdqa	[32+esp],xmm4
	pxor	xmm6,xmm5
	movdqa	[48+esp],xmm5
	pxor	xmm7,xmm6
	movdqa	[64+esp],xmm6
	movups	xmm0,[ecx*1+edx-48]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	pxor	xmm7,xmm7
	pxor	xmm1,xmm2
	pxor	xmm2,xmm0
	pxor	xmm1,xmm3
	pxor	xmm3,xmm0
	pxor	xmm1,xmm4
	pxor	xmm4,xmm0
	pxor	xmm1,xmm5
	pxor	xmm5,xmm0
	pxor	xmm1,xmm6
	pxor	xmm6,xmm0
	movdqa	[96+esp],xmm1
	movups	xmm1,[ecx*1+edx-32]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	movups	xmm0,[ecx*1+edx-16]
db	102,15,56,220,209
db	102,15,56,220,217
db	102,15,56,220,225
db	102,15,56,220,233
db	102,15,56,220,241
db	102,15,56,220,249
	mov	edi,DWORD [120+esp]
	call	L$_aesni_encrypt6_enter
	movdqa	xmm0,[64+esp]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,xmm0
	movdqa	xmm1,[96+esp]
	movdqu	[esi*1+edi],xmm2
	movdqu	[16+esi*1+edi],xmm3
	movdqu	[32+esi*1+edi],xmm4
	movdqu	[48+esi*1+edi],xmm5
	movdqu	[64+esi*1+edi],xmm6
	jmp	NEAR L$078done
align	16
L$079one:
	movdqu	xmm7,[ebx]
	mov	edx,DWORD [112+esp]
	movdqu	xmm2,[esi]
	mov	ecx,DWORD [240+edx]
	pxor	xmm7,xmm0
	pxor	xmm1,xmm2
	pxor	xmm2,xmm7
	movdqa	xmm6,xmm1
	mov	edi,DWORD [120+esp]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$083enc1_loop_16:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$083enc1_loop_16
db	102,15,56,221,209
	xorps	xmm2,xmm7
	movdqa	xmm0,xmm7
	movdqa	xmm1,xmm6
	movups	[esi*1+edi],xmm2
	jmp	NEAR L$078done
align	16
L$080two:
	lea	ecx,[1+ebp]
	mov	edx,DWORD [112+esp]
	bsf	ecx,ecx
	shl	ecx,4
	movdqu	xmm6,[ebx]
	movdqu	xmm7,[ecx*1+ebx]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	mov	ecx,DWORD [240+edx]
	pxor	xmm6,xmm0
	pxor	xmm7,xmm6
	pxor	xmm1,xmm2
	pxor	xmm2,xmm6
	pxor	xmm1,xmm3
	pxor	xmm3,xmm7
	movdqa	xmm5,xmm1
	mov	edi,DWORD [120+esp]
	call	__aesni_encrypt2
	xorps	xmm2,xmm6
	xorps	xmm3,xmm7
	movdqa	xmm0,xmm7
	movdqa	xmm1,xmm5
	movups	[esi*1+edi],xmm2
	movups	[16+esi*1+edi],xmm3
	jmp	NEAR L$078done
align	16
L$081three:
	lea	ecx,[1+ebp]
	mov	edx,DWORD [112+esp]
	bsf	ecx,ecx
	shl	ecx,4
	movdqu	xmm5,[ebx]
	movdqu	xmm6,[ecx*1+ebx]
	movdqa	xmm7,xmm5
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	mov	ecx,DWORD [240+edx]
	pxor	xmm5,xmm0
	pxor	xmm6,xmm5
	pxor	xmm7,xmm6
	pxor	xmm1,xmm2
	pxor	xmm2,xmm5
	pxor	xmm1,xmm3
	pxor	xmm3,xmm6
	pxor	xmm1,xmm4
	pxor	xmm4,xmm7
	movdqa	[96+esp],xmm1
	mov	edi,DWORD [120+esp]
	call	__aesni_encrypt3
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movdqa	xmm0,xmm7
	movdqa	xmm1,[96+esp]
	movups	[esi*1+edi],xmm2
	movups	[16+esi*1+edi],xmm3
	movups	[32+esi*1+edi],xmm4
	jmp	NEAR L$078done
align	16
L$082four:
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	bsf	ecx,ecx
	bsf	eax,eax
	mov	edx,DWORD [112+esp]
	shl	ecx,4
	shl	eax,4
	movdqu	xmm4,[ebx]
	movdqu	xmm5,[ecx*1+ebx]
	movdqa	xmm6,xmm4
	movdqu	xmm7,[eax*1+ebx]
	pxor	xmm4,xmm0
	movdqu	xmm2,[esi]
	pxor	xmm5,xmm4
	movdqu	xmm3,[16+esi]
	pxor	xmm6,xmm5
	movdqa	[esp],xmm4
	pxor	xmm7,xmm6
	movdqa	[16+esp],xmm5
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	mov	ecx,DWORD [240+edx]
	pxor	xmm1,xmm2
	pxor	xmm2,[esp]
	pxor	xmm1,xmm3
	pxor	xmm3,[16+esp]
	pxor	xmm1,xmm4
	pxor	xmm4,xmm6
	pxor	xmm1,xmm5
	pxor	xmm5,xmm7
	movdqa	[96+esp],xmm1
	mov	edi,DWORD [120+esp]
	call	__aesni_encrypt4
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm6
	movups	[esi*1+edi],xmm2
	xorps	xmm5,xmm7
	movups	[16+esi*1+edi],xmm3
	movdqa	xmm0,xmm7
	movups	[32+esi*1+edi],xmm4
	movdqa	xmm1,[96+esp]
	movups	[48+esi*1+edi],xmm5
L$078done:
	mov	edx,DWORD [128+esp]
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	movdqa	[esp],xmm2
	pxor	xmm4,xmm4
	movdqa	[16+esp],xmm2
	pxor	xmm5,xmm5
	movdqa	[32+esp],xmm2
	pxor	xmm6,xmm6
	movdqa	[48+esp],xmm2
	pxor	xmm7,xmm7
	movdqa	[64+esp],xmm2
	movdqa	[80+esp],xmm2
	movdqa	[96+esp],xmm2
	lea	esp,[edx]
	mov	ecx,DWORD [40+esp]
	mov	ebx,DWORD [48+esp]
	movdqu	[ecx],xmm0
	pxor	xmm0,xmm0
	movdqu	[ebx],xmm1
	pxor	xmm1,xmm1
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_ocb_decrypt
align	16
_aesni_ocb_decrypt:
L$_aesni_ocb_decrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ecx,DWORD [40+esp]
	mov	ebx,DWORD [48+esp]
	mov	esi,DWORD [20+esp]
	mov	edi,DWORD [24+esp]
	mov	eax,DWORD [28+esp]
	mov	edx,DWORD [32+esp]
	movdqu	xmm0,[ecx]
	mov	ebp,DWORD [36+esp]
	movdqu	xmm1,[ebx]
	mov	ebx,DWORD [44+esp]
	mov	ecx,esp
	sub	esp,132
	and	esp,-16
	sub	edi,esi
	shl	eax,4
	lea	eax,[eax*1+esi-96]
	mov	DWORD [120+esp],edi
	mov	DWORD [124+esp],eax
	mov	DWORD [128+esp],ecx
	mov	ecx,DWORD [240+edx]
	test	ebp,1
	jnz	NEAR L$084odd
	bsf	eax,ebp
	add	ebp,1
	shl	eax,4
	movdqu	xmm7,[eax*1+ebx]
	mov	eax,edx
	movdqu	xmm2,[esi]
	lea	esi,[16+esi]
	pxor	xmm7,xmm0
	pxor	xmm2,xmm7
	movdqa	xmm6,xmm1
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$085dec1_loop_17:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$085dec1_loop_17
db	102,15,56,223,209
	xorps	xmm2,xmm7
	movaps	xmm1,xmm6
	movdqa	xmm0,xmm7
	xorps	xmm1,xmm2
	movups	[esi*1+edi-16],xmm2
	mov	ecx,DWORD [240+eax]
	mov	edx,eax
	mov	eax,DWORD [124+esp]
L$084odd:
	shl	ecx,4
	mov	edi,16
	sub	edi,ecx
	mov	DWORD [112+esp],edx
	lea	edx,[32+ecx*1+edx]
	mov	DWORD [116+esp],edi
	cmp	esi,eax
	ja	NEAR L$086short
	jmp	NEAR L$087grandloop
align	32
L$087grandloop:
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	lea	edi,[5+ebp]
	add	ebp,6
	bsf	ecx,ecx
	bsf	eax,eax
	bsf	edi,edi
	shl	ecx,4
	shl	eax,4
	shl	edi,4
	movdqu	xmm2,[ebx]
	movdqu	xmm3,[ecx*1+ebx]
	mov	ecx,DWORD [116+esp]
	movdqa	xmm4,xmm2
	movdqu	xmm5,[eax*1+ebx]
	movdqa	xmm6,xmm2
	movdqu	xmm7,[edi*1+ebx]
	pxor	xmm2,xmm0
	pxor	xmm3,xmm2
	movdqa	[esp],xmm2
	pxor	xmm4,xmm3
	movdqa	[16+esp],xmm3
	pxor	xmm5,xmm4
	movdqa	[32+esp],xmm4
	pxor	xmm6,xmm5
	movdqa	[48+esp],xmm5
	pxor	xmm7,xmm6
	movdqa	[64+esp],xmm6
	movdqa	[80+esp],xmm7
	movups	xmm0,[ecx*1+edx-48]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	movdqu	xmm7,[80+esi]
	lea	esi,[96+esi]
	movdqa	[96+esp],xmm1
	pxor	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
	pxor	xmm7,xmm0
	movups	xmm1,[ecx*1+edx-32]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	pxor	xmm7,[80+esp]
	movups	xmm0,[ecx*1+edx-16]
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,222,233
db	102,15,56,222,241
db	102,15,56,222,249
	mov	edi,DWORD [120+esp]
	mov	eax,DWORD [124+esp]
	call	L$_aesni_decrypt6_enter
	movdqa	xmm0,[80+esp]
	pxor	xmm2,[esp]
	movdqa	xmm1,[96+esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	pxor	xmm7,xmm0
	pxor	xmm1,xmm2
	movdqu	[esi*1+edi-96],xmm2
	pxor	xmm1,xmm3
	movdqu	[esi*1+edi-80],xmm3
	pxor	xmm1,xmm4
	movdqu	[esi*1+edi-64],xmm4
	pxor	xmm1,xmm5
	movdqu	[esi*1+edi-48],xmm5
	pxor	xmm1,xmm6
	movdqu	[esi*1+edi-32],xmm6
	pxor	xmm1,xmm7
	movdqu	[esi*1+edi-16],xmm7
	cmp	esi,eax
	jbe	NEAR L$087grandloop
L$086short:
	add	eax,96
	sub	eax,esi
	jz	NEAR L$088done
	cmp	eax,32
	jb	NEAR L$089one
	je	NEAR L$090two
	cmp	eax,64
	jb	NEAR L$091three
	je	NEAR L$092four
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	bsf	ecx,ecx
	bsf	eax,eax
	shl	ecx,4
	shl	eax,4
	movdqu	xmm2,[ebx]
	movdqu	xmm3,[ecx*1+ebx]
	mov	ecx,DWORD [116+esp]
	movdqa	xmm4,xmm2
	movdqu	xmm5,[eax*1+ebx]
	movdqa	xmm6,xmm2
	pxor	xmm2,xmm0
	pxor	xmm3,xmm2
	movdqa	[esp],xmm2
	pxor	xmm4,xmm3
	movdqa	[16+esp],xmm3
	pxor	xmm5,xmm4
	movdqa	[32+esp],xmm4
	pxor	xmm6,xmm5
	movdqa	[48+esp],xmm5
	pxor	xmm7,xmm6
	movdqa	[64+esp],xmm6
	movups	xmm0,[ecx*1+edx-48]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	pxor	xmm7,xmm7
	movdqa	[96+esp],xmm1
	pxor	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
	movups	xmm1,[ecx*1+edx-32]
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,[64+esp]
	movups	xmm0,[ecx*1+edx-16]
db	102,15,56,222,209
db	102,15,56,222,217
db	102,15,56,222,225
db	102,15,56,222,233
db	102,15,56,222,241
db	102,15,56,222,249
	mov	edi,DWORD [120+esp]
	call	L$_aesni_decrypt6_enter
	movdqa	xmm0,[64+esp]
	pxor	xmm2,[esp]
	movdqa	xmm1,[96+esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,[32+esp]
	pxor	xmm5,[48+esp]
	pxor	xmm6,xmm0
	pxor	xmm1,xmm2
	movdqu	[esi*1+edi],xmm2
	pxor	xmm1,xmm3
	movdqu	[16+esi*1+edi],xmm3
	pxor	xmm1,xmm4
	movdqu	[32+esi*1+edi],xmm4
	pxor	xmm1,xmm5
	movdqu	[48+esi*1+edi],xmm5
	pxor	xmm1,xmm6
	movdqu	[64+esi*1+edi],xmm6
	jmp	NEAR L$088done
align	16
L$089one:
	movdqu	xmm7,[ebx]
	mov	edx,DWORD [112+esp]
	movdqu	xmm2,[esi]
	mov	ecx,DWORD [240+edx]
	pxor	xmm7,xmm0
	pxor	xmm2,xmm7
	movdqa	xmm6,xmm1
	mov	edi,DWORD [120+esp]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$093dec1_loop_18:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$093dec1_loop_18
db	102,15,56,223,209
	xorps	xmm2,xmm7
	movaps	xmm1,xmm6
	movdqa	xmm0,xmm7
	xorps	xmm1,xmm2
	movups	[esi*1+edi],xmm2
	jmp	NEAR L$088done
align	16
L$090two:
	lea	ecx,[1+ebp]
	mov	edx,DWORD [112+esp]
	bsf	ecx,ecx
	shl	ecx,4
	movdqu	xmm6,[ebx]
	movdqu	xmm7,[ecx*1+ebx]
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	mov	ecx,DWORD [240+edx]
	movdqa	xmm5,xmm1
	pxor	xmm6,xmm0
	pxor	xmm7,xmm6
	pxor	xmm2,xmm6
	pxor	xmm3,xmm7
	mov	edi,DWORD [120+esp]
	call	__aesni_decrypt2
	xorps	xmm2,xmm6
	xorps	xmm3,xmm7
	movdqa	xmm0,xmm7
	xorps	xmm5,xmm2
	movups	[esi*1+edi],xmm2
	xorps	xmm5,xmm3
	movups	[16+esi*1+edi],xmm3
	movaps	xmm1,xmm5
	jmp	NEAR L$088done
align	16
L$091three:
	lea	ecx,[1+ebp]
	mov	edx,DWORD [112+esp]
	bsf	ecx,ecx
	shl	ecx,4
	movdqu	xmm5,[ebx]
	movdqu	xmm6,[ecx*1+ebx]
	movdqa	xmm7,xmm5
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	mov	ecx,DWORD [240+edx]
	movdqa	[96+esp],xmm1
	pxor	xmm5,xmm0
	pxor	xmm6,xmm5
	pxor	xmm7,xmm6
	pxor	xmm2,xmm5
	pxor	xmm3,xmm6
	pxor	xmm4,xmm7
	mov	edi,DWORD [120+esp]
	call	__aesni_decrypt3
	movdqa	xmm1,[96+esp]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movups	[esi*1+edi],xmm2
	pxor	xmm1,xmm2
	movdqa	xmm0,xmm7
	movups	[16+esi*1+edi],xmm3
	pxor	xmm1,xmm3
	movups	[32+esi*1+edi],xmm4
	pxor	xmm1,xmm4
	jmp	NEAR L$088done
align	16
L$092four:
	lea	ecx,[1+ebp]
	lea	eax,[3+ebp]
	bsf	ecx,ecx
	bsf	eax,eax
	mov	edx,DWORD [112+esp]
	shl	ecx,4
	shl	eax,4
	movdqu	xmm4,[ebx]
	movdqu	xmm5,[ecx*1+ebx]
	movdqa	xmm6,xmm4
	movdqu	xmm7,[eax*1+ebx]
	pxor	xmm4,xmm0
	movdqu	xmm2,[esi]
	pxor	xmm5,xmm4
	movdqu	xmm3,[16+esi]
	pxor	xmm6,xmm5
	movdqa	[esp],xmm4
	pxor	xmm7,xmm6
	movdqa	[16+esp],xmm5
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	mov	ecx,DWORD [240+edx]
	movdqa	[96+esp],xmm1
	pxor	xmm2,[esp]
	pxor	xmm3,[16+esp]
	pxor	xmm4,xmm6
	pxor	xmm5,xmm7
	mov	edi,DWORD [120+esp]
	call	__aesni_decrypt4
	movdqa	xmm1,[96+esp]
	xorps	xmm2,[esp]
	xorps	xmm3,[16+esp]
	xorps	xmm4,xmm6
	movups	[esi*1+edi],xmm2
	pxor	xmm1,xmm2
	xorps	xmm5,xmm7
	movups	[16+esi*1+edi],xmm3
	pxor	xmm1,xmm3
	movdqa	xmm0,xmm7
	movups	[32+esi*1+edi],xmm4
	pxor	xmm1,xmm4
	movups	[48+esi*1+edi],xmm5
	pxor	xmm1,xmm5
L$088done:
	mov	edx,DWORD [128+esp]
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	movdqa	[esp],xmm2
	pxor	xmm4,xmm4
	movdqa	[16+esp],xmm2
	pxor	xmm5,xmm5
	movdqa	[32+esp],xmm2
	pxor	xmm6,xmm6
	movdqa	[48+esp],xmm2
	pxor	xmm7,xmm7
	movdqa	[64+esp],xmm2
	movdqa	[80+esp],xmm2
	movdqa	[96+esp],xmm2
	lea	esp,[edx]
	mov	ecx,DWORD [40+esp]
	mov	ebx,DWORD [48+esp]
	movdqu	[ecx],xmm0
	pxor	xmm0,xmm0
	movdqu	[ebx],xmm1
	pxor	xmm1,xmm1
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
global	_aesni_cbc_encrypt
align	16
_aesni_cbc_encrypt:
L$_aesni_cbc_encrypt_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD [20+esp]
	mov	ebx,esp
	mov	edi,DWORD [24+esp]
	sub	ebx,24
	mov	eax,DWORD [28+esp]
	and	ebx,-16
	mov	edx,DWORD [32+esp]
	mov	ebp,DWORD [36+esp]
	test	eax,eax
	jz	NEAR L$094cbc_abort
	cmp	DWORD [40+esp],0
	xchg	ebx,esp
	movups	xmm7,[ebp]
	mov	ecx,DWORD [240+edx]
	mov	ebp,edx
	mov	DWORD [16+esp],ebx
	mov	ebx,ecx
	je	NEAR L$095cbc_decrypt
	movaps	xmm2,xmm7
	cmp	eax,16
	jb	NEAR L$096cbc_enc_tail
	sub	eax,16
	jmp	NEAR L$097cbc_enc_loop
align	16
L$097cbc_enc_loop:
	movups	xmm7,[esi]
	lea	esi,[16+esi]
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	xorps	xmm7,xmm0
	lea	edx,[32+edx]
	xorps	xmm2,xmm7
L$098enc1_loop_19:
db	102,15,56,220,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$098enc1_loop_19
db	102,15,56,221,209
	mov	ecx,ebx
	mov	edx,ebp
	movups	[edi],xmm2
	lea	edi,[16+edi]
	sub	eax,16
	jnc	NEAR L$097cbc_enc_loop
	add	eax,16
	jnz	NEAR L$096cbc_enc_tail
	movaps	xmm7,xmm2
	pxor	xmm2,xmm2
	jmp	NEAR L$099cbc_ret
L$096cbc_enc_tail:
	mov	ecx,eax
dd	2767451785
	mov	ecx,16
	sub	ecx,eax
	xor	eax,eax
dd	2868115081
	lea	edi,[edi-16]
	mov	ecx,ebx
	mov	esi,edi
	mov	edx,ebp
	jmp	NEAR L$097cbc_enc_loop
align	16
L$095cbc_decrypt:
	cmp	eax,80
	jbe	NEAR L$100cbc_dec_tail
	movaps	[esp],xmm7
	sub	eax,80
	jmp	NEAR L$101cbc_dec_loop6_enter
align	16
L$102cbc_dec_loop6:
	movaps	[esp],xmm0
	movups	[edi],xmm7
	lea	edi,[16+edi]
L$101cbc_dec_loop6_enter:
	movdqu	xmm2,[esi]
	movdqu	xmm3,[16+esi]
	movdqu	xmm4,[32+esi]
	movdqu	xmm5,[48+esi]
	movdqu	xmm6,[64+esi]
	movdqu	xmm7,[80+esi]
	call	__aesni_decrypt6
	movups	xmm1,[esi]
	movups	xmm0,[16+esi]
	xorps	xmm2,[esp]
	xorps	xmm3,xmm1
	movups	xmm1,[32+esi]
	xorps	xmm4,xmm0
	movups	xmm0,[48+esi]
	xorps	xmm5,xmm1
	movups	xmm1,[64+esi]
	xorps	xmm6,xmm0
	movups	xmm0,[80+esi]
	xorps	xmm7,xmm1
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	lea	esi,[96+esi]
	movups	[32+edi],xmm4
	mov	ecx,ebx
	movups	[48+edi],xmm5
	mov	edx,ebp
	movups	[64+edi],xmm6
	lea	edi,[80+edi]
	sub	eax,96
	ja	NEAR L$102cbc_dec_loop6
	movaps	xmm2,xmm7
	movaps	xmm7,xmm0
	add	eax,80
	jle	NEAR L$103cbc_dec_clear_tail_collected
	movups	[edi],xmm2
	lea	edi,[16+edi]
L$100cbc_dec_tail:
	movups	xmm2,[esi]
	movaps	xmm6,xmm2
	cmp	eax,16
	jbe	NEAR L$104cbc_dec_one
	movups	xmm3,[16+esi]
	movaps	xmm5,xmm3
	cmp	eax,32
	jbe	NEAR L$105cbc_dec_two
	movups	xmm4,[32+esi]
	cmp	eax,48
	jbe	NEAR L$106cbc_dec_three
	movups	xmm5,[48+esi]
	cmp	eax,64
	jbe	NEAR L$107cbc_dec_four
	movups	xmm6,[64+esi]
	movaps	[esp],xmm7
	movups	xmm2,[esi]
	xorps	xmm7,xmm7
	call	__aesni_decrypt6
	movups	xmm1,[esi]
	movups	xmm0,[16+esi]
	xorps	xmm2,[esp]
	xorps	xmm3,xmm1
	movups	xmm1,[32+esi]
	xorps	xmm4,xmm0
	movups	xmm0,[48+esi]
	xorps	xmm5,xmm1
	movups	xmm7,[64+esi]
	xorps	xmm6,xmm0
	movups	[edi],xmm2
	movups	[16+edi],xmm3
	pxor	xmm3,xmm3
	movups	[32+edi],xmm4
	pxor	xmm4,xmm4
	movups	[48+edi],xmm5
	pxor	xmm5,xmm5
	lea	edi,[64+edi]
	movaps	xmm2,xmm6
	pxor	xmm6,xmm6
	sub	eax,80
	jmp	NEAR L$108cbc_dec_tail_collected
align	16
L$104cbc_dec_one:
	movups	xmm0,[edx]
	movups	xmm1,[16+edx]
	lea	edx,[32+edx]
	xorps	xmm2,xmm0
L$109dec1_loop_20:
db	102,15,56,222,209
	dec	ecx
	movups	xmm1,[edx]
	lea	edx,[16+edx]
	jnz	NEAR L$109dec1_loop_20
db	102,15,56,223,209
	xorps	xmm2,xmm7
	movaps	xmm7,xmm6
	sub	eax,16
	jmp	NEAR L$108cbc_dec_tail_collected
align	16
L$105cbc_dec_two:
	call	__aesni_decrypt2
	xorps	xmm2,xmm7
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	movaps	xmm2,xmm3
	pxor	xmm3,xmm3
	lea	edi,[16+edi]
	movaps	xmm7,xmm5
	sub	eax,32
	jmp	NEAR L$108cbc_dec_tail_collected
align	16
L$106cbc_dec_three:
	call	__aesni_decrypt3
	xorps	xmm2,xmm7
	xorps	xmm3,xmm6
	xorps	xmm4,xmm5
	movups	[edi],xmm2
	movaps	xmm2,xmm4
	pxor	xmm4,xmm4
	movups	[16+edi],xmm3
	pxor	xmm3,xmm3
	lea	edi,[32+edi]
	movups	xmm7,[32+esi]
	sub	eax,48
	jmp	NEAR L$108cbc_dec_tail_collected
align	16
L$107cbc_dec_four:
	call	__aesni_decrypt4
	movups	xmm1,[16+esi]
	movups	xmm0,[32+esi]
	xorps	xmm2,xmm7
	movups	xmm7,[48+esi]
	xorps	xmm3,xmm6
	movups	[edi],xmm2
	xorps	xmm4,xmm1
	movups	[16+edi],xmm3
	pxor	xmm3,xmm3
	xorps	xmm5,xmm0
	movups	[32+edi],xmm4
	pxor	xmm4,xmm4
	lea	edi,[48+edi]
	movaps	xmm2,xmm5
	pxor	xmm5,xmm5
	sub	eax,64
	jmp	NEAR L$108cbc_dec_tail_collected
align	16
L$103cbc_dec_clear_tail_collected:
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
L$108cbc_dec_tail_collected:
	and	eax,15
	jnz	NEAR L$110cbc_dec_tail_partial
	movups	[edi],xmm2
	pxor	xmm0,xmm0
	jmp	NEAR L$099cbc_ret
align	16
L$110cbc_dec_tail_partial:
	movaps	[esp],xmm2
	pxor	xmm0,xmm0
	mov	ecx,16
	mov	esi,esp
	sub	ecx,eax
dd	2767451785
	movdqa	[esp],xmm2
L$099cbc_ret:
	mov	esp,DWORD [16+esp]
	mov	ebp,DWORD [36+esp]
	pxor	xmm2,xmm2
	pxor	xmm1,xmm1
	movups	[ebp],xmm7
	pxor	xmm7,xmm7
L$094cbc_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	16
__aesni_set_encrypt_key:
	push	ebp
	push	ebx
	test	eax,eax
	jz	NEAR L$111bad_pointer
	test	edx,edx
	jz	NEAR L$111bad_pointer
	call	L$112pic
L$112pic:
	pop	ebx
	lea	ebx,[(L$key_const-L$112pic)+ebx]
	lea	ebp,[_OPENSSL_ia32cap_P]
	movups	xmm0,[eax]
	xorps	xmm4,xmm4
	mov	ebp,DWORD [4+ebp]
	lea	edx,[16+edx]
	and	ebp,268437504
	cmp	ecx,256
	je	NEAR L$11314rounds
	cmp	ecx,192
	je	NEAR L$11412rounds
	cmp	ecx,128
	jne	NEAR L$115bad_keybits
align	16
L$11610rounds:
	cmp	ebp,268435456
	je	NEAR L$11710rounds_alt
	mov	ecx,9
	movups	[edx-16],xmm0
db	102,15,58,223,200,1
	call	L$118key_128_cold
db	102,15,58,223,200,2
	call	L$119key_128
db	102,15,58,223,200,4
	call	L$119key_128
db	102,15,58,223,200,8
	call	L$119key_128
db	102,15,58,223,200,16
	call	L$119key_128
db	102,15,58,223,200,32
	call	L$119key_128
db	102,15,58,223,200,64
	call	L$119key_128
db	102,15,58,223,200,128
	call	L$119key_128
db	102,15,58,223,200,27
	call	L$119key_128
db	102,15,58,223,200,54
	call	L$119key_128
	movups	[edx],xmm0
	mov	DWORD [80+edx],ecx
	jmp	NEAR L$120good_key
align	16
L$119key_128:
	movups	[edx],xmm0
	lea	edx,[16+edx]
L$118key_128_cold:
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	ret
align	16
L$11710rounds_alt:
	movdqa	xmm5,[ebx]
	mov	ecx,8
	movdqa	xmm4,[32+ebx]
	movdqa	xmm2,xmm0
	movdqu	[edx-16],xmm0
L$121loop_key128:
db	102,15,56,0,197
db	102,15,56,221,196
	pslld	xmm4,1
	lea	edx,[16+edx]
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	[edx-16],xmm0
	movdqa	xmm2,xmm0
	dec	ecx
	jnz	NEAR L$121loop_key128
	movdqa	xmm4,[48+ebx]
db	102,15,56,0,197
db	102,15,56,221,196
	pslld	xmm4,1
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	[edx],xmm0
	movdqa	xmm2,xmm0
db	102,15,56,0,197
db	102,15,56,221,196
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	[16+edx],xmm0
	mov	ecx,9
	mov	DWORD [96+edx],ecx
	jmp	NEAR L$120good_key
align	16
L$11412rounds:
	movq	xmm2,[16+eax]
	cmp	ebp,268435456
	je	NEAR L$12212rounds_alt
	mov	ecx,11
	movups	[edx-16],xmm0
db	102,15,58,223,202,1
	call	L$123key_192a_cold
db	102,15,58,223,202,2
	call	L$124key_192b
db	102,15,58,223,202,4
	call	L$125key_192a
db	102,15,58,223,202,8
	call	L$124key_192b
db	102,15,58,223,202,16
	call	L$125key_192a
db	102,15,58,223,202,32
	call	L$124key_192b
db	102,15,58,223,202,64
	call	L$125key_192a
db	102,15,58,223,202,128
	call	L$124key_192b
	movups	[edx],xmm0
	mov	DWORD [48+edx],ecx
	jmp	NEAR L$120good_key
align	16
L$125key_192a:
	movups	[edx],xmm0
	lea	edx,[16+edx]
align	16
L$123key_192a_cold:
	movaps	xmm5,xmm2
L$126key_192b_warm:
	shufps	xmm4,xmm0,16
	movdqa	xmm3,xmm2
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	pslldq	xmm3,4
	xorps	xmm0,xmm4
	pshufd	xmm1,xmm1,85
	pxor	xmm2,xmm3
	pxor	xmm0,xmm1
	pshufd	xmm3,xmm0,255
	pxor	xmm2,xmm3
	ret
align	16
L$124key_192b:
	movaps	xmm3,xmm0
	shufps	xmm5,xmm0,68
	movups	[edx],xmm5
	shufps	xmm3,xmm2,78
	movups	[16+edx],xmm3
	lea	edx,[32+edx]
	jmp	NEAR L$126key_192b_warm
align	16
L$12212rounds_alt:
	movdqa	xmm5,[16+ebx]
	movdqa	xmm4,[32+ebx]
	mov	ecx,8
	movdqu	[edx-16],xmm0
L$127loop_key192:
	movq	[edx],xmm2
	movdqa	xmm1,xmm2
db	102,15,56,0,213
db	102,15,56,221,212
	pslld	xmm4,1
	lea	edx,[24+edx]
	movdqa	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm0,xmm3
	pshufd	xmm3,xmm0,255
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1
	pxor	xmm0,xmm2
	pxor	xmm2,xmm3
	movdqu	[edx-16],xmm0
	dec	ecx
	jnz	NEAR L$127loop_key192
	mov	ecx,11
	mov	DWORD [32+edx],ecx
	jmp	NEAR L$120good_key
align	16
L$11314rounds:
	movups	xmm2,[16+eax]
	lea	edx,[16+edx]
	cmp	ebp,268435456
	je	NEAR L$12814rounds_alt
	mov	ecx,13
	movups	[edx-32],xmm0
	movups	[edx-16],xmm2
db	102,15,58,223,202,1
	call	L$129key_256a_cold
db	102,15,58,223,200,1
	call	L$130key_256b
db	102,15,58,223,202,2
	call	L$131key_256a
db	102,15,58,223,200,2
	call	L$130key_256b
db	102,15,58,223,202,4
	call	L$131key_256a
db	102,15,58,223,200,4
	call	L$130key_256b
db	102,15,58,223,202,8
	call	L$131key_256a
db	102,15,58,223,200,8
	call	L$130key_256b
db	102,15,58,223,202,16
	call	L$131key_256a
db	102,15,58,223,200,16
	call	L$130key_256b
db	102,15,58,223,202,32
	call	L$131key_256a
db	102,15,58,223,200,32
	call	L$130key_256b
db	102,15,58,223,202,64
	call	L$131key_256a
	movups	[edx],xmm0
	mov	DWORD [16+edx],ecx
	xor	eax,eax
	jmp	NEAR L$120good_key
align	16
L$131key_256a:
	movups	[edx],xmm2
	lea	edx,[16+edx]
L$129key_256a_cold:
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	ret
align	16
L$130key_256b:
	movups	[edx],xmm0
	lea	edx,[16+edx]
	shufps	xmm4,xmm2,16
	xorps	xmm2,xmm4
	shufps	xmm4,xmm2,140
	xorps	xmm2,xmm4
	shufps	xmm1,xmm1,170
	xorps	xmm2,xmm1
	ret
align	16
L$12814rounds_alt:
	movdqa	xmm5,[ebx]
	movdqa	xmm4,[32+ebx]
	mov	ecx,7
	movdqu	[edx-32],xmm0
	movdqa	xmm1,xmm2
	movdqu	[edx-16],xmm2
L$132loop_key256:
db	102,15,56,0,213
db	102,15,56,221,212
	movdqa	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm0,xmm3
	pslld	xmm4,1
	pxor	xmm0,xmm2
	movdqu	[edx],xmm0
	dec	ecx
	jz	NEAR L$133done_key256
	pshufd	xmm2,xmm0,255
	pxor	xmm3,xmm3
db	102,15,56,221,211
	movdqa	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm1,xmm3
	pxor	xmm2,xmm1
	movdqu	[16+edx],xmm2
	lea	edx,[32+edx]
	movdqa	xmm1,xmm2
	jmp	NEAR L$132loop_key256
L$133done_key256:
	mov	ecx,13
	mov	DWORD [16+edx],ecx
L$120good_key:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	xor	eax,eax
	pop	ebx
	pop	ebp
	ret
align	4
L$111bad_pointer:
	mov	eax,-1
	pop	ebx
	pop	ebp
	ret
align	4
L$115bad_keybits:
	pxor	xmm0,xmm0
	mov	eax,-2
	pop	ebx
	pop	ebp
	ret
global	_aesni_set_encrypt_key
align	16
_aesni_set_encrypt_key:
L$_aesni_set_encrypt_key_begin:
	mov	eax,DWORD [4+esp]
	mov	ecx,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
	call	__aesni_set_encrypt_key
	ret
global	_aesni_set_decrypt_key
align	16
_aesni_set_decrypt_key:
L$_aesni_set_decrypt_key_begin:
	mov	eax,DWORD [4+esp]
	mov	ecx,DWORD [8+esp]
	mov	edx,DWORD [12+esp]
	call	__aesni_set_encrypt_key
	mov	edx,DWORD [12+esp]
	shl	ecx,4
	test	eax,eax
	jnz	NEAR L$134dec_key_ret
	lea	eax,[16+ecx*1+edx]
	movups	xmm0,[edx]
	movups	xmm1,[eax]
	movups	[eax],xmm0
	movups	[edx],xmm1
	lea	edx,[16+edx]
	lea	eax,[eax-16]
L$135dec_key_inverse:
	movups	xmm0,[edx]
	movups	xmm1,[eax]
db	102,15,56,219,192
db	102,15,56,219,201
	lea	edx,[16+edx]
	lea	eax,[eax-16]
	movups	[16+eax],xmm0
	movups	[edx-16],xmm1
	cmp	eax,edx
	ja	NEAR L$135dec_key_inverse
	movups	xmm0,[edx]
db	102,15,56,219,192
	movups	[edx],xmm0
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	xor	eax,eax
L$134dec_key_ret:
	ret
align	64
L$key_const:
dd	202313229,202313229,202313229,202313229
dd	67569157,67569157,67569157,67569157
dd	1,1,1,1
dd	27,27,27,27
db	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69
db	83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83
db	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
db	115,108,46,111,114,103,62,0
segment	.bss
common	_OPENSSL_ia32cap_P 40
