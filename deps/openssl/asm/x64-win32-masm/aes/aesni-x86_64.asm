OPTION	DOTNAME
.text$	SEGMENT ALIGN(64) 'CODE'
PUBLIC	aesni_encrypt

ALIGN	16
aesni_encrypt	PROC PUBLIC
	movups	xmm2,XMMWORD PTR[rcx]
	mov	eax,DWORD PTR[240+r8]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm2,xmm0
$L$oop_enc1_1::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_enc1_1

DB	102,15,56,221,209
	movups	XMMWORD PTR[rdx],xmm2
	DB	0F3h,0C3h		;repret
aesni_encrypt	ENDP

PUBLIC	aesni_decrypt

ALIGN	16
aesni_decrypt	PROC PUBLIC
	movups	xmm2,XMMWORD PTR[rcx]
	mov	eax,DWORD PTR[240+r8]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm2,xmm0
$L$oop_dec1_2::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_dec1_2

DB	102,15,56,223,209
	movups	XMMWORD PTR[rdx],xmm2
	DB	0F3h,0C3h		;repret
aesni_decrypt	ENDP

ALIGN	16
_aesni_encrypt3	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[rcx]

$L$enc_loop3::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$enc_loop3

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
	DB	0F3h,0C3h		;repret
_aesni_encrypt3	ENDP

ALIGN	16
_aesni_decrypt3	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[rcx]

$L$dec_loop3::
DB	102,15,56,222,209
DB	102,15,56,222,217
	dec	eax
DB	102,15,56,222,225
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,222,208
DB	102,15,56,222,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,222,224
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$dec_loop3

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
	DB	0F3h,0C3h		;repret
_aesni_decrypt3	ENDP

ALIGN	16
_aesni_encrypt4	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	xorps	xmm5,xmm0
	movups	xmm0,XMMWORD PTR[rcx]

$L$enc_loop4::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$enc_loop4

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
DB	102,15,56,221,232
	DB	0F3h,0C3h		;repret
_aesni_encrypt4	ENDP

ALIGN	16
_aesni_decrypt4	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	xorps	xmm5,xmm0
	movups	xmm0,XMMWORD PTR[rcx]

$L$dec_loop4::
DB	102,15,56,222,209
DB	102,15,56,222,217
	dec	eax
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,222,208
DB	102,15,56,222,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$dec_loop4

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
DB	102,15,56,223,232
	DB	0F3h,0C3h		;repret
_aesni_decrypt4	ENDP

ALIGN	16
_aesni_encrypt6	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
DB	102,15,56,220,209
	pxor	xmm4,xmm0
DB	102,15,56,220,217
	pxor	xmm5,xmm0
DB	102,15,56,220,225
	pxor	xmm6,xmm0
DB	102,15,56,220,233
	pxor	xmm7,xmm0
	dec	eax
DB	102,15,56,220,241
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,220,249
	jmp	$L$enc_loop6_enter
ALIGN	16
$L$enc_loop6::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
$L$enc_loop6_enter::
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$enc_loop6

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
	DB	0F3h,0C3h		;repret
_aesni_encrypt6	ENDP

ALIGN	16
_aesni_decrypt6	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
DB	102,15,56,222,209
	pxor	xmm4,xmm0
DB	102,15,56,222,217
	pxor	xmm5,xmm0
DB	102,15,56,222,225
	pxor	xmm6,xmm0
DB	102,15,56,222,233
	pxor	xmm7,xmm0
	dec	eax
DB	102,15,56,222,241
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,15,56,222,249
	jmp	$L$dec_loop6_enter
ALIGN	16
$L$dec_loop6::
DB	102,15,56,222,209
DB	102,15,56,222,217
	dec	eax
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
$L$dec_loop6_enter::
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,222,208
DB	102,15,56,222,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$dec_loop6

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
	DB	0F3h,0C3h		;repret
_aesni_decrypt6	ENDP

ALIGN	16
_aesni_encrypt8	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
DB	102,15,56,220,209
	pxor	xmm4,xmm0
DB	102,15,56,220,217
	pxor	xmm5,xmm0
DB	102,15,56,220,225
	pxor	xmm6,xmm0
DB	102,15,56,220,233
	pxor	xmm7,xmm0
	dec	eax
DB	102,15,56,220,241
	pxor	xmm8,xmm0
DB	102,15,56,220,249
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[16+rcx]
	jmp	$L$enc_loop8_enter
ALIGN	16
$L$enc_loop8::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[16+rcx]
$L$enc_loop8_enter::
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$enc_loop8

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
DB	102,15,56,221,208
DB	102,15,56,221,216
DB	102,15,56,221,224
DB	102,15,56,221,232
DB	102,15,56,221,240
DB	102,15,56,221,248
DB	102,68,15,56,221,192
DB	102,68,15,56,221,200
	DB	0F3h,0C3h		;repret
_aesni_encrypt8	ENDP

ALIGN	16
_aesni_decrypt8	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
DB	102,15,56,222,209
	pxor	xmm4,xmm0
DB	102,15,56,222,217
	pxor	xmm5,xmm0
DB	102,15,56,222,225
	pxor	xmm6,xmm0
DB	102,15,56,222,233
	pxor	xmm7,xmm0
	dec	eax
DB	102,15,56,222,241
	pxor	xmm8,xmm0
DB	102,15,56,222,249
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[16+rcx]
	jmp	$L$dec_loop8_enter
ALIGN	16
$L$dec_loop8::
DB	102,15,56,222,209
DB	102,15,56,222,217
	dec	eax
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[16+rcx]
$L$dec_loop8_enter::
DB	102,15,56,222,208
DB	102,15,56,222,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$dec_loop8

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
DB	102,15,56,223,208
DB	102,15,56,223,216
DB	102,15,56,223,224
DB	102,15,56,223,232
DB	102,15,56,223,240
DB	102,15,56,223,248
DB	102,68,15,56,223,192
DB	102,68,15,56,223,200
	DB	0F3h,0C3h		;repret
_aesni_decrypt8	ENDP
PUBLIC	aesni_ecb_encrypt

ALIGN	16
aesni_ecb_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_ecb_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	and	rdx,-16
	jz	$L$ecb_ret

	mov	eax,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[rcx]
	mov	r11,rcx
	mov	r10d,eax
	test	r8d,r8d
	jz	$L$ecb_decrypt

	cmp	rdx,080h
	jb	$L$ecb_enc_tail

	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	movdqu	xmm9,XMMWORD PTR[112+rdi]
	lea	rdi,QWORD PTR[128+rdi]
	sub	rdx,080h
	jmp	$L$ecb_enc_loop8_enter
ALIGN	16
$L$ecb_enc_loop8::
	movups	XMMWORD PTR[rsi],xmm2
	mov	rcx,r11
	movdqu	xmm2,XMMWORD PTR[rdi]
	mov	eax,r10d
	movups	XMMWORD PTR[16+rsi],xmm3
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movups	XMMWORD PTR[32+rsi],xmm4
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movups	XMMWORD PTR[48+rsi],xmm5
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movups	XMMWORD PTR[64+rsi],xmm6
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movups	XMMWORD PTR[80+rsi],xmm7
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movups	XMMWORD PTR[96+rsi],xmm8
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	movups	XMMWORD PTR[112+rsi],xmm9
	lea	rsi,QWORD PTR[128+rsi]
	movdqu	xmm9,XMMWORD PTR[112+rdi]
	lea	rdi,QWORD PTR[128+rdi]
$L$ecb_enc_loop8_enter::

	call	_aesni_encrypt8

	sub	rdx,080h
	jnc	$L$ecb_enc_loop8

	movups	XMMWORD PTR[rsi],xmm2
	mov	rcx,r11
	movups	XMMWORD PTR[16+rsi],xmm3
	mov	eax,r10d
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	movups	XMMWORD PTR[112+rsi],xmm9
	lea	rsi,QWORD PTR[128+rsi]
	add	rdx,080h
	jz	$L$ecb_ret

$L$ecb_enc_tail::
	movups	xmm2,XMMWORD PTR[rdi]
	cmp	rdx,020h
	jb	$L$ecb_enc_one
	movups	xmm3,XMMWORD PTR[16+rdi]
	je	$L$ecb_enc_two
	movups	xmm4,XMMWORD PTR[32+rdi]
	cmp	rdx,040h
	jb	$L$ecb_enc_three
	movups	xmm5,XMMWORD PTR[48+rdi]
	je	$L$ecb_enc_four
	movups	xmm6,XMMWORD PTR[64+rdi]
	cmp	rdx,060h
	jb	$L$ecb_enc_five
	movups	xmm7,XMMWORD PTR[80+rdi]
	je	$L$ecb_enc_six
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	call	_aesni_encrypt8
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_one::
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_3::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_3

DB	102,15,56,221,209
	movups	XMMWORD PTR[rsi],xmm2
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_two::
	xorps	xmm4,xmm4
	call	_aesni_encrypt3
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_three::
	call	_aesni_encrypt3
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_four::
	call	_aesni_encrypt4
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_five::
	xorps	xmm7,xmm7
	call	_aesni_encrypt6
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_enc_six::
	call	_aesni_encrypt6
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	jmp	$L$ecb_ret

ALIGN	16
$L$ecb_decrypt::
	cmp	rdx,080h
	jb	$L$ecb_dec_tail

	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	movdqu	xmm9,XMMWORD PTR[112+rdi]
	lea	rdi,QWORD PTR[128+rdi]
	sub	rdx,080h
	jmp	$L$ecb_dec_loop8_enter
ALIGN	16
$L$ecb_dec_loop8::
	movups	XMMWORD PTR[rsi],xmm2
	mov	rcx,r11
	movdqu	xmm2,XMMWORD PTR[rdi]
	mov	eax,r10d
	movups	XMMWORD PTR[16+rsi],xmm3
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movups	XMMWORD PTR[32+rsi],xmm4
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movups	XMMWORD PTR[48+rsi],xmm5
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movups	XMMWORD PTR[64+rsi],xmm6
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movups	XMMWORD PTR[80+rsi],xmm7
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movups	XMMWORD PTR[96+rsi],xmm8
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	movups	XMMWORD PTR[112+rsi],xmm9
	lea	rsi,QWORD PTR[128+rsi]
	movdqu	xmm9,XMMWORD PTR[112+rdi]
	lea	rdi,QWORD PTR[128+rdi]
$L$ecb_dec_loop8_enter::

	call	_aesni_decrypt8

	movups	xmm0,XMMWORD PTR[r11]
	sub	rdx,080h
	jnc	$L$ecb_dec_loop8

	movups	XMMWORD PTR[rsi],xmm2
	mov	rcx,r11
	movups	XMMWORD PTR[16+rsi],xmm3
	mov	eax,r10d
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	movups	XMMWORD PTR[112+rsi],xmm9
	lea	rsi,QWORD PTR[128+rsi]
	add	rdx,080h
	jz	$L$ecb_ret

$L$ecb_dec_tail::
	movups	xmm2,XMMWORD PTR[rdi]
	cmp	rdx,020h
	jb	$L$ecb_dec_one
	movups	xmm3,XMMWORD PTR[16+rdi]
	je	$L$ecb_dec_two
	movups	xmm4,XMMWORD PTR[32+rdi]
	cmp	rdx,040h
	jb	$L$ecb_dec_three
	movups	xmm5,XMMWORD PTR[48+rdi]
	je	$L$ecb_dec_four
	movups	xmm6,XMMWORD PTR[64+rdi]
	cmp	rdx,060h
	jb	$L$ecb_dec_five
	movups	xmm7,XMMWORD PTR[80+rdi]
	je	$L$ecb_dec_six
	movups	xmm8,XMMWORD PTR[96+rdi]
	movups	xmm0,XMMWORD PTR[rcx]
	call	_aesni_decrypt8
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_one::
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_4::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_4

DB	102,15,56,223,209
	movups	XMMWORD PTR[rsi],xmm2
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_two::
	xorps	xmm4,xmm4
	call	_aesni_decrypt3
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_three::
	call	_aesni_decrypt3
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_four::
	call	_aesni_decrypt4
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_five::
	xorps	xmm7,xmm7
	call	_aesni_decrypt6
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_six::
	call	_aesni_decrypt6
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7

$L$ecb_ret::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_ecb_encrypt::
aesni_ecb_encrypt	ENDP
PUBLIC	aesni_ccm64_encrypt_blocks

ALIGN	16
aesni_ccm64_encrypt_blocks	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_ccm64_encrypt_blocks::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	lea	rsp,QWORD PTR[((-88))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
$L$ccm64_enc_body::
	mov	eax,DWORD PTR[240+rcx]
	movdqu	xmm9,XMMWORD PTR[r8]
	movdqa	xmm6,XMMWORD PTR[$L$increment64]
	movdqa	xmm7,XMMWORD PTR[$L$bswap_mask]

	shr	eax,1
	lea	r11,QWORD PTR[rcx]
	movdqu	xmm3,XMMWORD PTR[r9]
	movdqa	xmm2,xmm9
	mov	r10d,eax
DB	102,68,15,56,0,207
	jmp	$L$ccm64_enc_outer
ALIGN	16
$L$ccm64_enc_outer::
	movups	xmm0,XMMWORD PTR[r11]
	mov	eax,r10d
	movups	xmm8,XMMWORD PTR[rdi]

	xorps	xmm2,xmm0
	movups	xmm1,XMMWORD PTR[16+r11]
	xorps	xmm0,xmm8
	lea	rcx,QWORD PTR[32+r11]
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR[rcx]

$L$ccm64_enc2_loop::
DB	102,15,56,220,209
	dec	eax
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$ccm64_enc2_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
	paddq	xmm9,xmm6
DB	102,15,56,221,208
DB	102,15,56,221,216

	dec	rdx
	lea	rdi,QWORD PTR[16+rdi]
	xorps	xmm8,xmm2
	movdqa	xmm2,xmm9
	movups	XMMWORD PTR[rsi],xmm8
	lea	rsi,QWORD PTR[16+rsi]
DB	102,15,56,0,215
	jnz	$L$ccm64_enc_outer

	movups	XMMWORD PTR[r9],xmm3
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	lea	rsp,QWORD PTR[88+rsp]
$L$ccm64_enc_ret::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_ccm64_encrypt_blocks::
aesni_ccm64_encrypt_blocks	ENDP
PUBLIC	aesni_ccm64_decrypt_blocks

ALIGN	16
aesni_ccm64_decrypt_blocks	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_ccm64_decrypt_blocks::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	lea	rsp,QWORD PTR[((-88))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
$L$ccm64_dec_body::
	mov	eax,DWORD PTR[240+rcx]
	movups	xmm9,XMMWORD PTR[r8]
	movdqu	xmm3,XMMWORD PTR[r9]
	movdqa	xmm6,XMMWORD PTR[$L$increment64]
	movdqa	xmm7,XMMWORD PTR[$L$bswap_mask]

	movaps	xmm2,xmm9
	mov	r10d,eax
	mov	r11,rcx
DB	102,68,15,56,0,207
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_5::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_5

DB	102,15,56,221,209
	movups	xmm8,XMMWORD PTR[rdi]
	paddq	xmm9,xmm6
	lea	rdi,QWORD PTR[16+rdi]
	jmp	$L$ccm64_dec_outer
ALIGN	16
$L$ccm64_dec_outer::
	xorps	xmm8,xmm2
	movdqa	xmm2,xmm9
	mov	eax,r10d
	movups	XMMWORD PTR[rsi],xmm8
	lea	rsi,QWORD PTR[16+rsi]
DB	102,15,56,0,215

	sub	rdx,1
	jz	$L$ccm64_dec_break

	movups	xmm0,XMMWORD PTR[r11]
	shr	eax,1
	movups	xmm1,XMMWORD PTR[16+r11]
	xorps	xmm8,xmm0
	lea	rcx,QWORD PTR[32+r11]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm8
	movups	xmm0,XMMWORD PTR[rcx]

$L$ccm64_dec2_loop::
DB	102,15,56,220,209
	dec	eax
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$ccm64_dec2_loop
	movups	xmm8,XMMWORD PTR[rdi]
	paddq	xmm9,xmm6
DB	102,15,56,220,209
DB	102,15,56,220,217
	lea	rdi,QWORD PTR[16+rdi]
DB	102,15,56,221,208
DB	102,15,56,221,216
	jmp	$L$ccm64_dec_outer

ALIGN	16
$L$ccm64_dec_break::

	movups	xmm0,XMMWORD PTR[r11]
	movups	xmm1,XMMWORD PTR[16+r11]
	xorps	xmm8,xmm0
	lea	r11,QWORD PTR[32+r11]
	xorps	xmm3,xmm8
$L$oop_enc1_6::
DB	102,15,56,220,217
	dec	eax
	movups	xmm1,XMMWORD PTR[r11]
	lea	r11,QWORD PTR[16+r11]
	jnz	$L$oop_enc1_6

DB	102,15,56,221,217
	movups	XMMWORD PTR[r9],xmm3
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	lea	rsp,QWORD PTR[88+rsp]
$L$ccm64_dec_ret::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_ccm64_decrypt_blocks::
aesni_ccm64_decrypt_blocks	ENDP
PUBLIC	aesni_ctr32_encrypt_blocks

ALIGN	16
aesni_ctr32_encrypt_blocks	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_ctr32_encrypt_blocks::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]


	lea	rsp,QWORD PTR[((-200))+rsp]
	movaps	XMMWORD PTR[32+rsp],xmm6
	movaps	XMMWORD PTR[48+rsp],xmm7
	movaps	XMMWORD PTR[64+rsp],xmm8
	movaps	XMMWORD PTR[80+rsp],xmm9
	movaps	XMMWORD PTR[96+rsp],xmm10
	movaps	XMMWORD PTR[112+rsp],xmm11
	movaps	XMMWORD PTR[128+rsp],xmm12
	movaps	XMMWORD PTR[144+rsp],xmm13
	movaps	XMMWORD PTR[160+rsp],xmm14
	movaps	XMMWORD PTR[176+rsp],xmm15
$L$ctr32_body::
	cmp	rdx,1
	je	$L$ctr32_one_shortcut

	movdqu	xmm14,XMMWORD PTR[r8]
	movdqa	xmm15,XMMWORD PTR[$L$bswap_mask]
	xor	eax,eax
DB	102,69,15,58,22,242,3
DB	102,68,15,58,34,240,3

	mov	eax,DWORD PTR[240+rcx]
	bswap	r10d
	pxor	xmm12,xmm12
	pxor	xmm13,xmm13
DB	102,69,15,58,34,226,0
	lea	r11,QWORD PTR[3+r10]
DB	102,69,15,58,34,235,0
	inc	r10d
DB	102,69,15,58,34,226,1
	inc	r11
DB	102,69,15,58,34,235,1
	inc	r10d
DB	102,69,15,58,34,226,2
	inc	r11
DB	102,69,15,58,34,235,2
	movdqa	XMMWORD PTR[rsp],xmm12
DB	102,69,15,56,0,231
	movdqa	XMMWORD PTR[16+rsp],xmm13
DB	102,69,15,56,0,239

	pshufd	xmm2,xmm12,192
	pshufd	xmm3,xmm12,128
	pshufd	xmm4,xmm12,64
	cmp	rdx,6
	jb	$L$ctr32_tail
	shr	eax,1
	mov	r11,rcx
	mov	r10d,eax
	sub	rdx,6
	jmp	$L$ctr32_loop6

ALIGN	16
$L$ctr32_loop6::
	pshufd	xmm5,xmm13,192
	por	xmm2,xmm14
	movups	xmm0,XMMWORD PTR[r11]
	pshufd	xmm6,xmm13,128
	por	xmm3,xmm14
	movups	xmm1,XMMWORD PTR[16+r11]
	pshufd	xmm7,xmm13,64
	por	xmm4,xmm14
	por	xmm5,xmm14
	xorps	xmm2,xmm0
	por	xmm6,xmm14
	por	xmm7,xmm14




	pxor	xmm3,xmm0
DB	102,15,56,220,209
	lea	rcx,QWORD PTR[32+r11]
	pxor	xmm4,xmm0
DB	102,15,56,220,217
	movdqa	xmm13,XMMWORD PTR[$L$increment32]
	pxor	xmm5,xmm0
DB	102,15,56,220,225
	movdqa	xmm12,XMMWORD PTR[rsp]
	pxor	xmm6,xmm0
DB	102,15,56,220,233
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
	dec	eax
DB	102,15,56,220,241
DB	102,15,56,220,249
	jmp	$L$ctr32_enc_loop6_enter
ALIGN	16
$L$ctr32_enc_loop6::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
$L$ctr32_enc_loop6_enter::
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$ctr32_enc_loop6

DB	102,15,56,220,209
	paddd	xmm12,xmm13
DB	102,15,56,220,217
	paddd	xmm13,XMMWORD PTR[16+rsp]
DB	102,15,56,220,225
	movdqa	XMMWORD PTR[rsp],xmm12
DB	102,15,56,220,233
	movdqa	XMMWORD PTR[16+rsp],xmm13
DB	102,15,56,220,241
DB	102,69,15,56,0,231
DB	102,15,56,220,249
DB	102,69,15,56,0,239

DB	102,15,56,221,208
	movups	xmm8,XMMWORD PTR[rdi]
DB	102,15,56,221,216
	movups	xmm9,XMMWORD PTR[16+rdi]
DB	102,15,56,221,224
	movups	xmm10,XMMWORD PTR[32+rdi]
DB	102,15,56,221,232
	movups	xmm11,XMMWORD PTR[48+rdi]
DB	102,15,56,221,240
	movups	xmm1,XMMWORD PTR[64+rdi]
DB	102,15,56,221,248
	movups	xmm0,XMMWORD PTR[80+rdi]
	lea	rdi,QWORD PTR[96+rdi]

	xorps	xmm8,xmm2
	pshufd	xmm2,xmm12,192
	xorps	xmm9,xmm3
	pshufd	xmm3,xmm12,128
	movups	XMMWORD PTR[rsi],xmm8
	xorps	xmm10,xmm4
	pshufd	xmm4,xmm12,64
	movups	XMMWORD PTR[16+rsi],xmm9
	xorps	xmm11,xmm5
	movups	XMMWORD PTR[32+rsi],xmm10
	xorps	xmm1,xmm6
	movups	XMMWORD PTR[48+rsi],xmm11
	xorps	xmm0,xmm7
	movups	XMMWORD PTR[64+rsi],xmm1
	movups	XMMWORD PTR[80+rsi],xmm0
	lea	rsi,QWORD PTR[96+rsi]
	mov	eax,r10d
	sub	rdx,6
	jnc	$L$ctr32_loop6

	add	rdx,6
	jz	$L$ctr32_done
	mov	rcx,r11
	lea	eax,DWORD PTR[1+rax*1+rax]

$L$ctr32_tail::
	por	xmm2,xmm14
	movups	xmm8,XMMWORD PTR[rdi]
	cmp	rdx,2
	jb	$L$ctr32_one

	por	xmm3,xmm14
	movups	xmm9,XMMWORD PTR[16+rdi]
	je	$L$ctr32_two

	pshufd	xmm5,xmm13,192
	por	xmm4,xmm14
	movups	xmm10,XMMWORD PTR[32+rdi]
	cmp	rdx,4
	jb	$L$ctr32_three

	pshufd	xmm6,xmm13,128
	por	xmm5,xmm14
	movups	xmm11,XMMWORD PTR[48+rdi]
	je	$L$ctr32_four

	por	xmm6,xmm14
	xorps	xmm7,xmm7

	call	_aesni_encrypt6

	movups	xmm1,XMMWORD PTR[64+rdi]
	xorps	xmm8,xmm2
	xorps	xmm9,xmm3
	movups	XMMWORD PTR[rsi],xmm8
	xorps	xmm10,xmm4
	movups	XMMWORD PTR[16+rsi],xmm9
	xorps	xmm11,xmm5
	movups	XMMWORD PTR[32+rsi],xmm10
	xorps	xmm1,xmm6
	movups	XMMWORD PTR[48+rsi],xmm11
	movups	XMMWORD PTR[64+rsi],xmm1
	jmp	$L$ctr32_done

ALIGN	16
$L$ctr32_one_shortcut::
	movups	xmm2,XMMWORD PTR[r8]
	movups	xmm8,XMMWORD PTR[rdi]
	mov	eax,DWORD PTR[240+rcx]
$L$ctr32_one::
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_7::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_7

DB	102,15,56,221,209
	xorps	xmm8,xmm2
	movups	XMMWORD PTR[rsi],xmm8
	jmp	$L$ctr32_done

ALIGN	16
$L$ctr32_two::
	xorps	xmm4,xmm4
	call	_aesni_encrypt3
	xorps	xmm8,xmm2
	xorps	xmm9,xmm3
	movups	XMMWORD PTR[rsi],xmm8
	movups	XMMWORD PTR[16+rsi],xmm9
	jmp	$L$ctr32_done

ALIGN	16
$L$ctr32_three::
	call	_aesni_encrypt3
	xorps	xmm8,xmm2
	xorps	xmm9,xmm3
	movups	XMMWORD PTR[rsi],xmm8
	xorps	xmm10,xmm4
	movups	XMMWORD PTR[16+rsi],xmm9
	movups	XMMWORD PTR[32+rsi],xmm10
	jmp	$L$ctr32_done

ALIGN	16
$L$ctr32_four::
	call	_aesni_encrypt4
	xorps	xmm8,xmm2
	xorps	xmm9,xmm3
	movups	XMMWORD PTR[rsi],xmm8
	xorps	xmm10,xmm4
	movups	XMMWORD PTR[16+rsi],xmm9
	xorps	xmm11,xmm5
	movups	XMMWORD PTR[32+rsi],xmm10
	movups	XMMWORD PTR[48+rsi],xmm11

$L$ctr32_done::
	movaps	xmm6,XMMWORD PTR[32+rsp]
	movaps	xmm7,XMMWORD PTR[48+rsp]
	movaps	xmm8,XMMWORD PTR[64+rsp]
	movaps	xmm9,XMMWORD PTR[80+rsp]
	movaps	xmm10,XMMWORD PTR[96+rsp]
	movaps	xmm11,XMMWORD PTR[112+rsp]
	movaps	xmm12,XMMWORD PTR[128+rsp]
	movaps	xmm13,XMMWORD PTR[144+rsp]
	movaps	xmm14,XMMWORD PTR[160+rsp]
	movaps	xmm15,XMMWORD PTR[176+rsp]
	lea	rsp,QWORD PTR[200+rsp]
$L$ctr32_ret::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_ctr32_encrypt_blocks::
aesni_ctr32_encrypt_blocks	ENDP
PUBLIC	aesni_xts_encrypt

ALIGN	16
aesni_xts_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	lea	rsp,QWORD PTR[((-264))+rsp]
	movaps	XMMWORD PTR[96+rsp],xmm6
	movaps	XMMWORD PTR[112+rsp],xmm7
	movaps	XMMWORD PTR[128+rsp],xmm8
	movaps	XMMWORD PTR[144+rsp],xmm9
	movaps	XMMWORD PTR[160+rsp],xmm10
	movaps	XMMWORD PTR[176+rsp],xmm11
	movaps	XMMWORD PTR[192+rsp],xmm12
	movaps	XMMWORD PTR[208+rsp],xmm13
	movaps	XMMWORD PTR[224+rsp],xmm14
	movaps	XMMWORD PTR[240+rsp],xmm15
$L$xts_enc_body::
	movups	xmm15,XMMWORD PTR[r9]
	mov	eax,DWORD PTR[240+r8]
	mov	r10d,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm15,xmm0
$L$oop_enc1_8::
DB	102,68,15,56,220,249
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_enc1_8

DB	102,68,15,56,221,249
	mov	r11,rcx
	mov	eax,r10d
	mov	r9,rdx
	and	rdx,-16

	movdqa	xmm8,XMMWORD PTR[$L$xts_magic]
	pxor	xmm14,xmm14
	pcmpgtd	xmm14,xmm15
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm10,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm11,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm12,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm13,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	sub	rdx,16*6
	jc	$L$xts_enc_short

	shr	eax,1
	sub	eax,1
	mov	r10d,eax
	jmp	$L$xts_enc_grandloop

ALIGN	16
$L$xts_enc_grandloop::
	pshufd	xmm9,xmm14,013h
	movdqa	xmm14,xmm15
	paddq	xmm15,xmm15
	movdqu	xmm2,XMMWORD PTR[rdi]
	pand	xmm9,xmm8
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm15,xmm9

	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm3,xmm11
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	pxor	xmm4,xmm12
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	lea	rdi,QWORD PTR[96+rdi]
	pxor	xmm5,xmm13
	movups	xmm0,XMMWORD PTR[r11]
	pxor	xmm6,xmm14
	pxor	xmm7,xmm15



	movups	xmm1,XMMWORD PTR[16+r11]
	pxor	xmm2,xmm0
	pxor	xmm3,xmm0
	movdqa	XMMWORD PTR[rsp],xmm10
DB	102,15,56,220,209
	lea	rcx,QWORD PTR[32+r11]
	pxor	xmm4,xmm0
	movdqa	XMMWORD PTR[16+rsp],xmm11
DB	102,15,56,220,217
	pxor	xmm5,xmm0
	movdqa	XMMWORD PTR[32+rsp],xmm12
DB	102,15,56,220,225
	pxor	xmm6,xmm0
	movdqa	XMMWORD PTR[48+rsp],xmm13
DB	102,15,56,220,233
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
	dec	eax
	movdqa	XMMWORD PTR[64+rsp],xmm14
DB	102,15,56,220,241
	movdqa	XMMWORD PTR[80+rsp],xmm15
DB	102,15,56,220,249
	pxor	xmm14,xmm14
	pcmpgtd	xmm14,xmm15
	jmp	$L$xts_enc_loop6_enter

ALIGN	16
$L$xts_enc_loop6::
DB	102,15,56,220,209
DB	102,15,56,220,217
	dec	eax
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
$L$xts_enc_loop6_enter::
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,220,208
DB	102,15,56,220,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$xts_enc_loop6

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	paddq	xmm15,xmm15
DB	102,15,56,220,209
	pand	xmm9,xmm8
DB	102,15,56,220,217
	pcmpgtd	xmm14,xmm15
DB	102,15,56,220,225
	pxor	xmm15,xmm9
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[16+rcx]

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm10,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,220,208
	pand	xmm9,xmm8
DB	102,15,56,220,216
	pcmpgtd	xmm14,xmm15
DB	102,15,56,220,224
	pxor	xmm15,xmm9
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[32+rcx]

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm11,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,220,209
	pand	xmm9,xmm8
DB	102,15,56,220,217
	pcmpgtd	xmm14,xmm15
DB	102,15,56,220,225
	pxor	xmm15,xmm9
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm12,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,221,208
	pand	xmm9,xmm8
DB	102,15,56,221,216
	pcmpgtd	xmm14,xmm15
DB	102,15,56,221,224
	pxor	xmm15,xmm9
DB	102,15,56,221,232
DB	102,15,56,221,240
DB	102,15,56,221,248

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm13,xmm15
	paddq	xmm15,xmm15
	xorps	xmm2,XMMWORD PTR[rsp]
	pand	xmm9,xmm8
	xorps	xmm3,XMMWORD PTR[16+rsp]
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9

	xorps	xmm4,XMMWORD PTR[32+rsp]
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,XMMWORD PTR[48+rsp]
	movups	XMMWORD PTR[16+rsi],xmm3
	xorps	xmm6,XMMWORD PTR[64+rsp]
	movups	XMMWORD PTR[32+rsi],xmm4
	xorps	xmm7,XMMWORD PTR[80+rsp]
	movups	XMMWORD PTR[48+rsi],xmm5
	mov	eax,r10d
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	lea	rsi,QWORD PTR[96+rsi]
	sub	rdx,16*6
	jnc	$L$xts_enc_grandloop

	lea	eax,DWORD PTR[3+rax*1+rax]
	mov	rcx,r11
	mov	r10d,eax

$L$xts_enc_short::
	add	rdx,16*6
	jz	$L$xts_enc_done

	cmp	rdx,020h
	jb	$L$xts_enc_one
	je	$L$xts_enc_two

	cmp	rdx,040h
	jb	$L$xts_enc_three
	je	$L$xts_enc_four

	pshufd	xmm9,xmm14,013h
	movdqa	xmm14,xmm15
	paddq	xmm15,xmm15
	movdqu	xmm2,XMMWORD PTR[rdi]
	pand	xmm9,xmm8
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm15,xmm9

	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm3,xmm11
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	lea	rdi,QWORD PTR[80+rdi]
	pxor	xmm4,xmm12
	pxor	xmm5,xmm13
	pxor	xmm6,xmm14

	call	_aesni_encrypt6

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm15
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	movdqu	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,xmm13
	movdqu	XMMWORD PTR[16+rsi],xmm3
	xorps	xmm6,xmm14
	movdqu	XMMWORD PTR[32+rsi],xmm4
	movdqu	XMMWORD PTR[48+rsi],xmm5
	movdqu	XMMWORD PTR[64+rsi],xmm6
	lea	rsi,QWORD PTR[80+rsi]
	jmp	$L$xts_enc_done

ALIGN	16
$L$xts_enc_one::
	movups	xmm2,XMMWORD PTR[rdi]
	lea	rdi,QWORD PTR[16+rdi]
	xorps	xmm2,xmm10
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_9::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_9

DB	102,15,56,221,209
	xorps	xmm2,xmm10
	movdqa	xmm10,xmm11
	movups	XMMWORD PTR[rsi],xmm2
	lea	rsi,QWORD PTR[16+rsi]
	jmp	$L$xts_enc_done

ALIGN	16
$L$xts_enc_two::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	lea	rdi,QWORD PTR[32+rdi]
	xorps	xmm2,xmm10
	xorps	xmm3,xmm11

	call	_aesni_encrypt3

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm12
	xorps	xmm3,xmm11
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	lea	rsi,QWORD PTR[32+rsi]
	jmp	$L$xts_enc_done

ALIGN	16
$L$xts_enc_three::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	movups	xmm4,XMMWORD PTR[32+rdi]
	lea	rdi,QWORD PTR[48+rdi]
	xorps	xmm2,xmm10
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12

	call	_aesni_encrypt3

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm13
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	lea	rsi,QWORD PTR[48+rsi]
	jmp	$L$xts_enc_done

ALIGN	16
$L$xts_enc_four::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	movups	xmm4,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm10
	movups	xmm5,XMMWORD PTR[48+rdi]
	lea	rdi,QWORD PTR[64+rdi]
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	xorps	xmm5,xmm13

	call	_aesni_encrypt4

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm15
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,xmm13
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	lea	rsi,QWORD PTR[64+rsi]
	jmp	$L$xts_enc_done

ALIGN	16
$L$xts_enc_done::
	and	r9,15
	jz	$L$xts_enc_ret
	mov	rdx,r9

$L$xts_enc_steal::
	movzx	eax,BYTE PTR[rdi]
	movzx	ecx,BYTE PTR[((-16))+rsi]
	lea	rdi,QWORD PTR[1+rdi]
	mov	BYTE PTR[((-16))+rsi],al
	mov	BYTE PTR[rsi],cl
	lea	rsi,QWORD PTR[1+rsi]
	sub	rdx,1
	jnz	$L$xts_enc_steal

	sub	rsi,r9
	mov	rcx,r11
	mov	eax,r10d

	movups	xmm2,XMMWORD PTR[((-16))+rsi]
	xorps	xmm2,xmm10
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_10::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_10

DB	102,15,56,221,209
	xorps	xmm2,xmm10
	movups	XMMWORD PTR[(-16)+rsi],xmm2

$L$xts_enc_ret::
	movaps	xmm6,XMMWORD PTR[96+rsp]
	movaps	xmm7,XMMWORD PTR[112+rsp]
	movaps	xmm8,XMMWORD PTR[128+rsp]
	movaps	xmm9,XMMWORD PTR[144+rsp]
	movaps	xmm10,XMMWORD PTR[160+rsp]
	movaps	xmm11,XMMWORD PTR[176+rsp]
	movaps	xmm12,XMMWORD PTR[192+rsp]
	movaps	xmm13,XMMWORD PTR[208+rsp]
	movaps	xmm14,XMMWORD PTR[224+rsp]
	movaps	xmm15,XMMWORD PTR[240+rsp]
	lea	rsp,QWORD PTR[264+rsp]
$L$xts_enc_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_xts_encrypt::
aesni_xts_encrypt	ENDP
PUBLIC	aesni_xts_decrypt

ALIGN	16
aesni_xts_decrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_xts_decrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	lea	rsp,QWORD PTR[((-264))+rsp]
	movaps	XMMWORD PTR[96+rsp],xmm6
	movaps	XMMWORD PTR[112+rsp],xmm7
	movaps	XMMWORD PTR[128+rsp],xmm8
	movaps	XMMWORD PTR[144+rsp],xmm9
	movaps	XMMWORD PTR[160+rsp],xmm10
	movaps	XMMWORD PTR[176+rsp],xmm11
	movaps	XMMWORD PTR[192+rsp],xmm12
	movaps	XMMWORD PTR[208+rsp],xmm13
	movaps	XMMWORD PTR[224+rsp],xmm14
	movaps	XMMWORD PTR[240+rsp],xmm15
$L$xts_dec_body::
	movups	xmm15,XMMWORD PTR[r9]
	mov	eax,DWORD PTR[240+r8]
	mov	r10d,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm15,xmm0
$L$oop_enc1_11::
DB	102,68,15,56,220,249
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_enc1_11

DB	102,68,15,56,221,249
	xor	eax,eax
	test	rdx,15
	setnz	al
	shl	rax,4
	sub	rdx,rax

	mov	r11,rcx
	mov	eax,r10d
	mov	r9,rdx
	and	rdx,-16

	movdqa	xmm8,XMMWORD PTR[$L$xts_magic]
	pxor	xmm14,xmm14
	pcmpgtd	xmm14,xmm15
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm10,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm11,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm12,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm13,xmm15
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9
	sub	rdx,16*6
	jc	$L$xts_dec_short

	shr	eax,1
	sub	eax,1
	mov	r10d,eax
	jmp	$L$xts_dec_grandloop

ALIGN	16
$L$xts_dec_grandloop::
	pshufd	xmm9,xmm14,013h
	movdqa	xmm14,xmm15
	paddq	xmm15,xmm15
	movdqu	xmm2,XMMWORD PTR[rdi]
	pand	xmm9,xmm8
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm15,xmm9

	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm3,xmm11
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	pxor	xmm4,xmm12
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	lea	rdi,QWORD PTR[96+rdi]
	pxor	xmm5,xmm13
	movups	xmm0,XMMWORD PTR[r11]
	pxor	xmm6,xmm14
	pxor	xmm7,xmm15



	movups	xmm1,XMMWORD PTR[16+r11]
	pxor	xmm2,xmm0
	pxor	xmm3,xmm0
	movdqa	XMMWORD PTR[rsp],xmm10
DB	102,15,56,222,209
	lea	rcx,QWORD PTR[32+r11]
	pxor	xmm4,xmm0
	movdqa	XMMWORD PTR[16+rsp],xmm11
DB	102,15,56,222,217
	pxor	xmm5,xmm0
	movdqa	XMMWORD PTR[32+rsp],xmm12
DB	102,15,56,222,225
	pxor	xmm6,xmm0
	movdqa	XMMWORD PTR[48+rsp],xmm13
DB	102,15,56,222,233
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
	dec	eax
	movdqa	XMMWORD PTR[64+rsp],xmm14
DB	102,15,56,222,241
	movdqa	XMMWORD PTR[80+rsp],xmm15
DB	102,15,56,222,249
	pxor	xmm14,xmm14
	pcmpgtd	xmm14,xmm15
	jmp	$L$xts_dec_loop6_enter

ALIGN	16
$L$xts_dec_loop6::
DB	102,15,56,222,209
DB	102,15,56,222,217
	dec	eax
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
$L$xts_dec_loop6_enter::
	movups	xmm1,XMMWORD PTR[16+rcx]
DB	102,15,56,222,208
DB	102,15,56,222,216
	lea	rcx,QWORD PTR[32+rcx]
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[rcx]
	jnz	$L$xts_dec_loop6

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	paddq	xmm15,xmm15
DB	102,15,56,222,209
	pand	xmm9,xmm8
DB	102,15,56,222,217
	pcmpgtd	xmm14,xmm15
DB	102,15,56,222,225
	pxor	xmm15,xmm9
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[16+rcx]

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm10,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,222,208
	pand	xmm9,xmm8
DB	102,15,56,222,216
	pcmpgtd	xmm14,xmm15
DB	102,15,56,222,224
	pxor	xmm15,xmm9
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[32+rcx]

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm11,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,222,209
	pand	xmm9,xmm8
DB	102,15,56,222,217
	pcmpgtd	xmm14,xmm15
DB	102,15,56,222,225
	pxor	xmm15,xmm9
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm12,xmm15
	paddq	xmm15,xmm15
DB	102,15,56,223,208
	pand	xmm9,xmm8
DB	102,15,56,223,216
	pcmpgtd	xmm14,xmm15
DB	102,15,56,223,224
	pxor	xmm15,xmm9
DB	102,15,56,223,232
DB	102,15,56,223,240
DB	102,15,56,223,248

	pshufd	xmm9,xmm14,013h
	pxor	xmm14,xmm14
	movdqa	xmm13,xmm15
	paddq	xmm15,xmm15
	xorps	xmm2,XMMWORD PTR[rsp]
	pand	xmm9,xmm8
	xorps	xmm3,XMMWORD PTR[16+rsp]
	pcmpgtd	xmm14,xmm15
	pxor	xmm15,xmm9

	xorps	xmm4,XMMWORD PTR[32+rsp]
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,XMMWORD PTR[48+rsp]
	movups	XMMWORD PTR[16+rsi],xmm3
	xorps	xmm6,XMMWORD PTR[64+rsp]
	movups	XMMWORD PTR[32+rsi],xmm4
	xorps	xmm7,XMMWORD PTR[80+rsp]
	movups	XMMWORD PTR[48+rsi],xmm5
	mov	eax,r10d
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	lea	rsi,QWORD PTR[96+rsi]
	sub	rdx,16*6
	jnc	$L$xts_dec_grandloop

	lea	eax,DWORD PTR[3+rax*1+rax]
	mov	rcx,r11
	mov	r10d,eax

$L$xts_dec_short::
	add	rdx,16*6
	jz	$L$xts_dec_done

	cmp	rdx,020h
	jb	$L$xts_dec_one
	je	$L$xts_dec_two

	cmp	rdx,040h
	jb	$L$xts_dec_three
	je	$L$xts_dec_four

	pshufd	xmm9,xmm14,013h
	movdqa	xmm14,xmm15
	paddq	xmm15,xmm15
	movdqu	xmm2,XMMWORD PTR[rdi]
	pand	xmm9,xmm8
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm15,xmm9

	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm3,xmm11
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	lea	rdi,QWORD PTR[80+rdi]
	pxor	xmm4,xmm12
	pxor	xmm5,xmm13
	pxor	xmm6,xmm14

	call	_aesni_decrypt6

	xorps	xmm2,xmm10
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	movdqu	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,xmm13
	movdqu	XMMWORD PTR[16+rsi],xmm3
	xorps	xmm6,xmm14
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm14,xmm14
	movdqu	XMMWORD PTR[48+rsi],xmm5
	pcmpgtd	xmm14,xmm15
	movdqu	XMMWORD PTR[64+rsi],xmm6
	lea	rsi,QWORD PTR[80+rsi]
	pshufd	xmm11,xmm14,013h
	and	r9,15
	jz	$L$xts_dec_ret

	movdqa	xmm10,xmm15
	paddq	xmm15,xmm15
	pand	xmm11,xmm8
	pxor	xmm11,xmm15
	jmp	$L$xts_dec_done2

ALIGN	16
$L$xts_dec_one::
	movups	xmm2,XMMWORD PTR[rdi]
	lea	rdi,QWORD PTR[16+rdi]
	xorps	xmm2,xmm10
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_12::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_12

DB	102,15,56,223,209
	xorps	xmm2,xmm10
	movdqa	xmm10,xmm11
	movups	XMMWORD PTR[rsi],xmm2
	movdqa	xmm11,xmm12
	lea	rsi,QWORD PTR[16+rsi]
	jmp	$L$xts_dec_done

ALIGN	16
$L$xts_dec_two::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	lea	rdi,QWORD PTR[32+rdi]
	xorps	xmm2,xmm10
	xorps	xmm3,xmm11

	call	_aesni_decrypt3

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm12
	xorps	xmm3,xmm11
	movdqa	xmm11,xmm13
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	lea	rsi,QWORD PTR[32+rsi]
	jmp	$L$xts_dec_done

ALIGN	16
$L$xts_dec_three::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	movups	xmm4,XMMWORD PTR[32+rdi]
	lea	rdi,QWORD PTR[48+rdi]
	xorps	xmm2,xmm10
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12

	call	_aesni_decrypt3

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm13
	xorps	xmm3,xmm11
	movdqa	xmm11,xmm15
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	lea	rsi,QWORD PTR[48+rsi]
	jmp	$L$xts_dec_done

ALIGN	16
$L$xts_dec_four::
	pshufd	xmm9,xmm14,013h
	movdqa	xmm14,xmm15
	paddq	xmm15,xmm15
	movups	xmm2,XMMWORD PTR[rdi]
	pand	xmm9,xmm8
	movups	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm15,xmm9

	movups	xmm4,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm10
	movups	xmm5,XMMWORD PTR[48+rdi]
	lea	rdi,QWORD PTR[64+rdi]
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	xorps	xmm5,xmm13

	call	_aesni_decrypt4

	xorps	xmm2,xmm10
	movdqa	xmm10,xmm14
	xorps	xmm3,xmm11
	movdqa	xmm11,xmm15
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm5,xmm13
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	lea	rsi,QWORD PTR[64+rsi]
	jmp	$L$xts_dec_done

ALIGN	16
$L$xts_dec_done::
	and	r9,15
	jz	$L$xts_dec_ret
$L$xts_dec_done2::
	mov	rdx,r9
	mov	rcx,r11
	mov	eax,r10d

	movups	xmm2,XMMWORD PTR[rdi]
	xorps	xmm2,xmm11
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_13::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_13

DB	102,15,56,223,209
	xorps	xmm2,xmm11
	movups	XMMWORD PTR[rsi],xmm2

$L$xts_dec_steal::
	movzx	eax,BYTE PTR[16+rdi]
	movzx	ecx,BYTE PTR[rsi]
	lea	rdi,QWORD PTR[1+rdi]
	mov	BYTE PTR[rsi],al
	mov	BYTE PTR[16+rsi],cl
	lea	rsi,QWORD PTR[1+rsi]
	sub	rdx,1
	jnz	$L$xts_dec_steal

	sub	rsi,r9
	mov	rcx,r11
	mov	eax,r10d

	movups	xmm2,XMMWORD PTR[rsi]
	xorps	xmm2,xmm10
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_14::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_14

DB	102,15,56,223,209
	xorps	xmm2,xmm10
	movups	XMMWORD PTR[rsi],xmm2

$L$xts_dec_ret::
	movaps	xmm6,XMMWORD PTR[96+rsp]
	movaps	xmm7,XMMWORD PTR[112+rsp]
	movaps	xmm8,XMMWORD PTR[128+rsp]
	movaps	xmm9,XMMWORD PTR[144+rsp]
	movaps	xmm10,XMMWORD PTR[160+rsp]
	movaps	xmm11,XMMWORD PTR[176+rsp]
	movaps	xmm12,XMMWORD PTR[192+rsp]
	movaps	xmm13,XMMWORD PTR[208+rsp]
	movaps	xmm14,XMMWORD PTR[224+rsp]
	movaps	xmm15,XMMWORD PTR[240+rsp]
	lea	rsp,QWORD PTR[264+rsp]
$L$xts_dec_epilogue::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_xts_decrypt::
aesni_xts_decrypt	ENDP
PUBLIC	aesni_cbc_encrypt

ALIGN	16
aesni_cbc_encrypt	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_aesni_cbc_encrypt::
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8
	mov	rcx,r9
	mov	r8,QWORD PTR[40+rsp]
	mov	r9,QWORD PTR[48+rsp]


	test	rdx,rdx
	jz	$L$cbc_ret

	mov	r10d,DWORD PTR[240+rcx]
	mov	r11,rcx
	test	r9d,r9d
	jz	$L$cbc_decrypt

	movups	xmm2,XMMWORD PTR[r8]
	mov	eax,r10d
	cmp	rdx,16
	jb	$L$cbc_enc_tail
	sub	rdx,16
	jmp	$L$cbc_enc_loop
ALIGN	16
$L$cbc_enc_loop::
	movups	xmm3,XMMWORD PTR[rdi]
	lea	rdi,QWORD PTR[16+rdi]

	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm3,xmm0
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm3
$L$oop_enc1_15::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_15

DB	102,15,56,221,209
	mov	eax,r10d
	mov	rcx,r11
	movups	XMMWORD PTR[rsi],xmm2
	lea	rsi,QWORD PTR[16+rsi]
	sub	rdx,16
	jnc	$L$cbc_enc_loop
	add	rdx,16
	jnz	$L$cbc_enc_tail
	movups	XMMWORD PTR[r8],xmm2
	jmp	$L$cbc_ret

$L$cbc_enc_tail::
	mov	rcx,rdx
	xchg	rsi,rdi
	DD	09066A4F3h

	mov	ecx,16
	sub	rcx,rdx
	xor	eax,eax
	DD	09066AAF3h

	lea	rdi,QWORD PTR[((-16))+rdi]
	mov	eax,r10d
	mov	rsi,rdi
	mov	rcx,r11
	xor	rdx,rdx
	jmp	$L$cbc_enc_loop


ALIGN	16
$L$cbc_decrypt::
	lea	rsp,QWORD PTR[((-88))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
$L$cbc_decrypt_body::
	movups	xmm9,XMMWORD PTR[r8]
	mov	eax,r10d
	cmp	rdx,070h
	jbe	$L$cbc_dec_tail
	shr	r10d,1
	sub	rdx,070h
	mov	eax,r10d
	movaps	XMMWORD PTR[64+rsp],xmm9
	jmp	$L$cbc_dec_loop8_enter
ALIGN	16
$L$cbc_dec_loop8::
	movaps	XMMWORD PTR[64+rsp],xmm0
	movups	XMMWORD PTR[rsi],xmm9
	lea	rsi,QWORD PTR[16+rsi]
$L$cbc_dec_loop8_enter::
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	movups	xmm1,XMMWORD PTR[16+rcx]

	lea	rcx,QWORD PTR[32+rcx]
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm0
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	xorps	xmm3,xmm0
	movdqu	xmm6,XMMWORD PTR[64+rdi]
DB	102,15,56,222,209
	pxor	xmm4,xmm0
	movdqu	xmm7,XMMWORD PTR[80+rdi]
DB	102,15,56,222,217
	pxor	xmm5,xmm0
	movdqu	xmm8,XMMWORD PTR[96+rdi]
DB	102,15,56,222,225
	pxor	xmm6,xmm0
	movdqu	xmm9,XMMWORD PTR[112+rdi]
DB	102,15,56,222,233
	pxor	xmm7,xmm0
	dec	eax
DB	102,15,56,222,241
	pxor	xmm8,xmm0
DB	102,15,56,222,249
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[rcx]
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[16+rcx]

	call	$L$dec_loop8_enter

	movups	xmm1,XMMWORD PTR[rdi]
	movups	xmm0,XMMWORD PTR[16+rdi]
	xorps	xmm2,XMMWORD PTR[64+rsp]
	xorps	xmm3,xmm1
	movups	xmm1,XMMWORD PTR[32+rdi]
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[48+rdi]
	xorps	xmm5,xmm1
	movups	xmm1,XMMWORD PTR[64+rdi]
	xorps	xmm6,xmm0
	movups	xmm0,XMMWORD PTR[80+rdi]
	xorps	xmm7,xmm1
	movups	xmm1,XMMWORD PTR[96+rdi]
	xorps	xmm8,xmm0
	movups	xmm0,XMMWORD PTR[112+rdi]
	xorps	xmm9,xmm1
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	mov	eax,r10d
	movups	XMMWORD PTR[64+rsi],xmm6
	mov	rcx,r11
	movups	XMMWORD PTR[80+rsi],xmm7
	lea	rdi,QWORD PTR[128+rdi]
	movups	XMMWORD PTR[96+rsi],xmm8
	lea	rsi,QWORD PTR[112+rsi]
	sub	rdx,080h
	ja	$L$cbc_dec_loop8

	movaps	xmm2,xmm9
	movaps	xmm9,xmm0
	add	rdx,070h
	jle	$L$cbc_dec_tail_collected
	movups	XMMWORD PTR[rsi],xmm2
	lea	eax,DWORD PTR[1+r10*1+r10]
	lea	rsi,QWORD PTR[16+rsi]
$L$cbc_dec_tail::
	movups	xmm2,XMMWORD PTR[rdi]
	movaps	xmm8,xmm2
	cmp	rdx,010h
	jbe	$L$cbc_dec_one

	movups	xmm3,XMMWORD PTR[16+rdi]
	movaps	xmm7,xmm3
	cmp	rdx,020h
	jbe	$L$cbc_dec_two

	movups	xmm4,XMMWORD PTR[32+rdi]
	movaps	xmm6,xmm4
	cmp	rdx,030h
	jbe	$L$cbc_dec_three

	movups	xmm5,XMMWORD PTR[48+rdi]
	cmp	rdx,040h
	jbe	$L$cbc_dec_four

	movups	xmm6,XMMWORD PTR[64+rdi]
	cmp	rdx,050h
	jbe	$L$cbc_dec_five

	movups	xmm7,XMMWORD PTR[80+rdi]
	cmp	rdx,060h
	jbe	$L$cbc_dec_six

	movups	xmm8,XMMWORD PTR[96+rdi]
	movaps	XMMWORD PTR[64+rsp],xmm9
	call	_aesni_decrypt8
	movups	xmm1,XMMWORD PTR[rdi]
	movups	xmm0,XMMWORD PTR[16+rdi]
	xorps	xmm2,XMMWORD PTR[64+rsp]
	xorps	xmm3,xmm1
	movups	xmm1,XMMWORD PTR[32+rdi]
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[48+rdi]
	xorps	xmm5,xmm1
	movups	xmm1,XMMWORD PTR[64+rdi]
	xorps	xmm6,xmm0
	movups	xmm0,XMMWORD PTR[80+rdi]
	xorps	xmm7,xmm1
	movups	xmm9,XMMWORD PTR[96+rdi]
	xorps	xmm8,xmm0
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	lea	rsi,QWORD PTR[96+rsi]
	movaps	xmm2,xmm8
	sub	rdx,070h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_one::
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_16::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_16

DB	102,15,56,223,209
	xorps	xmm2,xmm9
	movaps	xmm9,xmm8
	sub	rdx,010h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_two::
	xorps	xmm4,xmm4
	call	_aesni_decrypt3
	xorps	xmm2,xmm9
	xorps	xmm3,xmm8
	movups	XMMWORD PTR[rsi],xmm2
	movaps	xmm9,xmm7
	movaps	xmm2,xmm3
	lea	rsi,QWORD PTR[16+rsi]
	sub	rdx,020h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_three::
	call	_aesni_decrypt3
	xorps	xmm2,xmm9
	xorps	xmm3,xmm8
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm4,xmm7
	movups	XMMWORD PTR[16+rsi],xmm3
	movaps	xmm9,xmm6
	movaps	xmm2,xmm4
	lea	rsi,QWORD PTR[32+rsi]
	sub	rdx,030h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_four::
	call	_aesni_decrypt4
	xorps	xmm2,xmm9
	movups	xmm9,XMMWORD PTR[48+rdi]
	xorps	xmm3,xmm8
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm4,xmm7
	movups	XMMWORD PTR[16+rsi],xmm3
	xorps	xmm5,xmm6
	movups	XMMWORD PTR[32+rsi],xmm4
	movaps	xmm2,xmm5
	lea	rsi,QWORD PTR[48+rsi]
	sub	rdx,040h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_five::
	xorps	xmm7,xmm7
	call	_aesni_decrypt6
	movups	xmm1,XMMWORD PTR[16+rdi]
	movups	xmm0,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm9
	xorps	xmm3,xmm8
	xorps	xmm4,xmm1
	movups	xmm1,XMMWORD PTR[48+rdi]
	xorps	xmm5,xmm0
	movups	xmm9,XMMWORD PTR[64+rdi]
	xorps	xmm6,xmm1
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	lea	rsi,QWORD PTR[64+rsi]
	movaps	xmm2,xmm6
	sub	rdx,050h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_six::
	call	_aesni_decrypt6
	movups	xmm1,XMMWORD PTR[16+rdi]
	movups	xmm0,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm9
	xorps	xmm3,xmm8
	xorps	xmm4,xmm1
	movups	xmm1,XMMWORD PTR[48+rdi]
	xorps	xmm5,xmm0
	movups	xmm0,XMMWORD PTR[64+rdi]
	xorps	xmm6,xmm1
	movups	xmm9,XMMWORD PTR[80+rdi]
	xorps	xmm7,xmm0
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	lea	rsi,QWORD PTR[80+rsi]
	movaps	xmm2,xmm7
	sub	rdx,060h
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_tail_collected::
	and	rdx,15
	movups	XMMWORD PTR[r8],xmm9
	jnz	$L$cbc_dec_tail_partial
	movups	XMMWORD PTR[rsi],xmm2
	jmp	$L$cbc_dec_ret
ALIGN	16
$L$cbc_dec_tail_partial::
	movaps	XMMWORD PTR[64+rsp],xmm2
	mov	rcx,16
	mov	rdi,rsi
	sub	rcx,rdx
	lea	rsi,QWORD PTR[64+rsp]
	DD	09066A4F3h


$L$cbc_dec_ret::
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	xmm9,XMMWORD PTR[48+rsp]
	lea	rsp,QWORD PTR[88+rsp]
$L$cbc_ret::
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_aesni_cbc_encrypt::
aesni_cbc_encrypt	ENDP
PUBLIC	aesni_set_decrypt_key

ALIGN	16
aesni_set_decrypt_key	PROC PUBLIC
DB	048h,083h,0ECh,008h

	call	__aesni_set_encrypt_key
	shl	edx,4
	test	eax,eax
	jnz	$L$dec_key_ret
	lea	rcx,QWORD PTR[16+rdx*1+r8]

	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[rcx]
	movups	XMMWORD PTR[rcx],xmm0
	movups	XMMWORD PTR[r8],xmm1
	lea	r8,QWORD PTR[16+r8]
	lea	rcx,QWORD PTR[((-16))+rcx]

$L$dec_key_inverse::
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[rcx]
DB	102,15,56,219,192
DB	102,15,56,219,201
	lea	r8,QWORD PTR[16+r8]
	lea	rcx,QWORD PTR[((-16))+rcx]
	movups	XMMWORD PTR[16+rcx],xmm0
	movups	XMMWORD PTR[(-16)+r8],xmm1
	cmp	rcx,r8
	ja	$L$dec_key_inverse

	movups	xmm0,XMMWORD PTR[r8]
DB	102,15,56,219,192
	movups	XMMWORD PTR[rcx],xmm0
$L$dec_key_ret::
	add	rsp,8
	DB	0F3h,0C3h		;repret
$L$SEH_end_set_decrypt_key::
aesni_set_decrypt_key	ENDP
PUBLIC	aesni_set_encrypt_key

ALIGN	16
aesni_set_encrypt_key	PROC PUBLIC
__aesni_set_encrypt_key::
DB	048h,083h,0ECh,008h

	mov	rax,-1
	test	rcx,rcx
	jz	$L$enc_key_ret
	test	r8,r8
	jz	$L$enc_key_ret

	movups	xmm0,XMMWORD PTR[rcx]
	xorps	xmm4,xmm4
	lea	rax,QWORD PTR[16+r8]
	cmp	edx,256
	je	$L$14rounds
	cmp	edx,192
	je	$L$12rounds
	cmp	edx,128
	jne	$L$bad_keybits

$L$10rounds::
	mov	edx,9
	movups	XMMWORD PTR[r8],xmm0
DB	102,15,58,223,200,1
	call	$L$key_expansion_128_cold
DB	102,15,58,223,200,2
	call	$L$key_expansion_128
DB	102,15,58,223,200,4
	call	$L$key_expansion_128
DB	102,15,58,223,200,8
	call	$L$key_expansion_128
DB	102,15,58,223,200,16
	call	$L$key_expansion_128
DB	102,15,58,223,200,32
	call	$L$key_expansion_128
DB	102,15,58,223,200,64
	call	$L$key_expansion_128
DB	102,15,58,223,200,128
	call	$L$key_expansion_128
DB	102,15,58,223,200,27
	call	$L$key_expansion_128
DB	102,15,58,223,200,54
	call	$L$key_expansion_128
	movups	XMMWORD PTR[rax],xmm0
	mov	DWORD PTR[80+rax],edx
	xor	eax,eax
	jmp	$L$enc_key_ret

ALIGN	16
$L$12rounds::
	movq	xmm2,QWORD PTR[16+rcx]
	mov	edx,11
	movups	XMMWORD PTR[r8],xmm0
DB	102,15,58,223,202,1
	call	$L$key_expansion_192a_cold
DB	102,15,58,223,202,2
	call	$L$key_expansion_192b
DB	102,15,58,223,202,4
	call	$L$key_expansion_192a
DB	102,15,58,223,202,8
	call	$L$key_expansion_192b
DB	102,15,58,223,202,16
	call	$L$key_expansion_192a
DB	102,15,58,223,202,32
	call	$L$key_expansion_192b
DB	102,15,58,223,202,64
	call	$L$key_expansion_192a
DB	102,15,58,223,202,128
	call	$L$key_expansion_192b
	movups	XMMWORD PTR[rax],xmm0
	mov	DWORD PTR[48+rax],edx
	xor	rax,rax
	jmp	$L$enc_key_ret

ALIGN	16
$L$14rounds::
	movups	xmm2,XMMWORD PTR[16+rcx]
	mov	edx,13
	lea	rax,QWORD PTR[16+rax]
	movups	XMMWORD PTR[r8],xmm0
	movups	XMMWORD PTR[16+r8],xmm2
DB	102,15,58,223,202,1
	call	$L$key_expansion_256a_cold
DB	102,15,58,223,200,1
	call	$L$key_expansion_256b
DB	102,15,58,223,202,2
	call	$L$key_expansion_256a
DB	102,15,58,223,200,2
	call	$L$key_expansion_256b
DB	102,15,58,223,202,4
	call	$L$key_expansion_256a
DB	102,15,58,223,200,4
	call	$L$key_expansion_256b
DB	102,15,58,223,202,8
	call	$L$key_expansion_256a
DB	102,15,58,223,200,8
	call	$L$key_expansion_256b
DB	102,15,58,223,202,16
	call	$L$key_expansion_256a
DB	102,15,58,223,200,16
	call	$L$key_expansion_256b
DB	102,15,58,223,202,32
	call	$L$key_expansion_256a
DB	102,15,58,223,200,32
	call	$L$key_expansion_256b
DB	102,15,58,223,202,64
	call	$L$key_expansion_256a
	movups	XMMWORD PTR[rax],xmm0
	mov	DWORD PTR[16+rax],edx
	xor	rax,rax
	jmp	$L$enc_key_ret

ALIGN	16
$L$bad_keybits::
	mov	rax,-2
$L$enc_key_ret::
	add	rsp,8
	DB	0F3h,0C3h		;repret
$L$SEH_end_set_encrypt_key::

ALIGN	16
$L$key_expansion_128::
	movups	XMMWORD PTR[rax],xmm0
	lea	rax,QWORD PTR[16+rax]
$L$key_expansion_128_cold::
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	DB	0F3h,0C3h		;repret

ALIGN	16
$L$key_expansion_192a::
	movups	XMMWORD PTR[rax],xmm0
	lea	rax,QWORD PTR[16+rax]
$L$key_expansion_192a_cold::
	movaps	xmm5,xmm2
$L$key_expansion_192b_warm::
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
	DB	0F3h,0C3h		;repret

ALIGN	16
$L$key_expansion_192b::
	movaps	xmm3,xmm0
	shufps	xmm5,xmm0,68
	movups	XMMWORD PTR[rax],xmm5
	shufps	xmm3,xmm2,78
	movups	XMMWORD PTR[16+rax],xmm3
	lea	rax,QWORD PTR[32+rax]
	jmp	$L$key_expansion_192b_warm

ALIGN	16
$L$key_expansion_256a::
	movups	XMMWORD PTR[rax],xmm2
	lea	rax,QWORD PTR[16+rax]
$L$key_expansion_256a_cold::
	shufps	xmm4,xmm0,16
	xorps	xmm0,xmm4
	shufps	xmm4,xmm0,140
	xorps	xmm0,xmm4
	shufps	xmm1,xmm1,255
	xorps	xmm0,xmm1
	DB	0F3h,0C3h		;repret

ALIGN	16
$L$key_expansion_256b::
	movups	XMMWORD PTR[rax],xmm0
	lea	rax,QWORD PTR[16+rax]

	shufps	xmm4,xmm2,16
	xorps	xmm2,xmm4
	shufps	xmm4,xmm2,140
	xorps	xmm2,xmm4
	shufps	xmm1,xmm1,170
	xorps	xmm2,xmm1
	DB	0F3h,0C3h		;repret
aesni_set_encrypt_key	ENDP

ALIGN	64
$L$bswap_mask::
DB	15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
$L$increment32::
	DD	6,6,6,0
$L$increment64::
	DD	1,0,0,0
$L$xts_magic::
	DD	087h,0,1,0

DB	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69
DB	83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
ALIGN	64
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
ecb_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[152+r8]

	jmp	$L$common_seh_tail
ecb_se_handler	ENDP


ALIGN	16
ccm64_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,8
	DD	0a548f3fch

	lea	rax,QWORD PTR[88+rax]

	jmp	$L$common_seh_tail
ccm64_se_handler	ENDP


ALIGN	16
ctr32_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	lea	r10,QWORD PTR[$L$ctr32_body]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	lea	r10,QWORD PTR[$L$ctr32_ret]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[32+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

	lea	rax,QWORD PTR[200+rax]

	jmp	$L$common_seh_tail
ctr32_se_handler	ENDP


ALIGN	16
xts_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[120+r8]
	mov	rbx,QWORD PTR[248+r8]

	mov	rsi,QWORD PTR[8+r9]
	mov	r11,QWORD PTR[56+r9]

	mov	r10d,DWORD PTR[r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	mov	rax,QWORD PTR[152+r8]

	mov	r10d,DWORD PTR[4+r11]
	lea	r10,QWORD PTR[r10*1+rsi]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[96+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

	lea	rax,QWORD PTR[((104+160))+rax]

	jmp	$L$common_seh_tail
xts_se_handler	ENDP

ALIGN	16
cbc_se_handler	PROC PRIVATE
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD PTR[152+r8]
	mov	rbx,QWORD PTR[248+r8]

	lea	r10,QWORD PTR[$L$cbc_decrypt]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	lea	r10,QWORD PTR[$L$cbc_decrypt_body]
	cmp	rbx,r10
	jb	$L$restore_cbc_rax

	lea	r10,QWORD PTR[$L$cbc_ret]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,8
	DD	0a548f3fch

	lea	rax,QWORD PTR[88+rax]
	jmp	$L$common_seh_tail

$L$restore_cbc_rax::
	mov	rax,QWORD PTR[120+r8]

$L$common_seh_tail::
	mov	rdi,QWORD PTR[8+rax]
	mov	rsi,QWORD PTR[16+rax]
	mov	QWORD PTR[152+r8],rax
	mov	QWORD PTR[168+r8],rsi
	mov	QWORD PTR[176+r8],rdi

	mov	rdi,QWORD PTR[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0a548f3fch


	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD PTR[8+rsi]
	mov	r8,QWORD PTR[rsi]
	mov	r9,QWORD PTR[16+rsi]
	mov	r10,QWORD PTR[40+rsi]
	lea	r11,QWORD PTR[56+rsi]
	lea	r12,QWORD PTR[24+rsi]
	mov	QWORD PTR[32+rsp],r10
	mov	QWORD PTR[40+rsp],r11
	mov	QWORD PTR[48+rsp],r12
	mov	QWORD PTR[56+rsp],rcx
	call	QWORD PTR[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret
cbc_se_handler	ENDP

.text$	ENDS
.pdata	SEGMENT READONLY ALIGN(4)
ALIGN	4
	DD	imagerel $L$SEH_begin_aesni_ecb_encrypt
	DD	imagerel $L$SEH_end_aesni_ecb_encrypt
	DD	imagerel $L$SEH_info_ecb

	DD	imagerel $L$SEH_begin_aesni_ccm64_encrypt_blocks
	DD	imagerel $L$SEH_end_aesni_ccm64_encrypt_blocks
	DD	imagerel $L$SEH_info_ccm64_enc

	DD	imagerel $L$SEH_begin_aesni_ccm64_decrypt_blocks
	DD	imagerel $L$SEH_end_aesni_ccm64_decrypt_blocks
	DD	imagerel $L$SEH_info_ccm64_dec

	DD	imagerel $L$SEH_begin_aesni_ctr32_encrypt_blocks
	DD	imagerel $L$SEH_end_aesni_ctr32_encrypt_blocks
	DD	imagerel $L$SEH_info_ctr32

	DD	imagerel $L$SEH_begin_aesni_xts_encrypt
	DD	imagerel $L$SEH_end_aesni_xts_encrypt
	DD	imagerel $L$SEH_info_xts_enc

	DD	imagerel $L$SEH_begin_aesni_xts_decrypt
	DD	imagerel $L$SEH_end_aesni_xts_decrypt
	DD	imagerel $L$SEH_info_xts_dec
	DD	imagerel $L$SEH_begin_aesni_cbc_encrypt
	DD	imagerel $L$SEH_end_aesni_cbc_encrypt
	DD	imagerel $L$SEH_info_cbc

	DD	imagerel aesni_set_decrypt_key
	DD	imagerel $L$SEH_end_set_decrypt_key
	DD	imagerel $L$SEH_info_key

	DD	imagerel aesni_set_encrypt_key
	DD	imagerel $L$SEH_end_set_encrypt_key
	DD	imagerel $L$SEH_info_key
.pdata	ENDS
.xdata	SEGMENT READONLY ALIGN(8)
ALIGN	8
$L$SEH_info_ecb::
DB	9,0,0,0
	DD	imagerel ecb_se_handler
$L$SEH_info_ccm64_enc::
DB	9,0,0,0
	DD	imagerel ccm64_se_handler
	DD	imagerel $L$ccm64_enc_body,imagerel $L$ccm64_enc_ret

$L$SEH_info_ccm64_dec::
DB	9,0,0,0
	DD	imagerel ccm64_se_handler
	DD	imagerel $L$ccm64_dec_body,imagerel $L$ccm64_dec_ret

$L$SEH_info_ctr32::
DB	9,0,0,0
	DD	imagerel ctr32_se_handler
$L$SEH_info_xts_enc::
DB	9,0,0,0
	DD	imagerel xts_se_handler
	DD	imagerel $L$xts_enc_body,imagerel $L$xts_enc_epilogue

$L$SEH_info_xts_dec::
DB	9,0,0,0
	DD	imagerel xts_se_handler
	DD	imagerel $L$xts_dec_body,imagerel $L$xts_dec_epilogue

$L$SEH_info_cbc::
DB	9,0,0,0
	DD	imagerel cbc_se_handler
$L$SEH_info_key::
DB	001h,004h,001h,000h
DB	004h,002h,000h,000h


.xdata	ENDS
END
