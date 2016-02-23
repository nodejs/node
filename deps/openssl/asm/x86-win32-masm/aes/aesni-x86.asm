TITLE	../openssl/crypto/aes/asm/aesni-x86.asm
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
_aesni_encrypt	PROC PUBLIC
$L_aesni_encrypt_begin::
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 12[esp]
	movups	xmm2,XMMWORD PTR [eax]
	mov	ecx,DWORD PTR 240[edx]
	mov	eax,DWORD PTR 8[esp]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L000enc1_loop_1:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L000enc1_loop_1
DB	102,15,56,221,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	XMMWORD PTR [eax],xmm2
	pxor	xmm2,xmm2
	ret
_aesni_encrypt ENDP
ALIGN	16
_aesni_decrypt	PROC PUBLIC
$L_aesni_decrypt_begin::
	mov	eax,DWORD PTR 4[esp]
	mov	edx,DWORD PTR 12[esp]
	movups	xmm2,XMMWORD PTR [eax]
	mov	ecx,DWORD PTR 240[edx]
	mov	eax,DWORD PTR 8[esp]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L001dec1_loop_2:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L001dec1_loop_2
DB	102,15,56,223,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	XMMWORD PTR [eax],xmm2
	pxor	xmm2,xmm2
	ret
_aesni_decrypt ENDP
ALIGN	16
__aesni_encrypt2	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
	add	ecx,16
$L002enc2_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L002enc2_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,221,208
DB	102,15,56,221,216
	ret
__aesni_encrypt2 ENDP
ALIGN	16
__aesni_decrypt2	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
	add	ecx,16
$L003dec2_loop:
DB	102,15,56,222,209
DB	102,15,56,222,217
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,222,208
DB	102,15,56,222,216
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L003dec2_loop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,223,208
DB	102,15,56,223,216
	ret
__aesni_decrypt2 ENDP
ALIGN	16
__aesni_encrypt3	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
	add	ecx,16
$L004enc3_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L004enc3_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
	ret
__aesni_encrypt3 ENDP
ALIGN	16
__aesni_decrypt3	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
	add	ecx,16
$L005dec3_loop:
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L005dec3_loop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
	ret
__aesni_decrypt3 ENDP
ALIGN	16
__aesni_encrypt4	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	shl	ecx,4
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
DB	15,31,64,0
	add	ecx,16
$L006enc4_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L006enc4_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
DB	102,15,56,221,232
	ret
__aesni_encrypt4 ENDP
ALIGN	16
__aesni_decrypt4	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	shl	ecx,4
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	movups	xmm0,XMMWORD PTR 32[edx]
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
DB	15,31,64,0
	add	ecx,16
$L007dec4_loop:
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L007dec4_loop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
DB	102,15,56,223,232
	ret
__aesni_decrypt4 ENDP
ALIGN	16
__aesni_encrypt6	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
DB	102,15,56,220,209
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
DB	102,15,56,220,217
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
DB	102,15,56,220,225
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR [ecx*1+edx]
	add	ecx,16
	jmp	$L008_aesni_encrypt6_inner
ALIGN	16
$L009enc6_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
$L008_aesni_encrypt6_inner:
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
$L_aesni_encrypt6_enter::
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L009enc6_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
DB	102,15,56,221,232
DB	102,15,56,221,240
DB	102,15,56,221,248
	ret
__aesni_encrypt6 ENDP
ALIGN	16
__aesni_decrypt6	PROC PRIVATE
	movups	xmm0,XMMWORD PTR [edx]
	shl	ecx,4
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
DB	102,15,56,222,209
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
DB	102,15,56,222,217
	lea	edx,DWORD PTR 32[ecx*1+edx]
	neg	ecx
DB	102,15,56,222,225
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR [ecx*1+edx]
	add	ecx,16
	jmp	$L010_aesni_decrypt6_inner
ALIGN	16
$L011dec6_loop:
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
$L010_aesni_decrypt6_inner:
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
$L_aesni_decrypt6_enter::
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L011dec6_loop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
DB	102,15,56,223,232
DB	102,15,56,223,240
DB	102,15,56,223,248
	ret
__aesni_decrypt6 ENDP
ALIGN	16
_aesni_ecb_encrypt	PROC PUBLIC
$L_aesni_ecb_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebx,DWORD PTR 36[esp]
	and	eax,-16
	jz	$L012ecb_ret
	mov	ecx,DWORD PTR 240[edx]
	test	ebx,ebx
	jz	$L013ecb_decrypt
	mov	ebp,edx
	mov	ebx,ecx
	cmp	eax,96
	jb	$L014ecb_enc_tail
	movdqu	xmm2,XMMWORD PTR [esi]
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movdqu	xmm4,XMMWORD PTR 32[esi]
	movdqu	xmm5,XMMWORD PTR 48[esi]
	movdqu	xmm6,XMMWORD PTR 64[esi]
	movdqu	xmm7,XMMWORD PTR 80[esi]
	lea	esi,DWORD PTR 96[esi]
	sub	eax,96
	jmp	$L015ecb_enc_loop6_enter
ALIGN	16
$L016ecb_enc_loop6:
	movups	XMMWORD PTR [edi],xmm2
	movdqu	xmm2,XMMWORD PTR [esi]
	movups	XMMWORD PTR 16[edi],xmm3
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movups	XMMWORD PTR 32[edi],xmm4
	movdqu	xmm4,XMMWORD PTR 32[esi]
	movups	XMMWORD PTR 48[edi],xmm5
	movdqu	xmm5,XMMWORD PTR 48[esi]
	movups	XMMWORD PTR 64[edi],xmm6
	movdqu	xmm6,XMMWORD PTR 64[esi]
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	movdqu	xmm7,XMMWORD PTR 80[esi]
	lea	esi,DWORD PTR 96[esi]
$L015ecb_enc_loop6_enter:
	call	__aesni_encrypt6
	mov	edx,ebp
	mov	ecx,ebx
	sub	eax,96
	jnc	$L016ecb_enc_loop6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	add	eax,96
	jz	$L012ecb_ret
$L014ecb_enc_tail:
	movups	xmm2,XMMWORD PTR [esi]
	cmp	eax,32
	jb	$L017ecb_enc_one
	movups	xmm3,XMMWORD PTR 16[esi]
	je	$L018ecb_enc_two
	movups	xmm4,XMMWORD PTR 32[esi]
	cmp	eax,64
	jb	$L019ecb_enc_three
	movups	xmm5,XMMWORD PTR 48[esi]
	je	$L020ecb_enc_four
	movups	xmm6,XMMWORD PTR 64[esi]
	xorps	xmm7,xmm7
	call	__aesni_encrypt6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	jmp	$L012ecb_ret
ALIGN	16
$L017ecb_enc_one:
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L021enc1_loop_3:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L021enc1_loop_3
DB	102,15,56,221,209
	movups	XMMWORD PTR [edi],xmm2
	jmp	$L012ecb_ret
ALIGN	16
$L018ecb_enc_two:
	call	__aesni_encrypt2
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	jmp	$L012ecb_ret
ALIGN	16
$L019ecb_enc_three:
	call	__aesni_encrypt3
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	jmp	$L012ecb_ret
ALIGN	16
$L020ecb_enc_four:
	call	__aesni_encrypt4
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	jmp	$L012ecb_ret
ALIGN	16
$L013ecb_decrypt:
	mov	ebp,edx
	mov	ebx,ecx
	cmp	eax,96
	jb	$L022ecb_dec_tail
	movdqu	xmm2,XMMWORD PTR [esi]
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movdqu	xmm4,XMMWORD PTR 32[esi]
	movdqu	xmm5,XMMWORD PTR 48[esi]
	movdqu	xmm6,XMMWORD PTR 64[esi]
	movdqu	xmm7,XMMWORD PTR 80[esi]
	lea	esi,DWORD PTR 96[esi]
	sub	eax,96
	jmp	$L023ecb_dec_loop6_enter
ALIGN	16
$L024ecb_dec_loop6:
	movups	XMMWORD PTR [edi],xmm2
	movdqu	xmm2,XMMWORD PTR [esi]
	movups	XMMWORD PTR 16[edi],xmm3
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movups	XMMWORD PTR 32[edi],xmm4
	movdqu	xmm4,XMMWORD PTR 32[esi]
	movups	XMMWORD PTR 48[edi],xmm5
	movdqu	xmm5,XMMWORD PTR 48[esi]
	movups	XMMWORD PTR 64[edi],xmm6
	movdqu	xmm6,XMMWORD PTR 64[esi]
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	movdqu	xmm7,XMMWORD PTR 80[esi]
	lea	esi,DWORD PTR 96[esi]
$L023ecb_dec_loop6_enter:
	call	__aesni_decrypt6
	mov	edx,ebp
	mov	ecx,ebx
	sub	eax,96
	jnc	$L024ecb_dec_loop6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	add	eax,96
	jz	$L012ecb_ret
$L022ecb_dec_tail:
	movups	xmm2,XMMWORD PTR [esi]
	cmp	eax,32
	jb	$L025ecb_dec_one
	movups	xmm3,XMMWORD PTR 16[esi]
	je	$L026ecb_dec_two
	movups	xmm4,XMMWORD PTR 32[esi]
	cmp	eax,64
	jb	$L027ecb_dec_three
	movups	xmm5,XMMWORD PTR 48[esi]
	je	$L028ecb_dec_four
	movups	xmm6,XMMWORD PTR 64[esi]
	xorps	xmm7,xmm7
	call	__aesni_decrypt6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	jmp	$L012ecb_ret
ALIGN	16
$L025ecb_dec_one:
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L029dec1_loop_4:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L029dec1_loop_4
DB	102,15,56,223,209
	movups	XMMWORD PTR [edi],xmm2
	jmp	$L012ecb_ret
ALIGN	16
$L026ecb_dec_two:
	call	__aesni_decrypt2
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	jmp	$L012ecb_ret
ALIGN	16
$L027ecb_dec_three:
	call	__aesni_decrypt3
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	jmp	$L012ecb_ret
ALIGN	16
$L028ecb_dec_four:
	call	__aesni_decrypt4
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
$L012ecb_ret:
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
_aesni_ecb_encrypt ENDP
ALIGN	16
_aesni_ccm64_encrypt_blocks	PROC PUBLIC
$L_aesni_ccm64_encrypt_blocks_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebx,DWORD PTR 36[esp]
	mov	ecx,DWORD PTR 40[esp]
	mov	ebp,esp
	sub	esp,60
	and	esp,-16
	mov	DWORD PTR 48[esp],ebp
	movdqu	xmm7,XMMWORD PTR [ebx]
	movdqu	xmm3,XMMWORD PTR [ecx]
	mov	ecx,DWORD PTR 240[edx]
	mov	DWORD PTR [esp],202182159
	mov	DWORD PTR 4[esp],134810123
	mov	DWORD PTR 8[esp],67438087
	mov	DWORD PTR 12[esp],66051
	mov	ebx,1
	xor	ebp,ebp
	mov	DWORD PTR 16[esp],ebx
	mov	DWORD PTR 20[esp],ebp
	mov	DWORD PTR 24[esp],ebp
	mov	DWORD PTR 28[esp],ebp
	shl	ecx,4
	mov	ebx,16
	lea	ebp,DWORD PTR [edx]
	movdqa	xmm5,XMMWORD PTR [esp]
	movdqa	xmm2,xmm7
	lea	edx,DWORD PTR 32[ecx*1+edx]
	sub	ebx,ecx
DB	102,15,56,0,253
$L030ccm64_enc_outer:
	movups	xmm0,XMMWORD PTR [ebp]
	mov	ecx,ebx
	movups	xmm6,XMMWORD PTR [esi]
	xorps	xmm2,xmm0
	movups	xmm1,XMMWORD PTR 16[ebp]
	xorps	xmm0,xmm6
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR 32[ebp]
$L031ccm64_enc2_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L031ccm64_enc2_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
	paddq	xmm7,XMMWORD PTR 16[esp]
	dec	eax
DB	102,15,56,221,208
DB	102,15,56,221,216
	lea	esi,DWORD PTR 16[esi]
	xorps	xmm6,xmm2
	movdqa	xmm2,xmm7
	movups	XMMWORD PTR [edi],xmm6
DB	102,15,56,0,213
	lea	edi,DWORD PTR 16[edi]
	jnz	$L030ccm64_enc_outer
	mov	esp,DWORD PTR 48[esp]
	mov	edi,DWORD PTR 40[esp]
	movups	XMMWORD PTR [edi],xmm3
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
_aesni_ccm64_encrypt_blocks ENDP
ALIGN	16
_aesni_ccm64_decrypt_blocks	PROC PUBLIC
$L_aesni_ccm64_decrypt_blocks_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebx,DWORD PTR 36[esp]
	mov	ecx,DWORD PTR 40[esp]
	mov	ebp,esp
	sub	esp,60
	and	esp,-16
	mov	DWORD PTR 48[esp],ebp
	movdqu	xmm7,XMMWORD PTR [ebx]
	movdqu	xmm3,XMMWORD PTR [ecx]
	mov	ecx,DWORD PTR 240[edx]
	mov	DWORD PTR [esp],202182159
	mov	DWORD PTR 4[esp],134810123
	mov	DWORD PTR 8[esp],67438087
	mov	DWORD PTR 12[esp],66051
	mov	ebx,1
	xor	ebp,ebp
	mov	DWORD PTR 16[esp],ebx
	mov	DWORD PTR 20[esp],ebp
	mov	DWORD PTR 24[esp],ebp
	mov	DWORD PTR 28[esp],ebp
	movdqa	xmm5,XMMWORD PTR [esp]
	movdqa	xmm2,xmm7
	mov	ebp,edx
	mov	ebx,ecx
DB	102,15,56,0,253
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L032enc1_loop_5:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L032enc1_loop_5
DB	102,15,56,221,209
	shl	ebx,4
	mov	ecx,16
	movups	xmm6,XMMWORD PTR [esi]
	paddq	xmm7,XMMWORD PTR 16[esp]
	lea	esi,QWORD PTR 16[esi]
	sub	ecx,ebx
	lea	edx,DWORD PTR 32[ebx*1+ebp]
	mov	ebx,ecx
	jmp	$L033ccm64_dec_outer
ALIGN	16
$L033ccm64_dec_outer:
	xorps	xmm6,xmm2
	movdqa	xmm2,xmm7
	movups	XMMWORD PTR [edi],xmm6
	lea	edi,DWORD PTR 16[edi]
DB	102,15,56,0,213
	sub	eax,1
	jz	$L034ccm64_dec_break
	movups	xmm0,XMMWORD PTR [ebp]
	mov	ecx,ebx
	movups	xmm1,XMMWORD PTR 16[ebp]
	xorps	xmm6,xmm0
	xorps	xmm2,xmm0
	xorps	xmm3,xmm6
	movups	xmm0,XMMWORD PTR 32[ebp]
$L035ccm64_dec2_loop:
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR [ecx*1+edx]
	add	ecx,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR [ecx*1+edx-16]
	jnz	$L035ccm64_dec2_loop
	movups	xmm6,XMMWORD PTR [esi]
	paddq	xmm7,XMMWORD PTR 16[esp]
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,221,208
DB	102,15,56,221,216
	lea	esi,QWORD PTR 16[esi]
	jmp	$L033ccm64_dec_outer
ALIGN	16
$L034ccm64_dec_break:
	mov	ecx,DWORD PTR 240[ebp]
	mov	edx,ebp
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm6,xmm0
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm3,xmm6
$L036enc1_loop_6:
DB	102,15,56,220,217
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L036enc1_loop_6
DB	102,15,56,221,217
	mov	esp,DWORD PTR 48[esp]
	mov	edi,DWORD PTR 40[esp]
	movups	XMMWORD PTR [edi],xmm3
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
_aesni_ccm64_decrypt_blocks ENDP
ALIGN	16
_aesni_ctr32_encrypt_blocks	PROC PUBLIC
$L_aesni_ctr32_encrypt_blocks_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebx,DWORD PTR 36[esp]
	mov	ebp,esp
	sub	esp,88
	and	esp,-16
	mov	DWORD PTR 80[esp],ebp
	cmp	eax,1
	je	$L037ctr32_one_shortcut
	movdqu	xmm7,XMMWORD PTR [ebx]
	mov	DWORD PTR [esp],202182159
	mov	DWORD PTR 4[esp],134810123
	mov	DWORD PTR 8[esp],67438087
	mov	DWORD PTR 12[esp],66051
	mov	ecx,6
	xor	ebp,ebp
	mov	DWORD PTR 16[esp],ecx
	mov	DWORD PTR 20[esp],ecx
	mov	DWORD PTR 24[esp],ecx
	mov	DWORD PTR 28[esp],ebp
DB	102,15,58,22,251,3
DB	102,15,58,34,253,3
	mov	ecx,DWORD PTR 240[edx]
	bswap	ebx
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movdqa	xmm2,XMMWORD PTR [esp]
DB	102,15,58,34,195,0
	lea	ebp,DWORD PTR 3[ebx]
DB	102,15,58,34,205,0
	inc	ebx
DB	102,15,58,34,195,1
	inc	ebp
DB	102,15,58,34,205,1
	inc	ebx
DB	102,15,58,34,195,2
	inc	ebp
DB	102,15,58,34,205,2
	movdqa	XMMWORD PTR 48[esp],xmm0
DB	102,15,56,0,194
	movdqu	xmm6,XMMWORD PTR [edx]
	movdqa	XMMWORD PTR 64[esp],xmm1
DB	102,15,56,0,202
	pshufd	xmm2,xmm0,192
	pshufd	xmm3,xmm0,128
	cmp	eax,6
	jb	$L038ctr32_tail
	pxor	xmm7,xmm6
	shl	ecx,4
	mov	ebx,16
	movdqa	XMMWORD PTR 32[esp],xmm7
	mov	ebp,edx
	sub	ebx,ecx
	lea	edx,DWORD PTR 32[ecx*1+edx]
	sub	eax,6
	jmp	$L039ctr32_loop6
ALIGN	16
$L039ctr32_loop6:
	pshufd	xmm4,xmm0,64
	movdqa	xmm0,XMMWORD PTR 32[esp]
	pshufd	xmm5,xmm1,192
	pxor	xmm2,xmm0
	pshufd	xmm6,xmm1,128
	pxor	xmm3,xmm0
	pshufd	xmm7,xmm1,64
	movups	xmm1,XMMWORD PTR 16[ebp]
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
DB	102,15,56,220,209
	pxor	xmm6,xmm0
	pxor	xmm7,xmm0
DB	102,15,56,220,217
	movups	xmm0,XMMWORD PTR 32[ebp]
	mov	ecx,ebx
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
	call	$L_aesni_encrypt6_enter
	movups	xmm1,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm1
	movups	xmm1,XMMWORD PTR 32[esi]
	xorps	xmm3,xmm0
	movups	XMMWORD PTR [edi],xmm2
	movdqa	xmm0,XMMWORD PTR 16[esp]
	xorps	xmm4,xmm1
	movdqa	xmm1,XMMWORD PTR 64[esp]
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	paddd	xmm1,xmm0
	paddd	xmm0,XMMWORD PTR 48[esp]
	movdqa	xmm2,XMMWORD PTR [esp]
	movups	xmm3,XMMWORD PTR 48[esi]
	movups	xmm4,XMMWORD PTR 64[esi]
	xorps	xmm5,xmm3
	movups	xmm3,XMMWORD PTR 80[esi]
	lea	esi,DWORD PTR 96[esi]
	movdqa	XMMWORD PTR 48[esp],xmm0
DB	102,15,56,0,194
	xorps	xmm6,xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	xorps	xmm7,xmm3
	movdqa	XMMWORD PTR 64[esp],xmm1
DB	102,15,56,0,202
	movups	XMMWORD PTR 64[edi],xmm6
	pshufd	xmm2,xmm0,192
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	pshufd	xmm3,xmm0,128
	sub	eax,6
	jnc	$L039ctr32_loop6
	add	eax,6
	jz	$L040ctr32_ret
	movdqu	xmm7,XMMWORD PTR [ebp]
	mov	edx,ebp
	pxor	xmm7,XMMWORD PTR 32[esp]
	mov	ecx,DWORD PTR 240[ebp]
$L038ctr32_tail:
	por	xmm2,xmm7
	cmp	eax,2
	jb	$L041ctr32_one
	pshufd	xmm4,xmm0,64
	por	xmm3,xmm7
	je	$L042ctr32_two
	pshufd	xmm5,xmm1,192
	por	xmm4,xmm7
	cmp	eax,4
	jb	$L043ctr32_three
	pshufd	xmm6,xmm1,128
	por	xmm5,xmm7
	je	$L044ctr32_four
	por	xmm6,xmm7
	call	__aesni_encrypt6
	movups	xmm1,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm1
	movups	xmm1,XMMWORD PTR 32[esi]
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR 48[esi]
	xorps	xmm4,xmm1
	movups	xmm1,XMMWORD PTR 64[esi]
	xorps	xmm5,xmm0
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm6,xmm1
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	jmp	$L040ctr32_ret
ALIGN	16
$L037ctr32_one_shortcut:
	movups	xmm2,XMMWORD PTR [ebx]
	mov	ecx,DWORD PTR 240[edx]
$L041ctr32_one:
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L045enc1_loop_7:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L045enc1_loop_7
DB	102,15,56,221,209
	movups	xmm6,XMMWORD PTR [esi]
	xorps	xmm6,xmm2
	movups	XMMWORD PTR [edi],xmm6
	jmp	$L040ctr32_ret
ALIGN	16
$L042ctr32_two:
	call	__aesni_encrypt2
	movups	xmm5,XMMWORD PTR [esi]
	movups	xmm6,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	jmp	$L040ctr32_ret
ALIGN	16
$L043ctr32_three:
	call	__aesni_encrypt3
	movups	xmm5,XMMWORD PTR [esi]
	movups	xmm6,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm5
	movups	xmm7,XMMWORD PTR 32[esi]
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm4,xmm7
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	jmp	$L040ctr32_ret
ALIGN	16
$L044ctr32_four:
	call	__aesni_encrypt4
	movups	xmm6,XMMWORD PTR [esi]
	movups	xmm7,XMMWORD PTR 16[esi]
	movups	xmm1,XMMWORD PTR 32[esi]
	xorps	xmm2,xmm6
	movups	xmm0,XMMWORD PTR 48[esi]
	xorps	xmm3,xmm7
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm4,xmm1
	movups	XMMWORD PTR 16[edi],xmm3
	xorps	xmm5,xmm0
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
$L040ctr32_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	movdqa	XMMWORD PTR 32[esp],xmm0
	pxor	xmm5,xmm5
	movdqa	XMMWORD PTR 48[esp],xmm0
	pxor	xmm6,xmm6
	movdqa	XMMWORD PTR 64[esp],xmm0
	pxor	xmm7,xmm7
	mov	esp,DWORD PTR 80[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_aesni_ctr32_encrypt_blocks ENDP
ALIGN	16
_aesni_xts_encrypt	PROC PUBLIC
$L_aesni_xts_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edx,DWORD PTR 36[esp]
	mov	esi,DWORD PTR 40[esp]
	mov	ecx,DWORD PTR 240[edx]
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L046enc1_loop_8:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L046enc1_loop_8
DB	102,15,56,221,209
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebp,esp
	sub	esp,120
	mov	ecx,DWORD PTR 240[edx]
	and	esp,-16
	mov	DWORD PTR 96[esp],135
	mov	DWORD PTR 100[esp],0
	mov	DWORD PTR 104[esp],1
	mov	DWORD PTR 108[esp],0
	mov	DWORD PTR 112[esp],eax
	mov	DWORD PTR 116[esp],ebp
	movdqa	xmm1,xmm2
	pxor	xmm0,xmm0
	movdqa	xmm3,XMMWORD PTR 96[esp]
	pcmpgtd	xmm0,xmm1
	and	eax,-16
	mov	ebp,edx
	mov	ebx,ecx
	sub	eax,96
	jc	$L047xts_enc_short
	shl	ecx,4
	mov	ebx,16
	sub	ebx,ecx
	lea	edx,DWORD PTR 32[ecx*1+edx]
	jmp	$L048xts_enc_loop6
ALIGN	16
$L048xts_enc_loop6:
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR [esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 16[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 32[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 48[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm7,xmm0,19
	movdqa	XMMWORD PTR 64[esp],xmm1
	paddq	xmm1,xmm1
	movups	xmm0,XMMWORD PTR [ebp]
	pand	xmm7,xmm3
	movups	xmm2,XMMWORD PTR [esi]
	pxor	xmm7,xmm1
	mov	ecx,ebx
	movdqu	xmm3,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm0
	movdqu	xmm4,XMMWORD PTR 32[esi]
	pxor	xmm3,xmm0
	movdqu	xmm5,XMMWORD PTR 48[esi]
	pxor	xmm4,xmm0
	movdqu	xmm6,XMMWORD PTR 64[esi]
	pxor	xmm5,xmm0
	movdqu	xmm1,XMMWORD PTR 80[esi]
	pxor	xmm6,xmm0
	lea	esi,DWORD PTR 96[esi]
	pxor	xmm2,XMMWORD PTR [esp]
	movdqa	XMMWORD PTR 80[esp],xmm7
	pxor	xmm7,xmm1
	movups	xmm1,XMMWORD PTR 16[ebp]
	pxor	xmm3,XMMWORD PTR 16[esp]
	pxor	xmm4,XMMWORD PTR 32[esp]
DB	102,15,56,220,209
	pxor	xmm5,XMMWORD PTR 48[esp]
	pxor	xmm6,XMMWORD PTR 64[esp]
DB	102,15,56,220,217
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR 32[ebp]
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
	call	$L_aesni_encrypt6_enter
	movdqa	xmm1,XMMWORD PTR 80[esp]
	pxor	xmm0,xmm0
	xorps	xmm2,XMMWORD PTR [esp]
	pcmpgtd	xmm0,xmm1
	xorps	xmm3,XMMWORD PTR 16[esp]
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm4,XMMWORD PTR 32[esp]
	movups	XMMWORD PTR 16[edi],xmm3
	xorps	xmm5,XMMWORD PTR 48[esp]
	movups	XMMWORD PTR 32[edi],xmm4
	xorps	xmm6,XMMWORD PTR 64[esp]
	movups	XMMWORD PTR 48[edi],xmm5
	xorps	xmm7,xmm1
	movups	XMMWORD PTR 64[edi],xmm6
	pshufd	xmm2,xmm0,19
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	movdqa	xmm3,XMMWORD PTR 96[esp]
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	sub	eax,96
	jnc	$L048xts_enc_loop6
	mov	ecx,DWORD PTR 240[ebp]
	mov	edx,ebp
	mov	ebx,ecx
$L047xts_enc_short:
	add	eax,96
	jz	$L049xts_enc_done6x
	movdqa	xmm5,xmm1
	cmp	eax,32
	jb	$L050xts_enc_one
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	je	$L051xts_enc_two
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	cmp	eax,64
	jb	$L052xts_enc_three
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm7,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	movdqa	XMMWORD PTR [esp],xmm5
	movdqa	XMMWORD PTR 16[esp],xmm6
	je	$L053xts_enc_four
	movdqa	XMMWORD PTR 32[esp],xmm7
	pshufd	xmm7,xmm0,19
	movdqa	XMMWORD PTR 48[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm7,xmm3
	pxor	xmm7,xmm1
	movdqu	xmm2,XMMWORD PTR [esi]
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movdqu	xmm4,XMMWORD PTR 32[esi]
	pxor	xmm2,XMMWORD PTR [esp]
	movdqu	xmm5,XMMWORD PTR 48[esi]
	pxor	xmm3,XMMWORD PTR 16[esp]
	movdqu	xmm6,XMMWORD PTR 64[esi]
	pxor	xmm4,XMMWORD PTR 32[esp]
	lea	esi,DWORD PTR 80[esi]
	pxor	xmm5,XMMWORD PTR 48[esp]
	movdqa	XMMWORD PTR 64[esp],xmm7
	pxor	xmm6,xmm7
	call	__aesni_encrypt6
	movaps	xmm1,XMMWORD PTR 64[esp]
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,XMMWORD PTR 32[esp]
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm5,XMMWORD PTR 48[esp]
	movups	XMMWORD PTR 16[edi],xmm3
	xorps	xmm6,xmm1
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	lea	edi,DWORD PTR 80[edi]
	jmp	$L054xts_enc_done
ALIGN	16
$L050xts_enc_one:
	movups	xmm2,XMMWORD PTR [esi]
	lea	esi,DWORD PTR 16[esi]
	xorps	xmm2,xmm5
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L055enc1_loop_9:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L055enc1_loop_9
DB	102,15,56,221,209
	xorps	xmm2,xmm5
	movups	XMMWORD PTR [edi],xmm2
	lea	edi,DWORD PTR 16[edi]
	movdqa	xmm1,xmm5
	jmp	$L054xts_enc_done
ALIGN	16
$L051xts_enc_two:
	movaps	xmm6,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	lea	esi,DWORD PTR 32[esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	call	__aesni_encrypt2
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	lea	edi,DWORD PTR 32[edi]
	movdqa	xmm1,xmm6
	jmp	$L054xts_enc_done
ALIGN	16
$L052xts_enc_three:
	movaps	xmm7,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	movups	xmm4,XMMWORD PTR 32[esi]
	lea	esi,DWORD PTR 48[esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	call	__aesni_encrypt3
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	lea	edi,DWORD PTR 48[edi]
	movdqa	xmm1,xmm7
	jmp	$L054xts_enc_done
ALIGN	16
$L053xts_enc_four:
	movaps	xmm6,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	movups	xmm4,XMMWORD PTR 32[esi]
	xorps	xmm2,XMMWORD PTR [esp]
	movups	xmm5,XMMWORD PTR 48[esi]
	lea	esi,DWORD PTR 64[esi]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,xmm7
	xorps	xmm5,xmm6
	call	__aesni_encrypt4
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,xmm7
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm5,xmm6
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	lea	edi,DWORD PTR 64[edi]
	movdqa	xmm1,xmm6
	jmp	$L054xts_enc_done
ALIGN	16
$L049xts_enc_done6x:
	mov	eax,DWORD PTR 112[esp]
	and	eax,15
	jz	$L056xts_enc_ret
	movdqa	xmm5,xmm1
	mov	DWORD PTR 112[esp],eax
	jmp	$L057xts_enc_steal
ALIGN	16
$L054xts_enc_done:
	mov	eax,DWORD PTR 112[esp]
	pxor	xmm0,xmm0
	and	eax,15
	jz	$L056xts_enc_ret
	pcmpgtd	xmm0,xmm1
	mov	DWORD PTR 112[esp],eax
	pshufd	xmm5,xmm0,19
	paddq	xmm1,xmm1
	pand	xmm5,XMMWORD PTR 96[esp]
	pxor	xmm5,xmm1
$L057xts_enc_steal:
	movzx	ecx,BYTE PTR [esi]
	movzx	edx,BYTE PTR [edi-16]
	lea	esi,DWORD PTR 1[esi]
	mov	BYTE PTR [edi-16],cl
	mov	BYTE PTR [edi],dl
	lea	edi,DWORD PTR 1[edi]
	sub	eax,1
	jnz	$L057xts_enc_steal
	sub	edi,DWORD PTR 112[esp]
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,XMMWORD PTR [edi-16]
	xorps	xmm2,xmm5
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L058enc1_loop_10:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L058enc1_loop_10
DB	102,15,56,221,209
	xorps	xmm2,xmm5
	movups	XMMWORD PTR [edi-16],xmm2
$L056xts_enc_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movdqa	XMMWORD PTR [esp],xmm0
	pxor	xmm3,xmm3
	movdqa	XMMWORD PTR 16[esp],xmm0
	pxor	xmm4,xmm4
	movdqa	XMMWORD PTR 32[esp],xmm0
	pxor	xmm5,xmm5
	movdqa	XMMWORD PTR 48[esp],xmm0
	pxor	xmm6,xmm6
	movdqa	XMMWORD PTR 64[esp],xmm0
	pxor	xmm7,xmm7
	movdqa	XMMWORD PTR 80[esp],xmm0
	mov	esp,DWORD PTR 116[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_aesni_xts_encrypt ENDP
ALIGN	16
_aesni_xts_decrypt	PROC PUBLIC
$L_aesni_xts_decrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	edx,DWORD PTR 36[esp]
	mov	esi,DWORD PTR 40[esp]
	mov	ecx,DWORD PTR 240[edx]
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L059enc1_loop_11:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L059enc1_loop_11
DB	102,15,56,221,209
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	mov	ebp,esp
	sub	esp,120
	and	esp,-16
	xor	ebx,ebx
	test	eax,15
	setnz	bl
	shl	ebx,4
	sub	eax,ebx
	mov	DWORD PTR 96[esp],135
	mov	DWORD PTR 100[esp],0
	mov	DWORD PTR 104[esp],1
	mov	DWORD PTR 108[esp],0
	mov	DWORD PTR 112[esp],eax
	mov	DWORD PTR 116[esp],ebp
	mov	ecx,DWORD PTR 240[edx]
	mov	ebp,edx
	mov	ebx,ecx
	movdqa	xmm1,xmm2
	pxor	xmm0,xmm0
	movdqa	xmm3,XMMWORD PTR 96[esp]
	pcmpgtd	xmm0,xmm1
	and	eax,-16
	sub	eax,96
	jc	$L060xts_dec_short
	shl	ecx,4
	mov	ebx,16
	sub	ebx,ecx
	lea	edx,DWORD PTR 32[ecx*1+edx]
	jmp	$L061xts_dec_loop6
ALIGN	16
$L061xts_dec_loop6:
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR [esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 16[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 32[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	XMMWORD PTR 48[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	pshufd	xmm7,xmm0,19
	movdqa	XMMWORD PTR 64[esp],xmm1
	paddq	xmm1,xmm1
	movups	xmm0,XMMWORD PTR [ebp]
	pand	xmm7,xmm3
	movups	xmm2,XMMWORD PTR [esi]
	pxor	xmm7,xmm1
	mov	ecx,ebx
	movdqu	xmm3,XMMWORD PTR 16[esi]
	xorps	xmm2,xmm0
	movdqu	xmm4,XMMWORD PTR 32[esi]
	pxor	xmm3,xmm0
	movdqu	xmm5,XMMWORD PTR 48[esi]
	pxor	xmm4,xmm0
	movdqu	xmm6,XMMWORD PTR 64[esi]
	pxor	xmm5,xmm0
	movdqu	xmm1,XMMWORD PTR 80[esi]
	pxor	xmm6,xmm0
	lea	esi,DWORD PTR 96[esi]
	pxor	xmm2,XMMWORD PTR [esp]
	movdqa	XMMWORD PTR 80[esp],xmm7
	pxor	xmm7,xmm1
	movups	xmm1,XMMWORD PTR 16[ebp]
	pxor	xmm3,XMMWORD PTR 16[esp]
	pxor	xmm4,XMMWORD PTR 32[esp]
DB	102,15,56,222,209
	pxor	xmm5,XMMWORD PTR 48[esp]
	pxor	xmm6,XMMWORD PTR 64[esp]
DB	102,15,56,222,217
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR 32[ebp]
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
	call	$L_aesni_decrypt6_enter
	movdqa	xmm1,XMMWORD PTR 80[esp]
	pxor	xmm0,xmm0
	xorps	xmm2,XMMWORD PTR [esp]
	pcmpgtd	xmm0,xmm1
	xorps	xmm3,XMMWORD PTR 16[esp]
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm4,XMMWORD PTR 32[esp]
	movups	XMMWORD PTR 16[edi],xmm3
	xorps	xmm5,XMMWORD PTR 48[esp]
	movups	XMMWORD PTR 32[edi],xmm4
	xorps	xmm6,XMMWORD PTR 64[esp]
	movups	XMMWORD PTR 48[edi],xmm5
	xorps	xmm7,xmm1
	movups	XMMWORD PTR 64[edi],xmm6
	pshufd	xmm2,xmm0,19
	movups	XMMWORD PTR 80[edi],xmm7
	lea	edi,DWORD PTR 96[edi]
	movdqa	xmm3,XMMWORD PTR 96[esp]
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	sub	eax,96
	jnc	$L061xts_dec_loop6
	mov	ecx,DWORD PTR 240[ebp]
	mov	edx,ebp
	mov	ebx,ecx
$L060xts_dec_short:
	add	eax,96
	jz	$L062xts_dec_done6x
	movdqa	xmm5,xmm1
	cmp	eax,32
	jb	$L063xts_dec_one
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	je	$L064xts_dec_two
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	cmp	eax,64
	jb	$L065xts_dec_three
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm7,xmm1
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
	movdqa	XMMWORD PTR [esp],xmm5
	movdqa	XMMWORD PTR 16[esp],xmm6
	je	$L066xts_dec_four
	movdqa	XMMWORD PTR 32[esp],xmm7
	pshufd	xmm7,xmm0,19
	movdqa	XMMWORD PTR 48[esp],xmm1
	paddq	xmm1,xmm1
	pand	xmm7,xmm3
	pxor	xmm7,xmm1
	movdqu	xmm2,XMMWORD PTR [esi]
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movdqu	xmm4,XMMWORD PTR 32[esi]
	pxor	xmm2,XMMWORD PTR [esp]
	movdqu	xmm5,XMMWORD PTR 48[esi]
	pxor	xmm3,XMMWORD PTR 16[esp]
	movdqu	xmm6,XMMWORD PTR 64[esi]
	pxor	xmm4,XMMWORD PTR 32[esp]
	lea	esi,DWORD PTR 80[esi]
	pxor	xmm5,XMMWORD PTR 48[esp]
	movdqa	XMMWORD PTR 64[esp],xmm7
	pxor	xmm6,xmm7
	call	__aesni_decrypt6
	movaps	xmm1,XMMWORD PTR 64[esp]
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,XMMWORD PTR 32[esp]
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm5,XMMWORD PTR 48[esp]
	movups	XMMWORD PTR 16[edi],xmm3
	xorps	xmm6,xmm1
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	movups	XMMWORD PTR 64[edi],xmm6
	lea	edi,DWORD PTR 80[edi]
	jmp	$L067xts_dec_done
ALIGN	16
$L063xts_dec_one:
	movups	xmm2,XMMWORD PTR [esi]
	lea	esi,DWORD PTR 16[esi]
	xorps	xmm2,xmm5
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L068dec1_loop_12:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L068dec1_loop_12
DB	102,15,56,223,209
	xorps	xmm2,xmm5
	movups	XMMWORD PTR [edi],xmm2
	lea	edi,DWORD PTR 16[edi]
	movdqa	xmm1,xmm5
	jmp	$L067xts_dec_done
ALIGN	16
$L064xts_dec_two:
	movaps	xmm6,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	lea	esi,DWORD PTR 32[esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	call	__aesni_decrypt2
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	lea	edi,DWORD PTR 32[edi]
	movdqa	xmm1,xmm6
	jmp	$L067xts_dec_done
ALIGN	16
$L065xts_dec_three:
	movaps	xmm7,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	movups	xmm4,XMMWORD PTR 32[esi]
	lea	esi,DWORD PTR 48[esi]
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	call	__aesni_decrypt3
	xorps	xmm2,xmm5
	xorps	xmm3,xmm6
	xorps	xmm4,xmm7
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	lea	edi,DWORD PTR 48[edi]
	movdqa	xmm1,xmm7
	jmp	$L067xts_dec_done
ALIGN	16
$L066xts_dec_four:
	movaps	xmm6,xmm1
	movups	xmm2,XMMWORD PTR [esi]
	movups	xmm3,XMMWORD PTR 16[esi]
	movups	xmm4,XMMWORD PTR 32[esi]
	xorps	xmm2,XMMWORD PTR [esp]
	movups	xmm5,XMMWORD PTR 48[esi]
	lea	esi,DWORD PTR 64[esi]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,xmm7
	xorps	xmm5,xmm6
	call	__aesni_decrypt4
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,XMMWORD PTR 16[esp]
	xorps	xmm4,xmm7
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm5,xmm6
	movups	XMMWORD PTR 16[edi],xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	lea	edi,DWORD PTR 64[edi]
	movdqa	xmm1,xmm6
	jmp	$L067xts_dec_done
ALIGN	16
$L062xts_dec_done6x:
	mov	eax,DWORD PTR 112[esp]
	and	eax,15
	jz	$L069xts_dec_ret
	mov	DWORD PTR 112[esp],eax
	jmp	$L070xts_dec_only_one_more
ALIGN	16
$L067xts_dec_done:
	mov	eax,DWORD PTR 112[esp]
	pxor	xmm0,xmm0
	and	eax,15
	jz	$L069xts_dec_ret
	pcmpgtd	xmm0,xmm1
	mov	DWORD PTR 112[esp],eax
	pshufd	xmm2,xmm0,19
	pxor	xmm0,xmm0
	movdqa	xmm3,XMMWORD PTR 96[esp]
	paddq	xmm1,xmm1
	pand	xmm2,xmm3
	pcmpgtd	xmm0,xmm1
	pxor	xmm1,xmm2
$L070xts_dec_only_one_more:
	pshufd	xmm5,xmm0,19
	movdqa	xmm6,xmm1
	paddq	xmm1,xmm1
	pand	xmm5,xmm3
	pxor	xmm5,xmm1
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,XMMWORD PTR [esi]
	xorps	xmm2,xmm5
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L071dec1_loop_13:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L071dec1_loop_13
DB	102,15,56,223,209
	xorps	xmm2,xmm5
	movups	XMMWORD PTR [edi],xmm2
$L072xts_dec_steal:
	movzx	ecx,BYTE PTR 16[esi]
	movzx	edx,BYTE PTR [edi]
	lea	esi,DWORD PTR 1[esi]
	mov	BYTE PTR [edi],cl
	mov	BYTE PTR 16[edi],dl
	lea	edi,DWORD PTR 1[edi]
	sub	eax,1
	jnz	$L072xts_dec_steal
	sub	edi,DWORD PTR 112[esp]
	mov	edx,ebp
	mov	ecx,ebx
	movups	xmm2,XMMWORD PTR [edi]
	xorps	xmm2,xmm6
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L073dec1_loop_14:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L073dec1_loop_14
DB	102,15,56,223,209
	xorps	xmm2,xmm6
	movups	XMMWORD PTR [edi],xmm2
$L069xts_dec_ret:
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movdqa	XMMWORD PTR [esp],xmm0
	pxor	xmm3,xmm3
	movdqa	XMMWORD PTR 16[esp],xmm0
	pxor	xmm4,xmm4
	movdqa	XMMWORD PTR 32[esp],xmm0
	pxor	xmm5,xmm5
	movdqa	XMMWORD PTR 48[esp],xmm0
	pxor	xmm6,xmm6
	movdqa	XMMWORD PTR 64[esp],xmm0
	pxor	xmm7,xmm7
	movdqa	XMMWORD PTR 80[esp],xmm0
	mov	esp,DWORD PTR 116[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_aesni_xts_decrypt ENDP
ALIGN	16
_aesni_cbc_encrypt	PROC PUBLIC
$L_aesni_cbc_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	ebx,esp
	mov	edi,DWORD PTR 24[esp]
	sub	ebx,24
	mov	eax,DWORD PTR 28[esp]
	and	ebx,-16
	mov	edx,DWORD PTR 32[esp]
	mov	ebp,DWORD PTR 36[esp]
	test	eax,eax
	jz	$L074cbc_abort
	cmp	DWORD PTR 40[esp],0
	xchg	ebx,esp
	movups	xmm7,XMMWORD PTR [ebp]
	mov	ecx,DWORD PTR 240[edx]
	mov	ebp,edx
	mov	DWORD PTR 16[esp],ebx
	mov	ebx,ecx
	je	$L075cbc_decrypt
	movaps	xmm2,xmm7
	cmp	eax,16
	jb	$L076cbc_enc_tail
	sub	eax,16
	jmp	$L077cbc_enc_loop
ALIGN	16
$L077cbc_enc_loop:
	movups	xmm7,XMMWORD PTR [esi]
	lea	esi,DWORD PTR 16[esi]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	xorps	xmm7,xmm0
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm7
$L078enc1_loop_15:
DB	102,15,56,220,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L078enc1_loop_15
DB	102,15,56,221,209
	mov	ecx,ebx
	mov	edx,ebp
	movups	XMMWORD PTR [edi],xmm2
	lea	edi,DWORD PTR 16[edi]
	sub	eax,16
	jnc	$L077cbc_enc_loop
	add	eax,16
	jnz	$L076cbc_enc_tail
	movaps	xmm7,xmm2
	pxor	xmm2,xmm2
	jmp	$L079cbc_ret
$L076cbc_enc_tail:
	mov	ecx,eax
DD	2767451785
	mov	ecx,16
	sub	ecx,eax
	xor	eax,eax
DD	2868115081
	lea	edi,DWORD PTR [edi-16]
	mov	ecx,ebx
	mov	esi,edi
	mov	edx,ebp
	jmp	$L077cbc_enc_loop
ALIGN	16
$L075cbc_decrypt:
	cmp	eax,80
	jbe	$L080cbc_dec_tail
	movaps	XMMWORD PTR [esp],xmm7
	sub	eax,80
	jmp	$L081cbc_dec_loop6_enter
ALIGN	16
$L082cbc_dec_loop6:
	movaps	XMMWORD PTR [esp],xmm0
	movups	XMMWORD PTR [edi],xmm7
	lea	edi,DWORD PTR 16[edi]
$L081cbc_dec_loop6_enter:
	movdqu	xmm2,XMMWORD PTR [esi]
	movdqu	xmm3,XMMWORD PTR 16[esi]
	movdqu	xmm4,XMMWORD PTR 32[esi]
	movdqu	xmm5,XMMWORD PTR 48[esi]
	movdqu	xmm6,XMMWORD PTR 64[esi]
	movdqu	xmm7,XMMWORD PTR 80[esi]
	call	__aesni_decrypt6
	movups	xmm1,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR 16[esi]
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,xmm1
	movups	xmm1,XMMWORD PTR 32[esi]
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR 48[esi]
	xorps	xmm5,xmm1
	movups	xmm1,XMMWORD PTR 64[esi]
	xorps	xmm6,xmm0
	movups	xmm0,XMMWORD PTR 80[esi]
	xorps	xmm7,xmm1
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	lea	esi,DWORD PTR 96[esi]
	movups	XMMWORD PTR 32[edi],xmm4
	mov	ecx,ebx
	movups	XMMWORD PTR 48[edi],xmm5
	mov	edx,ebp
	movups	XMMWORD PTR 64[edi],xmm6
	lea	edi,DWORD PTR 80[edi]
	sub	eax,96
	ja	$L082cbc_dec_loop6
	movaps	xmm2,xmm7
	movaps	xmm7,xmm0
	add	eax,80
	jle	$L083cbc_dec_clear_tail_collected
	movups	XMMWORD PTR [edi],xmm2
	lea	edi,DWORD PTR 16[edi]
$L080cbc_dec_tail:
	movups	xmm2,XMMWORD PTR [esi]
	movaps	xmm6,xmm2
	cmp	eax,16
	jbe	$L084cbc_dec_one
	movups	xmm3,XMMWORD PTR 16[esi]
	movaps	xmm5,xmm3
	cmp	eax,32
	jbe	$L085cbc_dec_two
	movups	xmm4,XMMWORD PTR 32[esi]
	cmp	eax,48
	jbe	$L086cbc_dec_three
	movups	xmm5,XMMWORD PTR 48[esi]
	cmp	eax,64
	jbe	$L087cbc_dec_four
	movups	xmm6,XMMWORD PTR 64[esi]
	movaps	XMMWORD PTR [esp],xmm7
	movups	xmm2,XMMWORD PTR [esi]
	xorps	xmm7,xmm7
	call	__aesni_decrypt6
	movups	xmm1,XMMWORD PTR [esi]
	movups	xmm0,XMMWORD PTR 16[esi]
	xorps	xmm2,XMMWORD PTR [esp]
	xorps	xmm3,xmm1
	movups	xmm1,XMMWORD PTR 32[esi]
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR 48[esi]
	xorps	xmm5,xmm1
	movups	xmm7,XMMWORD PTR 64[esi]
	xorps	xmm6,xmm0
	movups	XMMWORD PTR [edi],xmm2
	movups	XMMWORD PTR 16[edi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR 32[edi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR 48[edi],xmm5
	pxor	xmm5,xmm5
	lea	edi,DWORD PTR 64[edi]
	movaps	xmm2,xmm6
	pxor	xmm6,xmm6
	sub	eax,80
	jmp	$L088cbc_dec_tail_collected
ALIGN	16
$L084cbc_dec_one:
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR 16[edx]
	lea	edx,DWORD PTR 32[edx]
	xorps	xmm2,xmm0
$L089dec1_loop_16:
DB	102,15,56,222,209
	dec	ecx
	movups	xmm1,XMMWORD PTR [edx]
	lea	edx,DWORD PTR 16[edx]
	jnz	$L089dec1_loop_16
DB	102,15,56,223,209
	xorps	xmm2,xmm7
	movaps	xmm7,xmm6
	sub	eax,16
	jmp	$L088cbc_dec_tail_collected
ALIGN	16
$L085cbc_dec_two:
	call	__aesni_decrypt2
	xorps	xmm2,xmm7
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	movaps	xmm2,xmm3
	pxor	xmm3,xmm3
	lea	edi,DWORD PTR 16[edi]
	movaps	xmm7,xmm5
	sub	eax,32
	jmp	$L088cbc_dec_tail_collected
ALIGN	16
$L086cbc_dec_three:
	call	__aesni_decrypt3
	xorps	xmm2,xmm7
	xorps	xmm3,xmm6
	xorps	xmm4,xmm5
	movups	XMMWORD PTR [edi],xmm2
	movaps	xmm2,xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR 16[edi],xmm3
	pxor	xmm3,xmm3
	lea	edi,DWORD PTR 32[edi]
	movups	xmm7,XMMWORD PTR 32[esi]
	sub	eax,48
	jmp	$L088cbc_dec_tail_collected
ALIGN	16
$L087cbc_dec_four:
	call	__aesni_decrypt4
	movups	xmm1,XMMWORD PTR 16[esi]
	movups	xmm0,XMMWORD PTR 32[esi]
	xorps	xmm2,xmm7
	movups	xmm7,XMMWORD PTR 48[esi]
	xorps	xmm3,xmm6
	movups	XMMWORD PTR [edi],xmm2
	xorps	xmm4,xmm1
	movups	XMMWORD PTR 16[edi],xmm3
	pxor	xmm3,xmm3
	xorps	xmm5,xmm0
	movups	XMMWORD PTR 32[edi],xmm4
	pxor	xmm4,xmm4
	lea	edi,DWORD PTR 48[edi]
	movaps	xmm2,xmm5
	pxor	xmm5,xmm5
	sub	eax,64
	jmp	$L088cbc_dec_tail_collected
ALIGN	16
$L083cbc_dec_clear_tail_collected:
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
$L088cbc_dec_tail_collected:
	and	eax,15
	jnz	$L090cbc_dec_tail_partial
	movups	XMMWORD PTR [edi],xmm2
	pxor	xmm0,xmm0
	jmp	$L079cbc_ret
ALIGN	16
$L090cbc_dec_tail_partial:
	movaps	XMMWORD PTR [esp],xmm2
	pxor	xmm0,xmm0
	mov	ecx,16
	mov	esi,esp
	sub	ecx,eax
DD	2767451785
	movdqa	XMMWORD PTR [esp],xmm2
$L079cbc_ret:
	mov	esp,DWORD PTR 16[esp]
	mov	ebp,DWORD PTR 36[esp]
	pxor	xmm2,xmm2
	pxor	xmm1,xmm1
	movups	XMMWORD PTR [ebp],xmm7
	pxor	xmm7,xmm7
$L074cbc_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_aesni_cbc_encrypt ENDP
ALIGN	16
__aesni_set_encrypt_key	PROC PRIVATE
	push	ebp
	push	ebx
	test	eax,eax
	jz	$L091bad_pointer
	test	edx,edx
	jz	$L091bad_pointer
	call	$L092pic
$L092pic:
	pop	ebx
	lea	ebx,DWORD PTR ($Lkey_const-$L092pic)[ebx]
	lea	ebp,DWORD PTR _OPENSSL_ia32cap_P
	movups	xmm0,XMMWORD PTR [eax]
	xorps	xmm4,xmm4
	mov	ebp,DWORD PTR 4[ebp]
	lea	edx,DWORD PTR 16[edx]
	and	ebp,268437504
	cmp	ecx,256
	je	$L09314rounds
	cmp	ecx,192
	je	$L09412rounds
	cmp	ecx,128
	jne	$L095bad_keybits
ALIGN	16
$L09610rounds:
	cmp	ebp,268435456
	je	$L09710rounds_alt
	mov	ecx,9
	movups	XMMWORD PTR [edx-16],xmm0
DB	102,15,58,223,200,1
	call	$L098key_128_cold
DB	102,15,58,223,200,2
	call	$L099key_128
DB	102,15,58,223,200,4
	call	$L099key_128
DB	102,15,58,223,200,8
	call	$L099key_128
DB	102,15,58,223,200,16
	call	$L099key_128
DB	102,15,58,223,200,32
	call	$L099key_128
DB	102,15,58,223,200,64
	call	$L099key_128
DB	102,15,58,223,200,128
	call	$L099key_128
DB	102,15,58,223,200,27
	call	$L099key_128
DB	102,15,58,223,200,54
	call	$L099key_128
	movups	XMMWORD PTR [edx],xmm0
	mov	DWORD PTR 80[edx],ecx
	jmp	$L100good_key
ALIGN	16
$L099key_128:
	movups	XMMWORD PTR [edx],xmm0
	lea	edx,DWORD PTR 16[edx]
$L098key_128_cold:
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	ret
ALIGN	16
$L09710rounds_alt:
	movdqa	xmm5,XMMWORD PTR [ebx]
	mov	ecx,8
	movdqa	xmm4,XMMWORD PTR 32[ebx]
	movdqa	xmm2,xmm0
	movdqu	XMMWORD PTR [edx-16],xmm0
$L101loop_key128:
DB	102,15,56,0,197
DB	102,15,56,221,196
	pslld	xmm4,1
	lea	edx,DWORD PTR 16[edx]
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	XMMWORD PTR [edx-16],xmm0
	movdqa	xmm2,xmm0
	dec	ecx
	jnz	$L101loop_key128
	movdqa	xmm4,XMMWORD PTR 48[ebx]
DB	102,15,56,0,197
DB	102,15,56,221,196
	pslld	xmm4,1
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	XMMWORD PTR [edx],xmm0
	movdqa	xmm2,xmm0
DB	102,15,56,0,197
DB	102,15,56,221,196
	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3
	pxor	xmm0,xmm2
	movdqu	XMMWORD PTR 16[edx],xmm0
	mov	ecx,9
	mov	DWORD PTR 96[edx],ecx
	jmp	$L100good_key
ALIGN	16
$L09412rounds:
	movq	xmm2,QWORD PTR 16[eax]
	cmp	ebp,268435456
	je	$L10212rounds_alt
	mov	ecx,11
	movups	XMMWORD PTR [edx-16],xmm0
DB	102,15,58,223,202,1
	call	$L103key_192a_cold
DB	102,15,58,223,202,2
	call	$L104key_192b
DB	102,15,58,223,202,4
	call	$L105key_192a
DB	102,15,58,223,202,8
	call	$L104key_192b
DB	102,15,58,223,202,16
	call	$L105key_192a
DB	102,15,58,223,202,32
	call	$L104key_192b
DB	102,15,58,223,202,64
	call	$L105key_192a
DB	102,15,58,223,202,128
	call	$L104key_192b
	movups	XMMWORD PTR [edx],xmm0
	mov	DWORD PTR 48[edx],ecx
	jmp	$L100good_key
ALIGN	16
$L105key_192a:
	movups	XMMWORD PTR [edx],xmm0
	lea	edx,DWORD PTR 16[edx]
ALIGN	16
$L103key_192a_cold:
	movaps	xmm5,xmm2
$L106key_192b_warm:
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
ALIGN	16
$L104key_192b:
	movaps	xmm3,xmm0
	shufps	xmm5,xmm0,68
	movups	XMMWORD PTR [edx],xmm5
	shufps	xmm3,xmm2,78
	movups	XMMWORD PTR 16[edx],xmm3
	lea	edx,DWORD PTR 32[edx]
	jmp	$L106key_192b_warm
ALIGN	16
$L10212rounds_alt:
	movdqa	xmm5,XMMWORD PTR 16[ebx]
	movdqa	xmm4,XMMWORD PTR 32[ebx]
	mov	ecx,8
	movdqu	XMMWORD PTR [edx-16],xmm0
$L107loop_key192:
	movq	QWORD PTR [edx],xmm2
	movdqa	xmm1,xmm2
DB	102,15,56,0,213
DB	102,15,56,221,212
	pslld	xmm4,1
	lea	edx,DWORD PTR 24[edx]
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
	movdqu	XMMWORD PTR [edx-16],xmm0
	dec	ecx
	jnz	$L107loop_key192
	mov	ecx,11
	mov	DWORD PTR 32[edx],ecx
	jmp	$L100good_key
ALIGN	16
$L09314rounds:
	movups	xmm2,XMMWORD PTR 16[eax]
	lea	edx,DWORD PTR 16[edx]
	cmp	ebp,268435456
	je	$L10814rounds_alt
	mov	ecx,13
	movups	XMMWORD PTR [edx-32],xmm0
	movups	XMMWORD PTR [edx-16],xmm2
DB	102,15,58,223,202,1
	call	$L109key_256a_cold
DB	102,15,58,223,200,1
	call	$L110key_256b
DB	102,15,58,223,202,2
	call	$L111key_256a
DB	102,15,58,223,200,2
	call	$L110key_256b
DB	102,15,58,223,202,4
	call	$L111key_256a
DB	102,15,58,223,200,4
	call	$L110key_256b
DB	102,15,58,223,202,8
	call	$L111key_256a
DB	102,15,58,223,200,8
	call	$L110key_256b
DB	102,15,58,223,202,16
	call	$L111key_256a
DB	102,15,58,223,200,16
	call	$L110key_256b
DB	102,15,58,223,202,32
	call	$L111key_256a
DB	102,15,58,223,200,32
	call	$L110key_256b
DB	102,15,58,223,202,64
	call	$L111key_256a
	movups	XMMWORD PTR [edx],xmm0
	mov	DWORD PTR 16[edx],ecx
	xor	eax,eax
	jmp	$L100good_key
ALIGN	16
$L111key_256a:
	movups	XMMWORD PTR [edx],xmm2
	lea	edx,DWORD PTR 16[edx]
$L109key_256a_cold:
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	ret
ALIGN	16
$L110key_256b:
	movups	XMMWORD PTR [edx],xmm0
	lea	edx,DWORD PTR 16[edx]
	shufps	xmm4,xmm2,16
	xorps	xmm2,xmm4
	shufps	xmm4,xmm2,140
	xorps	xmm2,xmm4
	shufps	xmm1,xmm1,170
	xorps	xmm2,xmm1
	ret
ALIGN	16
$L10814rounds_alt:
	movdqa	xmm5,XMMWORD PTR [ebx]
	movdqa	xmm4,XMMWORD PTR 32[ebx]
	mov	ecx,7
	movdqu	XMMWORD PTR [edx-32],xmm0
	movdqa	xmm1,xmm2
	movdqu	XMMWORD PTR [edx-16],xmm2
$L112loop_key256:
DB	102,15,56,0,213
DB	102,15,56,221,212
	movdqa	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm0,xmm3
	pslld	xmm4,1
	pxor	xmm0,xmm2
	movdqu	XMMWORD PTR [edx],xmm0
	dec	ecx
	jz	$L113done_key256
	pshufd	xmm2,xmm0,255
	pxor	xmm3,xmm3
DB	102,15,56,221,211
	movdqa	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm1,xmm3
	pxor	xmm2,xmm1
	movdqu	XMMWORD PTR 16[edx],xmm2
	lea	edx,DWORD PTR 32[edx]
	movdqa	xmm1,xmm2
	jmp	$L112loop_key256
$L113done_key256:
	mov	ecx,13
	mov	DWORD PTR 16[edx],ecx
$L100good_key:
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
ALIGN	4
$L091bad_pointer:
	mov	eax,-1
	pop	ebx
	pop	ebp
	ret
ALIGN	4
$L095bad_keybits:
	pxor	xmm0,xmm0
	mov	eax,-2
	pop	ebx
	pop	ebp
	ret
__aesni_set_encrypt_key ENDP
ALIGN	16
_aesni_set_encrypt_key	PROC PUBLIC
$L_aesni_set_encrypt_key_begin::
	mov	eax,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	call	__aesni_set_encrypt_key
	ret
_aesni_set_encrypt_key ENDP
ALIGN	16
_aesni_set_decrypt_key	PROC PUBLIC
$L_aesni_set_decrypt_key_begin::
	mov	eax,DWORD PTR 4[esp]
	mov	ecx,DWORD PTR 8[esp]
	mov	edx,DWORD PTR 12[esp]
	call	__aesni_set_encrypt_key
	mov	edx,DWORD PTR 12[esp]
	shl	ecx,4
	test	eax,eax
	jnz	$L114dec_key_ret
	lea	eax,DWORD PTR 16[ecx*1+edx]
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR [eax]
	movups	XMMWORD PTR [eax],xmm0
	movups	XMMWORD PTR [edx],xmm1
	lea	edx,DWORD PTR 16[edx]
	lea	eax,DWORD PTR [eax-16]
$L115dec_key_inverse:
	movups	xmm0,XMMWORD PTR [edx]
	movups	xmm1,XMMWORD PTR [eax]
DB	102,15,56,219,192
DB	102,15,56,219,201
	lea	edx,DWORD PTR 16[edx]
	lea	eax,DWORD PTR [eax-16]
	movups	XMMWORD PTR 16[eax],xmm0
	movups	XMMWORD PTR [edx-16],xmm1
	cmp	eax,edx
	ja	$L115dec_key_inverse
	movups	xmm0,XMMWORD PTR [edx]
DB	102,15,56,219,192
	movups	XMMWORD PTR [edx],xmm0
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	xor	eax,eax
$L114dec_key_ret:
	ret
_aesni_set_decrypt_key ENDP
ALIGN	64
$Lkey_const::
DD	202313229,202313229,202313229,202313229
DD	67569157,67569157,67569157,67569157
DD	1,1,1,1
DD	27,27,27,27
DB	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69
DB	83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
.text$	ENDS
.bss	SEGMENT 'BSS'
COMM	_OPENSSL_ia32cap_P:DWORD:4
.bss	ENDS
END
