OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'
EXTERN	OPENSSL_ia32cap_P:NEAR
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
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	XMMWORD PTR[rdx],xmm2
	pxor	xmm2,xmm2
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
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	XMMWORD PTR[rdx],xmm2
	pxor	xmm2,xmm2
	DB	0F3h,0C3h		;repret
aesni_decrypt	ENDP

ALIGN	16
_aesni_encrypt2	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
	add	rax,16

$L$enc_loop2::
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
	jnz	$L$enc_loop2

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,221,208
DB	102,15,56,221,216
	DB	0F3h,0C3h		;repret
_aesni_encrypt2	ENDP

ALIGN	16
_aesni_decrypt2	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
	add	rax,16

$L$dec_loop2::
DB	102,15,56,222,209
DB	102,15,56,222,217
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,222,208
DB	102,15,56,222,216
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
	jnz	$L$dec_loop2

DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,223,208
DB	102,15,56,223,216
	DB	0F3h,0C3h		;repret
_aesni_decrypt2	ENDP

ALIGN	16
_aesni_encrypt3	PROC PRIVATE
	movups	xmm0,XMMWORD PTR[rcx]
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
	add	rax,16

$L$enc_loop3::
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
	add	rax,16

$L$dec_loop3::
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	xorps	xmm5,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	00fh,01fh,000h
	add	rax,16

$L$enc_loop4::
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	xorps	xmm4,xmm0
	xorps	xmm5,xmm0
	movups	xmm0,XMMWORD PTR[32+rcx]
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	00fh,01fh,000h
	add	rax,16

$L$dec_loop4::
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
DB	102,15,56,220,209
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	102,15,56,220,217
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
DB	102,15,56,220,225
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR[rax*1+rcx]
	add	rax,16
	jmp	$L$enc_loop6_enter
ALIGN	16
$L$enc_loop6::
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
$L$enc_loop6_enter::
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	pxor	xmm3,xmm0
	pxor	xmm4,xmm0
DB	102,15,56,222,209
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	102,15,56,222,217
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
DB	102,15,56,222,225
	pxor	xmm7,xmm0
	movups	xmm0,XMMWORD PTR[rax*1+rcx]
	add	rax,16
	jmp	$L$dec_loop6_enter
ALIGN	16
$L$dec_loop6::
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
$L$dec_loop6_enter::
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	102,15,56,220,209
	pxor	xmm7,xmm0
	pxor	xmm8,xmm0
DB	102,15,56,220,217
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[rax*1+rcx]
	add	rax,16
	jmp	$L$enc_loop8_inner
ALIGN	16
$L$enc_loop8::
DB	102,15,56,220,209
DB	102,15,56,220,217
$L$enc_loop8_inner::
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
$L$enc_loop8_enter::
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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
	shl	eax,4
	movups	xmm1,XMMWORD PTR[16+rcx]
	xorps	xmm2,xmm0
	xorps	xmm3,xmm0
	pxor	xmm4,xmm0
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	neg	rax
DB	102,15,56,222,209
	pxor	xmm7,xmm0
	pxor	xmm8,xmm0
DB	102,15,56,222,217
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[rax*1+rcx]
	add	rax,16
	jmp	$L$dec_loop8_inner
ALIGN	16
$L$dec_loop8::
DB	102,15,56,222,209
DB	102,15,56,222,217
$L$dec_loop8_inner::
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
$L$dec_loop8_enter::
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
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


	lea	rsp,QWORD PTR[((-88))+rsp]
	movaps	XMMWORD PTR[rsp],xmm6
	movaps	XMMWORD PTR[16+rsp],xmm7
	movaps	XMMWORD PTR[32+rsp],xmm8
	movaps	XMMWORD PTR[48+rsp],xmm9
$L$ecb_enc_body::
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
	xorps	xmm9,xmm9
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
	call	_aesni_encrypt2
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
	pxor	xmm2,xmm2
	mov	rcx,r11
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	mov	eax,r10d
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	pxor	xmm7,xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	pxor	xmm8,xmm8
	movups	XMMWORD PTR[112+rsi],xmm9
	pxor	xmm9,xmm9
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
	xorps	xmm9,xmm9
	call	_aesni_decrypt8
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	pxor	xmm7,xmm7
	movups	XMMWORD PTR[96+rsi],xmm8
	pxor	xmm8,xmm8
	pxor	xmm9,xmm9
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
	pxor	xmm2,xmm2
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_two::
	call	_aesni_decrypt2
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_three::
	call	_aesni_decrypt3
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_four::
	call	_aesni_decrypt4
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_five::
	xorps	xmm7,xmm7
	call	_aesni_decrypt6
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	jmp	$L$ecb_ret
ALIGN	16
$L$ecb_dec_six::
	call	_aesni_decrypt6
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movups	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	movups	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	movups	XMMWORD PTR[80+rsi],xmm7
	pxor	xmm7,xmm7

$L$ecb_ret::
	xorps	xmm0,xmm0
	pxor	xmm1,xmm1
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	XMMWORD PTR[48+rsp],xmm0
	lea	rsp,QWORD PTR[88+rsp]
$L$ecb_enc_ret::
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
	movdqu	xmm6,XMMWORD PTR[r8]
	movdqa	xmm9,XMMWORD PTR[$L$increment64]
	movdqa	xmm7,XMMWORD PTR[$L$bswap_mask]

	shl	eax,4
	mov	r10d,16
	lea	r11,QWORD PTR[rcx]
	movdqu	xmm3,XMMWORD PTR[r9]
	movdqa	xmm2,xmm6
	lea	rcx,QWORD PTR[32+rax*1+rcx]
DB	102,15,56,0,247
	sub	r10,rax
	jmp	$L$ccm64_enc_outer
ALIGN	16
$L$ccm64_enc_outer::
	movups	xmm0,XMMWORD PTR[r11]
	mov	rax,r10
	movups	xmm8,XMMWORD PTR[rdi]

	xorps	xmm2,xmm0
	movups	xmm1,XMMWORD PTR[16+r11]
	xorps	xmm0,xmm8
	xorps	xmm3,xmm0
	movups	xmm0,XMMWORD PTR[32+r11]

$L$ccm64_enc2_loop::
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
	jnz	$L$ccm64_enc2_loop
DB	102,15,56,220,209
DB	102,15,56,220,217
	paddq	xmm6,xmm9
	dec	rdx
DB	102,15,56,221,208
DB	102,15,56,221,216

	lea	rdi,QWORD PTR[16+rdi]
	xorps	xmm8,xmm2
	movdqa	xmm2,xmm6
	movups	XMMWORD PTR[rsi],xmm8
DB	102,15,56,0,215
	lea	rsi,QWORD PTR[16+rsi]
	jnz	$L$ccm64_enc_outer

	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[r9],xmm3
	pxor	xmm3,xmm3
	pxor	xmm8,xmm8
	pxor	xmm6,xmm6
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	XMMWORD PTR[48+rsp],xmm0
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
	movups	xmm6,XMMWORD PTR[r8]
	movdqu	xmm3,XMMWORD PTR[r9]
	movdqa	xmm9,XMMWORD PTR[$L$increment64]
	movdqa	xmm7,XMMWORD PTR[$L$bswap_mask]

	movaps	xmm2,xmm6
	mov	r10d,eax
	mov	r11,rcx
DB	102,15,56,0,247
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
	shl	r10d,4
	mov	eax,16
	movups	xmm8,XMMWORD PTR[rdi]
	paddq	xmm6,xmm9
	lea	rdi,QWORD PTR[16+rdi]
	sub	rax,r10
	lea	rcx,QWORD PTR[32+r10*1+r11]
	mov	r10,rax
	jmp	$L$ccm64_dec_outer
ALIGN	16
$L$ccm64_dec_outer::
	xorps	xmm8,xmm2
	movdqa	xmm2,xmm6
	movups	XMMWORD PTR[rsi],xmm8
	lea	rsi,QWORD PTR[16+rsi]
DB	102,15,56,0,215

	sub	rdx,1
	jz	$L$ccm64_dec_break

	movups	xmm0,XMMWORD PTR[r11]
	mov	rax,r10
	movups	xmm1,XMMWORD PTR[16+r11]
	xorps	xmm8,xmm0
	xorps	xmm2,xmm0
	xorps	xmm3,xmm8
	movups	xmm0,XMMWORD PTR[32+r11]
	jmp	$L$ccm64_dec2_loop
ALIGN	16
$L$ccm64_dec2_loop::
DB	102,15,56,220,209
DB	102,15,56,220,217
	movups	xmm1,XMMWORD PTR[rax*1+rcx]
	add	rax,32
DB	102,15,56,220,208
DB	102,15,56,220,216
	movups	xmm0,XMMWORD PTR[((-16))+rax*1+rcx]
	jnz	$L$ccm64_dec2_loop
	movups	xmm8,XMMWORD PTR[rdi]
	paddq	xmm6,xmm9
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,221,208
DB	102,15,56,221,216
	lea	rdi,QWORD PTR[16+rdi]
	jmp	$L$ccm64_dec_outer

ALIGN	16
$L$ccm64_dec_break::

	mov	eax,DWORD PTR[240+r11]
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
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	movups	XMMWORD PTR[r9],xmm3
	pxor	xmm3,xmm3
	pxor	xmm8,xmm8
	pxor	xmm6,xmm6
	movaps	xmm6,XMMWORD PTR[rsp]
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	xmm7,XMMWORD PTR[16+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	xmm8,XMMWORD PTR[32+rsp]
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	xmm9,XMMWORD PTR[48+rsp]
	movaps	XMMWORD PTR[48+rsp],xmm0
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


	cmp	rdx,1
	jne	$L$ctr32_bulk



	movups	xmm2,XMMWORD PTR[r8]
	movups	xmm3,XMMWORD PTR[rdi]
	mov	edx,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_enc1_7::
DB	102,15,56,220,209
	dec	edx
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_enc1_7
DB	102,15,56,221,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	xorps	xmm2,xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm2,xmm2
	jmp	$L$ctr32_epilogue

ALIGN	16
$L$ctr32_bulk::
	lea	rax,QWORD PTR[rsp]
	push	rbp
	sub	rsp,288
	and	rsp,-16
	movaps	XMMWORD PTR[(-168)+rax],xmm6
	movaps	XMMWORD PTR[(-152)+rax],xmm7
	movaps	XMMWORD PTR[(-136)+rax],xmm8
	movaps	XMMWORD PTR[(-120)+rax],xmm9
	movaps	XMMWORD PTR[(-104)+rax],xmm10
	movaps	XMMWORD PTR[(-88)+rax],xmm11
	movaps	XMMWORD PTR[(-72)+rax],xmm12
	movaps	XMMWORD PTR[(-56)+rax],xmm13
	movaps	XMMWORD PTR[(-40)+rax],xmm14
	movaps	XMMWORD PTR[(-24)+rax],xmm15
$L$ctr32_body::
	lea	rbp,QWORD PTR[((-8))+rax]




	movdqu	xmm2,XMMWORD PTR[r8]
	movdqu	xmm0,XMMWORD PTR[rcx]
	mov	r8d,DWORD PTR[12+r8]
	pxor	xmm2,xmm0
	mov	r11d,DWORD PTR[12+rcx]
	movdqa	XMMWORD PTR[rsp],xmm2
	bswap	r8d
	movdqa	xmm3,xmm2
	movdqa	xmm4,xmm2
	movdqa	xmm5,xmm2
	movdqa	XMMWORD PTR[64+rsp],xmm2
	movdqa	XMMWORD PTR[80+rsp],xmm2
	movdqa	XMMWORD PTR[96+rsp],xmm2
	mov	r10,rdx
	movdqa	XMMWORD PTR[112+rsp],xmm2

	lea	rax,QWORD PTR[1+r8]
	lea	rdx,QWORD PTR[2+r8]
	bswap	eax
	bswap	edx
	xor	eax,r11d
	xor	edx,r11d
DB	102,15,58,34,216,3
	lea	rax,QWORD PTR[3+r8]
	movdqa	XMMWORD PTR[16+rsp],xmm3
DB	102,15,58,34,226,3
	bswap	eax
	mov	rdx,r10
	lea	r10,QWORD PTR[4+r8]
	movdqa	XMMWORD PTR[32+rsp],xmm4
	xor	eax,r11d
	bswap	r10d
DB	102,15,58,34,232,3
	xor	r10d,r11d
	movdqa	XMMWORD PTR[48+rsp],xmm5
	lea	r9,QWORD PTR[5+r8]
	mov	DWORD PTR[((64+12))+rsp],r10d
	bswap	r9d
	lea	r10,QWORD PTR[6+r8]
	mov	eax,DWORD PTR[240+rcx]
	xor	r9d,r11d
	bswap	r10d
	mov	DWORD PTR[((80+12))+rsp],r9d
	xor	r10d,r11d
	lea	r9,QWORD PTR[7+r8]
	mov	DWORD PTR[((96+12))+rsp],r10d
	bswap	r9d
	mov	r10d,DWORD PTR[((OPENSSL_ia32cap_P+4))]
	xor	r9d,r11d
	and	r10d,71303168
	mov	DWORD PTR[((112+12))+rsp],r9d

	movups	xmm1,XMMWORD PTR[16+rcx]

	movdqa	xmm6,XMMWORD PTR[64+rsp]
	movdqa	xmm7,XMMWORD PTR[80+rsp]

	cmp	rdx,8
	jb	$L$ctr32_tail

	sub	rdx,6
	cmp	r10d,4194304
	je	$L$ctr32_6x

	lea	rcx,QWORD PTR[128+rcx]
	sub	rdx,2
	jmp	$L$ctr32_loop8

ALIGN	16
$L$ctr32_6x::
	shl	eax,4
	mov	r10d,48
	bswap	r11d
	lea	rcx,QWORD PTR[32+rax*1+rcx]
	sub	r10,rax
	jmp	$L$ctr32_loop6

ALIGN	16
$L$ctr32_loop6::
	add	r8d,6
	movups	xmm0,XMMWORD PTR[((-48))+r10*1+rcx]
DB	102,15,56,220,209
	mov	eax,r8d
	xor	eax,r11d
DB	102,15,56,220,217
DB	00fh,038h,0f1h,044h,024h,12
	lea	eax,DWORD PTR[1+r8]
DB	102,15,56,220,225
	xor	eax,r11d
DB	00fh,038h,0f1h,044h,024h,28
DB	102,15,56,220,233
	lea	eax,DWORD PTR[2+r8]
	xor	eax,r11d
DB	102,15,56,220,241
DB	00fh,038h,0f1h,044h,024h,44
	lea	eax,DWORD PTR[3+r8]
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[((-32))+r10*1+rcx]
	xor	eax,r11d

DB	102,15,56,220,208
DB	00fh,038h,0f1h,044h,024h,60
	lea	eax,DWORD PTR[4+r8]
DB	102,15,56,220,216
	xor	eax,r11d
DB	00fh,038h,0f1h,044h,024h,76
DB	102,15,56,220,224
	lea	eax,DWORD PTR[5+r8]
	xor	eax,r11d
DB	102,15,56,220,232
DB	00fh,038h,0f1h,044h,024h,92
	mov	rax,r10
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[((-16))+r10*1+rcx]

	call	$L$enc_loop6

	movdqu	xmm8,XMMWORD PTR[rdi]
	movdqu	xmm9,XMMWORD PTR[16+rdi]
	movdqu	xmm10,XMMWORD PTR[32+rdi]
	movdqu	xmm11,XMMWORD PTR[48+rdi]
	movdqu	xmm12,XMMWORD PTR[64+rdi]
	movdqu	xmm13,XMMWORD PTR[80+rdi]
	lea	rdi,QWORD PTR[96+rdi]
	movups	xmm1,XMMWORD PTR[((-64))+r10*1+rcx]
	pxor	xmm8,xmm2
	movaps	xmm2,XMMWORD PTR[rsp]
	pxor	xmm9,xmm3
	movaps	xmm3,XMMWORD PTR[16+rsp]
	pxor	xmm10,xmm4
	movaps	xmm4,XMMWORD PTR[32+rsp]
	pxor	xmm11,xmm5
	movaps	xmm5,XMMWORD PTR[48+rsp]
	pxor	xmm12,xmm6
	movaps	xmm6,XMMWORD PTR[64+rsp]
	pxor	xmm13,xmm7
	movaps	xmm7,XMMWORD PTR[80+rsp]
	movdqu	XMMWORD PTR[rsi],xmm8
	movdqu	XMMWORD PTR[16+rsi],xmm9
	movdqu	XMMWORD PTR[32+rsi],xmm10
	movdqu	XMMWORD PTR[48+rsi],xmm11
	movdqu	XMMWORD PTR[64+rsi],xmm12
	movdqu	XMMWORD PTR[80+rsi],xmm13
	lea	rsi,QWORD PTR[96+rsi]

	sub	rdx,6
	jnc	$L$ctr32_loop6

	add	rdx,6
	jz	$L$ctr32_done

	lea	eax,DWORD PTR[((-48))+r10]
	lea	rcx,QWORD PTR[((-80))+r10*1+rcx]
	neg	eax
	shr	eax,4
	jmp	$L$ctr32_tail

ALIGN	32
$L$ctr32_loop8::
	add	r8d,8
	movdqa	xmm8,XMMWORD PTR[96+rsp]
DB	102,15,56,220,209
	mov	r9d,r8d
	movdqa	xmm9,XMMWORD PTR[112+rsp]
DB	102,15,56,220,217
	bswap	r9d
	movups	xmm0,XMMWORD PTR[((32-128))+rcx]
DB	102,15,56,220,225
	xor	r9d,r11d
	nop
DB	102,15,56,220,233
	mov	DWORD PTR[((0+12))+rsp],r9d
	lea	r9,QWORD PTR[1+r8]
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((48-128))+rcx]
	bswap	r9d
DB	102,15,56,220,208
DB	102,15,56,220,216
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,224
DB	102,15,56,220,232
	mov	DWORD PTR[((16+12))+rsp],r9d
	lea	r9,QWORD PTR[2+r8]
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((64-128))+rcx]
	bswap	r9d
DB	102,15,56,220,209
DB	102,15,56,220,217
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,225
DB	102,15,56,220,233
	mov	DWORD PTR[((32+12))+rsp],r9d
	lea	r9,QWORD PTR[3+r8]
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((80-128))+rcx]
	bswap	r9d
DB	102,15,56,220,208
DB	102,15,56,220,216
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,224
DB	102,15,56,220,232
	mov	DWORD PTR[((48+12))+rsp],r9d
	lea	r9,QWORD PTR[4+r8]
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((96-128))+rcx]
	bswap	r9d
DB	102,15,56,220,209
DB	102,15,56,220,217
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,225
DB	102,15,56,220,233
	mov	DWORD PTR[((64+12))+rsp],r9d
	lea	r9,QWORD PTR[5+r8]
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((112-128))+rcx]
	bswap	r9d
DB	102,15,56,220,208
DB	102,15,56,220,216
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,224
DB	102,15,56,220,232
	mov	DWORD PTR[((80+12))+rsp],r9d
	lea	r9,QWORD PTR[6+r8]
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((128-128))+rcx]
	bswap	r9d
DB	102,15,56,220,209
DB	102,15,56,220,217
	xor	r9d,r11d
DB	066h,090h
DB	102,15,56,220,225
DB	102,15,56,220,233
	mov	DWORD PTR[((96+12))+rsp],r9d
	lea	r9,QWORD PTR[7+r8]
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((144-128))+rcx]
	bswap	r9d
DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
	xor	r9d,r11d
	movdqu	xmm10,XMMWORD PTR[rdi]
DB	102,15,56,220,232
	mov	DWORD PTR[((112+12))+rsp],r9d
	cmp	eax,11
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((160-128))+rcx]

	jb	$L$ctr32_enc_done

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((176-128))+rcx]

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((192-128))+rcx]
	je	$L$ctr32_enc_done

DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movups	xmm1,XMMWORD PTR[((208-128))+rcx]

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
DB	102,68,15,56,220,192
DB	102,68,15,56,220,200
	movups	xmm0,XMMWORD PTR[((224-128))+rcx]
	jmp	$L$ctr32_enc_done

ALIGN	16
$L$ctr32_enc_done::
	movdqu	xmm11,XMMWORD PTR[16+rdi]
	pxor	xmm10,xmm0
	movdqu	xmm12,XMMWORD PTR[32+rdi]
	pxor	xmm11,xmm0
	movdqu	xmm13,XMMWORD PTR[48+rdi]
	pxor	xmm12,xmm0
	movdqu	xmm14,XMMWORD PTR[64+rdi]
	pxor	xmm13,xmm0
	movdqu	xmm15,XMMWORD PTR[80+rdi]
	pxor	xmm14,xmm0
	pxor	xmm15,xmm0
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
DB	102,68,15,56,220,193
DB	102,68,15,56,220,201
	movdqu	xmm1,XMMWORD PTR[96+rdi]
	lea	rdi,QWORD PTR[128+rdi]

DB	102,65,15,56,221,210
	pxor	xmm1,xmm0
	movdqu	xmm10,XMMWORD PTR[((112-128))+rdi]
DB	102,65,15,56,221,219
	pxor	xmm10,xmm0
	movdqa	xmm11,XMMWORD PTR[rsp]
DB	102,65,15,56,221,228
DB	102,65,15,56,221,237
	movdqa	xmm12,XMMWORD PTR[16+rsp]
	movdqa	xmm13,XMMWORD PTR[32+rsp]
DB	102,65,15,56,221,246
DB	102,65,15,56,221,255
	movdqa	xmm14,XMMWORD PTR[48+rsp]
	movdqa	xmm15,XMMWORD PTR[64+rsp]
DB	102,68,15,56,221,193
	movdqa	xmm0,XMMWORD PTR[80+rsp]
	movups	xmm1,XMMWORD PTR[((16-128))+rcx]
DB	102,69,15,56,221,202

	movups	XMMWORD PTR[rsi],xmm2
	movdqa	xmm2,xmm11
	movups	XMMWORD PTR[16+rsi],xmm3
	movdqa	xmm3,xmm12
	movups	XMMWORD PTR[32+rsi],xmm4
	movdqa	xmm4,xmm13
	movups	XMMWORD PTR[48+rsi],xmm5
	movdqa	xmm5,xmm14
	movups	XMMWORD PTR[64+rsi],xmm6
	movdqa	xmm6,xmm15
	movups	XMMWORD PTR[80+rsi],xmm7
	movdqa	xmm7,xmm0
	movups	XMMWORD PTR[96+rsi],xmm8
	movups	XMMWORD PTR[112+rsi],xmm9
	lea	rsi,QWORD PTR[128+rsi]

	sub	rdx,8
	jnc	$L$ctr32_loop8

	add	rdx,8
	jz	$L$ctr32_done
	lea	rcx,QWORD PTR[((-128))+rcx]

$L$ctr32_tail::


	lea	rcx,QWORD PTR[16+rcx]
	cmp	rdx,4
	jb	$L$ctr32_loop3
	je	$L$ctr32_loop4


	shl	eax,4
	movdqa	xmm8,XMMWORD PTR[96+rsp]
	pxor	xmm9,xmm9

	movups	xmm0,XMMWORD PTR[16+rcx]
DB	102,15,56,220,209
DB	102,15,56,220,217
	lea	rcx,QWORD PTR[((32-16))+rax*1+rcx]
	neg	rax
DB	102,15,56,220,225
	add	rax,16
	movups	xmm10,XMMWORD PTR[rdi]
DB	102,15,56,220,233
DB	102,15,56,220,241
	movups	xmm11,XMMWORD PTR[16+rdi]
	movups	xmm12,XMMWORD PTR[32+rdi]
DB	102,15,56,220,249
DB	102,68,15,56,220,193

	call	$L$enc_loop8_enter

	movdqu	xmm13,XMMWORD PTR[48+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm10,XMMWORD PTR[64+rdi]
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm6,xmm10
	movdqu	XMMWORD PTR[48+rsi],xmm5
	movdqu	XMMWORD PTR[64+rsi],xmm6
	cmp	rdx,6
	jb	$L$ctr32_done

	movups	xmm11,XMMWORD PTR[80+rdi]
	xorps	xmm7,xmm11
	movups	XMMWORD PTR[80+rsi],xmm7
	je	$L$ctr32_done

	movups	xmm12,XMMWORD PTR[96+rdi]
	xorps	xmm8,xmm12
	movups	XMMWORD PTR[96+rsi],xmm8
	jmp	$L$ctr32_done

ALIGN	32
$L$ctr32_loop4::
DB	102,15,56,220,209
	lea	rcx,QWORD PTR[16+rcx]
	dec	eax
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
	movups	xmm1,XMMWORD PTR[rcx]
	jnz	$L$ctr32_loop4
DB	102,15,56,221,209
DB	102,15,56,221,217
	movups	xmm10,XMMWORD PTR[rdi]
	movups	xmm11,XMMWORD PTR[16+rdi]
DB	102,15,56,221,225
DB	102,15,56,221,233
	movups	xmm12,XMMWORD PTR[32+rdi]
	movups	xmm13,XMMWORD PTR[48+rdi]

	xorps	xmm2,xmm10
	movups	XMMWORD PTR[rsi],xmm2
	xorps	xmm3,xmm11
	movups	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[48+rsi],xmm5
	jmp	$L$ctr32_done

ALIGN	32
$L$ctr32_loop3::
DB	102,15,56,220,209
	lea	rcx,QWORD PTR[16+rcx]
	dec	eax
DB	102,15,56,220,217
DB	102,15,56,220,225
	movups	xmm1,XMMWORD PTR[rcx]
	jnz	$L$ctr32_loop3
DB	102,15,56,221,209
DB	102,15,56,221,217
DB	102,15,56,221,225

	movups	xmm10,XMMWORD PTR[rdi]
	xorps	xmm2,xmm10
	movups	XMMWORD PTR[rsi],xmm2
	cmp	rdx,2
	jb	$L$ctr32_done

	movups	xmm11,XMMWORD PTR[16+rdi]
	xorps	xmm3,xmm11
	movups	XMMWORD PTR[16+rsi],xmm3
	je	$L$ctr32_done

	movups	xmm12,XMMWORD PTR[32+rdi]
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[32+rsi],xmm4

$L$ctr32_done::
	xorps	xmm0,xmm0
	xor	r11d,r11d
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movaps	xmm6,XMMWORD PTR[((-160))+rbp]
	movaps	XMMWORD PTR[(-160)+rbp],xmm0
	movaps	xmm7,XMMWORD PTR[((-144))+rbp]
	movaps	XMMWORD PTR[(-144)+rbp],xmm0
	movaps	xmm8,XMMWORD PTR[((-128))+rbp]
	movaps	XMMWORD PTR[(-128)+rbp],xmm0
	movaps	xmm9,XMMWORD PTR[((-112))+rbp]
	movaps	XMMWORD PTR[(-112)+rbp],xmm0
	movaps	xmm10,XMMWORD PTR[((-96))+rbp]
	movaps	XMMWORD PTR[(-96)+rbp],xmm0
	movaps	xmm11,XMMWORD PTR[((-80))+rbp]
	movaps	XMMWORD PTR[(-80)+rbp],xmm0
	movaps	xmm12,XMMWORD PTR[((-64))+rbp]
	movaps	XMMWORD PTR[(-64)+rbp],xmm0
	movaps	xmm13,XMMWORD PTR[((-48))+rbp]
	movaps	XMMWORD PTR[(-48)+rbp],xmm0
	movaps	xmm14,XMMWORD PTR[((-32))+rbp]
	movaps	XMMWORD PTR[(-32)+rbp],xmm0
	movaps	xmm15,XMMWORD PTR[((-16))+rbp]
	movaps	XMMWORD PTR[(-16)+rbp],xmm0
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	XMMWORD PTR[48+rsp],xmm0
	movaps	XMMWORD PTR[64+rsp],xmm0
	movaps	XMMWORD PTR[80+rsp],xmm0
	movaps	XMMWORD PTR[96+rsp],xmm0
	movaps	XMMWORD PTR[112+rsp],xmm0
	lea	rsp,QWORD PTR[rbp]
	pop	rbp
$L$ctr32_epilogue::
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


	lea	rax,QWORD PTR[rsp]
	push	rbp
	sub	rsp,272
	and	rsp,-16
	movaps	XMMWORD PTR[(-168)+rax],xmm6
	movaps	XMMWORD PTR[(-152)+rax],xmm7
	movaps	XMMWORD PTR[(-136)+rax],xmm8
	movaps	XMMWORD PTR[(-120)+rax],xmm9
	movaps	XMMWORD PTR[(-104)+rax],xmm10
	movaps	XMMWORD PTR[(-88)+rax],xmm11
	movaps	XMMWORD PTR[(-72)+rax],xmm12
	movaps	XMMWORD PTR[(-56)+rax],xmm13
	movaps	XMMWORD PTR[(-40)+rax],xmm14
	movaps	XMMWORD PTR[(-24)+rax],xmm15
$L$xts_enc_body::
	lea	rbp,QWORD PTR[((-8))+rax]
	movups	xmm2,XMMWORD PTR[r9]
	mov	eax,DWORD PTR[240+r8]
	mov	r10d,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm2,xmm0
$L$oop_enc1_8::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_enc1_8
DB	102,15,56,221,209
	movups	xmm0,XMMWORD PTR[rcx]
	mov	r11,rcx
	mov	eax,r10d
	shl	r10d,4
	mov	r9,rdx
	and	rdx,-16

	movups	xmm1,XMMWORD PTR[16+r10*1+rcx]

	movdqa	xmm8,XMMWORD PTR[$L$xts_magic]
	movdqa	xmm15,xmm2
	pshufd	xmm9,xmm2,05fh
	pxor	xmm1,xmm0
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm10,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm10,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm11,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm11,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm12,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm12,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm13,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm13,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm15
	psrad	xmm9,31
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pxor	xmm14,xmm0
	pxor	xmm15,xmm9
	movaps	XMMWORD PTR[96+rsp],xmm1

	sub	rdx,16*6
	jc	$L$xts_enc_short

	mov	eax,16+96
	lea	rcx,QWORD PTR[32+r10*1+r11]
	sub	rax,r10
	movups	xmm1,XMMWORD PTR[16+r11]
	mov	r10,rax
	lea	r8,QWORD PTR[$L$xts_magic]
	jmp	$L$xts_enc_grandloop

ALIGN	32
$L$xts_enc_grandloop::
	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqa	xmm8,xmm0
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm3,xmm11
DB	102,15,56,220,209
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm4,xmm12
DB	102,15,56,220,217
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	pxor	xmm5,xmm13
DB	102,15,56,220,225
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	pxor	xmm8,xmm15
	movdqa	xmm9,XMMWORD PTR[96+rsp]
	pxor	xmm6,xmm14
DB	102,15,56,220,233
	movups	xmm0,XMMWORD PTR[32+r11]
	lea	rdi,QWORD PTR[96+rdi]
	pxor	xmm7,xmm8

	pxor	xmm10,xmm9
DB	102,15,56,220,241
	pxor	xmm11,xmm9
	movdqa	XMMWORD PTR[rsp],xmm10
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[48+r11]
	pxor	xmm12,xmm9

DB	102,15,56,220,208
	pxor	xmm13,xmm9
	movdqa	XMMWORD PTR[16+rsp],xmm11
DB	102,15,56,220,216
	pxor	xmm14,xmm9
	movdqa	XMMWORD PTR[32+rsp],xmm12
DB	102,15,56,220,224
DB	102,15,56,220,232
	pxor	xmm8,xmm9
	movdqa	XMMWORD PTR[64+rsp],xmm14
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[64+r11]
	movdqa	XMMWORD PTR[80+rsp],xmm8
	pshufd	xmm9,xmm15,05fh
	jmp	$L$xts_enc_loop6
ALIGN	32
$L$xts_enc_loop6::
DB	102,15,56,220,209
DB	102,15,56,220,217
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[((-64))+rax*1+rcx]
	add	rax,32

DB	102,15,56,220,208
DB	102,15,56,220,216
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[((-80))+rax*1+rcx]
	jnz	$L$xts_enc_loop6

	movdqa	xmm8,XMMWORD PTR[r8]
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
DB	102,15,56,220,209
	paddq	xmm15,xmm15
	psrad	xmm14,31
DB	102,15,56,220,217
	pand	xmm14,xmm8
	movups	xmm10,XMMWORD PTR[r11]
DB	102,15,56,220,225
DB	102,15,56,220,233
DB	102,15,56,220,241
	pxor	xmm15,xmm14
	movaps	xmm11,xmm10
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[((-64))+rcx]

	movdqa	xmm14,xmm9
DB	102,15,56,220,208
	paddd	xmm9,xmm9
	pxor	xmm10,xmm15
DB	102,15,56,220,216
	psrad	xmm14,31
	paddq	xmm15,xmm15
DB	102,15,56,220,224
DB	102,15,56,220,232
	pand	xmm14,xmm8
	movaps	xmm12,xmm11
DB	102,15,56,220,240
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
DB	102,15,56,220,248
	movups	xmm0,XMMWORD PTR[((-48))+rcx]

	paddd	xmm9,xmm9
DB	102,15,56,220,209
	pxor	xmm11,xmm15
	psrad	xmm14,31
DB	102,15,56,220,217
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
DB	102,15,56,220,225
DB	102,15,56,220,233
	movdqa	XMMWORD PTR[48+rsp],xmm13
	pxor	xmm15,xmm14
DB	102,15,56,220,241
	movaps	xmm13,xmm12
	movdqa	xmm14,xmm9
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[((-32))+rcx]

	paddd	xmm9,xmm9
DB	102,15,56,220,208
	pxor	xmm12,xmm15
	psrad	xmm14,31
DB	102,15,56,220,216
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
DB	102,15,56,220,224
DB	102,15,56,220,232
DB	102,15,56,220,240
	pxor	xmm15,xmm14
	movaps	xmm14,xmm13
DB	102,15,56,220,248

	movdqa	xmm0,xmm9
	paddd	xmm9,xmm9
DB	102,15,56,220,209
	pxor	xmm13,xmm15
	psrad	xmm0,31
DB	102,15,56,220,217
	paddq	xmm15,xmm15
	pand	xmm0,xmm8
DB	102,15,56,220,225
DB	102,15,56,220,233
	pxor	xmm15,xmm0
	movups	xmm0,XMMWORD PTR[r11]
DB	102,15,56,220,241
DB	102,15,56,220,249
	movups	xmm1,XMMWORD PTR[16+r11]

	pxor	xmm14,xmm15
DB	102,15,56,221,84,36,0
	psrad	xmm9,31
	paddq	xmm15,xmm15
DB	102,15,56,221,92,36,16
DB	102,15,56,221,100,36,32
	pand	xmm9,xmm8
	mov	rax,r10
DB	102,15,56,221,108,36,48
DB	102,15,56,221,116,36,64
DB	102,15,56,221,124,36,80
	pxor	xmm15,xmm9

	lea	rsi,QWORD PTR[96+rsi]
	movups	XMMWORD PTR[(-96)+rsi],xmm2
	movups	XMMWORD PTR[(-80)+rsi],xmm3
	movups	XMMWORD PTR[(-64)+rsi],xmm4
	movups	XMMWORD PTR[(-48)+rsi],xmm5
	movups	XMMWORD PTR[(-32)+rsi],xmm6
	movups	XMMWORD PTR[(-16)+rsi],xmm7
	sub	rdx,16*6
	jnc	$L$xts_enc_grandloop

	mov	eax,16+96
	sub	eax,r10d
	mov	rcx,r11
	shr	eax,4

$L$xts_enc_short::

	mov	r10d,eax
	pxor	xmm10,xmm0
	add	rdx,16*6
	jz	$L$xts_enc_done

	pxor	xmm11,xmm0
	cmp	rdx,020h
	jb	$L$xts_enc_one
	pxor	xmm12,xmm0
	je	$L$xts_enc_two

	pxor	xmm13,xmm0
	cmp	rdx,040h
	jb	$L$xts_enc_three
	pxor	xmm14,xmm0
	je	$L$xts_enc_four

	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm3,xmm11
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	lea	rdi,QWORD PTR[80+rdi]
	pxor	xmm4,xmm12
	pxor	xmm5,xmm13
	pxor	xmm6,xmm14
	pxor	xmm7,xmm7

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

	call	_aesni_encrypt2

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

	pxor	xmm2,xmm10
	movdqa	xmm10,xmm14
	pxor	xmm3,xmm11
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[16+rsi],xmm3
	movdqu	XMMWORD PTR[32+rsi],xmm4
	movdqu	XMMWORD PTR[48+rsi],xmm5
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
	xorps	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movaps	xmm6,XMMWORD PTR[((-160))+rbp]
	movaps	XMMWORD PTR[(-160)+rbp],xmm0
	movaps	xmm7,XMMWORD PTR[((-144))+rbp]
	movaps	XMMWORD PTR[(-144)+rbp],xmm0
	movaps	xmm8,XMMWORD PTR[((-128))+rbp]
	movaps	XMMWORD PTR[(-128)+rbp],xmm0
	movaps	xmm9,XMMWORD PTR[((-112))+rbp]
	movaps	XMMWORD PTR[(-112)+rbp],xmm0
	movaps	xmm10,XMMWORD PTR[((-96))+rbp]
	movaps	XMMWORD PTR[(-96)+rbp],xmm0
	movaps	xmm11,XMMWORD PTR[((-80))+rbp]
	movaps	XMMWORD PTR[(-80)+rbp],xmm0
	movaps	xmm12,XMMWORD PTR[((-64))+rbp]
	movaps	XMMWORD PTR[(-64)+rbp],xmm0
	movaps	xmm13,XMMWORD PTR[((-48))+rbp]
	movaps	XMMWORD PTR[(-48)+rbp],xmm0
	movaps	xmm14,XMMWORD PTR[((-32))+rbp]
	movaps	XMMWORD PTR[(-32)+rbp],xmm0
	movaps	xmm15,XMMWORD PTR[((-16))+rbp]
	movaps	XMMWORD PTR[(-16)+rbp],xmm0
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	XMMWORD PTR[48+rsp],xmm0
	movaps	XMMWORD PTR[64+rsp],xmm0
	movaps	XMMWORD PTR[80+rsp],xmm0
	movaps	XMMWORD PTR[96+rsp],xmm0
	lea	rsp,QWORD PTR[rbp]
	pop	rbp
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


	lea	rax,QWORD PTR[rsp]
	push	rbp
	sub	rsp,272
	and	rsp,-16
	movaps	XMMWORD PTR[(-168)+rax],xmm6
	movaps	XMMWORD PTR[(-152)+rax],xmm7
	movaps	XMMWORD PTR[(-136)+rax],xmm8
	movaps	XMMWORD PTR[(-120)+rax],xmm9
	movaps	XMMWORD PTR[(-104)+rax],xmm10
	movaps	XMMWORD PTR[(-88)+rax],xmm11
	movaps	XMMWORD PTR[(-72)+rax],xmm12
	movaps	XMMWORD PTR[(-56)+rax],xmm13
	movaps	XMMWORD PTR[(-40)+rax],xmm14
	movaps	XMMWORD PTR[(-24)+rax],xmm15
$L$xts_dec_body::
	lea	rbp,QWORD PTR[((-8))+rax]
	movups	xmm2,XMMWORD PTR[r9]
	mov	eax,DWORD PTR[240+r8]
	mov	r10d,DWORD PTR[240+rcx]
	movups	xmm0,XMMWORD PTR[r8]
	movups	xmm1,XMMWORD PTR[16+r8]
	lea	r8,QWORD PTR[32+r8]
	xorps	xmm2,xmm0
$L$oop_enc1_11::
DB	102,15,56,220,209
	dec	eax
	movups	xmm1,XMMWORD PTR[r8]
	lea	r8,QWORD PTR[16+r8]
	jnz	$L$oop_enc1_11
DB	102,15,56,221,209
	xor	eax,eax
	test	rdx,15
	setnz	al
	shl	rax,4
	sub	rdx,rax

	movups	xmm0,XMMWORD PTR[rcx]
	mov	r11,rcx
	mov	eax,r10d
	shl	r10d,4
	mov	r9,rdx
	and	rdx,-16

	movups	xmm1,XMMWORD PTR[16+r10*1+rcx]

	movdqa	xmm8,XMMWORD PTR[$L$xts_magic]
	movdqa	xmm15,xmm2
	pshufd	xmm9,xmm2,05fh
	pxor	xmm1,xmm0
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm10,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm10,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm11,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm11,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm12,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm12,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
	movdqa	xmm13,xmm15
	psrad	xmm14,31
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
	pxor	xmm13,xmm0
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm15
	psrad	xmm9,31
	paddq	xmm15,xmm15
	pand	xmm9,xmm8
	pxor	xmm14,xmm0
	pxor	xmm15,xmm9
	movaps	XMMWORD PTR[96+rsp],xmm1

	sub	rdx,16*6
	jc	$L$xts_dec_short

	mov	eax,16+96
	lea	rcx,QWORD PTR[32+r10*1+r11]
	sub	rax,r10
	movups	xmm1,XMMWORD PTR[16+r11]
	mov	r10,rax
	lea	r8,QWORD PTR[$L$xts_magic]
	jmp	$L$xts_dec_grandloop

ALIGN	32
$L$xts_dec_grandloop::
	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqa	xmm8,xmm0
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	pxor	xmm2,xmm10
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	pxor	xmm3,xmm11
DB	102,15,56,222,209
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	pxor	xmm4,xmm12
DB	102,15,56,222,217
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	pxor	xmm5,xmm13
DB	102,15,56,222,225
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	pxor	xmm8,xmm15
	movdqa	xmm9,XMMWORD PTR[96+rsp]
	pxor	xmm6,xmm14
DB	102,15,56,222,233
	movups	xmm0,XMMWORD PTR[32+r11]
	lea	rdi,QWORD PTR[96+rdi]
	pxor	xmm7,xmm8

	pxor	xmm10,xmm9
DB	102,15,56,222,241
	pxor	xmm11,xmm9
	movdqa	XMMWORD PTR[rsp],xmm10
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[48+r11]
	pxor	xmm12,xmm9

DB	102,15,56,222,208
	pxor	xmm13,xmm9
	movdqa	XMMWORD PTR[16+rsp],xmm11
DB	102,15,56,222,216
	pxor	xmm14,xmm9
	movdqa	XMMWORD PTR[32+rsp],xmm12
DB	102,15,56,222,224
DB	102,15,56,222,232
	pxor	xmm8,xmm9
	movdqa	XMMWORD PTR[64+rsp],xmm14
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[64+r11]
	movdqa	XMMWORD PTR[80+rsp],xmm8
	pshufd	xmm9,xmm15,05fh
	jmp	$L$xts_dec_loop6
ALIGN	32
$L$xts_dec_loop6::
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[((-64))+rax*1+rcx]
	add	rax,32

DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[((-80))+rax*1+rcx]
	jnz	$L$xts_dec_loop6

	movdqa	xmm8,XMMWORD PTR[r8]
	movdqa	xmm14,xmm9
	paddd	xmm9,xmm9
DB	102,15,56,222,209
	paddq	xmm15,xmm15
	psrad	xmm14,31
DB	102,15,56,222,217
	pand	xmm14,xmm8
	movups	xmm10,XMMWORD PTR[r11]
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
	pxor	xmm15,xmm14
	movaps	xmm11,xmm10
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[((-64))+rcx]

	movdqa	xmm14,xmm9
DB	102,15,56,222,208
	paddd	xmm9,xmm9
	pxor	xmm10,xmm15
DB	102,15,56,222,216
	psrad	xmm14,31
	paddq	xmm15,xmm15
DB	102,15,56,222,224
DB	102,15,56,222,232
	pand	xmm14,xmm8
	movaps	xmm12,xmm11
DB	102,15,56,222,240
	pxor	xmm15,xmm14
	movdqa	xmm14,xmm9
DB	102,15,56,222,248
	movups	xmm0,XMMWORD PTR[((-48))+rcx]

	paddd	xmm9,xmm9
DB	102,15,56,222,209
	pxor	xmm11,xmm15
	psrad	xmm14,31
DB	102,15,56,222,217
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
DB	102,15,56,222,225
DB	102,15,56,222,233
	movdqa	XMMWORD PTR[48+rsp],xmm13
	pxor	xmm15,xmm14
DB	102,15,56,222,241
	movaps	xmm13,xmm12
	movdqa	xmm14,xmm9
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[((-32))+rcx]

	paddd	xmm9,xmm9
DB	102,15,56,222,208
	pxor	xmm12,xmm15
	psrad	xmm14,31
DB	102,15,56,222,216
	paddq	xmm15,xmm15
	pand	xmm14,xmm8
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
	pxor	xmm15,xmm14
	movaps	xmm14,xmm13
DB	102,15,56,222,248

	movdqa	xmm0,xmm9
	paddd	xmm9,xmm9
DB	102,15,56,222,209
	pxor	xmm13,xmm15
	psrad	xmm0,31
DB	102,15,56,222,217
	paddq	xmm15,xmm15
	pand	xmm0,xmm8
DB	102,15,56,222,225
DB	102,15,56,222,233
	pxor	xmm15,xmm0
	movups	xmm0,XMMWORD PTR[r11]
DB	102,15,56,222,241
DB	102,15,56,222,249
	movups	xmm1,XMMWORD PTR[16+r11]

	pxor	xmm14,xmm15
DB	102,15,56,223,84,36,0
	psrad	xmm9,31
	paddq	xmm15,xmm15
DB	102,15,56,223,92,36,16
DB	102,15,56,223,100,36,32
	pand	xmm9,xmm8
	mov	rax,r10
DB	102,15,56,223,108,36,48
DB	102,15,56,223,116,36,64
DB	102,15,56,223,124,36,80
	pxor	xmm15,xmm9

	lea	rsi,QWORD PTR[96+rsi]
	movups	XMMWORD PTR[(-96)+rsi],xmm2
	movups	XMMWORD PTR[(-80)+rsi],xmm3
	movups	XMMWORD PTR[(-64)+rsi],xmm4
	movups	XMMWORD PTR[(-48)+rsi],xmm5
	movups	XMMWORD PTR[(-32)+rsi],xmm6
	movups	XMMWORD PTR[(-16)+rsi],xmm7
	sub	rdx,16*6
	jnc	$L$xts_dec_grandloop

	mov	eax,16+96
	sub	eax,r10d
	mov	rcx,r11
	shr	eax,4

$L$xts_dec_short::

	mov	r10d,eax
	pxor	xmm10,xmm0
	pxor	xmm11,xmm0
	add	rdx,16*6
	jz	$L$xts_dec_done

	pxor	xmm12,xmm0
	cmp	rdx,020h
	jb	$L$xts_dec_one
	pxor	xmm13,xmm0
	je	$L$xts_dec_two

	pxor	xmm14,xmm0
	cmp	rdx,040h
	jb	$L$xts_dec_three
	je	$L$xts_dec_four

	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
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

	call	_aesni_decrypt2

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
	movdqa	xmm11,xmm14
	xorps	xmm4,xmm12
	movups	XMMWORD PTR[rsi],xmm2
	movups	XMMWORD PTR[16+rsi],xmm3
	movups	XMMWORD PTR[32+rsi],xmm4
	lea	rsi,QWORD PTR[48+rsi]
	jmp	$L$xts_dec_done

ALIGN	16
$L$xts_dec_four::
	movups	xmm2,XMMWORD PTR[rdi]
	movups	xmm3,XMMWORD PTR[16+rdi]
	movups	xmm4,XMMWORD PTR[32+rdi]
	xorps	xmm2,xmm10
	movups	xmm5,XMMWORD PTR[48+rdi]
	lea	rdi,QWORD PTR[64+rdi]
	xorps	xmm3,xmm11
	xorps	xmm4,xmm12
	xorps	xmm5,xmm13

	call	_aesni_decrypt4

	pxor	xmm2,xmm10
	movdqa	xmm10,xmm14
	pxor	xmm3,xmm11
	movdqa	xmm11,xmm15
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[16+rsi],xmm3
	movdqu	XMMWORD PTR[32+rsi],xmm4
	movdqu	XMMWORD PTR[48+rsi],xmm5
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
	xorps	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	movaps	xmm6,XMMWORD PTR[((-160))+rbp]
	movaps	XMMWORD PTR[(-160)+rbp],xmm0
	movaps	xmm7,XMMWORD PTR[((-144))+rbp]
	movaps	XMMWORD PTR[(-144)+rbp],xmm0
	movaps	xmm8,XMMWORD PTR[((-128))+rbp]
	movaps	XMMWORD PTR[(-128)+rbp],xmm0
	movaps	xmm9,XMMWORD PTR[((-112))+rbp]
	movaps	XMMWORD PTR[(-112)+rbp],xmm0
	movaps	xmm10,XMMWORD PTR[((-96))+rbp]
	movaps	XMMWORD PTR[(-96)+rbp],xmm0
	movaps	xmm11,XMMWORD PTR[((-80))+rbp]
	movaps	XMMWORD PTR[(-80)+rbp],xmm0
	movaps	xmm12,XMMWORD PTR[((-64))+rbp]
	movaps	XMMWORD PTR[(-64)+rbp],xmm0
	movaps	xmm13,XMMWORD PTR[((-48))+rbp]
	movaps	XMMWORD PTR[(-48)+rbp],xmm0
	movaps	xmm14,XMMWORD PTR[((-32))+rbp]
	movaps	XMMWORD PTR[(-32)+rbp],xmm0
	movaps	xmm15,XMMWORD PTR[((-16))+rbp]
	movaps	XMMWORD PTR[(-16)+rbp],xmm0
	movaps	XMMWORD PTR[rsp],xmm0
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	XMMWORD PTR[48+rsp],xmm0
	movaps	XMMWORD PTR[64+rsp],xmm0
	movaps	XMMWORD PTR[80+rsp],xmm0
	movaps	XMMWORD PTR[96+rsp],xmm0
	lea	rsp,QWORD PTR[rbp]
	pop	rbp
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
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movups	XMMWORD PTR[r8],xmm2
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
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
	cmp	rdx,16
	jne	$L$cbc_decrypt_bulk



	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[r8]
	movdqa	xmm4,xmm2
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_16::
DB	102,15,56,222,209
	dec	r10d
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_16
DB	102,15,56,223,209
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	movdqu	XMMWORD PTR[r8],xmm4
	xorps	xmm2,xmm3
	pxor	xmm3,xmm3
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	jmp	$L$cbc_ret
ALIGN	16
$L$cbc_decrypt_bulk::
	lea	rax,QWORD PTR[rsp]
	push	rbp
	sub	rsp,176
	and	rsp,-16
	movaps	XMMWORD PTR[16+rsp],xmm6
	movaps	XMMWORD PTR[32+rsp],xmm7
	movaps	XMMWORD PTR[48+rsp],xmm8
	movaps	XMMWORD PTR[64+rsp],xmm9
	movaps	XMMWORD PTR[80+rsp],xmm10
	movaps	XMMWORD PTR[96+rsp],xmm11
	movaps	XMMWORD PTR[112+rsp],xmm12
	movaps	XMMWORD PTR[128+rsp],xmm13
	movaps	XMMWORD PTR[144+rsp],xmm14
	movaps	XMMWORD PTR[160+rsp],xmm15
$L$cbc_decrypt_body::
	lea	rbp,QWORD PTR[((-8))+rax]
	movups	xmm10,XMMWORD PTR[r8]
	mov	eax,r10d
	cmp	rdx,050h
	jbe	$L$cbc_dec_tail

	movups	xmm0,XMMWORD PTR[rcx]
	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movdqa	xmm11,xmm2
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movdqa	xmm12,xmm3
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movdqa	xmm13,xmm4
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movdqa	xmm14,xmm5
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movdqa	xmm15,xmm6
	mov	r9d,DWORD PTR[((OPENSSL_ia32cap_P+4))]
	cmp	rdx,070h
	jbe	$L$cbc_dec_six_or_seven

	and	r9d,71303168
	sub	rdx,050h
	cmp	r9d,4194304
	je	$L$cbc_dec_loop6_enter
	sub	rdx,020h
	lea	rcx,QWORD PTR[112+rcx]
	jmp	$L$cbc_dec_loop8_enter
ALIGN	16
$L$cbc_dec_loop8::
	movups	XMMWORD PTR[rsi],xmm9
	lea	rsi,QWORD PTR[16+rsi]
$L$cbc_dec_loop8_enter::
	movdqu	xmm8,XMMWORD PTR[96+rdi]
	pxor	xmm2,xmm0
	movdqu	xmm9,XMMWORD PTR[112+rdi]
	pxor	xmm3,xmm0
	movups	xmm1,XMMWORD PTR[((16-112))+rcx]
	pxor	xmm4,xmm0
	xor	r11,r11
	cmp	rdx,070h
	pxor	xmm5,xmm0
	pxor	xmm6,xmm0
	pxor	xmm7,xmm0
	pxor	xmm8,xmm0

DB	102,15,56,222,209
	pxor	xmm9,xmm0
	movups	xmm0,XMMWORD PTR[((32-112))+rcx]
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
	setnc	r11b
	shl	r11,7
DB	102,68,15,56,222,201
	add	r11,rdi
	movups	xmm1,XMMWORD PTR[((48-112))+rcx]
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((64-112))+rcx]
	nop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[((80-112))+rcx]
	nop
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((96-112))+rcx]
	nop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[((112-112))+rcx]
	nop
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((128-112))+rcx]
	nop
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[((144-112))+rcx]
	cmp	eax,11
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((160-112))+rcx]
	jb	$L$cbc_dec_done
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[((176-112))+rcx]
	nop
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((192-112))+rcx]
	je	$L$cbc_dec_done
DB	102,15,56,222,209
DB	102,15,56,222,217
DB	102,15,56,222,225
DB	102,15,56,222,233
DB	102,15,56,222,241
DB	102,15,56,222,249
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movups	xmm1,XMMWORD PTR[((208-112))+rcx]
	nop
DB	102,15,56,222,208
DB	102,15,56,222,216
DB	102,15,56,222,224
DB	102,15,56,222,232
DB	102,15,56,222,240
DB	102,15,56,222,248
DB	102,68,15,56,222,192
DB	102,68,15,56,222,200
	movups	xmm0,XMMWORD PTR[((224-112))+rcx]
	jmp	$L$cbc_dec_done
ALIGN	16
$L$cbc_dec_done::
DB	102,15,56,222,209
DB	102,15,56,222,217
	pxor	xmm10,xmm0
	pxor	xmm11,xmm0
DB	102,15,56,222,225
DB	102,15,56,222,233
	pxor	xmm12,xmm0
	pxor	xmm13,xmm0
DB	102,15,56,222,241
DB	102,15,56,222,249
	pxor	xmm14,xmm0
	pxor	xmm15,xmm0
DB	102,68,15,56,222,193
DB	102,68,15,56,222,201
	movdqu	xmm1,XMMWORD PTR[80+rdi]

DB	102,65,15,56,223,210
	movdqu	xmm10,XMMWORD PTR[96+rdi]
	pxor	xmm1,xmm0
DB	102,65,15,56,223,219
	pxor	xmm10,xmm0
	movdqu	xmm0,XMMWORD PTR[112+rdi]
DB	102,65,15,56,223,228
	lea	rdi,QWORD PTR[128+rdi]
	movdqu	xmm11,XMMWORD PTR[r11]
DB	102,65,15,56,223,237
DB	102,65,15,56,223,246
	movdqu	xmm12,XMMWORD PTR[16+r11]
	movdqu	xmm13,XMMWORD PTR[32+r11]
DB	102,65,15,56,223,255
DB	102,68,15,56,223,193
	movdqu	xmm14,XMMWORD PTR[48+r11]
	movdqu	xmm15,XMMWORD PTR[64+r11]
DB	102,69,15,56,223,202
	movdqa	xmm10,xmm0
	movdqu	xmm1,XMMWORD PTR[80+r11]
	movups	xmm0,XMMWORD PTR[((-112))+rcx]

	movups	XMMWORD PTR[rsi],xmm2
	movdqa	xmm2,xmm11
	movups	XMMWORD PTR[16+rsi],xmm3
	movdqa	xmm3,xmm12
	movups	XMMWORD PTR[32+rsi],xmm4
	movdqa	xmm4,xmm13
	movups	XMMWORD PTR[48+rsi],xmm5
	movdqa	xmm5,xmm14
	movups	XMMWORD PTR[64+rsi],xmm6
	movdqa	xmm6,xmm15
	movups	XMMWORD PTR[80+rsi],xmm7
	movdqa	xmm7,xmm1
	movups	XMMWORD PTR[96+rsi],xmm8
	lea	rsi,QWORD PTR[112+rsi]

	sub	rdx,080h
	ja	$L$cbc_dec_loop8

	movaps	xmm2,xmm9
	lea	rcx,QWORD PTR[((-112))+rcx]
	add	rdx,070h
	jle	$L$cbc_dec_clear_tail_collected
	movups	XMMWORD PTR[rsi],xmm9
	lea	rsi,QWORD PTR[16+rsi]
	cmp	rdx,050h
	jbe	$L$cbc_dec_tail

	movaps	xmm2,xmm11
$L$cbc_dec_six_or_seven::
	cmp	rdx,060h
	ja	$L$cbc_dec_seven

	movaps	xmm8,xmm7
	call	_aesni_decrypt6
	pxor	xmm2,xmm10
	movaps	xmm10,xmm8
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	pxor	xmm6,xmm14
	movdqu	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	pxor	xmm7,xmm15
	movdqu	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	lea	rsi,QWORD PTR[80+rsi]
	movdqa	xmm2,xmm7
	pxor	xmm7,xmm7
	jmp	$L$cbc_dec_tail_collected

ALIGN	16
$L$cbc_dec_seven::
	movups	xmm8,XMMWORD PTR[96+rdi]
	xorps	xmm9,xmm9
	call	_aesni_decrypt8
	movups	xmm9,XMMWORD PTR[80+rdi]
	pxor	xmm2,xmm10
	movups	xmm10,XMMWORD PTR[96+rdi]
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	pxor	xmm6,xmm14
	movdqu	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	pxor	xmm7,xmm15
	movdqu	XMMWORD PTR[64+rsi],xmm6
	pxor	xmm6,xmm6
	pxor	xmm8,xmm9
	movdqu	XMMWORD PTR[80+rsi],xmm7
	pxor	xmm7,xmm7
	lea	rsi,QWORD PTR[96+rsi]
	movdqa	xmm2,xmm8
	pxor	xmm8,xmm8
	pxor	xmm9,xmm9
	jmp	$L$cbc_dec_tail_collected

ALIGN	16
$L$cbc_dec_loop6::
	movups	XMMWORD PTR[rsi],xmm7
	lea	rsi,QWORD PTR[16+rsi]
	movdqu	xmm2,XMMWORD PTR[rdi]
	movdqu	xmm3,XMMWORD PTR[16+rdi]
	movdqa	xmm11,xmm2
	movdqu	xmm4,XMMWORD PTR[32+rdi]
	movdqa	xmm12,xmm3
	movdqu	xmm5,XMMWORD PTR[48+rdi]
	movdqa	xmm13,xmm4
	movdqu	xmm6,XMMWORD PTR[64+rdi]
	movdqa	xmm14,xmm5
	movdqu	xmm7,XMMWORD PTR[80+rdi]
	movdqa	xmm15,xmm6
$L$cbc_dec_loop6_enter::
	lea	rdi,QWORD PTR[96+rdi]
	movdqa	xmm8,xmm7

	call	_aesni_decrypt6

	pxor	xmm2,xmm10
	movdqa	xmm10,xmm8
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm6,xmm14
	mov	rcx,r11
	movdqu	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm7,xmm15
	mov	eax,r10d
	movdqu	XMMWORD PTR[64+rsi],xmm6
	lea	rsi,QWORD PTR[80+rsi]
	sub	rdx,060h
	ja	$L$cbc_dec_loop6

	movdqa	xmm2,xmm7
	add	rdx,050h
	jle	$L$cbc_dec_clear_tail_collected
	movups	XMMWORD PTR[rsi],xmm7
	lea	rsi,QWORD PTR[16+rsi]

$L$cbc_dec_tail::
	movups	xmm2,XMMWORD PTR[rdi]
	sub	rdx,010h
	jbe	$L$cbc_dec_one

	movups	xmm3,XMMWORD PTR[16+rdi]
	movaps	xmm11,xmm2
	sub	rdx,010h
	jbe	$L$cbc_dec_two

	movups	xmm4,XMMWORD PTR[32+rdi]
	movaps	xmm12,xmm3
	sub	rdx,010h
	jbe	$L$cbc_dec_three

	movups	xmm5,XMMWORD PTR[48+rdi]
	movaps	xmm13,xmm4
	sub	rdx,010h
	jbe	$L$cbc_dec_four

	movups	xmm6,XMMWORD PTR[64+rdi]
	movaps	xmm14,xmm5
	movaps	xmm15,xmm6
	xorps	xmm7,xmm7
	call	_aesni_decrypt6
	pxor	xmm2,xmm10
	movaps	xmm10,xmm15
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	pxor	xmm6,xmm14
	movdqu	XMMWORD PTR[48+rsi],xmm5
	pxor	xmm5,xmm5
	lea	rsi,QWORD PTR[64+rsi]
	movdqa	xmm2,xmm6
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	sub	rdx,010h
	jmp	$L$cbc_dec_tail_collected

ALIGN	16
$L$cbc_dec_one::
	movaps	xmm11,xmm2
	movups	xmm0,XMMWORD PTR[rcx]
	movups	xmm1,XMMWORD PTR[16+rcx]
	lea	rcx,QWORD PTR[32+rcx]
	xorps	xmm2,xmm0
$L$oop_dec1_17::
DB	102,15,56,222,209
	dec	eax
	movups	xmm1,XMMWORD PTR[rcx]
	lea	rcx,QWORD PTR[16+rcx]
	jnz	$L$oop_dec1_17
DB	102,15,56,223,209
	xorps	xmm2,xmm10
	movaps	xmm10,xmm11
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_two::
	movaps	xmm12,xmm3
	call	_aesni_decrypt2
	pxor	xmm2,xmm10
	movaps	xmm10,xmm12
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	movdqa	xmm2,xmm3
	pxor	xmm3,xmm3
	lea	rsi,QWORD PTR[16+rsi]
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_three::
	movaps	xmm13,xmm4
	call	_aesni_decrypt3
	pxor	xmm2,xmm10
	movaps	xmm10,xmm13
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	movdqa	xmm2,xmm4
	pxor	xmm4,xmm4
	lea	rsi,QWORD PTR[32+rsi]
	jmp	$L$cbc_dec_tail_collected
ALIGN	16
$L$cbc_dec_four::
	movaps	xmm14,xmm5
	call	_aesni_decrypt4
	pxor	xmm2,xmm10
	movaps	xmm10,xmm14
	pxor	xmm3,xmm11
	movdqu	XMMWORD PTR[rsi],xmm2
	pxor	xmm4,xmm12
	movdqu	XMMWORD PTR[16+rsi],xmm3
	pxor	xmm3,xmm3
	pxor	xmm5,xmm13
	movdqu	XMMWORD PTR[32+rsi],xmm4
	pxor	xmm4,xmm4
	movdqa	xmm2,xmm5
	pxor	xmm5,xmm5
	lea	rsi,QWORD PTR[48+rsi]
	jmp	$L$cbc_dec_tail_collected

ALIGN	16
$L$cbc_dec_clear_tail_collected::
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
$L$cbc_dec_tail_collected::
	movups	XMMWORD PTR[r8],xmm10
	and	rdx,15
	jnz	$L$cbc_dec_tail_partial
	movups	XMMWORD PTR[rsi],xmm2
	pxor	xmm2,xmm2
	jmp	$L$cbc_dec_ret
ALIGN	16
$L$cbc_dec_tail_partial::
	movaps	XMMWORD PTR[rsp],xmm2
	pxor	xmm2,xmm2
	mov	rcx,16
	mov	rdi,rsi
	sub	rcx,rdx
	lea	rsi,QWORD PTR[rsp]
	DD	09066A4F3h
	movdqa	XMMWORD PTR[rsp],xmm2

$L$cbc_dec_ret::
	xorps	xmm0,xmm0
	pxor	xmm1,xmm1
	movaps	xmm6,XMMWORD PTR[16+rsp]
	movaps	XMMWORD PTR[16+rsp],xmm0
	movaps	xmm7,XMMWORD PTR[32+rsp]
	movaps	XMMWORD PTR[32+rsp],xmm0
	movaps	xmm8,XMMWORD PTR[48+rsp]
	movaps	XMMWORD PTR[48+rsp],xmm0
	movaps	xmm9,XMMWORD PTR[64+rsp]
	movaps	XMMWORD PTR[64+rsp],xmm0
	movaps	xmm10,XMMWORD PTR[80+rsp]
	movaps	XMMWORD PTR[80+rsp],xmm0
	movaps	xmm11,XMMWORD PTR[96+rsp]
	movaps	XMMWORD PTR[96+rsp],xmm0
	movaps	xmm12,XMMWORD PTR[112+rsp]
	movaps	XMMWORD PTR[112+rsp],xmm0
	movaps	xmm13,XMMWORD PTR[128+rsp]
	movaps	XMMWORD PTR[128+rsp],xmm0
	movaps	xmm14,XMMWORD PTR[144+rsp]
	movaps	XMMWORD PTR[144+rsp],xmm0
	movaps	xmm15,XMMWORD PTR[160+rsp]
	movaps	XMMWORD PTR[160+rsp],xmm0
	lea	rsp,QWORD PTR[rbp]
	pop	rbp
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
	pxor	xmm1,xmm1
	movups	XMMWORD PTR[rcx],xmm0
	pxor	xmm0,xmm0
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

	mov	r10d,268437504
	movups	xmm0,XMMWORD PTR[rcx]
	xorps	xmm4,xmm4
	and	r10d,DWORD PTR[((OPENSSL_ia32cap_P+4))]
	lea	rax,QWORD PTR[16+r8]
	cmp	edx,256
	je	$L$14rounds
	cmp	edx,192
	je	$L$12rounds
	cmp	edx,128
	jne	$L$bad_keybits

$L$10rounds::
	mov	edx,9
	cmp	r10d,268435456
	je	$L$10rounds_alt

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
$L$10rounds_alt::
	movdqa	xmm5,XMMWORD PTR[$L$key_rotate]
	mov	r10d,8
	movdqa	xmm4,XMMWORD PTR[$L$key_rcon1]
	movdqa	xmm2,xmm0
	movdqu	XMMWORD PTR[r8],xmm0
	jmp	$L$oop_key128

ALIGN	16
$L$oop_key128::
DB	102,15,56,0,197
DB	102,15,56,221,196
	pslld	xmm4,1
	lea	rax,QWORD PTR[16+rax]

	movdqa	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm3,xmm2
	pslldq	xmm2,4
	pxor	xmm2,xmm3

	pxor	xmm0,xmm2
	movdqu	XMMWORD PTR[(-16)+rax],xmm0
	movdqa	xmm2,xmm0

	dec	r10d
	jnz	$L$oop_key128

	movdqa	xmm4,XMMWORD PTR[$L$key_rcon1b]

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
	movdqu	XMMWORD PTR[rax],xmm0

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
	movdqu	XMMWORD PTR[16+rax],xmm0

	mov	DWORD PTR[96+rax],edx
	xor	eax,eax
	jmp	$L$enc_key_ret

ALIGN	16
$L$12rounds::
	movq	xmm2,QWORD PTR[16+rcx]
	mov	edx,11
	cmp	r10d,268435456
	je	$L$12rounds_alt

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
$L$12rounds_alt::
	movdqa	xmm5,XMMWORD PTR[$L$key_rotate192]
	movdqa	xmm4,XMMWORD PTR[$L$key_rcon1]
	mov	r10d,8
	movdqu	XMMWORD PTR[r8],xmm0
	jmp	$L$oop_key192

ALIGN	16
$L$oop_key192::
	movq	QWORD PTR[rax],xmm2
	movdqa	xmm1,xmm2
DB	102,15,56,0,213
DB	102,15,56,221,212
	pslld	xmm4,1
	lea	rax,QWORD PTR[24+rax]

	movdqa	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm3,xmm0
	pslldq	xmm0,4
	pxor	xmm0,xmm3

	pshufd	xmm3,xmm0,0ffh
	pxor	xmm3,xmm1
	pslldq	xmm1,4
	pxor	xmm3,xmm1

	pxor	xmm0,xmm2
	pxor	xmm2,xmm3
	movdqu	XMMWORD PTR[(-16)+rax],xmm0

	dec	r10d
	jnz	$L$oop_key192

	mov	DWORD PTR[32+rax],edx
	xor	eax,eax
	jmp	$L$enc_key_ret

ALIGN	16
$L$14rounds::
	movups	xmm2,XMMWORD PTR[16+rcx]
	mov	edx,13
	lea	rax,QWORD PTR[16+rax]
	cmp	r10d,268435456
	je	$L$14rounds_alt

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
$L$14rounds_alt::
	movdqa	xmm5,XMMWORD PTR[$L$key_rotate]
	movdqa	xmm4,XMMWORD PTR[$L$key_rcon1]
	mov	r10d,7
	movdqu	XMMWORD PTR[r8],xmm0
	movdqa	xmm1,xmm2
	movdqu	XMMWORD PTR[16+r8],xmm2
	jmp	$L$oop_key256

ALIGN	16
$L$oop_key256::
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
	movdqu	XMMWORD PTR[rax],xmm0

	dec	r10d
	jz	$L$done_key256

	pshufd	xmm2,xmm0,0ffh
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
	movdqu	XMMWORD PTR[16+rax],xmm2
	lea	rax,QWORD PTR[32+rax]
	movdqa	xmm1,xmm2

	jmp	$L$oop_key256

$L$done_key256::
	mov	DWORD PTR[16+rax],edx
	xor	eax,eax
	jmp	$L$enc_key_ret

ALIGN	16
$L$bad_keybits::
	mov	rax,-2
$L$enc_key_ret::
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
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
$L$increment1::
DB	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
$L$key_rotate::
	DD	00c0f0e0dh,00c0f0e0dh,00c0f0e0dh,00c0f0e0dh
$L$key_rotate192::
	DD	004070605h,004070605h,004070605h,004070605h
$L$key_rcon1::
	DD	1,1,1,1
$L$key_rcon1b::
	DD	01bh,01bh,01bh,01bh

DB	65,69,83,32,102,111,114,32,73,110,116,101,108,32,65,69
DB	83,45,78,73,44,32,67,82,89,80,84,79,71,65,77,83
DB	32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115
DB	115,108,46,111,114,103,62,0
ALIGN	64
EXTERN	__imp_RtlVirtualUnwind:NEAR

ALIGN	16
ecb_ccm64_se_handler	PROC PRIVATE
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
ecb_ccm64_se_handler	ENDP


ALIGN	16
ctr_xts_se_handler	PROC PRIVATE
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

	mov	rax,QWORD PTR[160+r8]
	lea	rsi,QWORD PTR[((-160))+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

	jmp	$L$common_rbp_tail
ctr_xts_se_handler	ENDP

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

	lea	r10,QWORD PTR[$L$cbc_decrypt_bulk]
	cmp	rbx,r10
	jb	$L$common_seh_tail

	lea	r10,QWORD PTR[$L$cbc_decrypt_body]
	cmp	rbx,r10
	jb	$L$restore_cbc_rax

	lea	r10,QWORD PTR[$L$cbc_ret]
	cmp	rbx,r10
	jae	$L$common_seh_tail

	lea	rsi,QWORD PTR[16+rax]
	lea	rdi,QWORD PTR[512+r8]
	mov	ecx,20
	DD	0a548f3fch

$L$common_rbp_tail::
	mov	rax,QWORD PTR[160+r8]
	mov	rbp,QWORD PTR[rax]
	lea	rax,QWORD PTR[8+rax]
	mov	QWORD PTR[160+r8],rbp
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
	DD	imagerel ecb_ccm64_se_handler
	DD	imagerel $L$ecb_enc_body,imagerel $L$ecb_enc_ret
$L$SEH_info_ccm64_enc::
DB	9,0,0,0
	DD	imagerel ecb_ccm64_se_handler
	DD	imagerel $L$ccm64_enc_body,imagerel $L$ccm64_enc_ret
$L$SEH_info_ccm64_dec::
DB	9,0,0,0
	DD	imagerel ecb_ccm64_se_handler
	DD	imagerel $L$ccm64_dec_body,imagerel $L$ccm64_dec_ret
$L$SEH_info_ctr32::
DB	9,0,0,0
	DD	imagerel ctr_xts_se_handler
	DD	imagerel $L$ctr32_body,imagerel $L$ctr32_epilogue
$L$SEH_info_xts_enc::
DB	9,0,0,0
	DD	imagerel ctr_xts_se_handler
	DD	imagerel $L$xts_enc_body,imagerel $L$xts_enc_epilogue
$L$SEH_info_xts_dec::
DB	9,0,0,0
	DD	imagerel ctr_xts_se_handler
	DD	imagerel $L$xts_dec_body,imagerel $L$xts_dec_epilogue
$L$SEH_info_cbc::
DB	9,0,0,0
	DD	imagerel cbc_se_handler
$L$SEH_info_key::
DB	001h,004h,001h,000h
DB	004h,002h,000h,000h

.xdata	ENDS
END
